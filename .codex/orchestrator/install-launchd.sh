#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=.codex/orchestrator/lib.sh
source "$SCRIPT_DIR/lib.sh"

PLIST_DIR="$HOME/Library/LaunchAgents"
PLIST_PATH="$PLIST_DIR/com.vibrowser.codex-estate.plist"
STDOUT_LOG="$LOG_DIR/launchd.stdout.log"
STDERR_LOG="$LOG_DIR/launchd.stderr.log"

mkdir -p "$PLIST_DIR" "$LOG_DIR"

cat > "$PLIST_PATH" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key>
  <string>com.vibrowser.codex-estate</string>
  <key>ProgramArguments</key>
  <array>
    <string>/bin/zsh</string>
    <string>-lc</string>
    <string>bash "$PROJECT_DIR/.codex/orchestrator/supervisor-runner.sh"</string>
  </array>
  <key>WorkingDirectory</key>
  <string>$PROJECT_DIR</string>
  <key>RunAtLoad</key>
  <true/>
  <key>KeepAlive</key>
  <true/>
  <key>StandardOutPath</key>
  <string>$STDOUT_LOG</string>
  <key>StandardErrorPath</key>
  <string>$STDERR_LOG</string>
  <key>EnvironmentVariables</key>
  <dict>
    <key>CODEX_ESTATE_MAIN_MODEL</key>
    <string>$MAIN_MODEL</string>
    <key>CODEX_ESTATE_MAIN_REASONING</key>
    <string>$MAIN_REASONING</string>
    <key>CODEX_ESTATE_MAIN_FAST_FLAG</key>
    <string>$MAIN_FAST_FLAG</string>
    <key>CODEX_ESTATE_SUBAGENT_MODEL</key>
    <string>$SUBAGENT_MODEL</string>
    <key>CODEX_ESTATE_SUBAGENT_REASONING</key>
    <string>$SUBAGENT_REASONING</string>
    <key>CODEX_ESTATE_SUBAGENT_FAST_FLAG</key>
    <string>$SUBAGENT_FAST_FLAG</string>
    <key>CODEX_ESTATE_WORKERS</key>
    <string>$WORKER_COUNT</string>
    <key>CODEX_ESTATE_AUTOGIT</key>
    <string>$AUTO_GIT</string>
    <key>CODEX_ESTATE_PUSH</key>
    <string>$AUTO_PUSH</string>
    <key>CODEX_ESTATE_CREATE_PR</key>
    <string>$AUTO_PR</string>
    <key>CODEX_ESTATE_WAIT_FOR_CI</key>
    <string>$AUTO_CI_WAIT</string>
    <key>CODEX_ESTATE_CI_AUTOFIX</key>
    <string>$AUTO_CI_FIX</string>
    <key>CODEX_ALLOW_KNOWN_BASELINE_FAILURES</key>
    <string>${CODEX_ALLOW_KNOWN_BASELINE_FAILURES:-0}</string>
  </dict>
</dict>
</plist>
EOF

launchctl unload "$PLIST_PATH" >/dev/null 2>&1 || true
launchctl load "$PLIST_PATH"
echo "Installed launchd agent: $PLIST_PATH"
