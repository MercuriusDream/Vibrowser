#include <clever/js/js_window.h>
#include <clever/js/js_web_storage.h>
#include <clever/js/js_workers.h>
#include <clever/js/js_dom_bindings.h>

extern "C" {
#include <quickjs.h>
}

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <ctime>
#include <map>
#include <mutex>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#ifdef __APPLE__
#include <Security/SecRandom.h>
#endif

namespace clever::js {

// Forward declaration: dispatch_window_listeners is defined in js_dom_bindings.cpp.
// It iterates the window sentinel listeners (stored under nullptr key in DOMState::listeners)
// and calls each registered handler with the given event object.
void dispatch_window_listeners(JSContext* ctx, const std::string& event_type, JSValue event_obj);

// =========================================================================
// Blob URL registry — maps "blob:<uuid>" -> (mime_type, raw_data)
// Thread-safe: guarded by g_blob_registry_mutex.
// =========================================================================

static std::mutex g_blob_registry_mutex;
static std::map<std::string, std::pair<std::string, std::string>> g_blob_registry;

// Generate a UUID v4 string suitable for blob: URLs.
static std::string generate_blob_uuid() {
    uint8_t bytes[16] = {};
#ifdef __APPLE__
    (void)SecRandomCopyBytes(kSecRandomDefault, sizeof(bytes), bytes);
#else
    // Fallback: use rand() (not cryptographically secure, but sufficient for blob URLs)
    for (auto& b : bytes) b = static_cast<uint8_t>(rand() & 0xFF);
#endif
    // Set UUID v4 version and variant bits.
    bytes[6] = (bytes[6] & 0x0F) | 0x40;
    bytes[8] = (bytes[8] & 0x3F) | 0x80;

    char buf[37];
    snprintf(buf, sizeof(buf),
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             bytes[0], bytes[1], bytes[2], bytes[3],
             bytes[4], bytes[5],
             bytes[6], bytes[7],
             bytes[8], bytes[9],
             bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
    return std::string(buf);
}

// Public accessor: look up blob data by URL.
const std::pair<std::string, std::string>* get_blob_data(const std::string& url) {
    std::lock_guard<std::mutex> lock(g_blob_registry_mutex);
    auto it = g_blob_registry.find(url);
    if (it == g_blob_registry.end()) return nullptr;
    return &it->second;
}

// =========================================================================
// POST form submission storage — thread-local pending form data
// =========================================================================

thread_local std::string g_pending_post_body;
thread_local std::string g_pending_post_content_type;

// Public accessor: consume pending POST data and clear it
bool consume_pending_post_submission(std::string& out_body, std::string& out_content_type) {
    if (g_pending_post_body.empty()) return false;
    out_body = std::move(g_pending_post_body);
    out_content_type = std::move(g_pending_post_content_type);
    g_pending_post_body.clear();
    g_pending_post_content_type.clear();
    return true;
}

// Native C++ function to store pending form submission data
static JSValue js_window_set_pending_form_submission(JSContext* ctx, JSValueConst /*this_val*/,
                                                      int argc, JSValueConst* argv) {
    if (argc < 2) return JS_UNDEFINED;

    const char* body_cstr = JS_ToCString(ctx, argv[0]);
    const char* enctype_cstr = JS_ToCString(ctx, argv[1]);

    if (body_cstr) {
        g_pending_post_body = body_cstr;
        JS_FreeCString(ctx, body_cstr);
    }
    if (enctype_cstr) {
        g_pending_post_content_type = enctype_cstr;
        JS_FreeCString(ctx, enctype_cstr);
    }

    return JS_UNDEFINED;
}

// Build and dispatch a Window hashchange event.
// - Calls window.onhashchange if set.
// - Dispatches to window.addEventListener('hashchange', ...) listeners.
static void fire_hashchange_event(JSContext* ctx,
                                 const std::string& old_url,
                                 const std::string& new_url) {
    JSValue global = JS_GetGlobalObject(ctx);

    JSValue event = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, event, "type", JS_NewString(ctx, "hashchange"));
    JS_SetPropertyStr(ctx, event, "oldURL", JS_NewString(ctx, old_url.c_str()));
    JS_SetPropertyStr(ctx, event, "newURL", JS_NewString(ctx, new_url.c_str()));
    JS_SetPropertyStr(ctx, event, "bubbles", JS_FALSE);
    JS_SetPropertyStr(ctx, event, "cancelable", JS_FALSE);
    JS_SetPropertyStr(ctx, event, "defaultPrevented", JS_FALSE);
    JS_SetPropertyStr(ctx, event, "target", JS_DupValue(ctx, global));
    JS_SetPropertyStr(ctx, event, "currentTarget", JS_DupValue(ctx, global));
    JS_SetPropertyStr(ctx, event, "eventPhase", JS_NewInt32(ctx, 2)); // AT_TARGET

    JSValue handler = JS_GetPropertyStr(ctx, global, "onhashchange");
    if (JS_IsFunction(ctx, handler)) {
        JSValue result = JS_Call(ctx, handler, global, 1, &event);
        if (JS_IsException(result)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, result);
    }
    JS_FreeValue(ctx, handler);

    dispatch_window_listeners(ctx, "hashchange", event);

    JS_FreeValue(ctx, event);
    JS_FreeValue(ctx, global);
}

static JSValue js_window_fire_hashchange(JSContext* ctx, JSValueConst /*this_val*/,
                                          int argc, JSValueConst* argv) {
    if (argc < 2) {
        return JS_UNDEFINED;
    }

    const char* old_url = JS_ToCString(ctx, argv[0]);
    const char* new_url = JS_ToCString(ctx, argv[1]);
    if (!old_url || !new_url) {
        if (old_url) JS_FreeCString(ctx, old_url);
        if (new_url) JS_FreeCString(ctx, new_url);
        return JS_UNDEFINED;
    }

    fire_hashchange_event(ctx, old_url, new_url);

    JS_FreeCString(ctx, old_url);
    JS_FreeCString(ctx, new_url);
    return JS_UNDEFINED;
}

// C++ native implementation of URL.createObjectURL(blob).
// Reads blob._parts (array of string parts) and blob.type (MIME type),
// assembles the data, stores it in the registry, and returns the blob: URL.
static JSValue js_url_create_object_url(JSContext* ctx, JSValueConst /*this_val*/,
                                         int argc, JSValueConst* argv) {
    if (argc < 1 || !JS_IsObject(argv[0]))
        return JS_ThrowTypeError(ctx, "URL.createObjectURL requires a Blob argument");

    // Read MIME type from blob.type
    JSValue type_val = JS_GetPropertyStr(ctx, argv[0], "type");
    const char* type_cstr = JS_ToCString(ctx, type_val);
    std::string mime_type = type_cstr ? type_cstr : "application/octet-stream";
    if (type_cstr) JS_FreeCString(ctx, type_cstr);
    JS_FreeValue(ctx, type_val);

    // Read blob._parts (array of string/ArrayBuffer parts)
    std::string data;
    JSValue parts_val = JS_GetPropertyStr(ctx, argv[0], "_parts");
    if (JS_IsArray(ctx, parts_val)) {
        JSValue len_val = JS_GetPropertyStr(ctx, parts_val, "length");
        uint32_t len = 0;
        JS_ToUint32(ctx, &len, len_val);
        JS_FreeValue(ctx, len_val);

        for (uint32_t i = 0; i < len; ++i) {
            JSValue part = JS_GetPropertyUint32(ctx, parts_val, i);
            if (JS_IsString(part)) {
                const char* s = JS_ToCString(ctx, part);
                if (s) { data += s; JS_FreeCString(ctx, s); }
            } else {
                // ArrayBuffer or other binary part — try to read as string
                size_t byte_len = 0;
                uint8_t* buf = JS_GetArrayBuffer(ctx, &byte_len, part);
                if (buf && byte_len > 0) {
                    data.append(reinterpret_cast<char*>(buf), byte_len);
                }
            }
            JS_FreeValue(ctx, part);
        }
    }
    JS_FreeValue(ctx, parts_val);

    // Generate URL and store in registry.
    std::string blob_url = "blob:" + generate_blob_uuid();
    {
        std::lock_guard<std::mutex> lock(g_blob_registry_mutex);
        g_blob_registry[blob_url] = { mime_type, std::move(data) };
    }

    return JS_NewString(ctx, blob_url.c_str());
}

// C++ native implementation of URL.revokeObjectURL(url).
// Removes the blob URL from the registry, freeing the stored data.
static JSValue js_url_revoke_object_url(JSContext* ctx, JSValueConst /*this_val*/,
                                         int argc, JSValueConst* argv) {
    if (argc < 1) return JS_UNDEFINED;
    const char* url_cstr = JS_ToCString(ctx, argv[0]);
    if (url_cstr) {
        std::string url(url_cstr);
        JS_FreeCString(ctx, url_cstr);
        std::lock_guard<std::mutex> lock(g_blob_registry_mutex);
        g_blob_registry.erase(url);
    }
    return JS_UNDEFINED;
}

// =========================================================================
// window.crypto.getRandomValues and crypto.randomUUID — C++ native
// =========================================================================

static JSValue js_window_crypto_get_random_values(JSContext* ctx, JSValueConst /*this_val*/,
                                                    int argc, JSValueConst* argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "crypto.getRandomValues requires 1 argument");

    size_t byte_offset = 0, byte_length = 0, bytes_per_element = 0;
    JSValue ab = JS_GetTypedArrayBuffer(ctx, argv[0], &byte_offset, &byte_length, &bytes_per_element);
    if (JS_IsException(ab)) {
        JSValue exc = JS_GetException(ctx);
        JS_FreeValue(ctx, exc);
        size_t direct_len = 0;
        uint8_t* direct_ptr = JS_GetArrayBuffer(ctx, &direct_len, argv[0]);
        if (!direct_ptr)
            return JS_ThrowTypeError(ctx, "crypto.getRandomValues: argument must be a TypedArray");
        if (direct_len > 65536)
            return JS_ThrowRangeError(ctx, "crypto.getRandomValues: byte length exceeds 65536");
        arc4random_buf(direct_ptr, direct_len);
        return JS_DupValue(ctx, argv[0]);
    }
    if (byte_length > 65536) {
        JS_FreeValue(ctx, ab);
        return JS_ThrowRangeError(ctx, "crypto.getRandomValues: byte length exceeds 65536");
    }
    size_t ab_len = 0;
    uint8_t* buf_ptr = JS_GetArrayBuffer(ctx, &ab_len, ab);
    JS_FreeValue(ctx, ab);
    if (!buf_ptr)
        return JS_ThrowTypeError(ctx, "crypto.getRandomValues: could not access array buffer");
    arc4random_buf(buf_ptr + byte_offset, byte_length);
    return JS_DupValue(ctx, argv[0]);
}

static JSValue js_window_crypto_random_uuid(JSContext* ctx, JSValueConst /*this_val*/,
                                             int /*argc*/, JSValueConst* /*argv*/) {
    uint8_t bytes[16];
    arc4random_buf(bytes, sizeof(bytes));
    bytes[6] = (bytes[6] & 0x0f) | 0x40;
    bytes[8] = (bytes[8] & 0x3f) | 0x80;
    char buf[37];
    snprintf(buf, sizeof(buf),
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             bytes[0], bytes[1], bytes[2], bytes[3],
             bytes[4], bytes[5], bytes[6], bytes[7],
             bytes[8], bytes[9],
             bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
    return JS_NewString(ctx, buf);
}

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
// Performance API — mark, measure, getEntries, timing, navigation
// =========================================================================

// Page load start time, set once per install_window_bindings call.
static std::chrono::steady_clock::time_point page_start_time;

// Wall-clock epoch time origin (milliseconds since Unix epoch), for performance.timeOrigin
static double page_time_origin_ms = 0.0;

// A single performance entry (mark or measure)
struct PerformanceEntry {
    std::string name;
    std::string entry_type; // "mark" or "measure"
    double start_time = 0.0;
    double duration = 0.0;
};

// Per-context performance state stored as a plain C++ object, accessed via
// a hidden property on the global object (same pattern as RAFState).
struct PerformanceState {
    std::vector<PerformanceEntry> entries;
};

static double perf_now_ms() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        now - page_start_time);
    return static_cast<double>(elapsed.count()) / 1000.0;
}

static PerformanceState* get_perf_state(JSContext* ctx) {
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue val = JS_GetPropertyStr(ctx, global, "__perf_state_ptr");
    PerformanceState* state = nullptr;
    if (JS_IsNumber(val)) {
        int64_t ptr_val = 0;
        JS_ToInt64(ctx, &ptr_val, val);
        state = reinterpret_cast<PerformanceState*>(static_cast<uintptr_t>(ptr_val));
    }
    JS_FreeValue(ctx, val);
    JS_FreeValue(ctx, global);
    return state;
}

// Build a plain JS object representing a PerformanceEntry
static JSValue make_perf_entry_obj(JSContext* ctx, const PerformanceEntry& e) {
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "name",      JS_NewString(ctx, e.name.c_str()));
    JS_SetPropertyStr(ctx, obj, "entryType", JS_NewString(ctx, e.entry_type.c_str()));
    JS_SetPropertyStr(ctx, obj, "startTime", JS_NewFloat64(ctx, e.start_time));
    JS_SetPropertyStr(ctx, obj, "duration",  JS_NewFloat64(ctx, e.duration));
    return obj;
}

static JSValue js_performance_now(JSContext* ctx, JSValueConst /*this_val*/,
                                   int /*argc*/, JSValueConst* /*argv*/) {
    return JS_NewFloat64(ctx, perf_now_ms());
}

// performance.mark(name [, options]) — stores a PerformanceMark entry
static JSValue js_performance_mark(JSContext* ctx, JSValueConst /*this_val*/,
                                    int argc, JSValueConst* argv) {
    if (argc < 1 || !JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "performance.mark: name must be a string");

    const char* name_cstr = JS_ToCString(ctx, argv[0]);
    if (!name_cstr) return JS_EXCEPTION;
    std::string name(name_cstr);
    JS_FreeCString(ctx, name_cstr);

    // options.startTime overrides current time (optional)
    double start_time = perf_now_ms();
    if (argc >= 2 && JS_IsObject(argv[1])) {
        JSValue st = JS_GetPropertyStr(ctx, argv[1], "startTime");
        if (JS_IsNumber(st)) {
            double v = 0.0;
            JS_ToFloat64(ctx, &v, st);
            start_time = v;
        }
        JS_FreeValue(ctx, st);
    }

    PerformanceEntry entry;
    entry.name       = name;
    entry.entry_type = "mark";
    entry.start_time = start_time;
    entry.duration   = 0.0;

    auto* state = get_perf_state(ctx);
    if (state) state->entries.push_back(entry);

    return make_perf_entry_obj(ctx, entry);
}

// performance.measure(name [, startMarkOrOptions [, endMark]])
static JSValue js_performance_measure(JSContext* ctx, JSValueConst /*this_val*/,
                                       int argc, JSValueConst* argv) {
    if (argc < 1 || !JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "performance.measure: name must be a string");

    const char* name_cstr = JS_ToCString(ctx, argv[0]);
    if (!name_cstr) return JS_EXCEPTION;
    std::string name(name_cstr);
    JS_FreeCString(ctx, name_cstr);

    auto* state = get_perf_state(ctx);

    // Helper: look up the most recent mark with given name, return its startTime
    auto find_mark_time = [&](const std::string& mark_name) -> double {
        if (!state) return 0.0;
        for (int i = static_cast<int>(state->entries.size()) - 1; i >= 0; --i) {
            if (state->entries[i].entry_type == "mark" &&
                state->entries[i].name == mark_name)
                return state->entries[i].start_time;
        }
        return 0.0; // default to navigationStart (0)
    };

    double start_time = 0.0;
    double end_time   = perf_now_ms();

    if (argc >= 2) {
        if (JS_IsString(argv[1])) {
            // startMark name
            const char* sm = JS_ToCString(ctx, argv[1]);
            if (sm) { start_time = find_mark_time(sm); JS_FreeCString(ctx, sm); }
        } else if (JS_IsObject(argv[1])) {
            // options object: { start, end, startTime, duration }
            JSValue js_start = JS_GetPropertyStr(ctx, argv[1], "start");
            JSValue js_end   = JS_GetPropertyStr(ctx, argv[1], "end");
            JSValue js_st    = JS_GetPropertyStr(ctx, argv[1], "startTime");
            JSValue js_dur   = JS_GetPropertyStr(ctx, argv[1], "duration");
            if (JS_IsString(js_start)) {
                const char* s = JS_ToCString(ctx, js_start);
                if (s) { start_time = find_mark_time(s); JS_FreeCString(ctx, s); }
            } else if (JS_IsNumber(js_st)) {
                JS_ToFloat64(ctx, &start_time, js_st);
            }
            if (JS_IsString(js_end)) {
                const char* s = JS_ToCString(ctx, js_end);
                if (s) { end_time = find_mark_time(s); JS_FreeCString(ctx, s); }
            } else if (JS_IsNumber(js_dur)) {
                double dur = 0.0;
                JS_ToFloat64(ctx, &dur, js_dur);
                end_time = start_time + dur;
            }
            JS_FreeValue(ctx, js_start);
            JS_FreeValue(ctx, js_end);
            JS_FreeValue(ctx, js_st);
            JS_FreeValue(ctx, js_dur);
        }
    }

    if (argc >= 3 && JS_IsString(argv[2])) {
        const char* em = JS_ToCString(ctx, argv[2]);
        if (em) { end_time = find_mark_time(em); JS_FreeCString(ctx, em); }
    }

    PerformanceEntry entry;
    entry.name       = name;
    entry.entry_type = "measure";
    entry.start_time = start_time;
    entry.duration   = end_time - start_time;

    if (state) state->entries.push_back(entry);

    return make_perf_entry_obj(ctx, entry);
}

// performance.getEntries() — returns all entries
static JSValue js_performance_get_entries(JSContext* ctx, JSValueConst /*this_val*/,
                                           int /*argc*/, JSValueConst* /*argv*/) {
    JSValue arr = JS_NewArray(ctx);
    auto* state = get_perf_state(ctx);
    if (!state) return arr;
    uint32_t idx = 0;
    for (const auto& e : state->entries) {
        JS_SetPropertyUint32(ctx, arr, idx++, make_perf_entry_obj(ctx, e));
    }
    return arr;
}

// performance.getEntriesByType(type)
static JSValue js_performance_get_entries_by_type(JSContext* ctx, JSValueConst /*this_val*/,
                                                    int argc, JSValueConst* argv) {
    JSValue arr = JS_NewArray(ctx);
    if (argc < 1 || !JS_IsString(argv[0])) return arr;
    const char* type_cstr = JS_ToCString(ctx, argv[0]);
    if (!type_cstr) return arr;
    std::string type(type_cstr);
    JS_FreeCString(ctx, type_cstr);
    auto* state = get_perf_state(ctx);
    if (!state) return arr;
    uint32_t idx = 0;
    for (const auto& e : state->entries) {
        if (e.entry_type == type)
            JS_SetPropertyUint32(ctx, arr, idx++, make_perf_entry_obj(ctx, e));
    }
    return arr;
}

// performance.getEntriesByName(name [, type])
static JSValue js_performance_get_entries_by_name(JSContext* ctx, JSValueConst /*this_val*/,
                                                    int argc, JSValueConst* argv) {
    JSValue arr = JS_NewArray(ctx);
    if (argc < 1 || !JS_IsString(argv[0])) return arr;
    const char* name_cstr = JS_ToCString(ctx, argv[0]);
    if (!name_cstr) return arr;
    std::string name(name_cstr);
    JS_FreeCString(ctx, name_cstr);
    std::string type_filter;
    if (argc >= 2 && JS_IsString(argv[1])) {
        const char* tc = JS_ToCString(ctx, argv[1]);
        if (tc) { type_filter = tc; JS_FreeCString(ctx, tc); }
    }
    auto* state = get_perf_state(ctx);
    if (!state) return arr;
    uint32_t idx = 0;
    for (const auto& e : state->entries) {
        if (e.name == name && (type_filter.empty() || e.entry_type == type_filter))
            JS_SetPropertyUint32(ctx, arr, idx++, make_perf_entry_obj(ctx, e));
    }
    return arr;
}

