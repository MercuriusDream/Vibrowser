from __future__ import annotations

import threading
import time
from concurrent.futures import Future, ThreadPoolExecutor
from dataclasses import dataclass
from datetime import datetime, timedelta
from typing import Any

from .agent_runner import AgentRunner, RunnerDependencies
from .config import is_active_state, is_terminal_state, load_service_config, validate_dispatch_config
from .errors import ConfigValidationError
from .models import Issue, RetryEntry, ServiceConfig, WorkerResult
from .utils import normalize_state, utc_now
from .workflow import WorkflowStore
from .workspace import WorkspaceManager


@dataclass(slots=True)
class RunningEntry:
    issue: Issue
    future: Future[WorkerResult]
    cancel_event: threading.Event
    retry_attempt: int | None
    started_at: datetime
    session_id: str | None = None
    codex_app_server_pid: str | None = None
    last_codex_message: str | None = None
    last_codex_event: str | None = None
    last_codex_timestamp: datetime | None = None
    codex_input_tokens: int = 0
    codex_output_tokens: int = 0
    codex_total_tokens: int = 0
    last_reported_input_tokens: int = 0
    last_reported_output_tokens: int = 0
    last_reported_total_tokens: int = 0
    turn_count: int = 0
    stop_reason: str | None = None
    cleanup_workspace_on_stop: bool = False


