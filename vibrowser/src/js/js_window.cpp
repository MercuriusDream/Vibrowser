#include <clever/js/js_window.h>

extern "C" {
#include <quickjs.h>
}

#include <chrono>
#include <cmath>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace clever::js {

namespace {

// =========================================================================
// Base64 encode/decode tables for atob/btoa
// =========================================================================

static const char base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string base64_encode(const std::string& input) {
    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);

    unsigned int val = 0;
    int valb = -6;
    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            output.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) {
        output.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    while (output.size() % 4) {
        output.push_back('=');
    }
    return output;
}

static int base64_decode_char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static bool base64_decode(const std::string& input, std::string& output) {
    output.clear();
    unsigned int val = 0;
    int valb = -8;
    for (char c : input) {
        if (c == '=') break;
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
        int d = base64_decode_char(c);
        if (d < 0) return false; // invalid character
        val = (val << 6) + d;
        valb += 6;
        if (valb >= 0) {
            output.push_back(static_cast<char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return true;
}

// =========================================================================
// window.btoa(str) -- Base64 encode
// =========================================================================

static JSValue js_window_btoa(JSContext* ctx, JSValueConst /*this_val*/,
                               int argc, JSValueConst* argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "btoa requires 1 argument");
    const char* str = JS_ToCString(ctx, argv[0]);
    if (!str) return JS_EXCEPTION;

    // Check for characters outside latin1 range
    for (const char* p = str; *p; ++p) {
        if (static_cast<unsigned char>(*p) > 255) {
            JS_FreeCString(ctx, str);
            return JS_ThrowRangeError(ctx,
                "btoa: string contains characters outside latin1 range");
        }
    }

    std::string encoded = base64_encode(str);
    JS_FreeCString(ctx, str);
    return JS_NewString(ctx, encoded.c_str());
}

// =========================================================================
// window.atob(str) -- Base64 decode
// =========================================================================

static JSValue js_window_atob(JSContext* ctx, JSValueConst /*this_val*/,
                               int argc, JSValueConst* argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "atob requires 1 argument");
    const char* str = JS_ToCString(ctx, argv[0]);
    if (!str) return JS_EXCEPTION;

    std::string decoded;
    if (!base64_decode(str, decoded)) {
        JS_FreeCString(ctx, str);
        return JS_ThrowSyntaxError(ctx,
            "atob: invalid base64 string");
    }
    JS_FreeCString(ctx, str);
    return JS_NewString(ctx, decoded.c_str());
}

// =========================================================================
// performance.now() -- high-resolution timestamp
// =========================================================================

// Page load start time, set once per install_window_bindings call.
static std::chrono::steady_clock::time_point page_start_time;

// Wall-clock epoch time origin (milliseconds since Unix epoch), for performance.timeOrigin
static double page_time_origin_ms = 0.0;

static JSValue js_performance_now(JSContext* ctx, JSValueConst /*this_val*/,
                                   int /*argc*/, JSValueConst* /*argv*/) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        now - page_start_time);
    double ms = static_cast<double>(elapsed.count()) / 1000.0;
    return JS_NewFloat64(ctx, ms);
}

// performance.getEntries() — returns empty array
static JSValue js_performance_get_entries(JSContext* ctx, JSValueConst /*this_val*/,
                                           int /*argc*/, JSValueConst* /*argv*/) {
    return JS_NewArray(ctx);
}

// performance.getEntriesByType(type) — returns empty array
static JSValue js_performance_get_entries_by_type(JSContext* ctx, JSValueConst /*this_val*/,
                                                    int /*argc*/, JSValueConst* /*argv*/) {
    return JS_NewArray(ctx);
}

// performance.getEntriesByName(name) — returns empty array
static JSValue js_performance_get_entries_by_name(JSContext* ctx, JSValueConst /*this_val*/,
                                                    int /*argc*/, JSValueConst* /*argv*/) {
    return JS_NewArray(ctx);
}

// performance.mark(name) — no-op, returns undefined
static JSValue js_performance_mark(JSContext* /*ctx*/, JSValueConst /*this_val*/,
                                    int /*argc*/, JSValueConst* /*argv*/) {
    return JS_UNDEFINED;
}

// performance.measure(name, startMark, endMark) — no-op, returns undefined
static JSValue js_performance_measure(JSContext* /*ctx*/, JSValueConst /*this_val*/,
                                       int /*argc*/, JSValueConst* /*argv*/) {
    return JS_UNDEFINED;
}

// performance.clearMarks() — no-op
static JSValue js_performance_clear_marks(JSContext* /*ctx*/, JSValueConst /*this_val*/,
                                           int /*argc*/, JSValueConst* /*argv*/) {
    return JS_UNDEFINED;
}

// performance.clearMeasures() — no-op
static JSValue js_performance_clear_measures(JSContext* /*ctx*/, JSValueConst /*this_val*/,
                                              int /*argc*/, JSValueConst* /*argv*/) {
    return JS_UNDEFINED;
}

// performance.clearResourceTimings() — no-op
static JSValue js_performance_clear_resource_timings(JSContext* /*ctx*/, JSValueConst /*this_val*/,
                                                      int /*argc*/, JSValueConst* /*argv*/) {
    return JS_UNDEFINED;
}

// performance.toJSON() — returns object with timeOrigin and entries
static JSValue js_performance_to_json(JSContext* ctx, JSValueConst /*this_val*/,
                                       int /*argc*/, JSValueConst* /*argv*/) {
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "timeOrigin", JS_NewFloat64(ctx, page_time_origin_ms));
    return obj;
}

// =========================================================================
// requestAnimationFrame(callback) / cancelAnimationFrame(id)
// We reuse the timer infrastructure: treat rAF like setTimeout(fn, 16).
// Since we're synchronous, we store the callback and execute it immediately.
// =========================================================================

struct RAFEntry {
    int id = 0;
    JSValue callback = JS_UNDEFINED;
    bool cancelled = false;
};

struct RAFState {
    std::vector<RAFEntry> entries;
    int next_id = 1;
};

static RAFState* get_raf_state(JSContext* ctx) {
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue val = JS_GetPropertyStr(ctx, global, "__raf_state_ptr");
    RAFState* state = nullptr;
    if (JS_IsNumber(val)) {
        int64_t ptr_val = 0;
        JS_ToInt64(ctx, &ptr_val, val);
        state = reinterpret_cast<RAFState*>(static_cast<uintptr_t>(ptr_val));
    }
    JS_FreeValue(ctx, val);
    JS_FreeValue(ctx, global);
    return state;
}

// Recursion depth guard for requestAnimationFrame — real websites schedule
// the next frame inside the callback, which recurses infinitely in our
// synchronous engine.
static thread_local int s_raf_depth = 0;
static constexpr int kMaxRAFDepth = 4;

static JSValue js_request_animation_frame(JSContext* ctx, JSValueConst /*this_val*/,
                                           int argc, JSValueConst* argv) {
    if (argc < 1 || !JS_IsFunction(ctx, argv[0]))
        return JS_NewInt32(ctx, 0);

    auto* state = get_raf_state(ctx);
    if (!state) return JS_NewInt32(ctx, 0);

    int id = state->next_id++;

    // Limit recursion depth — animation loops call rAF from inside rAF
    if (s_raf_depth >= kMaxRAFDepth)
        return JS_NewInt32(ctx, id);

    ++s_raf_depth;

    // In our synchronous engine, execute the callback immediately
    // with a DOMHighResTimeStamp argument (performance.now())
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        now - page_start_time);
    double ms = static_cast<double>(elapsed.count()) / 1000.0;
    JSValue timestamp = JS_NewFloat64(ctx, ms);
    JSValue ret = JS_Call(ctx, argv[0], JS_UNDEFINED, 1, &timestamp);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, timestamp);

    --s_raf_depth;

    return JS_NewInt32(ctx, id);
}

static JSValue js_cancel_animation_frame(JSContext* ctx, JSValueConst /*this_val*/,
                                          int argc, JSValueConst* argv) {
    (void)ctx;
    // Since we execute immediately, cancel is essentially a no-op,
    // but we maintain the API contract.
    auto* state = get_raf_state(ctx);
    if (!state || argc < 1) return JS_UNDEFINED;

    int id = 0;
    JS_ToInt32(ctx, &id, argv[0]);

    for (auto& entry : state->entries) {
        if (entry.id == id && !entry.cancelled) {
            entry.cancelled = true;
            JS_FreeValue(ctx, entry.callback);
            entry.callback = JS_UNDEFINED;
            break;
        }
    }

    return JS_UNDEFINED;
}

// =========================================================================
// queueMicrotask(fn) -- execute fn immediately (synchronous engine)
// =========================================================================

static JSValue js_queue_microtask(JSContext* ctx, JSValueConst /*this_val*/,
                                   int argc, JSValueConst* argv) {
    if (argc < 1 || !JS_IsFunction(ctx, argv[0]))
        return JS_ThrowTypeError(ctx, "queueMicrotask requires a function argument");

    // Execute immediately in our synchronous engine
    JSValue ret = JS_Call(ctx, argv[0], JS_UNDEFINED, 0, nullptr);
    JS_FreeValue(ctx, ret);
    return JS_UNDEFINED;
}

// =========================================================================
// window.alert(msg) -- no-op (logs to console if available)
// =========================================================================

static JSValue js_window_alert(JSContext* ctx, JSValueConst /*this_val*/,
                                int argc, JSValueConst* argv) {
    (void)ctx;
    (void)argc;
    (void)argv;
    // No-op: real browsers show a dialog, we just silently consume the call.
    return JS_UNDEFINED;
}

// =========================================================================
// window.confirm(msg) -- stub, always returns true
// =========================================================================

static JSValue js_window_confirm(JSContext* ctx, JSValueConst /*this_val*/,
                                  int /*argc*/, JSValueConst* /*argv*/) {
    (void)ctx;
    return JS_TRUE;
}

// =========================================================================
// window.prompt(msg, default) -- stub, always returns ""
// =========================================================================

static JSValue js_window_prompt(JSContext* ctx, JSValueConst /*this_val*/,
                                 int /*argc*/, JSValueConst* /*argv*/) {
    return JS_NewString(ctx, "");
}

// =========================================================================
// window.localStorage — in-memory Storage backed by static std::map
// =========================================================================

// In-memory backing store. Persists within the process but not across
// sessions, which matches the task requirements.
static std::map<std::string, std::string>& local_storage_map() {
    static std::map<std::string, std::string> store;
    return store;
}

