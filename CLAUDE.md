
## Screenshot (Always Available)

To capture vibrowser output, run: `screencapture -x -D2 /tmp/vibrowser_screenshot.png` then read the PNG with the Read tool. **Display 2 is the default** (where vibrowser launches). No need to invoke `/screenshot` — just capture directly whenever you need to see the browser output.

## USE Codex AS Subagent AS ALWAYS SINCE SONNETS COST A LOT

Use the custom `codex` agent instead of the `/codex` skill to keep context clean:

```
Agent(subagent_type="codex", model="haiku", prompt="your task here")
Agent(subagent_type="codex-spark", model="haiku", prompt="your task here")
```

Haiku orchestrates the codex exec call and returns a short analysis. Spawn multiple in parallel for swarm work.
