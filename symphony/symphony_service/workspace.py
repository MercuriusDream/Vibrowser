from __future__ import annotations

import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path

from .errors import (
    HookExecutionError,
    HookTimeoutError,
    InvalidWorkspaceCwdError,
    WorkspaceError,
)
from .models import ServiceConfig

MAX_HOOK_LOG_CHARS = 2048


@dataclass(slots=True)
class WorkspaceResult:
    path: Path
    workspace_key: str
    created_now: bool


class WorkspaceManager:
    def __init__(self, config: ServiceConfig, logger):
        self._config = config
        self._logger = logger

    @property
    def root(self) -> Path:
        return self._config.workspace.root

    def update_config(self, config: ServiceConfig) -> None:
        self._config = config

    def ensure_workspace_for_issue(self, issue_identifier: str) -> WorkspaceResult:
        key = sanitize_workspace_key(issue_identifier)
        workspace = self.root / key

        self._ensure_root_exists()
        self._validate_containment(workspace)

        created_now = False
        if workspace.exists() and not workspace.is_dir():
            raise WorkspaceError(f"workspace path exists as non-directory: {workspace}")
        if not workspace.exists():
            workspace.mkdir(parents=True, exist_ok=False)
            created_now = True

        self._cleanup_transient_artifacts(workspace)

        if created_now and self._config.hooks.after_create:
            self._run_hook("after_create", self._config.hooks.after_create, workspace, fatal=True)

        return WorkspaceResult(path=workspace, workspace_key=key, created_now=created_now)

    def run_before_run(self, workspace: Path) -> None:
        script = self._config.hooks.before_run
        if script:
            self._run_hook("before_run", script, workspace, fatal=True)

    def run_after_run(self, workspace: Path) -> None:
        script = self._config.hooks.after_run
        if script:
            self._run_hook("after_run", script, workspace, fatal=False)

    def remove_workspace_for_identifier(self, issue_identifier: str) -> None:
        key = sanitize_workspace_key(issue_identifier)
        workspace = self.root / key
        self._validate_containment(workspace)

        if not workspace.exists():
            return

        if self._config.hooks.before_remove:
            self._run_hook("before_remove", self._config.hooks.before_remove, workspace, fatal=False)

        shutil.rmtree(workspace, ignore_errors=False)
        self._logger.info(
            "action=workspace_removed workspace=%s workspace_key=%s",
            workspace,
            key,
        )

    def validate_agent_cwd(self, workspace: Path) -> None:
        self._validate_containment(workspace)
        if not workspace.exists() or not workspace.is_dir():
            raise InvalidWorkspaceCwdError(f"workspace cwd does not exist or is not directory: {workspace}")

    def _ensure_root_exists(self) -> None:
        self.root.mkdir(parents=True, exist_ok=True)
        if not self.root.is_dir():
            raise WorkspaceError(f"workspace root is not a directory: {self.root}")

    def _validate_containment(self, workspace_path: Path) -> None:
        root_abs = self.root.expanduser().resolve()
        path_abs = workspace_path.expanduser().resolve()

        try:
            path_abs.relative_to(root_abs)
        except ValueError as exc:
            raise InvalidWorkspaceCwdError(
                f"workspace path escapes root: root={root_abs} path={path_abs}"
            ) from exc

    def _cleanup_transient_artifacts(self, workspace: Path) -> None:
        for relative in ("tmp", ".elixir_ls"):
            candidate = workspace / relative
            if candidate.exists() and candidate.is_dir():
                shutil.rmtree(candidate, ignore_errors=True)

    def _run_hook(self, name: str, script: str, cwd: Path, fatal: bool) -> None:
        timeout = self._config.hooks.timeout_ms / 1000
        self._logger.info("action=hook_start hook=%s cwd=%s", name, cwd)
        try:
            completed = subprocess.run(
                ["bash", "-lc", script],
                cwd=cwd,
                capture_output=True,
                text=True,
                timeout=timeout,
                check=False,
            )
        except subprocess.TimeoutExpired as exc:
            error = HookTimeoutError(f"hook={name} timed out after {self._config.hooks.timeout_ms}ms")
            if fatal:
                raise error from exc
            self._logger.warning("action=hook_timeout hook=%s error=%s", name, error)
            return

        if completed.returncode != 0:
            stderr = (completed.stderr or "")[:MAX_HOOK_LOG_CHARS]
            stdout = (completed.stdout or "")[:MAX_HOOK_LOG_CHARS]
            error = HookExecutionError(
                f"hook={name} failed code={completed.returncode} stdout={stdout!r} stderr={stderr!r}"
            )
            if fatal:
                raise error
            self._logger.warning("action=hook_failed hook=%s error=%s", name, error)
            return

        self._logger.info("action=hook_completed hook=%s cwd=%s", name, cwd)


def sanitize_workspace_key(identifier: str) -> str:
    return "".join(ch if ch.isalnum() or ch in "._-" else "_" for ch in identifier)
