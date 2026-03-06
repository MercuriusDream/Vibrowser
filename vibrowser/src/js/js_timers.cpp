#include <clever/js/js_timers.h>
#include <clever/platform/timer.h>

extern "C" {
#include <quickjs.h>
}

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>

namespace clever::js {

namespace {

void run_timer(JSContext* ctx, struct TimerState& state, int id);

struct TimerEntry {
    int id = 0;
    int scheduled_task_id = 0;
    JSValue callback = JS_UNDEFINED;
    std::string code;
    clever::platform::TimerScheduler::Duration delay { clever::platform::TimerScheduler::Duration::zero() };
    clever::platform::TimerScheduler::TimePoint scheduled_fire_time {
        clever::platform::TimerScheduler::Duration::zero()
    };
    bool is_interval = false;
    bool cancelled = false;
};

struct TimerState {
    clever::platform::TimerScheduler scheduler;
    clever::platform::TimerScheduler::TimePoint now { clever::platform::TimerScheduler::Duration::zero() };
    int next_timer_id = 1;
    std::unordered_map<int, std::shared_ptr<TimerEntry>> timers;
};

TimerState* get_timer_state(JSContext* ctx) {
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

void drain_pending_jobs(JSContext* ctx) {
    JSContext* ctx_job = nullptr;
    while (JS_ExecutePendingJob(JS_GetRuntime(ctx), &ctx_job) > 0) {
    }
}

void release_timer_entry(JSContext* ctx, const std::shared_ptr<TimerEntry>& entry) {
    if (!JS_IsUndefined(entry->callback)) {
        JS_FreeValue(ctx, entry->callback);
        entry->callback = JS_UNDEFINED;
    }
    entry->cancelled = true;
}

void schedule_timer(JSContext* ctx, TimerState& state, const std::shared_ptr<TimerEntry>& entry,
    clever::platform::TimerScheduler::Duration delay)
{
    entry->scheduled_task_id = state.scheduler.schedule_timeout(delay, [ctx, &state, id = entry->id]() {
        run_timer(ctx, state, id);
    });
}

void cleanup_timer(JSContext* ctx, TimerState& state, int id) {
    auto it = state.timers.find(id);
    if (it == state.timers.end()) {
        return;
    }
    release_timer_entry(ctx, it->second);
    state.timers.erase(it);
}

void run_timer(JSContext* ctx, TimerState& state, int id) {
    auto it = state.timers.find(id);
    if (it == state.timers.end()) {
        return;
    }

    auto entry = it->second;
    entry->scheduled_task_id = 0;
    if (entry->cancelled) {
        cleanup_timer(ctx, state, id);
        return;
    }

    if (!entry->code.empty()) {
        JSValue result = JS_Eval(
            ctx,
            entry->code.c_str(),
            entry->code.size(),
            "<setTimeout>",
            JS_EVAL_TYPE_GLOBAL);
        JS_FreeValue(ctx, result);
    } else if (!JS_IsUndefined(entry->callback)) {
        // Keep the JS function alive even if clearTimeout/clearInterval runs inside it.
        JSValue callback = JS_DupValue(ctx, entry->callback);
        JSValue result = JS_Call(ctx, callback, JS_UNDEFINED, 0, nullptr);
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, callback);
    }

    drain_pending_jobs(ctx);

    if (!entry->is_interval || entry->cancelled) {
        cleanup_timer(ctx, state, id);
        return;
    }

    if (state.timers.find(id) == state.timers.end()) {
        return;
    }

    if (entry->delay <= clever::platform::TimerScheduler::Duration::zero()) {
        entry->scheduled_fire_time = state.now;
    } else {
        do {
            entry->scheduled_fire_time += entry->delay;
        } while (entry->scheduled_fire_time <= state.now);
    }

    auto next_delay = entry->scheduled_fire_time - state.now;
    if (next_delay < clever::platform::TimerScheduler::Duration::zero()) {
        next_delay = clever::platform::TimerScheduler::Duration::zero();
    }

    schedule_timer(ctx, state, entry, next_delay);
}

JSValue js_set_timeout(JSContext* ctx, JSValueConst /*this_val*/, int argc, JSValueConst* argv) {
    auto* state = get_timer_state(ctx);
    if (!state || argc < 1) {
        return JS_NewInt32(ctx, 0);
    }

    int delay = 0;
    if (argc >= 2) {
        JS_ToInt32(ctx, &delay, argv[1]);
    }
    auto delay_ms = std::chrono::milliseconds(std::max(delay, 0));

    auto entry = std::make_shared<TimerEntry>();
    entry->id = state->next_timer_id++;
    entry->delay = delay_ms;
    entry->scheduled_fire_time = state->now + delay_ms;
    if (JS_IsString(argv[0])) {
        const char* code = JS_ToCString(ctx, argv[0]);
        if (code) {
            entry->code = code;
            JS_FreeCString(ctx, code);
        }
    } else if (JS_IsFunction(ctx, argv[0])) {
        entry->callback = JS_DupValue(ctx, argv[0]);
    } else {
        return JS_NewInt32(ctx, 0);
    }

    state->timers[entry->id] = entry;
    schedule_timer(ctx, *state, entry, delay_ms);
    return JS_NewInt32(ctx, entry->id);
}

JSValue js_set_interval(JSContext* ctx, JSValueConst /*this_val*/, int argc, JSValueConst* argv) {
    auto* state = get_timer_state(ctx);
    if (!state || argc < 1 || !JS_IsFunction(ctx, argv[0])) {
        return JS_NewInt32(ctx, 0);
    }

    int delay = 0;
    if (argc >= 2) {
        JS_ToInt32(ctx, &delay, argv[1]);
    }
    auto interval = std::chrono::milliseconds(std::max(delay, 0));

    auto entry = std::make_shared<TimerEntry>();
    entry->id = state->next_timer_id++;
    entry->callback = JS_DupValue(ctx, argv[0]);
    entry->delay = interval;
    entry->scheduled_fire_time = state->now + interval;
    entry->is_interval = true;

    state->timers[entry->id] = entry;
    schedule_timer(ctx, *state, entry, interval);
    return JS_NewInt32(ctx, entry->id);
}

JSValue js_clear_timer(JSContext* ctx, JSValueConst /*this_val*/, int argc, JSValueConst* argv) {
    auto* state = get_timer_state(ctx);
    if (!state || argc < 1) {
        return JS_UNDEFINED;
    }

    int id = 0;
    JS_ToInt32(ctx, &id, argv[0]);
    auto it = state->timers.find(id);
    if (it != state->timers.end() && it->second->scheduled_task_id != 0) {
        state->scheduler.cancel(it->second->scheduled_task_id);
        it->second->scheduled_task_id = 0;
    }
    cleanup_timer(ctx, *state, id);
    return JS_UNDEFINED;
}

} // namespace

void install_timer_bindings(JSContext* ctx) {
    auto* state = new TimerState();

    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(
        ctx,
        global,
        "__timer_state_ptr",
        JS_NewInt64(ctx, static_cast<int64_t>(reinterpret_cast<uintptr_t>(state))));

    JS_SetPropertyStr(ctx, global, "setTimeout", JS_NewCFunction(ctx, js_set_timeout, "setTimeout", 2));
    JS_SetPropertyStr(ctx, global, "setInterval", JS_NewCFunction(ctx, js_set_interval, "setInterval", 2));
    JS_SetPropertyStr(ctx, global, "clearTimeout", JS_NewCFunction(ctx, js_clear_timer, "clearTimeout", 1));
    JS_SetPropertyStr(ctx, global, "clearInterval", JS_NewCFunction(ctx, js_clear_timer, "clearInterval", 1));

    JS_FreeValue(ctx, global);
}

int flush_ready_timers(JSContext* ctx, int max_delay_ms) {
    auto* state = get_timer_state(ctx);
    if (!state) {
        return 0;
    }
    auto advance = std::chrono::milliseconds(std::max(max_delay_ms, 0));
    state->now += advance;
    return static_cast<int>(state->scheduler.run_ready(advance));
}

void cleanup_timers(JSContext* ctx) {
    auto* state = get_timer_state(ctx);
    if (!state) {
        return;
    }

    state->scheduler.clear();
    for (auto& [id, entry] : state->timers) {
        (void)id;
        release_timer_entry(ctx, entry);
    }
    state->timers.clear();
    delete state;

    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "__timer_state_ptr", JS_NewInt64(ctx, 0));
    JS_FreeValue(ctx, global);
}

void flush_pending_timers(JSContext* ctx) {
    flush_ready_timers(ctx, 0);
    cleanup_timers(ctx);
}

} // namespace clever::js
