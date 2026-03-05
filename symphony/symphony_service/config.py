from __future__ import annotations

import os
import tempfile
from pathlib import Path
from typing import Any

from .errors import ConfigValidationError
from .models import (
    AgentConfig,
    CodexConfig,
    HooksConfig,
    PollingConfig,
    ServiceConfig,
    TrackerConfig,
    WorkspaceConfig,
    WorkflowDefinition,
)
from .utils import coerce_int, normalize_state

LINEAR_ENDPOINT = "https://api.linear.app/graphql"
DEFAULT_ACTIVE_STATES = ["Todo", "In Progress"]
DEFAULT_TERMINAL_STATES = ["Closed", "Cancelled", "Canceled", "Duplicate", "Done"]


def load_service_config(
    workflow: WorkflowDefinition,
    env: dict[str, str] | None = None,
) -> ServiceConfig:
    values = workflow.config
    env_map = env if env is not None else dict(os.environ)

    tracker_raw = _as_map(values.get("tracker"))
    polling_raw = _as_map(values.get("polling"))
    workspace_raw = _as_map(values.get("workspace"))
    hooks_raw = _as_map(values.get("hooks"))
    agent_raw = _as_map(values.get("agent"))
    codex_raw = _as_map(values.get("codex"))
    server_raw = _as_map(values.get("server"))

    tracker_kind = str(tracker_raw.get("kind") or "").strip()
    tracker_endpoint = str(tracker_raw.get("endpoint") or LINEAR_ENDPOINT).strip()
    tracker_api_key = _resolve_secret(
        tracker_raw.get("api_key"),
        env_map,
        fallback_env_var="LINEAR_API_KEY" if tracker_kind == "linear" else None,
    )
    tracker_project_slug = str(tracker_raw.get("project_slug") or "").strip()
    tracker_active_states = _to_state_list(tracker_raw.get("active_states"), DEFAULT_ACTIVE_STATES)
    tracker_terminal_states = _to_state_list(
        tracker_raw.get("terminal_states"), DEFAULT_TERMINAL_STATES
    )

    polling_interval_ms = max(coerce_int(polling_raw.get("interval_ms"), 30000), 100)

    workspace_root_value = workspace_raw.get("root")
    workspace_root = _resolve_path(workspace_root_value, env_map)
    if workspace_root is None:
        workspace_root = Path(tempfile.gettempdir()) / "symphony_workspaces"

    hooks_timeout = coerce_int(hooks_raw.get("timeout_ms"), 60000)
    if hooks_timeout <= 0:
        hooks_timeout = 60000

    state_limits: dict[str, int] = {}
    raw_limits = agent_raw.get("max_concurrent_agents_by_state")
    if isinstance(raw_limits, dict):
        for key, raw in raw_limits.items():
            limit = coerce_int(raw, -1)
            if limit > 0:
                state_limits[normalize_state(str(key))] = limit

    server_port = None
    if "port" in server_raw:
        port_value = coerce_int(server_raw.get("port"), -1)
        if port_value >= 0:
            server_port = port_value

    cfg = ServiceConfig(
        tracker=TrackerConfig(
            kind=tracker_kind,
            endpoint=tracker_endpoint,
            api_key=tracker_api_key,
            project_slug=tracker_project_slug,
            active_states=tracker_active_states,
            terminal_states=tracker_terminal_states,
        ),
        polling=PollingConfig(interval_ms=polling_interval_ms),
        workspace=WorkspaceConfig(root=workspace_root),
        hooks=HooksConfig(
            after_create=_as_script(hooks_raw.get("after_create")),
            before_run=_as_script(hooks_raw.get("before_run")),
            after_run=_as_script(hooks_raw.get("after_run")),
            before_remove=_as_script(hooks_raw.get("before_remove")),
            timeout_ms=hooks_timeout,
        ),
        agent=AgentConfig(
            max_concurrent_agents=max(coerce_int(agent_raw.get("max_concurrent_agents"), 10), 1),
            max_turns=max(coerce_int(agent_raw.get("max_turns"), 20), 1),
            max_retry_backoff_ms=max(coerce_int(agent_raw.get("max_retry_backoff_ms"), 300000), 1000),
            max_concurrent_agents_by_state=state_limits,
        ),
        codex=CodexConfig(
            command=str(codex_raw.get("command") or "codex app-server").strip(),
            approval_policy=codex_raw.get("approval_policy"),
            thread_sandbox=codex_raw.get("thread_sandbox"),
            turn_sandbox_policy=codex_raw.get("turn_sandbox_policy"),
            turn_timeout_ms=max(coerce_int(codex_raw.get("turn_timeout_ms"), 3600000), 1000),
            read_timeout_ms=max(coerce_int(codex_raw.get("read_timeout_ms"), 5000), 100),
            stall_timeout_ms=coerce_int(codex_raw.get("stall_timeout_ms"), 300000),
        ),
        server_port=server_port,
    )
    return cfg


def validate_dispatch_config(config: ServiceConfig) -> None:
    if config.tracker.kind != "linear":
        raise ConfigValidationError("tracker.kind must be supported and currently equals 'linear'")
    if not config.tracker.api_key:
        raise ConfigValidationError("tracker.api_key must be present after $ resolution")
    if not config.tracker.project_slug:
        raise ConfigValidationError("tracker.project_slug is required")
    if not config.codex.command:
        raise ConfigValidationError("codex.command must be non-empty")


def is_terminal_state(config: ServiceConfig, state: str) -> bool:
    normalized = normalize_state(state)
    return normalized in {normalize_state(s) for s in config.tracker.terminal_states}


def is_active_state(config: ServiceConfig, state: str) -> bool:
    normalized = normalize_state(state)
    return normalized in {normalize_state(s) for s in config.tracker.active_states}


def _as_map(value: Any) -> dict[str, Any]:
    return value if isinstance(value, dict) else {}


def _resolve_secret(raw: Any, env_map: dict[str, str], fallback_env_var: str | None) -> str:
    if isinstance(raw, str):
        raw = raw.strip()
        if raw.startswith("$"):
            return env_map.get(raw[1:], "").strip()
        return raw

    if fallback_env_var:
        return env_map.get(fallback_env_var, "").strip()

    return ""


def _resolve_path(raw: Any, env_map: dict[str, str]) -> Path | None:
    if raw is None:
        return None

    value = str(raw).strip()
    if not value:
        return None

    if value.startswith("$"):
        value = env_map.get(value[1:], "").strip()
        if not value:
            return None

    path_like = (
        value.startswith("~")
        or value.startswith(".")
        or "/" in value
        or "\\" in value
    )
    if path_like:
        return Path(os.path.expandvars(value)).expanduser()

    return Path(value)


def _to_state_list(raw: Any, defaults: list[str]) -> list[str]:
    if isinstance(raw, str):
        values = [item.strip() for item in raw.split(",") if item.strip()]
        return values or list(defaults)
    if isinstance(raw, list):
        values = [str(item).strip() for item in raw if str(item).strip()]
        return values or list(defaults)
    return list(defaults)


def _as_script(raw: Any) -> str | None:
    if raw is None:
        return None
    text = str(raw).strip()
    return text if text else None
