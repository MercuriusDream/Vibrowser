# Audit Reconciliation Matrix

Generated from `AUDIT_FULL.md` with code-evidence probes.

## Summary

- Total checklist items: 631
- Status counts: implemented=311, partial=128, missing=69, unspecified=123
- Items flagged with stale evidence: 4

## Probe Results

- `events-keyboard-dispatch`: claims=1, evidence_found=true
- `events-dblclick-dispatch`: claims=1, evidence_found=true
- `events-contextmenu-dispatch`: claims=1, evidence_found=true
- `events-form-dispatch`: claims=1, evidence_found=true

## Flagged Claims

- Line 712: `Keyboard events (keydown, keyup, keypress)` (JS Events API / CRITICAL: Events Never Dispatched) -> evidence: events-keyboard-dispatch
- Line 713: `Form events (submit, reset)` (JS Events API / CRITICAL: Events Never Dispatched) -> evidence: events-form-dispatch
- Line 717: `dblclick` (JS Events API / CRITICAL: Events Never Dispatched) -> evidence: events-dblclick-dispatch
- Line 718: `contextmenu` (JS Events API / CRITICAL: Events Never Dispatched) -> evidence: events-contextmenu-dispatch
