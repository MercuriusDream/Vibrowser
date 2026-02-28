#pragma once

extern "C" {
struct JSContext;
}

namespace clever::js {

// Install XMLHttpRequest, WebSocket constructors/prototypes and the global
// fetch() function into the JS context.  Also registers the Response and
// Headers classes used by fetch().
//
// - XMLHttpRequest: synchronous XHR via the engine's HTTP client
// - fetch(url, options?): returns a Promise<Response> (synchronous internally)
// - Response: .ok, .status, .statusText, .url, .type, .headers, .text(), .json(), .clone()
// - Headers: .get(name), .has(name), .forEach(cb), .entries(), .keys(), .values()
// - WebSocket: .send(data), .close([code, reason]), readyState, url, protocol,
//              bufferedAmount, onopen/onmessage/onclose/onerror,
//              CONNECTING/OPEN/CLOSING/CLOSED static constants
void install_fetch_bindings(JSContext* ctx);

// Execute all pending QuickJS Promise microtask jobs.
// Call this after script evaluation to ensure .then() callbacks run.
void flush_fetch_promise_jobs(JSContext* ctx);

} // namespace clever::js