// performance.clearMarks([name])
static JSValue js_performance_clear_marks(JSContext* ctx, JSValueConst /*this_val*/,
                                           int argc, JSValueConst* argv) {
    auto* state = get_perf_state(ctx);
    if (!state) return JS_UNDEFINED;
    if (argc >= 1 && JS_IsString(argv[0])) {
        const char* nc = JS_ToCString(ctx, argv[0]);
        if (nc) {
            std::string name(nc);
            JS_FreeCString(ctx, nc);
            state->entries.erase(
                std::remove_if(state->entries.begin(), state->entries.end(),
                    [&](const PerformanceEntry& e) {
                        return e.entry_type == "mark" && e.name == name;
                    }),
                state->entries.end());
        }
    } else {
        state->entries.erase(
            std::remove_if(state->entries.begin(), state->entries.end(),
                [](const PerformanceEntry& e) { return e.entry_type == "mark"; }),
            state->entries.end());
    }
    return JS_UNDEFINED;
}

// performance.clearMeasures([name])
static JSValue js_performance_clear_measures(JSContext* ctx, JSValueConst /*this_val*/,
                                              int argc, JSValueConst* argv) {
    auto* state = get_perf_state(ctx);
    if (!state) return JS_UNDEFINED;
    if (argc >= 1 && JS_IsString(argv[0])) {
        const char* nc = JS_ToCString(ctx, argv[0]);
        if (nc) {
            std::string name(nc);
            JS_FreeCString(ctx, nc);
            state->entries.erase(
                std::remove_if(state->entries.begin(), state->entries.end(),
                    [&](const PerformanceEntry& e) {
                        return e.entry_type == "measure" && e.name == name;
                    }),
                state->entries.end());
        }
    } else {
        state->entries.erase(
            std::remove_if(state->entries.begin(), state->entries.end(),
                [](const PerformanceEntry& e) { return e.entry_type == "measure"; }),
            state->entries.end());
    }
    return JS_UNDEFINED;
}

// performance.clearResourceTimings() — no-op (we don't track resource entries yet)
static JSValue js_performance_clear_resource_timings(JSContext* /*ctx*/, JSValueConst /*this_val*/,
                                                      int /*argc*/, JSValueConst* /*argv*/) {
    return JS_UNDEFINED;
}

// performance.toJSON() — returns object with timeOrigin and entries
static JSValue js_performance_to_json(JSContext* ctx, JSValueConst /*this_val*/,
                                       int /*argc*/, JSValueConst* /*argv*/) {
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "timeOrigin", JS_NewFloat64(ctx, page_time_origin_ms));
    // Include current entries array
    auto* state = get_perf_state(ctx);
    JSValue arr = JS_NewArray(ctx);
    if (state) {
        uint32_t idx = 0;
        for (const auto& e : state->entries) {
            JS_SetPropertyUint32(ctx, arr, idx++, make_perf_entry_obj(ctx, e));
        }
    }
    JS_SetPropertyStr(ctx, obj, "entries", arr);
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

static JSValue js_request_animation_frame(JSContext* ctx, JSValueConst /*this_val*/,
                                           int argc, JSValueConst* argv) {
    if (argc < 1 || !JS_IsFunction(ctx, argv[0]))
        return JS_NewInt32(ctx, 0);

    auto* state = get_raf_state(ctx);
    if (!state) return JS_NewInt32(ctx, 0);

    int id = state->next_id++;

    // Queue the callback for the next flush_animation_frames() call.
    RAFEntry raf_entry;
    raf_entry.id = id;
    raf_entry.callback = JS_DupValue(ctx, argv[0]);
    raf_entry.cancelled = false;
    state->entries.push_back(std::move(raf_entry));

    return JS_NewInt32(ctx, id);
}

