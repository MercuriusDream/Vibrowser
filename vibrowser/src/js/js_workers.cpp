#include <clever/js/js_workers.h>

extern "C" {
#include <quickjs.h>
}

#include <chrono>
#include <unordered_map>
#include <vector>

namespace clever::js {

namespace {

// =========================================================================
// Class IDs and per-context state
// =========================================================================

static JSClassID worker_class_id = 0;

struct WorkerStateEntry {
    std::shared_ptr<WorkerThread> thread;
    JSValue js_object;  // Keep reference to the JS object for message delivery
};

struct WorkerState {
    // Map of worker threads to their JS objects and state
    std::unordered_map<WorkerThread*, WorkerStateEntry> workers;
    JSContext* ctx = nullptr;
};

// Static map of context → WorkerState (avoids overwriting JS_SetContextOpaque)
static std::unordered_map<JSContext*, std::unique_ptr<WorkerState>> g_worker_states;
static std::mutex g_worker_states_mutex;

// Get or create WorkerState for a context
WorkerState* get_worker_state(JSContext* ctx) {
    std::lock_guard<std::mutex> lock(g_worker_states_mutex);
    auto it = g_worker_states.find(ctx);
    if (it != g_worker_states.end()) {
        return it->second.get();
    }
    auto state = std::make_unique<WorkerState>();
    state->ctx = ctx;
    WorkerState* raw = state.get();
    g_worker_states[ctx] = std::move(state);
    return raw;
}

static bool parse_json_payload(JSContext* ctx, JSValueConst value, std::string& out) {
    JSValue json_val = JS_JSONStringify(ctx, value, JS_UNDEFINED, JS_UNDEFINED);
    if (JS_IsException(json_val)) {
        JSValue error = JS_GetException(ctx);
        const char* cstr = JS_ToCString(ctx, error);
        if (cstr) {
            JS_ThrowTypeError(ctx, "Failed to serialize message: %s", cstr);
            JS_FreeCString(ctx, cstr);
        } else {
            JS_ThrowTypeError(ctx, "Failed to serialize message");
        }
        JS_FreeValue(ctx, error);
        return false;
    }

    if (JS_IsUndefined(json_val)) {
        out = "null";
        JS_FreeValue(ctx, json_val);
        return true;
    }

    const char* json_cstr = JS_ToCString(ctx, json_val);
    if (!json_cstr) {
        JS_ThrowTypeError(ctx, "Failed to serialize message");
        JS_FreeValue(ctx, json_val);
        return false;
    }

    out = json_cstr;
    JS_FreeCString(ctx, json_cstr);
    JS_FreeValue(ctx, json_val);
    return true;
}

static bool parse_ports_option(JSContext* ctx, JSValueConst arg, std::string& ports_json) {
    if (JS_IsUndefined(arg) || JS_IsNull(arg)) {
        ports_json = "[]";
        return true;
    }

    JSValue ports_arg = JS_UNDEFINED;
    if (JS_IsArray(ctx, arg)) {
        ports_arg = JS_DupValue(ctx, arg);
    } else if (JS_IsObject(arg)) {
        ports_arg = JS_GetPropertyStr(ctx, arg, "ports");
        if (JS_IsUndefined(ports_arg) || JS_IsNull(ports_arg)) {
            ports_json = "[]";
            JS_FreeValue(ctx, ports_arg);
            return true;
        }
    } else {
        JS_ThrowTypeError(ctx, "postMessage options must be an object or transfer list");
        return false;
    }

    if (!JS_IsArray(ctx, ports_arg)) {
        JS_ThrowTypeError(ctx, "postMessage options ports must be an array");
        JS_FreeValue(ctx, ports_arg);
        return false;
    }

    bool ok = parse_json_payload(ctx, ports_arg, ports_json);
    JS_FreeValue(ctx, ports_arg);
    return ok;
}

static std::string get_string_property(JSContext* ctx, JSValueConst obj, const char* key) {
    JSValue value = JS_GetPropertyStr(ctx, obj, key);
    if (JS_IsException(value)) {
        JS_FreeValue(ctx, value);
        return "";
    }
    if (JS_IsUndefined(value) || JS_IsNull(value)) {
        JS_FreeValue(ctx, value);
        return "";
    }

    const char* cstr = JS_ToCString(ctx, value);
    std::string result;
    if (cstr) {
        result = cstr;
        JS_FreeCString(ctx, cstr);
    }
    JS_FreeValue(ctx, value);
    return result;
}

static int32_t get_int_property(JSContext* ctx, JSValueConst obj, const char* key, int32_t fallback) {
    JSValue value = JS_GetPropertyStr(ctx, obj, key);
    if (JS_IsException(value) || JS_IsUndefined(value) || JS_IsNull(value)) {
        JS_FreeValue(ctx, value);
        return fallback;
    }

    int32_t result = fallback;
    JS_ToInt32(ctx, &result, value);
    JS_FreeValue(ctx, value);
    return result;
}

static WorkerMessage worker_error_from_exception(
    JSContext* ctx,
    const std::string& default_message,
    const std::string& default_filename = {},
    int32_t default_lineno = 0) {
    WorkerMessage msg;
    msg.kind = WorkerMessageKind::kError;
    msg.ports = "[]";
    msg.name = "Error";
    msg.filename = default_filename;
    msg.lineno = default_lineno;

    JSValue error = JS_GetException(ctx);
    std::string message = get_string_property(ctx, error, "message");
    if (!message.empty()) {
        msg.data = message;
    } else {
        const char* message_cstr = JS_ToCString(ctx, error);
        if (message_cstr) {
            msg.data = message_cstr;
            JS_FreeCString(ctx, message_cstr);
        }
    }
    if (msg.data.empty()) {
        msg.data = default_message;
    }

    std::string name = get_string_property(ctx, error, "name");
    if (!name.empty()) {
        msg.name = name;
    }

    std::string filename = get_string_property(ctx, error, "fileName");
    if (filename.empty()) {
        filename = get_string_property(ctx, error, "filename");
    }
    if (!filename.empty()) {
        msg.filename = filename;
    }
    int32_t lineno = get_int_property(ctx, error, "lineNumber", msg.lineno);
    if (lineno != 0) {
        msg.lineno = lineno;
    }
    if (msg.lineno == 0) {
        msg.lineno = get_int_property(ctx, error, "lineno", 0);
    }

    JS_FreeValue(ctx, error);
    return msg;
}

static JSValue make_message_event(JSContext* ctx,
                                 const std::string& type,
                                 JSValue data,
                                 const std::string& origin,
                                 JSValue source,
                                 const std::string& ports_json,
                                 bool bubbles,
                                 bool cancelable) {
    JSValue event_obj = JS_NewObject(ctx);
    JS_DefinePropertyValueStr(ctx, event_obj, "type", JS_NewString(ctx, type.c_str()), 0);
    JS_DefinePropertyValueStr(ctx, event_obj, "data", data, 0);
    JS_DefinePropertyValueStr(ctx, event_obj, "origin", JS_NewString(ctx, origin.c_str()), 0);
    JS_DefinePropertyValueStr(ctx, event_obj, "source", JS_DupValue(ctx, source), 0);

    JSValue ports = JS_ParseJSON(ctx, ports_json.c_str(), ports_json.size(), "<message ports>");
    if (JS_IsException(ports)) {
        JS_FreeValue(ctx, JS_GetException(ctx));
        ports = JS_NewArray(ctx);
    }
    JS_DefinePropertyValueStr(ctx, event_obj, "ports", ports, 0);

    JS_DefinePropertyValueStr(ctx, event_obj, "bubbles", JS_NewBool(ctx, bubbles), 0);
    JS_DefinePropertyValueStr(ctx, event_obj, "cancelable", JS_NewBool(ctx, cancelable), 0);
    JS_DefinePropertyValueStr(ctx, event_obj, "defaultPrevented", JS_FALSE, 0);
    JS_DefinePropertyValueStr(ctx, event_obj, "target", JS_NULL, 0);
    JS_DefinePropertyValueStr(ctx, event_obj, "currentTarget", JS_NULL, 0);
    return event_obj;
}

static JSValue make_error_event(JSContext* ctx, const WorkerMessage& msg) {
    JSValue event_obj = JS_NewObject(ctx);
    JS_DefinePropertyValueStr(ctx, event_obj, "type", JS_NewString(ctx, "error"), 0);
    JS_DefinePropertyValueStr(ctx, event_obj, "bubbles", JS_FALSE, 0);
    JS_DefinePropertyValueStr(ctx, event_obj, "cancelable", JS_FALSE, 0);
    JS_DefinePropertyValueStr(ctx, event_obj, "defaultPrevented", JS_FALSE, 0);
    JS_DefinePropertyValueStr(ctx, event_obj, "target", JS_NULL, 0);
    JS_DefinePropertyValueStr(ctx, event_obj, "currentTarget", JS_NULL, 0);
    JS_DefinePropertyValueStr(ctx, event_obj, "message", JS_NewString(ctx, msg.data.c_str()), 0);
    JS_DefinePropertyValueStr(ctx, event_obj, "filename", JS_NewString(ctx, msg.filename.c_str()), 0);
    JS_DefinePropertyValueStr(ctx, event_obj, "lineno", JS_NewInt32(ctx, msg.lineno), 0);
    JS_DefinePropertyValueStr(ctx, event_obj, "colno", JS_NewInt32(ctx, 0), 0);

    JSValue error_obj = JS_NewObject(ctx);
    JS_DefinePropertyValueStr(ctx, error_obj, "name", JS_NewString(ctx, msg.name.c_str()), 0);
    JS_DefinePropertyValueStr(ctx, error_obj, "message", JS_NewString(ctx, msg.data.c_str()), 0);
    JS_DefinePropertyValueStr(ctx, event_obj, "error", error_obj, 0);
    return event_obj;
}

static void dispatch_worker_error(JSContext* ctx, JSValue error_target, const WorkerMessage& msg) {
    if (!JS_IsObject(error_target)) {
        return;
    }

    JSValue onerror = JS_GetPropertyStr(ctx, error_target, "onerror");
    if (JS_IsFunction(ctx, onerror)) {
        JSValue evt = make_error_event(ctx, msg);
        JSValue result = JS_Call(ctx, onerror, error_target, 1, &evt);
        if (JS_IsException(result)) {
            JS_FreeValue(ctx, JS_GetException(ctx));
        }
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, evt);
    }
    JS_FreeValue(ctx, onerror);
}

static JSValue parse_message_data(JSContext* ctx, const std::string& json_data) {
    JSValue parsed = JS_ParseJSON(ctx, json_data.c_str(), json_data.size(), "<message>");
    if (JS_IsException(parsed)) {
        JS_FreeValue(ctx, JS_GetException(ctx));
        return JS_NewString(ctx, json_data.c_str());
    }
    return parsed;
}

// Standard event constructor
static JSValue message_event_constructor(JSContext* ctx, JSValueConst /*new_target*/,
                                        int argc, JSValueConst* argv) {
    std::string type;
    if (argc > 0 && !JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
        const char* type_cstr = JS_ToCString(ctx, argv[0]);
        if (type_cstr) {
            type.assign(type_cstr);
            JS_FreeCString(ctx, type_cstr);
        }
    }
    if (type.empty()) {
        type = "message";
    }

    JSValue event_obj = JS_NewObject(ctx);
    JS_DefinePropertyValueStr(ctx, event_obj, "type", JS_NewString(ctx, type.c_str()), 0);
    JS_DefinePropertyValueStr(ctx, event_obj, "data", JS_NULL, 0);
    JS_DefinePropertyValueStr(ctx, event_obj, "origin", JS_NewString(ctx, ""), 0);
    JS_DefinePropertyValueStr(ctx, event_obj, "source", JS_NULL, 0);
    JS_DefinePropertyValueStr(ctx, event_obj, "ports", JS_NewArray(ctx), 0);
    JS_DefinePropertyValueStr(ctx, event_obj, "bubbles", JS_FALSE, 0);
    JS_DefinePropertyValueStr(ctx, event_obj, "cancelable", JS_FALSE, 0);

    if (argc > 1 && JS_IsObject(argv[1])) {
        JSValue data = JS_GetPropertyStr(ctx, argv[1], "data");
        if (!JS_IsUndefined(data) && !JS_IsNull(data)) {
            JS_DefinePropertyValueStr(ctx, event_obj, "data", JS_DupValue(ctx, data), 0);
        }
        JS_FreeValue(ctx, data);

        JSValue origin = JS_GetPropertyStr(ctx, argv[1], "origin");
        if (!JS_IsUndefined(origin) && !JS_IsNull(origin)) {
            const char* origin_cstr = JS_ToCString(ctx, origin);
            if (origin_cstr) {
                JS_DefinePropertyValueStr(ctx, event_obj, "origin", JS_NewString(ctx, origin_cstr), 0);
                JS_FreeCString(ctx, origin_cstr);
            }
        }
        JS_FreeValue(ctx, origin);

        JSValue source = JS_GetPropertyStr(ctx, argv[1], "source");
        if (!JS_IsUndefined(source) && !JS_IsNull(source)) {
            JS_DefinePropertyValueStr(ctx, event_obj, "source", JS_DupValue(ctx, source), 0);
        }
        JS_FreeValue(ctx, source);

        JSValue ports = JS_GetPropertyStr(ctx, argv[1], "ports");
        if (!JS_IsUndefined(ports) && !JS_IsNull(ports) && JS_IsArray(ctx, ports)) {
            JS_DefinePropertyValueStr(ctx, event_obj, "ports", JS_DupValue(ctx, ports), 0);
        } else {
            JS_DefinePropertyValueStr(ctx, event_obj, "ports", JS_NewArray(ctx), 0);
        }
        JS_FreeValue(ctx, ports);

        JSValue bubbles = JS_GetPropertyStr(ctx, argv[1], "bubbles");
        if (!JS_IsUndefined(bubbles)) {
            JS_DefinePropertyValueStr(ctx, event_obj, "bubbles",
                                      JS_NewBool(ctx, JS_ToBool(ctx, bubbles)), 0);
        }
        JS_FreeValue(ctx, bubbles);

        JSValue cancelable = JS_GetPropertyStr(ctx, argv[1], "cancelable");
        if (!JS_IsUndefined(cancelable)) {
            JS_DefinePropertyValueStr(ctx, event_obj, "cancelable",
                                      JS_NewBool(ctx, JS_ToBool(ctx, cancelable)), 0);
        }
        JS_FreeValue(ctx, cancelable);
    }

    return event_obj;
}

// =========================================================================
// Worker finalizer
// =========================================================================

static void worker_finalizer(JSRuntime* /*rt*/, JSValue val) {
    WorkerThread* worker = static_cast<WorkerThread*>(JS_GetOpaque(val, worker_class_id));
    if (worker) {
        worker->terminate();
        // Remove from global state map to release JS references
        std::lock_guard<std::mutex> lock(g_worker_states_mutex);
        for (auto& [ctx, state] : g_worker_states) {
            auto it = state->workers.find(worker);
            if (it != state->workers.end()) {
                if (!JS_IsUndefined(it->second.js_object)) {
                    JS_FreeValue(ctx, it->second.js_object);
                }
                state->workers.erase(it);
                break;
            }
        }
    }
}

// =========================================================================
// Worker.postMessage(data)
// =========================================================================

static JSValue worker_post_message(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    WorkerThread* worker = static_cast<WorkerThread*>(JS_GetOpaque(this_val, worker_class_id));
    if (!worker) {
        return JS_ThrowTypeError(ctx, "Invalid worker object");
    }
    if (worker->is_terminated()) {
        return JS_UNDEFINED;
    }

    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "postMessage requires at least 1 argument");
    }

    std::string json_data;
    if (!parse_json_payload(ctx, argv[0], json_data)) {
        return JS_EXCEPTION;
    }

    std::string ports_json;
    if (!parse_ports_option(ctx, argc > 1 ? argv[1] : JS_UNDEFINED, ports_json)) {
        return JS_EXCEPTION;
    }

    // Post message to worker
    worker->post_message_to_worker(json_data, ports_json);

    return JS_UNDEFINED;
}

