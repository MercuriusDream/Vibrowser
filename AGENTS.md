
## BMAD Commands

### Command: estate-browser

Perpetual autonomous agent for building a fully-working and fully-modern web browser. Read `.agents/estate-browser/estate-browser.agent.yaml` for full protocol.

---

### Command: advanced-elicitation

Read and execute the workflow/task at `_bmad/core/workflows/advanced-elicitation/workflow.xml`.

---

### Command: adversarial-review

Read and execute the workflow/task at `_bmad/core/tasks/review-adversarial-general.xml`.

---

### Command: analyst

Read and follow the agent defined in `_bmad/bmm/agents/analyst.agent.yaml`.

---

### Command: architect

Read and follow the agent defined in `_bmad/bmm/agents/architect.agent.yaml`.

---

### Command: bmad-help

Read and execute the workflow/task at `_bmad/core/tasks/bmad-help.md`.

---

### Command: bmalph-implement

# Start Implementation

Transition from BMAD planning (Phase 3) to Ralph implementation (Phase 4).

## Prerequisites

- bmalph must be initialized (`bmalph/config.json` must exist)
- Stories must exist in `_bmad-output/planning-artifacts/`

## Steps

### 1. Validate Prerequisites

Check that:
- `bmalph/config.json` exists
- `.ralph/ralph_loop.sh` exists
- `_bmad-output/planning-artifacts/` exists with story files

If any are missing, report the error and suggest running the required commands.

### 2. Parse Stories

Read `_bmad-output/planning-artifacts/stories.md` (or any file matching `*stor*.md` or `*epic*.md`):
- Extract epics: `## Epic N: Title`
- Extract stories: `### Story N.M: Title`
- Parse acceptance criteria (Given/When/Then blocks)

### 3. Generate `.ralph/@fix_plan.md`

Create an ordered list of stories as checkboxes:

```markdown
# Ralph Fix Plan

## Stories to Implement

### [Epic Title]
> Goal: [Epic description]

- [ ] Story 1.1: [Title]
  > [Description line 1]
  > AC: Given..., When..., Then...

## Completed

## Notes
- Follow TDD methodology (red-green-refactor)
- One story per Ralph loop iteration
- Update this file after completing each story
```

**Important:** If existing `@fix_plan.md` has completed items `[x]`, preserve their completion status in the new file.

### 4. Generate `.ralph/PROJECT_CONTEXT.md`

Extract from planning artifacts:
- Project goals from PRD (Executive Summary, Vision, Goals sections)
- Success metrics from PRD
- Architecture constraints from architecture document
- Technical risks from architecture document
- Scope boundaries from PRD
- Target users from PRD
- Non-functional requirements from PRD

Format as:

```markdown
# [Project Name] — Project Context

## Project Goals
[extracted content]

## Success Metrics
[extracted content]

## Architecture Constraints
[extracted content]

## Technical Risks
[extracted content]

## Scope Boundaries
[extracted content]

## Target Users
[extracted content]

## Non-Functional Requirements
[extracted content]
```

### 5. Copy Specs to `.ralph/specs/`

Copy the entire `_bmad-output/` tree to `.ralph/specs/`:
- This includes `planning-artifacts/`, `implementation-artifacts/`, etc.
- Preserve directory structure

If specs existed before, generate `SPECS_CHANGELOG.md`:
```markdown
# Specs Changelog

Last updated: [timestamp]

## Added
- [new files]

## Modified
- [changed files]

## Removed
- [deleted files]
```

### 6. Update State

Update `bmalph/state/current-phase.json`:
```json
{
  "currentPhase": 4,
  "status": "implementing",
  "startedAt": "[original or now]",
  "lastUpdated": "[now]"
}
```

### 7. Report Results

Output:
- Number of stories found
- Any warnings (missing PRD, architecture, NO-GO status in readiness report)
- Whether fix_plan progress was preserved

### 8. Instruct User

Display:

```
Transition complete. To start the Ralph autonomous loop:

    bash .ralph/ralph_loop.sh

Or in a separate terminal for background execution:

    nohup bash .ralph/ralph_loop.sh > .ralph/logs/ralph.log 2>&1 &
```

## Warnings to Check

- No PRD document found
- No architecture document found
- Readiness report indicates NO-GO status
- No stories parsed from the epics file

---

### Command: bmalph

Read and follow the agent defined in `_bmad/core/agents/bmad-master.agent.yaml`.

---

### Command: brainstorm-project

Adopt the role of the agent defined in `_bmad/bmm/agents/analyst.agent.yaml`, then read and execute the workflow at `_bmad/core/workflows/brainstorming/workflow.md` using `_bmad/bmm/data/project-context-template.md` as context data.

