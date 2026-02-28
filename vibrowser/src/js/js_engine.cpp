#include <clever/js/js_engine.h>

extern "C" {
#include <quickjs.h>
}

#include <chrono>
#include <map>
#include <optional>

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
// ES module loader — fetches, caches, and compiles imported modules
// ---------------------------------------------------------------------------

// Trampoline struct to access private module_cache_ and module_fetcher_
struct ModuleLoaderTrampoline {
    static JSModuleDef* js_module_loader_impl(JSContext* ctx,
                                               const char* module_name,
                                               void* /*opaque*/) {
        if (!ctx || !module_name) {
            return nullptr;
        }

        // Retrieve the engine from runtime opaque
        JSEngine* engine = static_cast<JSEngine*>(JS_GetRuntimeOpaque(JS_GetRuntime(ctx)));
        if (!engine) {
            return nullptr;
        }

        // Check module cache first
        auto cache_it = engine->module_cache_.find(module_name);
        if (cache_it != engine->module_cache_.end()) {
            return cache_it->second;
        }

        // Try to fetch the module source
        std::optional<std::string> module_source;
        if (engine->module_fetcher_) {
            module_source = engine->module_fetcher_(module_name);
        }

        if (!module_source.has_value()) {
            // Fetch failed, return nullptr to signal error
            return nullptr;
        }

        // Compile the module source
        JSValue module_val = JS_Eval(ctx, module_source->c_str(), module_source->size(),
                                      module_name,
                                      JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);

        if (JS_IsException(module_val)) {
            JS_FreeValue(ctx, module_val);
            return nullptr;
        }

        // Extract the JSModuleDef* from the function value
        JSModuleDef* m = (JSModuleDef*)JS_VALUE_GET_PTR(module_val);
        JS_FreeValue(ctx, module_val);

        if (m) {
            // Cache the compiled module
            engine->module_cache_[module_name] = m;
        }

        return m;
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

    // Stash `this` on the runtime so the module loader can access it
    JS_SetRuntimeOpaque(rt_, this);

    // Install the module loader (will call js_module_loader for import statements)
    JS_SetModuleLoaderFunc(rt_, nullptr, ModuleLoaderTrampoline::js_module_loader_impl, nullptr);

    ctx_ = JS_NewContext(rt_);
    if (!ctx_) return;

    // Stash `this` so C callbacks can find the engine via context
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
      console_callback_(std::move(other.console_callback_)),
      module_fetcher_(std::move(other.module_fetcher_)),
      module_cache_(std::move(other.module_cache_)) {
    other.rt_ = nullptr;
    other.ctx_ = nullptr;
    // Re-point the context opaque to the new address.
    if (ctx_) {
        JS_SetContextOpaque(ctx_, this);
        JS_SetRuntimeOpaque(rt_, this);
    }
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
        module_fetcher_ = std::move(other.module_fetcher_);
        module_cache_ = std::move(other.module_cache_);

        other.rt_ = nullptr;
        other.ctx_ = nullptr;

        if (ctx_) {
            JS_SetContextOpaque(ctx_, this);
            JS_SetRuntimeOpaque(rt_, this);
        }
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
// Module evaluation — executes code as an ES module (import/export support)
// ---------------------------------------------------------------------------

std::string JSEngine::evaluate_module(const std::string& code,
                                      const std::string& filename) {
    clear_error();

    if (!ctx_) {
        has_error_ = true;
        last_error_ = "JSEngine not initialized";
        return "";
    }

    JSValue result = JS_Eval(ctx_, code.c_str(), code.size(),
                             filename.c_str(),
                             JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);

    if (JS_IsException(result)) {
        has_error_ = true;

        JSValue exception = JS_GetException(ctx_);
        const char* str = JS_ToCString(ctx_, exception);
        if (str) {
            last_error_ = str;
            JS_FreeCString(ctx_, str);
        } else {
            last_error_ = "Unknown module parse error";
        }

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

    // Evaluate the compiled module (this runs top-level module code).
    JSValue eval_result = JS_EvalFunction(ctx_, result);

    if (JS_IsException(eval_result)) {
        has_error_ = true;

        JSValue exception = JS_GetException(ctx_);
        const char* str = JS_ToCString(ctx_, exception);
        if (str) {
            last_error_ = str;
            JS_FreeCString(ctx_, str);
        } else {
            last_error_ = "Unknown module evaluation error";
        }

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

    std::string result_str;
    if (!JS_IsUndefined(eval_result)) {
        const char* str = JS_ToCString(ctx_, eval_result);
        if (str) {
            result_str = str;
            JS_FreeCString(ctx_, str);
        }
    }

    JS_FreeValue(ctx_, eval_result);
    return result_str;
}

// ---------------------------------------------------------------------------
// Console callback
// ---------------------------------------------------------------------------

void JSEngine::set_console_callback(ConsoleCallback cb) {
    console_callback_ = std::move(cb);
}

void JSEngine::set_module_fetcher(ModuleFetcher fetcher) {
    module_fetcher_ = std::move(fetcher);
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
