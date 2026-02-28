#include <clever/js/js_timers.h>

extern "C" {
#include <quickjs.h>
}

#include <cstring>
#include <vector>

namespace clever::js {

namespace {

// =========================================================================
// Per-context timer state
// =========================================================================

struct TimerEntry {
    int id = 0;
    JSValue callback = JS_UNDEFINED;
    int delay_ms = 0;
    bool is_interval = false;
    bool cancelled = false;
};

struct TimerState {
    std::vector<TimerEntry> timers;
    int next_id = 1;
};

// Retrieve the TimerState stashed in the global object.
static TimerState* get_timer_state(JSContext* ctx) {
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue val = JS_GetPropertyStr(ctx, global, "__timer_state_ptr");
    TimerState* state = nullptr;
    if (JS_IsNumber(val)) {
        int64_t ptr_val = 0;
        JS_ToInt64(ctx, &ptr_val, val);
        state = reinterpret_cast<TimerState*>(static_cast<uintptr_t>(ptr_val));
    }
    JS_FreeValue(ctx, val);
    JS_FreeValue(ctx, global);
    return state;
}

// =========================================================================
// setTimeout(fn, delay)
// =========================================================================

static JSValue js_set_timeout(JSContext* ctx, JSValueConst /*this_val*/,
                               int argc, JSValueConst* argv) {
    auto* state = get_timer_state(ctx);
    if (!state || argc < 1) return JS_NewInt32(ctx, 0);

    int delay = 0;
    if (argc >= 2) {
        JS_ToInt32(ctx, &delay, argv[1]);
    }

    int id = state->next_id++;

    // Handle string argument (eval-style setTimeout)
    if (JS_IsString(argv[0])) {
        const char* code = JS_ToCString(ctx, argv[0]);
        if (code) {
            if (delay == 0) {
                JSValue ret = JS_Eval(ctx, code, strlen(code), "<setTimeout>", JS_EVAL_TYPE_GLOBAL);
                JS_FreeValue(ctx, ret);
            }
            // Non-zero delay string evals not supported (would need event loop)
            JS_FreeCString(ctx, code);
        }
        return JS_NewInt32(ctx, id);
    }

    if (!JS_IsFunction(ctx, argv[0])) return JS_NewInt32(ctx, 0);

    TimerEntry entry;
    entry.id = id;
    entry.callback = JS_DupValue(ctx, argv[0]);
    entry.delay_ms = delay;
    entry.is_interval = false;
    entry.cancelled = false;

    // If delay is 0, execute immediately
    if (delay == 0) {
        JSValue ret = JS_Call(ctx, entry.callback, JS_UNDEFINED, 0, nullptr);
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, entry.callback);
    } else {
        // Store for later (future event loop)
        state->timers.push_back(entry);
    }

    return JS_NewInt32(ctx, id);
}

// =========================================================================
// setInterval(fn, delay)
// =========================================================================

static JSValue js_set_interval(JSContext* ctx, JSValueConst /*this_val*/,
                                int argc, JSValueConst* argv) {
    auto* state = get_timer_state(ctx);
    if (!state || argc < 1) return JS_NewInt32(ctx, 0);

    if (!JS_IsFunction(ctx, argv[0])) return JS_NewInt32(ctx, 0);

    int delay = 0;
    if (argc >= 2) {
        JS_ToInt32(ctx, &delay, argv[1]);
    }

    int id = state->next_id++;
    TimerEntry entry;
    entry.id = id;
    entry.callback = JS_DupValue(ctx, argv[0]);
    entry.delay_ms = delay;
    entry.is_interval = true;
    entry.cancelled = false;

    // Store but don't execute (no event loop yet)
    state->timers.push_back(entry);

    return JS_NewInt32(ctx, id);
}

// =========================================================================
// clearTimeout(id) / clearInterval(id)
// =========================================================================

static JSValue js_clear_timer(JSContext* ctx, JSValueConst /*this_val*/,
                               int argc, JSValueConst* argv) {
    auto* state = get_timer_state(ctx);
    if (!state || argc < 1) return JS_UNDEFINED;

    int id = 0;
    JS_ToInt32(ctx, &id, argv[0]);

    for (auto& entry : state->timers) {
        if (entry.id == id && !entry.cancelled) {
            entry.cancelled = true;
            JS_FreeValue(ctx, entry.callback);
            entry.callback = JS_UNDEFINED;
            break;
        }
    }

    return JS_UNDEFINED;
}

} // anonymous namespace

