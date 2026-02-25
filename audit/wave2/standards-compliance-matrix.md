# Wave 2 — Standards Compliance Mapping

| id | standard_section | feature_group | status | evidence | deviation | certainty |
|---|---|---|---|---|---|---|
| W2-C-01 | WHATWG HTML — tokenizer/tree construction | Parser | Partial | `src/html/html_parser.cpp:238-508`, `clever/src/html/tree_builder.cpp:295-788`, `clever/src/html/tokenizer.cpp:1-120` | from_scratch and clever are both simplified; no full tokenizer state coverage. | High |
| W2-D-01 | WHATWG DOM API + event model | DOM/mutation/events | Partial | `include/browser/html/dom.h:11-33`, `src/js/runtime.cpp:520-757`, `clever/src/js/js_dom_bindings.cpp:211-240` | Lightweight DOM in from_scratch; selective event APIs in clever; interface parity is partial. | High |
| W2-CSS-01 | CSS/CSSOM | Parsing + cascade | Partial | `src/css/css_parser.cpp:1-360`, `include/browser/css/css_parser.h:1-80`, `clever/src/css/style/style_resolver.cpp:5773-5957` | Selector grammar and media/effect coverage are not complete. | High |
| W2-JS-01 | ECMAScript runtime host bindings | Runtime APIs | Partial | `include/browser/js/runtime.h:20-78`, `src/js/runtime.cpp:1-220`, `clever/src/js/js_engine.cpp:1-220`, `clever/src/js/js_dom_bindings.cpp:1-200` | Host API set is minimal/custom and lacks complete standardized objects. | Medium |
| W2-F-01 | Fetch | Networking lifecycle | Partial | `include/browser/net/http_client.h:11-124`, `src/net/http_client.cpp:887-1013`, `src/net/http_client.cpp:1146-1196`, `clever/src/net/http_client.cpp:531-681` | No complete fetch body/streaming/abort semantics. | High |
| W2-U-01 | WHATWG URL | URL parser/serializer | Partial | `include/browser/net/url.h:1-22`, `src/net/url.cpp:268-606`, `clever/src/url/url_parser.cpp:321-589` | Both implementations are narrower than full URL Standard scope. | High |
| W2-WIDL-01 | Web IDL | IDL pipeline | Missing | `clever/src/js/js_dom_bindings.cpp:1-200`, `include/browser/js/runtime.h:1`, `CLEVER_ENGINE_BLUEPRINT.md:1166-1214` | Manual binding path is used instead of generated WebIDL pipeline. | High |
| W2-ENC-01 | Encoding | Text decoding negotiation | Missing | `src/net/url.cpp:268-318`, `clever/src/js/js_dom_bindings.cpp:9216-9222` | No Encoding Standard negotiation/decoder API implementation. | Medium |
| W2-HTTP-01 | HTTP/1.1 | Message flow | Partial | `src/net/http_client.cpp:887-1013`, `clever/src/net/response.cpp:148-260` | Basic request/response handling present; missing full invariant handling and backpressure semantics. | High |
| W2-HTTP-23 | HTTP/2/HTTP/3 | Protocols | Missing | `audit/wave0/third-party-linkage.md:29-44`, `audit/wave0/build-graph.md:1` | No protocol-client stack for H2/H3 is present. | Medium |
| W2-SEC-01 | CSP / SOP / CORS | Security policy | Partial | `include/browser/net/http_client.h:31-125`, `src/net/http_client.cpp:1146-1196`, `clever/src/js/js_fetch_bindings.cpp:135-205` | CSP/CORS enforcement is incomplete. | High |
| W2-CK-01 | Cookies / Storage | Cookie policy | Partial | `clever/src/net/cookie_jar.cpp:31-139`, `include/browser/net/http_client.h:1-25`, `src/net/http_client.cpp:1-80` | Session persistence and SameSite/context enforcement are incomplete. | High |