// =========================================================================
// Worker.terminate()
// =========================================================================

static JSValue worker_terminate(JSContext* ctx, JSValueConst this_val, int /*argc*/, JSValueConst* /*argv*/) {
    WorkerThread* worker = static_cast<WorkerThread*>(JS_GetOpaque(this_val, worker_class_id));
    if (!worker) {
        return JS_ThrowTypeError(ctx, "Invalid worker object");
    }
    if (worker->is_terminated()) {
        return JS_UNDEFINED;
    }

    worker->terminate();
    return JS_UNDEFINED;
}

// =========================================================================
// Worker constructor
// =========================================================================

static JSValue worker_constructor(JSContext* ctx, JSValueConst /*new_target*/, int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "Worker requires a script URL");
    }

    const char* script_url = JS_ToCString(ctx, argv[0]);
    if (!script_url) {
        return JS_EXCEPTION;
    }

    std::string url(script_url);
    JS_FreeCString(ctx, script_url);

    // Create a new WorkerThread
    auto worker = std::make_shared<WorkerThread>(url);

    // Get module fetcher from context (placeholder for now)
    auto module_fetcher = [](const std::string& /*url*/) -> std::string {
        return "";
    };

    worker->start(module_fetcher);

    // Create the JS object for the worker
    JSValue worker_obj = JS_NewObjectClass(ctx, worker_class_id);
    if (JS_IsException(worker_obj)) {
        return JS_EXCEPTION;
    }

    WorkerThread* worker_ptr = worker.get();
    JS_SetOpaque(worker_obj, worker_ptr);

    // Store the worker in the context state
    WorkerState* state = get_worker_state(ctx);
    state->workers[worker_ptr] = WorkerStateEntry{worker, JS_DupValue(ctx, worker_obj)};

    // Add methods: postMessage, terminate
    JS_DefinePropertyValueStr(ctx, worker_obj, "postMessage",
                             JS_NewCFunction(ctx, worker_post_message, "postMessage", 1),
                             JS_PROP_CONFIGURABLE);

    JS_DefinePropertyValueStr(ctx, worker_obj, "terminate",
                             JS_NewCFunction(ctx, worker_terminate, "terminate", 0),
                             JS_PROP_CONFIGURABLE);

    // Add onmessage handler (initially null)
    JS_DefinePropertyValueStr(ctx, worker_obj, "onmessage", JS_NULL, JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE);

    // Add onerror handler (initially null)
    JS_DefinePropertyValueStr(ctx, worker_obj, "onerror", JS_NULL, JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE);

    return worker_obj;
}