---

### Command: brainstorming

Read and execute the workflow/task at `_bmad/core/workflows/brainstorming/workflow.md`.

---

### Command: correct-course

Adopt the role of the agent defined in `_bmad/bmm/agents/sm.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/4-implementation/correct-course/workflow.yaml` in Create mode.

---

### Command: create-architecture

Adopt the role of the agent defined in `_bmad/bmm/agents/architect.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/3-solutioning/create-architecture/workflow.md` in Create mode.

---

### Command: create-brief

Adopt the role of the agent defined in `_bmad/bmm/agents/analyst.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/1-analysis/create-product-brief/workflow.md` in Create mode.

---

### Command: create-epics-stories

Adopt the role of the agent defined in `_bmad/bmm/agents/pm.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/3-solutioning/create-epics-and-stories/workflow.md` in Create mode.

---

### Command: create-prd

Adopt the role of the agent defined in `_bmad/bmm/agents/pm.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/2-plan-workflows/create-prd/workflow-create-prd.md`.

---

### Command: create-story

Adopt the role of the agent defined in `_bmad/bmm/agents/sm.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/4-implementation/create-story/workflow.yaml` in Create mode.

---

### Command: create-ux

Adopt the role of the agent defined in `_bmad/bmm/agents/ux-designer.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/2-plan-workflows/create-ux-design/workflow.md` in Create mode.

---

### Command: dev

Read and follow the agent defined in `_bmad/bmm/agents/dev.agent.yaml`.

---

### Command: document-project

Adopt the role of the agent defined in `_bmad/bmm/agents/analyst.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/document-project/workflow.yaml` in Create mode.

---

### Command: domain-research

Adopt the role of the agent defined in `_bmad/bmm/agents/analyst.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/1-analysis/research/workflow-domain-research.md`.

---

### Command: editorial-prose

Read and execute the workflow/task at `_bmad/core/tasks/editorial-review-prose.xml`.

---

### Command: editorial-structure

Read and execute the workflow/task at `_bmad/core/tasks/editorial-review-structure.xml`.

---

### Command: execute-workflow

Read and execute the workflow/task at `_bmad/core/tasks/workflow.xml`.

---

### Command: generate-project-context

Adopt the role of the agent defined in `_bmad/bmm/agents/analyst.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/generate-project-context/workflow.md`.

---

### Command: implementation-readiness

Adopt the role of the agent defined in `_bmad/bmm/agents/architect.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/3-solutioning/check-implementation-readiness/workflow.md` in Validate mode.

---

### Command: index-docs

Read and execute the workflow/task at `_bmad/core/tasks/index-docs.xml`.

---

### Command: market-research

Adopt the role of the agent defined in `_bmad/bmm/agents/analyst.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/1-analysis/research/workflow-market-research.md`.

---

### Command: party-mode

Read and execute the workflow/task at `_bmad/core/workflows/party-mode/workflow.md`.

---

### Command: pm

Read and follow the agent defined in `_bmad/bmm/agents/pm.agent.yaml`.

---

### Command: qa-automate

Adopt the role of the agent defined in `_bmad/bmm/agents/qa.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/qa/automate/workflow.yaml`.

---

### Command: qa

Read and follow the agent defined in `_bmad/bmm/agents/qa.agent.yaml`.

---

### Command: quick-dev

Adopt the role of the agent defined in `_bmad/bmm/agents/quick-flow-solo-dev.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/bmad-quick-flow/quick-dev/workflow.md` in Create mode.

---

### Command: quick-flow-solo-dev

Read and follow the agent defined in `_bmad/bmm/agents/quick-flow-solo-dev.agent.yaml`.

---

### Command: retrospective

Adopt the role of the agent defined in `_bmad/bmm/agents/sm.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/4-implementation/retrospective/workflow.yaml`.

---

### Command: shard-doc

Read and execute the workflow/task at `_bmad/core/tasks/shard-doc.xml`.

---

### Command: sm

Read and follow the agent defined in `_bmad/bmm/agents/sm.agent.yaml`.

---

### Command: sprint-planning

Adopt the role of the agent defined in `_bmad/bmm/agents/sm.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/4-implementation/sprint-planning/workflow.yaml` in Create mode.

---

### Command: sprint-status

Adopt the role of the agent defined in `_bmad/bmm/agents/sm.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/4-implementation/sprint-status/workflow.yaml`.

---

### Command: tech-spec

Adopt the role of the agent defined in `_bmad/bmm/agents/quick-flow-solo-dev.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/bmad-quick-flow/quick-spec/workflow.md` in Create mode.