// localStorage.getItem(key)  ->  string | null
static JSValue js_storage_get_item(JSContext* ctx, JSValueConst /*this_val*/,
                                    int argc, JSValueConst* argv) {
    if (argc < 1) return JS_NULL;
    const char* key = JS_ToCString(ctx, argv[0]);
    if (!key) return JS_EXCEPTION;
    auto& store = local_storage_map();
    auto it = store.find(key);
    JS_FreeCString(ctx, key);
    if (it == store.end()) return JS_NULL;
    return JS_NewString(ctx, it->second.c_str());
}

// localStorage.setItem(key, value)
static JSValue js_storage_set_item(JSContext* ctx, JSValueConst /*this_val*/,
                                    int argc, JSValueConst* argv) {
    if (argc < 2)
        return JS_ThrowTypeError(ctx, "setItem requires 2 arguments");
    const char* key = JS_ToCString(ctx, argv[0]);
    if (!key) return JS_EXCEPTION;
    const char* value = JS_ToCString(ctx, argv[1]);
    if (!value) {
        JS_FreeCString(ctx, key);
        return JS_EXCEPTION;
    }
    local_storage_map()[key] = value;
    JS_FreeCString(ctx, key);
    JS_FreeCString(ctx, value);
    return JS_UNDEFINED;
}

// localStorage.removeItem(key)
static JSValue js_storage_remove_item(JSContext* ctx, JSValueConst /*this_val*/,
                                       int argc, JSValueConst* argv) {
    if (argc < 1) return JS_UNDEFINED;
    const char* key = JS_ToCString(ctx, argv[0]);
    if (!key) return JS_EXCEPTION;
    local_storage_map().erase(key);
    JS_FreeCString(ctx, key);
    return JS_UNDEFINED;
}

// localStorage.clear()
static JSValue js_storage_clear(JSContext* ctx, JSValueConst /*this_val*/,
                                 int /*argc*/, JSValueConst* /*argv*/) {
    (void)ctx;
    local_storage_map().clear();
    return JS_UNDEFINED;
}

// localStorage.key(index)  ->  string | null
static JSValue js_storage_key(JSContext* ctx, JSValueConst /*this_val*/,
                               int argc, JSValueConst* argv) {
    if (argc < 1) return JS_NULL;
    int32_t index = 0;
    if (JS_ToInt32(ctx, &index, argv[0]) < 0) return JS_EXCEPTION;
    auto& store = local_storage_map();
    if (index < 0 || static_cast<size_t>(index) >= store.size())
        return JS_NULL;
    auto it = store.begin();
    std::advance(it, index);
    return JS_NewString(ctx, it->first.c_str());
}

// Internal getter for localStorage.length (called from JS defineProperty)
static JSValue js_storage_get_length(JSContext* ctx, JSValueConst /*this_val*/,
                                      int /*argc*/, JSValueConst* /*argv*/) {
    return JS_NewInt32(ctx, static_cast<int32_t>(local_storage_map().size()));
}

// Create and return the localStorage object for the given context.
static JSValue create_local_storage(JSContext* ctx) {
    JSValue storage = JS_NewObject(ctx);

    JS_SetPropertyStr(ctx, storage, "getItem",
        JS_NewCFunction(ctx, js_storage_get_item, "getItem", 1));
    JS_SetPropertyStr(ctx, storage, "setItem",
        JS_NewCFunction(ctx, js_storage_set_item, "setItem", 2));
    JS_SetPropertyStr(ctx, storage, "removeItem",
        JS_NewCFunction(ctx, js_storage_remove_item, "removeItem", 1));
    JS_SetPropertyStr(ctx, storage, "clear",
        JS_NewCFunction(ctx, js_storage_clear, "clear", 0));
    JS_SetPropertyStr(ctx, storage, "key",
        JS_NewCFunction(ctx, js_storage_key, "key", 1));

    // Wire up .length as a getter property via JS_DefinePropertyGetSet
    JS_SetPropertyStr(ctx, storage, "__getLength",
        JS_NewCFunction(ctx, js_storage_get_length, "__getLength", 0));

    // We'll define the 'length' getter via a small eval after installing
    // the storage object on the global.
    return storage;
}

// =========================================================================
// URL parsing helpers
// =========================================================================

struct ParsedURL {
    std::string protocol; // e.g. "https:"
    std::string hostname; // e.g. "example.com"
    std::string pathname; // e.g. "/path/to/page"
    std::string href;     // the full URL
    std::string port;     // e.g. "8080" or "" for default
    std::string host;     // hostname:port (or just hostname if default port)
    std::string origin;   // protocol + "//" + host
    std::string search;   // e.g. "?q=1" or ""
    std::string hash;     // e.g. "#section" or ""
};

static ParsedURL parse_url(const std::string& url) {
    ParsedURL result;
    result.href = url;

    // Parse protocol
    auto colon_slash = url.find("://");
    if (colon_slash != std::string::npos) {
        result.protocol = url.substr(0, colon_slash + 1); // "https:"
        std::string rest = url.substr(colon_slash + 3);

        // Parse hostname (up to /, ?, # or end)
        auto path_start = rest.find_first_of("/?#");
        std::string host_part;
        std::string path_and_rest;
        if (path_start != std::string::npos) {
            host_part = rest.substr(0, path_start);
            path_and_rest = rest.substr(path_start);
        } else {
            host_part = rest;
            path_and_rest = "";
        }

        // Extract port from host_part (hostname:port)
        auto colon_pos = host_part.find(':');
        if (colon_pos != std::string::npos) {
            result.hostname = host_part.substr(0, colon_pos);
            result.port = host_part.substr(colon_pos + 1);
        } else {
            result.hostname = host_part;
            result.port = "";
        }

        // host = hostname:port or just hostname
        result.host = result.port.empty() ? result.hostname
                                           : (result.hostname + ":" + result.port);

        // origin = protocol + "//" + host
        result.origin = result.protocol + "//" + result.host;

        // Parse path, search, hash from path_and_rest
        if (!path_and_rest.empty()) {
            // Extract hash first
            auto hash_pos = path_and_rest.find('#');
            if (hash_pos != std::string::npos) {
                result.hash = path_and_rest.substr(hash_pos);
                path_and_rest = path_and_rest.substr(0, hash_pos);
            }

            // Extract search
            auto q_pos = path_and_rest.find('?');
            if (q_pos != std::string::npos) {
                result.search = path_and_rest.substr(q_pos);
                result.pathname = path_and_rest.substr(0, q_pos);
            } else {
                result.pathname = path_and_rest;
            }

            if (result.pathname.empty()) result.pathname = "/";
        } else {
            result.pathname = "/";
        }
    } else {
        result.protocol = "";
        result.hostname = "";
        result.pathname = url;
        result.port = "";
        result.host = "";
        result.origin = "";
        result.search = "";
        result.hash = "";
    }

    return result;
}

// =========================================================================
// window.matchMedia(query) -- stub
// =========================================================================

static JSValue js_window_match_media(JSContext* ctx, JSValueConst /*this_val*/,
                                      int argc, JSValueConst* argv) {
    // Get the media query string (or empty string)
    const char* query = "";
    if (argc >= 1) {
        query = JS_ToCString(ctx, argv[0]);
        if (!query) return JS_EXCEPTION;
    }

    JSValue result = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, result, "matches", JS_FALSE);
    JS_SetPropertyStr(ctx, result, "media", JS_NewString(ctx, query));

    if (argc >= 1) {
        JS_FreeCString(ctx, query);
    }

    // Install stub addEventListener/removeEventListener via eval
    // (easiest way to attach JS functions to a C-created object)
    const char* stub_setup = R"JS(
(function(obj) {
    obj.addEventListener = function() {};
    obj.removeEventListener = function() {};
    obj.addListener = function() {};
    obj.removeListener = function() {};
    obj.onchange = null;
    return obj;
})
)JS";
    JSValue fn = JS_Eval(ctx, stub_setup, std::strlen(stub_setup),
                          "<matchMedia-setup>", JS_EVAL_TYPE_GLOBAL);
    if (!JS_IsException(fn)) {
        JSValue wrapped = JS_Call(ctx, fn, JS_UNDEFINED, 1, &result);
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, fn);
        return wrapped;
    }
    JS_FreeValue(ctx, fn);
    return result;
}

// =========================================================================
// window.getSelection() -- stub
// =========================================================================

static JSValue js_window_get_selection(JSContext* ctx, JSValueConst /*this_val*/,
                                        int /*argc*/, JSValueConst* /*argv*/) {
    const char* sel_setup = R"JS(
(function() {
    return {
        toString: function() { return ''; },
        rangeCount: 0,
        anchorNode: null,
        anchorOffset: 0,
        focusNode: null,
        focusOffset: 0,
        isCollapsed: true,
        type: 'None',
        removeAllRanges: function() {},
        addRange: function(range) {},
        getRangeAt: function(index) {
            if (typeof document !== 'undefined' && typeof document.createRange === 'function') {
                return document.createRange();
            }
            return null;
        },
        collapse: function(node, offset) {},
        collapseToStart: function() {},
        collapseToEnd: function() {},
        selectAllChildren: function(node) {},
        deleteFromDocument: function() {},
        containsNode: function(node, allowPartial) { return false; },
        extend: function(node, offset) {},
        setBaseAndExtent: function(anchorNode, anchorOffset, focusNode, focusOffset) {},
        empty: function() {},
        modify: function(alter, direction, granularity) {}
    };
})()
)JS";
    JSValue ret = JS_Eval(ctx, sel_setup, std::strlen(sel_setup),
                           "<getSelection>", JS_EVAL_TYPE_GLOBAL);
    return ret;
}

// =========================================================================
// window.scrollTo / window.scrollBy / window.scroll
// =========================================================================

static double js_read_window_number(JSContext* ctx, JSValueConst global,
                                    const char* name, double fallback) {
    JSValue value = JS_GetPropertyStr(ctx, global, name);
    double result = fallback;
    if (JS_IsNumber(value)) {
        JS_ToFloat64(ctx, &result, value);
    }
    JS_FreeValue(ctx, value);
    return result;
}

static void js_write_window_scroll_state(JSContext* ctx, JSValueConst global,
                                         double x, double y) {
    if (!std::isfinite(x) || x < 0.0) x = 0.0;
    if (!std::isfinite(y) || y < 0.0) y = 0.0;
    JS_SetPropertyStr(ctx, global, "scrollX", JS_NewFloat64(ctx, x));
    JS_SetPropertyStr(ctx, global, "scrollY", JS_NewFloat64(ctx, y));
    JS_SetPropertyStr(ctx, global, "pageXOffset", JS_NewFloat64(ctx, x));
    JS_SetPropertyStr(ctx, global, "pageYOffset", JS_NewFloat64(ctx, y));
}

