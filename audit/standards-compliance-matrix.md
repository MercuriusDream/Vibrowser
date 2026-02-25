# Standards Compliance Matrix

This matrix maps current engine implementation against requested standard surfaces for both subsystems (`from_scratch` + `clever` integration points).  
Status semantics: `Implemented|Partial|Missing|Incorrect|Undefined`.

| Standard + section | Feature group | Status | Evidence | Deviation | Certainty |
|---|---|---|---|---|---|
| WHATWG HTML — tokenizer/tree construction | Parsing and tree builder (`from_scratch`, `clever`) | Partial | `src/html/html_parser.cpp:238-508`; `clever/src/html/tree_builder.cpp:295-788`; `clever/src/html/tokenizer.cpp:1-120`; `audit/wave1/wave1a-from-scratch-architecture.md:1`; `audit/wave1/wave1b-clever-architecture.md:1` | from_scratch uses a single-pass tag/text parser with no state-machine tokenizer modes for raw/text/script contexts; clever parser is simplified (`adoption agency` and insertion-mode handling is partial). | High |
| WHATWG DOM API + event model | DOM construction, query, dispatch (`from_scratch`, `clever`) | Partial | `include/browser/html/dom.h:11-33`; `src/js/runtime.cpp:520-757`; `clever/src/js/js_dom_bindings.cpp:211-240`; `clever/include/clever/dom/event.h:1-120` | from_scratch exposes only a lightweight node shape and limited DOM helpers; clever binds common getters/listeners but omits many DOM interfaces and event semantics required for full API parity (e.g., `Document`, `Window`, richer event options/objects). | High |
| CSSOM — selector/cascade/inheritance | CSS parsing and computed style (`from_scratch`, `clever`) | Partial | `src/css/css_parser.cpp:1-360`; `include/browser/css/css_parser.h:1-80`; `clever/src/css/style/style_resolver.cpp:5773-5957`; `clever/src/css/parser.cpp` | From_scratch supports only limited selector/declaration coverage; clever resolves computed style but lacks broad selector grammar and media/effect coverage required by CSSOM/CSS2.1+ semantics. | High |
| ECMAScript host bindings | JS runtime bridge (`from_scratch`, `clever`) | Partial | `include/browser/js/runtime.h:20-78`; `src/js/runtime.cpp:1-220`; `clever/src/js/js_engine.h:1-220`; `clever/src/js/js_engine.cpp:1-240` | Both engines implement custom host glue, but they do not demonstrate complete ECMAScript host API coverage and environment object contract required by WebIDL-backed bindings. | High |
| Fetch Standard | Network fetch lifecycle | Partial | `include/browser/net/http_client.h:11-124`; `src/net/http_client.cpp:887-1013`; `src/net/http_client.cpp:1023-1196`; `clever/src/net/request.cpp:37-94`; `clever/src/net/http_client.cpp:531-681` | Request/response abstractions and redirect/cache scaffolding exist, but fetch algorithms are not fully implemented (missing request body/body stream, robust body consumption semantics, preflight handling, and deterministic abort/backpressure semantics). | High |
| WHATWG URL | URL parsing, resolution, and serialization (`from_scratch`, `clever`) | Partial | `include/browser/net/url.h:1-22`; `src/net/url.cpp:268-606`; `clever/include/clever/url/url.h:1-40`; `clever/src/url/url_parser.cpp:321-589` | URL handling is feature-reduced in from_scratch; clever has stronger parsing but still diverges from full URL Standard scope (limited non-HTTP edge cases, IDNA complexity, and search-params normalization breadth). | High |
| Web IDL | Binding generation and runtime reflection | Missing | `clever/include/clever/js/js_dom_bindings.h:1-80`; `clever/src/js/js_dom_bindings.cpp:1-200`; `include/browser/js/runtime.h:1`; `CLEVER_ENGINE_BLUEPRINT.md:1166-1214` | No generated `.webidl` pipeline is present in runtime implementation; bindings are manually authored rather than WebIDL-generated reflection contracts. | High |
| Encoding Standard | Charset decoding and encoding edge semantics (`from_scratch`, `clever`) | Missing | `src/net/url.cpp:268-318`; `clever/src/js/js_dom_bindings.cpp:9216-9222` | Path decoding and hard-coded charset reporting exist without full Encoding Standard behaviors (`TextDecoder/TextEncoder`, BOM/header meta negotiation, and legacy fallback rules). | Medium |
| HTTP/1.1 | Request/response framing and semantics | Partial | `src/net/http_client.cpp:515-560`; `src/net/http_client.cpp:887-1013`; `clever/src/net/response.cpp:148-260`; `clever/src/net/http_client.cpp:531-681` | Both codepaths perform HTTP/1.1 style request/response handling; response parsing/transfer support exists, but spec-level invariants (header normalization, strict connection semantics, and error-correct recovery) are incomplete. | High |
| HTTP/2 / HTTP/3 | Transport protocol coverage | Missing | `audit/wave0/third-party-linkage.md:29-44`; `audit/wave0/third-party-linkage.md:51-78`; `audit/wave0/build-graph.md:1` | No evidence of `h2/h3` transport clients, ALPN-aware protocol negotiation, or push/stream concurrency semantics; current stack is HTTP/1.1-centred. | Medium |
| CSP / SOP / CORS | Policy enforcement and cross-origin fetch restrictions | Partial | `include/browser/net/http_client.h:31-125`; `src/net/http_client.cpp:1146-1196`; `clever/src/js/js_fetch_bindings.cpp:135-205`; `clever/src/net/cookie_jar.cpp:31-139` | Policy structs/checks exist, but full CSP header evaluation, CORS preflight/forbidden response-header checks, and site/scheme-validated request rejection are not fully enforced. | High |
| Cookies / storage session policy | Cookie persistence and context partitioning (`from_scratch`, `clever`) | Partial | `include/browser/net/http_client.h:1-25`; `src/net/http_client.cpp:1-120`; `clever/src/net/cookie_jar.cpp:1-190`; `clever/src/net/request.cpp:37-120` | Cookie APIs are present and parsed, but session partitioning/persistence, private-cache partitioning, and SameSite-driven request-context enforcement are incomplete. | High |
| Browser-level task event loop (ECMAScript jobs) | Microtask/macrotask/RAF/timer ordering (`from_scratch`, `clever`) | Partial | `include/browser/js/runtime.h:43-78`; `src/js/runtime.cpp:1155-1181`; `clever/src/js/js_timers.cpp:50-125`; `clever/src/js/js_window.cpp:228-301` | Neither path demonstrates a full host task queue model with ECMAScript-compliant microtask checkpoints and deterministic timer/RAF ordering relative to rendering. | High |

## ComplianceRecord format (observed)

- `standard_section`: as above.
- `feature_group`: column 2.
- `status`: values in matrix.
- `evidence`: file path + line markers.
- `deviation_notes`: provided in row.
- `certainty`: High/Medium/Low.
- `source_wave`: optional annotation (`Wave2` required for new rows).

## Wave 2 summary

- Completed with adjudicated entries for all rows that were previously `Partial`/`Missing`.
- All rows above include direct file-line evidence from `src`, `include`, and `clever/src`.
- High-severity gaps were sent to Wave 6 for adversarial verification and retained in `audit/wave6/final/adversarial-verification.md`.

## Wave 2 gap note

No fully standards-conformant behavior is claimed in this matrix; all `Partial`/`Missing` entries are deliberate and include `status` + `deviation`.
