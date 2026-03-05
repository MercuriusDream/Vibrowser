from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path
from typing import Any


@dataclass(slots=True)
class BlockerRef:
    id: str | None = None
    identifier: str | None = None
    state: str | None = None


@dataclass(slots=True)
class Issue:
    id: str
    identifier: str
    title: str
    description: str | None = None
    priority: int | None = None
    state: str = ""
    branch_name: str | None = None
    url: str | None = None
    labels: list[str] = field(default_factory=list)
    blocked_by: list[BlockerRef] = field(default_factory=list)
    created_at: datetime | None = None
    updated_at: datetime | None = None


@dataclass(slots=True)
class WorkflowDefinition:
    config: dict[str, Any]
    prompt_template: str


@dataclass(slots=True)
class TrackerConfig:
    kind: str
    endpoint: str
    api_key: str
    project_slug: str
    active_states: list[str]
    terminal_states: list[str]


@dataclass(slots=True)
class PollingConfig:
    interval_ms: int


@dataclass(slots=True)
class WorkspaceConfig:
    root: Path


@dataclass(slots=True)
class HooksConfig:
    after_create: str | None = None
    before_run: str | None = None
    after_run: str | None = None
    before_remove: str | None = None
    timeout_ms: int = 60000


@dataclass(slots=True)
class AgentConfig:
    max_concurrent_agents: int = 10
    max_turns: int = 20
    max_retry_backoff_ms: int = 300000
    max_concurrent_agents_by_state: dict[str, int] = field(default_factory=dict)


@dataclass(slots=True)
class CodexConfig:
    command: str = "codex app-server"
    approval_policy: Any = None
    thread_sandbox: Any = None
    turn_sandbox_policy: Any = None
    turn_timeout_ms: int = 3600000
    read_timeout_ms: int = 5000
    stall_timeout_ms: int = 300000


@dataclass(slots=True)
class ServiceConfig:
    tracker: TrackerConfig
    polling: PollingConfig
    workspace: WorkspaceConfig
    hooks: HooksConfig
    agent: AgentConfig
    codex: CodexConfig
    server_port: int | None = None


@dataclass(slots=True)
class RetryEntry:
    issue_id: str
    identifier: str
    attempt: int
    error: str | None
    due_at: datetime


@dataclass(slots=True)
class LiveSession:
    session_id: str
    thread_id: str
    turn_id: str
    codex_app_server_pid: str | None = None
    last_codex_event: str | None = None
    last_codex_timestamp: datetime | None = None
    last_codex_message: str | None = None
    codex_input_tokens: int = 0
    codex_output_tokens: int = 0
    codex_total_tokens: int = 0
    last_reported_input_tokens: int = 0
    last_reported_output_tokens: int = 0
    last_reported_total_tokens: int = 0
    turn_count: int = 0


@dataclass(slots=True)
class WorkerResult:
    issue_id: str
    identifier: str
    status: str
    error: str | None = None
    attempt: int | None = None
    runtime_seconds: float = 0.0