// =========================================================================
// self.postMessage(data) in worker context
// =========================================================================

static JSValue worker_self_post_message(JSContext* ctx, JSValueConst /*this_val*/, int argc, JSValueConst* argv) {
    WorkerThread* worker = static_cast<WorkerThread*>(JS_GetContextOpaque(ctx));
    if (!worker || worker->is_terminated()) {
        return JS_UNDEFINED;
    }

    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "postMessage requires at least 1 argument");
    }

    std::string json_data;
    if (!parse_json_payload(ctx, argv[0], json_data)) {
        return JS_EXCEPTION;
    }

    std::string ports_json;
    if (!parse_ports_option(ctx, argc > 1 ? argv[1] : JS_UNDEFINED, ports_json)) {
        return JS_EXCEPTION;
    }

    worker->post_message_to_worker(json_data, ports_json);

    return JS_UNDEFINED;
}

static JSValue worker_self_close(JSContext* ctx, JSValueConst /*this_val*/, int /*argc*/, JSValueConst* /*argv*/) {
    WorkerThread* worker = static_cast<WorkerThread*>(JS_GetContextOpaque(ctx));
    if (worker) {
        worker->terminate();
    }
    return JS_UNDEFINED;
}

}  // anonymous namespace

// =========================================================================
// WorkerThread implementation
// =========================================================================