---

### Command: tech-writer

Read and follow the agent defined in `_bmad/bmm/agents/tech-writer/tech-writer.agent.yaml`.

---

### Command: technical-research

Adopt the role of the agent defined in `_bmad/bmm/agents/analyst.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/1-analysis/research/workflow-technical-research.md`.

---

### Command: ux-designer

Read and follow the agent defined in `_bmad/bmm/agents/ux-designer.agent.yaml`.

---

### Command: validate-architecture

Adopt the role of the agent defined in `_bmad/bmm/agents/architect.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/3-solutioning/create-architecture/workflow.md` in Validate mode.

---

### Command: validate-brief

Adopt the role of the agent defined in `_bmad/bmm/agents/analyst.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/1-analysis/create-product-brief/workflow.md` in Validate mode.

---

### Command: validate-epics-stories

Adopt the role of the agent defined in `_bmad/bmm/agents/pm.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/3-solutioning/create-epics-and-stories/workflow.md` in Validate mode.

---

### Command: validate-prd

Adopt the role of the agent defined in `_bmad/bmm/agents/pm.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/2-plan-workflows/create-prd/workflow-validate-prd.md`.

---

### Command: validate-story

Adopt the role of the agent defined in `_bmad/bmm/agents/sm.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/4-implementation/create-story/workflow.yaml` in Validate mode.

---

### Command: validate-ux

Adopt the role of the agent defined in `_bmad/bmm/agents/ux-designer.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/2-plan-workflows/create-ux-design/workflow.md` in Validate mode.

## Prerequisites

- bmalph must be initialized (`bmalph/config.json` must exist)
- Stories must exist in `_bmad-output/planning-artifacts/`

## Steps

### 1. Validate Prerequisites

Check that:
- `bmalph/config.json` exists
- `.ralph/ralph_loop.sh` exists
- `_bmad-output/planning-artifacts/` exists with story files

If any are missing, report the error and suggest running the required commands.

### 2. Parse Stories

Read `_bmad-output/planning-artifacts/stories.md` (or any file matching `*stor*.md` or `*epic*.md`):
- Extract epics: `## Epic N: Title`
- Extract stories: `### Story N.M: Title`
- Parse acceptance criteria (Given/When/Then blocks)

### 3. Generate `.ralph/@fix_plan.md`

Create an ordered list of stories as checkboxes:

```markdown
# Ralph Fix Plan

## Stories to Implement

### [Epic Title]
> Goal: [Epic description]

- [ ] Story 1.1: [Title]
  > [Description line 1]
  > AC: Given..., When..., Then...

## Completed

## Notes
- Follow TDD methodology (red-green-refactor)
- One story per Ralph loop iteration
- Update this file after completing each story
```

**Important:** If existing `@fix_plan.md` has completed items `[x]`, preserve their completion status in the new file.

### 4. Generate `.ralph/PROJECT_CONTEXT.md`

Extract from planning artifacts:
- Project goals from PRD (Executive Summary, Vision, Goals sections)
- Success metrics from PRD
- Architecture constraints from architecture document
- Technical risks from architecture document
- Scope boundaries from PRD
- Target users from PRD
- Non-functional requirements from PRD

Format as:

```markdown
# [Project Name] — Project Context

## Project Goals
[extracted content]

## Success Metrics
[extracted content]

## Architecture Constraints
[extracted content]

## Technical Risks
[extracted content]

## Scope Boundaries
[extracted content]

## Target Users
[extracted content]

## Non-Functional Requirements
[extracted content]
```

### 5. Copy Specs to `.ralph/specs/`

Copy the entire `_bmad-output/` tree to `.ralph/specs/`:
- This includes `planning-artifacts/`, `implementation-artifacts/`, etc.
- Preserve directory structure

If specs existed before, generate `SPECS_CHANGELOG.md`:
```markdown
# Specs Changelog

Last updated: [timestamp]

## Added
- [new files]

## Modified
- [changed files]

## Removed
- [deleted files]
```

### 6. Update State

Update `bmalph/state/current-phase.json`:
```json
{
  "currentPhase": 4,
  "status": "implementing",
  "startedAt": "[original or now]",
  "lastUpdated": "[now]"
}
```

### 7. Report Results

Output:
- Number of stories found
- Any warnings (missing PRD, architecture, NO-GO status in readiness report)
- Whether fix_plan progress was preserved

### 8. Instruct User

Display:

```
Transition complete. To start the Ralph autonomous loop:

    bash .ralph/ralph_loop.sh

Or in a separate terminal for background execution:

    nohup bash .ralph/ralph_loop.sh > .ralph/logs/ralph.log 2>&1 &
```

