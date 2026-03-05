from __future__ import annotations

import logging
from datetime import timedelta
from pathlib import Path

from symphony_service.config import load_service_config
from symphony_service.models import BlockerRef, Issue
from symphony_service.orchestrator import Orchestrator, _dispatch_sort_key
from symphony_service.workflow import WorkflowStore
from symphony_service.workspace import WorkspaceManager


class _TrackerStub:
    def __init__(self):
        self.candidates = []

    def fetch_candidate_issues(self):
        return list(self.candidates)

    def fetch_issues_by_states(self, _states):
        return []

    def fetch_issue_states_by_ids(self, _ids):
        return []


def _build_orchestrator(tmp_path: Path) -> Orchestrator:
    wf = tmp_path / "WORKFLOW.md"
    wf.write_text(
        "---\n"
        "tracker:\n"
        "  kind: linear\n"
        "  project_slug: demo\n"
        "  api_key: token\n"
        "workspace:\n"
        f"  root: {tmp_path / 'workspaces'}\n"
        "agent:\n"
        "  max_retry_backoff_ms: 300000\n"
        "codex:\n"
        "  command: codex app-server\n"
        "---\n"
        "{{ issue.identifier }}",
        encoding="utf-8",
    )

    store = WorkflowStore(wf)
    cfg = load_service_config(store.load_initial())
    workspace = WorkspaceManager(cfg, logging.getLogger("test"))
    tracker = _TrackerStub()

    orch = Orchestrator(store, tracker, workspace, logging.getLogger("test"))
    orch.startup()
    return orch


def test_dispatch_sort_key() -> None:
    a = Issue(id="1", identifier="A", title="a", state="Todo", priority=2)
    b = Issue(id="2", identifier="B", title="b", state="Todo", priority=1)
    c = Issue(id="3", identifier="C", title="c", state="Todo", priority=None)

    ordered = sorted([a, c, b], key=_dispatch_sort_key)
    assert [i.identifier for i in ordered] == ["B", "A", "C"]


def test_todo_with_non_terminal_blocker_not_eligible(tmp_path: Path) -> None:
    orch = _build_orchestrator(tmp_path)

    blocked = Issue(
        id="1",
        identifier="ABC-1",
        title="Work",
        state="Todo",
        blocked_by=[BlockerRef(id="2", identifier="ABC-2", state="In Progress")],
    )
    assert orch._should_dispatch(blocked) is False

    ready = Issue(
        id="3",
        identifier="ABC-3",
        title="Work",
        state="Todo",
        blocked_by=[BlockerRef(id="4", identifier="ABC-4", state="Done")],
    )
    assert orch._should_dispatch(ready) is True


def test_retry_backoff_cap_and_continuation_delay(tmp_path: Path) -> None:
    orch = _build_orchestrator(tmp_path)

    start = orch.snapshot()["generated_at"]
    _ = start

    now = orch._retry_attempts.get("i1")
    assert now is None

    before = orch.snapshot()["generated_at"]
    _ = before

    t0 = orch._require_config()
    _ = t0

    from symphony_service.utils import utc_now

    anchor = utc_now()
    orch._schedule_retry("i1", "ABC-1", attempt=10, continuation=False, error="x")
    capped = orch._retry_attempts["i1"]
    delay = capped.due_at - anchor
    assert delay <= timedelta(milliseconds=300000, seconds=1)
    assert delay >= timedelta(milliseconds=299000)

    anchor2 = utc_now()
    orch._schedule_retry("i2", "ABC-2", attempt=1, continuation=True, error=None)
    continuation = orch._retry_attempts["i2"]
    delay2 = continuation.due_at - anchor2
    assert timedelta(milliseconds=800) <= delay2 <= timedelta(milliseconds=1500)
