from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime
from typing import Any, Protocol

import requests

from .errors import (
    LinearApiRequestError,
    LinearApiStatusError,
    LinearGraphQLError,
    LinearMissingEndCursorError,
    LinearUnknownPayloadError,
)
from .models import BlockerRef, Issue, ServiceConfig

DEFAULT_PAGE_SIZE = 50
DEFAULT_TIMEOUT_MS = 30000


class TrackerClient(Protocol):
    def fetch_candidate_issues(self) -> list[Issue]: ...

    def fetch_issues_by_states(self, state_names: list[str]) -> list[Issue]: ...

    def fetch_issue_states_by_ids(self, issue_ids: list[str]) -> list[Issue]: ...


@dataclass(slots=True)
class LinearTrackerClient:
    config: ServiceConfig
    session: requests.Session | None = None
    page_size: int = DEFAULT_PAGE_SIZE
    timeout_ms: int = DEFAULT_TIMEOUT_MS
    _session: requests.Session = field(init=False, repr=False)

    def __post_init__(self) -> None:
        self._session = self.session or requests.Session()

    def fetch_candidate_issues(self) -> list[Issue]:
        query = """
        query CandidateIssues($projectSlug: String!, $activeStates: [String!], $after: String, $first: Int!) {
          projects(filter: { slugId: { eq: $projectSlug } }, first: 1) {
            nodes {
              issues(
                filter: { state: { name: { in: $activeStates } } }
                first: $first
                after: $after
              ) {
                nodes {
                  id
                  identifier
                  title
                  description
                  priority
                  branchName
                  url
                  createdAt
                  updatedAt
                  state { name }
                  labels { nodes { name } }
                  relations {
                    nodes {
                      type
                      relationType
                      inverse
                      isInverse
                      relatedIssue {
                        id
                        identifier
                        state { name }
                      }
                    }
                  }
                }
                pageInfo {
                  hasNextPage
                  endCursor
                }
              }
            }
          }
        }
        """
        variables: dict[str, Any] = {
            "projectSlug": self.config.tracker.project_slug,
            "activeStates": self.config.tracker.active_states,
            "after": None,
            "first": self.page_size,
        }

        issues: list[Issue] = []
        while True:
            payload = self._graphql(query, variables)
            projects = _deep_get(payload, "data", "projects", "nodes")
            if not isinstance(projects, list):
                raise LinearUnknownPayloadError("missing data.projects.nodes in Linear response")
            if not projects:
                return []

            issues_conn = projects[0].get("issues") if isinstance(projects[0], dict) else None
            if not isinstance(issues_conn, dict):
                raise LinearUnknownPayloadError("missing project issues connection")

            nodes = issues_conn.get("nodes")
            if not isinstance(nodes, list):
                raise LinearUnknownPayloadError("missing issues.nodes")

            issues.extend(self._normalize_issue(node) for node in nodes)

            page_info = issues_conn.get("pageInfo")
            if not isinstance(page_info, dict):
                raise LinearUnknownPayloadError("missing issues.pageInfo")

            if not bool(page_info.get("hasNextPage")):
                break

            cursor = page_info.get("endCursor")
            if not isinstance(cursor, str) or not cursor:
                raise LinearMissingEndCursorError("hasNextPage=true without endCursor")
            variables["after"] = cursor

        return issues

    def fetch_issues_by_states(self, state_names: list[str]) -> list[Issue]:
        if not state_names:
            return []

        query = """
        query IssuesByStates($projectSlug: String!, $states: [String!], $after: String, $first: Int!) {
          projects(filter: { slugId: { eq: $projectSlug } }, first: 1) {
            nodes {
              issues(filter: { state: { name: { in: $states } } }, first: $first, after: $after) {
                nodes {
                  id
                  identifier
                  title
                  state { name }
                  createdAt
                  updatedAt
                }
                pageInfo {
                  hasNextPage
                  endCursor
                }
              }
            }
          }
        }
        """
        variables: dict[str, Any] = {
            "projectSlug": self.config.tracker.project_slug,
            "states": state_names,
            "after": None,
            "first": self.page_size,
        }

        issues: list[Issue] = []
        while True:
            payload = self._graphql(query, variables)
            projects = _deep_get(payload, "data", "projects", "nodes")
            if not isinstance(projects, list):
                raise LinearUnknownPayloadError("missing data.projects.nodes in Linear response")
            if not projects:
                return []

            conn = projects[0].get("issues") if isinstance(projects[0], dict) else None
            if not isinstance(conn, dict):
                raise LinearUnknownPayloadError("missing issues connection")
            nodes = conn.get("nodes")
            if not isinstance(nodes, list):
                raise LinearUnknownPayloadError("missing issues.nodes")

            issues.extend(self._normalize_issue(node) for node in nodes)

            page_info = conn.get("pageInfo")
            if not isinstance(page_info, dict):
                raise LinearUnknownPayloadError("missing issues.pageInfo")
            if not page_info.get("hasNextPage"):
                break

            cursor = page_info.get("endCursor")
            if not isinstance(cursor, str) or not cursor:
                raise LinearMissingEndCursorError("hasNextPage=true without endCursor")
            variables["after"] = cursor

        return issues

    def fetch_issue_states_by_ids(self, issue_ids: list[str]) -> list[Issue]:
        if not issue_ids:
            return []

        query = """
        query IssueStatesByIds($ids: [ID!]) {
          issues(filter: { id: { in: $ids } }, first: 250) {
            nodes {
              id
              identifier
              title
              state { name }
              createdAt
              updatedAt
            }
          }
        }
        """
        payload = self._graphql(query, {"ids": issue_ids})
        nodes = _deep_get(payload, "data", "issues", "nodes")
        if not isinstance(nodes, list):
            raise LinearUnknownPayloadError("missing data.issues.nodes")
        return [self._normalize_issue(node) for node in nodes]

    def _graphql(self, query: str, variables: dict[str, Any]) -> dict[str, Any]:
        try:
            response = self._session.post(
                self.config.tracker.endpoint,
                json={"query": query, "variables": variables},
                headers={
                    "Authorization": self.config.tracker.api_key,
                    "Content-Type": "application/json",
                },
                timeout=self.timeout_ms / 1000,
            )
        except requests.RequestException as exc:
            raise LinearApiRequestError(f"Linear request failed: {exc}") from exc

        if response.status_code != 200:
            raise LinearApiStatusError(
                f"Linear API status={response.status_code}: {response.text[:200]}"
            )

        try:
            data = response.json()
        except ValueError as exc:
            raise LinearUnknownPayloadError("Linear response is not valid JSON") from exc

        if not isinstance(data, dict):
            raise LinearUnknownPayloadError("Linear response payload is not an object")

        errors = data.get("errors")
        if errors:
            raise LinearGraphQLError(f"Linear GraphQL returned errors: {errors}")

        return data

    def _normalize_issue(self, node: Any) -> Issue:
        if not isinstance(node, dict):
            raise LinearUnknownPayloadError("issue node is not an object")

        labels = []
        label_nodes = _deep_get(node, "labels", "nodes")
        if isinstance(label_nodes, list):
            for item in label_nodes:
                if isinstance(item, dict) and isinstance(item.get("name"), str):
                    labels.append(item["name"].strip().lower())

        blockers: list[BlockerRef] = []
        rel_nodes = _deep_get(node, "relations", "nodes")
        if isinstance(rel_nodes, list):
            for rel in rel_nodes:
                if not isinstance(rel, dict):
                    continue
                relation_type = str(rel.get("type") or rel.get("relationType") or "").strip().lower()
                inverse = bool(rel.get("inverse") or rel.get("isInverse"))
                if relation_type != "blocks" or not inverse:
                    continue
                related = rel.get("relatedIssue")
                if not isinstance(related, dict):
                    continue
                blockers.append(
                    BlockerRef(
                        id=_as_optional_str(related.get("id")),
                        identifier=_as_optional_str(related.get("identifier")),
                        state=_as_optional_str(_deep_get(related, "state", "name")),
                    )
                )

        priority = node.get("priority")
        if not isinstance(priority, int):
            priority = None

        return Issue(
            id=_required_str(node.get("id"), "issue.id"),
            identifier=_required_str(node.get("identifier"), "issue.identifier"),
            title=_required_str(node.get("title"), "issue.title", allow_empty=True),
            description=_as_optional_str(node.get("description")),
            priority=priority,
            state=_required_str(_deep_get(node, "state", "name"), "issue.state", allow_empty=True),
            branch_name=_as_optional_str(node.get("branchName")),
            url=_as_optional_str(node.get("url")),
            labels=labels,
            blocked_by=blockers,
            created_at=_parse_iso8601(node.get("createdAt")),
            updated_at=_parse_iso8601(node.get("updatedAt")),
        )


def _deep_get(container: Any, *path: str) -> Any:
    cursor = container
    for key in path:
        if not isinstance(cursor, dict):
            return None
        cursor = cursor.get(key)
    return cursor


def _required_str(value: Any, field_name: str, allow_empty: bool = False) -> str:
    if isinstance(value, str):
        text = value.strip()
        if text or allow_empty:
            return text
    raise LinearUnknownPayloadError(f"missing or invalid {field_name}")


def _as_optional_str(value: Any) -> str | None:
    if isinstance(value, str):
        text = value.strip()
        return text if text else None
    return None


def _parse_iso8601(value: Any) -> datetime | None:
    if not isinstance(value, str):
        return None
    text = value.strip()
    if not text:
        return None
    try:
        if text.endswith("Z"):
            text = text[:-1] + "+00:00"
        return datetime.fromisoformat(text)
    except ValueError:
        return None