static void js_read_scroll_args(JSContext* ctx, int argc, JSValueConst* argv,
                                double current_x, double current_y,
                                bool relative, double* out_x, double* out_y) {
    double x = current_x;
    double y = current_y;

    if (argc > 0 && JS_IsObject(argv[0])) {
        JSValue left = JS_GetPropertyStr(ctx, argv[0], "left");
        JSValue top = JS_GetPropertyStr(ctx, argv[0], "top");
        JSValue x_prop = JS_GetPropertyStr(ctx, argv[0], "x");
        JSValue y_prop = JS_GetPropertyStr(ctx, argv[0], "y");

        double next_x = 0.0;
        double next_y = 0.0;
        if (!relative) {
            next_x = current_x;
            next_y = current_y;
        }
        if (JS_IsNumber(left)) JS_ToFloat64(ctx, &next_x, left);
        if (JS_IsNumber(x_prop)) JS_ToFloat64(ctx, &next_x, x_prop);
        if (JS_IsNumber(top)) JS_ToFloat64(ctx, &next_y, top);
        if (JS_IsNumber(y_prop)) JS_ToFloat64(ctx, &next_y, y_prop);

        JS_FreeValue(ctx, left);
        JS_FreeValue(ctx, top);
        JS_FreeValue(ctx, x_prop);
        JS_FreeValue(ctx, y_prop);

        x = relative ? (current_x + next_x) : next_x;
        y = relative ? (current_y + next_y) : next_y;
    } else {
        if (argc > 0 && JS_IsNumber(argv[0])) {
            JS_ToFloat64(ctx, &x, argv[0]);
            if (relative) x += current_x;
        }
        if (argc > 1 && JS_IsNumber(argv[1])) {
            JS_ToFloat64(ctx, &y, argv[1]);
            if (relative) y += current_y;
        }
    }

    *out_x = x;
    *out_y = y;
}

static JSValue js_window_scroll_to(JSContext* ctx, JSValueConst /*this_val*/,
                                    int argc, JSValueConst* argv) {
    JSValue global = JS_GetGlobalObject(ctx);
    double current_x = js_read_window_number(ctx, global, "scrollX", 0.0);
    double current_y = js_read_window_number(ctx, global, "scrollY", 0.0);
    double next_x = current_x;
    double next_y = current_y;
    js_read_scroll_args(ctx, argc, argv, current_x, current_y, false, &next_x, &next_y);
    js_write_window_scroll_state(ctx, global, next_x, next_y);
    JS_FreeValue(ctx, global);
    return JS_UNDEFINED;
}

static JSValue js_window_scroll_by(JSContext* ctx, JSValueConst /*this_val*/,
                                    int argc, JSValueConst* argv) {
    JSValue global = JS_GetGlobalObject(ctx);
    double current_x = js_read_window_number(ctx, global, "scrollX", 0.0);
    double current_y = js_read_window_number(ctx, global, "scrollY", 0.0);
    double next_x = current_x;
    double next_y = current_y;
    js_read_scroll_args(ctx, argc, argv, current_x, current_y, true, &next_x, &next_y);
    js_write_window_scroll_state(ctx, global, next_x, next_y);
    JS_FreeValue(ctx, global);
    return JS_UNDEFINED;
}

// =========================================================================
// window.open(url) -- no-op, returns null
// =========================================================================

static JSValue js_window_open(JSContext* /*ctx*/, JSValueConst /*this_val*/,
                                int /*argc*/, JSValueConst* /*argv*/) {
    return JS_NULL;
}

// =========================================================================
// window.close() -- no-op
// =========================================================================

static JSValue js_window_close(JSContext* /*ctx*/, JSValueConst /*this_val*/,
                                 int /*argc*/, JSValueConst* /*argv*/) {
    return JS_UNDEFINED;
}

// =========================================================================
// window.dispatchEvent(event) -- no-op, returns true
// =========================================================================

static JSValue js_window_dispatch_event(JSContext* /*ctx*/, JSValueConst /*this_val*/,
                                         int /*argc*/, JSValueConst* /*argv*/) {
    return JS_TRUE;
}

// =========================================================================
// window.removeEventListener -- no-op stub
// (window.addEventListener is in js_dom_bindings.cpp; this is a fallback
//  for when DOM bindings are not installed or for non-DOMContentLoaded events)
// =========================================================================

static JSValue js_window_remove_event_listener(JSContext* /*ctx*/, JSValueConst /*this_val*/,
                                                int /*argc*/, JSValueConst* /*argv*/) {
    return JS_UNDEFINED;
}

// =========================================================================
// Web Workers API — simplified synchronous model
//
// The Worker creates a SEPARATE QuickJS runtime+context but runs on the
// SAME thread (synchronous model). When main calls worker.postMessage(),
// the worker script is evaluated in the worker context and the worker's
// onmessage handler is invoked synchronously. Messages sent by the worker
// via self.postMessage() are collected and dispatched to the main thread's
// onmessage handler after the worker script returns.
// =========================================================================

static JSClassID worker_class_id = 0;

struct WorkerState {
    std::string script_url;   // URL of the worker script
    std::string script_code;  // Inline script (for testing / data: URLs)
    JSRuntime* worker_rt = nullptr;
    JSContext* worker_ctx = nullptr;

    // Message queues (thread-safe, though currently same-thread)
    std::mutex mtx;
    std::vector<std::string> to_worker;   // main -> worker messages
    std::vector<std::string> from_worker; // worker -> main messages

    bool terminated = false;
    bool script_loaded = false;

    // Event handlers (stored as JSValue in the MAIN context)
    JSValue onmessage = JS_UNDEFINED;
    JSValue onerror = JS_UNDEFINED;

    // The main context (needed for dispatching events back)
    JSContext* main_ctx = nullptr;
};

// ---- Helpers ----

static WorkerState* get_worker_state(JSValueConst this_val) {
    return static_cast<WorkerState*>(JS_GetOpaque(this_val, worker_class_id));
}

// ---- Worker context: self.postMessage (worker -> main) ----
// This is installed in the WORKER context's global scope.
// We stash the WorkerState pointer in the worker context's opaque.

static JSValue js_worker_self_post_message(JSContext* ctx, JSValueConst /*this_val*/,
                                            int argc, JSValueConst* argv) {
    auto* state = static_cast<WorkerState*>(JS_GetContextOpaque(ctx));
    if (!state || state->terminated) return JS_UNDEFINED;

    if (argc < 1) return JS_UNDEFINED;

    // Serialize the value to JSON
    JSValue json_str = JS_JSONStringify(ctx, argv[0], JS_UNDEFINED, JS_UNDEFINED);
    if (JS_IsException(json_str)) {
        // Fall back to string conversion
        const char* str = JS_ToCString(ctx, argv[0]);
        if (str) {
            std::lock_guard<std::mutex> lock(state->mtx);
            state->from_worker.push_back(std::string("\"") + str + "\"");
            JS_FreeCString(ctx, str);
        }
        return JS_UNDEFINED;
    }

    const char* str = JS_ToCString(ctx, json_str);
    if (str) {
        std::lock_guard<std::mutex> lock(state->mtx);
        state->from_worker.push_back(str);
        JS_FreeCString(ctx, str);
    }
    JS_FreeValue(ctx, json_str);

    return JS_UNDEFINED;
}

// ---- Worker context: self.close() ----

static JSValue js_worker_self_close(JSContext* ctx, JSValueConst /*this_val*/,
                                     int /*argc*/, JSValueConst* /*argv*/) {
    auto* state = static_cast<WorkerState*>(JS_GetContextOpaque(ctx));
    if (state) {
        state->terminated = true;
    }
    return JS_UNDEFINED;
}

// ---- Worker context: console.log/warn/error/info ----

static JSValue js_worker_console_log(JSContext* ctx, JSValueConst /*this_val*/,
                                      int argc, JSValueConst* argv, int /*magic*/) {
    (void)ctx;
    // Workers have console but we just silently consume output
    // (no access to main engine's console_output_)
    std::string message;
    for (int i = 0; i < argc; i++) {
        if (i > 0) message += ' ';
        const char* str = JS_ToCString(ctx, argv[i]);
        if (str) {
            message += str;
            JS_FreeCString(ctx, str);
        }
    }
    // Silently discard (worker console output)
    return JS_UNDEFINED;
}

// ---- Set up the worker context's global scope ----

static void setup_worker_context(WorkerState* state) {
    if (!state || !state->worker_ctx) return;

    JSContext* ctx = state->worker_ctx;

    // Stash WorkerState in context opaque so callbacks can find it
    JS_SetContextOpaque(ctx, state);

    JSValue global = JS_GetGlobalObject(ctx);

    // self === globalThis
    JS_SetPropertyStr(ctx, global, "self", JS_DupValue(ctx, global));

    // self.postMessage(data)
    JS_SetPropertyStr(ctx, global, "postMessage",
        JS_NewCFunction(ctx, js_worker_self_post_message, "postMessage", 1));

    // self.close()
    JS_SetPropertyStr(ctx, global, "close",
        JS_NewCFunction(ctx, js_worker_self_close, "close", 0));

    // self.onmessage = null (will be set by worker script)
    JS_SetPropertyStr(ctx, global, "onmessage", JS_NULL);

    // console object
    JSValue console = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, console, "log",
        JS_NewCFunctionMagic(ctx, js_worker_console_log, "log", 1,
                             JS_CFUNC_generic_magic, 0));
    JS_SetPropertyStr(ctx, console, "warn",
        JS_NewCFunctionMagic(ctx, js_worker_console_log, "warn", 1,
                             JS_CFUNC_generic_magic, 1));
    JS_SetPropertyStr(ctx, console, "error",
        JS_NewCFunctionMagic(ctx, js_worker_console_log, "error", 1,
                             JS_CFUNC_generic_magic, 2));
    JS_SetPropertyStr(ctx, console, "info",
        JS_NewCFunctionMagic(ctx, js_worker_console_log, "info", 1,
                             JS_CFUNC_generic_magic, 3));
    JS_SetPropertyStr(ctx, global, "console", console);

    JS_FreeValue(ctx, global);
}

// ---- Load and execute the worker script in the worker context ----

