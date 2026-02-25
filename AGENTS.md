
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
