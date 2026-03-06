#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXECUTE=0

if [[ "${1:-}" == "--execute" ]]; then
  EXECUTE=1
fi

cat <<'EOF'
Codex Estate safe restart plan

1. Detect current writer:
   bash .codex/orchestrator/detect-writer.sh

2. Stop tmux-owned sessions:
   bash .codex/orchestrator/tmux-stop.sh

3. Unload launchd agent:
   bash .codex/orchestrator/uninstall-launchd.sh

4. Verify nothing is still writing:
   bash .codex/orchestrator/detect-writer.sh

5. Start a fresh zsh entry session:
   zsh .codex/orchestrator/start-full-autonomy-fast.zsh

6. Reinstall launchd only after the fresh session looks healthy:
   bash .codex/orchestrator/install-launchd.sh
EOF

if [[ $EXECUTE -eq 0 ]]; then
  exit 0
fi

bash "$SCRIPT_DIR/detect-writer.sh"
bash "$SCRIPT_DIR/tmux-stop.sh"
bash "$SCRIPT_DIR/uninstall-launchd.sh"
bash "$SCRIPT_DIR/detect-writer.sh"
echo "Safe restart stop phase completed. Start phase is intentionally not auto-invoked."