static JSValue js_cancel_animation_frame(JSContext* ctx, JSValueConst /*this_val*/,
                                          int argc, JSValueConst* argv) {
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
// queueMicrotask(fn) -- schedule a microtask via JS_EnqueueJob
// =========================================================================

// Job function: called by the QuickJS job queue with the callback as argv[0].
static JSValue microtask_job(JSContext* ctx, int argc, JSValueConst* argv) {
    if (argc < 1 || !JS_IsFunction(ctx, argv[0])) return JS_UNDEFINED;
    JSValue ret = JS_Call(ctx, argv[0], JS_UNDEFINED, 0, nullptr);
    if (JS_IsException(ret)) {
        // Unhandled exception in microtask: print to console and clear.
        JSValue exc = JS_GetException(ctx);
        const char* str = JS_ToCString(ctx, exc);
        if (str) {
            fprintf(stderr, "[queueMicrotask] Uncaught: %s\n", str);
            JS_FreeCString(ctx, str);
        }
        JS_FreeValue(ctx, exc);
        return JS_UNDEFINED;
    }
    JS_FreeValue(ctx, ret);
    return JS_UNDEFINED;
}

static JSValue js_queue_microtask(JSContext* ctx, JSValueConst /*this_val*/,
                                   int argc, JSValueConst* argv) {
    if (argc < 1 || !JS_IsFunction(ctx, argv[0]))
        return JS_ThrowTypeError(ctx, "queueMicrotask requires a function argument");

    // Use JS_EnqueueJob so the callback runs after the current synchronous
    // task completes (i.e., after the current JS_Eval / JS_Call returns) but
    // before the next macrotask — matching the spec's microtask checkpoint.
    if (JS_EnqueueJob(ctx, microtask_job, 1, argv) < 0) {
        // Fallback: run immediately if enqueue fails (memory pressure etc.)
        JSValue ret = JS_Call(ctx, argv[0], JS_UNDEFINED, 0, nullptr);
        JS_FreeValue(ctx, ret);
    }
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
// window.prompt(msg, default) -- stub, always returns null
// =========================================================================

static JSValue js_window_prompt(JSContext* /*ctx*/, JSValueConst /*this_val*/,
                                 int /*argc*/, JSValueConst* /*argv*/) {
    return JS_NULL;
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

static std::string trim_ascii(const std::string& value) {
    size_t start = 0;
    while (start < value.size() &&
           std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }
    if (start == value.size()) return "";

    size_t end = value.size();
    while (end > start &&
           std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(start, end - start);
}

static std::string to_lower_ascii(const std::string& value) {
    std::string lower = value;
    for (char& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return lower;
}

static std::string js_value_to_string(JSContext* ctx, JSValueConst value) {
    const char* cstr = JS_ToCString(ctx, value);
    if (!cstr) return "";
    std::string out(cstr);
    JS_FreeCString(ctx, cstr);
    return out;
}

// =========================================================================
// window.getComputedStyle(element [, pseudoElement])
// =========================================================================

// js_window_get_computed_style was removed: getComputedStyle is now
// installed by install_dom_bindings() with full LayoutRect-backed CSS values.

static int read_viewport_dimension(JSContext* ctx, const char* name, int fallback) {
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue val = JS_GetPropertyStr(ctx, global, name);
    int result = fallback;
    if (JS_IsNumber(val)) {
        int32_t parsed = 0;
        if (JS_ToInt32(ctx, &parsed, val) == 0 && parsed > 0) {
            result = parsed;
        }
    }
    JS_FreeValue(ctx, val);
    JS_FreeValue(ctx, global);
    return result;
}

static bool parse_px_clause_value(const std::string& text, int* out) {
    std::string value = trim_ascii(text);
    if (value.size() < 3) return false;

    size_t pos = 0;
    while (pos < value.size() &&
           std::isspace(static_cast<unsigned char>(value[pos]))) {
        ++pos;
    }
    if (pos >= value.size() || !std::isdigit(static_cast<unsigned char>(value[pos]))) {
        return false;
    }

    int parsed = 0;
    while (pos < value.size() &&
           std::isdigit(static_cast<unsigned char>(value[pos]))) {
        parsed = parsed * 10 + (value[pos] - '0');
        ++pos;
    }
    while (pos < value.size() &&
           std::isspace(static_cast<unsigned char>(value[pos]))) {
        ++pos;
    }
    if (pos + 1 >= value.size() ||
        value[pos] != 'p' || value[pos + 1] != 'x') {
        return false;
    }
    *out = parsed;
    return true;
}

static int read_prefers_color_scheme_hint(JSContext* ctx) {
    int scheme = 1; // Default to light
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue scheme_value = JS_GetPropertyStr(ctx, global, "__vibrowser_prefers_color_scheme");
    if (JS_IsNumber(scheme_value)) {
        int32_t parsed = 1;
        if (JS_ToInt32(ctx, &parsed, scheme_value) == 0) {
            if (parsed == 2) scheme = 2;
        }
    }
    JS_FreeValue(ctx, scheme_value);
    JS_FreeValue(ctx, global);
    return scheme;
}

static bool evaluate_media_clause(const std::string& clause, JSContext* ctx,
                                  int viewport_width, int viewport_height,
                                  bool* known_clause) {
    std::string lower = to_lower_ascii(trim_ascii(clause));
    *known_clause = true;

    auto colon_pos = lower.find(':');
    if (colon_pos == std::string::npos) {
        *known_clause = false;
        return false;
    }

    std::string feature = trim_ascii(lower.substr(0, colon_pos));
    std::string value = trim_ascii(lower.substr(colon_pos + 1));

    if (feature == "prefers-color-scheme") {
        int scheme = read_prefers_color_scheme_hint(ctx);
        if (value == "dark") return scheme == 2;
        if (value == "light") return scheme == 1;
        *known_clause = false;
        return false;
    }

    if (feature == "max-width") {
        int px = 0;
        if (!parse_px_clause_value(value, &px)) return false;
        return viewport_width <= px;
    }

    if (feature == "min-width") {
        int px = 0;
        if (!parse_px_clause_value(value, &px)) return false;
        return viewport_width >= px;
    }

    if (feature == "min-height") {
        int px = 0;
        if (!parse_px_clause_value(value, &px)) return false;
        return viewport_height >= px;
    }

    if (feature == "max-height") {
        int px = 0;
        if (!parse_px_clause_value(value, &px)) return false;
        return viewport_height <= px;
    }

    if (feature == "orientation") {
        if (value == "portrait") return viewport_height >= viewport_width;
        if (value == "landscape") return viewport_width > viewport_height;
        *known_clause = false;
        return false;
    }

    // prefers-reduced-motion: assume no preference (motion is fine)
    if (feature == "prefers-reduced-motion") {
        if (value == "no-preference") return true;
        if (value == "reduce") return false;
        *known_clause = false;
        return false;
    }

    // prefers-contrast: assume no-preference
    if (feature == "prefers-contrast") {
        if (value == "no-preference") return true;
        if (value == "more" || value == "less" || value == "forced") return false;
        *known_clause = false;
        return false;
    }

    // prefers-reduced-data: assume no-preference
    if (feature == "prefers-reduced-data") {
        if (value == "no-preference") return true;
        if (value == "reduce") return false;
        *known_clause = false;
        return false;
    }

    // prefers-transparency: no-preference
    if (feature == "prefers-transparency") {
        if (value == "no-preference") return true;
        if (value == "reduce") return false;
        *known_clause = false;
        return false;
    }

    // forced-colors: none (no forced colors)
    if (feature == "forced-colors") {
        if (value == "none") return true;
        if (value == "active") return false;
        *known_clause = false;
        return false;
    }

    // inverted-colors: none
    if (feature == "inverted-colors") {
        if (value == "none") return true;
        if (value == "inverted") return false;
        *known_clause = false;
        return false;
    }

    // display-mode: browser
    if (feature == "display-mode") {
        if (value == "browser") return true;
        if (value == "fullscreen" || value == "standalone" || value == "minimal-ui") return false;
        *known_clause = false;
        return false;
    }

    // hover: hover (we have a mouse)
    if (feature == "hover") {
        if (value == "hover") return true;
        if (value == "none") return false;
        *known_clause = false;
        return false;
    }

    // any-hover: hover
    if (feature == "any-hover") {
        if (value == "hover") return true;
        if (value == "none") return false;
        *known_clause = false;
        return false;
    }

    // pointer: fine (mouse)
    if (feature == "pointer") {
        if (value == "fine") return true;
        if (value == "coarse" || value == "none") return false;
        *known_clause = false;
        return false;
    }

    // any-pointer: fine
    if (feature == "any-pointer") {
        if (value == "fine") return true;
        if (value == "coarse" || value == "none") return false;
        *known_clause = false;
        return false;
    }

    // color: assume 8-bit color (truthy)
    if (feature == "color") {
        if (value.empty() || value == "8" || value == "1") return true;
        *known_clause = false;
        return false;
    }

    // color-gamut: srgb
    if (feature == "color-gamut") {
        if (value == "srgb") return true;
        if (value == "p3" || value == "rec2020") return false;
        *known_clause = false;
        return false;
    }

    // monochrome: 0 (not monochrome)
    if (feature == "monochrome") {
        if (value == "0") return true;
        *known_clause = false;
        return false;
    }

    // scripting: enabled
    if (feature == "scripting") {
        if (value == "enabled") return true;
        if (value == "none" || value == "initial-only") return false;
        *known_clause = false;
        return false;
    }

    // update: fast
    if (feature == "update") {
        if (value == "fast") return true;
        if (value == "slow" || value == "none") return false;
        *known_clause = false;
        return false;
    }

    // overflow-block: scroll
    if (feature == "overflow-block") {
        if (value == "scroll" || value == "optional-paged") return true;
        if (value == "none" || value == "paged") return false;
        *known_clause = false;
        return false;
    }

    // overflow-inline: scroll
    if (feature == "overflow-inline") {
        if (value == "scroll") return true;
        if (value == "none") return false;
        *known_clause = false;
        return false;
    }

    // resolution: treat as known but unknown value (return false for non-1dppx)
    if (feature == "resolution" || feature == "min-resolution" || feature == "max-resolution") {
        // We assume 96dpi/1dppx
        *known_clause = true;
        if (value.find("dppx") != std::string::npos) {
            try { float v = std::stof(value); return (feature == "max-resolution") ? (1.0f <= v) : (1.0f >= v); } catch (...) {}
        }
        return false;
    }

    // aspect-ratio: compute from viewport
    if (feature == "aspect-ratio" || feature == "min-aspect-ratio" || feature == "max-aspect-ratio") {
        // Parse "W/H" format
        auto slash = value.find('/');
        if (slash != std::string::npos) {
            try {
                float w = std::stof(value.substr(0, slash));
                float h = std::stof(value.substr(slash + 1));
                if (h <= 0) { *known_clause = false; return false; }
                float query_ratio = w / h;
                float vp_ratio = (viewport_height > 0) ? (float)viewport_width / viewport_height : 1.0f;
                if (feature == "aspect-ratio") return std::fabs(vp_ratio - query_ratio) < 0.01f;
                if (feature == "min-aspect-ratio") return vp_ratio >= query_ratio;
                if (feature == "max-aspect-ratio") return vp_ratio <= query_ratio;
            } catch (...) {}
        }
        *known_clause = false;
        return false;
    }

    *known_clause = false;
    return false;
}

static bool evaluate_media_group(const std::string& group, JSContext* ctx,
                                 int viewport_width, int viewport_height,
                                 bool* known_group) {
    *known_group = false;
    bool all_match = true;
    size_t pos = 0;
    while (true) {
        size_t open = group.find('(', pos);
        if (open == std::string::npos) break;
        size_t close = group.find(')', open + 1);
        if (close == std::string::npos) break;

        std::string clause = group.substr(open + 1, close - open - 1);
        bool known_clause = false;
        bool clause_match = evaluate_media_clause(
            clause, ctx, viewport_width, viewport_height, &known_clause);
        if (!known_clause) {
            *known_group = true;
            return false;
        }
        all_match = all_match && clause_match;
        *known_group = true;
        pos = close + 1;
    }

    // Allow bare feature syntax without parentheses.
    if (!*known_group) {
        bool known_clause = false;
        bool clause_match = evaluate_media_clause(
            group, ctx, viewport_width, viewport_height, &known_clause);
        *known_group = known_clause;
        return known_clause ? clause_match : false;
    }

    return all_match;
}

static JSValue js_media_query_noop(JSContext* /*ctx*/, JSValueConst /*this_val*/,
                                   int /*argc*/, JSValueConst* /*argv*/) {
    return JS_UNDEFINED;
}

// =========================================================================
// window.matchMedia(query)
// =========================================================================

static JSValue js_window_match_media(JSContext* ctx, JSValueConst /*this_val*/,
                                     int argc, JSValueConst* argv) {
    std::string query;
    if (argc >= 1) {
        query = js_value_to_string(ctx, argv[0]);
    }

    int viewport_width = read_viewport_dimension(ctx, "innerWidth", 1024);
    int viewport_height = read_viewport_dimension(ctx, "innerHeight", 768);

    bool matches = false;
    bool found_known_group = false;
    size_t pos = 0;
    while (true) {
        size_t comma = query.find(',', pos);
        std::string group = (comma == std::string::npos)
            ? query.substr(pos)
            : query.substr(pos, comma - pos);
        bool known_group = false;
        bool group_match = evaluate_media_group(
            group, ctx, viewport_width, viewport_height, &known_group);
        if (known_group) {
            found_known_group = true;
            if (group_match) {
                matches = true;
                break;
            }
        }
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
    if (!found_known_group) matches = false;

    JSValue result = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, result, "matches", matches ? JS_TRUE : JS_FALSE);
    JS_SetPropertyStr(ctx, result, "media", JS_NewString(ctx, query.c_str()));
    JS_SetPropertyStr(ctx, result, "onchange", JS_NULL);
    JS_SetPropertyStr(ctx, result, "addListener",
        JS_NewCFunction(ctx, js_media_query_noop, "addListener", 1));
    JS_SetPropertyStr(ctx, result, "removeListener",
        JS_NewCFunction(ctx, js_media_query_noop, "removeListener", 1));
    JS_SetPropertyStr(ctx, result, "addEventListener",
        JS_NewCFunction(ctx, js_media_query_noop, "addEventListener", 2));
    JS_SetPropertyStr(ctx, result, "removeEventListener",
        JS_NewCFunction(ctx, js_media_query_noop, "removeEventListener", 2));
    JS_SetPropertyStr(ctx, result, "dispatchEvent",
        JS_NewCFunction(ctx, js_media_query_noop, "dispatchEvent", 1));
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
    if (next_x < 0.0) next_x = 0.0;
    if (next_y < 0.0) next_y = 0.0;
    js_write_window_scroll_state(ctx, global, next_x, next_y);
    JS_FreeValue(ctx, global);
    // Signal the browser shell to update the viewport scroll position
    clever::js::set_pending_scroll(ctx, next_x, next_y);
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
    if (next_x < 0.0) next_x = 0.0;
    if (next_y < 0.0) next_y = 0.0;
    js_write_window_scroll_state(ctx, global, next_x, next_y);
    JS_FreeValue(ctx, global);
    // Signal the browser shell to update the viewport scroll position
    clever::js::set_pending_scroll(ctx, next_x, next_y);
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

// =========================================================================
// History API — session history management
// =========================================================================

struct HistoryEntry {
    std::string state;   // JSON-serialized state; empty means null
    std::string title;   // unused by browsers but part of spec
    std::string url;     // URL path component for this entry
};

// Per-context history state.  Stored as opaque pointer on the global object
// under "__history_state_ptr", same pattern as RAFState.
struct HistoryState {
    std::vector<HistoryEntry> entries;
    int current_index = 0;
};

static HistoryState* get_history_state(JSContext* ctx) {
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue val = JS_GetPropertyStr(ctx, global, "__history_state_ptr");
    HistoryState* hs = nullptr;
    if (JS_IsNumber(val)) {
        int64_t ptr_val = 0;
        JS_ToInt64(ctx, &ptr_val, val);
        hs = reinterpret_cast<HistoryState*>(static_cast<uintptr_t>(ptr_val));
    }
    JS_FreeValue(ctx, val);
    JS_FreeValue(ctx, global);
    return hs;
}

static bool history_serialize_state(JSContext* ctx, JSValueConst state_val,
                                    std::string& out_serialized_state) {
    out_serialized_state.clear();
    if (JS_IsUndefined(state_val) || JS_IsNull(state_val)) {
        return true;
    }

    const char* stringify_src = "(function(v){ return JSON.stringify(v); })";
    JSValue stringify_fn = JS_Eval(ctx, stringify_src, std::strlen(stringify_src),
                                   "<history-stringify>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(stringify_fn)) {
        JS_FreeValue(ctx, stringify_fn);
        return false;
    }
    if (!JS_IsFunction(ctx, stringify_fn)) {
        JS_FreeValue(ctx, stringify_fn);
        JS_ThrowTypeError(ctx, "JSON.stringify is not available");
        return false;
    }

    JSValue arg = JS_DupValue(ctx, state_val);
    JSValue result = JS_Call(ctx, stringify_fn, JS_UNDEFINED, 1, &arg);
    JS_FreeValue(ctx, arg);
    JS_FreeValue(ctx, stringify_fn);
    if (JS_IsException(result)) {
        return false;
    }
    if (JS_IsUndefined(result) || JS_IsNull(result)) {
        JS_FreeValue(ctx, result);
        return true;
    }

    const char* serialized = JS_ToCString(ctx, result);
    if (!serialized) {
        JS_FreeValue(ctx, result);
        return false;
    }
    out_serialized_state = serialized;
    JS_FreeCString(ctx, serialized);
    JS_FreeValue(ctx, result);
    return true;
}

static JSValue history_deserialize_state(JSContext* ctx,
                                         const std::string& serialized_state) {
    if (serialized_state.empty()) return JS_NULL;

    const char* parse_src = "(function(v){ return JSON.parse(v); })";
    JSValue parse_fn = JS_Eval(ctx, parse_src, std::strlen(parse_src),
                               "<history-parse>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(parse_fn) || !JS_IsFunction(ctx, parse_fn)) {
        if (JS_IsException(parse_fn)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, parse_fn);
        return JS_NULL;
    }

    JSValue arg = JS_NewString(ctx, serialized_state.c_str());
    JSValue result = JS_Call(ctx, parse_fn, JS_UNDEFINED, 1, &arg);
    JS_FreeValue(ctx, arg);
    JS_FreeValue(ctx, parse_fn);
    if (JS_IsException(result)) {
        JSValue exc = JS_GetException(ctx);
        JS_FreeValue(ctx, exc);
        return JS_NULL;
    }
    return result;
}

// Helper: check if a string starts with a given character
static inline bool str_starts_with_char(const std::string& s, char c) {
    return !s.empty() && s[0] == c;
}

// Resolve a URL (relative path, absolute path, or full URL) against the
// current location's full href and return the resolved full URL.
static std::string history_resolve_url(JSContext* ctx, const std::string& url) {
    if (url.empty()) return url;

    // If it's already an absolute URL (has scheme), return as-is
    if (url.find("://") != std::string::npos) return url;

    // Get the current location href to resolve relative URLs against
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue location = JS_GetPropertyStr(ctx, global, "location");
    std::string base_href;
    if (JS_IsObject(location)) {
        JSValue href_val = JS_GetPropertyStr(ctx, location, "href");
        if (JS_IsString(href_val)) {
            const char* href_cstr = JS_ToCString(ctx, href_val);
            if (href_cstr) {
                base_href = href_cstr;
                JS_FreeCString(ctx, href_cstr);
            }
        }
        JS_FreeValue(ctx, href_val);
    }
    JS_FreeValue(ctx, location);
    JS_FreeValue(ctx, global);

    if (base_href.empty()) return url;

    ParsedURL base = parse_url(base_href);

    if (str_starts_with_char(url, '/')) {
        // Absolute path: resolve against origin
        return base.origin + url;
    } else if (str_starts_with_char(url, '#')) {
        // Hash-only: append to base path+search (strip existing hash)
        std::string without_hash = base_href;
        auto hash_pos = without_hash.find('#');
        if (hash_pos != std::string::npos) without_hash = without_hash.substr(0, hash_pos);
        return without_hash + url;
    } else if (str_starts_with_char(url, '?')) {
        // Query-only: append to base path
        return base.origin + base.pathname + url;
    } else {
        // Relative path: resolve against base directory
        std::string base_dir = base.origin + base.pathname;
        auto last_slash = base_dir.rfind('/');
        if (last_slash != std::string::npos) {
            return base_dir.substr(0, last_slash + 1) + url;
        }
        return base_dir + "/" + url;
    }
}

// Update window.location with the new URL. This triggers the location object's
// JS setter (if present), which updates all sub-properties (pathname, search,
// hash, etc.) atomically. Falls back to updating href directly if needed.
static void history_update_displayed_url(JSContext* ctx, const std::string& url) {
    if (url.empty()) return;

    JSValue global = JS_GetGlobalObject(ctx);

    // Use JS evaluation to trigger the location setter properly.
    // This ensures pathname, search, hash etc. are all updated.
    // We pass the URL as a JS string and let the location setter resolve
    // relative URLs against the current origin.
    JSValue js_url = JS_NewString(ctx, url.c_str());
    JS_SetPropertyStr(ctx, global, "__history_new_url__", js_url);

    const char* update_src = "(function(){"
        "var u = __history_new_url__; delete __history_new_url__;"
        "if (typeof location !== 'undefined' && location) {"
        "  try { location.href = u; } catch(e) {}"
        "}"
        "})()";
    JSValue ret = JS_Eval(ctx, update_src, std::strlen(update_src),
                          "<history-update-url>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(ret)) {
        JSValue exc = JS_GetException(ctx);
        JS_FreeValue(ctx, exc);
        // Fallback: set href directly
        JSValue location = JS_GetPropertyStr(ctx, global, "location");
        if (JS_IsObject(location)) {
            JS_SetPropertyStr(ctx, location, "href", JS_NewString(ctx, url.c_str()));
        }
        JS_FreeValue(ctx, location);
    }
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, global);
}

static void fire_popstate_event(JSContext* ctx, JSValue state_val) {
    JSValue global = JS_GetGlobalObject(ctx);

    // Build the PopStateEvent object.
    // Note: JS_SetPropertyStr "steals" the value reference, so we must
    // JS_DupValue for any value we set as a property AND use afterwards.
    JSValue event = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, event, "type", JS_NewString(ctx, "popstate"));
    JS_SetPropertyStr(ctx, event, "state", JS_DupValue(ctx, state_val));
    JS_SetPropertyStr(ctx, event, "bubbles", JS_TRUE);
    JS_SetPropertyStr(ctx, event, "cancelable", JS_FALSE);
    JS_SetPropertyStr(ctx, event, "defaultPrevented", JS_FALSE);
    // Dup global for each property since SetPropertyStr steals the reference
    JS_SetPropertyStr(ctx, event, "target", JS_DupValue(ctx, global));
    JS_SetPropertyStr(ctx, event, "currentTarget", JS_DupValue(ctx, global));
    JS_SetPropertyStr(ctx, event, "eventPhase", JS_NewInt32(ctx, 2)); // AT_TARGET

    // 1. Call window.onpopstate if set
    JSValue handler = JS_GetPropertyStr(ctx, global, "onpopstate");
    if (JS_IsFunction(ctx, handler)) {
        JSValue result = JS_Call(ctx, handler, global, 1, &event);
        if (JS_IsException(result)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, result);
    }
    JS_FreeValue(ctx, handler);

    // 2. Dispatch to window.addEventListener('popstate', ...) listeners.
    // Call directly into js_dom_bindings which holds the real listener registry.
    dispatch_window_listeners(ctx, "popstate", event);

    JS_FreeValue(ctx, event);
    JS_FreeValue(ctx, global);
}

// history.pushState(state, title, url)
static JSValue js_history_push_state(JSContext* ctx, JSValueConst /*this_val*/,
                                      int argc, JSValueConst* argv) {
    auto* hs = get_history_state(ctx);
    if (!hs) return JS_UNDEFINED;

    std::string serialized_state;
    if (argc >= 1 && !history_serialize_state(ctx, argv[0], serialized_state)) {
        return JS_EXCEPTION;
    }

    std::string title;
    if (argc >= 2 && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
        const char* t = JS_ToCString(ctx, argv[1]);
        if (!t) return JS_EXCEPTION;
        title = t;
        JS_FreeCString(ctx, t);
    }

    bool has_url_arg = argc >= 3 && !JS_IsUndefined(argv[2]) && !JS_IsNull(argv[2]);
    std::string url;
    if (has_url_arg) {
        const char* u = JS_ToCString(ctx, argv[2]);
        if (!u) return JS_EXCEPTION;
        // Resolve the URL against the current location (handles relative URLs)
        url = history_resolve_url(ctx, std::string(u));
        JS_FreeCString(ctx, u);
    } else {
        if (hs->current_index >= 0 &&
            hs->current_index < static_cast<int>(hs->entries.size())) {
            url = hs->entries[hs->current_index].url;
        } else {
            url = "/";
        }
    }

    // Truncate any forward entries (per spec: pushState removes forward history)
    if (hs->current_index >= 0 &&
        hs->current_index < static_cast<int>(hs->entries.size()) - 1) {
        hs->entries.resize(hs->current_index + 1);
    }

    // Push new entry
    hs->entries.push_back({serialized_state, title, url});
    hs->current_index = static_cast<int>(hs->entries.size()) - 1;

    // Update displayed URL when provided.
    // Per spec: pushState does NOT fire popstate.
    if (has_url_arg) {
        history_update_displayed_url(ctx, url);
    }

    return JS_UNDEFINED;
}

// history.replaceState(state, title, url)
static JSValue js_history_replace_state(JSContext* ctx, JSValueConst /*this_val*/,
                                         int argc, JSValueConst* argv) {
    auto* hs = get_history_state(ctx);
    if (!hs) return JS_UNDEFINED;
    if (hs->entries.empty()) {
        hs->entries.push_back({"", "", "/"});
        hs->current_index = 0;
    }
    if (hs->current_index < 0 ||
        hs->current_index >= static_cast<int>(hs->entries.size())) {
        hs->current_index = static_cast<int>(hs->entries.size()) - 1;
    }

    auto& current = hs->entries[hs->current_index];
    std::string serialized_state;
    if (argc >= 1 && !history_serialize_state(ctx, argv[0], serialized_state)) {
        return JS_EXCEPTION;
    }
    current.state = serialized_state;

    if (argc >= 2 && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
        const char* t = JS_ToCString(ctx, argv[1]);
        if (!t) return JS_EXCEPTION;
        current.title = t;
        JS_FreeCString(ctx, t);
    }

    if (argc >= 3 && !JS_IsUndefined(argv[2]) && !JS_IsNull(argv[2])) {
        const char* u = JS_ToCString(ctx, argv[2]);
        if (!u) return JS_EXCEPTION;
        // Resolve the URL against the current location (handles relative URLs)
        current.url = history_resolve_url(ctx, std::string(u));
        JS_FreeCString(ctx, u);
        history_update_displayed_url(ctx, current.url);
    }

    return JS_UNDEFINED;
}

// history.back()
static JSValue js_history_back(JSContext* ctx, JSValueConst /*this_val*/,
                                int /*argc*/, JSValueConst* /*argv*/) {
    auto* hs = get_history_state(ctx);
    if (!hs || hs->current_index <= 0) return JS_UNDEFINED;

    hs->current_index--;
    auto& entry = hs->entries[hs->current_index];

    history_update_displayed_url(ctx, entry.url);
    JSValue state = history_deserialize_state(ctx, entry.state);
    fire_popstate_event(ctx, state);
    JS_FreeValue(ctx, state);
    return JS_UNDEFINED;
}

// history.forward()
static JSValue js_history_forward(JSContext* ctx, JSValueConst /*this_val*/,
                                   int /*argc*/, JSValueConst* /*argv*/) {
    auto* hs = get_history_state(ctx);
    if (!hs || hs->current_index >= static_cast<int>(hs->entries.size()) - 1)
        return JS_UNDEFINED;

    hs->current_index++;
    auto& entry = hs->entries[hs->current_index];

    history_update_displayed_url(ctx, entry.url);
    JSValue state = history_deserialize_state(ctx, entry.state);
    fire_popstate_event(ctx, state);
    JS_FreeValue(ctx, state);
    return JS_UNDEFINED;
}

// history.go(delta)
static JSValue js_history_go(JSContext* ctx, JSValueConst /*this_val*/,
                              int argc, JSValueConst* argv) {
    auto* hs = get_history_state(ctx);
    if (!hs) return JS_UNDEFINED;

    int delta = 0;
    if (argc >= 1 && !JS_IsUndefined(argv[0])) {
        if (JS_ToInt32(ctx, &delta, argv[0]) < 0) {
            return JS_EXCEPTION;
        }
    }

    if (delta == 0) {
        // go(0) is equivalent to reload — no-op in our implementation
        return JS_UNDEFINED;
    }

    int new_index = hs->current_index + delta;
    if (new_index < 0 || new_index >= static_cast<int>(hs->entries.size()))
        return JS_UNDEFINED;

    hs->current_index = new_index;
    auto& entry = hs->entries[hs->current_index];

    history_update_displayed_url(ctx, entry.url);
    JSValue state = history_deserialize_state(ctx, entry.state);
    fire_popstate_event(ctx, state);
    JS_FreeValue(ctx, state);
    return JS_UNDEFINED;
}

// history.length getter
static JSValue js_history_get_length(JSContext* ctx, JSValueConst /*this_val*/,
                                      int /*argc*/, JSValueConst* /*argv*/) {
    auto* hs = get_history_state(ctx);
    if (!hs) return JS_NewInt32(ctx, 0);
    return JS_NewInt32(ctx, static_cast<int>(hs->entries.size()));
}

// history.state getter
static JSValue js_history_get_state(JSContext* ctx, JSValueConst /*this_val*/,
                                     int /*argc*/, JSValueConst* /*argv*/) {
    auto* hs = get_history_state(ctx);
    if (!hs || hs->current_index < 0 ||
        hs->current_index >= static_cast<int>(hs->entries.size()))
        return JS_NULL;
    return history_deserialize_state(ctx, hs->entries[hs->current_index].state);
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
        // Note: We skip JS_FreeContext/JS_FreeRuntime because QuickJS
        // asserts gc_obj_list is empty but worker global objects (self=global
        // circular ref, console, closures) cannot be fully cleaned up.
        // The OS reclaims all memory on process exit.
        state->worker_ctx = nullptr;
        state->worker_rt = nullptr;
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

    // Skip JS_FreeContext/JS_FreeRuntime — see comment in finalizer
    state->worker_ctx = nullptr;
    state->worker_rt = nullptr;

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

    // Comprehensive set of all CSS properties that vibrowser parses.
    // Sourced from every `prop ==` and `d.property ==` check in
    // style_resolver.cpp and render_pipeline.cpp.
    static const std::unordered_set<std::string> known_properties = {
        // Box model
        "all",
        "box-sizing", "box-decoration-break", "-webkit-box-decoration-break",
        "width", "height",
        "min-width", "max-width", "min-height", "max-height",
        "min-inline-size", "max-inline-size",
        "min-block-size", "max-block-size",
        "inline-size", "block-size",
        "aspect-ratio",
        // Margin
        "margin", "margin-top", "margin-right", "margin-bottom", "margin-left",
        "margin-block", "margin-block-start", "margin-block-end",
        "margin-inline", "margin-inline-start", "margin-inline-end",
        "margin-trim",
        // Padding
        "padding", "padding-top", "padding-right", "padding-bottom", "padding-left",
        "padding-block", "padding-block-start", "padding-block-end",
        "padding-inline", "padding-inline-start", "padding-inline-end",
        // Inset / positioning
        "inset", "inset-block", "inset-inline",
        "inset-block-start", "inset-block-end",
        "inset-inline-start", "inset-inline-end",
        "top", "right", "bottom", "left",
        "position",
        "z-index",
        // Display / layout
        "display",
        "float", "clear",
        "overflow", "overflow-x", "overflow-y",
        "overflow-block", "overflow-inline",
        "overflow-anchor", "overflow-clip-margin",
        "overscroll-behavior", "overscroll-behavior-x", "overscroll-behavior-y",
        "visibility",
        "contain", "contain-intrinsic-size",
        "contain-intrinsic-width", "contain-intrinsic-height",
        "content-visibility",
        "resize",
        // Color / background
        "color",
        "background", "background-color", "background-image",
        "background-size", "background-repeat",
        "background-position", "background-position-x", "background-position-y",
        "background-clip", "-webkit-background-clip",
        "background-origin", "background-attachment", "background-blend-mode",
        "color-scheme", "forced-color-adjust",
        "print-color-adjust", "-webkit-print-color-adjust",
        "color-interpolation",
        // Opacity / blending / isolation
        "opacity",
        "mix-blend-mode", "isolation",
        // Border
        "border", "border-top", "border-right", "border-bottom", "border-left",
        "border-block", "border-block-start", "border-block-end",
        "border-inline-start", "border-inline-end",
        "border-color",
        "border-top-color", "border-right-color",
        "border-bottom-color", "border-left-color",
        "border-block-start-color", "border-block-end-color",
        "border-inline-start-color", "border-inline-end-color",
        "border-block-color", "border-inline-color",
        "border-style",
        "border-top-style", "border-right-style",
        "border-bottom-style", "border-left-style",
        "border-block-start-style", "border-block-end-style",
        "border-inline-start-style", "border-inline-end-style",
        "border-block-style", "border-inline-style",
        "border-width",
        "border-top-width", "border-right-width",
        "border-bottom-width", "border-left-width",
        "border-block-start-width", "border-block-end-width",
        "border-inline-start-width", "border-inline-end-width",
        "border-block-width", "border-inline-width",
        "border-radius",
        "border-top-left-radius", "border-top-right-radius",
        "border-bottom-left-radius", "border-bottom-right-radius",
        "border-start-start-radius", "border-start-end-radius",
        "border-end-start-radius", "border-end-end-radius",
        "border-image",
        "border-image-source", "border-image-slice",
        "border-image-width", "border-image-outset", "border-image-repeat",
        "border-collapse", "border-spacing",
        // Outline
        "outline", "outline-width", "outline-color", "outline-style", "outline-offset",
        // Box shadow
        "box-shadow",
        // Table
        "table-layout", "caption-side", "empty-cells",
        // Font
        "font", "font-size", "font-family", "font-weight", "font-style",
        "font-variant", "font-variant-caps", "font-variant-numeric",
        "font-variant-ligatures", "font-variant-east-asian",
        "font-variant-alternates", "font-variant-position",
        "font-feature-settings", "font-variation-settings",
        "font-optical-sizing", "font-kerning",
        "font-synthesis", "font-stretch", "font-size-adjust",
        "font-palette", "font-language-override",
        "font-smooth", "-webkit-font-smoothing",
        // Text
        "text-align", "text-align-last",
        "text-indent", "text-transform",
        "text-decoration", "text-decoration-line",
        "text-decoration-color", "text-decoration-style",
        "text-decoration-thickness",
        "text-decoration-skip", "text-decoration-skip-ink",
        "text-shadow",
        "text-overflow",
        "text-wrap", "text-wrap-mode", "text-wrap-style",
        "text-underline-offset", "text-underline-position",
        "text-rendering",
        "text-justify",
        "text-emphasis", "text-emphasis-style",
        "text-emphasis-color", "text-emphasis-position",
        "text-size-adjust", "-webkit-text-size-adjust",
        "text-combine-upright",
        "text-orientation",
        "text-anchor",
        "text-stroke", "-webkit-text-stroke",
        "-webkit-text-stroke-width", "-webkit-text-stroke-color",
        "-webkit-text-fill-color",
        // Letter/word/line spacing
        "letter-spacing", "word-spacing", "line-height",
        // White space / wrapping
        "white-space", "white-space-collapse",
        "word-break", "overflow-wrap", "word-wrap",
        "line-break", "hyphens",
        "tab-size", "-moz-tab-size",
        // Line clamp
        "line-clamp", "-webkit-line-clamp",
        // Vertical align
        "vertical-align",
        // List
        "list-style", "list-style-type", "list-style-position", "list-style-image",
        "quotes",
        // Flex
        "flex", "flex-direction", "flex-wrap", "flex-flow",
        "flex-grow", "flex-shrink", "flex-basis",
        "order",
        "justify-content", "justify-items", "justify-self",
        "align-content", "align-items", "align-self",
        "place-content", "place-items", "place-self",
        "-webkit-box-orient",
        // Grid
        "grid", "grid-template", "grid-template-columns", "grid-template-rows",
        "grid-template-areas",
        "grid-column", "grid-row", "grid-area",
        "grid-column-start", "grid-column-end",
        "grid-row-start", "grid-row-end",
        "grid-auto-rows", "grid-auto-columns", "grid-auto-flow",
        "grid-gap", "row-gap", "grid-row-gap",
        "column-gap", "grid-column-gap",
        // Gap
        "gap",
        // Multi-column
        "columns", "column-count", "column-width",
        "column-fill", "column-gap", "column-span",
        "column-rule", "column-rule-width", "column-rule-color", "column-rule-style",
        // Transform / 3D
        "transform", "transform-origin", "transform-style", "transform-box",
        "perspective", "perspective-origin",
        "backface-visibility",
        "rotate", "scale", "translate",
        // Transition
        "transition", "transition-property", "transition-duration",
        "transition-timing-function", "transition-delay",
        "transition-behavior",
        // Animation
        "animation", "animation-name", "animation-duration",
        "animation-timing-function", "animation-delay",
        "animation-iteration-count", "animation-direction",
        "animation-fill-mode", "animation-play-state",
        "animation-composition", "animation-timeline",
        "animation-range",
        // Filter / backdrop
        "filter", "backdrop-filter", "-webkit-backdrop-filter",
        // Clip / shape / mask
        "clip-path", "clip-rule",
        "shape-outside", "shape-margin", "shape-image-threshold",
        "shape-rendering",
        "mask", "-webkit-mask",
        "mask-image", "-webkit-mask-image",
        "mask-size", "-webkit-mask-size",
        "mask-repeat", "-webkit-mask-repeat",
        "mask-composite", "-webkit-mask-composite",
        "mask-mode", "mask-origin", "-webkit-mask-origin",
        "mask-position", "-webkit-mask-position",
        "mask-clip", "-webkit-mask-clip",
        "mask-border", "mask-border-source", "mask-border-slice",
        "mask-border-width", "mask-border-outset",
        "mask-border-repeat", "mask-border-mode",
        // Object fit
        "object-fit", "object-position",
        "image-rendering", "image-orientation",
        // Scroll
        "scroll-behavior",
        "scroll-snap-type", "scroll-snap-align", "scroll-snap-stop",
        "scroll-margin",
        "scroll-margin-top", "scroll-margin-right",
        "scroll-margin-bottom", "scroll-margin-left",
        "scroll-margin-block-start", "scroll-margin-block-end",
        "scroll-margin-inline-start", "scroll-margin-inline-end",
        "scroll-padding",
        "scroll-padding-top", "scroll-padding-right",
        "scroll-padding-bottom", "scroll-padding-left",
        "scroll-padding-inline", "scroll-padding-block",
        "scrollbar-color", "scrollbar-width", "scrollbar-gutter",
        // Cursor / pointer / interaction
        "cursor", "pointer-events",
        "user-select", "-webkit-user-select",
        "touch-action", "will-change",
        "caret-color", "accent-color",
        "appearance", "-webkit-appearance",
        "resize",
        // Container queries
        "container", "container-type", "container-name",
        // Writing mode / direction / bidi
        "writing-mode", "direction", "unicode-bidi",
        "text-combine-upright", "text-orientation",
        // Columns / fragments
        "break-before", "break-after", "break-inside",
        "page-break-before", "page-break-after", "page-break-inside",
        "orphans", "widows",
        // Counter
        "counter-increment", "counter-reset", "counter-set",
        // Content
        "content",
        // Page
        "page",
        // Placeholder
        "placeholder-color",
        // Hanging punctuation
        "hanging-punctuation",
        // Ruby
        "ruby-align", "ruby-position", "ruby-overhang",
        // Math
        "math-style", "math-depth",
        // Offset path (motion path)
        "offset", "offset-path", "offset-distance",
        "offset-rotate", "offset-anchor", "offset-position",
        // SVG presentation attributes
        "fill", "fill-opacity", "fill-rule",
        "stroke", "stroke-width", "stroke-opacity",
        "stroke-dasharray", "stroke-dashoffset",
        "stroke-linecap", "stroke-linejoin", "stroke-miterlimit",
        "stop-color", "stop-opacity",
        "flood-color", "flood-opacity",
        "lighting-color",
        "marker", "marker-start", "marker-mid", "marker-end",
        "vector-effect",
        "paint-order",
        "dominant-baseline",
        "initial-letter", "initial-letter-align",
        // Miscellaneous
        "color-interpolation",
        "color-scheme",
        "isolation",
        "mix-blend-mode",
        "contain",
        "content-visibility",
        "overflow-anchor", "overflow-clip-margin",
    };

    // Helper: check if a property name is known.
    // Custom properties (--*) always return true.
    auto is_known_property = [&](const std::string& prop_name) -> bool {
        if (prop_name.size() >= 2 && prop_name[0] == '-' && prop_name[1] == '-')
            return true;  // CSS custom properties are always supported
        return known_properties.count(prop_name) > 0;
    };

    // Helper: validate a value for a specific property.
    // Returns true if the (property, value) pair is valid.
    auto is_valid_value = [](const std::string& prop_name,
                             const std::string& val) -> bool {
        // Global CSS-wide keywords are valid for any property.
        if (val == "initial" || val == "inherit" || val == "unset" ||
            val == "revert" || val == "revert-layer")
            return true;

        if (prop_name == "display") {
            static const std::unordered_set<std::string> display_vals = {
                "none", "block", "inline", "inline-block",
                "flex", "inline-flex",
                "grid", "inline-grid",
                "table", "inline-table",
                "table-row", "table-cell", "table-caption",
                "table-row-group", "table-header-group", "table-footer-group",
                "table-column", "table-column-group",
                "list-item", "flow-root",
                "contents", "run-in",
                "ruby", "ruby-base", "ruby-text",
                "ruby-base-container", "ruby-text-container",
                "math",
            };
            return display_vals.count(val) > 0;
        }

        if (prop_name == "position") {
            static const std::unordered_set<std::string> position_vals = {
                "static", "relative", "absolute", "fixed", "sticky",
            };
            return position_vals.count(val) > 0;
        }

        if (prop_name == "float") {
            static const std::unordered_set<std::string> float_vals = {
                "none", "left", "right", "inline-start", "inline-end",
            };
            return float_vals.count(val) > 0;
        }

        if (prop_name == "clear") {
            static const std::unordered_set<std::string> clear_vals = {
                "none", "left", "right", "both", "inline-start", "inline-end",
            };
            return clear_vals.count(val) > 0;
        }

        if (prop_name == "visibility") {
            static const std::unordered_set<std::string> vis_vals = {
                "visible", "hidden", "collapse",
            };
            return vis_vals.count(val) > 0;
        }

        if (prop_name == "overflow" || prop_name == "overflow-x" ||
            prop_name == "overflow-y" || prop_name == "overflow-block" ||
            prop_name == "overflow-inline") {
            static const std::unordered_set<std::string> ov_vals = {
                "visible", "hidden", "scroll", "auto", "clip",
            };
            return ov_vals.count(val) > 0;
        }

        if (prop_name == "box-sizing") {
            return val == "border-box" || val == "content-box";
        }

        // For all other known properties, accept any non-empty value.
        return !val.empty();
    };

    if (argc == 1) {
        // CSS.supports("(display: grid)") form
        const char* condition = JS_ToCString(ctx, argv[0]);
        if (!condition) return JS_FALSE;
        std::string cond(condition);
        JS_FreeCString(ctx, condition);

        // Strip leading/trailing whitespace and outer parens
        size_t start = cond.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return JS_FALSE;
        if (cond[start] == '(') ++start;
        size_t end = cond.find_last_not_of(" \t\n\r");
        if (end == std::string::npos || end < start) return JS_FALSE;
        if (cond[end] == ')') {
            if (end == 0) return JS_FALSE;
            --end;
        }
        std::string inner = cond.substr(start, end - start + 1);

        // Find the colon separating property from value
        size_t colon = inner.find(':');
        if (colon == std::string::npos) return JS_FALSE;

        std::string prop = inner.substr(0, colon);
        std::string val  = inner.substr(colon + 1);

        // Trim whitespace from prop
        size_t ps = prop.find_first_not_of(" \t");
        size_t pe = prop.find_last_not_of(" \t");
        if (ps == std::string::npos) return JS_FALSE;
        prop = prop.substr(ps, pe - ps + 1);

        // Trim whitespace from val
        size_t vs = val.find_first_not_of(" \t");
        size_t ve = val.find_last_not_of(" \t");
        if (vs == std::string::npos) return JS_FALSE;
        val = val.substr(vs, ve - vs + 1);

        if (!is_known_property(prop)) return JS_FALSE;
        return is_valid_value(prop, val) ? JS_TRUE : JS_FALSE;
    } else {
        // CSS.supports("display", "grid") form
        const char* prop_cstr = JS_ToCString(ctx, argv[0]);
        if (!prop_cstr) return JS_FALSE;
        std::string prop_str(prop_cstr);
        JS_FreeCString(ctx, prop_cstr);

        if (!is_known_property(prop_str)) return JS_FALSE;

        const char* val_cstr = JS_ToCString(ctx, argv[1]);
        if (!val_cstr) return JS_FALSE;
        std::string val_str(val_cstr);
        JS_FreeCString(ctx, val_cstr);

        // Trim whitespace from value
        size_t vs = val_str.find_first_not_of(" \t");
        size_t ve = val_str.find_last_not_of(" \t");
        if (vs == std::string::npos) return JS_FALSE;
        val_str = val_str.substr(vs, ve - vs + 1);

        return is_valid_value(prop_str, val_str) ? JS_TRUE : JS_FALSE;
    }
}

static JSValue js_css_escape(JSContext* ctx, JSValueConst /*this_val*/,
                             int argc, JSValueConst* argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "CSS.escape requires 1 argument");
    const char* str = JS_ToCString(ctx, argv[0]);
    if (!str) return JS_EXCEPTION;
    
    std::string result;
    size_t len = std::strlen(str);
    for (size_t i = 0; i < len; i++) {
        char c = str[i];
        if (c == 0) {
            result += "\\fffd ";
        } else if ((c >= 0x01 && c <= 0x1f) || c == 0x7f) {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\%x ", (unsigned char)c);
            result += buf;
        } else if (i == 0 && c >= '0' && c <= '9') {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\%x ", (unsigned char)c);
            result += buf;
        } else if (i == 0 && c == '-' && len == 1) {
            result += "\\-";
        } else if (c == '-' || c == '_' || (static_cast<unsigned char>(c) >= 0x80) ||
                   (c >= '0' && c <= '9') ||
                   (c >= 'A' && c <= 'Z') ||
                   (c >= 'a' && c <= 'z')) {
            result += c;
        } else {
            result += '\\';
            result += c;
        }
    }
    JS_FreeCString(ctx, str);
    return JS_NewStringLen(ctx, result.c_str(), result.size());
}

static JSValue js_css_register_property(JSContext* ctx, JSValueConst /*this_val*/,
                                        int argc, JSValueConst* argv) {
    // CSS.registerProperty({name, syntax, inherits, initialValue})
    // Stores registered custom properties for use in var() resolution
    if (argc < 1 || !JS_IsObject(argv[0]))
        return JS_ThrowTypeError(ctx, "CSS.registerProperty requires a descriptor object");

    JSValue desc = argv[0];
    JSValue name_val = JS_GetPropertyStr(ctx, desc, "name");
    if (!JS_IsString(name_val)) {
        JS_FreeValue(ctx, name_val);
        return JS_ThrowTypeError(ctx, "CSS.registerProperty: 'name' is required");
    }
    const char* name_str = JS_ToCString(ctx, name_val);
    if (!name_str || std::string(name_str).substr(0, 2) != "--") {
        if (name_str) JS_FreeCString(ctx, name_str);
        JS_FreeValue(ctx, name_val);
        return JS_ThrowTypeError(ctx, "CSS.registerProperty: name must start with '--'");
    }

    // Store registration on globalThis.__cssRegisteredProperties
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue registry = JS_GetPropertyStr(ctx, global, "__cssRegisteredProperties");
    if (!JS_IsObject(registry)) {
        JS_FreeValue(ctx, registry);
        registry = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, global, "__cssRegisteredProperties", JS_DupValue(ctx, registry));
    }

    // Store the full descriptor
    JS_SetPropertyStr(ctx, registry, name_str, JS_DupValue(ctx, desc));

    // If initialValue is set and it's a custom property, register it as default
    JSValue initial = JS_GetPropertyStr(ctx, desc, "initialValue");
    if (JS_IsString(initial)) {
        const char* init_str = JS_ToCString(ctx, initial);
        if (init_str) {
            // Store as a CSS custom property default on document.documentElement.style
            JSValue doc = JS_GetPropertyStr(ctx, global, "document");
            if (JS_IsObject(doc)) {
                JSValue docEl = JS_GetPropertyStr(ctx, doc, "documentElement");
                if (JS_IsObject(docEl)) {
                    JSValue style = JS_GetPropertyStr(ctx, docEl, "style");
                    if (JS_IsObject(style)) {
                        JSValue existing = JS_GetPropertyStr(ctx, style, name_str);
                        if (JS_IsUndefined(existing) || !JS_IsString(existing)) {
                            JS_SetPropertyStr(ctx, style, name_str, JS_NewString(ctx, init_str));
                        }
                        JS_FreeValue(ctx, existing);
                        JS_FreeValue(ctx, style);
                    }
                    JS_FreeValue(ctx, docEl);
                }
                JS_FreeValue(ctx, doc);
            }
            JS_FreeCString(ctx, init_str);
        }
    }
    JS_FreeValue(ctx, initial);

    JS_FreeValue(ctx, registry);
    JS_FreeCString(ctx, name_str);
    JS_FreeValue(ctx, name_val);
    JS_FreeValue(ctx, global);
    return JS_UNDEFINED;
}

// =========================================================================
// Intl constructors and format helpers
// =========================================================================

struct IntlIntOption {
    bool has_value = false;
    int value = 0;
};

static IntlIntOption js_intl_get_int_option(JSContext* ctx, JSValueConst obj,
                                           const char* prop, int min_value,
                                           int max_value) {
    IntlIntOption result;
    if (!JS_IsObject(obj)) return result;

    JSValue prop_val = JS_GetPropertyStr(ctx, obj, prop);
    if (JS_IsNumber(prop_val)) {
        int parsed = 0;
        if (JS_ToInt32(ctx, &parsed, prop_val) == 0 &&
            parsed >= min_value && parsed <= max_value) {
            result.has_value = true;
            result.value = parsed;
        }
    }
    JS_FreeValue(ctx, prop_val);
    return result;
}

static std::string js_intl_get_string_option(JSContext* ctx, JSValueConst obj,
                                            const char* prop,
                                            const std::string& fallback) {
    std::string result = fallback;
    if (!JS_IsObject(obj)) return result;

    JSValue prop_val = JS_GetPropertyStr(ctx, obj, prop);
    if (JS_IsString(prop_val)) {
        const char* s = JS_ToCString(ctx, prop_val);
        if (s) {
            result = s;
            JS_FreeCString(ctx, s);
        }
    }
    JS_FreeValue(ctx, prop_val);
    return result;
}

static const char* js_intl_currency_symbol(const std::string& currency_code) {
    if (currency_code == "USD") return "$";
    if (currency_code == "EUR") return "€";
    if (currency_code == "GBP") return "£";
    if (currency_code == "JPY") return "¥";
    if (currency_code == "CNY") return "¥";
    if (currency_code == "KRW") return "₩";
    if (currency_code == "INR") return "₹";
    if (currency_code == "CAD" || currency_code == "AUD") return "$";
    return "";
}

static JSValue js_intl_number_format_format(JSContext* ctx, JSValueConst this_val,
                                           int argc, JSValueConst* argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "NumberFormat.format requires 1 argument");
    if (!JS_IsObject(this_val))
        return JS_ThrowTypeError(ctx, "Invalid NumberFormat receiver");

    double value = 0.0;
    if (JS_ToFloat64(ctx, &value, argv[0]) != 0) return JS_EXCEPTION;

    if (!std::isfinite(value)) {
        return JS_NewString(ctx, "NaN");
    }

    std::string style = js_intl_get_string_option(ctx, this_val, "style", "decimal");
    if (style != "decimal" && style != "currency" && style != "percent") {
        style = "decimal";
    }

    std::string currency = js_intl_get_string_option(
        ctx, this_val, "currency", style == "currency" ? "USD" : "");
    for (auto& ch : currency) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));

    IntlIntOption min_opt = js_intl_get_int_option(
        ctx, this_val, "minimumFractionDigits", 0, 20);
    IntlIntOption max_opt = js_intl_get_int_option(
        ctx, this_val, "maximumFractionDigits", 0, 20);

    int minimum_digits = min_opt.has_value ? min_opt.value : -1;
    int maximum_digits = max_opt.has_value ? max_opt.value : -1;

    if (style == "currency") {
        if (minimum_digits < 0) minimum_digits = 2;
        if (maximum_digits < 0) maximum_digits = 2;
    } else if (style == "percent") {
        if (minimum_digits < 0) minimum_digits = 0;
        if (maximum_digits < 0) maximum_digits = 0;
    } else {
        if (minimum_digits < 0) minimum_digits = 0;
        if (maximum_digits < 0) maximum_digits = 3;
    }

    if (maximum_digits < minimum_digits) maximum_digits = minimum_digits;
    double scaled_value = value;
    if (style == "percent") scaled_value *= 100.0;

    char formatted_value[128];
    std::snprintf(formatted_value, sizeof(formatted_value), "%.*f",
                  maximum_digits, scaled_value);

    std::string result(formatted_value);
    size_t dot = result.find('.');
    if (dot != std::string::npos && result.size() > dot + static_cast<size_t>(minimum_digits + 1)) {
        size_t end = result.size();
        while (end > dot + static_cast<size_t>(minimum_digits + 1) &&
               end > dot + 1 &&
               result[end - 1] == '0') {
            --end;
        }
        if (end == dot + 1) {
            result.resize(dot);
        } else {
            result.resize(end);
            if (end == dot + 1) {
                result.resize(dot);
            }
        }
    }

    if (style == "currency" && !currency.empty()) {
        return JS_NewString(ctx, (std::string(js_intl_currency_symbol(currency)) + result).c_str());
    }

    if (style == "percent") {
        return JS_NewString(ctx, (result + "%").c_str());
    }

    return JS_NewString(ctx, result.c_str());
}

