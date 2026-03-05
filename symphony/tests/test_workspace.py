from __future__ import annotations

import logging
from pathlib import Path

import pytest

from symphony_service.models import (
    AgentConfig,
    CodexConfig,
    HooksConfig,
    PollingConfig,
    ServiceConfig,
    TrackerConfig,
    WorkspaceConfig,
)
from symphony_service.workspace import WorkspaceManager, sanitize_workspace_key


def _cfg(root: Path, hooks: HooksConfig) -> ServiceConfig:
    return ServiceConfig(
        tracker=TrackerConfig(
            kind="linear",
            endpoint="https://api.linear.app/graphql",
            api_key="x",
            project_slug="demo",
            active_states=["Todo", "In Progress"],
            terminal_states=["Done"],
        ),
        polling=PollingConfig(interval_ms=30000),
        workspace=WorkspaceConfig(root=root),
        hooks=hooks,
        agent=AgentConfig(),
        codex=CodexConfig(),
    )


def test_workspace_after_create_runs_once(tmp_path: Path) -> None:
    hooks = HooksConfig(after_create="echo created >> created.log", timeout_ms=1000)
    manager = WorkspaceManager(_cfg(tmp_path, hooks), logging.getLogger("test"))

    first = manager.ensure_workspace_for_issue("ABC-1")
    second = manager.ensure_workspace_for_issue("ABC-1")

    assert first.created_now is True
    assert second.created_now is False
    created_log = first.path / "created.log"
    assert created_log.read_text(encoding="utf-8").strip() == "created"


def test_before_run_failure_aborts(tmp_path: Path) -> None:
    hooks = HooksConfig(before_run="exit 7", timeout_ms=1000)
    manager = WorkspaceManager(_cfg(tmp_path, hooks), logging.getLogger("test"))
    workspace = manager.ensure_workspace_for_issue("ABC-2").path
    with pytest.raises(Exception):
        manager.run_before_run(workspace)


def test_after_run_failure_is_ignored(tmp_path: Path) -> None:
    hooks = HooksConfig(after_run="exit 2", timeout_ms=1000)
    manager = WorkspaceManager(_cfg(tmp_path, hooks), logging.getLogger("test"))
    workspace = manager.ensure_workspace_for_issue("ABC-3").path
    manager.run_after_run(workspace)


def test_before_remove_failure_is_ignored(tmp_path: Path) -> None:
    hooks = HooksConfig(before_remove="exit 9", timeout_ms=1000)
    manager = WorkspaceManager(_cfg(tmp_path, hooks), logging.getLogger("test"))
    workspace = manager.ensure_workspace_for_issue("ABC-4").path
    assert workspace.exists()
    manager.remove_workspace_for_identifier("ABC-4")
    assert not workspace.exists()


def test_workspace_key_sanitization() -> None:
    assert sanitize_workspace_key("ABC-1") == "ABC-1"
    assert sanitize_workspace_key("A/B:C*") == "A_B_C_"
