#include <clever/js/js_engine.h>

extern "C" {
#include <quickjs.h>
}

#include <chrono>
#include <map>

namespace clever::js {

// Timer storage for console.time/timeEnd
static std::map<std::string, std::chrono::steady_clock::time_point>& console_timers() {
    static std::map<std::string, std::chrono::steady_clock::time_point> timers;
    return timers;
}

// Counter storage for console.count
static std::map<std::string, int>& console_counters() {
    static std::map<std::string, int> counters;
    return counters;
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Retrieve the owning JSEngine* stashed in the context opaque pointer.
static JSEngine* get_engine(JSContext* ctx) {
    return static_cast<JSEngine*>(JS_GetContextOpaque(ctx));
}

// Console.log / warn / error / info implementation.
// Wrapped in ConsoleTrampoline (a friend struct declared in js_engine.h)
// so it can access JSEngine's private members.
struct ConsoleTrampoline {
    // Registered with JS_CFUNC_generic_magic so the `magic` parameter selects
    // the log level:  0 = log, 1 = warn, 2 = error, 3 = info.
    static JSValue js_console_log(JSContext* ctx, JSValueConst this_val,
                                   int argc, JSValueConst* argv, int magic) {
        (void)this_val;
        auto* engine = get_engine(ctx);

        // Concatenate all arguments with spaces (like browser console).
        std::string message;
        for (int i = 0; i < argc; i++) {
            if (i > 0) message += ' ';
            const char* str = JS_ToCString(ctx, argv[i]);
            if (str) {
                message += str;
                JS_FreeCString(ctx, str);
            }
        }

        // Map magic value to level string.
        static const char* levels[] = {"log", "warn", "error", "info"};
        const char* level = (magic >= 0 && magic <= 3) ? levels[magic] : "log";

        if (engine) {
            engine->console_output_.push_back(std::string("[") + level + "] " + message);
            if (engine->console_callback_) {
                engine->console_callback_(level, message);
            }
        }

        return JS_UNDEFINED;
    }

    // console.trace(...args) — prints "console.trace()" prefix then args
    static JSValue js_console_trace(JSContext* ctx, JSValueConst this_val,
                                     int argc, JSValueConst* argv) {
        (void)this_val;
        auto* engine = get_engine(ctx);

        std::string message = "console.trace()";
        for (int i = 0; i < argc; i++) {
            message += ' ';
            const char* str = JS_ToCString(ctx, argv[i]);
            if (str) {
                message += str;
                JS_FreeCString(ctx, str);
            }
        }

        if (engine) {
            engine->console_output_.push_back("[log] " + message);
            if (engine->console_callback_) {
                engine->console_callback_("log", message);
            }
        }
        return JS_UNDEFINED;
    }

    // console.assert(condition, ...args)
    static JSValue js_console_assert(JSContext* ctx, JSValueConst this_val,
                                      int argc, JSValueConst* argv) {
        (void)this_val;
        auto* engine = get_engine(ctx);

        // If condition is truthy, do nothing
        if (argc >= 1 && JS_ToBool(ctx, argv[0])) {
            return JS_UNDEFINED;
        }

        std::string message = "Assertion failed:";
        for (int i = 1; i < argc; i++) {
            message += ' ';
            const char* str = JS_ToCString(ctx, argv[i]);
            if (str) {
                message += str;
                JS_FreeCString(ctx, str);
            }
        }

        if (engine) {
            engine->console_output_.push_back("[error] " + message);
            if (engine->console_callback_) {
                engine->console_callback_("error", message);
            }
        }
        return JS_UNDEFINED;
    }

    // console.time(label)
    static JSValue js_console_time(JSContext* ctx, JSValueConst /*this_val*/,
                                    int argc, JSValueConst* argv) {
        std::string label = "default";
        if (argc >= 1) {
            const char* str = JS_ToCString(ctx, argv[0]);
            if (str) {
                label = str;
                JS_FreeCString(ctx, str);
            }
        }
        console_timers()[label] = std::chrono::steady_clock::now();
        return JS_UNDEFINED;
    }

    // console.timeEnd(label)
    static JSValue js_console_time_end(JSContext* ctx, JSValueConst /*this_val*/,
                                        int argc, JSValueConst* argv) {
        auto* engine = get_engine(ctx);
        std::string label = "default";
        if (argc >= 1) {
            const char* str = JS_ToCString(ctx, argv[0]);
            if (str) {
                label = str;
                JS_FreeCString(ctx, str);
            }
        }

        auto& timers = console_timers();
        auto it = timers.find(label);
        if (it != timers.end()) {
            auto elapsed = std::chrono::steady_clock::now() - it->second;
            double ms = std::chrono::duration<double, std::milli>(elapsed).count();
            timers.erase(it);

            char buf[128];
            std::snprintf(buf, sizeof(buf), "%s: %.3fms", label.c_str(), ms);
            std::string message(buf);

            if (engine) {
                engine->console_output_.push_back("[log] " + message);
                if (engine->console_callback_) {
                    engine->console_callback_("log", message);
                }
            }
        }
        return JS_UNDEFINED;
    }

    // console.count(label)
    static JSValue js_console_count(JSContext* ctx, JSValueConst /*this_val*/,
                                     int argc, JSValueConst* argv) {
        auto* engine = get_engine(ctx);
        std::string label = "default";
        if (argc >= 1) {
            const char* str = JS_ToCString(ctx, argv[0]);
            if (str) {
                label = str;
                JS_FreeCString(ctx, str);
            }
        }

        int count = ++console_counters()[label];
        std::string message = label + ": " + std::to_string(count);

        if (engine) {
            engine->console_output_.push_back("[log] " + message);
            if (engine->console_callback_) {
                engine->console_callback_("log", message);
            }
        }
        return JS_UNDEFINED;
    }

    // console.clear / console.group / console.groupEnd — no-ops
    static JSValue js_console_noop(JSContext* /*ctx*/, JSValueConst /*this_val*/,
                                    int /*argc*/, JSValueConst* /*argv*/) {
        return JS_UNDEFINED;
    }
};

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

JSEngine::JSEngine() {
    rt_ = JS_NewRuntime();
    if (!rt_) return;

    // Set memory limit (256 MB) and stack size (8 MB).
    // 64 MB was too small for complex pages like Google.
    JS_SetMemoryLimit(rt_, 256 * 1024 * 1024);
    JS_SetMaxStackSize(rt_, 8 * 1024 * 1024);

    ctx_ = JS_NewContext(rt_);
    if (!ctx_) return;

    // Stash `this` so C callbacks can find the engine.
    JS_SetContextOpaque(ctx_, this);
    setup_console();
}

JSEngine::~JSEngine() {
    if (ctx_) JS_FreeContext(ctx_);
    if (rt_) JS_FreeRuntime(rt_);
}

// ---------------------------------------------------------------------------
// Move semantics
// ---------------------------------------------------------------------------

JSEngine::JSEngine(JSEngine&& other) noexcept
    : rt_(other.rt_),
      ctx_(other.ctx_),
      has_error_(other.has_error_),
      last_error_(std::move(other.last_error_)),
      console_output_(std::move(other.console_output_)),
      console_callback_(std::move(other.console_callback_)) {
    other.rt_ = nullptr;
    other.ctx_ = nullptr;
    // Re-point the context opaque to the new address.
    if (ctx_) JS_SetContextOpaque(ctx_, this);
}

JSEngine& JSEngine::operator=(JSEngine&& other) noexcept {
    if (this != &other) {
        // Tear down existing state.
        if (ctx_) JS_FreeContext(ctx_);
        if (rt_) JS_FreeRuntime(rt_);

        rt_ = other.rt_;
        ctx_ = other.ctx_;
        has_error_ = other.has_error_;
        last_error_ = std::move(other.last_error_);
        console_output_ = std::move(other.console_output_);
        console_callback_ = std::move(other.console_callback_);

        other.rt_ = nullptr;
        other.ctx_ = nullptr;

        if (ctx_) JS_SetContextOpaque(ctx_, this);
    }
    return *this;
}

// ---------------------------------------------------------------------------
// Error handling
// ---------------------------------------------------------------------------

void JSEngine::clear_error() {
    has_error_ = false;
    last_error_.clear();
}

// ---------------------------------------------------------------------------
// Evaluation
// ---------------------------------------------------------------------------

std::string JSEngine::evaluate(const std::string& code, const std::string& filename) {
    clear_error();

    if (!ctx_) {
        has_error_ = true;
        last_error_ = "JSEngine not initialized";
        return "";
    }

    JSValue result = JS_Eval(ctx_, code.c_str(), code.size(),
                             filename.c_str(), JS_EVAL_TYPE_GLOBAL);

    if (JS_IsException(result)) {
        has_error_ = true;

        JSValue exception = JS_GetException(ctx_);
        const char* str = JS_ToCString(ctx_, exception);
        if (str) {
            last_error_ = str;
            JS_FreeCString(ctx_, str);
        } else {
            last_error_ = "Unknown error";
        }

        // Attempt to capture the stack trace as well.
        JSValue stack = JS_GetPropertyStr(ctx_, exception, "stack");
        if (!JS_IsUndefined(stack)) {
            const char* stack_str = JS_ToCString(ctx_, stack);
            if (stack_str) {
                last_error_ += '\n';
                last_error_ += stack_str;
                JS_FreeCString(ctx_, stack_str);
            }
        }
        JS_FreeValue(ctx_, stack);
        JS_FreeValue(ctx_, exception);
        return "";
    }

    // Convert the result to a string representation.
    std::string result_str;
    if (!JS_IsUndefined(result)) {
        const char* str = JS_ToCString(ctx_, result);
        if (str) {
            result_str = str;
            JS_FreeCString(ctx_, str);
        }
    }

    JS_FreeValue(ctx_, result);
    return result_str;
}

// ---------------------------------------------------------------------------
// Console callback
// ---------------------------------------------------------------------------

void JSEngine::set_console_callback(ConsoleCallback cb) {
    console_callback_ = std::move(cb);
}

// ---------------------------------------------------------------------------
// Console setup — installs a global `console` object with log/warn/error/info
// ---------------------------------------------------------------------------

void JSEngine::setup_console() {
    if (!ctx_) return;

    JSValue global = JS_GetGlobalObject(ctx_);
    JSValue console = JS_NewObject(ctx_);

    // Use JS_NewCFunctionMagic which is the type-safe wrapper provided by
    // quickjs.h (avoids undefined-behaviour casts).  The `magic` integer is
    // forwarded to js_console_log and selects the log level.
    JS_SetPropertyStr(ctx_, console, "log",
        JS_NewCFunctionMagic(ctx_, ConsoleTrampoline::js_console_log, "log", 1,
                             JS_CFUNC_generic_magic, 0));
    JS_SetPropertyStr(ctx_, console, "warn",
        JS_NewCFunctionMagic(ctx_, ConsoleTrampoline::js_console_log, "warn", 1,
                             JS_CFUNC_generic_magic, 1));
    JS_SetPropertyStr(ctx_, console, "error",
        JS_NewCFunctionMagic(ctx_, ConsoleTrampoline::js_console_log, "error", 1,
                             JS_CFUNC_generic_magic, 2));
    JS_SetPropertyStr(ctx_, console, "info",
        JS_NewCFunctionMagic(ctx_, ConsoleTrampoline::js_console_log, "info", 1,
                             JS_CFUNC_generic_magic, 3));

    // debug / dir / table — same as console.log (magic level 0)
    JS_SetPropertyStr(ctx_, console, "debug",
        JS_NewCFunctionMagic(ctx_, ConsoleTrampoline::js_console_log, "debug", 1,
                             JS_CFUNC_generic_magic, 0));
    JS_SetPropertyStr(ctx_, console, "dir",
        JS_NewCFunctionMagic(ctx_, ConsoleTrampoline::js_console_log, "dir", 1,
                             JS_CFUNC_generic_magic, 0));
    JS_SetPropertyStr(ctx_, console, "table",
        JS_NewCFunctionMagic(ctx_, ConsoleTrampoline::js_console_log, "table", 1,
                             JS_CFUNC_generic_magic, 0));

    // console.trace
    JS_SetPropertyStr(ctx_, console, "trace",
        JS_NewCFunction(ctx_, ConsoleTrampoline::js_console_trace, "trace", 0));

    // console.assert
    JS_SetPropertyStr(ctx_, console, "assert",
        JS_NewCFunction(ctx_, ConsoleTrampoline::js_console_assert, "assert", 1));

    // console.time / timeEnd
    JS_SetPropertyStr(ctx_, console, "time",
        JS_NewCFunction(ctx_, ConsoleTrampoline::js_console_time, "time", 0));
    JS_SetPropertyStr(ctx_, console, "timeEnd",
        JS_NewCFunction(ctx_, ConsoleTrampoline::js_console_time_end, "timeEnd", 0));

    // console.count
    JS_SetPropertyStr(ctx_, console, "count",
        JS_NewCFunction(ctx_, ConsoleTrampoline::js_console_count, "count", 0));

    // console.clear / group / groupEnd — no-ops
    JS_SetPropertyStr(ctx_, console, "clear",
        JS_NewCFunction(ctx_, ConsoleTrampoline::js_console_noop, "clear", 0));
    JS_SetPropertyStr(ctx_, console, "group",
        JS_NewCFunction(ctx_, ConsoleTrampoline::js_console_noop, "group", 0));
    JS_SetPropertyStr(ctx_, console, "groupEnd",
        JS_NewCFunction(ctx_, ConsoleTrampoline::js_console_noop, "groupEnd", 0));

    JS_SetPropertyStr(ctx_, global, "console", console);
    JS_FreeValue(ctx_, global);
}

} // namespace clever::js