static JSValue js_intl_number_format_constructor(JSContext* ctx, JSValueConst /*new_target*/,
                                                 int argc, JSValueConst* argv) {
    std::string locale = "en-US";
    if (argc >= 1 && !JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
        const char* locale_cstr = JS_ToCString(ctx, argv[0]);
        if (!locale_cstr) return JS_EXCEPTION;
        locale = locale_cstr;
        JS_FreeCString(ctx, locale_cstr);
    }
    if (argc >= 2 && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1]) &&
        !JS_IsObject(argv[1])) {
        return JS_ThrowTypeError(ctx,
            "Intl.NumberFormat options must be an object");
    }

    JSValue options = (argc >= 2 && JS_IsObject(argv[1])) ? argv[1] : JS_UNDEFINED;

    std::string style = js_intl_get_string_option(ctx, options, "style", "decimal");
    if (style != "decimal" && style != "currency" && style != "percent") {
        style = "decimal";
    }
    std::string currency = js_intl_get_string_option(
        ctx, options, "currency", style == "currency" ? "USD" : "");
    for (auto& ch : currency) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));

    IntlIntOption min_opt = js_intl_get_int_option(
        ctx, options, "minimumFractionDigits", 0, 20);
    IntlIntOption max_opt = js_intl_get_int_option(
        ctx, options, "maximumFractionDigits", 0, 20);

    int min_digits = min_opt.has_value ? min_opt.value : -1;
    int max_digits = max_opt.has_value ? max_opt.value : -1;

    if (style == "currency") {
        if (min_digits < 0) min_digits = 2;
        if (max_digits < 0) max_digits = 2;
    } else if (style == "percent") {
        if (min_digits < 0) min_digits = 0;
        if (max_digits < 0) max_digits = 0;
    } else {
        if (min_digits < 0) min_digits = 0;
        if (max_digits < 0) max_digits = 3;
    }
    if (max_digits < min_digits) max_digits = min_digits;

    JSValue formatter = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, formatter, "locale", JS_NewString(ctx, locale.c_str()));
    JS_SetPropertyStr(ctx, formatter, "style", JS_NewString(ctx, style.c_str()));
    JS_SetPropertyStr(ctx, formatter, "currency", JS_NewString(ctx, currency.c_str()));
    JS_SetPropertyStr(ctx, formatter, "minimumFractionDigits", JS_NewInt32(ctx, min_digits));
    JS_SetPropertyStr(ctx, formatter, "maximumFractionDigits", JS_NewInt32(ctx, max_digits));
    JS_SetPropertyStr(ctx, formatter, "format",
        JS_NewCFunction(ctx, js_intl_number_format_format, "format", 1));
    return formatter;
}

