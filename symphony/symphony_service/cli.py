from __future__ import annotations

import argparse
import sys

from .config import load_service_config, validate_dispatch_config
from .logging_utils import configure_logging
from .orchestrator import Orchestrator
from .tracker import LinearTrackerClient
from .workflow import WorkflowStore, resolve_workflow_path
from .workspace import WorkspaceManager


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="symphony")
    parser.add_argument("workflow_path", nargs="?", help="path to WORKFLOW.md")
    parser.add_argument("--once", action="store_true", help="run a single orchestrator tick and exit")
    parser.add_argument(
        "--port",
        type=int,
        default=None,
        help="optional dashboard port override (extension, not implemented in this runtime)",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    logger = configure_logging()

    workflow_path = resolve_workflow_path(args.workflow_path)
    store = WorkflowStore(workflow_path)

    try:
        initial_workflow = store.load_initial()
        config = load_service_config(initial_workflow)
        if args.port is not None:
            config.server_port = args.port
        validate_dispatch_config(config)
    except Exception as exc:
        logger.error("action=startup_failed workflow=%s error=%s", workflow_path, exc)
        return 1

    tracker = LinearTrackerClient(config=config)
    workspace = WorkspaceManager(config=config, logger=logger)
    orchestrator = Orchestrator(
        workflow_store=store,
        tracker_client=tracker,
        workspace_manager=workspace,
        logger=logger,
    )

    try:
        orchestrator.startup()
        if args.once:
            orchestrator.tick()
            return 0
        orchestrator.run_forever()
        return 0
    except KeyboardInterrupt:
        logger.info("action=shutdown signal=keyboard_interrupt")
        return 0
    except Exception as exc:
        logger.error("action=runtime_failed error=%s", exc)
        return 1
    finally:
        orchestrator.stop()


if __name__ == "__main__":
    sys.exit(main())