## Warnings to Check

- No PRD document found
- No architecture document found
- Readiness report indicates NO-GO status
- No stories parsed from the epics file

---

### Command: bmalph

Read and follow the agent defined in `_bmad/core/agents/bmad-master.agent.yaml`.

---

### Command: brainstorm-project

Adopt the role of the agent defined in `_bmad/bmm/agents/analyst.agent.yaml`, then read and execute the workflow at `_bmad/core/workflows/brainstorming/workflow.md` using `_bmad/bmm/data/project-context-template.md` as context data.

---

### Command: brainstorming

Read and execute the workflow/task at `_bmad/core/workflows/brainstorming/workflow.md`.

---

### Command: correct-course

Adopt the role of the agent defined in `_bmad/bmm/agents/sm.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/4-implementation/correct-course/workflow.yaml` in Create mode.

---

### Command: create-architecture

Adopt the role of the agent defined in `_bmad/bmm/agents/architect.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/3-solutioning/create-architecture/workflow.md` in Create mode.

---

### Command: create-brief

Adopt the role of the agent defined in `_bmad/bmm/agents/analyst.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/1-analysis/create-product-brief/workflow.md` in Create mode.

---

### Command: create-epics-stories

Adopt the role of the agent defined in `_bmad/bmm/agents/pm.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/3-solutioning/create-epics-and-stories/workflow.md` in Create mode.

---

### Command: create-prd

Adopt the role of the agent defined in `_bmad/bmm/agents/pm.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/2-plan-workflows/create-prd/workflow-create-prd.md`.

---

### Command: create-story

Adopt the role of the agent defined in `_bmad/bmm/agents/sm.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/4-implementation/create-story/workflow.yaml` in Create mode.

---

### Command: create-ux

Adopt the role of the agent defined in `_bmad/bmm/agents/ux-designer.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/2-plan-workflows/create-ux-design/workflow.md` in Create mode.

---

### Command: dev

Read and follow the agent defined in `_bmad/bmm/agents/dev.agent.yaml`.

---

### Command: document-project

Adopt the role of the agent defined in `_bmad/bmm/agents/analyst.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/document-project/workflow.yaml` in Create mode.

---

### Command: domain-research

Adopt the role of the agent defined in `_bmad/bmm/agents/analyst.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/1-analysis/research/workflow-domain-research.md`.

---

### Command: editorial-prose

Read and execute the workflow/task at `_bmad/core/tasks/editorial-review-prose.xml`.

---

### Command: editorial-structure

Read and execute the workflow/task at `_bmad/core/tasks/editorial-review-structure.xml`.

---

### Command: execute-workflow

Read and execute the workflow/task at `_bmad/core/tasks/workflow.xml`.

---

### Command: generate-project-context

Adopt the role of the agent defined in `_bmad/bmm/agents/analyst.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/generate-project-context/workflow.md`.

---

### Command: implementation-readiness

Adopt the role of the agent defined in `_bmad/bmm/agents/architect.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/3-solutioning/check-implementation-readiness/workflow.md` in Validate mode.

---

### Command: index-docs

Read and execute the workflow/task at `_bmad/core/tasks/index-docs.xml`.

---

### Command: market-research

Adopt the role of the agent defined in `_bmad/bmm/agents/analyst.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/1-analysis/research/workflow-market-research.md`.

---

### Command: party-mode

Read and execute the workflow/task at `_bmad/core/workflows/party-mode/workflow.md`.

---

### Command: pm

Read and follow the agent defined in `_bmad/bmm/agents/pm.agent.yaml`.

---

### Command: qa-automate

Adopt the role of the agent defined in `_bmad/bmm/agents/qa.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/qa/automate/workflow.yaml`.

---

### Command: qa

Read and follow the agent defined in `_bmad/bmm/agents/qa.agent.yaml`.

---

### Command: quick-dev

Adopt the role of the agent defined in `_bmad/bmm/agents/quick-flow-solo-dev.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/bmad-quick-flow/quick-dev/workflow.md` in Create mode.

---

### Command: quick-flow-solo-dev

Read and follow the agent defined in `_bmad/bmm/agents/quick-flow-solo-dev.agent.yaml`.

---

### Command: retrospective

Adopt the role of the agent defined in `_bmad/bmm/agents/sm.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/4-implementation/retrospective/workflow.yaml`.

---

### Command: shard-doc

Read and execute the workflow/task at `_bmad/core/tasks/shard-doc.xml`.

---

### Command: sm

Read and follow the agent defined in `_bmad/bmm/agents/sm.agent.yaml`.

---

### Command: sprint-planning