static JSValue js_intl_date_time_format_format(JSContext* ctx, JSValueConst this_val,
                                              int argc, JSValueConst* argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "DateTimeFormat.format requires 1 argument");
    if (!JS_IsObject(this_val))
        return JS_ThrowTypeError(ctx, "Invalid DateTimeFormat receiver");

    // Accept Date objects (via valueOf) or numbers as timestamps
    double timestamp_ms = 0.0;
    if (JS_IsObject(argv[0])) {
        // Try calling valueOf() to get the timestamp from a Date object
        JSAtom valueOf_atom = JS_NewAtom(ctx, "valueOf");
        JSValue vo = JS_Invoke(ctx, argv[0], valueOf_atom, 0, nullptr);
        JS_FreeAtom(ctx, valueOf_atom);
        if (JS_IsException(vo)) return JS_EXCEPTION;
        if (JS_ToFloat64(ctx, &timestamp_ms, vo) != 0) { JS_FreeValue(ctx, vo); return JS_EXCEPTION; }
        JS_FreeValue(ctx, vo);
    } else if (JS_ToFloat64(ctx, &timestamp_ms, argv[0]) != 0) {
        return JS_EXCEPTION;
    }

    if (!std::isfinite(timestamp_ms)) {
        return JS_NewString(ctx, "Invalid Date");
    }

    time_t timestamp_seconds = static_cast<time_t>(timestamp_ms / 1000.0);
    std::tm* gm = std::gmtime(&timestamp_seconds);
    if (!gm) return JS_NewString(ctx, "Invalid Date");
    std::tm tm_copy = *gm;

    char ctime_buf[64] = {};
    const char* ctime_result = std::ctime(&timestamp_seconds);
    if (ctime_result) {
        std::strncpy(ctime_buf, ctime_result, sizeof(ctime_buf) - 1);
    }

    std::string year = js_intl_get_string_option(
        ctx, this_val, "year", "numeric");
    std::string month = js_intl_get_string_option(
        ctx, this_val, "month", "numeric");
    std::string day = js_intl_get_string_option(
        ctx, this_val, "day", "numeric");
    std::string hour = js_intl_get_string_option(
        ctx, this_val, "hour", "numeric");
    std::string minute = js_intl_get_string_option(
        ctx, this_val, "minute", "numeric");
    std::string second = js_intl_get_string_option(
        ctx, this_val, "second", "numeric");

    auto add_piece = [](std::string& pattern, const std::string& value,
                        const char* spec2, const char* spec4) {
        if (value == "2-digit") pattern += spec2;
        else if (value == "numeric") pattern += spec4;
    };

    std::string date_pattern;
    std::string time_pattern;
    add_piece(date_pattern, year, "%y", "%Y");
    add_piece(date_pattern, month, "%m", "%m");
    add_piece(date_pattern, day, "%d", "%d");
    add_piece(time_pattern, hour, "%H", "%H");
    add_piece(time_pattern, minute, "%M", "%M");
    add_piece(time_pattern, second, "%S", "%S");

    if (date_pattern.empty() && time_pattern.empty()) {
        if (ctime_buf[0] != '\0') {
            size_t len = std::strlen(ctime_buf);
            if (len > 0 && ctime_buf[len - 1] == '\n') ctime_buf[len - 1] = '\0';
            return JS_NewString(ctx, ctime_buf);
        }
        return JS_NewString(ctx, "");
    }

    char date_buf[128] = {};
    char time_buf[128] = {};
    std::string formatted;

    if (!date_pattern.empty()) {
        std::string date_sep_pattern;
        for (size_t i = 0; i < date_pattern.size(); ++i) {
            if (date_pattern[i] == '%') {
                date_sep_pattern += date_pattern.substr(i, 2);
                ++i;
                if (i + 1 < date_pattern.size()) date_sep_pattern += '-';
            }
        }
        if (!date_sep_pattern.empty() &&
            date_sep_pattern.back() == '-') date_sep_pattern.pop_back();
        if (date_sep_pattern.empty()) date_sep_pattern = date_pattern;
        std::strftime(date_buf, sizeof(date_buf), date_sep_pattern.c_str(), &tm_copy);
        formatted += date_buf;
    }

    if (!time_pattern.empty()) {
        std::string time_sep_pattern;
        for (size_t i = 0; i < time_pattern.size(); ++i) {
            if (time_pattern[i] == '%') {
                time_sep_pattern += time_pattern.substr(i, 2);
                ++i;
                if (i + 1 < time_pattern.size()) time_sep_pattern += ':';
            }
        }
        if (!time_sep_pattern.empty() &&
            time_sep_pattern.back() == ':') time_sep_pattern.pop_back();
        if (time_sep_pattern.empty()) time_sep_pattern = time_pattern;
        std::strftime(time_buf, sizeof(time_buf), time_sep_pattern.c_str(), &tm_copy);
        if (!formatted.empty()) formatted += " ";
        formatted += time_buf;
    }

    return JS_NewString(ctx, formatted.c_str());
}

static JSValue js_intl_date_time_format_constructor(JSContext* ctx, JSValueConst /*new_target*/,
                                                    int argc, JSValueConst* argv) {
    std::string locale = "en-US";
    if (argc >= 1 && !JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
        const char* locale_cstr = JS_ToCString(ctx, argv[0]);
        if (!locale_cstr) return JS_EXCEPTION;
        locale = locale_cstr;
        JS_FreeCString(ctx, locale_cstr);
    }

    if (argc >= 2 && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1]) &&
        !JS_IsObject(argv[1])) {
        return JS_ThrowTypeError(ctx,
            "Intl.DateTimeFormat options must be an object");
    }

    JSValue options = (argc >= 2 && JS_IsObject(argv[1])) ? argv[1] : JS_UNDEFINED;

    JSValue formatter = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, formatter, "locale", JS_NewString(ctx, locale.c_str()));
    JS_SetPropertyStr(ctx, formatter, "year",
        JS_NewString(ctx, js_intl_get_string_option(ctx, options, "year", "numeric").c_str()));
    JS_SetPropertyStr(ctx, formatter, "month",
        JS_NewString(ctx, js_intl_get_string_option(ctx, options, "month", "numeric").c_str()));
    JS_SetPropertyStr(ctx, formatter, "day",
        JS_NewString(ctx, js_intl_get_string_option(ctx, options, "day", "numeric").c_str()));
    JS_SetPropertyStr(ctx, formatter, "hour",
        JS_NewString(ctx, js_intl_get_string_option(ctx, options, "hour", "numeric").c_str()));
    JS_SetPropertyStr(ctx, formatter, "minute",
        JS_NewString(ctx, js_intl_get_string_option(ctx, options, "minute", "numeric").c_str()));
    JS_SetPropertyStr(ctx, formatter, "second",
        JS_NewString(ctx, js_intl_get_string_option(ctx, options, "second", "numeric").c_str()));
    JS_SetPropertyStr(ctx, formatter, "format",
        JS_NewCFunction(ctx, js_intl_date_time_format_format, "format", 1));
    return formatter;
}

static JSValue js_intl_collator_compare(JSContext* ctx, JSValueConst this_val,
                                       int argc, JSValueConst* argv) {
    if (!JS_IsObject(this_val))
        return JS_ThrowTypeError(ctx, "Invalid Collator receiver");
    if (argc < 2) return JS_ThrowTypeError(ctx, "Intl.Collator.compare requires 2 arguments");

    const char* a = JS_ToCString(ctx, argv[0]);
    if (!a) return JS_EXCEPTION;
    const char* b = JS_ToCString(ctx, argv[1]);
    if (!b) {
        JS_FreeCString(ctx, a);
        return JS_EXCEPTION;
    }

    int cmp = std::strcoll(a, b);
    JS_FreeCString(ctx, a);
    JS_FreeCString(ctx, b);

    if (cmp < 0) return JS_NewInt32(ctx, -1);
    if (cmp > 0) return JS_NewInt32(ctx, 1);
    return JS_NewInt32(ctx, 0);
}

static JSValue js_intl_collator_constructor(JSContext* ctx, JSValueConst /*new_target*/,
                                           int argc, JSValueConst* argv) {
    std::string locale = "en-US";
    if (argc >= 1 && !JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
        const char* locale_cstr = JS_ToCString(ctx, argv[0]);
        if (!locale_cstr) return JS_EXCEPTION;
        locale = locale_cstr;
        JS_FreeCString(ctx, locale_cstr);
    }

    JSValue collator = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, collator, "locale", JS_NewString(ctx, locale.c_str()));
    JS_SetPropertyStr(ctx, collator, "compare",
        JS_NewCFunction(ctx, js_intl_collator_compare, "compare", 2));
    return collator;
}

static JSValue js_intl_plural_rules_select(JSContext* ctx, JSValueConst this_val,
                                          int argc, JSValueConst* argv) {
    if (!JS_IsObject(this_val))
        return JS_ThrowTypeError(ctx, "Invalid PluralRules receiver");
    if (argc < 1) return JS_ThrowTypeError(ctx, "Intl.PluralRules.select requires 1 argument");

    double value = 0.0;
    if (JS_ToFloat64(ctx, &value, argv[0]) != 0) return JS_EXCEPTION;

    return JS_NewString(ctx, value == 1.0 ? "one" : "other");
}

static JSValue js_intl_plural_rules_constructor(JSContext* ctx, JSValueConst /*new_target*/,
                                               int argc, JSValueConst* argv) {
    std::string locale = "en-US";
    if (argc >= 1 && !JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
        const char* locale_cstr = JS_ToCString(ctx, argv[0]);
        if (!locale_cstr) return JS_EXCEPTION;
        locale = locale_cstr;
        JS_FreeCString(ctx, locale_cstr);
    }

    JSValue rules = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, rules, "locale", JS_NewString(ctx, locale.c_str()));
    JS_SetPropertyStr(ctx, rules, "select",
        JS_NewCFunction(ctx, js_intl_plural_rules_select, "select", 1));
    return rules;
}

// =========================================================================
// Intl.ListFormat — format lists with conjunctions/disjunctions
// =========================================================================

static JSValue js_intl_list_format_format(JSContext* ctx, JSValueConst this_val,
                                          int argc, JSValueConst* argv) {
    if (argc < 1 || !JS_IsArray(ctx, argv[0]))
        return JS_ThrowTypeError(ctx, "ListFormat.format requires an array");

    std::string type_str = js_intl_get_string_option(ctx, this_val, "type", "conjunction");
    std::string style_str = js_intl_get_string_option(ctx, this_val, "style", "long");

    JSValue length_val = JS_GetPropertyStr(ctx, argv[0], "length");
    int32_t len = 0;
    JS_ToInt32(ctx, &len, length_val);
    JS_FreeValue(ctx, length_val);

    std::vector<std::string> items;
    for (int32_t i = 0; i < len; i++) {
        JSValue elem = JS_GetPropertyUint32(ctx, argv[0], i);
        const char* s = JS_ToCString(ctx, elem);
        if (s) { items.push_back(s); JS_FreeCString(ctx, s); }
        JS_FreeValue(ctx, elem);
    }

    if (items.empty()) return JS_NewString(ctx, "");
    if (items.size() == 1) return JS_NewString(ctx, items[0].c_str());

    std::string conjunction = (type_str == "disjunction") ? "or" : "and";
    if (style_str == "narrow" && type_str == "unit") conjunction = ",";

    std::string result;
    for (size_t i = 0; i < items.size(); i++) {
        if (i > 0) {
            if (i == items.size() - 1) {
                if (style_str == "narrow")
                    result += ", " + items[i];
                else
                    result += ", " + conjunction + " " + items[i];
            } else {
                result += ", " + items[i];
            }
        } else {
            result = items[i];
        }
    }
    return JS_NewString(ctx, result.c_str());
}

static JSValue js_intl_list_format_constructor(JSContext* ctx, JSValueConst /*new_target*/,
                                               int argc, JSValueConst* argv) {
    std::string locale = "en-US";
    if (argc >= 1 && JS_IsString(argv[0])) {
        const char* s = JS_ToCString(ctx, argv[0]);
        if (s) { locale = s; JS_FreeCString(ctx, s); }
    }
    JSValue options = (argc >= 2 && JS_IsObject(argv[1])) ? argv[1] : JS_UNDEFINED;

    JSValue fmt = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, fmt, "locale", JS_NewString(ctx, locale.c_str()));
    std::string type_val = "conjunction";
    std::string style_val = "long";
    if (JS_IsObject(options)) {
        type_val = js_intl_get_string_option(ctx, options, "type", "conjunction");
        style_val = js_intl_get_string_option(ctx, options, "style", "long");
    }
    JS_SetPropertyStr(ctx, fmt, "type", JS_NewString(ctx, type_val.c_str()));
    JS_SetPropertyStr(ctx, fmt, "style", JS_NewString(ctx, style_val.c_str()));
    JS_SetPropertyStr(ctx, fmt, "format",
        JS_NewCFunction(ctx, js_intl_list_format_format, "format", 1));
    return fmt;
}

// =========================================================================
// Intl.RelativeTimeFormat — "3 days ago", "in 2 hours"
// =========================================================================

static JSValue js_intl_relative_time_format_format(JSContext* ctx, JSValueConst this_val,
                                                   int argc, JSValueConst* argv) {
    if (argc < 2) return JS_ThrowTypeError(ctx, "RelativeTimeFormat.format requires value and unit");

    double value = 0;
    JS_ToFloat64(ctx, &value, argv[0]);
    const char* unit_cstr = JS_ToCString(ctx, argv[1]);
    std::string unit = unit_cstr ? unit_cstr : "second";
    if (unit_cstr) JS_FreeCString(ctx, unit_cstr);

    // Normalize plural units
    if (unit.back() == 's' && unit.size() > 1) unit.pop_back();

    std::string numeric = js_intl_get_string_option(ctx, this_val, "numeric", "always");
    int abs_val = static_cast<int>(std::abs(value));

    std::string unit_str = unit;
    if (abs_val != 1) unit_str += "s";

    std::string result;
    if (numeric == "auto" && abs_val == 0) {
        if (unit == "day") result = "today";
        else if (unit == "year") result = "this year";
        else if (unit == "month") result = "this month";
        else if (unit == "week") result = "this week";
        else if (unit == "hour") result = "this hour";
        else if (unit == "minute") result = "this minute";
        else result = "now";
    } else if (numeric == "auto" && abs_val == 1 && value < 0) {
        if (unit == "day") result = "yesterday";
        else if (unit == "year") result = "last year";
        else if (unit == "month") result = "last month";
        else if (unit == "week") result = "last week";
        else result = std::to_string(abs_val) + " " + unit_str + " ago";
    } else if (numeric == "auto" && abs_val == 1 && value > 0) {
        if (unit == "day") result = "tomorrow";
        else if (unit == "year") result = "next year";
        else if (unit == "month") result = "next month";
        else if (unit == "week") result = "next week";
        else result = "in " + std::to_string(abs_val) + " " + unit_str;
    } else if (value < 0) {
        result = std::to_string(abs_val) + " " + unit_str + " ago";
    } else {
        result = "in " + std::to_string(abs_val) + " " + unit_str;
    }

    return JS_NewString(ctx, result.c_str());
}

static JSValue js_intl_relative_time_format_constructor(JSContext* ctx, JSValueConst /*new_target*/,
                                                        int argc, JSValueConst* argv) {
    std::string locale = "en-US";
    if (argc >= 1 && JS_IsString(argv[0])) {
        const char* s = JS_ToCString(ctx, argv[0]);
        if (s) { locale = s; JS_FreeCString(ctx, s); }
    }
    JSValue options = (argc >= 2 && JS_IsObject(argv[1])) ? argv[1] : JS_UNDEFINED;

    JSValue fmt = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, fmt, "locale", JS_NewString(ctx, locale.c_str()));
    std::string numeric_val = "always";
    std::string style_val = "long";
    if (JS_IsObject(options)) {
        numeric_val = js_intl_get_string_option(ctx, options, "numeric", "always");
        style_val = js_intl_get_string_option(ctx, options, "style", "long");
    }
    JS_SetPropertyStr(ctx, fmt, "numeric", JS_NewString(ctx, numeric_val.c_str()));
    JS_SetPropertyStr(ctx, fmt, "style", JS_NewString(ctx, style_val.c_str()));
    JS_SetPropertyStr(ctx, fmt, "format",
        JS_NewCFunction(ctx, js_intl_relative_time_format_format, "format", 2));
    return fmt;
}

// =========================================================================
// Intl.DisplayNames — display names for languages, regions, scripts, currencies
// =========================================================================