// =========================================================================
// Public API
// =========================================================================

void install_timer_bindings(JSContext* ctx) {
    // Allocate per-context timer state
    auto* state = new TimerState();

    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "__timer_state_ptr",
        JS_NewInt64(ctx, static_cast<int64_t>(
            reinterpret_cast<uintptr_t>(state))));

    // Install global functions
    JS_SetPropertyStr(ctx, global, "setTimeout",
        JS_NewCFunction(ctx, js_set_timeout, "setTimeout", 2));
    JS_SetPropertyStr(ctx, global, "setInterval",
        JS_NewCFunction(ctx, js_set_interval, "setInterval", 2));
    JS_SetPropertyStr(ctx, global, "clearTimeout",
        JS_NewCFunction(ctx, js_clear_timer, "clearTimeout", 1));
    JS_SetPropertyStr(ctx, global, "clearInterval",
        JS_NewCFunction(ctx, js_clear_timer, "clearInterval", 1));

    JS_FreeValue(ctx, global);
}

int flush_ready_timers(JSContext* ctx, int max_delay_ms) {
    auto* state = get_timer_state(ctx);
    if (!state) return 0;

    int fired = 0;

    // Execute pending timeouts whose delay <= max_delay_ms.
    // We snapshot the current size because callbacks may add new timers.
    size_t n = state->timers.size();
    for (size_t i = 0; i < n; i++) {
        auto& entry = state->timers[i];
        if (!entry.cancelled && !entry.is_interval && entry.delay_ms <= max_delay_ms) {
            JSValue ret = JS_Call(ctx, entry.callback, JS_UNDEFINED, 0, nullptr);
            JS_FreeValue(ctx, ret);
            JS_FreeValue(ctx, entry.callback);
            entry.callback = JS_UNDEFINED;
            entry.cancelled = true;
            fired++;
        }
    }

    // Also fire setInterval callbacks once (for intervals with delay <= max_delay_ms).
    // Many sites rely on setInterval for initial setup that runs at least once.
    n = state->timers.size();
    for (size_t i = 0; i < n; i++) {
        auto& entry = state->timers[i];
        if (!entry.cancelled && entry.is_interval && entry.delay_ms <= max_delay_ms) {
            JSValue ret = JS_Call(ctx, entry.callback, JS_UNDEFINED, 0, nullptr);
            JS_FreeValue(ctx, ret);
            // Don't free callback or mark cancelled — interval persists.
            // But mark it so we don't fire it again in the same flush loop.
            // We set delay to a sentinel value larger than any reasonable threshold.
            entry.delay_ms = 999999;
            fired++;
        }
    }

    return fired;
}

void cleanup_timers(JSContext* ctx) {
    auto* state = get_timer_state(ctx);
    if (!state) return;

    // Free remaining callback references
    for (auto& entry : state->timers) {
        if (!entry.cancelled) {
            JS_FreeValue(ctx, entry.callback);
        }
    }

    delete state;

    // Clear the pointer from the global object
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "__timer_state_ptr", JS_NewInt64(ctx, 0));
    JS_FreeValue(ctx, global);
}

void flush_pending_timers(JSContext* ctx) {
    // Legacy API: flush zero-delay timers, then destroy state.
    flush_ready_timers(ctx, 0);
    cleanup_timers(ctx);
}

} // namespace clever::js