Adopt the role of the agent defined in `_bmad/bmm/agents/sm.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/4-implementation/sprint-planning/workflow.yaml` in Create mode.

---

### Command: sprint-status

Adopt the role of the agent defined in `_bmad/bmm/agents/sm.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/4-implementation/sprint-status/workflow.yaml`.

---

### Command: tech-spec

Adopt the role of the agent defined in `_bmad/bmm/agents/quick-flow-solo-dev.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/bmad-quick-flow/quick-spec/workflow.md` in Create mode.

---

### Command: tech-writer

Read and follow the agent defined in `_bmad/bmm/agents/tech-writer/tech-writer.agent.yaml`.

---

### Command: technical-research

Adopt the role of the agent defined in `_bmad/bmm/agents/analyst.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/1-analysis/research/workflow-technical-research.md`.

---

### Command: ux-designer

Read and follow the agent defined in `_bmad/bmm/agents/ux-designer.agent.yaml`.

---

### Command: validate-architecture

Adopt the role of the agent defined in `_bmad/bmm/agents/architect.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/3-solutioning/create-architecture/workflow.md` in Validate mode.

---

### Command: validate-brief

Adopt the role of the agent defined in `_bmad/bmm/agents/analyst.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/1-analysis/create-product-brief/workflow.md` in Validate mode.

---

### Command: validate-epics-stories

Adopt the role of the agent defined in `_bmad/bmm/agents/pm.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/3-solutioning/create-epics-and-stories/workflow.md` in Validate mode.

---

### Command: validate-prd

Adopt the role of the agent defined in `_bmad/bmm/agents/pm.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/2-plan-workflows/create-prd/workflow-validate-prd.md`.

---

### Command: validate-story

Adopt the role of the agent defined in `_bmad/bmm/agents/sm.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/4-implementation/create-story/workflow.yaml` in Validate mode.

---

### Command: validate-ux

Adopt the role of the agent defined in `_bmad/bmm/agents/ux-designer.agent.yaml`, then read and execute the workflow at `_bmad/bmm/workflows/2-plan-workflows/create-ux-design/workflow.md` in Validate mode.

## BMAD-METHOD Integration

Run the BMAD master agent to navigate phases. Ask for help to discover all available agents and workflows.

### Phases

| Phase | Focus | Key Agents |
|-------|-------|-----------|
| 1. Analysis | Understand the problem | Analyst agent |
| 2. Planning | Define the solution | Product Manager agent |
| 3. Solutioning | Design the architecture | Architect agent |
| 4. Implementation | Build it | Developer agent, then Ralph autonomous loop |

### Workflow

1. Work through Phases 1-3 using BMAD agents and workflows
2. Use the bmalph-implement transition to prepare Ralph format, then start Ralph

### Available Agents

| Agent | Role |
|-------|------|
| Analyst | Research, briefs, discovery |
| Architect | Technical design, architecture |
| Product Manager | PRDs, epics, stories |
| Scrum Master | Sprint planning, status, coordination |
| Developer | Implementation, coding |
| UX Designer | User experience, wireframes |
| QA Engineer | Test automation, quality assurance |

## Coding Rules (Persistent)

- **No single-use extraction:** Do not split logic into a separate function unless that logic is shared and called by at least **two different function definitions**. If logic is used in only one function, keep it inline in that function. This only applies to **meaningless** helpers.
  - For example, in C/C++, a helper declared as `static`/`static inline` is **meaningless** only if **all** conditions hold:
    1. It is a trivial pass-through (e.g., single-call wrapper, trivial cast/return glue).
    2. It is effectively used by only one concrete call site.
    3. It adds no **objectively meaningful logic**. Treat this as true only when all are true:
      - body has at most **3 executable statements**,
      - no control-flow (`if/for/while/switch/try/catch`),
      - at most **1 call expression** (the forwarded callee),
      - no ownership/lifetime handling, locking/atomic synchronization, validation/contract checks, error-code/`errno` translation, cleanup, retry/backoff, boundary/clamping logic.
    4. It provides no **objective readability benefit**. Treat this as true only when all are true:
      - inlining the helper into its caller would add at most **3 executable statements**,
      - caller nesting depth would not increase,
      - inlining would not duplicate the same code block in **2+** places.
  - Only this class should be force-inlined/removed.
  - ***Explicit exclusions (do NOT treat as meaningless by default):***
    1. For example, in C/C++, callback/entrypoint functions invoked indirectly (function pointers, `std::thread`, signal handlers, hook registries, `atexit`, etc.).
    2. Template/generic helpers whose reuse appears through instantiations/types rather than obvious direct call counts.
    3. ABI/API boundary helpers where shape/signature stability matters.
    4. Non-trivial single-caller helpers with objective complexity (e.g., control-flow present, or >=4 executable statements, or ownership/locking/validation/error-translation/cleanup/retry/boundary handling).
  - ***Audit policy:***
    1. `caller_count < 2` is a **candidate signal**, not an automatic violation.
    2. Require manual review before refactoring/removal.

