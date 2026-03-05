---
tracker:
  kind: linear
  endpoint: https://api.linear.app/graphql
  api_key: $LINEAR_API_KEY
  project_slug: testslug
  active_states: Todo, In Progress
  terminal_states: Closed, Cancelled, Canceled, Duplicate, Done

polling:
  interval_ms: 30000

workspace:
  root: /tmp/symphony_workspaces

hooks:
  timeout_ms: 60000
  after_create: |
    echo "workspace initialized"
  before_run: |
    echo "starting run"

agent:
  max_concurrent_agents: 10
  max_turns: 20
  max_retry_backoff_ms: 300000

codex:
  command: codex app-server
  turn_timeout_ms: 3600000
  read_timeout_ms: 5000
  stall_timeout_ms: 300000
---

You are working issue {{ issue.identifier }}: {{ issue.title }}.

Attempt: {{ attempt }}

Implement the requested changes, run tests, and leave the issue in the workflow-defined handoff state.
