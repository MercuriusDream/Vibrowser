from __future__ import annotations

from pathlib import Path

import pytest

from symphony_service.config import load_service_config
from symphony_service.errors import (
    MissingWorkflowFileError,
    TemplateRenderError,
    WorkflowFrontMatterNotMapError,
)
from symphony_service.models import BlockerRef, Issue
from symphony_service.workflow import load_workflow, render_prompt, resolve_workflow_path


def test_workflow_path_precedence(tmp_path: Path) -> None:
    explicit = tmp_path / "custom.md"
    resolved = resolve_workflow_path(explicit, cwd=tmp_path)
    assert resolved == explicit.resolve()

    default = resolve_workflow_path(None, cwd=tmp_path)
    assert default == (tmp_path / "WORKFLOW.md").resolve()


def test_missing_workflow_file_error(tmp_path: Path) -> None:
    with pytest.raises(MissingWorkflowFileError):
        load_workflow(tmp_path / "WORKFLOW.md")


def test_front_matter_must_be_map(tmp_path: Path) -> None:
    wf = tmp_path / "WORKFLOW.md"
    wf.write_text("---\n- bad\n---\nhello", encoding="utf-8")
    with pytest.raises(WorkflowFrontMatterNotMapError):
        load_workflow(wf)


def test_strict_prompt_rendering_unknown_variable_fails() -> None:
    issue = Issue(id="1", identifier="ABC-1", title="Example", state="Todo")
    with pytest.raises(TemplateRenderError):
        render_prompt("{{ issue.not_here }}", issue, None)


def test_prompt_renders_issue_and_attempt(tmp_path: Path) -> None:
    wf = tmp_path / "WORKFLOW.md"
    wf.write_text(
        "---\ntracker:\n  kind: linear\n  project_slug: demo\n---\n{{ issue.identifier }} {{ attempt }}",
        encoding="utf-8",
    )

    parsed = load_workflow(wf)
    issue = Issue(
        id="1",
        identifier="ABC-1",
        title="Example",
        state="Todo",
        blocked_by=[BlockerRef(state="Done")],
    )
    rendered = render_prompt(parsed.prompt_template, issue, 3)
    assert rendered == "ABC-1 3"


def test_config_env_and_defaults(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    monkeypatch.setenv("LINEAR_API_KEY", "k-test")
    monkeypatch.setenv("WORKSPACE_ROOT", str(tmp_path / "w"))

    wf = tmp_path / "WORKFLOW.md"
    wf.write_text(
        "---\n"
        "tracker:\n"
        "  kind: linear\n"
        "  project_slug: demo\n"
        "workspace:\n"
        "  root: $WORKSPACE_ROOT\n"
        "agent:\n"
        "  max_concurrent_agents_by_state:\n"
        "    In Progress: 2\n"
        "    Todo: 0\n"
        "codex:\n"
        "  command: codex app-server\n"
        "---\n"
        "hello",
        encoding="utf-8",
    )
    parsed = load_workflow(wf)
    cfg = load_service_config(parsed)

    assert cfg.tracker.api_key == "k-test"
    assert cfg.workspace.root == Path(tmp_path / "w")
    assert cfg.tracker.active_states == ["Todo", "In Progress"]
    assert cfg.agent.max_concurrent_agents_by_state == {"in progress": 2}