- **No Over-Engineering:**
  - Do not over-engineer solutions. Always implement the simplest approach that correctly satisfies current requirements, constraints, and expected scale.
  - Do not introduce extra abstraction layers, speculative extension points, generic frameworks, or premature optimizations unless there is a concrete and present need.
  - Prefer straightforward, readable, and maintainable code over clever designs that add unnecessary complexity.
  - If additional complexity is required, document why it is necessary and what practical benefit it provides.

- **Clear, Consistent Naming and Function Signatures:**
  - Function and variable names must be easy to understand at a glance and consistent with existing codebase style (casing, terminology, prefixes/suffixes, and domain vocabulary).
  - Avoid ambiguous or cryptic names and unnecessary abbreviations unless they are widely established in this project.
  - Function signatures must also be consistent:
    - Return types should reflect the function’s primary contract.
    - Parameter types should match domain meaning and existing conventions.
    - Parameter order should follow a predictable pattern across related APIs (e.g., primary context/object first, required inputs next, optional flags/settings after that, and output pointers/buffers in conventionally expected positions).
  - Avoid arbitrary signature variations that increase cognitive load for readers and callers.

- **Tuning Heuristic Constants Placement:**
  - This rule applies only to hardcoded constants that are intended as tuning knobs (e.g., timeouts, limits, thresholds, intervals, polling/sleep values, queue sizes, batch/window/chunk sizes).
  - For example, in C/C++, place tuning constants in a clearly visible constants area near the top of the file, **after the full include declaration section is complete**.
  - For example, in C/C++, the include declaration section includes conditional include groups (`#if/#ifdef/#elif ... #include ... #endif`) for platform/special-case headers.
  - Do **not** insert tuning constants in the middle of conditional include groups; keep include structure intact and place constants only after those include groups finish.
  - Do not scatter tuning constants inside function bodies or unrelated sections.
  - Do not treat semantic/protocol constants as tuning constants (e.g., state markers, ABI/API contract values, enum-like fixed values, syscall/errno mappings, IDs, or spec-mandated bit masks).
  - If classification is unclear, treat the constant as non-tuning by default and avoid forcing relocation solely for this rule.

## Web Interface Guidelines

Interfaces succeed because of hundreds of choices. This is a living, non-exhaustive list of those decisions. Most guidelines are framework-agnostic, some specific to React/Next.js. Feedback is welcome.

### Interactions

- Keyboard works everywhere. All flows are keyboard-operable and follow the WAI-ARIA Authoring Patterns.
- Clear focus. Every focusable element shows a visible focus ring. Prefer `:focus-visible` over `:focus` to avoid distracting pointer users. Set `:focus-within` for grouped controls.
- Manage focus. Use focus traps, move and return focus according to the WAI-ARIA Patterns.
- Match visual and hit targets. Exception: if the visual target is < 24px, expand its hit target to >= 24px. On mobile, the minimum size is 44px.
- Mobile input size. `<input>` font size is >= 16px on mobile to prevent iOS Safari auto-zoom/pan on focus. Or set `<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1" />`.
- Respect zoom. Never disable browser zoom.
- Hydration-safe inputs. Inputs must not lose focus or value after hydration.
- Don't block paste. Never disable paste in `<input>` or `<textarea>`.
- Loading buttons. Show a loading indicator and keep the original label.
- Minimum loading-state duration. If you show a spinner/skeleton, add a short show-delay (~150-300 ms) and a minimum visible time (~300-500 ms) to avoid flicker on fast responses. The `<Suspense>` component in React does this automatically.
- URL as state. Persist state in the URL so share, refresh, Back/Forward navigation work e.g., nuqs.
- Optimistic updates. Update the UI immediately when success is likely; reconcile on server response. On failure, show an error and roll back or provide Undo.
- Ellipsis for further input and loading states. Menu options that open a follow-up e.g., `Rename...` and loading/processing states e.g., `Loading...`, `Saving...`, `Generating...` end with an ellipsis.
- Confirm destructive actions. Require confirmation or provide Undo with a safe window.
- Prevent double-tap zoom on controls. Set `touch-action: manipulation`.
- Tap highlight follows design. Set `webkit-tap-highlight-color`.
- Design forgiving interactions. Controls minimize finickiness with generous hit targets, clear affordances, and predictable interactions, e.g., prediction cones.
- Tooltip timing. Delay the first tooltip in a group; subsequent peers have no delay.
- Overscroll behavior. Set `overscroll-behavior: contain` intentionally e.g., in modals/drawers.
- Scroll positions persist. Back/Forward restores prior scroll.
- Autofocus for speed. On desktop screens with a single primary input, autofocus. Rarely autofocus on mobile because the keyboard opening can cause layout shift.
- No dead zones. If part of a control looks interactive, it should be interactive. Don't leave users guessing where to interact.
- Deep-link everything. Filters, tabs, pagination, expanded panels, anytime `useState` is used.
- Clean drag interactions. Disable text selection and apply `inert` (which prevents interaction) while an element is dragged so selection/hover don't occur simultaneously.
- Links are links. Use `<a>` or `<Link>` for navigation so standard browser behaviors work (Cmd/Ctrl+Click, middle-click, right-click to open in a new tab). Never substitute with `<button>` or `<div>` for navigational links.
- Announce async updates. Use polite `aria-live` for toasts and inline validation.
- Locale-aware keyboard shortcuts. Internationalize keyboard shortcuts for non-QWERTY layouts. Show platform-specific symbols.