class Orchestrator:
    def __init__(
        self,
        workflow_store: WorkflowStore,
        tracker_client,
        workspace_manager: WorkspaceManager,
        logger,
    ) -> None:
        self._workflow_store = workflow_store
        self._tracker = tracker_client
        self._workspace_manager = workspace_manager
        self._logger = logger

        self._lock = threading.RLock()
        self._executor = ThreadPoolExecutor(max_workers=64, thread_name_prefix="symphony-worker")
        self._stop_event = threading.Event()

        self._config: ServiceConfig | None = None
        self._running: dict[str, RunningEntry] = {}
        self._claimed: set[str] = set()
        self._retry_attempts: dict[str, RetryEntry] = {}
        self._completed: set[str] = set()

        self._codex_totals = {
            "input_tokens": 0,
            "output_tokens": 0,
            "total_tokens": 0,
            "seconds_running": 0.0,
        }
        self._codex_rate_limits: dict[str, Any] | None = None

    def startup(self) -> None:
        workflow = self._workflow_store.load_initial()
        self._config = load_service_config(workflow)
        validate_dispatch_config(self._config)
        self._apply_runtime_config(self._config)
        self._startup_terminal_workspace_cleanup()

    def run_forever(self) -> None:
        while not self._stop_event.is_set():
            started = time.monotonic()
            self.tick()
            cfg = self._require_config()
            wait_s = max(cfg.polling.interval_ms / 1000 - (time.monotonic() - started), 0.05)
            self._stop_event.wait(wait_s)

    def stop(self) -> None:
        self._stop_event.set()
        with self._lock:
            entries = list(self._running.values())
        for entry in entries:
            entry.cancel_event.set()
        self._executor.shutdown(wait=False, cancel_futures=True)

    def tick(self) -> None:
        self._reload_workflow_if_changed()
        self._reconcile_running_issues()
        self._process_due_retries()

        cfg = self._require_config()
        try:
            validate_dispatch_config(cfg)
        except ConfigValidationError as exc:
            self._logger.error("action=dispatch_validation_failed error=%s", exc)
            return

        try:
            candidates = self._tracker.fetch_candidate_issues()
        except Exception as exc:
            self._logger.error("action=fetch_candidates_failed error=%s", exc)
            return

        sorted_issues = sorted(candidates, key=_dispatch_sort_key)
        for issue in sorted_issues:
            if self._available_slots() <= 0:
                break
            if self._should_dispatch(issue):
                self._dispatch_issue(issue, attempt=None)

    def snapshot(self) -> dict[str, Any]:
        now = utc_now()
        with self._lock:
            running_rows = []
            for issue_id, entry in self._running.items():
                running_rows.append(
                    {
                        "issue_id": issue_id,
                        "issue_identifier": entry.issue.identifier,
                        "state": entry.issue.state,
                        "session_id": entry.session_id,
                        "turn_count": entry.turn_count,
                        "last_event": entry.last_codex_event,
                        "last_message": entry.last_codex_message,
                        "started_at": entry.started_at.isoformat().replace("+00:00", "Z"),
                        "last_event_at": (
                            entry.last_codex_timestamp.isoformat().replace("+00:00", "Z")
                            if entry.last_codex_timestamp
                            else None
                        ),
                        "tokens": {
                            "input_tokens": entry.codex_input_tokens,
                            "output_tokens": entry.codex_output_tokens,
                            "total_tokens": entry.codex_total_tokens,
                        },
                    }
                )

            retry_rows = [
                {
                    "issue_id": row.issue_id,
                    "issue_identifier": row.identifier,
                    "attempt": row.attempt,
                    "due_at": row.due_at.isoformat().replace("+00:00", "Z"),
                    "error": row.error,
                }
                for row in self._retry_attempts.values()
            ]

            active_runtime = sum((now - row.started_at).total_seconds() for row in self._running.values())
            totals = dict(self._codex_totals)
            totals["seconds_running"] = totals["seconds_running"] + active_runtime

            return {
                "generated_at": now.isoformat().replace("+00:00", "Z"),
                "counts": {"running": len(running_rows), "retrying": len(retry_rows)},
                "running": running_rows,
                "retrying": retry_rows,
                "codex_totals": totals,
                "rate_limits": self._codex_rate_limits,
            }

    def _reload_workflow_if_changed(self) -> None:
        changed, error = self._workflow_store.reload_if_changed()
        if error is not None:
            self._logger.error("action=workflow_reload_failed error=%s", error)
            return
        if not changed:
            return
        workflow = self._workflow_store.get()
        try:
            updated = load_service_config(workflow)
            validate_dispatch_config(updated)
            self._config = updated
            self._apply_runtime_config(updated)
            self._logger.info("action=workflow_reloaded status=ok")
        except Exception as exc:
            self._logger.error("action=workflow_reload_invalid error=%s", exc)

    def _startup_terminal_workspace_cleanup(self) -> None:
        cfg = self._require_config()
        try:
            terminal_issues = self._tracker.fetch_issues_by_states(cfg.tracker.terminal_states)
        except Exception as exc:
            self._logger.warning("action=startup_terminal_cleanup_skipped error=%s", exc)
            return

        for issue in terminal_issues:
            try:
                self._workspace_manager.remove_workspace_for_identifier(issue.identifier)
            except Exception as exc:
                self._logger.warning(
                    "action=startup_terminal_cleanup_failed issue_identifier=%s error=%s",
                    issue.identifier,
                    exc,
                )

    def _reconcile_running_issues(self) -> None:
        cfg = self._require_config()
        self._reconcile_stalled_runs(cfg)

        with self._lock:
            running_ids = list(self._running.keys())
        if not running_ids:
            return

        try:
            refreshed = self._tracker.fetch_issue_states_by_ids(running_ids)
        except Exception as exc:
            self._logger.warning("action=reconcile_refresh_failed error=%s", exc)
            return

        refreshed_by_id = {issue.id: issue for issue in refreshed}

        with self._lock:
            active_ids = list(self._running.keys())

        for issue_id in active_ids:
            issue = refreshed_by_id.get(issue_id)
            if issue is None:
                continue
            if is_terminal_state(cfg, issue.state):
                self._terminate_running_issue(
                    issue_id,
                    reason="terminal_state",
                    cleanup_workspace=True,
                )
            elif is_active_state(cfg, issue.state):
                with self._lock:
                    entry = self._running.get(issue_id)
                    if entry:
                        entry.issue = issue
            else:
                self._terminate_running_issue(
                    issue_id,
                    reason="non_active_state",
                    cleanup_workspace=False,
                )

    def _reconcile_stalled_runs(self, cfg: ServiceConfig) -> None:
        timeout_ms = cfg.codex.stall_timeout_ms
        if timeout_ms <= 0:
            return

        now = utc_now()
        with self._lock:
            entries = list(self._running.items())

        for issue_id, entry in entries:
            base = entry.last_codex_timestamp or entry.started_at
            elapsed_ms = (now - base).total_seconds() * 1000
            if elapsed_ms > timeout_ms:
                self._terminate_running_issue(issue_id, reason="stalled", cleanup_workspace=False)

    def _terminate_running_issue(self, issue_id: str, reason: str, cleanup_workspace: bool) -> None:
        with self._lock:
            entry = self._running.get(issue_id)
            if not entry:
                return
            entry.stop_reason = reason
            entry.cleanup_workspace_on_stop = cleanup_workspace
            entry.cancel_event.set()

    def _process_due_retries(self) -> None:
        now = utc_now()
        with self._lock:
            due = [item for item in self._retry_attempts.values() if item.due_at <= now]
        for entry in due:
            self._handle_retry_entry(entry)

    def _handle_retry_entry(self, entry: RetryEntry) -> None:
        with self._lock:
            current = self._retry_attempts.get(entry.issue_id)
            if current is None or current.due_at != entry.due_at:
                return
            self._retry_attempts.pop(entry.issue_id, None)

        try:
            candidates = self._tracker.fetch_candidate_issues()
        except Exception as exc:
            self._schedule_retry(
                issue_id=entry.issue_id,
                identifier=entry.identifier,
                attempt=entry.attempt + 1,
                continuation=False,
                error=f"retry poll failed: {exc}",
            )
            return

        issue = next((item for item in candidates if item.id == entry.issue_id), None)
        if issue is None:
            with self._lock:
                self._claimed.discard(entry.issue_id)
            return

        if not self._should_dispatch(issue):
            with self._lock:
                self._claimed.discard(entry.issue_id)
            return

        if self._available_slots() <= 0:
            self._schedule_retry(
                issue_id=entry.issue_id,
                identifier=issue.identifier,
                attempt=entry.attempt + 1,
                continuation=False,
                error="no available orchestrator slots",
            )
            return

        self._dispatch_issue(issue, attempt=entry.attempt)

    def _dispatch_issue(self, issue: Issue, attempt: int | None) -> None:
        cancel_event = threading.Event()
        cfg = self._require_config()
        runner = AgentRunner(
            RunnerDependencies(
                config=cfg,
                workflow_store=self._workflow_store,
                workspace_manager=self._workspace_manager,
                tracker_client=self._tracker,
                logger=self._logger,
            )
        )

        def on_event(event: dict[str, Any]) -> None:
            self._on_worker_event(issue.id, event)

        future = self._executor.submit(runner.run_attempt, issue, attempt, cancel_event, on_event)
        with self._lock:
            self._running[issue.id] = RunningEntry(
                issue=issue,
                future=future,
                cancel_event=cancel_event,
                retry_attempt=attempt,
                started_at=utc_now(),
            )
            self._claimed.add(issue.id)
            self._retry_attempts.pop(issue.id, None)

        future.add_done_callback(lambda fut, issue_id=issue.id: self._on_worker_done(issue_id, fut))

        self._logger.info(
            "action=dispatch issue_id=%s issue_identifier=%s attempt=%s status=running",
            issue.id,
            issue.identifier,
            attempt,
        )

    def _on_worker_event(self, issue_id: str, event: dict[str, Any]) -> None:
        with self._lock:
            entry = self._running.get(issue_id)
            if entry is None:
                return

            entry.last_codex_event = str(event.get("event") or "notification")
            entry.last_codex_message = str(event.get("message") or "")
            entry.last_codex_timestamp = utc_now()

            session_id = event.get("session_id")
            if isinstance(session_id, str):
                entry.session_id = session_id
                entry.turn_count += 1
            pid = event.get("codex_app_server_pid")
            if isinstance(pid, str):
                entry.codex_app_server_pid = pid

            usage = event.get("usage") if isinstance(event.get("usage"), dict) else None
            if usage is not None:
                in_tok = int(usage.get("input_tokens", 0) or 0)
                out_tok = int(usage.get("output_tokens", 0) or 0)
                total_tok = int(usage.get("total_tokens", 0) or 0)

                entry.codex_input_tokens = in_tok
                entry.codex_output_tokens = out_tok
                entry.codex_total_tokens = total_tok

                delta_in = max(in_tok - entry.last_reported_input_tokens, 0)
                delta_out = max(out_tok - entry.last_reported_output_tokens, 0)
                delta_total = max(total_tok - entry.last_reported_total_tokens, 0)
                self._codex_totals["input_tokens"] += delta_in
                self._codex_totals["output_tokens"] += delta_out
                self._codex_totals["total_tokens"] += delta_total

                entry.last_reported_input_tokens = in_tok
                entry.last_reported_output_tokens = out_tok
                entry.last_reported_total_tokens = total_tok

            rate_limits = event.get("rate_limits")
            if isinstance(rate_limits, dict):
                self._codex_rate_limits = rate_limits

            self._logger.info(
                "action=codex_event issue_id=%s issue_identifier=%s session_id=%s event=%s",
                entry.issue.id,
                entry.issue.identifier,
                entry.session_id,
                entry.last_codex_event,
            )

    def _on_worker_done(self, issue_id: str, future: Future[WorkerResult]) -> None:
        error: Exception | None = None
        result: WorkerResult | None = None
        try:
            result = future.result()
        except Exception as exc:
            error = exc

        with self._lock:
            entry = self._running.pop(issue_id, None)
        if entry is None:
            return

        with self._lock:
            self._codex_totals["seconds_running"] += max((utc_now() - entry.started_at).total_seconds(), 0.0)

        if entry.cleanup_workspace_on_stop:
            try:
                self._workspace_manager.remove_workspace_for_identifier(entry.issue.identifier)
            except Exception as exc:
                self._logger.warning(
                    "action=workspace_cleanup_failed issue_id=%s issue_identifier=%s error=%s",
                    entry.issue.id,
                    entry.issue.identifier,
                    exc,
                )

        if entry.stop_reason in {"terminal_state", "non_active_state"}:
            with self._lock:
                self._claimed.discard(issue_id)
            return

        if error is not None:
            self._schedule_retry(
                issue_id=entry.issue.id,
                identifier=entry.issue.identifier,
                attempt=(entry.retry_attempt or 0) + 1,
                continuation=False,
                error=f"worker exception: {error}",
            )
            return

        assert result is not None
        if result.status == "succeeded":
            with self._lock:
                self._completed.add(issue_id)
            self._schedule_retry(
                issue_id=entry.issue.id,
                identifier=entry.issue.identifier,
                attempt=1,
                continuation=True,
                error=None,
            )
            return

        if result.status == "canceled_by_reconciliation":
            if entry.stop_reason in {"stalled"}:
                self._schedule_retry(
                    issue_id=entry.issue.id,
                    identifier=entry.issue.identifier,
                    attempt=(entry.retry_attempt or 0) + 1,
                    continuation=False,
                    error="stalled",
                )
            else:
                with self._lock:
                    self._claimed.discard(issue_id)
            return

        self._schedule_retry(
            issue_id=entry.issue.id,
            identifier=entry.issue.identifier,
            attempt=(entry.retry_attempt or 0) + 1,
            continuation=False,
            error=result.error or "worker failed",
        )

    def _schedule_retry(
        self,
        issue_id: str,
        identifier: str,
        attempt: int,
        continuation: bool,
        error: str | None,
    ) -> None:
        cfg = self._require_config()
        delay_ms = 1000 if continuation else min(
            10000 * (2 ** max(attempt - 1, 0)),
            cfg.agent.max_retry_backoff_ms,
        )
        due = utc_now() + timedelta(milliseconds=delay_ms)
        with self._lock:
            self._retry_attempts[issue_id] = RetryEntry(
                issue_id=issue_id,
                identifier=identifier,
                attempt=attempt,
                error=error,
                due_at=due,
            )

        self._logger.info(
            "action=retry_scheduled issue_id=%s issue_identifier=%s attempt=%s delay_ms=%s error=%s",
            issue_id,
            identifier,
            attempt,
            delay_ms,
            error,
        )

    def _should_dispatch(self, issue: Issue) -> bool:
        cfg = self._require_config()
        if not issue.id or not issue.identifier or issue.title is None or not issue.state:
            return False
        if not is_active_state(cfg, issue.state):
            return False
        if is_terminal_state(cfg, issue.state):
            return False

        with self._lock:
            if issue.id in self._running or issue.id in self._claimed:
                return False

        if normalize_state(issue.state) == "todo":
            for blocker in issue.blocked_by:
                if blocker.state and not is_terminal_state(cfg, blocker.state):
                    return False

        state_limit = self._state_limit(issue.state)
        running_in_state = self._running_count_for_state(issue.state)
        if running_in_state >= state_limit:
            return False

        return True

    def _available_slots(self) -> int:
        cfg = self._require_config()
        with self._lock:
            running_count = len(self._running)
        return max(cfg.agent.max_concurrent_agents - running_count, 0)

    def _state_limit(self, state: str) -> int:
        cfg = self._require_config()
        normalized = normalize_state(state)
        return cfg.agent.max_concurrent_agents_by_state.get(
            normalized,
            cfg.agent.max_concurrent_agents,
        )

    def _running_count_for_state(self, state: str) -> int:
        normalized = normalize_state(state)
        with self._lock:
            return sum(1 for row in self._running.values() if normalize_state(row.issue.state) == normalized)

    def _require_config(self) -> ServiceConfig:
        if self._config is None:
            raise RuntimeError("orchestrator config is not initialized")
        return self._config

    def _apply_runtime_config(self, config: ServiceConfig) -> None:
        self._workspace_manager.update_config(config)
        if hasattr(self._tracker, "config"):
            self._tracker.config = config


def _dispatch_sort_key(issue: Issue) -> tuple[int, float, str]:
    if issue.priority is None:
        priority = 10_000
    else:
        priority = issue.priority
    created_epoch = issue.created_at.timestamp() if issue.created_at is not None else float("inf")
    return (priority, created_epoch, issue.identifier)