WorkerThread::WorkerThread(const std::string& script_url)
    : script_url_(script_url), worker_rt_(nullptr), worker_ctx_(nullptr) {
}

WorkerThread::~WorkerThread() {
    terminate();
    if (worker_ctx_) {
        JS_FreeContext(worker_ctx_);
        worker_ctx_ = nullptr;
    }
    if (worker_rt_) {
        JS_FreeRuntime(worker_rt_);
        worker_rt_ = nullptr;
    }
}

void WorkerThread::start(std::function<std::string(const std::string&)> module_fetcher) {
    module_fetcher_ = module_fetcher;
    worker_thread_ = std::thread(&WorkerThread::worker_main, this);
}

void WorkerThread::post_message_to_worker(const std::string& json_data, const std::string& ports_json) {
    if (should_terminate_) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (should_terminate_) {
            return;
        }
        main_to_worker_.push(WorkerMessage{json_data, ports_json});
    }
    queue_cv_.notify_one();
}

bool WorkerThread::try_recv_message_from_worker(WorkerMessage& message) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (worker_to_main_.empty()) {
        return false;
    }
    message = worker_to_main_.front();
    worker_to_main_.pop();
    return true;
}

void WorkerThread::terminate() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        should_terminate_ = true;
        while (!main_to_worker_.empty()) {
            main_to_worker_.pop();
        }
        while (!worker_to_main_.empty()) {
            worker_to_main_.pop();
        }
    }
    queue_cv_.notify_one();
    queue_cv_.notify_all();

    if (worker_thread_.get_id() == std::this_thread::get_id()) {
        return;
    }

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    finished_ = true;
}