static bool load_worker_script(WorkerState* state) {
    if (!state || !state->worker_ctx || state->script_loaded) return state && state->script_loaded;

    std::string code;
    if (!state->script_code.empty()) {
        // Inline script (for testing or data: URLs)
        code = state->script_code;
    } else {
        // URL-based script: in a real implementation we'd fetch the URL.
        // For now, we just mark it as loaded without executing anything,
        // since we can't fetch URLs from here without network access.
        state->script_loaded = true;
        return true;
    }

    JSValue result = JS_Eval(state->worker_ctx, code.c_str(), code.size(),
                              "<worker>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)) {
        JSValue exc = JS_GetException(state->worker_ctx);
        JS_FreeValue(state->worker_ctx, exc);
        JS_FreeValue(state->worker_ctx, result);
        return false;
    }
    JS_FreeValue(state->worker_ctx, result);
    state->script_loaded = true;
    return true;
}

// ---- Dispatch a message to the worker's onmessage handler ----

static void dispatch_to_worker(WorkerState* state, const std::string& json_data) {
    if (!state || !state->worker_ctx || state->terminated) return;

    // Load script on first message if not yet loaded
    if (!state->script_loaded) {
        load_worker_script(state);
    }

    JSContext* ctx = state->worker_ctx;

    // Get the worker's onmessage handler from its global scope
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue handler = JS_GetPropertyStr(ctx, global, "onmessage");

    if (JS_IsFunction(ctx, handler)) {
        // Parse the JSON data to create the message value
        JSValue data = JS_ParseJSON(ctx, json_data.c_str(), json_data.size(), "<message>");
        if (JS_IsException(data)) {
            // If JSON parse fails, pass as string
            data = JS_NewString(ctx, json_data.c_str());
        }

        // Create a MessageEvent-like object: { data: ... }
        JSValue event = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, event, "data", data);
        JS_SetPropertyStr(ctx, event, "type", JS_NewString(ctx, "message"));

        JSValue ret = JS_Call(ctx, handler, global, 1, &event);
        if (JS_IsException(ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, event);
    }

    JS_FreeValue(ctx, handler);
    JS_FreeValue(ctx, global);
}

// ---- Dispatch collected worker messages back to main thread ----

static void dispatch_from_worker(WorkerState* state, JSContext* main_ctx,
                                  JSValueConst worker_obj) {
    if (!state) return;

    // Collect messages from the worker FIRST (worker may have posted
    // messages and then called self.close(), so terminated may be true
    // but there are still pending outbound messages to deliver).
    std::vector<std::string> messages;
    {
        std::lock_guard<std::mutex> lock(state->mtx);
        messages.swap(state->from_worker);
    }

    // Dispatch each message to the main thread's onmessage handler
    if (JS_IsFunction(main_ctx, state->onmessage)) {
        for (const auto& json_msg : messages) {
            JSValue data = JS_ParseJSON(main_ctx, json_msg.c_str(), json_msg.size(), "<worker-msg>");
            if (JS_IsException(data)) {
                data = JS_NewString(main_ctx, json_msg.c_str());
            }

            JSValue event = JS_NewObject(main_ctx);
            JS_SetPropertyStr(main_ctx, event, "data", data);
            JS_SetPropertyStr(main_ctx, event, "type", JS_NewString(main_ctx, "message"));

            JSValue ret = JS_Call(main_ctx, state->onmessage, worker_obj, 1, &event);
            if (JS_IsException(ret)) {
                JSValue exc = JS_GetException(main_ctx);
                JS_FreeValue(main_ctx, exc);
            }
            JS_FreeValue(main_ctx, ret);
            JS_FreeValue(main_ctx, event);
        }
    }
}

// ---- Worker finalizer ----

static void js_worker_finalizer(JSRuntime* /*rt*/, JSValue val) {
    auto* state = static_cast<WorkerState*>(JS_GetOpaque(val, worker_class_id));
    if (state) {
        // Free event handlers in the main context
        if (state->main_ctx) {
            if (!JS_IsUndefined(state->onmessage))
                JS_FreeValue(state->main_ctx, state->onmessage);
            if (!JS_IsUndefined(state->onerror))
                JS_FreeValue(state->main_ctx, state->onerror);
        }
        // Tear down worker runtime
        if (state->worker_ctx) {
            JS_FreeContext(state->worker_ctx);
            state->worker_ctx = nullptr;
        }
        if (state->worker_rt) {
            JS_FreeRuntime(state->worker_rt);
            state->worker_rt = nullptr;
        }
        delete state;
    }
}

// ---- Worker GC mark (so event handler JSValues are not collected) ----

static void js_worker_gc_mark(JSRuntime* rt, JSValueConst val,
                               JS_MarkFunc* mark_func) {
    auto* state = static_cast<WorkerState*>(JS_GetOpaque(val, worker_class_id));
    if (!state) return;
    JS_MarkValue(rt, state->onmessage, mark_func);
    JS_MarkValue(rt, state->onerror, mark_func);
}

static JSClassDef worker_class_def = {
    "Worker",
    js_worker_finalizer,
    js_worker_gc_mark,
    nullptr, nullptr
};

// ---- Worker constructor: new Worker(urlOrScript) ----

static JSValue js_worker_constructor(JSContext* ctx, JSValueConst new_target,
                                      int argc, JSValueConst* argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "Worker requires a URL argument");

    const char* url = JS_ToCString(ctx, argv[0]);
    if (!url) return JS_EXCEPTION;

    JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto)) {
        JS_FreeCString(ctx, url);
        return JS_EXCEPTION;
    }

    JSValue obj = JS_NewObjectProtoClass(ctx, proto, worker_class_id);
    JS_FreeValue(ctx, proto);
    if (JS_IsException(obj)) {
        JS_FreeCString(ctx, url);
        return JS_EXCEPTION;
    }

    auto* state = new WorkerState();
    state->script_url = url;
    state->main_ctx = ctx;
    JS_FreeCString(ctx, url);

    // Check if the URL is a "data:" or "blob:" URL containing inline script
    // For testing, we support a special "__inline:" prefix
    if (state->script_url.starts_with("__inline:")) {
        state->script_code = state->script_url.substr(9);
    } else if (state->script_url.starts_with("data:")) {
        // Extract script from data: URL (simplified)
        auto comma_pos = state->script_url.find(',');
        if (comma_pos != std::string::npos) {
            state->script_code = state->script_url.substr(comma_pos + 1);
        }
    }

    // Create a separate QuickJS runtime+context for the worker
    state->worker_rt = JS_NewRuntime();
    if (state->worker_rt) {
        JS_SetMemoryLimit(state->worker_rt, 32 * 1024 * 1024); // 32 MB for workers
        JS_SetMaxStackSize(state->worker_rt, 4 * 1024 * 1024);  // 4 MB stack

        state->worker_ctx = JS_NewContext(state->worker_rt);
        if (state->worker_ctx) {
            setup_worker_context(state);
        }
    }

    JS_SetOpaque(obj, state);
    return obj;
}

// ---- Worker.postMessage(data) ----

static JSValue js_worker_post_message(JSContext* ctx, JSValueConst this_val,
                                       int argc, JSValueConst* argv) {
    auto* state = get_worker_state(this_val);
    if (!state) return JS_ThrowTypeError(ctx, "not a Worker");
    // Silently ignore messages to terminated workers (matches browser behavior)
    if (state->terminated || !state->worker_ctx)
        return JS_UNDEFINED;

    if (argc < 1) return JS_UNDEFINED;

    // Serialize the message to JSON
    JSValue json_str = JS_JSONStringify(ctx, argv[0], JS_UNDEFINED, JS_UNDEFINED);
    std::string json_data;
    if (JS_IsException(json_str)) {
        // Fallback: convert to string
        const char* str = JS_ToCString(ctx, argv[0]);
        if (str) {
            json_data = std::string("\"") + str + "\"";
            JS_FreeCString(ctx, str);
        } else {
            return JS_UNDEFINED;
        }
    } else {
        const char* str = JS_ToCString(ctx, json_str);
        if (str) {
            json_data = str;
            JS_FreeCString(ctx, str);
        }
        JS_FreeValue(ctx, json_str);
    }

    // Dispatch the message to the worker context synchronously
    dispatch_to_worker(state, json_data);

    // After the worker script runs, collect any messages it sent back
    dispatch_from_worker(state, ctx, this_val);

    return JS_UNDEFINED;
}

// ---- Worker.terminate() ----

static JSValue js_worker_terminate(JSContext* ctx, JSValueConst this_val,
                                    int /*argc*/, JSValueConst* /*argv*/) {
    auto* state = get_worker_state(this_val);
    if (!state) return JS_ThrowTypeError(ctx, "not a Worker");

    state->terminated = true;

    // Tear down the worker context immediately
    if (state->worker_ctx) {
        JS_FreeContext(state->worker_ctx);
        state->worker_ctx = nullptr;
    }
    if (state->worker_rt) {
        JS_FreeRuntime(state->worker_rt);
        state->worker_rt = nullptr;
    }

    return JS_UNDEFINED;
}

// ---- Worker.onmessage getter/setter ----

static JSValue js_worker_get_onmessage(JSContext* ctx, JSValueConst this_val) {
    auto* state = get_worker_state(this_val);
    if (!state) return JS_NULL;
    return JS_DupValue(ctx, state->onmessage);
}

static JSValue js_worker_set_onmessage(JSContext* ctx, JSValueConst this_val, JSValueConst val) {
    auto* state = get_worker_state(this_val);
    if (!state) return JS_EXCEPTION;
    JS_FreeValue(ctx, state->onmessage);
    state->onmessage = JS_DupValue(ctx, val);
    return JS_UNDEFINED;
}

// ---- Worker.onerror getter/setter ----

static JSValue js_worker_get_onerror(JSContext* ctx, JSValueConst this_val) {
    auto* state = get_worker_state(this_val);
    if (!state) return JS_NULL;
    return JS_DupValue(ctx, state->onerror);
}

static JSValue js_worker_set_onerror(JSContext* ctx, JSValueConst this_val, JSValueConst val) {
    auto* state = get_worker_state(this_val);
    if (!state) return JS_EXCEPTION;
    JS_FreeValue(ctx, state->onerror);
    state->onerror = JS_DupValue(ctx, val);
    return JS_UNDEFINED;
}

// ---- Worker prototype function list ----

static const JSCFunctionListEntry worker_proto_funcs[] = {
    JS_CFUNC_DEF("postMessage", 1, js_worker_post_message),
    JS_CFUNC_DEF("terminate", 0, js_worker_terminate),
    JS_CGETSET_DEF("onmessage", js_worker_get_onmessage, js_worker_set_onmessage),
    JS_CGETSET_DEF("onerror", js_worker_get_onerror, js_worker_set_onerror),
};

// ---- Register Worker class on a context ----