### Animations

- Honor `prefers-reduced-motion`. Provide a reduced-motion variant.
- Implementation preference. Prefer CSS, avoid main-thread JS-driven animations when possible.
- Preference: CSS > Web Animations API > JavaScript libraries e.g., motion.
- Compositor-friendly. Prioritize GPU-accelerated properties (`transform`, `opacity`) and avoid properties that trigger reflows/repaints (`width`, `height`, `top`, `left`).
- Necessity check. Only animate when it clarifies cause and effect or when it adds deliberate delight.
- Easing fits the subject. Choose easing based on what changes (size, distance, trigger).
- Interruptible. Animations are cancelable by user input.
- Input-driven. Avoid autoplay; animate in response to actions.
- Correct transform origin. Anchor motion to where it physically starts.
- Never `transition: all`. Explicitly list only the properties you intend to animate (typically `opacity`, `transform`). `all` can unintentionally animate layout-affecting properties causing jank.
- Cross-browser SVG transforms. Apply CSS transforms/animations to `<g>` wrappers and set `transform-box: fill-box; transform-origin: center;`. Safari historically had bugs with `transform-origin` on SVG and grouping avoids origin miscalculation.

### Layout

- Optical alignment. Adjust +/- 1px when perception beats geometry.
- Deliberate alignment. Every element aligns with something intentionally whether to a grid, baseline, edge, or optical center. No accidental positioning.
- Balance contrast in lockups. When text and icons sit side by side, adjust weight, size, spacing, or color so they don't clash.
- Responsive coverage. Verify on mobile, laptop, and ultra-wide. For ultra-wide, zoom out to 50% to simulate.
- Respect safe areas. Account for notches and insets with safe-area variables.
- No excessive scrollbars. Only render useful scrollbars; fix overflow issues to prevent unwanted scrollbars. On macOS set "Show scroll bars" to "Always" to test what Windows users would see.
- Let the browser size things. Prefer flex/grid/intrinsic layout over measuring in JS. Avoid layout thrash by letting CSS handle flow, wrapping, and alignment.

### Content

- Inline help first. Prefer inline explanations; use tooltips as a last resort.
- Stable skeletons. Skeletons mirror final content exactly to avoid layout shift.
- Accurate page titles. `<title>` reflects the current context.
- No dead ends. Every screen offers a next step or recovery path.
- All states designed. Empty, sparse, dense, and error states.
- Typographic quotes. Prefer curly quotes over straight quotes where style permits.
- Avoid widows/orphans. Tidy rag and line breaks.
- Tabular numbers for comparisons. Use `font-variant-numeric: tabular-nums` or a monospace like Geist Mono.
- Redundant status cues. Don't rely on color alone; include text labels.
- Icons have labels. Convey the same meaning with text for non-sighted users.
- Don't ship the schema. Visual layouts may omit visible labels, but accessible names/labels still exist for assistive tech.
- Use the ellipsis character where appropriate.
- Anchored headings. Set `scroll-margin-top` for headers when linking to sections.
- Resilient to user-generated content. Layouts handle short, average, and very long content.
- Locale-aware formats. Format dates, times, numbers, delimiters, and currencies for the user's locale.
- Prefer language settings over location. Detect language via `Accept-Language` header and `navigator.languages`. Never rely on IP/GPS for language.
- Accessible content. Set accurate names (`aria-label`), hide decoration (`aria-hidden`) and verify in the accessibility tree.
- Icon-only buttons are named. Provide a descriptive `aria-label`.
- Semantics before ARIA. Prefer native elements (`button`, `a`, `label`, `table`) before `aria-*`.
- Headings and skip link. Hierarchical `<h1-h6>` and a "Skip to content" link.
- Brand resources from the logo. Right-click the nav logo to surface brand assets for quick access.
- Non-breaking spaces for glued terms. Use `&nbsp;` to keep units, shortcuts and names together. Use `&#x2060;` for no space.