bool WorkerThread::is_running() const {
    return !finished_ && !should_terminate_;
}

bool WorkerThread::is_terminated() const {
    return should_terminate_;
}

void WorkerThread::post_error_to_main(const std::string& message,
                                     const std::string& filename,
                                     int32_t lineno,
                                     const std::string& name) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (should_terminate_) {
            return;
        }
        WorkerMessage error_message;
        error_message.kind = WorkerMessageKind::kError;
        error_message.data = message;
        error_message.filename = filename;
        error_message.name = name;
        error_message.lineno = lineno;
        error_message.ports = "[]";
        worker_to_main_.push(std::move(error_message));
    }
    queue_cv_.notify_one();
}

void WorkerThread::worker_main() {
    // Create a new JS runtime and context for this worker
    worker_rt_ = JS_NewRuntime();
    if (!worker_rt_) {
        finished_ = true;
        should_terminate_ = true;
        return;
    }

    worker_ctx_ = JS_NewContext(worker_rt_);
    if (!worker_ctx_) {
        JS_FreeRuntime(worker_rt_);
        worker_rt_ = nullptr;
        finished_ = true;
        should_terminate_ = true;
        return;
    }

    // Set context opaque to this worker thread for self.postMessage
    JS_SetContextOpaque(worker_ctx_, this);

    // Set up console.log for the worker
    JSValue console_obj = JS_NewObject(worker_ctx_);
    JSValue log_func = JS_NewCFunction(worker_ctx_,
        [](JSContext* ctx, JSValueConst /*this_val*/, int argc, JSValueConst* argv) -> JSValue {
            for (int i = 0; i < argc; i++) {
                const char* str = JS_ToCString(ctx, argv[i]);
                if (str) {
                    JS_FreeCString(ctx, str);
                }
            }
            return JS_UNDEFINED;
        }, "log", 0);
    JS_DefinePropertyValueStr(worker_ctx_, console_obj, "log", log_func, 0);
    JSValue global_for_console = JS_GetGlobalObject(worker_ctx_);
    JS_DefinePropertyValueStr(worker_ctx_, global_for_console, "console", console_obj, 0);
    JS_FreeValue(worker_ctx_, global_for_console);

    // Set up self object with postMessage and onmessage
    JSValue self_obj = JS_GetGlobalObject(worker_ctx_);
    JS_DefinePropertyValueStr(worker_ctx_, self_obj, "postMessage",
                             JS_NewCFunction(worker_ctx_, worker_self_post_message, "postMessage", 1),
                             JS_PROP_CONFIGURABLE);
    JS_DefinePropertyValueStr(worker_ctx_, self_obj, "close",
                              JS_NewCFunction(worker_ctx_, worker_self_close, "close", 0),
                              JS_PROP_CONFIGURABLE);
    JS_DefinePropertyValueStr(worker_ctx_, self_obj, "onerror", JS_NULL, JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE);
    JS_DefinePropertyValueStr(worker_ctx_, self_obj, "onmessage", JS_NULL, JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE);
    JS_DefinePropertyValueStr(worker_ctx_, self_obj, "self", JS_DupValue(worker_ctx_, self_obj), 0);
    JS_DefinePropertyValueStr(worker_ctx_, self_obj, "globalThis", JS_DupValue(worker_ctx_, self_obj), 0);

    // Fetch and evaluate the worker script
    std::string script_source;
    if (module_fetcher_) {
        script_source = module_fetcher_(script_url_);
    }

    if (!script_source.empty()) {
        JSValue result = JS_Eval(worker_ctx_, script_source.c_str(), script_source.size(),
                                script_url_.c_str(), JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(result)) {
            WorkerMessage msg = worker_error_from_exception(worker_ctx_, "Error evaluating worker script");
            if (msg.filename.empty()) {
                msg.filename = script_url_;
            }
            msg.ports = "[]";
            post_error_to_main(msg.data, msg.filename, msg.lineno, msg.name);
            dispatch_worker_error(worker_ctx_, self_obj, msg);
            should_terminate_ = true;
        }
        JS_FreeValue(worker_ctx_, result);
    }
    if (should_terminate_) {
        JS_FreeValue(worker_ctx_, self_obj);
        JS_FreeContext(worker_ctx_);
        worker_ctx_ = nullptr;
        JS_FreeRuntime(worker_rt_);
        worker_rt_ = nullptr;
        finished_ = true;
        return;
    }

    // Message loop
    while (!should_terminate_) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        queue_cv_.wait_for(lock, std::chrono::milliseconds(100),
                          [this]() { return !main_to_worker_.empty() || should_terminate_; });

        if (should_terminate_) {
            break;
        }

        if (!main_to_worker_.empty()) {
            WorkerMessage msg = main_to_worker_.front();
            main_to_worker_.pop();
            lock.unlock();

            JSValue data = parse_message_data(worker_ctx_, msg.data);
            JSValue event_obj = make_message_event(worker_ctx_, "message", data, "", self_obj, msg.ports, false, false);
            JSValue onmessage = JS_GetPropertyStr(worker_ctx_, self_obj, "onmessage");
            if (JS_IsFunction(worker_ctx_, onmessage)) {
                JSValue call_result = JS_Call(worker_ctx_, onmessage, self_obj, 1, &event_obj);
                if (JS_IsException(call_result)) {
                    WorkerMessage error_msg = worker_error_from_exception(worker_ctx_, "Uncaught exception in worker");
                    if (error_msg.filename.empty()) {
                        error_msg.filename = script_url_;
                    }
                    error_msg.ports = "[]";
                    post_error_to_main(error_msg.data, error_msg.filename, error_msg.lineno, error_msg.name);
                    dispatch_worker_error(worker_ctx_, self_obj, error_msg);
                }
                JS_FreeValue(worker_ctx_, call_result);
            }
            JS_FreeValue(worker_ctx_, onmessage);
            JS_FreeValue(worker_ctx_, event_obj);
            JS_FreeValue(worker_ctx_, data);
        }
    }

    std::lock_guard<std::mutex> lock(queue_mutex_);
    while (!main_to_worker_.empty()) {
        main_to_worker_.pop();
    }
    while (!worker_to_main_.empty()) {
        worker_to_main_.pop();
    }

    JS_FreeValue(worker_ctx_, self_obj);
    JS_FreeContext(worker_ctx_);
    worker_ctx_ = nullptr;
    JS_FreeRuntime(worker_rt_);
    worker_rt_ = nullptr;

    finished_ = true;
}

