# Wave 6 — Synthesis and Adversarial Validation

## Adversarial Check Summary

| finding_id | finding | result | evidence | certainty | challenge_summary |
|---|---|---|---|---|---|
| MC-01 | Parser state parity | Pass (fail claim) | `src/html/html_parser.cpp:238-508`; `clever/src/html/tree_builder.cpp:295-788` | High | No counter-evidence found for tokenization/state-machine omission; code confirms simplified behavior. |
| MC-02 | DOM API breadth | Pass (fail claim) | `include/browser/html/dom.h:11-33`; `clever/src/js/js_dom_bindings.cpp:1001-1381` | High | Minimal DOM surface in from_scratch and partial bindings in clever remain under-par; no hidden full implementation found. |
| MC-04 | WebIDL pipeline | Pass (fail claim) | `clever/src/js/js_dom_bindings.cpp:1-200`; `CLEVER_ENGINE_BLUEPRINT.md:1166-1214` | High | No generated WebIDL binding artifacts were produced in code implementation paths. |
| MC-07 | Task queue semantics | Pass (fail claim) | `include/browser/js/runtime.h:43-78`; `src/js/runtime.cpp:1155-1181`; `clever/src/js/js_timers.cpp:50-125` | Medium | No full microtask/macrotask/RAF scheduling path is evident; manual/embedded flush model remains. |
| MC-09 | CSP/CORS/SOP enforcement | Pass (fail claim) | `include/browser/net/http_client.h:31-125`; `src/net/http_client.cpp:1146-1196`; `clever/src/js/js_fetch_bindings.cpp:135-205` | High | No full preflight/header eval/enforcement loop is implemented at runtime; policy model remains shallow. |
| FJNS-06 | TLS + fetch hardening | Pass (high confidence) | `src/net/http_client.cpp:285-317`; `audit/wave1/wave1a-from_scratch-architecture.md:1` | High | TLS chain verification path is not materially evidenced; mitigation identified is consistent with observed fetch flow. |
| FJNS-11 | CORS bypass risk in clever fetch | Pass (high confidence) | `clever/src/js/js_fetch_bindings.cpp:135-205` | High | Evidence supports missing CORS enforcement in XHR path. |

## Consolidated Recommendation

- All High-impact claims from Waves 2–4 were challenged and retained as `Partial`/`Missing` where evidence is insufficient.
- No high-impact claim had contradictory evidence during Wave 6 review.
- Proceed to Gate computation using the adversarial-confirmed findings in `audit/viability-verdict.md` and the updated priority map.
