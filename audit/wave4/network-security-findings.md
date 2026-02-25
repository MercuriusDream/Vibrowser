# Wave 4 — Networking, Security, Robustness

## FindingRecord

| id | subsystem | scope | status | severity | criticality | evidence | spec_citation | certainty | source_wave | notes | resolution_hint |
|---|---|---|---|---|---|---|---|---|---|---|
| W4-NET-01 | from_scratch | `src/net/http_client.cpp` | Missing | High | High | `src/net/http_client.cpp:285-317` | TLS/PKI | High | Wave 4/6 | HTTPS path does not enforce certificate verification policy before trust acceptance. | Add `SSL_set_verify` and `SSL_get_verify_result` with hard-failure handling. |
| W4-NET-02 | clever | `clever/src/js/js_fetch_bindings.cpp` | Partial | High | High | `clever/src/js/js_fetch_bindings.cpp:135-205` | CORS model | High | Wave 4/6 | `XMLHttpRequest.send` exposes cross-origin responses without CORS preflight/Origin enforcement. | Add `Origin` emission, preflight flow, and response allowlist checks. |
| W4-SEC-01 | clever | `clever/src/net/cookie_jar.cpp` | Partial | Medium | High | `clever/src/net/cookie_jar.cpp:31-139` | SameSite context policy | High | Wave 4/6 | `SameSite` values are parsed but ignored when selecting cookies to send. | Enforce SameSite at `get_cookie_header` time. |
| W4-SEC-02 | clever | `clever/src/net/http_client.cpp` | Partial | Medium | Medium | `clever/src/net/http_client.cpp:58-75`, `clever/src/net/http_client.cpp:637-675` | HTTP caching | High | Wave 4/6 | `private` cache flags are parsed but not fully enforced in cache store/lookup. | Skip caching private responses or partition cache by context. |
| W4-NET-03 | shared | `audit/full-traversal-coverage-ledger.md`, `audit/wave0/build-graph.md` | Missing | Medium | Medium | `audit/wave0/third-party-linkage.md:29-44` | HTTP/2/3 transport | Medium | Wave 4 | No evidenced HTTP/2/HTTP/3 protocol path in either stack. | Add transport stack decision with ALPN and stream/priority semantics if production target expands. |
