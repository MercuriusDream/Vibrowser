#pragma once

struct JSContext;

namespace clever::js {

// Install setTimeout, setInterval, clearTimeout, clearInterval into the global.
// Timers are scheduled onto a host-managed queue and fire only when
// flush_ready_timers() advances the host clock to their deadline.
void install_timer_bindings(JSContext* ctx);

// Execute the currently ready timers, then destroy timer state.
// DEPRECATED: Prefer flush_ready_timers() + cleanup_timers() for multi-round flushing.
void flush_pending_timers(JSContext* ctx);

// Advance the host timer clock by max_delay_ms and execute timers whose
// scheduled deadline is now due. Does NOT destroy timer state.
// Returns the number of macrotasks that were fired.
int flush_ready_timers(JSContext* ctx, int max_delay_ms = 0);

// Destroy per-context timer state and free callback references.
// Call once when done with JS execution.
void cleanup_timers(JSContext* ctx);

} // namespace clever::js
