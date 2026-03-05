from __future__ import annotations

from pathlib import Path

from symphony_service.codex_app_server import CodexAppServerClient
from symphony_service.models import CodexConfig, Issue


def test_handshake_and_turn_completion(tmp_path: Path) -> None:
    server = tmp_path / "fake_codex.py"
    server.write_text(
        """
import json, sys

for raw in sys.stdin:
    msg = json.loads(raw)
    method = msg.get('method')
    if method == 'initialize':
        print(json.dumps({'id': msg['id'], 'result': {'ok': True}}), flush=True)
    elif method == 'initialized':
        pass
    elif method == 'thread/start':
        print(json.dumps({'id': msg['id'], 'result': {'thread': {'id': 'thread-1'}}}), flush=True)
    elif method == 'turn/start':
        print(json.dumps({'id': msg['id'], 'result': {'turn': {'id': 'turn-1'}}}), flush=True)
        print(json.dumps({'method': 'thread/tokenUsage/updated', 'params': {'total_token_usage': {'input_tokens': 2, 'output_tokens': 3, 'total_tokens': 5}}}), flush=True)
        print(json.dumps({'method': 'turn/completed', 'params': {}}), flush=True)
        break
""".strip(),
        encoding="utf-8",
    )

    events = []
    client = CodexAppServerClient(
        codex_config=CodexConfig(
            command=f"python3 {server}",
            read_timeout_ms=2000,
            turn_timeout_ms=5000,
        ),
        workspace=tmp_path,
        logger=type("L", (), {"warning": lambda *args, **kwargs: None})(),
        event_callback=lambda evt: events.append(evt),
    )

    session = client.start_session()
    issue = Issue(id="i1", identifier="ABC-1", title="Demo", state="In Progress")
    outcome = client.run_turn(session=session, issue=issue, prompt="hello")
    client.stop_session(session)

    assert outcome.status == "completed"
    kinds = {evt.get("event") for evt in events}
    assert "session_started" in kinds
    assert "turn_completed" in kinds
