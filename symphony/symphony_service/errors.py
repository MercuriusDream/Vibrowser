from __future__ import annotations


class SymphonyError(Exception):
    """Base error for Symphony service."""

    code = "symphony_error"

    def __init__(self, message: str):
        super().__init__(message)
        self.message = message


class WorkflowError(SymphonyError):
    """Workflow read/parse/render error."""


class MissingWorkflowFileError(WorkflowError):
    code = "missing_workflow_file"


class WorkflowParseError(WorkflowError):
    code = "workflow_parse_error"


class WorkflowFrontMatterNotMapError(WorkflowError):
    code = "workflow_front_matter_not_a_map"


class TemplateParseError(WorkflowError):
    code = "template_parse_error"


class TemplateRenderError(WorkflowError):
    code = "template_render_error"


class ConfigValidationError(SymphonyError):
    code = "config_validation_error"


class TrackerError(SymphonyError):
    code = "tracker_error"


class UnsupportedTrackerKindError(TrackerError):
    code = "unsupported_tracker_kind"


class MissingTrackerApiKeyError(TrackerError):
    code = "missing_tracker_api_key"


class MissingTrackerProjectSlugError(TrackerError):
    code = "missing_tracker_project_slug"


class LinearApiRequestError(TrackerError):
    code = "linear_api_request"


class LinearApiStatusError(TrackerError):
    code = "linear_api_status"


class LinearGraphQLError(TrackerError):
    code = "linear_graphql_errors"


class LinearUnknownPayloadError(TrackerError):
    code = "linear_unknown_payload"


class LinearMissingEndCursorError(TrackerError):
    code = "linear_missing_end_cursor"


class WorkspaceError(SymphonyError):
    code = "workspace_error"


class InvalidWorkspaceCwdError(WorkspaceError):
    code = "invalid_workspace_cwd"


class HookTimeoutError(WorkspaceError):
    code = "hook_timeout"


class HookExecutionError(WorkspaceError):
    code = "hook_execution_error"


class AgentProtocolError(SymphonyError):
    code = "agent_protocol_error"


class CodexNotFoundError(AgentProtocolError):
    code = "codex_not_found"


class ResponseTimeoutError(AgentProtocolError):
    code = "response_timeout"


class TurnTimeoutError(AgentProtocolError):
    code = "turn_timeout"


class PortExitError(AgentProtocolError):
    code = "port_exit"


class ResponseError(AgentProtocolError):
    code = "response_error"


class TurnFailedError(AgentProtocolError):
    code = "turn_failed"


class TurnCancelledError(AgentProtocolError):
    code = "turn_cancelled"


class TurnInputRequiredError(AgentProtocolError):
    code = "turn_input_required"