static void install_worker_class(JSContext* ctx) {
    JSRuntime* rt = JS_GetRuntime(ctx);

    // Register Worker class (once per runtime)
    if (worker_class_id == 0) {
        JS_NewClassID(&worker_class_id);
    }
    if (!JS_IsRegisteredClass(rt, worker_class_id)) {
        JS_NewClass(rt, worker_class_id, &worker_class_def);
    }

    // Create the prototype with methods and getters/setters
    JSValue proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, worker_proto_funcs,
                                sizeof(worker_proto_funcs) / sizeof(worker_proto_funcs[0]));

    // Create the constructor function
    JSValue ctor = JS_NewCFunction2(ctx, js_worker_constructor,
                                     "Worker", 1,
                                     JS_CFUNC_constructor, 0);

    // Set prototype on constructor
    JS_SetConstructor(ctx, ctor, proto);
    JS_SetClassProto(ctx, worker_class_id, proto);

    // Register on globalThis
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "Worker", ctor);
    JS_FreeValue(ctx, global);
}

// =========================================================================
// AbortController / AbortSignal (Cycle 220)
// =========================================================================

static JSValue js_abort_signal_throw_if_aborted(JSContext* ctx,
                                                  JSValueConst this_val,
                                                  int /*argc*/,
                                                  JSValueConst* /*argv*/) {
    JSValue aborted = JS_GetPropertyStr(ctx, this_val, "aborted");
    bool is_aborted = JS_ToBool(ctx, aborted) != 0;
    JS_FreeValue(ctx, aborted);
    if (is_aborted) {
        JSValue reason = JS_GetPropertyStr(ctx, this_val, "reason");
        if (JS_IsUndefined(reason)) {
            JSValue err = JS_NewError(ctx);
            JS_SetPropertyStr(ctx, err, "name", JS_NewString(ctx, "AbortError"));
            JS_SetPropertyStr(ctx, err, "message",
                JS_NewString(ctx, "The operation was aborted"));
            return JS_Throw(ctx, err);
        }
        return JS_Throw(ctx, reason);
    }
    return JS_UNDEFINED;
}

static JSValue js_abort_controller_abort(JSContext* ctx,
                                          JSValueConst this_val,
                                          int argc, JSValueConst* argv) {
    JSValue sig = JS_GetPropertyStr(ctx, this_val, "_signal_ref");
    if (!JS_IsUndefined(sig)) {
        JS_SetPropertyStr(ctx, sig, "aborted", JS_TRUE);
        if (argc > 0) {
            JS_SetPropertyStr(ctx, sig, "reason", JS_DupValue(ctx, argv[0]));
        } else {
            JSValue err = JS_NewError(ctx);
            JS_SetPropertyStr(ctx, err, "name", JS_NewString(ctx, "AbortError"));
            JS_SetPropertyStr(ctx, err, "message",
                JS_NewString(ctx, "The operation was aborted"));
            JS_SetPropertyStr(ctx, sig, "reason", err);
        }
    }
    JS_FreeValue(ctx, sig);
    return JS_UNDEFINED;
}

static JSValue js_abort_controller_constructor(JSContext* ctx,
                                                JSValueConst /*new_target*/,
                                                int /*argc*/,
                                                JSValueConst* /*argv*/) {
    JSValue controller = JS_NewObject(ctx);

    // Create signal
    JSValue signal = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, signal, "aborted", JS_FALSE);
    JS_SetPropertyStr(ctx, signal, "reason", JS_UNDEFINED);
    JS_SetPropertyStr(ctx, signal, "throwIfAborted",
        JS_NewCFunction(ctx, js_abort_signal_throw_if_aborted,
                        "throwIfAborted", 0));

    JS_SetPropertyStr(ctx, controller, "signal", signal);
    JS_SetPropertyStr(ctx, controller, "_signal_ref", JS_DupValue(ctx, signal));
    JS_SetPropertyStr(ctx, controller, "abort",
        JS_NewCFunction(ctx, js_abort_controller_abort, "abort", 0));

    return controller;
}

// =========================================================================
// structuredClone(value) — JSON round-trip (Cycle 220)
// =========================================================================

static JSValue js_structured_clone(JSContext* ctx, JSValueConst /*this_val*/,
                                    int argc, JSValueConst* argv) {
    if (argc < 1) return JS_UNDEFINED;
    JSValue json_str = JS_JSONStringify(ctx, argv[0], JS_UNDEFINED, JS_UNDEFINED);
    if (JS_IsException(json_str)) return json_str;
    const char* str = JS_ToCString(ctx, json_str);
    if (!str) {
        JS_FreeValue(ctx, json_str);
        return JS_UNDEFINED;
    }
    JSValue result = JS_ParseJSON(ctx, str, std::strlen(str), "<structuredClone>");
    JS_FreeCString(ctx, str);
    JS_FreeValue(ctx, json_str);
    return result;
}

// =========================================================================
// requestIdleCallback(callback) / cancelIdleCallback(id) (Cycle 220)
// =========================================================================

static int next_idle_id = 1;

static JSValue js_idle_deadline_time_remaining(JSContext* ctx,
                                                JSValueConst /*this_val*/,
                                                int /*argc*/,
                                                JSValueConst* /*argv*/) {
    return JS_NewFloat64(ctx, 50.0);
}

static JSValue js_request_idle_callback(JSContext* ctx,
                                         JSValueConst /*this_val*/,
                                         int argc, JSValueConst* argv) {
    if (argc > 0 && JS_IsFunction(ctx, argv[0])) {
        // Create deadline object
        JSValue deadline = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, deadline, "didTimeout", JS_FALSE);
        JS_SetPropertyStr(ctx, deadline, "timeRemaining",
            JS_NewCFunction(ctx, js_idle_deadline_time_remaining,
                            "timeRemaining", 0));
        JSValue args[1] = { deadline };
        JSValue ret = JS_Call(ctx, argv[0], JS_UNDEFINED, 1, args);
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, deadline);
    }
    return JS_NewInt32(ctx, next_idle_id++);
}

static JSValue js_cancel_idle_callback(JSContext* /*ctx*/,
                                        JSValueConst /*this_val*/,
                                        int /*argc*/,
                                        JSValueConst* /*argv*/) {
    // No-op in synchronous engine
    return JS_UNDEFINED;
}

// =========================================================================
// CSS.supports() (Cycle 220)
// =========================================================================

static JSValue js_css_supports(JSContext* ctx, JSValueConst /*this_val*/,
                                int argc, JSValueConst* argv) {
    if (argc < 1) return JS_FALSE;

    // Known CSS properties we support (or that are widely supported)
    static const char* known_properties[] = {
        "display", "color", "background-color", "background",
        "width", "height", "margin", "padding", "border",
        "font-size", "font-family", "font-weight", "font-style",
        "text-align", "text-decoration", "text-transform",
        "position", "top", "right", "bottom", "left",
        "flex", "flex-direction", "flex-wrap", "justify-content",
        "align-items", "align-content", "align-self",
        "grid", "grid-template-columns", "grid-template-rows",
        "grid-gap", "gap", "overflow", "opacity", "visibility",
        "z-index", "transform", "transition", "animation",
        "box-sizing", "cursor", "pointer-events",
        "border-radius", "box-shadow", "text-shadow",
        "min-width", "max-width", "min-height", "max-height",
        "line-height", "letter-spacing", "word-spacing",
        "white-space", "word-break", "overflow-wrap",
        "list-style", "list-style-type", "float", "clear",
        "vertical-align", "content", "outline",
        nullptr
    };

    if (argc == 1) {
        // CSS.supports("(display: grid)") form
        const char* condition = JS_ToCString(ctx, argv[0]);
        if (!condition) return JS_FALSE;
        std::string cond(condition);
        JS_FreeCString(ctx, condition);

        // Strip leading/trailing parens and whitespace
        size_t start = cond.find_first_not_of(" \t(");
        size_t end = cond.find_last_not_of(" \t)");
        if (start == std::string::npos || end == std::string::npos)
            return JS_FALSE;
        std::string inner = cond.substr(start, end - start + 1);

        // Find the property name (before the colon)
        size_t colon = inner.find(':');
        if (colon == std::string::npos) return JS_FALSE;
        std::string prop = inner.substr(0, colon);
        // Trim whitespace from prop
        size_t ps = prop.find_first_not_of(" \t");
        size_t pe = prop.find_last_not_of(" \t");
        if (ps == std::string::npos) return JS_FALSE;
        prop = prop.substr(ps, pe - ps + 1);

        for (const char** p = known_properties; *p; ++p) {
            if (prop == *p) return JS_TRUE;
        }
        return JS_FALSE;
    } else {
        // CSS.supports("display", "grid") form
        const char* prop = JS_ToCString(ctx, argv[0]);
        if (!prop) return JS_FALSE;
        std::string prop_str(prop);
        JS_FreeCString(ctx, prop);

        for (const char** p = known_properties; *p; ++p) {
            if (prop_str == *p) return JS_TRUE;
        }
        return JS_FALSE;
    }
}

// =========================================================================
// TextEncoder class (Encoding API)
// =========================================================================

static JSClassID text_encoder_class_id = 0;

static void js_text_encoder_finalizer(JSRuntime* /*rt*/, JSValue /*val*/) {
    // No opaque data to free
}

static JSClassDef text_encoder_class_def = {
    "TextEncoder",
    js_text_encoder_finalizer,
    nullptr, nullptr, nullptr
};

static JSValue js_text_encoder_encode(JSContext* ctx, JSValueConst /*this_val*/,
                                       int argc, JSValueConst* argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "encode requires 1 argument");

    size_t len = 0;
    const char* str = JS_ToCStringLen(ctx, &len, argv[0]);
    if (!str) return JS_EXCEPTION;

    // Create a Uint8Array with the UTF-8 bytes
    JSValue array_buf = JS_NewArrayBufferCopy(ctx,
        reinterpret_cast<const uint8_t*>(str), len);
    JS_FreeCString(ctx, str);
    if (JS_IsException(array_buf)) return array_buf;

    // Create Uint8Array view over the ArrayBuffer
    JSValue g = JS_GetGlobalObject(ctx);
    JSValue uint8_ctor = JS_GetPropertyStr(ctx, g, "Uint8Array");
    JS_FreeValue(ctx, g);
    JSValue args[3] = { array_buf, JS_NewInt32(ctx, 0), JS_NewInt64(ctx, static_cast<int64_t>(len)) };
    JSValue result = JS_CallConstructor(ctx, uint8_ctor, 3, args);
    JS_FreeValue(ctx, uint8_ctor);
    JS_FreeValue(ctx, array_buf);
    JS_FreeValue(ctx, args[1]);
    JS_FreeValue(ctx, args[2]);
    return result;
}

