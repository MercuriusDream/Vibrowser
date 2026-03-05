from __future__ import annotations

import json
import queue
import subprocess
import threading
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable

from .errors import (
    CodexNotFoundError,
    PortExitError,
    ResponseError,
    ResponseTimeoutError,
    TurnCancelledError,
    TurnFailedError,
    TurnInputRequiredError,
    TurnTimeoutError,
)
from .models import CodexConfig, Issue
from .utils import utc_now

EventCallback = Callable[[dict[str, Any]], None]


@dataclass(slots=True)
class SessionHandle:
    process: subprocess.Popen[str]
    thread_id: str
    workspace: Path


@dataclass(slots=True)
class TurnOutcome:
    status: str
    turn_id: str


class CodexAppServerClient:
    def __init__(
        self,
        codex_config: CodexConfig,
        workspace: Path,
        logger,
        event_callback: EventCallback,
        cancel_event: threading.Event | None = None,
    ) -> None:
        self._config = codex_config
        self._workspace = workspace
        self._logger = logger
        self._event_callback = event_callback
        self._cancel_event = cancel_event or threading.Event()

        self._next_id = 1
        self._responses: dict[int, queue.Queue[dict[str, Any]]] = {}
        self._responses_lock = threading.Lock()
        self._events: queue.Queue[dict[str, Any]] = queue.Queue()

        self._process: subprocess.Popen[str] | None = None
        self._stdin = None

    def start_session(self) -> SessionHandle:
        process = self._launch_process()
        self._process = process
        self._stdin = process.stdin
        self._start_readers(process)

        self._request(
            method="initialize",
            params={
                "clientInfo": {"name": "symphony", "version": "1.0"},
                "capabilities": {},
            },
            timeout_ms=self._config.read_timeout_ms,
        )

        self._notify(method="initialized", params={})

        thread_response = self._request(
            method="thread/start",
            params={
                "approvalPolicy": self._config.approval_policy,
                "sandbox": self._config.thread_sandbox,
                "cwd": str(self._workspace.resolve()),
            },
            timeout_ms=self._config.read_timeout_ms,
        )
        thread_id = self._extract_thread_id(thread_response)

        return SessionHandle(process=process, thread_id=thread_id, workspace=self._workspace)

    def run_turn(
        self,
        session: SessionHandle,
        issue: Issue,
        prompt: str,
    ) -> TurnOutcome:
        title = f"{issue.identifier}: {issue.title}"
        start_response = self._request(
            method="turn/start",
            params={
                "threadId": session.thread_id,
                "input": [{"type": "text", "text": prompt}],
                "cwd": str(self._workspace.resolve()),
                "title": title,
                "approvalPolicy": self._config.approval_policy,
                "sandboxPolicy": self._config.turn_sandbox_policy,
            },
            timeout_ms=self._config.read_timeout_ms,
        )
        turn_id = self._extract_turn_id(start_response)
        session_id = f"{session.thread_id}-{turn_id}"
        self._emit_event(
            {
                "event": "session_started",
                "session_id": session_id,
                "thread_id": session.thread_id,
                "turn_id": turn_id,
                "codex_app_server_pid": str(session.process.pid),
            }
        )

        deadline = utc_now().timestamp() + (self._config.turn_timeout_ms / 1000)
        while True:
            if self._cancel_event.is_set():
                self._terminate_process(session.process)
                raise TurnCancelledError("turn cancelled by orchestrator")

            if session.process.poll() is not None:
                raise PortExitError(f"app-server exited code={session.process.returncode}")

            remaining = deadline - utc_now().timestamp()
            if remaining <= 0:
                self._terminate_process(session.process)
                raise TurnTimeoutError("turn timed out")

            message = self._wait_for_event(timeout_s=min(remaining, 0.25))
            if message is None:
                continue

            self._handle_message_side_effects(message)
            method = message.get("method")
            if not isinstance(method, str):
                continue

            if method == "turn/completed":
                self._emit_event({"event": "turn_completed", "session_id": session_id})
                return TurnOutcome(status="completed", turn_id=turn_id)
            if method == "turn/failed":
                self._emit_event({"event": "turn_failed", "session_id": session_id})
                raise TurnFailedError("turn failed")
            if method == "turn/cancelled":
                self._emit_event({"event": "turn_cancelled", "session_id": session_id})
                raise TurnCancelledError("turn cancelled")

    def stop_session(self, session: SessionHandle) -> None:
        self._terminate_process(session.process)

    def _launch_process(self) -> subprocess.Popen[str]:
        try:
            process = subprocess.Popen(
                ["bash", "-lc", self._config.command],
                cwd=self._workspace,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                bufsize=1,
            )
        except FileNotFoundError as exc:
            raise CodexNotFoundError(f"codex launch failed: {exc}") from exc
        except OSError as exc:
            raise PortExitError(f"failed to launch codex process: {exc}") from exc

        return process

    def _start_readers(self, process: subprocess.Popen[str]) -> None:
        assert process.stdout is not None
        assert process.stderr is not None

        threading.Thread(
            target=self._stdout_reader,
            args=(process,),
            daemon=True,
            name="codex-stdout-reader",
        ).start()
        threading.Thread(
            target=self._stderr_reader,
            args=(process,),
            daemon=True,
            name="codex-stderr-reader",
        ).start()

    def _stdout_reader(self, process: subprocess.Popen[str]) -> None:
        assert process.stdout is not None
        for raw in process.stdout:
            line = raw.strip()
            if not line:
                continue
            try:
                payload = json.loads(line)
            except json.JSONDecodeError:
                self._emit_event({"event": "malformed", "line": line[:256]})
                continue
            if not isinstance(payload, dict):
                self._emit_event({"event": "malformed", "line": line[:256]})
                continue

            req_id = payload.get("id")
            if isinstance(req_id, int) and ("result" in payload or "error" in payload):
                with self._responses_lock:
                    q = self._responses.get(req_id)
                if q is not None:
                    q.put(payload)
                    continue

            self._events.put(payload)

    def _stderr_reader(self, process: subprocess.Popen[str]) -> None:
        assert process.stderr is not None
        for raw in process.stderr:
            text = raw.strip()
            if text:
                self._logger.warning("action=codex_stderr line=%r", text[:512])

    def _request(self, method: str, params: dict[str, Any], timeout_ms: int) -> dict[str, Any]:
        req_id = self._allocate_request_id()
        holder: queue.Queue[dict[str, Any]] = queue.Queue(maxsize=1)
        with self._responses_lock:
            self._responses[req_id] = holder

        self._send_json({"id": req_id, "method": method, "params": params})

        timeout_s = max(timeout_ms / 1000, 0.1)
        try:
            response = holder.get(timeout=timeout_s)
        except queue.Empty as exc:
            with self._responses_lock:
                self._responses.pop(req_id, None)
            if self._process and self._process.poll() is not None and self._process.returncode == 127:
                raise CodexNotFoundError("codex app-server command not found") from exc
            raise ResponseTimeoutError(f"request timed out method={method}") from exc

        with self._responses_lock:
            self._responses.pop(req_id, None)

        if "error" in response:
            raise ResponseError(f"request error method={method} error={response['error']}")
        return response

    def _notify(self, method: str, params: dict[str, Any]) -> None:
        self._send_json({"method": method, "params": params})

    def _send_json(self, payload: dict[str, Any]) -> None:
        if self._stdin is None:
            raise PortExitError("codex stdin is unavailable")
        line = json.dumps(payload, separators=(",", ":"))
        try:
            self._stdin.write(line + "\n")
            self._stdin.flush()
        except (BrokenPipeError, OSError) as exc:
            raise PortExitError(f"failed to write to codex app-server: {exc}") from exc

    def _wait_for_event(self, timeout_s: float) -> dict[str, Any] | None:
        try:
            return self._events.get(timeout=max(timeout_s, 0.01))
        except queue.Empty:
            return None

    def _handle_message_side_effects(self, message: dict[str, Any]) -> None:
        method = message.get("method")
        if isinstance(method, str):
            method_lc = method.lower()
            if "requestuserinput" in method_lc or "input_required" in method_lc:
                self._emit_event({"event": "turn_input_required"})
                raise TurnInputRequiredError("agent requested user input")

            if "approval" in method_lc and isinstance(message.get("id"), int):
                self._send_json({"id": message["id"], "result": {"approved": True}})
                self._emit_event({"event": "approval_auto_approved"})
                return

            if method_lc.endswith("tool/call") and isinstance(message.get("id"), int):
                params = message.get("params") if isinstance(message.get("params"), dict) else {}
                tool_name = str(params.get("name") or params.get("tool") or "").strip()
                self._send_json(
                    {
                        "id": message["id"],
                        "result": {
                            "success": False,
                            "error": "unsupported_tool_call",
                            "tool": tool_name,
                        },
                    }
                )
                self._emit_event({"event": "unsupported_tool_call", "tool": tool_name})
                return

        usage = _extract_usage(message)
        rate_limits = _extract_rate_limits(message)
        payload: dict[str, Any] = {"event": "notification", "message": method or "other_message"}
        if usage:
            payload["usage"] = usage
        if rate_limits:
            payload["rate_limits"] = rate_limits
        self._emit_event(payload)

    def _allocate_request_id(self) -> int:
        req_id = self._next_id
        self._next_id += 1
        return req_id

    def _extract_thread_id(self, response: dict[str, Any]) -> str:
        result = response.get("result")
        if isinstance(result, dict):
            thread = result.get("thread")
            if isinstance(thread, dict) and isinstance(thread.get("id"), str):
                return thread["id"]
            if isinstance(result.get("threadId"), str):
                return result["threadId"]
        raise ResponseError("thread/start response missing thread id")

    def _extract_turn_id(self, response: dict[str, Any]) -> str:
        result = response.get("result")
        if isinstance(result, dict):
            turn = result.get("turn")
            if isinstance(turn, dict) and isinstance(turn.get("id"), str):
                return turn["id"]
            if isinstance(result.get("turnId"), str):
                return result["turnId"]
        raise ResponseError("turn/start response missing turn id")

    def _emit_event(self, payload: dict[str, Any]) -> None:
        payload.setdefault("timestamp", utc_now().isoformat().replace("+00:00", "Z"))
        self._event_callback(payload)

    def _terminate_process(self, process: subprocess.Popen[str]) -> None:
        if process.poll() is not None:
            return
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=5)


