#pragma once

struct JSContext;

namespace clever::js {

// Install setTimeout, setInterval, clearTimeout, clearInterval into the global.
// For the initial render pass, timers are NOT actually delayed -- setTimeout(fn, 0)
// executes immediately. This matches browser behavior during initial page load
// for synchronous rendering. True async timers need an event loop (future work).
void install_timer_bindings(JSContext* ctx);

// Execute any pending timer callbacks (setTimeout with delay=0).
// Call after all scripts have been evaluated.
// DEPRECATED: This flushes AND destroys timer state.  Prefer
// flush_ready_timers() + cleanup_timers() for multi-round flushing.
void flush_pending_timers(JSContext* ctx);

// Execute pending timer callbacks that are "ready" (delay <= max_delay_ms).
// Does NOT destroy timer state -- can be called multiple times.
// Returns the number of callbacks that were fired.
int flush_ready_timers(JSContext* ctx, int max_delay_ms = 0);

// Destroy per-context timer state and free callback references.
// Call once when done with JS execution.
void cleanup_timers(JSContext* ctx);

} // namespace clever::js
