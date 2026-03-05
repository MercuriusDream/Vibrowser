from __future__ import annotations

import threading
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable

from .codex_app_server import CodexAppServerClient, SessionHandle
from .config import is_active_state
from .errors import SymphonyError
from .models import Issue, ServiceConfig, WorkerResult
from .utils import utc_now
from .workflow import WorkflowStore, render_prompt
from .workspace import WorkspaceManager

RunEventCallback = Callable[[dict[str, Any]], None]


@dataclass(slots=True)
class RunnerDependencies:
    config: ServiceConfig
    workflow_store: WorkflowStore
    workspace_manager: WorkspaceManager
    tracker_client: Any
    logger: Any


class AgentRunner:
    def __init__(self, deps: RunnerDependencies):
        self._deps = deps

    def run_attempt(
        self,
        issue: Issue,
        attempt: int | None,
        cancel_event: threading.Event,
        event_callback: RunEventCallback,
    ) -> WorkerResult:
        started = utc_now()
        workspace: Path | None = None
        session: SessionHandle | None = None
        codex: CodexAppServerClient | None = None

        def emit(payload: dict[str, Any]) -> None:
            payload.setdefault("issue_id", issue.id)
            payload.setdefault("issue_identifier", issue.identifier)
            event_callback(payload)

        try:
            workspace_result = self._deps.workspace_manager.ensure_workspace_for_issue(issue.identifier)
            workspace = workspace_result.path
            self._deps.workspace_manager.validate_agent_cwd(workspace)
            self._deps.workspace_manager.run_before_run(workspace)

            codex = CodexAppServerClient(
                codex_config=self._deps.config.codex,
                workspace=workspace,
                logger=self._deps.logger,
                event_callback=emit,
                cancel_event=cancel_event,
            )
            session = codex.start_session()

            workflow = self._deps.workflow_store.get()
            current_issue = issue
            turn_number = 1
            max_turns = self._deps.config.agent.max_turns

            while True:
                if turn_number == 1:
                    prompt = render_prompt(workflow.prompt_template, current_issue, attempt)
                else:
                    prompt = (
                        "Continue working on this issue using existing thread context. "
                        "Do not repeat prior output; progress the implementation and validation."
                    )

                codex.run_turn(session=session, issue=current_issue, prompt=prompt)

                refreshed = self._deps.tracker_client.fetch_issue_states_by_ids([current_issue.id])
                if not refreshed:
                    break
                current_issue = refreshed[0]

                if not is_active_state(self._deps.config, current_issue.state):
                    break
                if turn_number >= max_turns:
                    break
                turn_number += 1

            status = "succeeded" if not cancel_event.is_set() else "canceled_by_reconciliation"
            return WorkerResult(
                issue_id=issue.id,
                identifier=issue.identifier,
                status=status,
                attempt=attempt,
                runtime_seconds=(utc_now() - started).total_seconds(),
            )
        except Exception as exc:
            if cancel_event.is_set():
                status = "canceled_by_reconciliation"
            else:
                status = "failed"
            return WorkerResult(
                issue_id=issue.id,
                identifier=issue.identifier,
                status=status,
                error=self._format_error(exc),
                attempt=attempt,
                runtime_seconds=(utc_now() - started).total_seconds(),
            )
        finally:
            if session is not None and codex is not None:
                try:
                    codex.stop_session(session)
                except Exception:
                    pass
            if workspace is not None:
                try:
                    self._deps.workspace_manager.run_after_run(workspace)
                except Exception:
                    pass

    @staticmethod
    def _format_error(exc: Exception) -> str:
        if isinstance(exc, SymphonyError):
            return f"{exc.code}: {exc}"
        return str(exc)