static JSValue js_text_encoder_get_encoding(JSContext* ctx, JSValueConst /*this_val*/) {
    return JS_NewString(ctx, "utf-8");
}

static const JSCFunctionListEntry text_encoder_proto_funcs[] = {
    JS_CFUNC_DEF("encode", 1, js_text_encoder_encode),
    JS_CGETSET_DEF("encoding", js_text_encoder_get_encoding, nullptr),
};

static JSValue js_text_encoder_constructor(JSContext* ctx, JSValueConst new_target,
                                            int /*argc*/, JSValueConst* /*argv*/) {
    JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto)) return JS_EXCEPTION;

    JSValue obj = JS_NewObjectProtoClass(ctx, proto, text_encoder_class_id);
    JS_FreeValue(ctx, proto);
    return obj;
}

static void install_text_encoder(JSContext* ctx) {
    JSRuntime* rt = JS_GetRuntime(ctx);

    if (text_encoder_class_id == 0) {
        JS_NewClassID(&text_encoder_class_id);
    }
    if (!JS_IsRegisteredClass(rt, text_encoder_class_id)) {
        JS_NewClass(rt, text_encoder_class_id, &text_encoder_class_def);
    }

    JSValue proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, text_encoder_proto_funcs,
                                sizeof(text_encoder_proto_funcs) / sizeof(text_encoder_proto_funcs[0]));

    JSValue ctor = JS_NewCFunction2(ctx, js_text_encoder_constructor,
                                     "TextEncoder", 0,
                                     JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, ctor, proto);
    JS_SetClassProto(ctx, text_encoder_class_id, proto);

    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "TextEncoder", ctor);
    JS_FreeValue(ctx, global);
}

// =========================================================================
// TextDecoder class (Encoding API)
// =========================================================================

static JSClassID text_decoder_class_id = 0;

struct TextDecoderState {
    std::string encoding = "utf-8";
};

static void js_text_decoder_finalizer(JSRuntime* /*rt*/, JSValue val) {
    auto* state = static_cast<TextDecoderState*>(JS_GetOpaque(val, text_decoder_class_id));
    delete state;
}

static JSClassDef text_decoder_class_def = {
    "TextDecoder",
    js_text_decoder_finalizer,
    nullptr, nullptr, nullptr
};

static JSValue js_text_decoder_decode(JSContext* ctx, JSValueConst /*this_val*/,
                                       int argc, JSValueConst* argv) {
    if (argc < 1)
        return JS_NewString(ctx, "");

    // Handle ArrayBuffer, TypedArray, or DataView
    size_t byte_length = 0;
    uint8_t* buf = nullptr;

    // Try to get as typed array first
    size_t offset = 0;
    size_t arr_len = 0;
    JSValue ab = JS_GetTypedArrayBuffer(ctx, argv[0], &offset, &arr_len, nullptr);
    if (!JS_IsException(ab)) {
        buf = JS_GetArrayBuffer(ctx, &byte_length, ab);
        if (buf) {
            buf += offset;
            byte_length = arr_len;
        }
        JS_FreeValue(ctx, ab);
    } else {
        // Maybe it's a raw ArrayBuffer
        JS_FreeValue(ctx, JS_GetException(ctx)); // clear the exception
        buf = JS_GetArrayBuffer(ctx, &byte_length, argv[0]);
    }

    if (!buf || byte_length == 0)
        return JS_NewString(ctx, "");

    return JS_NewStringLen(ctx, reinterpret_cast<const char*>(buf), byte_length);
}

static JSValue js_text_decoder_get_encoding(JSContext* ctx, JSValueConst this_val) {
    auto* state = static_cast<TextDecoderState*>(JS_GetOpaque(this_val, text_decoder_class_id));
    if (!state) return JS_NewString(ctx, "utf-8");
    return JS_NewString(ctx, state->encoding.c_str());
}

static const JSCFunctionListEntry text_decoder_proto_funcs[] = {
    JS_CFUNC_DEF("decode", 1, js_text_decoder_decode),
    JS_CGETSET_DEF("encoding", js_text_decoder_get_encoding, nullptr),
};

static JSValue js_text_decoder_constructor(JSContext* ctx, JSValueConst new_target,
                                            int argc, JSValueConst* argv) {
    JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto)) return JS_EXCEPTION;

    JSValue obj = JS_NewObjectProtoClass(ctx, proto, text_decoder_class_id);
    JS_FreeValue(ctx, proto);
    if (JS_IsException(obj)) return obj;

    auto* state = new TextDecoderState();

    // Accept optional encoding argument (only "utf-8" supported)
    if (argc >= 1 && JS_IsString(argv[0])) {
        const char* enc = JS_ToCString(ctx, argv[0]);
        if (enc) {
            std::string enc_str(enc);
            JS_FreeCString(ctx, enc);
            // Normalize common aliases — map to canonical names
            // ASCII and ISO-8859-1/Latin1 are pass-through compatible
            // with UTF-8 for the ASCII range (0x00-0x7F)
            if (enc_str == "utf-8" || enc_str == "utf8" || enc_str == "UTF-8") {
                state->encoding = "utf-8";
            } else if (enc_str == "ascii" || enc_str == "us-ascii" || enc_str == "ASCII") {
                state->encoding = "utf-8"; // ASCII is a subset of UTF-8
            } else if (enc_str == "iso-8859-1" || enc_str == "latin1" ||
                       enc_str == "latin-1" || enc_str == "ISO-8859-1" ||
                       enc_str == "windows-1252" || enc_str == "cp1252") {
                state->encoding = "utf-8"; // Latin1 pass-through for ASCII-range content
            } else {
                state->encoding = enc_str;
            }
        }
    }

    JS_SetOpaque(obj, state);
    return obj;
}

static void install_text_decoder(JSContext* ctx) {
    JSRuntime* rt = JS_GetRuntime(ctx);

    if (text_decoder_class_id == 0) {
        JS_NewClassID(&text_decoder_class_id);
    }
    if (!JS_IsRegisteredClass(rt, text_decoder_class_id)) {
        JS_NewClass(rt, text_decoder_class_id, &text_decoder_class_def);
    }

    JSValue proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, text_decoder_proto_funcs,
                                sizeof(text_decoder_proto_funcs) / sizeof(text_decoder_proto_funcs[0]));

    JSValue ctor = JS_NewCFunction2(ctx, js_text_decoder_constructor,
                                     "TextDecoder", 0,
                                     JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, ctor, proto);
    JS_SetClassProto(ctx, text_decoder_class_id, proto);

    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "TextDecoder", ctor);
    JS_FreeValue(ctx, global);
}

} // anonymous namespace

// =========================================================================
// Public API
// =========================================================================

