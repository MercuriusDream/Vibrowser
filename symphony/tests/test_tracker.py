from __future__ import annotations

from pathlib import Path

from symphony_service.models import (
    AgentConfig,
    CodexConfig,
    HooksConfig,
    PollingConfig,
    ServiceConfig,
    TrackerConfig,
    WorkspaceConfig,
)
from symphony_service.tracker import LinearTrackerClient


class _Response:
    def __init__(self, status_code: int, payload: dict):
        self.status_code = status_code
        self._payload = payload
        self.text = ""

    def json(self):
        return self._payload


class _Session:
    def __init__(self, responses):
        self._responses = list(responses)
        self.calls = []

    def post(self, url, json, headers, timeout):
        self.calls.append({"url": url, "json": json, "headers": headers, "timeout": timeout})
        return self._responses.pop(0)


def _cfg(tmp_path: Path) -> ServiceConfig:
    return ServiceConfig(
        tracker=TrackerConfig(
            kind="linear",
            endpoint="https://api.linear.app/graphql",
            api_key="tok",
            project_slug="demo",
            active_states=["Todo", "In Progress"],
            terminal_states=["Done", "Canceled"],
        ),
        polling=PollingConfig(interval_ms=30000),
        workspace=WorkspaceConfig(root=tmp_path),
        hooks=HooksConfig(),
        agent=AgentConfig(),
        codex=CodexConfig(),
    )


def test_fetch_by_states_empty_does_not_call_api(tmp_path: Path) -> None:
    session = _Session([])
    client = LinearTrackerClient(config=_cfg(tmp_path), session=session)
    assert client.fetch_issues_by_states([]) == []
    assert session.calls == []


def test_candidate_pagination_and_normalization(tmp_path: Path) -> None:
    page1 = {
        "data": {
            "projects": {
                "nodes": [
                    {
                        "issues": {
                            "nodes": [
                                {
                                    "id": "1",
                                    "identifier": "ABC-1",
                                    "title": "Title",
                                    "description": "Desc",
                                    "priority": 2,
                                    "branchName": "feat/abc",
                                    "url": "https://example.com",
                                    "createdAt": "2026-01-01T00:00:00Z",
                                    "updatedAt": "2026-01-01T01:00:00Z",
                                    "state": {"name": "Todo"},
                                    "labels": {"nodes": [{"name": "Urgent"}]},
                                    "relations": {
                                        "nodes": [
                                            {
                                                "type": "blocks",
                                                "inverse": True,
                                                "relatedIssue": {
                                                    "id": "9",
                                                    "identifier": "ABC-9",
                                                    "state": {"name": "In Progress"},
                                                },
                                            }
                                        ]
                                    },
                                }
                            ],
                            "pageInfo": {"hasNextPage": True, "endCursor": "c1"},
                        }
                    }
                ]
            }
        }
    }
    page2 = {
        "data": {
            "projects": {
                "nodes": [
                    {
                        "issues": {
                            "nodes": [],
                            "pageInfo": {"hasNextPage": False, "endCursor": None},
                        }
                    }
                ]
            }
        }
    }

    session = _Session([_Response(200, page1), _Response(200, page2)])
    client = LinearTrackerClient(config=_cfg(tmp_path), session=session)

    items = client.fetch_candidate_issues()
    assert len(items) == 1
    issue = items[0]
    assert issue.labels == ["urgent"]
    assert len(issue.blocked_by) == 1
    assert issue.blocked_by[0].identifier == "ABC-9"
    assert len(session.calls) == 2