### Forms

- Enter submits. When a text input is focused, Enter submits if it is the only control. If there are many controls, apply to the last control.
- Textarea behavior. In `<textarea>`, Cmd/Ctrl+Enter submits; Enter inserts a new line.
- Labels everywhere. Every control has a `<label>` or is associated with a label for assistive tech.
- Label activation. Clicking a `<label>` focuses the associated control.
- Submission rule. Keep submit enabled until submission starts; then disable during the in-flight request, show a spinner, and include an idempotency key.
- Don't block typing. Even if a field only accepts numbers, allow any input and show validation feedback.
- Don't pre-disable submit. Allow submitting incomplete forms to surface validation feedback.
- No dead zones on controls. Checkboxes and radios avoid dead zones; the label and control share a single generous hit target.
- Error placement. Show errors next to their fields; on submit, focus the first error.
- Autocomplete and names. Set `autocomplete` and meaningful `name` values to enable autofill.
- Spellcheck selectively. Disable for emails, codes, usernames, etc.
- Correct types and input modes. Use the right `type` and `inputmode` for better keyboards and validation.
- Placeholders signal emptiness. End with an ellipsis where appropriate.
- Placeholder value. Set placeholder to an example value or pattern.
- Unsaved changes. Warn before navigation when data could be lost.
- Password managers and 2FA. Ensure compatibility and allow pasting one-time codes.
- Don't trigger password managers for non-auth fields. For inputs like search avoid reserved names (e.g., `password`), use `autocomplete="off"` or a specific token like `autocomplete="one-time-code"` for OTP fields.
- Text replacements and expansions. Some input methods add trailing whitespace. The input should trim the value to avoid showing a confusing error message.
- Windows `<select>` background. Explicitly set `background-color` and `color` on native `<select>` to avoid dark-mode contrast bugs on Windows.

### Performance

- Device/browser matrix. Test iOS Low Power Mode and macOS Safari.
- Measure reliably. Disable extensions that add overhead or change runtime behavior.
- Track re-renders. Minimize and make re-renders fast. Use React DevTools or React Scan.
- Throttle when profiling. Test with CPU and network throttling.
- Minimize layout work. Batch reads/writes; avoid unnecessary reflows/repaints.
- Network latency budgets. POST/PATCH/DELETE complete in <500ms.
- Keystroke cost. Prefer uncontrolled inputs; make controlled loops cheap.
- Large lists. Virtualize large lists e.g., virtua or `content-visibility: auto`.
- Preload wisely. Preload only above-the-fold images; lazy-load the rest.
- No image-caused CLS. Set explicit image dimensions and reserve space.
- Preconnect to origins. Use `<link rel="preconnect">` for asset/CDN domains (with `crossorigin` when needed) to reduce DNS/TLS latency.
- Preload fonts. For critical text to avoid flash and layout shift.
- Subset fonts. Ship only the code points/scripts you use via `unicode-range` to shrink size.
- Don't use the main thread for expensive work. Move especially long tasks to Web Workers to avoid blocking interaction with the page.

### Design

- Layered shadows. Mimic ambient and direct light with at least two layers.
- Crisp borders. Combine borders and shadows; semi-transparent borders improve edge clarity.
- Nested radii. Child radius <= parent radius and concentric so curves align.
- Hue consistency. On non-neutral backgrounds, tint borders/shadows/text toward the same hue.
- Accessible charts. Use color-blind-friendly palettes.
- Minimum contrast. Prefer APCA over WCAG 2 for more accurate perceptual contrast.
- Interactions increase contrast. `:hover`, `:active`, `:focus` have more contrast than rest state.
- Browser UI matches your background. Set `<meta name="theme-color" content="#000000">` (or matching value) to align browser theme color with the page background.
- Set the appropriate color-scheme. Style `<html>` with a suitable `color-scheme` so scrollbars and device UI have proper contrast.
- Text anti-aliasing and transforms. Scaling text can change smoothing. Prefer animating a wrapper instead of the text node.
- Avoid gradient banding. Fading content to dark colors using CSS masks can cause banding. Background images can be used instead.