void install_window_bindings(JSContext* ctx, const std::string& url,
                              int width, int height) {
    // Record page start time for performance.now()
    page_start_time = std::chrono::steady_clock::now();

    // Record wall-clock time origin for performance.timeOrigin
    {
        auto wall_now = std::chrono::system_clock::now();
        auto epoch_us = std::chrono::duration_cast<std::chrono::microseconds>(
            wall_now.time_since_epoch());
        page_time_origin_ms = static_cast<double>(epoch_us.count()) / 1000.0;
    }

    JSValue global = JS_GetGlobalObject(ctx);

    // ---- window === globalThis ----
    // Set window to reference the global object itself
    JS_SetPropertyStr(ctx, global, "window", JS_DupValue(ctx, global));

    // ---- window.innerWidth / window.innerHeight ----
    JS_SetPropertyStr(ctx, global, "innerWidth", JS_NewInt32(ctx, width));
    JS_SetPropertyStr(ctx, global, "innerHeight", JS_NewInt32(ctx, height));
    JS_SetPropertyStr(ctx, global, "outerWidth", JS_NewInt32(ctx, width));
    JS_SetPropertyStr(ctx, global, "outerHeight", JS_NewInt32(ctx, height));

    // ---- window.scrollX / scrollY / pageXOffset / pageYOffset ----
    JS_SetPropertyStr(ctx, global, "scrollX", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, global, "scrollY", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, global, "pageXOffset", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, global, "pageYOffset", JS_NewInt32(ctx, 0));

    // ---- window.alert / confirm / prompt ----
    JS_SetPropertyStr(ctx, global, "alert",
        JS_NewCFunction(ctx, js_window_alert, "alert", 1));
    JS_SetPropertyStr(ctx, global, "confirm",
        JS_NewCFunction(ctx, js_window_confirm, "confirm", 1));
    JS_SetPropertyStr(ctx, global, "prompt",
        JS_NewCFunction(ctx, js_window_prompt, "prompt", 2));

    // ---- window.btoa / window.atob ----
    JS_SetPropertyStr(ctx, global, "btoa",
        JS_NewCFunction(ctx, js_window_btoa, "btoa", 1));
    JS_SetPropertyStr(ctx, global, "atob",
        JS_NewCFunction(ctx, js_window_atob, "atob", 1));

    // ---- window.performance ----
    JSValue performance = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, performance, "now",
        JS_NewCFunction(ctx, js_performance_now, "now", 0));
    JS_SetPropertyStr(ctx, performance, "timeOrigin",
        JS_NewFloat64(ctx, page_time_origin_ms));
    JS_SetPropertyStr(ctx, performance, "getEntries",
        JS_NewCFunction(ctx, js_performance_get_entries, "getEntries", 0));
    JS_SetPropertyStr(ctx, performance, "getEntriesByType",
        JS_NewCFunction(ctx, js_performance_get_entries_by_type, "getEntriesByType", 1));
    JS_SetPropertyStr(ctx, performance, "getEntriesByName",
        JS_NewCFunction(ctx, js_performance_get_entries_by_name, "getEntriesByName", 1));
    JS_SetPropertyStr(ctx, performance, "mark",
        JS_NewCFunction(ctx, js_performance_mark, "mark", 1));
    JS_SetPropertyStr(ctx, performance, "measure",
        JS_NewCFunction(ctx, js_performance_measure, "measure", 1));
    JS_SetPropertyStr(ctx, performance, "clearMarks",
        JS_NewCFunction(ctx, js_performance_clear_marks, "clearMarks", 0));
    JS_SetPropertyStr(ctx, performance, "clearMeasures",
        JS_NewCFunction(ctx, js_performance_clear_measures, "clearMeasures", 0));
    JS_SetPropertyStr(ctx, performance, "clearResourceTimings",
        JS_NewCFunction(ctx, js_performance_clear_resource_timings, "clearResourceTimings", 0));
    JS_SetPropertyStr(ctx, performance, "toJSON",
        JS_NewCFunction(ctx, js_performance_to_json, "toJSON", 0));
    JS_SetPropertyStr(ctx, global, "performance", performance);

    // ---- requestAnimationFrame / cancelAnimationFrame ----
    auto* raf_state = new RAFState();
    JS_SetPropertyStr(ctx, global, "__raf_state_ptr",
        JS_NewInt64(ctx, static_cast<int64_t>(
            reinterpret_cast<uintptr_t>(raf_state))));
    JS_SetPropertyStr(ctx, global, "requestAnimationFrame",
        JS_NewCFunction(ctx, js_request_animation_frame, "requestAnimationFrame", 1));
    JS_SetPropertyStr(ctx, global, "cancelAnimationFrame",
        JS_NewCFunction(ctx, js_cancel_animation_frame, "cancelAnimationFrame", 1));

    // ---- matchMedia ----
    JS_SetPropertyStr(ctx, global, "matchMedia",
        JS_NewCFunction(ctx, js_window_match_media, "matchMedia", 1));

    // ---- queueMicrotask ----
    JS_SetPropertyStr(ctx, global, "queueMicrotask",
        JS_NewCFunction(ctx, js_queue_microtask, "queueMicrotask", 1));

    // ---- getSelection ----
    JS_SetPropertyStr(ctx, global, "getSelection",
        JS_NewCFunction(ctx, js_window_get_selection, "getSelection", 0));

    // ---- window.location ----
    ParsedURL parsed = parse_url(url);
    JSValue location = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, location, "href",
        JS_NewString(ctx, parsed.href.c_str()));
    JS_SetPropertyStr(ctx, location, "hostname",
        JS_NewString(ctx, parsed.hostname.c_str()));
    JS_SetPropertyStr(ctx, location, "pathname",
        JS_NewString(ctx, parsed.pathname.c_str()));
    JS_SetPropertyStr(ctx, location, "protocol",
        JS_NewString(ctx, parsed.protocol.c_str()));
    JS_SetPropertyStr(ctx, location, "origin",
        JS_NewString(ctx, parsed.origin.c_str()));
    JS_SetPropertyStr(ctx, location, "host",
        JS_NewString(ctx, parsed.host.c_str()));
    JS_SetPropertyStr(ctx, location, "port",
        JS_NewString(ctx, parsed.port.c_str()));
    JS_SetPropertyStr(ctx, location, "search",
        JS_NewString(ctx, parsed.search.c_str()));
    JS_SetPropertyStr(ctx, location, "hash",
        JS_NewString(ctx, parsed.hash.c_str()));

    // Navigation stubs (no-ops in sync engine)
    {
        const char* noop_code = R"JS(
(function(loc) {
    loc.assign = function(url) {};
    loc.replace = function(url) {};
    loc.reload = function() {};
    loc.toString = function() { return loc.href; };
})(this)
)JS";
        JSValue noop_fn = JS_Eval(ctx, noop_code, std::strlen(noop_code),
                                   "<location-methods>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsFunction(ctx, noop_fn)) {
            JS_Call(ctx, noop_fn, location, 0, nullptr);
        }
        JS_FreeValue(ctx, noop_fn);
    }

    JS_SetPropertyStr(ctx, global, "location", location);

    // ---- window.navigator ----
    JSValue navigator = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, navigator, "userAgent",
        JS_NewString(ctx, "Vibrowser/0.7.0"));
    JS_SetPropertyStr(ctx, navigator, "language",
        JS_NewString(ctx, "en-US"));
    // navigator.languages = ["en-US", "en"]
    {
        JSValue langs = JS_NewArray(ctx);
        JS_SetPropertyUint32(ctx, langs, 0, JS_NewString(ctx, "en-US"));
        JS_SetPropertyUint32(ctx, langs, 1, JS_NewString(ctx, "en"));
        JS_SetPropertyStr(ctx, navigator, "languages", langs);
    }
    JS_SetPropertyStr(ctx, navigator, "platform",
        JS_NewString(ctx, "MacIntel"));
    JS_SetPropertyStr(ctx, navigator, "vendor",
        JS_NewString(ctx, "Vibrowser"));
    JS_SetPropertyStr(ctx, navigator, "onLine", JS_TRUE);
    JS_SetPropertyStr(ctx, navigator, "cookieEnabled", JS_TRUE);
    JS_SetPropertyStr(ctx, navigator, "hardwareConcurrency",
        JS_NewInt32(ctx, 4));
    JS_SetPropertyStr(ctx, navigator, "maxTouchPoints",
        JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, navigator, "doNotTrack",
        JS_NewString(ctx, "1"));
    JS_SetPropertyStr(ctx, navigator, "webdriver", JS_FALSE);
    JS_SetPropertyStr(ctx, global, "navigator", navigator);

    // ---- window.localStorage ----
    JSValue storage = create_local_storage(ctx);
    JS_SetPropertyStr(ctx, global, "localStorage", storage);
    // Also alias as window.sessionStorage (same in-memory store)
    JSValue ls_ref = JS_GetPropertyStr(ctx, global, "localStorage");
    JS_SetPropertyStr(ctx, global, "sessionStorage", ls_ref);
    // JS_SetPropertyStr consumes a reference, and JS_GetPropertyStr returned
    // one, so ownership is transferred -- no need to free ls_ref.

    // ---- window.devicePixelRatio ----
    JS_SetPropertyStr(ctx, global, "devicePixelRatio",
        JS_NewFloat64(ctx, 1.0));

    // ---- window.isSecureContext ----
    JS_SetPropertyStr(ctx, global, "isSecureContext", JS_TRUE);

    // ---- window.scrollTo / window.scrollBy / window.scroll ----
    JS_SetPropertyStr(ctx, global, "scrollTo",
        JS_NewCFunction(ctx, js_window_scroll_to, "scrollTo", 2));
    JS_SetPropertyStr(ctx, global, "scrollBy",
        JS_NewCFunction(ctx, js_window_scroll_by, "scrollBy", 2));
    JS_SetPropertyStr(ctx, global, "scroll",
        JS_NewCFunction(ctx, js_window_scroll_to, "scroll", 2));

    // ---- window.open / window.close ----
    JS_SetPropertyStr(ctx, global, "open",
        JS_NewCFunction(ctx, js_window_open, "open", 1));
    JS_SetPropertyStr(ctx, global, "close",
        JS_NewCFunction(ctx, js_window_close, "close", 0));

    // ---- window.dispatchEvent ----
    JS_SetPropertyStr(ctx, global, "dispatchEvent",
        JS_NewCFunction(ctx, js_window_dispatch_event, "dispatchEvent", 1));

    // ---- window.removeEventListener (fallback stub) ----
    // NOTE: If js_dom_bindings is installed *after* this, it may override
    // window.addEventListener. This removeEventListener covers the case
    // where DOM bindings set addEventListener but not removeEventListener.
    JS_SetPropertyStr(ctx, global, "removeEventListener",
        JS_NewCFunction(ctx, js_window_remove_event_listener,
                        "removeEventListener", 2));

    // ---- window.history ----
    JSValue history = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, history, "length", JS_NewInt32(ctx, 1));
    JS_SetPropertyStr(ctx, history, "state", JS_NULL);
    JS_SetPropertyStr(ctx, global, "history", history);

    // ---- window.screen ----
    JSValue screen = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, screen, "width", JS_NewInt32(ctx, width));
    JS_SetPropertyStr(ctx, screen, "height", JS_NewInt32(ctx, height));
    JS_SetPropertyStr(ctx, screen, "availWidth", JS_NewInt32(ctx, width));
    JS_SetPropertyStr(ctx, screen, "availHeight", JS_NewInt32(ctx, height));
    JS_SetPropertyStr(ctx, screen, "availLeft", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, screen, "availTop", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, screen, "colorDepth", JS_NewInt32(ctx, 24));
    JS_SetPropertyStr(ctx, screen, "pixelDepth", JS_NewInt32(ctx, 24));
    // screen.orientation object
    JSValue orientation = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, orientation, "type",
        JS_NewString(ctx, "landscape-primary"));
    JS_SetPropertyStr(ctx, orientation, "angle", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, screen, "orientation", orientation);
    JS_SetPropertyStr(ctx, global, "screen", screen);

    // ---- AbortController (Cycle 220) ----
    JS_SetPropertyStr(ctx, global, "AbortController",
        JS_NewCFunction2(ctx, js_abort_controller_constructor,
                         "AbortController", 0, JS_CFUNC_constructor, 0));

    // ---- structuredClone (Cycle 220) ----
    JS_SetPropertyStr(ctx, global, "structuredClone",
        JS_NewCFunction(ctx, js_structured_clone, "structuredClone", 1));

    // ---- requestIdleCallback / cancelIdleCallback (Cycle 220) ----
    JS_SetPropertyStr(ctx, global, "requestIdleCallback",
        JS_NewCFunction(ctx, js_request_idle_callback,
                        "requestIdleCallback", 1));
    JS_SetPropertyStr(ctx, global, "cancelIdleCallback",
        JS_NewCFunction(ctx, js_cancel_idle_callback,
                        "cancelIdleCallback", 1));

    // ---- CSS.supports (Cycle 220) ----
    JSValue css_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, css_obj, "supports",
        JS_NewCFunction(ctx, js_css_supports, "supports", 1));
    JS_SetPropertyStr(ctx, global, "CSS", css_obj);

    JS_FreeValue(ctx, global);

    // ---- Worker class ----
    install_worker_class(ctx);

    // ---- TextEncoder / TextDecoder ----
    install_text_encoder(ctx);
    install_text_decoder(ctx);

    // ---- TextEncoder.encodeInto() ----
    {
        const char* encode_into_src = R"JS(
        TextEncoder.prototype.encodeInto = function(str, dest) {
            var encoded = this.encode(str);
            var written = Math.min(encoded.length, dest.length || 0);
            for (var i = 0; i < written; i++) dest[i] = encoded[i];
            return { read: str.length, written: written };
        };
)JS";
        JSValue ei_ret = JS_Eval(ctx, encode_into_src, std::strlen(encode_into_src),
                                  "<text-encoder-extra>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(ei_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, ei_ret);
    }

    // ---- navigator.clipboard (stub) ----
    {
        JSValue g = JS_GetGlobalObject(ctx);
        JSValue nav = JS_GetPropertyStr(ctx, g, "navigator");
        JS_FreeValue(ctx, g);
        if (!JS_IsException(nav) && !JS_IsUndefined(nav)) {
            const char* clipboard_setup = R"JS(
(function(nav) {
    nav.clipboard = {
        writeText: function(text) {
            return Promise.resolve(undefined);
        },
        readText: function() {
            return Promise.resolve('');
        },
        write: function(data) {
            return Promise.resolve(undefined);
        },
        read: function() {
            return Promise.resolve([]);
        }
    };
})
)JS";
            JSValue fn = JS_Eval(ctx, clipboard_setup, std::strlen(clipboard_setup),
                                  "<clipboard-setup>", JS_EVAL_TYPE_GLOBAL);
            if (!JS_IsException(fn)) {
                JSValue wrapped = JS_Call(ctx, fn, JS_UNDEFINED, 1, &nav);
                JS_FreeValue(ctx, wrapped);
            }
            JS_FreeValue(ctx, fn);
        }
        JS_FreeValue(ctx, nav);
    }

    // =========================================================================
    // Complex objects installed via JS eval (history methods, crypto, URL,
    // URLSearchParams).  Using eval is the established pattern in this file
    // for objects that need closures, prototypes, or complex property setups.
    // =========================================================================

    const char* extended_setup = R"JS(
