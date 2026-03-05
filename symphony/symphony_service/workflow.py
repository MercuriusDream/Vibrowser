from __future__ import annotations

from dataclasses import asdict, is_dataclass
from pathlib import Path
from threading import RLock
from typing import Any

import yaml
from jinja2 import Environment, StrictUndefined, TemplateAssertionError, TemplateSyntaxError
from jinja2.exceptions import UndefinedError

from .errors import (
    MissingWorkflowFileError,
    TemplateParseError,
    TemplateRenderError,
    WorkflowFrontMatterNotMapError,
    WorkflowParseError,
)
from .models import Issue, WorkflowDefinition


def resolve_workflow_path(explicit_path: str | Path | None, cwd: str | Path | None = None) -> Path:
    if explicit_path:
        return Path(explicit_path).expanduser().resolve()
    base = Path(cwd) if cwd is not None else Path.cwd()
    return (base / "WORKFLOW.md").resolve()


def load_workflow(path: Path) -> WorkflowDefinition:
    if not path.exists() or not path.is_file():
        raise MissingWorkflowFileError(f"missing workflow file at {path}")

    try:
        content = path.read_text(encoding="utf-8")
    except OSError as exc:
        raise MissingWorkflowFileError(f"cannot read workflow file at {path}: {exc}") from exc

    if content.startswith("---"):
        lines = content.splitlines()
        if not lines or lines[0].strip() != "---":
            raise WorkflowParseError("workflow front matter start marker is invalid")

        end_idx = None
        for idx in range(1, len(lines)):
            if lines[idx].strip() == "---":
                end_idx = idx
                break

        if end_idx is None:
            raise WorkflowParseError("workflow front matter is not terminated")

        yaml_blob = "\n".join(lines[1:end_idx])
        prompt_body = "\n".join(lines[end_idx + 1 :]).strip()

        try:
            raw = yaml.safe_load(yaml_blob) if yaml_blob.strip() else {}
        except yaml.YAMLError as exc:
            raise WorkflowParseError(f"workflow yaml parse error: {exc}") from exc

        if raw is None:
            raw = {}
        if not isinstance(raw, dict):
            raise WorkflowFrontMatterNotMapError("workflow front matter must be a map/object")

        return WorkflowDefinition(config=raw, prompt_template=prompt_body)

    return WorkflowDefinition(config={}, prompt_template=content.strip())


def _strict_environment() -> Environment:
    return Environment(undefined=StrictUndefined, autoescape=False)


def validate_template(template_text: str) -> None:
    try:
        _strict_environment().parse(template_text)
    except (TemplateSyntaxError, TemplateAssertionError) as exc:
        raise TemplateParseError(f"template parse failed: {exc}") from exc


def render_prompt(template_text: str, issue: Issue, attempt: int | None) -> str:
    env = _strict_environment()
    try:
        template = env.from_string(template_text)
    except (TemplateSyntaxError, TemplateAssertionError) as exc:
        raise TemplateParseError(f"template parse failed: {exc}") from exc

    if template_text.strip() == "":
        return "You are working on an issue from Linear."

    issue_payload = _to_template_data(issue)
    try:
        rendered = template.render(issue=issue_payload, attempt=attempt)
    except (UndefinedError, TemplateAssertionError, TypeError, ValueError) as exc:
        raise TemplateRenderError(f"template render failed: {exc}") from exc

    return rendered.strip()


def _to_template_data(value: Any) -> Any:
    if is_dataclass(value):
        return {str(k): _to_template_data(v) for k, v in asdict(value).items()}
    if isinstance(value, dict):
        return {str(k): _to_template_data(v) for k, v in value.items()}
    if isinstance(value, list):
        return [_to_template_data(v) for v in value]
    return value


class WorkflowStore:
    """Holds current workflow and supports dynamic reload while keeping last known good."""

    def __init__(self, workflow_path: Path):
        self._path = workflow_path
        self._lock = RLock()
        self._workflow: WorkflowDefinition | None = None
        self._mtime_ns: int | None = None

    @property
    def path(self) -> Path:
        return self._path

    def load_initial(self) -> WorkflowDefinition:
        workflow = load_workflow(self._path)
        validate_template(workflow.prompt_template)
        stat = self._path.stat()
        with self._lock:
            self._workflow = workflow
            self._mtime_ns = stat.st_mtime_ns
        return workflow

    def get(self) -> WorkflowDefinition:
        with self._lock:
            if self._workflow is None:
                raise MissingWorkflowFileError("workflow store not initialized")
            return self._workflow

    def reload_if_changed(self) -> tuple[bool, Exception | None]:
        try:
            stat = self._path.stat()
        except OSError as exc:
            return False, MissingWorkflowFileError(f"workflow stat failed: {exc}")

        with self._lock:
            if self._mtime_ns is not None and stat.st_mtime_ns == self._mtime_ns:
                return False, None

        try:
            updated = load_workflow(self._path)
            validate_template(updated.prompt_template)
        except Exception as exc:  # keep last known good config
            return False, exc

        with self._lock:
            self._workflow = updated
            self._mtime_ns = stat.st_mtime_ns
        return True, None
