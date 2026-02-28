
## Screenshot (Always Available)

To capture vibrowser output, run: `screencapture -x -D2 /tmp/vibrowser_screenshot.png` then read the PNG with the Read tool. **Display 2 is the default** (where vibrowser launches). No need to invoke `/screenshot` — just capture directly whenever you need to see the browser output.

## Codex Subagent

Use the custom `codex` agent instead of the `/codex` skill to keep context clean:
```
Agent(subagent_type="codex", model="haiku", prompt="your task here")
```
Haiku orchestrates the codex exec call and returns a short analysis. Spawn multiple in parallel for swarm work.

## BMAD-METHOD Integration

Use `/bmalph` to navigate phases. Use `/bmad-help` to discover all commands. Use `/bmalph-status` or `/b-malph` for a quick status and implementation loop handoff into Claude Code.

### Phases

| Phase | Focus | Key Commands |
|-------|-------|-------------|
| 1. Analysis | Understand the problem | `/create-brief`, `/brainstorm-project`, `/market-research` |
| 2. Planning | Define the solution | `/create-prd`, `/create-ux` |
| 3. Solutioning | Design the architecture | `/create-architecture`, `/create-epics-stories`, `/implementation-readiness` |
| 4. Implementation | Build it | `/sprint-planning`, `/create-story`, then `/bmalph-implement` for Ralph |

### Workflow

1. Work through Phases 1-3 using BMAD agents and workflows (interactive, command-driven)
2. Run `/bmalph-implement` to transition planning artifacts into Ralph format, then start Ralph with `/b-malph`

### Management Commands

| Command | Description |
|---------|-------------|
| `/bmalph-status` | Show current BMAD phase and Ralph loop status |
| `/bmalph-implement` | Transition planning artifacts → prepare Ralph loop |
| `/b-malph` | Start/continue implementation loop in Claude Code while preserving bmalph phase status |
| `/bmalph-upgrade` | Update bundled assets to match current bmalph version |
| `/bmalph-doctor` | Check project health and report issues |
| `/bmalph-reset` | Reset state (soft or hard reset with confirmation) |

### Available Agents

| Command | Agent | Role |
|---------|-------|------|
| `/analyst` | Analyst | Research, briefs, discovery |
| `/architect` | Architect | Technical design, architecture |
| `/pm` | Product Manager | PRDs, epics, stories |
| `/sm` | Scrum Master | Sprint planning, status, coordination |
| `/dev` | Developer | Implementation, coding |
| `/ux-designer` | UX Designer | User experience, wireframes |
| `/qa` | QA Engineer | Test automation, quality assurance |