static JSValue js_intl_display_names_of(JSContext* ctx, JSValueConst this_val,
                                        int argc, JSValueConst* argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "DisplayNames.of requires a code argument");

    const char* code_cstr = JS_ToCString(ctx, argv[0]);
    std::string code = code_cstr ? code_cstr : "";
    if (code_cstr) JS_FreeCString(ctx, code_cstr);

    std::string type_str = js_intl_get_string_option(ctx, this_val, "type", "language");

    // Basic lookup tables for common codes
    if (type_str == "language") {
        static const std::map<std::string, std::string> langs = {
            {"en", "English"}, {"es", "Spanish"}, {"fr", "French"}, {"de", "German"},
            {"ja", "Japanese"}, {"ko", "Korean"}, {"zh", "Chinese"}, {"pt", "Portuguese"},
            {"ru", "Russian"}, {"it", "Italian"}, {"ar", "Arabic"}, {"hi", "Hindi"},
            {"nl", "Dutch"}, {"sv", "Swedish"}, {"pl", "Polish"}, {"tr", "Turkish"},
            {"th", "Thai"}, {"vi", "Vietnamese"}, {"uk", "Ukrainian"}, {"cs", "Czech"},
            {"en-US", "American English"}, {"en-GB", "British English"},
            {"zh-Hans", "Simplified Chinese"}, {"zh-Hant", "Traditional Chinese"},
            {"pt-BR", "Brazilian Portuguese"},
        };
        auto it = langs.find(code);
        if (it != langs.end()) return JS_NewString(ctx, it->second.c_str());
    } else if (type_str == "region") {
        static const std::map<std::string, std::string> regions = {
            {"US", "United States"}, {"GB", "United Kingdom"}, {"DE", "Germany"},
            {"FR", "France"}, {"JP", "Japan"}, {"KR", "South Korea"}, {"CN", "China"},
            {"CA", "Canada"}, {"AU", "Australia"}, {"BR", "Brazil"}, {"IN", "India"},
            {"IT", "Italy"}, {"ES", "Spain"}, {"MX", "Mexico"}, {"RU", "Russia"},
        };
        auto it = regions.find(code);
        if (it != regions.end()) return JS_NewString(ctx, it->second.c_str());
    } else if (type_str == "currency") {
        static const std::map<std::string, std::string> currencies = {
            {"USD", "US Dollar"}, {"EUR", "Euro"}, {"GBP", "British Pound"},
            {"JPY", "Japanese Yen"}, {"KRW", "South Korean Won"}, {"CNY", "Chinese Yuan"},
            {"CAD", "Canadian Dollar"}, {"AUD", "Australian Dollar"}, {"CHF", "Swiss Franc"},
            {"BRL", "Brazilian Real"}, {"INR", "Indian Rupee"}, {"MXN", "Mexican Peso"},
        };
        auto it = currencies.find(code);
        if (it != currencies.end()) return JS_NewString(ctx, it->second.c_str());
    } else if (type_str == "script") {
        static const std::map<std::string, std::string> scripts = {
            {"Latn", "Latin"}, {"Cyrl", "Cyrillic"}, {"Arab", "Arabic"},
            {"Hans", "Simplified"}, {"Hant", "Traditional"}, {"Kana", "Katakana"},
            {"Hang", "Hangul"}, {"Deva", "Devanagari"}, {"Thai", "Thai"},
        };
        auto it = scripts.find(code);
        if (it != scripts.end()) return JS_NewString(ctx, it->second.c_str());
    }

    return JS_NewString(ctx, code.c_str());
}

static JSValue js_intl_display_names_constructor(JSContext* ctx, JSValueConst /*new_target*/,
                                                 int argc, JSValueConst* argv) {
    std::string locale = "en-US";
    if (argc >= 1 && JS_IsString(argv[0])) {
        const char* s = JS_ToCString(ctx, argv[0]);
        if (s) { locale = s; JS_FreeCString(ctx, s); }
    }
    JSValue options = (argc >= 2 && JS_IsObject(argv[1])) ? argv[1] : JS_UNDEFINED;

    JSValue fmt = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, fmt, "locale", JS_NewString(ctx, locale.c_str()));
    std::string type_val = "language";
    if (JS_IsObject(options)) {
        type_val = js_intl_get_string_option(ctx, options, "type", "language");
    }
    JS_SetPropertyStr(ctx, fmt, "type", JS_NewString(ctx, type_val.c_str()));
    JS_SetPropertyStr(ctx, fmt, "of",
        JS_NewCFunction(ctx, js_intl_display_names_of, "of", 1));
    return fmt;
}

// =========================================================================
// Intl.Segmenter — text segmentation (grapheme, word, sentence)
// =========================================================================

static JSValue js_intl_segmenter_segment(JSContext* ctx, JSValueConst this_val,
                                         int argc, JSValueConst* argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "Segmenter.segment requires a string");

    const char* str = JS_ToCString(ctx, argv[0]);
    if (!str) return JS_UNDEFINED;
    std::string input(str);
    JS_FreeCString(ctx, str);

    std::string granularity = js_intl_get_string_option(ctx, this_val, "granularity", "grapheme");

    JSValue segments = JS_NewArray(ctx);
    uint32_t idx = 0;

    if (granularity == "word") {
        // Simple word boundary segmentation
        size_t start = 0;
        bool in_word = false;
        for (size_t i = 0; i <= input.size(); i++) {
            bool is_word_char = (i < input.size()) &&
                (std::isalnum(static_cast<unsigned char>(input[i])) || input[i] == '\'');
            if (is_word_char && !in_word) { start = i; in_word = true; }
            else if (!is_word_char && in_word) {
                JSValue seg = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, seg, "segment",
                    JS_NewStringLen(ctx, input.c_str() + start, i - start));
                JS_SetPropertyStr(ctx, seg, "index", JS_NewInt32(ctx, static_cast<int>(start)));
                JS_SetPropertyStr(ctx, seg, "isWordLike", JS_TRUE);
                JS_SetPropertyUint32(ctx, segments, idx++, seg);
                in_word = false;
                // Add non-word segment
                start = i;
            }
            if (!is_word_char && i < input.size()) {
                size_t ns = i;
                while (i < input.size() && !std::isalnum(static_cast<unsigned char>(input[i])) && input[i] != '\'')
                    i++;
                JSValue seg = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, seg, "segment",
                    JS_NewStringLen(ctx, input.c_str() + ns, i - ns));
                JS_SetPropertyStr(ctx, seg, "index", JS_NewInt32(ctx, static_cast<int>(ns)));
                JS_SetPropertyStr(ctx, seg, "isWordLike", JS_FALSE);
                JS_SetPropertyUint32(ctx, segments, idx++, seg);
                if (i < input.size()) { start = i; in_word = true; }
            }
        }
    } else if (granularity == "sentence") {
        // Simple sentence segmentation on .!?
        size_t start = 0;
        for (size_t i = 0; i < input.size(); i++) {
            if (input[i] == '.' || input[i] == '!' || input[i] == '?') {
                size_t end = i + 1;
                while (end < input.size() && input[end] == ' ') end++;
                JSValue seg = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, seg, "segment",
                    JS_NewStringLen(ctx, input.c_str() + start, end - start));
                JS_SetPropertyStr(ctx, seg, "index", JS_NewInt32(ctx, static_cast<int>(start)));
                JS_SetPropertyUint32(ctx, segments, idx++, seg);
                start = end;
                i = end - 1;
            }
        }
        if (start < input.size()) {
            JSValue seg = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, seg, "segment",
                JS_NewStringLen(ctx, input.c_str() + start, input.size() - start));
            JS_SetPropertyStr(ctx, seg, "index", JS_NewInt32(ctx, static_cast<int>(start)));
            JS_SetPropertyUint32(ctx, segments, idx++, seg);
        }
    } else {
        // Grapheme: each character (simplified — proper grapheme clusters need ICU)
        size_t i = 0;
        while (i < input.size()) {
            size_t char_len = 1;
            unsigned char c = static_cast<unsigned char>(input[i]);
            if (c >= 0xF0) char_len = 4;
            else if (c >= 0xE0) char_len = 3;
            else if (c >= 0xC0) char_len = 2;
            if (i + char_len > input.size()) char_len = input.size() - i;

            JSValue seg = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, seg, "segment",
                JS_NewStringLen(ctx, input.c_str() + i, char_len));
            JS_SetPropertyStr(ctx, seg, "index", JS_NewInt32(ctx, static_cast<int>(i)));
            JS_SetPropertyUint32(ctx, segments, idx++, seg);
            i += char_len;
        }
    }

    return segments;
}