def _extract_usage(payload: Any) -> dict[str, int] | None:
    usage = _find_map_with_keys(payload, ["input_tokens", "output_tokens", "total_tokens"])
    if usage:
        return {
            "input_tokens": int(usage.get("input_tokens", 0) or 0),
            "output_tokens": int(usage.get("output_tokens", 0) or 0),
            "total_tokens": int(usage.get("total_tokens", 0) or 0),
        }

    usage = _find_map_with_keys(payload, ["inputTokens", "outputTokens", "totalTokens"])
    if usage:
        return {
            "input_tokens": int(usage.get("inputTokens", 0) or 0),
            "output_tokens": int(usage.get("outputTokens", 0) or 0),
            "total_tokens": int(usage.get("totalTokens", 0) or 0),
        }

    return None


def _extract_rate_limits(payload: Any) -> dict[str, Any] | None:
    if isinstance(payload, dict):
        for key in ("rate_limits", "rateLimits"):
            value = payload.get(key)
            if isinstance(value, dict):
                return value
        for value in payload.values():
            nested = _extract_rate_limits(value)
            if nested:
                return nested
    elif isinstance(payload, list):
        for value in payload:
            nested = _extract_rate_limits(value)
            if nested:
                return nested
    return None


def _find_map_with_keys(payload: Any, keys: list[str]) -> dict[str, Any] | None:
    if isinstance(payload, dict):
        if all(k in payload for k in keys):
            return payload
        for value in payload.values():
            nested = _find_map_with_keys(value, keys)
            if nested:
                return nested
    elif isinstance(payload, list):
        for value in payload:
            nested = _find_map_with_keys(value, keys)
            if nested:
                return nested
    return None