// =========================================================================
// Public API
// =========================================================================

void install_worker_bindings(JSContext* ctx) {
    JSValue global = JS_GetGlobalObject(ctx);

    // Create Worker class
    JSClassDef worker_class = {
        "Worker",
        .finalizer = worker_finalizer,
    };

    JS_NewClassID(&worker_class_id);
    JS_NewClass(JS_GetRuntime(ctx), worker_class_id, &worker_class);

    JS_DefinePropertyValueStr(ctx, global, "MessageEvent",
                             JS_NewCFunction2(ctx, message_event_constructor,
                                              "MessageEvent", 2, JS_CFUNC_constructor, 0),
                             JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE);

    // Create Worker constructor
    JSValue worker_ctor = JS_NewCFunction2(ctx, worker_constructor, "Worker", 1,
                                          JS_CFUNC_constructor, 0);

    JS_DefinePropertyValueStr(ctx, global, "Worker", worker_ctor, 0);
    JS_FreeValue(ctx, global);
}

void process_worker_messages(JSContext* ctx) {
    WorkerState* state = get_worker_state(ctx);
    std::vector<std::pair<std::shared_ptr<WorkerThread>, JSValue>> active_workers;
    {
        std::lock_guard<std::mutex> lock(g_worker_states_mutex);
        if (!state || state->workers.empty()) {
            return;
        }

        active_workers.reserve(state->workers.size());
        for (auto& state_entry : state->workers) {
            auto& entry = state_entry.second;
            if (!JS_IsUndefined(entry.js_object)) {
                active_workers.emplace_back(entry.thread, JS_DupValue(ctx, entry.js_object));
            }
        }
    }

    for (auto& pair : active_workers) {
        auto worker = pair.first;
        JSValue worker_obj = pair.second;

        WorkerMessage msg;
        while (worker->try_recv_message_from_worker(msg)) {
            if (msg.kind == WorkerMessageKind::kError) {
                JSValue onerror = JS_GetPropertyStr(ctx, worker_obj, "onerror");
                if (JS_IsFunction(ctx, onerror)) {
                    JSValue event_obj = make_error_event(ctx, msg);
                    JSValue call_result = JS_Call(ctx, onerror, worker_obj, 1, &event_obj);
                    if (JS_IsException(call_result)) {
                        JS_FreeValue(ctx, JS_GetException(ctx));
                    }
                    JS_FreeValue(ctx, call_result);
                    JS_FreeValue(ctx, event_obj);
                }
                JS_FreeValue(ctx, onerror);
                continue;
            }

            JSValue data = parse_message_data(ctx, msg.data);
            JSValue event_obj = make_message_event(ctx, "message", data, msg.filename, worker_obj,
                                                  msg.ports, false, false);
            JSValue onmessage = JS_GetPropertyStr(ctx, worker_obj, "onmessage");
            if (JS_IsFunction(ctx, onmessage)) {
                JSValue call_result = JS_Call(ctx, onmessage, worker_obj, 1, &event_obj);
                if (JS_IsException(call_result)) {
                    JS_FreeValue(ctx, JS_GetException(ctx));
                }
                JS_FreeValue(ctx, call_result);
            }
            JS_FreeValue(ctx, onmessage);
            JS_FreeValue(ctx, event_obj);
            JS_FreeValue(ctx, data);
        }

        // Clean up finished workers
        if (worker->is_finished()) {
            std::lock_guard<std::mutex> lock(g_worker_states_mutex);
            auto it = state->workers.find(worker.get());
            if (it != state->workers.end()) {
                JS_FreeValue(ctx, it->second.js_object);
                state->workers.erase(it);
            }
        }
        JS_FreeValue(ctx, worker_obj);
    }
}

}  // namespace clever::js