static JSValue js_intl_segmenter_constructor(JSContext* ctx, JSValueConst /*new_target*/,
                                             int argc, JSValueConst* argv) {
    std::string locale = "en-US";
    if (argc >= 1 && JS_IsString(argv[0])) {
        const char* s = JS_ToCString(ctx, argv[0]);
        if (s) { locale = s; JS_FreeCString(ctx, s); }
    }
    JSValue options = (argc >= 2 && JS_IsObject(argv[1])) ? argv[1] : JS_UNDEFINED;

    JSValue fmt = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, fmt, "locale", JS_NewString(ctx, locale.c_str()));
    std::string gran = "grapheme";
    if (JS_IsObject(options)) {
        gran = js_intl_get_string_option(ctx, options, "granularity", "grapheme");
    }
    JS_SetPropertyStr(ctx, fmt, "granularity", JS_NewString(ctx, gran.c_str()));
    JS_SetPropertyStr(ctx, fmt, "segment",
        JS_NewCFunction(ctx, js_intl_segmenter_segment, "segment", 1));
    return fmt;
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
                              int width, int height,
                              float device_pixel_ratio) {
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
    // Allocate per-context storage for marks/measures
    {
        auto* perf_state = new PerformanceState();
        JS_SetPropertyStr(ctx, global, "__perf_state_ptr",
            JS_NewInt64(ctx, static_cast<int64_t>(
                reinterpret_cast<uintptr_t>(perf_state))));
    }

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

    // performance.timing — Navigation Timing Level 1
    // All timestamps are Unix epoch milliseconds (like Date.now())
    {
        JSValue timing = JS_NewObject(ctx);
        double nav_start = page_time_origin_ms; // navigationStart = page load epoch ms
        // We record DOMContentLoaded/load as occurring shortly after nav start.
        // These are placeholders — a real implementation would record these at
        // the actual event dispatch points in the render pipeline.
        double dcl_start = nav_start;
        double dcl_end   = nav_start;
        double load_start = nav_start;
        double load_end   = nav_start;
        JS_SetPropertyStr(ctx, timing, "navigationStart",           JS_NewFloat64(ctx, nav_start));
        JS_SetPropertyStr(ctx, timing, "unloadEventStart",          JS_NewFloat64(ctx, 0.0));
        JS_SetPropertyStr(ctx, timing, "unloadEventEnd",            JS_NewFloat64(ctx, 0.0));
        JS_SetPropertyStr(ctx, timing, "redirectStart",             JS_NewFloat64(ctx, 0.0));
        JS_SetPropertyStr(ctx, timing, "redirectEnd",               JS_NewFloat64(ctx, 0.0));
        JS_SetPropertyStr(ctx, timing, "fetchStart",                JS_NewFloat64(ctx, nav_start));
        JS_SetPropertyStr(ctx, timing, "domainLookupStart",         JS_NewFloat64(ctx, nav_start));
        JS_SetPropertyStr(ctx, timing, "domainLookupEnd",           JS_NewFloat64(ctx, nav_start));
        JS_SetPropertyStr(ctx, timing, "connectStart",              JS_NewFloat64(ctx, nav_start));
        JS_SetPropertyStr(ctx, timing, "connectEnd",                JS_NewFloat64(ctx, nav_start));
        JS_SetPropertyStr(ctx, timing, "secureConnectionStart",     JS_NewFloat64(ctx, 0.0));
        JS_SetPropertyStr(ctx, timing, "requestStart",              JS_NewFloat64(ctx, nav_start));
        JS_SetPropertyStr(ctx, timing, "responseStart",             JS_NewFloat64(ctx, nav_start));
        JS_SetPropertyStr(ctx, timing, "responseEnd",               JS_NewFloat64(ctx, nav_start));
        JS_SetPropertyStr(ctx, timing, "domLoading",                JS_NewFloat64(ctx, nav_start));
        JS_SetPropertyStr(ctx, timing, "domInteractive",            JS_NewFloat64(ctx, nav_start));
        JS_SetPropertyStr(ctx, timing, "domContentLoadedEventStart",JS_NewFloat64(ctx, dcl_start));
        JS_SetPropertyStr(ctx, timing, "domContentLoadedEventEnd",  JS_NewFloat64(ctx, dcl_end));
        JS_SetPropertyStr(ctx, timing, "domComplete",               JS_NewFloat64(ctx, nav_start));
        JS_SetPropertyStr(ctx, timing, "loadEventStart",            JS_NewFloat64(ctx, load_start));
        JS_SetPropertyStr(ctx, timing, "loadEventEnd",              JS_NewFloat64(ctx, load_end));
        JS_SetPropertyStr(ctx, performance, "timing", timing);
    }

    // performance.navigation — Navigation Timing Level 1
    {
        JSValue nav = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, nav, "type",          JS_NewInt32(ctx, 0)); // 0 = navigate
        JS_SetPropertyStr(ctx, nav, "redirectCount",  JS_NewInt32(ctx, 0));
        JS_SetPropertyStr(ctx, performance, "navigation", nav);
    }

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

    // getComputedStyle is installed by install_dom_bindings (js_dom_bindings.cpp) where
    // it has access to the full DOMState/LayoutRect cache for accurate CSS values.
    // Do NOT override it here.

    // ---- queueMicrotask ----
    JS_SetPropertyStr(ctx, global, "queueMicrotask",
        JS_NewCFunction(ctx, js_queue_microtask, "queueMicrotask", 1));

    // ---- getSelection ----
    JS_SetPropertyStr(ctx, global, "getSelection",
        JS_NewCFunction(ctx, js_window_get_selection, "getSelection", 0));

    // ---- window.location (full spec: assign, replace, reload, origin, searchParams) ----
    {
        ParsedURL parsed = parse_url(url);
        JSValue location = JS_NewObject(ctx);

        // Store all parsed properties as plain data properties initially
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

        // Install full spec methods and href getter/setter via JS eval.
        // This defines:
        //   - href as a getter/setter (setter updates all derived properties)
        //   - assign(url), replace(url), reload(), toString()
        //   - searchParams (URLSearchParams-like getter)
        //   - window.location setter (setting window.location = "url" navigates)
        const char* location_code = R"JS(
(function(loc, win) {
    // Internal storage for the current href
    var _href = loc.href;
    var _hash = loc.hash;

    // URL parser (mirrors the C++ ParsedURL logic)
    function parseURL(url) {
        var result = {
            href: url, protocol: '', hostname: '', pathname: '/',
            port: '', host: '', origin: '', search: '', hash: ''
        };
        var idx = url.indexOf('://');
        if (idx !== -1) {
            result.protocol = url.substring(0, idx + 1); // "https:"
            var rest = url.substring(idx + 3);
            // Find path start
            var pathStart = -1;
            for (var i = 0; i < rest.length; i++) {
                if (rest[i] === '/' || rest[i] === '?' || rest[i] === '#') {
                    pathStart = i; break;
                }
            }
            var hostPart, pathAndRest;
            if (pathStart !== -1) {
                hostPart = rest.substring(0, pathStart);
                pathAndRest = rest.substring(pathStart);
            } else {
                hostPart = rest;
                pathAndRest = '';
            }
            // Extract port
            var colonPos = hostPart.indexOf(':');
            if (colonPos !== -1) {
                result.hostname = hostPart.substring(0, colonPos);
                result.port = hostPart.substring(colonPos + 1);
            } else {
                result.hostname = hostPart;
                result.port = '';
            }
            result.host = result.port ? (result.hostname + ':' + result.port) : result.hostname;
            result.origin = result.protocol + '//' + result.host;
            // Parse path, search, hash
            if (pathAndRest) {
                var hashPos = pathAndRest.indexOf('#');
                if (hashPos !== -1) {
                    result.hash = pathAndRest.substring(hashPos);
                    pathAndRest = pathAndRest.substring(0, hashPos);
                }
                var qPos = pathAndRest.indexOf('?');
                if (qPos !== -1) {
                    result.search = pathAndRest.substring(qPos);
                    result.pathname = pathAndRest.substring(0, qPos);
                } else {
                    result.pathname = pathAndRest;
                }
                if (!result.pathname) result.pathname = '/';
            } else {
                result.pathname = '/';
            }
        }
        return result;
    }

    // Update all location properties from a parsed URL
    function updateProps(p) {
        _href = p.href;
        loc.hostname = p.hostname;
        loc.pathname = p.pathname;
        loc.protocol = p.protocol;
        loc.origin = p.origin;
        loc.host = p.host;
        loc.port = p.port;
        loc.search = p.search;
        _hash = p.hash;
    }

    // Define href as a getter/setter via Object.defineProperty
    // First delete the existing plain 'href' property
    delete loc.href;
    Object.defineProperty(loc, 'href', {
        get: function() { return _href; },
        set: function(v) {
            var newUrl = String(v);
            // Handle relative URLs by resolving against current origin+path
            if (newUrl.indexOf('://') === -1) {
                if (newUrl.charAt(0) === '/') {
                    // Absolute path relative to origin
                    newUrl = loc.origin + newUrl;
                } else if (newUrl.charAt(0) === '#') {
                    // Hash-only change
                    var base = _href.split('#')[0];
                    newUrl = base + newUrl;
                } else if (newUrl.charAt(0) === '?') {
                    // Search-only change
                    var base2 = _href.split('?')[0].split('#')[0];
                    newUrl = base2 + newUrl;
                } else {
                    // Relative path
                    var basePath = loc.origin + loc.pathname;
                    var lastSlash = basePath.lastIndexOf('/');
                    if (lastSlash !== -1) {
                        newUrl = basePath.substring(0, lastSlash + 1) + newUrl;
                    } else {
                        newUrl = basePath + '/' + newUrl;
                    }
                }
            }

            // Check for pending POST form submission data
            if (loc.__formMethod === "POST" && typeof win.__setPendingFormSubmission === 'function') {
                var formData = loc.__formData || '';
                var formEnctype = loc.__formEnctype || 'application/x-www-form-urlencoded';
                win.__setPendingFormSubmission(formData, formEnctype);
                // Clear the form metadata after consuming it
                delete loc.__formMethod;
                delete loc.__formData;
                delete loc.__formEnctype;
            }

            var oldHref = _href;
            var oldPathname = loc.pathname;
            var oldSearch = loc.search;
            var oldHash = loc.hash;
            var parsed = parseURL(newUrl);
            var isHashOnlyChange = (oldPathname === parsed.pathname &&
                                    oldSearch === parsed.search &&
                                    oldHash !== parsed.hash);

            updateProps(parsed);

            if (isHashOnlyChange && typeof win.__fireHashChange === "function") {
                win.__fireHashChange(oldHref, _href);
            }
        },
        configurable: true,
        enumerable: true
    });

    // Define hash as an accessor so assignments trigger navigation.
    Object.defineProperty(loc, 'hash', {
        get: function() { return _hash; },
        set: function(v) {
            var hashValue = String(v);
            var base = _href.split('#')[0];
            if (hashValue === '') {
                loc.href = base;
            } else {
                if (hashValue.charAt(0) !== '#') hashValue = '#' + hashValue;
                loc.href = base + hashValue;
            }
        },
        configurable: true,
        enumerable: true
    });

    // Define searchParams as a getter (URLSearchParams-like)
    Object.defineProperty(loc, 'searchParams', {
        get: function() {
            var params = {};
            var s = loc.search;
            if (s && s.charAt(0) === '?') s = s.substring(1);
            if (!s) return {
                get: function() { return null; },
                has: function() { return false; },
                toString: function() { return ''; },
                entries: function() { return []; },
                keys: function() { return []; },
                values: function() { return []; },
                forEach: function() {}
            };
            var pairs = s.split('&');
            for (var i = 0; i < pairs.length; i++) {
                var eq = pairs[i].indexOf('=');
                if (eq !== -1) {
                    var key = decodeURIComponent(pairs[i].substring(0, eq));
                    var val = decodeURIComponent(pairs[i].substring(eq + 1));
                    params[key] = val;
                } else {
                    params[decodeURIComponent(pairs[i])] = '';
                }
            }
            return {
                get: function(k) { return params.hasOwnProperty(k) ? params[k] : null; },
                has: function(k) { return params.hasOwnProperty(k); },
                toString: function() { return loc.search ? loc.search.substring(1) : ''; },
                entries: function() {
                    var r = [];
                    for (var k in params) if (params.hasOwnProperty(k)) r.push([k, params[k]]);
                    return r;
                },
                keys: function() {
                    var r = [];
                    for (var k in params) if (params.hasOwnProperty(k)) r.push(k);
                    return r;
                },
                values: function() {
                    var r = [];
                    for (var k in params) if (params.hasOwnProperty(k)) r.push(params[k]);
                    return r;
                },
                forEach: function(cb) {
                    for (var k in params) if (params.hasOwnProperty(k)) cb(params[k], k);
                }
            };
        },
        configurable: true,
        enumerable: true
    });

    // Methods
    // assign(url): navigate to url, adding to history (same-origin only per spec)
    loc.assign = function(url) { loc.href = String(url); };
    // replace(url): navigate to url, replacing current history entry
    loc.replace = function(url) {
        var newUrl = String(url);
        if (typeof history !== 'undefined' && history &&
            typeof history.replaceState === 'function') {
            history.replaceState(history.state, '', newUrl);
        } else {
            loc.href = newUrl;
        }
    };
    loc.reload = function() { loc.href = _href; };
    loc.toString = function() { return _href; };

    // Make window.location a getter/setter so that:
    //   window.location = "http://example.com" triggers navigation
    // We need to store the location object reference and redefine the property.
    var _locObj = loc;

    // First delete the plain 'location' property we set earlier via C++
    // (it will be re-set below via defineProperty)
    // NOTE: We do this from within the eval since the global hasn't had
    // 'location' set via defineProperty yet -- it will be set AFTER this eval.
    // Instead, we'll define it from within the function using 'win'.

    Object.defineProperty(win, 'location', {
        get: function() { return _locObj; },
        set: function(v) { _locObj.href = String(v); },
        configurable: true,
        enumerable: true
    });
})
)JS";
        JSValue loc_fn = JS_Eval(ctx, location_code, std::strlen(location_code),
                                  "<location-spec>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsFunction(ctx, loc_fn)) {
            JSValue args[2] = { location, global };
            JSValue ret = JS_Call(ctx, loc_fn, JS_UNDEFINED, 2, args);
            JS_FreeValue(ctx, ret);
        }
        JS_FreeValue(ctx, loc_fn);

        // Note: We do NOT set location on global via JS_SetPropertyStr here
        // because the JS code above already defined it via Object.defineProperty
        // with getter/setter. Setting it again would overwrite the defineProperty.
        // However, we need to free the location JSValue since the JS closure
        // captured a reference to it.
        JS_FreeValue(ctx, location);
    }

    // ---- window.__setPendingFormSubmission (internal POST form support) ----
    // This native function is called by the location.href setter when a POST form is submitted.
    // It stores the form body and content-type for the fetch pipeline to use.
    JS_SetPropertyStr(ctx, global, "__setPendingFormSubmission",
        JS_NewCFunction(ctx, js_window_set_pending_form_submission, "__setPendingFormSubmission", 2));
    JS_SetPropertyStr(ctx, global, "__fireHashChange",
        JS_NewCFunction(ctx, js_window_fire_hashchange, "__fireHashChange", 2));

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
    JS_SetPropertyStr(ctx, navigator, "deviceMemory", JS_NewFloat64(ctx, 8.0));
    JS_SetPropertyStr(ctx, navigator, "pdfViewerEnabled", JS_TRUE);

    // ---- navigator.connection (NetworkInformation stub) ----
    {
        JSValue conn = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, conn, "effectiveType", JS_NewString(ctx, "4g"));
        JS_SetPropertyStr(ctx, conn, "downlink", JS_NewFloat64(ctx, 10.0));
        JS_SetPropertyStr(ctx, conn, "rtt", JS_NewInt32(ctx, 50));
        JS_SetPropertyStr(ctx, conn, "saveData", JS_FALSE);
        JS_SetPropertyStr(ctx, conn, "type", JS_NewString(ctx, "wifi"));
        JS_SetPropertyStr(ctx, conn, "downlinkMax", JS_NewFloat64(ctx, 100.0));
        JS_SetPropertyStr(ctx, conn, "onchange", JS_NULL);
        JS_SetPropertyStr(ctx, conn, "addEventListener",
            JS_NewCFunction(ctx, [](JSContext*, JSValueConst, int, JSValueConst*) -> JSValue {
                return JS_UNDEFINED;
            }, "addEventListener", 2));
        JS_SetPropertyStr(ctx, conn, "removeEventListener",
            JS_NewCFunction(ctx, [](JSContext*, JSValueConst, int, JSValueConst*) -> JSValue {
                return JS_UNDEFINED;
            }, "removeEventListener", 2));
        JS_SetPropertyStr(ctx, navigator, "connection", conn);
    }

    // ---- navigator.serviceWorker (ServiceWorkerContainer stub) ----
    // Full stub with register/ready/controller; register returns a resolved
    // ServiceWorkerRegistration so sites that call register() don't throw.
    {
        JSValue sw_container = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, sw_container, "controller", JS_NULL);
        JS_SetPropertyStr(ctx, sw_container, "oncontrollerchange", JS_NULL);
        JS_SetPropertyStr(ctx, sw_container, "onmessage", JS_NULL);
        JS_SetPropertyStr(ctx, sw_container, "onmessageerror", JS_NULL);
        JS_SetPropertyStr(ctx, sw_container, "register",
            JS_NewCFunction(ctx, [](JSContext* c, JSValueConst, int, JSValueConst*) -> JSValue {
                // Build a ServiceWorkerRegistration stub
                JSValue reg = JS_NewObject(c);
                JS_SetPropertyStr(c, reg, "installing", JS_NULL);
                JS_SetPropertyStr(c, reg, "waiting", JS_NULL);
                JS_SetPropertyStr(c, reg, "active", JS_NULL);
                JS_SetPropertyStr(c, reg, "scope", JS_NewString(c, "/"));
                JS_SetPropertyStr(c, reg, "updateViaCache", JS_NewString(c, "imports"));
                JS_SetPropertyStr(c, reg, "navigationPreload", JS_NewObject(c));
                JS_SetPropertyStr(c, reg, "onupdatefound", JS_NULL);
                JS_SetPropertyStr(c, reg, "update",
                    JS_NewCFunction(c, [](JSContext* c2, JSValueConst, int, JSValueConst*) -> JSValue {
                        JSValue rf[2];
                        JSValue promise = JS_NewPromiseCapability(c2, rf);
                        if (JS_IsException(promise)) return promise;
                        JSValue ret = JS_Call(c2, rf[0], JS_UNDEFINED, 0, nullptr);
                        JS_FreeValue(c2, ret);
                        JS_FreeValue(c2, rf[0]);
                        JS_FreeValue(c2, rf[1]);
                        return promise;
                    }, "update", 0));
                JS_SetPropertyStr(c, reg, "unregister",
                    JS_NewCFunction(c, [](JSContext* c2, JSValueConst, int, JSValueConst*) -> JSValue {
                        JSValue rf[2];
                        JSValue promise = JS_NewPromiseCapability(c2, rf);
                        if (JS_IsException(promise)) return promise;
                        JSValue t = JS_TRUE;
                        JSValue ret = JS_Call(c2, rf[0], JS_UNDEFINED, 1, &t);
                        JS_FreeValue(c2, ret);
                        JS_FreeValue(c2, rf[0]);
                        JS_FreeValue(c2, rf[1]);
                        return promise;
                    }, "unregister", 0));
                JS_SetPropertyStr(c, reg, "addEventListener",
                    JS_NewCFunction(c, [](JSContext*, JSValueConst, int, JSValueConst*) -> JSValue {
                        return JS_UNDEFINED;
                    }, "addEventListener", 2));
                JS_SetPropertyStr(c, reg, "removeEventListener",
                    JS_NewCFunction(c, [](JSContext*, JSValueConst, int, JSValueConst*) -> JSValue {
                        return JS_UNDEFINED;
                    }, "removeEventListener", 2));
                // Return Promise.resolve(reg)
                JSValue rf[2];
                JSValue promise = JS_NewPromiseCapability(c, rf);
                if (JS_IsException(promise)) { JS_FreeValue(c, reg); return promise; }
                JSValue ret = JS_Call(c, rf[0], JS_UNDEFINED, 1, &reg);
                JS_FreeValue(c, ret);
                JS_FreeValue(c, rf[0]);
                JS_FreeValue(c, rf[1]);
                JS_FreeValue(c, reg);
                return promise;
            }, "register", 2));
        JS_SetPropertyStr(ctx, sw_container, "getRegistrations",
            JS_NewCFunction(ctx, [](JSContext* c, JSValueConst, int, JSValueConst*) -> JSValue {
                JSValue arr = JS_NewArray(c);
                JSValue rf[2];
                JSValue promise = JS_NewPromiseCapability(c, rf);
                if (JS_IsException(promise)) { JS_FreeValue(c, arr); return promise; }
                JSValue ret = JS_Call(c, rf[0], JS_UNDEFINED, 1, &arr);
                JS_FreeValue(c, ret);
                JS_FreeValue(c, rf[0]);
                JS_FreeValue(c, rf[1]);
                JS_FreeValue(c, arr);
                return promise;
            }, "getRegistrations", 0));
        JS_SetPropertyStr(ctx, sw_container, "getRegistration",
            JS_NewCFunction(ctx, [](JSContext* c, JSValueConst, int, JSValueConst*) -> JSValue {
                JSValue rf[2];
                JSValue promise = JS_NewPromiseCapability(c, rf);
                if (JS_IsException(promise)) return promise;
                JSValue undef = JS_UNDEFINED;
                JSValue ret = JS_Call(c, rf[0], JS_UNDEFINED, 1, &undef);
                JS_FreeValue(c, ret);
                JS_FreeValue(c, rf[0]);
                JS_FreeValue(c, rf[1]);
                return promise;
            }, "getRegistration", 1));
        JS_SetPropertyStr(ctx, sw_container, "addEventListener",
            JS_NewCFunction(ctx, [](JSContext*, JSValueConst, int, JSValueConst*) -> JSValue {
                return JS_UNDEFINED;
            }, "addEventListener", 2));
        JS_SetPropertyStr(ctx, sw_container, "removeEventListener",
            JS_NewCFunction(ctx, [](JSContext*, JSValueConst, int, JSValueConst*) -> JSValue {
                return JS_UNDEFINED;
            }, "removeEventListener", 2));
        JS_SetPropertyStr(ctx, navigator, "serviceWorker", sw_container);
    }

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
    if (!std::isfinite(device_pixel_ratio) || device_pixel_ratio <= 0.0f) {
        device_pixel_ratio = 1.0f;
    }
    JS_SetPropertyStr(ctx, global, "devicePixelRatio",
        JS_NewFloat64(ctx, static_cast<double>(device_pixel_ratio)));

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

    // ---- window.removeEventListener (fallback) ----
    // Provides a no-op removeEventListener when DOM bindings are not installed.
    // When js_dom_bindings is installed later it overrides this with the real impl.
    JS_SetPropertyStr(ctx, global, "removeEventListener",
        JS_NewCFunction(ctx, [](JSContext*, JSValueConst, int, JSValueConst*) -> JSValue {
            return JS_UNDEFINED;
        }, "removeEventListener", 2));

    // ---- window.history (full History API) ----
    {
        // Create HistoryState and seed with the initial page entry.
        // Store the full URL (not just path component) so back/forward restore correctly.
        auto* hs = new HistoryState();
        hs->entries.push_back({"", "", url});
        hs->current_index = 0;

        // Stash pointer on global for later retrieval
        JS_SetPropertyStr(ctx, global, "__history_state_ptr",
            JS_NewInt64(ctx, static_cast<int64_t>(
                reinterpret_cast<uintptr_t>(hs))));

        JSValue history = JS_NewObject(ctx);

        // Install C++ methods
        JS_SetPropertyStr(ctx, history, "pushState",
            JS_NewCFunction(ctx, js_history_push_state, "pushState", 3));
        JS_SetPropertyStr(ctx, history, "replaceState",
            JS_NewCFunction(ctx, js_history_replace_state, "replaceState", 3));
        JS_SetPropertyStr(ctx, history, "back",
            JS_NewCFunction(ctx, js_history_back, "back", 0));
        JS_SetPropertyStr(ctx, history, "forward",
            JS_NewCFunction(ctx, js_history_forward, "forward", 0));
        JS_SetPropertyStr(ctx, history, "go",
            JS_NewCFunction(ctx, js_history_go, "go", 1));

        // Install __getLength and __getState helpers for defineProperty getters
        JS_SetPropertyStr(ctx, history, "__getLength",
            JS_NewCFunction(ctx, js_history_get_length, "__getLength", 0));
        JS_SetPropertyStr(ctx, history, "__getState",
            JS_NewCFunction(ctx, js_history_get_state, "__getState", 0));

        JS_SetPropertyStr(ctx, global, "history", history);
        JS_SetPropertyStr(ctx, global, "onhashchange", JS_NULL);
        JS_SetPropertyStr(ctx, global, "onpopstate", JS_NULL);
    }

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

    // ---- window.visualViewport ----
    {
        JSValue vv = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, vv, "width", JS_NewFloat64(ctx, static_cast<double>(width)));
        JS_SetPropertyStr(ctx, vv, "height", JS_NewFloat64(ctx, static_cast<double>(height)));
        JS_SetPropertyStr(ctx, vv, "offsetLeft", JS_NewFloat64(ctx, 0.0));
        JS_SetPropertyStr(ctx, vv, "offsetTop", JS_NewFloat64(ctx, 0.0));
        JS_SetPropertyStr(ctx, vv, "pageLeft", JS_NewFloat64(ctx, 0.0));
        JS_SetPropertyStr(ctx, vv, "pageTop", JS_NewFloat64(ctx, 0.0));
        JS_SetPropertyStr(ctx, vv, "scale", JS_NewFloat64(ctx, 1.0));
        JS_SetPropertyStr(ctx, vv, "onresize", JS_NULL);
        JS_SetPropertyStr(ctx, vv, "onscroll", JS_NULL);
        JS_SetPropertyStr(ctx, vv, "addEventListener",
            JS_NewCFunction(ctx, [](JSContext*, JSValueConst, int, JSValueConst*) -> JSValue {
                return JS_UNDEFINED;
            }, "addEventListener", 2));
        JS_SetPropertyStr(ctx, vv, "removeEventListener",
            JS_NewCFunction(ctx, [](JSContext*, JSValueConst, int, JSValueConst*) -> JSValue {
                return JS_UNDEFINED;
            }, "removeEventListener", 2));
        JS_SetPropertyStr(ctx, global, "visualViewport", vv);
    }

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
    JS_SetPropertyStr(ctx, css_obj, "escape",
        JS_NewCFunction(ctx, js_css_escape, "escape", 1));
    JS_SetPropertyStr(ctx, css_obj, "registerProperty",
        JS_NewCFunction(ctx, js_css_register_property, "registerProperty", 1));
    JS_SetPropertyStr(ctx, global, "CSS", css_obj);

    // ---- Intl constructors (NumberFormat / DateTimeFormat / Collator / PluralRules) ----
    JSValue intl_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, intl_obj, "NumberFormat",
        JS_NewCFunction2(ctx, js_intl_number_format_constructor,
                         "NumberFormat", 0, JS_CFUNC_constructor, 0));
    JS_SetPropertyStr(ctx, intl_obj, "DateTimeFormat",
        JS_NewCFunction2(ctx, js_intl_date_time_format_constructor,
                         "DateTimeFormat", 0, JS_CFUNC_constructor, 0));
    JS_SetPropertyStr(ctx, intl_obj, "Collator",
        JS_NewCFunction2(ctx, js_intl_collator_constructor,
                         "Collator", 0, JS_CFUNC_constructor, 0));
    JS_SetPropertyStr(ctx, intl_obj, "PluralRules",
        JS_NewCFunction2(ctx, js_intl_plural_rules_constructor,
                         "PluralRules", 0, JS_CFUNC_constructor, 0));
    JS_SetPropertyStr(ctx, intl_obj, "ListFormat",
        JS_NewCFunction2(ctx, js_intl_list_format_constructor,
                         "ListFormat", 0, JS_CFUNC_constructor, 0));
    JS_SetPropertyStr(ctx, intl_obj, "RelativeTimeFormat",
        JS_NewCFunction2(ctx, js_intl_relative_time_format_constructor,
                         "RelativeTimeFormat", 0, JS_CFUNC_constructor, 0));
    JS_SetPropertyStr(ctx, intl_obj, "DisplayNames",
        JS_NewCFunction2(ctx, js_intl_display_names_constructor,
                         "DisplayNames", 0, JS_CFUNC_constructor, 0));
    JS_SetPropertyStr(ctx, intl_obj, "Segmenter",
        JS_NewCFunction2(ctx, js_intl_segmenter_constructor,
                         "Segmenter", 0, JS_CFUNC_constructor, 0));
    JS_SetPropertyStr(ctx, global, "Intl", intl_obj);

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

    // ---- history.length and history.state getters ----
    Object.defineProperty(history, 'length', {
        get: function() { return history.__getLength(); },
        configurable: true
    });
    Object.defineProperty(history, 'state', {
        get: function() { return history.__getState(); },
        configurable: true
    });

    // ---- window.crypto placeholder ----
    // The real crypto object with native getRandomValues/randomUUID is installed
    // below as C++ native functions after this eval block completes.
    if (typeof window.crypto === 'undefined' || !window.crypto) {
        window.crypto = { subtle: {} };
    }

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
        } else if (init && typeof init === 'object') {
            // Support FormData, another URLSearchParams, or plain object with entries()
            if (typeof init.entries === 'function') {
                // FormData or URLSearchParams — iterate entries
                var entries = init.entries();
                if (entries && typeof entries[Symbol.iterator] === 'function') {
                    for (var pair of entries) {
                        if (pair && pair.length >= 2) {
                            this._params.push([String(pair[0]), String(pair[1])]);
                        }
                    }
                } else if (Array.isArray(entries)) {
                    for (var j = 0; j < entries.length; j++) {
                        if (entries[j] && entries[j].length >= 2) {
                            this._params.push([String(entries[j][0]), String(entries[j][1])]);
                        }
                    }
                }
            } else if (Array.isArray(init)) {
                // Array of [key, value] pairs
                for (var k = 0; k < init.length; k++) {
                    if (Array.isArray(init[k]) && init[k].length >= 2) {
                        this._params.push([String(init[k][0]), String(init[k][1])]);
                    }
                }
            } else {
                // Plain object: iterate own properties
                var keys = Object.keys(init);
                for (var m = 0; m < keys.length; m++) {
                    this._params.push([keys[m], String(init[keys[m]])]);
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
        return this._params.slice();
    };
    URLSearchParams.prototype[Symbol.iterator] = function() {
        var arr = this._params;
        var idx = 0;
        return {
            next: function() {
                if (idx < arr.length) {
                    return { value: arr[idx++].slice(), done: false };
                }
                return { value: undefined, done: true };
            },
            [Symbol.iterator]: function() { return this; }
        };
    };
    URLSearchParams.prototype.keys = function() {
        var result = this._params.map(function(p) { return p[0]; });
        result[Symbol.iterator] = Array.prototype[Symbol.iterator];
        return result;
    };
    URLSearchParams.prototype.values = function() {
        var result = this._params.map(function(p) { return p[1]; });
        result[Symbol.iterator] = Array.prototype[Symbol.iterator];
        return result;
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
    // Replace JS stubs for URL.createObjectURL / URL.revokeObjectURL with
    // real C++ native functions backed by the blob registry.
    // ------------------------------------------------------------------
    {
        JSValue g2 = JS_GetGlobalObject(ctx);
        JSValue url_ctor = JS_GetPropertyStr(ctx, g2, "URL");
        if (JS_IsObject(url_ctor)) {
            JS_SetPropertyStr(ctx, url_ctor, "createObjectURL",
                JS_NewCFunction(ctx, js_url_create_object_url, "createObjectURL", 1));
            JS_SetPropertyStr(ctx, url_ctor, "revokeObjectURL",
                JS_NewCFunction(ctx, js_url_revoke_object_url, "revokeObjectURL", 1));
        }
        JS_FreeValue(ctx, url_ctor);
        JS_FreeValue(ctx, g2);
    }

    // ------------------------------------------------------------------
    // Replace window.crypto with native C++ implementation using arc4random_buf
    // (overrides the placeholder set by the extended_setup eval above).
    // ------------------------------------------------------------------
    {
        JSValue g3 = JS_GetGlobalObject(ctx);
        JSValue crypto_obj = JS_NewObject(ctx);

        JS_SetPropertyStr(ctx, crypto_obj, "getRandomValues",
            JS_NewCFunction(ctx, js_window_crypto_get_random_values, "getRandomValues", 1));
        JS_SetPropertyStr(ctx, crypto_obj, "randomUUID",
            JS_NewCFunction(ctx, js_window_crypto_random_uuid, "randomUUID", 0));

        // crypto.subtle: minimal stub object
        JSValue subtle_obj = JS_NewObject(ctx);
        // Install a digest that returns a rejected promise (not supported at window level)
        const char* digest_stub = "(function() { return Promise.reject(new Error('Not supported')); })";
        JSValue digest_fn = JS_Eval(ctx, digest_stub, std::strlen(digest_stub),
                                     "<subtle-digest>", JS_EVAL_TYPE_GLOBAL);
        if (!JS_IsException(digest_fn)) {
            JS_SetPropertyStr(ctx, subtle_obj, "digest", digest_fn);
        } else {
            JS_FreeValue(ctx, digest_fn);
        }
        JS_SetPropertyStr(ctx, crypto_obj, "subtle", subtle_obj);

        // Install on globalThis
        JS_SetPropertyStr(ctx, g3, "crypto", crypto_obj);
        JS_FreeValue(ctx, g3);
    }

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

    // ------------------------------------------------------------------
    // ES2022+ / modern API polyfills
    // ------------------------------------------------------------------
    {
        const char* polyfill_structuredclone_src = R"JS(
        if (typeof globalThis.structuredClone === 'undefined') {
            globalThis.structuredClone = function(obj) {
                return JSON.parse(JSON.stringify(obj));
            };
        }
)JS";
        JSValue polyfill_structuredclone = JS_Eval(ctx, polyfill_structuredclone_src, std::strlen(polyfill_structuredclone_src),
                                                  "<polyfill-structuredClone>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(polyfill_structuredclone)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, polyfill_structuredclone);
    }

    {
        const char* polyfill_queuemicrotask_src = R"JS(
        if (typeof globalThis.queueMicrotask === 'undefined') {
            globalThis.queueMicrotask = function(fn) {
                Promise.resolve().then(fn);
            };
        }
)JS";
        JSValue polyfill_queuemicrotask = JS_Eval(ctx, polyfill_queuemicrotask_src, std::strlen(polyfill_queuemicrotask_src),
                                                  "<polyfill-queueMicrotask>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(polyfill_queuemicrotask)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, polyfill_queuemicrotask);
    }

    {
        const char* polyfill_hasown_src = R"JS(
        if (!Object.hasOwn) {
            Object.hasOwn = function(obj, prop) {
                return Object.prototype.hasOwnProperty.call(obj, prop);
            };
        }
)JS";
        JSValue polyfill_hasown = JS_Eval(ctx, polyfill_hasown_src, std::strlen(polyfill_hasown_src),
                                         "<polyfill-ObjectHasOwn>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(polyfill_hasown)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, polyfill_hasown);
    }

    {
        const char* polyfill_array_string_at_src = R"JS(
        if (!Array.prototype.at) {
            Array.prototype.at = function(index) {
                index = Math.trunc(index) || 0;
                if (index < 0) index += this.length;
                if (index < 0 || index >= this.length) return undefined;
                return this[index];
            };
        }
        if (!String.prototype.at) {
            String.prototype.at = function(index) {
                index = Math.trunc(index) || 0;
                if (index < 0) index += this.length;
                if (index < 0 || index >= this.length) return undefined;
                return this[index];
            };
        }
)JS";
        JSValue polyfill_array_string_at = JS_Eval(ctx, polyfill_array_string_at_src, std::strlen(polyfill_array_string_at_src),
                                                  "<polyfill-array-string-at>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(polyfill_array_string_at)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, polyfill_array_string_at);
    }

    {
        const char* polyfill_findlast_src = R"JS(
        if (!Array.prototype.findLast) {
            Array.prototype.findLast = function(fn) {
                for (var i = this.length - 1; i >= 0; i--) {
                    if (fn(this[i], i, this)) return this[i];
                }
            };
            Array.prototype.findLastIndex = function(fn) {
                for (var i = this.length - 1; i >= 0; i--) {
                    if (fn(this[i], i, this)) return i;
                }
                return -1;
            };
        }
)JS";
        JSValue polyfill_findlast = JS_Eval(ctx, polyfill_findlast_src, std::strlen(polyfill_findlast_src),
                                           "<polyfill-findLast>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(polyfill_findlast)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, polyfill_findlast);
    }

    {
        const char* polyfill_fromentries_src = R"JS(
        if (!Object.fromEntries) {
            Object.fromEntries = function(entries) {
                return Array.from(entries).reduce(function(acc, kv) {
                    acc[kv[0]] = kv[1];
                    return acc;
                }, {});
            };
        }
)JS";
        JSValue polyfill_fromentries = JS_Eval(ctx, polyfill_fromentries_src, std::strlen(polyfill_fromentries_src),
                                              "<polyfill-fromEntries>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(polyfill_fromentries)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, polyfill_fromentries);
    }

    {
        const char* polyfill_abortsignal_src = R"JS(
        if (typeof globalThis.AbortSignal !== 'undefined' && !globalThis.AbortSignal.timeout) {
            globalThis.AbortSignal.timeout = function(ms) {
                var controller = new AbortController();
                setTimeout(function() { controller.abort(new DOMException('TimeoutError', 'TimeoutError')); }, ms);
                return controller.signal;
            };
            globalThis.AbortSignal.any = function(signals) {
                var controller = new AbortController();
                signals.forEach(function(s) {
                    if (s.aborted) { controller.abort(s.reason); return; }
                    s.addEventListener('abort', function() { controller.abort(s.reason); });
                });
                return controller.signal;
            };
        }
)JS";
        JSValue polyfill_abortsignal = JS_Eval(ctx, polyfill_abortsignal_src, std::strlen(polyfill_abortsignal_src),
                                              "<polyfill-abortSignal>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(polyfill_abortsignal)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, polyfill_abortsignal);
    }

    {
        const char* polyfill_reporterror_src = R"JS(
        if (typeof globalThis.reportError === 'undefined') {
            globalThis.reportError = function(err) { console.error(err); };
        }
)JS";
        JSValue polyfill_reporterror = JS_Eval(ctx, polyfill_reporterror_src, std::strlen(polyfill_reporterror_src),
                                              "<polyfill-reportError>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(polyfill_reporterror)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, polyfill_reporterror);
    }

    {
        const char* polyfill_useragentdata_src = R"JS(
        if (typeof navigator !== 'undefined' && !navigator.userAgentData) {
            Object.defineProperty(navigator, 'userAgentData', {
                get: function() {
                    return {
                        brands: [
                            {brand: 'Chromium', version: '120'},
                            {brand: 'Not A;Brand', version: '99'}
                        ],
                        mobile: false,
                        platform: 'macOS',
                        getHighEntropyValues: function(hints) {
                            return Promise.resolve({
                                platform: 'macOS', platformVersion: '14.0',
                                architecture: 'arm', model: '', uaFullVersion: '120.0.0.0'
                            });
                        },
                        toJSON: function() { return {}; }
                    };
                }
            });
        }
)JS";
        JSValue polyfill_useragentdata = JS_Eval(ctx, polyfill_useragentdata_src, std::strlen(polyfill_useragentdata_src),
                                                "<polyfill-userAgentData>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(polyfill_useragentdata)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, polyfill_useragentdata);
    }

    {
        const char* polyfill_scheduler_src = R"JS(
        if (typeof globalThis.scheduler === 'undefined') {
            globalThis.scheduler = {
                postTask: function(fn, opts) { return Promise.resolve().then(fn); },
                yield: function() { return Promise.resolve(); }
            };
        }
)JS";
        JSValue polyfill_scheduler = JS_Eval(ctx, polyfill_scheduler_src, std::strlen(polyfill_scheduler_src),
                                            "<polyfill-scheduler>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(polyfill_scheduler)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, polyfill_scheduler);
    }

    {
        const char* polyfill_aggregate_error_src = R"JS(
        if (typeof globalThis.AggregateError === 'undefined') {
            var AggregateError = function(errors, message) {
                if (!(this instanceof AggregateError)) {
                    return new AggregateError(errors, message);
                }
                var msg = message || '';
                this.name = 'AggregateError';
                this.message = msg;
                this.errors = Array.isArray(errors) ? errors : [];
                if (typeof Error.captureStackTrace === 'function') {
                    Error.captureStackTrace(this, AggregateError);
                } else {
                    this.stack = new Error().stack;
                }
            };
            AggregateError.prototype = Object.create(Error.prototype);
            AggregateError.prototype.constructor = AggregateError;
            AggregateError.prototype.toString = function() {
                return this.name + ': ' + this.message;
            };
            globalThis.AggregateError = AggregateError;
        }
)JS";
        JSValue polyfill_aggregate_error = JS_Eval(ctx, polyfill_aggregate_error_src, std::strlen(polyfill_aggregate_error_src),
                                                  "<polyfill-AggregateError>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(polyfill_aggregate_error)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, polyfill_aggregate_error);
    }

    {
        const char* polyfill_symbol_async_iterator_src = R"JS(
        if (typeof globalThis.Symbol !== 'undefined' && typeof globalThis.Symbol.asyncIterator === 'undefined') {
            globalThis.Symbol.asyncIterator = (typeof globalThis.Symbol.for === 'function')
                ? globalThis.Symbol.for('Symbol.asyncIterator')
                : Symbol('Symbol.asyncIterator');
        }
)JS";
        JSValue polyfill_symbol_async_iterator = JS_Eval(ctx, polyfill_symbol_async_iterator_src, std::strlen(polyfill_symbol_async_iterator_src),
                                                  "<polyfill-Symbol.asyncIterator>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(polyfill_symbol_async_iterator)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, polyfill_symbol_async_iterator);
    }

    {
        const char* polyfill_promise_allsettled_src = R"JS(
        if (typeof globalThis.Promise !== 'undefined' && !globalThis.Promise.allSettled) {
            Promise.allSettled = function(iterable) {
                return Promise.resolve(iterable).then(function(values) {
                    var pending = values.length;
                    var resolved = new Array(values.length);
                    if (!values.length) {
                        return [];
                    }

                    return new Promise(function(resolve) {
                        for (var i = 0; i < values.length; i++) {
                            (function(i) {
                                Promise.resolve(values[i]).then(function(value) {
                                    resolved[i] = {
                                        status: 'fulfilled',
                                        value: value
                                    };
                                    pending--;
                                    if (pending === 0) {
                                        resolve(resolved);
                                    }
                                }, function(reason) {
                                    resolved[i] = {
                                        status: 'rejected',
                                        reason: reason
                                    };
                                    pending--;
                                    if (pending === 0) {
                                        resolve(resolved);
                                    }
                                });
                            })(i);
                        }
                    });
                });
            };
        }
)JS";
        JSValue polyfill_promise_allsettled = JS_Eval(ctx, polyfill_promise_allsettled_src, std::strlen(polyfill_promise_allsettled_src),
                                                  "<polyfill-PromiseAllSettled>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(polyfill_promise_allsettled)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, polyfill_promise_allsettled);
    }

    {
        const char* polyfill_promise_any_src = R"JS(
        if (typeof globalThis.Promise !== 'undefined' && !globalThis.Promise.any) {
            Promise.any = function(iterable) {
                return new Promise(function(resolve, reject) {
                    if (!iterable.length) {
                        reject(new AggregateError([], 'All promises were rejected'));
                        return;
                    }

                    var errors = [];
                    var pending = iterable.length;
                    for (var i = 0; i < iterable.length; i++) {
                        (function(i) {
                            Promise.resolve(iterable[i]).then(function(value) {
                                resolve(value);
                            }, function(reason) {
                                errors[i] = reason;
                                pending--;
                                if (pending === 0) {
                                    reject(new AggregateError(errors, 'All promises were rejected'));
                                }
                            });
                        })(i);
                    }
                });
            };
        }
)JS";
        JSValue polyfill_promise_any = JS_Eval(ctx, polyfill_promise_any_src, std::strlen(polyfill_promise_any_src),
                                                  "<polyfill-PromiseAny>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(polyfill_promise_any)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, polyfill_promise_any);
    }

    // ---- ES2021-2023 polyfills ----
    {
        const char* polyfill_es2023_src = R"JS(
// String.prototype.replaceAll (ES2021)
if (!String.prototype.replaceAll) {
    String.prototype.replaceAll = function(search, replacement) {
        if (search instanceof RegExp) {
            if (!search.global) throw new TypeError('replaceAll requires a global regexp');
            return this.replace(search, replacement);
        }
        return this.split(String(search)).join(
            typeof replacement === 'function'
                ? replacement(search, 0, this)
                : String(replacement)
        );
    };
}

// Array.prototype.flat (ES2019)
if (!Array.prototype.flat) {
    Array.prototype.flat = function(depth) {
        var d = depth === undefined ? 1 : Math.floor(depth);
        if (d < 1) return Array.prototype.slice.call(this);
        return (function flat(arr, d) {
            var result = [];
            arr.forEach(function(item) {
                if (Array.isArray(item) && d > 0) {
                    result.push.apply(result, flat(item, d - 1));
                } else {
                    result.push(item);
                }
            });
            return result;
        })(this, d);
    };
}

// Array.prototype.flatMap (ES2019)
if (!Array.prototype.flatMap) {
    Array.prototype.flatMap = function(fn, thisArg) {
        return Array.prototype.flat.call(
            Array.prototype.map.call(this, fn, thisArg), 1
        );
    };
}

// Array.prototype.toReversed (ES2023)
if (!Array.prototype.toReversed) {
    Array.prototype.toReversed = function() {
        return Array.prototype.slice.call(this).reverse();
    };
}

// Array.prototype.toSorted (ES2023)
if (!Array.prototype.toSorted) {
    Array.prototype.toSorted = function(compareFn) {
        return Array.prototype.slice.call(this).sort(compareFn);
    };
}

// Array.prototype.toSpliced (ES2023)
if (!Array.prototype.toSpliced) {
    Array.prototype.toSpliced = function(start, deleteCount) {
        var copy = Array.prototype.slice.call(this);
        var args = [start, deleteCount].concat(Array.prototype.slice.call(arguments, 2));
        Array.prototype.splice.apply(copy, args);
        return copy;
    };
}

// Array.prototype.with (ES2023)
if (!Array.prototype.with) {
    Array.prototype.with = function(index, value) {
        var copy = Array.prototype.slice.call(this);
        var len = copy.length;
        var i = index < 0 ? len + index : index;
        if (i < 0 || i >= len) throw new RangeError('Invalid index');
        copy[i] = value;
        return copy;
    };
}

// Object.groupBy (ES2024)
if (!Object.groupBy) {
    Object.groupBy = function(iterable, keyFn) {
        var result = Object.create(null);
        var i = 0;
        for (var item of iterable) {
            var key = keyFn(item, i++);
            key = String(key);
            if (!Object.prototype.hasOwnProperty.call(result, key)) result[key] = [];
            result[key].push(item);
        }
        return result;
    };
}

// Map.groupBy (ES2024)
if (!Map.groupBy) {
    Map.groupBy = function(iterable, keyFn) {
        var result = new Map();
        var i = 0;
        for (var item of iterable) {
            var key = keyFn(item, i++);
            if (!result.has(key)) result.set(key, []);
            result.get(key).push(item);
        }
        return result;
    };
}

// String.prototype.padStart (ES2017 - some older environments lack it)
if (!String.prototype.padStart) {
    String.prototype.padStart = function(targetLength, padString) {
        var str = String(this);
        targetLength = Math.floor(targetLength) || 0;
        padString = padString !== undefined ? String(padString) : ' ';
        if (str.length >= targetLength || !padString.length) return str;
        var pad = '';
        var need = targetLength - str.length;
        while (pad.length < need) pad += padString;
        return pad.slice(0, need) + str;
    };
}

// String.prototype.padEnd (ES2017)
if (!String.prototype.padEnd) {
    String.prototype.padEnd = function(targetLength, padString) {
        var str = String(this);
        targetLength = Math.floor(targetLength) || 0;
        padString = padString !== undefined ? String(padString) : ' ';
        if (str.length >= targetLength || !padString.length) return str;
        var pad = '';
        var need = targetLength - str.length;
        while (pad.length < need) pad += padString;
        return str + pad.slice(0, need);
    };
}

// String.prototype.trimStart / trimEnd (ES2019)
if (!String.prototype.trimStart) {
    String.prototype.trimStart = function() { return this.replace(/^\s+/, ''); };
    String.prototype.trimLeft = String.prototype.trimStart;
}
if (!String.prototype.trimEnd) {
    String.prototype.trimEnd = function() { return this.replace(/\s+$/, ''); };
    String.prototype.trimRight = String.prototype.trimEnd;
}
)JS";
        JSValue polyfill_es2023 = JS_Eval(ctx, polyfill_es2023_src, std::strlen(polyfill_es2023_src),
                                         "<polyfill-es2023>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(polyfill_es2023)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, polyfill_es2023);
    }

    // ---- window.localStorage and window.sessionStorage ----
    ParsedURL parsed_url = parse_url(url);
    install_web_storage_bindings(ctx, parsed_url.origin);

    // ---- Worker API ----
    install_worker_bindings(ctx);
}

void flush_animation_frames(JSContext* ctx) {
    auto* raf_s = get_raf_state(ctx);
    if (!raf_s || raf_s->entries.empty()) return;

    // Snapshot pending entries — callbacks registered during this flush go to next frame.
    std::vector<RAFEntry> raf_snap = std::move(raf_s->entries);
    raf_s->entries.clear();

    // Compute DOMHighResTimeStamp (milliseconds since page load).
    auto raf_now = std::chrono::steady_clock::now();
    auto raf_elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        raf_now - page_start_time);
    double raf_ms = static_cast<double>(raf_elapsed.count()) / 1000.0;
    JSValue raf_ts = JS_NewFloat64(ctx, raf_ms);

    for (auto& raf_e : raf_snap) {
        if (!raf_e.cancelled && JS_IsFunction(ctx, raf_e.callback)) {
            JSValue raf_r = JS_Call(ctx, raf_e.callback, JS_UNDEFINED, 1, &raf_ts);
            if (JS_IsException(raf_r)) {
                JSValue exc = JS_GetException(ctx);
                JS_FreeValue(ctx, exc);
            }
            JS_FreeValue(ctx, raf_r);
        }
        JS_FreeValue(ctx, raf_e.callback);
    }

    JS_FreeValue(ctx, raf_ts);
}

void cleanup_animation_frames(JSContext* ctx) {
    auto* state = get_raf_state(ctx);
    if (!state) return;

    // Free any still-pending callback references to avoid GC leaks.
    for (auto& entry : state->entries) {
        JS_FreeValue(ctx, entry.callback);
    }
    state->entries.clear();

    // Remove the pointer stored on the global and delete the RAFState.
    delete state;
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "__raf_state_ptr", JS_NewInt64(ctx, 0));
    JS_FreeValue(ctx, global);
}

} // namespace clever::js