(function() {
    // ---- localStorage.length getter ----
    Object.defineProperty(localStorage, 'length', {
        get: function() { return localStorage.__getLength(); },
        configurable: true
    });

    // ---- history methods (no-ops) ----
    history.pushState = function(state, title, url) {};
    history.replaceState = function(state, title, url) {};
    history.back = function() {};
    history.forward = function() {};
    history.go = function(delta) {};

    // ---- window.crypto.getRandomValues ----
    window.crypto = {
        getRandomValues: function(array) {
            // Fill typed array with pseudo-random bytes.
            // We use Math.random() since we don't have access to
            // arc4random_buf from JS, but this is only a stub for
            // compatibility -- not for cryptographic security.
            if (array && array.length !== undefined) {
                for (var i = 0; i < array.length; i++) {
                    array[i] = (Math.random() * 256) | 0;
                }
            }
            return array;
        },
        subtle: {},
        randomUUID: function() {
            // Generate a v4 UUID using Math.random
            var hex = '0123456789abcdef';
            var uuid = '';
            for (var i = 0; i < 36; i++) {
                if (i === 8 || i === 13 || i === 18 || i === 23) {
                    uuid += '-';
                } else if (i === 14) {
                    uuid += '4';
                } else if (i === 19) {
                    uuid += hex[(Math.random() * 4 | 0) + 8];
                } else {
                    uuid += hex[Math.random() * 16 | 0];
                }
            }
            return uuid;
        }
    };

    // ---- URLSearchParams class ----
    window.URLSearchParams = function URLSearchParams(init) {
        this._params = [];
        if (typeof init === 'string') {
            var s = init;
            if (s.charAt(0) === '?') s = s.substring(1);
            if (s.length > 0) {
                var pairs = s.split('&');
                for (var i = 0; i < pairs.length; i++) {
                    var eq = pairs[i].indexOf('=');
                    if (eq >= 0) {
                        this._params.push([
                            decodeURIComponent(pairs[i].substring(0, eq).replace(/\+/g, ' ')),
                            decodeURIComponent(pairs[i].substring(eq + 1).replace(/\+/g, ' '))
                        ]);
                    } else {
                        this._params.push([decodeURIComponent(pairs[i].replace(/\+/g, ' ')), '']);
                    }
                }
            }
        }
    };
    URLSearchParams.prototype.get = function(name) {
        for (var i = 0; i < this._params.length; i++) {
            if (this._params[i][0] === name) return this._params[i][1];
        }
        return null;
    };
    URLSearchParams.prototype.set = function(name, value) {
        var found = false;
        var newParams = [];
        for (var i = 0; i < this._params.length; i++) {
            if (this._params[i][0] === name) {
                if (!found) {
                    newParams.push([name, String(value)]);
                    found = true;
                }
                // skip duplicates
            } else {
                newParams.push(this._params[i]);
            }
        }
        if (!found) newParams.push([name, String(value)]);
        this._params = newParams;
    };
    URLSearchParams.prototype.has = function(name) {
        for (var i = 0; i < this._params.length; i++) {
            if (this._params[i][0] === name) return true;
        }
        return false;
    };
    URLSearchParams.prototype['delete'] = function(name) {
        var newParams = [];
        for (var i = 0; i < this._params.length; i++) {
            if (this._params[i][0] !== name) newParams.push(this._params[i]);
        }
        this._params = newParams;
    };
    URLSearchParams.prototype.toString = function() {
        var parts = [];
        for (var i = 0; i < this._params.length; i++) {
            parts.push(encodeURIComponent(this._params[i][0]) + '=' +
                        encodeURIComponent(this._params[i][1]));
        }
        return parts.join('&');
    };
    URLSearchParams.prototype.forEach = function(cb) {
        for (var i = 0; i < this._params.length; i++) {
            cb(this._params[i][1], this._params[i][0], this);
        }
    };
    URLSearchParams.prototype.entries = function() {
        return this._params[Symbol.iterator] ? this._params : this._params.slice();
    };
    URLSearchParams.prototype.keys = function() {
        return this._params.map(function(p) { return p[0]; });
    };
    URLSearchParams.prototype.values = function() {
        return this._params.map(function(p) { return p[1]; });
    };
    URLSearchParams.prototype.append = function(name, value) {
        this._params.push([String(name), String(value)]);
    };
    URLSearchParams.prototype.getAll = function(name) {
        var result = [];
        for (var i = 0; i < this._params.length; i++) {
            if (this._params[i][0] === name) result.push(this._params[i][1]);
        }
        return result;
    };
    URLSearchParams.prototype.sort = function() {
        this._params.sort(function(a, b) {
            return a[0] < b[0] ? -1 : a[0] > b[0] ? 1 : 0;
        });
    };
    URLSearchParams.prototype.size = 0;
    Object.defineProperty(URLSearchParams.prototype, 'size', {
        get: function() { return this._params.length; },
        configurable: true
    });

    // ---- URL class ----
    window.URL = function URL(urlString, base) {
        if (typeof urlString !== 'string') urlString = String(urlString);
        // Handle relative URLs with base
        if (base !== undefined && urlString.indexOf('://') === -1) {
            var b = (typeof base === 'string') ? base : base.href || String(base);
            // Simple resolution: strip trailing path from base and append
            var baseNoPath = b;
            var protoEnd = b.indexOf('://');
            if (protoEnd >= 0) {
                var rest = b.substring(protoEnd + 3);
                var firstSlash = rest.indexOf('/');
                if (firstSlash >= 0) {
                    baseNoPath = b.substring(0, protoEnd + 3 + firstSlash);
                }
            }
            if (urlString.charAt(0) === '/') {
                urlString = baseNoPath + urlString;
            } else {
                urlString = baseNoPath + '/' + urlString;
            }
        }

        this.href = urlString;
        this.protocol = '';
        this.hostname = '';
        this.host = '';
        this.port = '';
        this.pathname = '/';
        this.search = '';
        this.hash = '';
        this.origin = '';
        this.username = '';
        this.password = '';

        // Parse the URL
        var s = urlString;
        var protoIdx = s.indexOf('://');
        if (protoIdx >= 0) {
            this.protocol = s.substring(0, protoIdx + 1);
            s = s.substring(protoIdx + 3);
        }

        // Hash
        var hashIdx = s.indexOf('#');
        if (hashIdx >= 0) {
            this.hash = s.substring(hashIdx);
            s = s.substring(0, hashIdx);
        }

        // Search/query
        var qIdx = s.indexOf('?');
        if (qIdx >= 0) {
            this.search = s.substring(qIdx);
            s = s.substring(0, qIdx);
        }

        // Pathname
        var slashIdx = s.indexOf('/');
        if (slashIdx >= 0) {
            this.pathname = s.substring(slashIdx);
            s = s.substring(0, slashIdx);
        }

        // Host (may include port)
        this.host = s;
        var colonIdx = s.lastIndexOf(':');
        if (colonIdx >= 0 && colonIdx < s.length - 1) {
            this.hostname = s.substring(0, colonIdx);
            this.port = s.substring(colonIdx + 1);
        } else {
            this.hostname = s;
            this.port = '';
        }

        // Origin
        if (this.protocol) {
            this.origin = this.protocol + '//' + this.host;
        }

        // searchParams
        this.searchParams = new URLSearchParams(this.search);
    };
    URL.prototype.toString = function() {
        return this.href;
    };
    URL.prototype.toJSON = function() {
        return this.href;
    };
    // Static methods
    URL.createObjectURL = function() { return 'blob:null/stub'; };
    URL.revokeObjectURL = function() {};
})();
)JS";
    JSValue ret = JS_Eval(ctx, extended_setup, std::strlen(extended_setup),
                           "<window-extended-setup>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(ret)) {
        JSValue exc = JS_GetException(ctx);
        JS_FreeValue(ctx, exc);
    }
    JS_FreeValue(ctx, ret);

    // ------------------------------------------------------------------
    // SharedWorker stub
    // ------------------------------------------------------------------
    {
        const char* sw_src = R"JS(
        (function() {
            function SharedWorker(url, options) {
                this.port = {
                    onmessage: null,
                    onmessageerror: null,
                    postMessage: function(msg) {},
                    start: function() {},
                    close: function() {},
                    addEventListener: function() {},
                    removeEventListener: function() {}
                };
                this.onerror = null;
            }
            globalThis.SharedWorker = SharedWorker;
        })();
)JS";
        JSValue sw_ret = JS_Eval(ctx, sw_src, std::strlen(sw_src),
                                  "<shared-worker>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(sw_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, sw_ret);
    }

    // ------------------------------------------------------------------
    // importScripts stub (worker global scope)
    // ------------------------------------------------------------------
    {
        const char* is_src = R"JS(
        if (typeof importScripts === 'undefined') {
            globalThis.importScripts = function() {};
        }
)JS";
        JSValue is_ret = JS_Eval(ctx, is_src, std::strlen(is_src),
                                  "<import-scripts>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(is_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, is_ret);
    }

    // ------------------------------------------------------------------
    // performance.memory stub (non-standard, Chrome-only)
    // ------------------------------------------------------------------
    {
        const char* perf_mem_src = R"JS(
        if (typeof performance !== 'undefined') {
            performance.memory = {
                usedJSHeapSize: 0,
                totalJSHeapSize: 0,
                jsHeapSizeLimit: 0
            };
        }
)JS";
        JSValue pm_ret = JS_Eval(ctx, perf_mem_src, std::strlen(perf_mem_src),
                                  "<performance-memory>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(pm_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, pm_ret);
    }
}

} // namespace clever::js
