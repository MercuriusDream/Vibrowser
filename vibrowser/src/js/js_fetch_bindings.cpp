#include <clever/js/js_fetch_bindings.h>
#include <clever/js/cors_policy.h>
#include <clever/js/js_engine.h>

extern "C" {
#include <quickjs.h>
}

#include <clever/net/http_client.h>
#include <clever/net/cookie_jar.h>
#include <clever/net/request.h>
#include <clever/net/response.h>
#include <clever/net/tls_socket.h>

#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <map>
#include <mutex>
#include <netdb.h>
#include <optional>
#include <poll.h>
#include <random>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace clever::js {

namespace {

// Forward declaration (defined later in this file)
static std::string url_decode_value(const std::string& s);

// =========================================================================
// XMLHttpRequest class ID
// =========================================================================

static JSClassID xhr_class_id = 0;

// =========================================================================
// Per-instance XHR state
// =========================================================================

struct XHRState {
    std::string method;
    std::string url;
    std::map<std::string, std::string> request_headers;
    int status = 0;
    std::string status_text;
    std::string response_text;
    std::map<std::string, std::string> response_headers;
    int ready_state = 0; // 0=UNSENT, 1=OPENED, 4=DONE

    // Extended properties
    std::string response_type; // "" (default, same as "text") or "json"
    int timeout = 0;           // timeout in milliseconds (0 = no timeout)
    bool with_credentials = false;

    // Event handlers are stored as JS properties on the object itself,
    // not in C++ state, to avoid GC reference counting issues.
};

// =========================================================================
// Helpers
// =========================================================================

static XHRState* get_xhr_state(JSValueConst this_val) {
    return static_cast<XHRState*>(JS_GetOpaque(this_val, xhr_class_id));
}

enum class FetchCredentialsMode {
    Omit,
    SameOrigin,
    Include,
};

static std::string current_document_origin(JSContext* ctx) {
    std::string origin;
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue location = JS_GetPropertyStr(ctx, global, "location");
    if (JS_IsObject(location)) {
        JSValue origin_val = JS_GetPropertyStr(ctx, location, "origin");
        if (JS_IsString(origin_val)) {
            const char* origin_cstr = JS_ToCString(ctx, origin_val);
            if (origin_cstr) {
                origin = origin_cstr;
                JS_FreeCString(ctx, origin_cstr);
            }
        }
        JS_FreeValue(ctx, origin_val);
    }
    JS_FreeValue(ctx, location);
    JS_FreeValue(ctx, global);
    return origin;
}

// =========================================================================
// XMLHttpRequest methods
// =========================================================================

// xhr.open(method, url)
static JSValue js_xhr_open(JSContext* ctx, JSValueConst this_val,
                            int argc, JSValueConst* argv) {
    if (argc < 2) {
        return JS_ThrowTypeError(ctx, "XMLHttpRequest.open requires 2 arguments");
    }

    auto* state = get_xhr_state(this_val);
    if (!state) return JS_EXCEPTION;

    const char* method = JS_ToCString(ctx, argv[0]);
    if (!method) return JS_EXCEPTION;

    const char* url = JS_ToCString(ctx, argv[1]);
    if (!url) {
        JS_FreeCString(ctx, method);
        return JS_EXCEPTION;
    }

    state->method = method;
    state->url = url;
    state->ready_state = 1; // OPENED
    state->request_headers.clear();
    state->status = 0;
    state->status_text.clear();
    state->response_text.clear();
    state->response_headers.clear();

    JS_FreeCString(ctx, method);
    JS_FreeCString(ctx, url);

    return JS_UNDEFINED;
}

// xhr.setRequestHeader(name, value)
static JSValue js_xhr_set_request_header(JSContext* ctx, JSValueConst this_val,
                                          int argc, JSValueConst* argv) {
    if (argc < 2) {
        return JS_ThrowTypeError(ctx, "setRequestHeader requires 2 arguments");
    }

    auto* state = get_xhr_state(this_val);
    if (!state) return JS_EXCEPTION;

    if (state->ready_state != 1) {
        return JS_ThrowTypeError(ctx, "setRequestHeader called before open");
    }

    const char* name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_EXCEPTION;

    const char* value = JS_ToCString(ctx, argv[1]);
    if (!value) {
        JS_FreeCString(ctx, name);
        return JS_EXCEPTION;
    }

    state->request_headers[name] = value;

    JS_FreeCString(ctx, name);
    JS_FreeCString(ctx, value);

    return JS_UNDEFINED;
}

// xhr.send(body?)
static JSValue js_xhr_send(JSContext* ctx, JSValueConst this_val,
                            int argc, JSValueConst* argv) {
    auto* state = get_xhr_state(this_val);
    if (!state) return JS_EXCEPTION;

    if (state->ready_state != 1) {
        return JS_ThrowTypeError(ctx, "send called before open");
    }

    // Build the request
    clever::net::Request req;
    req.url = state->url;
    req.method = clever::net::string_to_method(state->method);

    // Set request headers
    for (const auto& [name, value] : state->request_headers) {
        req.headers.set(name, value);
    }

    // Set body if provided
    if (argc >= 1 && JS_IsString(argv[0])) {
        const char* body_str = JS_ToCString(ctx, argv[0]);
        if (body_str) {
            req.body.assign(
                reinterpret_cast<const uint8_t*>(body_str),
                reinterpret_cast<const uint8_t*>(body_str) + std::strlen(body_str));
            JS_FreeCString(ctx, body_str);
        }
    }

    // Parse the URL into host/port/path components
    req.parse_url();

    const std::string document_origin = current_document_origin(ctx);
    const bool enforce_cors_request_policy =
        cors::has_enforceable_document_origin(document_origin) || document_origin == "null";
    const bool request_url_eligible = cors::is_cors_eligible_request_url(state->url);
    if (enforce_cors_request_policy && !request_url_eligible) {
        state->status = 0;
        state->status_text = "";
        state->response_text = "";
        state->response_headers.clear();
        state->ready_state = 4; // DONE
        return JS_UNDEFINED;
    }

    const bool cross_origin = cors::is_cross_origin(document_origin, state->url);
    const bool should_send_cookies =
        request_url_eligible && (!cross_origin || state->with_credentials);

    cors::normalize_outgoing_origin_header(req.headers, document_origin, state->url);

    // Attach cookies from shared cookie jar
    auto& jar = clever::net::CookieJar::shared();
    if (should_send_cookies) {
        std::string cookies = jar.get_cookie_header(req.host, req.path, req.use_tls);
        if (!cookies.empty() && !req.headers.has("cookie")) {
            req.headers.set("Cookie", cookies);
        }
    }

    // Perform the synchronous fetch
    clever::net::HttpClient client;
    client.set_timeout(std::chrono::seconds(30));
    auto resp = client.fetch(req);

    if (resp.has_value()) {
        const bool cors_allowed = cors::cors_allows_response(
            document_origin, state->url, resp->headers, state->with_credentials);

        if (!cors_allowed) {
            state->status = 0;
            state->status_text = "";
            state->response_text = "";
            state->response_headers.clear();
            state->ready_state = 4; // DONE
            return JS_UNDEFINED;
        }

        state->status = resp->status;
        state->status_text = resp->status_text;
        state->response_text = resp->body_as_string();

        // Store Set-Cookie from response
        auto set_cookie = resp->headers.get("set-cookie");
        if (should_send_cookies && set_cookie.has_value()) {
            jar.set_from_header(*set_cookie, req.host);
        }

        // Copy response headers
        state->response_headers.clear();
        for (const auto& [name, value] : resp->headers) {
            state->response_headers[name] = value;
        }
    } else {
        // Network error: status stays 0
        state->status = 0;
        state->status_text = "";
        state->response_text = "";
        state->response_headers.clear();
    }

    state->ready_state = 4; // DONE

    return JS_UNDEFINED;
}

// xhr.getResponseHeader(name)
static JSValue js_xhr_get_response_header(JSContext* ctx, JSValueConst this_val,
                                           int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_NULL;
    }

    auto* state = get_xhr_state(this_val);
    if (!state) return JS_NULL;

    const char* name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_NULL;

    // Case-insensitive lookup: convert to lowercase
    std::string lower_name = name;
    JS_FreeCString(ctx, name);
    for (auto& c : lower_name) {
        if (c >= 'A' && c <= 'Z') c += ('a' - 'A');
    }

    auto it = state->response_headers.find(lower_name);
    if (it != state->response_headers.end()) {
        return JS_NewString(ctx, it->second.c_str());
    }

    return JS_NULL;
}

// xhr.getAllResponseHeaders()
static JSValue js_xhr_get_all_response_headers(JSContext* ctx,
                                                JSValueConst this_val,
                                                int /*argc*/,
                                                JSValueConst* /*argv*/) {
    auto* state = get_xhr_state(this_val);
    if (!state) return JS_NewString(ctx, "");

    std::string result;
    for (const auto& [name, value] : state->response_headers) {
        result += name;
        result += ": ";
        result += value;
        result += "\r\n";
    }

    return JS_NewString(ctx, result.c_str());
}

// =========================================================================
// Property getters for readyState, status, statusText, responseText
// =========================================================================

static JSValue js_xhr_get_ready_state(JSContext* ctx, JSValueConst this_val) {
    auto* state = get_xhr_state(this_val);
    if (!state) return JS_NewInt32(ctx, 0);
    return JS_NewInt32(ctx, state->ready_state);
}

static JSValue js_xhr_get_status(JSContext* ctx, JSValueConst this_val) {
    auto* state = get_xhr_state(this_val);
    if (!state) return JS_NewInt32(ctx, 0);
    return JS_NewInt32(ctx, state->status);
}

static JSValue js_xhr_get_status_text(JSContext* ctx, JSValueConst this_val) {
    auto* state = get_xhr_state(this_val);
    if (!state) return JS_NewString(ctx, "");
    return JS_NewString(ctx, state->status_text.c_str());
}

static JSValue js_xhr_get_response_text(JSContext* ctx, JSValueConst this_val) {
    auto* state = get_xhr_state(this_val);
    if (!state) return JS_NewString(ctx, "");
    return JS_NewString(ctx, state->response_text.c_str());
}

// =========================================================================
// responseType getter/setter
// =========================================================================

static JSValue js_xhr_get_response_type(JSContext* ctx, JSValueConst this_val) {
    auto* state = get_xhr_state(this_val);
    if (!state) return JS_NewString(ctx, "");
    return JS_NewString(ctx, state->response_type.c_str());
}

static JSValue js_xhr_set_response_type(JSContext* ctx, JSValueConst this_val,
                                         JSValueConst val) {
    auto* state = get_xhr_state(this_val);
    if (!state) return JS_UNDEFINED;

    const char* str = JS_ToCString(ctx, val);
    if (str) {
        state->response_type = str;
        JS_FreeCString(ctx, str);
    }
    return JS_UNDEFINED;
}

// =========================================================================
// response getter — returns text or parsed JSON depending on responseType
// =========================================================================

static JSValue js_xhr_get_response(JSContext* ctx, JSValueConst this_val) {
    auto* state = get_xhr_state(this_val);
    if (!state) return JS_NULL;

    if (state->response_type.empty() || state->response_type == "text") {
        return JS_NewString(ctx, state->response_text.c_str());
    }
    if (state->response_type == "json") {
        if (state->response_text.empty()) return JS_NULL;
        return JS_ParseJSON(ctx, state->response_text.c_str(),
                            state->response_text.size(), "<xhr-json>");
    }
    // For other types (arraybuffer, blob, document), return null stub
    return JS_NULL;
}

// =========================================================================
// abort() method — resets readyState to 0, clears response data
// =========================================================================

static JSValue js_xhr_abort(JSContext* /*ctx*/, JSValueConst this_val,
                             int /*argc*/, JSValueConst* /*argv*/) {
    auto* state = get_xhr_state(this_val);
    if (!state) return JS_UNDEFINED;

    state->ready_state = 0; // UNSENT
    state->status = 0;
    state->status_text.clear();
    state->response_text.clear();
    state->response_headers.clear();

    return JS_UNDEFINED;
}

// =========================================================================
// timeout getter/setter
// =========================================================================

static JSValue js_xhr_get_timeout(JSContext* ctx, JSValueConst this_val) {
    auto* state = get_xhr_state(this_val);
    if (!state) return JS_NewInt32(ctx, 0);
    return JS_NewInt32(ctx, state->timeout);
}

static JSValue js_xhr_set_timeout(JSContext* ctx, JSValueConst this_val,
                                   JSValueConst val) {
    auto* state = get_xhr_state(this_val);
    if (!state) return JS_UNDEFINED;

    int32_t t = 0;
    JS_ToInt32(ctx, &t, val);
    state->timeout = t;
    return JS_UNDEFINED;
}

// =========================================================================
// withCredentials getter/setter
// =========================================================================

static JSValue js_xhr_get_with_credentials(JSContext* ctx, JSValueConst this_val) {
    auto* state = get_xhr_state(this_val);
    if (!state) return JS_FALSE;
    return JS_NewBool(ctx, state->with_credentials);
}

static JSValue js_xhr_set_with_credentials(JSContext* ctx, JSValueConst this_val,
                                            JSValueConst val) {
    auto* state = get_xhr_state(this_val);
    if (!state) return JS_UNDEFINED;

    state->with_credentials = JS_ToBool(ctx, val);
    return JS_UNDEFINED;
}

// =========================================================================
// Event handler getters/setters: onreadystatechange, onload, onerror
// Stored as JS properties on the object (avoids GC ref-count issues).
// =========================================================================

static JSValue js_xhr_get_onreadystatechange(JSContext* ctx, JSValueConst this_val) {
    return JS_GetPropertyStr(ctx, this_val, "_onreadystatechange");
}

static JSValue js_xhr_set_onreadystatechange(JSContext* ctx, JSValueConst this_val,
                                              JSValueConst val) {
    JS_SetPropertyStr(ctx, this_val, "_onreadystatechange", JS_DupValue(ctx, val));
    return JS_UNDEFINED;
}

static JSValue js_xhr_get_onload(JSContext* ctx, JSValueConst this_val) {
    return JS_GetPropertyStr(ctx, this_val, "_onload");
}

static JSValue js_xhr_set_onload(JSContext* ctx, JSValueConst this_val,
                                  JSValueConst val) {
    JS_SetPropertyStr(ctx, this_val, "_onload", JS_DupValue(ctx, val));
    return JS_UNDEFINED;
}

static JSValue js_xhr_get_onerror(JSContext* ctx, JSValueConst this_val) {
    return JS_GetPropertyStr(ctx, this_val, "_onerror");
}

static JSValue js_xhr_set_onerror(JSContext* ctx, JSValueConst this_val,
                                   JSValueConst val) {
    JS_SetPropertyStr(ctx, this_val, "_onerror", JS_DupValue(ctx, val));
    return JS_UNDEFINED;
}

// =========================================================================
// Class definition and finalizer
// =========================================================================

static void js_xhr_finalizer(JSRuntime* /*rt*/, JSValue val) {
    auto* state = static_cast<XHRState*>(JS_GetOpaque(val, xhr_class_id));
    delete state;
}

static JSClassDef xhr_class_def = {
    "XMLHttpRequest",
    js_xhr_finalizer,
    nullptr, nullptr, nullptr
};

// =========================================================================
// Constructor: new XMLHttpRequest()
// =========================================================================

static JSValue js_xhr_constructor(JSContext* ctx, JSValueConst new_target,
                                   int /*argc*/, JSValueConst* /*argv*/) {
    JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto)) return JS_EXCEPTION;

    JSValue obj = JS_NewObjectProtoClass(ctx, proto, xhr_class_id);
    JS_FreeValue(ctx, proto);
    if (JS_IsException(obj)) return JS_EXCEPTION;

    auto* state = new XHRState();
    JS_SetOpaque(obj, state);

    return obj;
}

// =========================================================================
// Property list for getters
// =========================================================================

static const JSCFunctionListEntry xhr_proto_funcs[] = {
    // Methods
    JS_CFUNC_DEF("open", 2, js_xhr_open),
    JS_CFUNC_DEF("setRequestHeader", 2, js_xhr_set_request_header),
    JS_CFUNC_DEF("send", 1, js_xhr_send),
    JS_CFUNC_DEF("getResponseHeader", 1, js_xhr_get_response_header),
    JS_CFUNC_DEF("getAllResponseHeaders", 0, js_xhr_get_all_response_headers),
    JS_CFUNC_DEF("abort", 0, js_xhr_abort),

    // Property getters (read-only)
    JS_CGETSET_DEF("readyState", js_xhr_get_ready_state, nullptr),
    JS_CGETSET_DEF("status", js_xhr_get_status, nullptr),
    JS_CGETSET_DEF("statusText", js_xhr_get_status_text, nullptr),
    JS_CGETSET_DEF("responseText", js_xhr_get_response_text, nullptr),
    JS_CGETSET_DEF("response", js_xhr_get_response, nullptr),

    // Property getters/setters
    JS_CGETSET_DEF("responseType", js_xhr_get_response_type, js_xhr_set_response_type),
    JS_CGETSET_DEF("timeout", js_xhr_get_timeout, js_xhr_set_timeout),
    JS_CGETSET_DEF("withCredentials", js_xhr_get_with_credentials, js_xhr_set_with_credentials),

    // Event handler properties (stored but not invoked)
    JS_CGETSET_DEF("onreadystatechange", js_xhr_get_onreadystatechange, js_xhr_set_onreadystatechange),
    JS_CGETSET_DEF("onload", js_xhr_get_onload, js_xhr_set_onload),
    JS_CGETSET_DEF("onerror", js_xhr_get_onerror, js_xhr_set_onerror),
};

// =========================================================================
// =========================================================================
//
//   FETCH API: Response class, Headers class, and global fetch()
//
// =========================================================================
// =========================================================================

// =========================================================================
// Headers class — simplified read-only headers
// =========================================================================

static JSClassID headers_class_id = 0;

struct HeadersState {
    std::map<std::string, std::string> headers;
};

static HeadersState* get_headers_state(JSContext* /*ctx*/, JSValueConst this_val) {
    return static_cast<HeadersState*>(JS_GetOpaque(this_val, headers_class_id));
}

static void js_headers_finalizer(JSRuntime* /*rt*/, JSValue val) {
    auto* state = static_cast<HeadersState*>(JS_GetOpaque(val, headers_class_id));
    delete state;
}

static JSClassDef headers_class_def = {
    "Headers",
    js_headers_finalizer,
    nullptr, nullptr, nullptr
};

// headers.get(name) -> string | null
static JSValue js_headers_get(JSContext* ctx, JSValueConst this_val,
                               int argc, JSValueConst* argv) {
    if (argc < 1) return JS_NULL;

    auto* state = get_headers_state(ctx, this_val);
    if (!state) return JS_NULL;

    const char* name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_NULL;

    // Normalize to lowercase
    std::string lower_name = name;
    JS_FreeCString(ctx, name);
    for (auto& c : lower_name) {
        if (c >= 'A' && c <= 'Z') c += ('a' - 'A');
    }

    auto it = state->headers.find(lower_name);
    if (it != state->headers.end()) {
        return JS_NewString(ctx, it->second.c_str());
    }
    return JS_NULL;
}

// headers.has(name) -> bool
static JSValue js_headers_has(JSContext* ctx, JSValueConst this_val,
                               int argc, JSValueConst* argv) {
    if (argc < 1) return JS_FALSE;

    auto* state = get_headers_state(ctx, this_val);
    if (!state) return JS_FALSE;

    const char* name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_FALSE;

    std::string lower_name = name;
    JS_FreeCString(ctx, name);
    for (auto& c : lower_name) {
        if (c >= 'A' && c <= 'Z') c += ('a' - 'A');
    }

    return state->headers.count(lower_name) > 0 ? JS_TRUE : JS_FALSE;
}

// headers.forEach(callback)
static JSValue js_headers_for_each(JSContext* ctx, JSValueConst this_val,
                                    int argc, JSValueConst* argv) {
    if (argc < 1 || !JS_IsFunction(ctx, argv[0])) {
        return JS_ThrowTypeError(ctx, "Headers.forEach requires a callback function");
    }

    auto* state = get_headers_state(ctx, this_val);
    if (!state) return JS_UNDEFINED;

    for (const auto& [key, value] : state->headers) {
        JSValue args[2];
        args[0] = JS_NewString(ctx, value.c_str());
        args[1] = JS_NewString(ctx, key.c_str());
        JSValue ret = JS_Call(ctx, argv[0], JS_UNDEFINED, 2, args);
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, args[0]);
        JS_FreeValue(ctx, args[1]);
    }

    return JS_UNDEFINED;
}

// headers.entries() -> array of [key, value] pairs
static JSValue js_headers_entries(JSContext* ctx, JSValueConst this_val,
                                   int /*argc*/, JSValueConst* /*argv*/) {
    auto* state = get_headers_state(ctx, this_val);
    if (!state) return JS_NewArray(ctx);

    JSValue arr = JS_NewArray(ctx);
    uint32_t idx = 0;
    for (const auto& [key, value] : state->headers) {
        JSValue pair = JS_NewArray(ctx);
        JS_SetPropertyUint32(ctx, pair, 0, JS_NewString(ctx, key.c_str()));
        JS_SetPropertyUint32(ctx, pair, 1, JS_NewString(ctx, value.c_str()));
        JS_SetPropertyUint32(ctx, arr, idx++, pair);
    }
    return arr;
}

// headers.keys() -> array of key strings
static JSValue js_headers_keys(JSContext* ctx, JSValueConst this_val,
                                int /*argc*/, JSValueConst* /*argv*/) {
    auto* state = get_headers_state(ctx, this_val);
    if (!state) return JS_NewArray(ctx);

    JSValue arr = JS_NewArray(ctx);
    uint32_t idx = 0;
    for (const auto& [key, value] : state->headers) {
        (void)value;
        JS_SetPropertyUint32(ctx, arr, idx++, JS_NewString(ctx, key.c_str()));
    }
    return arr;
}

// headers.values() -> array of value strings
static JSValue js_headers_values(JSContext* ctx, JSValueConst this_val,
                                  int /*argc*/, JSValueConst* /*argv*/) {
    auto* state = get_headers_state(ctx, this_val);
    if (!state) return JS_NewArray(ctx);

    JSValue arr = JS_NewArray(ctx);
    uint32_t idx = 0;
    for (const auto& [key, value] : state->headers) {
        (void)key;
        JS_SetPropertyUint32(ctx, arr, idx++, JS_NewString(ctx, value.c_str()));
    }
    return arr;
}

// headers.set(name, value) — set a header (overwrites existing)
static JSValue js_headers_set(JSContext* ctx, JSValueConst this_val,
                               int argc, JSValueConst* argv) {
    if (argc < 2) return JS_UNDEFINED;

    auto* state = get_headers_state(ctx, this_val);
    if (!state) return JS_UNDEFINED;

    const char* name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_UNDEFINED;
    const char* value = JS_ToCString(ctx, argv[1]);
    if (!value) {
        JS_FreeCString(ctx, name);
        return JS_UNDEFINED;
    }

    // Normalize to lowercase
    std::string lower_name = name;
    JS_FreeCString(ctx, name);
    for (auto& c : lower_name) {
        if (c >= 'A' && c <= 'Z') c += ('a' - 'A');
    }

    state->headers[lower_name] = value;
    JS_FreeCString(ctx, value);
    return JS_UNDEFINED;
}

// headers.append(name, value) — append (for now, same as set)
static JSValue js_headers_append(JSContext* ctx, JSValueConst this_val,
                                  int argc, JSValueConst* argv) {
    return js_headers_set(ctx, this_val, argc, argv);
}

// headers.delete(name) — remove a header
static JSValue js_headers_delete(JSContext* ctx, JSValueConst this_val,
                                  int argc, JSValueConst* argv) {
    if (argc < 1) return JS_UNDEFINED;

    auto* state = get_headers_state(ctx, this_val);
    if (!state) return JS_UNDEFINED;

    const char* name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_UNDEFINED;

    std::string lower_name = name;
    JS_FreeCString(ctx, name);
    for (auto& c : lower_name) {
        if (c >= 'A' && c <= 'Z') c += ('a' - 'A');
    }

    state->headers.erase(lower_name);
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry headers_proto_funcs[] = {
    JS_CFUNC_DEF("get", 1, js_headers_get),
    JS_CFUNC_DEF("has", 1, js_headers_has),
    JS_CFUNC_DEF("set", 2, js_headers_set),
    JS_CFUNC_DEF("append", 2, js_headers_append),
    JS_CFUNC_DEF("delete", 1, js_headers_delete),
    JS_CFUNC_DEF("forEach", 1, js_headers_for_each),
    JS_CFUNC_DEF("entries", 0, js_headers_entries),
    JS_CFUNC_DEF("keys", 0, js_headers_keys),
    JS_CFUNC_DEF("values", 0, js_headers_values),
};

// Helper: create a Headers JS object from a std::map
static JSValue create_headers_object(JSContext* ctx,
                                      const std::map<std::string, std::string>& hdrs) {
    JSValue obj = JS_NewObjectClass(ctx, static_cast<int>(headers_class_id));
    if (JS_IsException(obj)) return obj;

    auto* state = new HeadersState();
    state->headers = hdrs;
    JS_SetOpaque(obj, state);
    return obj;
}

// =========================================================================
// FormData class ID and state — declared early so Response.formData() can use them
// =========================================================================

static JSClassID formdata_class_id = 0;

struct FormDataState {
    std::vector<std::pair<std::string, std::string>> entries;
};

// =========================================================================
// Response class
// =========================================================================

static JSClassID response_class_id = 0;

struct ResponseState {
    int status = 0;
    std::string status_text;
    std::string body;
    std::map<std::string, std::string> headers;
    std::string url;
    bool ok = false; // status 200-299
};

static ResponseState* get_response_state(JSContext* /*ctx*/, JSValueConst this_val) {
    return static_cast<ResponseState*>(JS_GetOpaque(this_val, response_class_id));
}

static void js_response_finalizer(JSRuntime* /*rt*/, JSValue val) {
    auto* state = static_cast<ResponseState*>(JS_GetOpaque(val, response_class_id));
    delete state;
}

static JSClassDef response_class_def = {
    "Response",
    js_response_finalizer,
    nullptr, nullptr, nullptr
};

// ---- Response property getters ----

static JSValue js_response_get_ok(JSContext* ctx, JSValueConst this_val) {
    auto* state = get_response_state(ctx, this_val);
    if (!state) return JS_FALSE;
    return state->ok ? JS_TRUE : JS_FALSE;
}

static JSValue js_response_get_status(JSContext* ctx, JSValueConst this_val) {
    auto* state = get_response_state(ctx, this_val);
    if (!state) return JS_NewInt32(ctx, 0);
    return JS_NewInt32(ctx, state->status);
}

static JSValue js_response_get_status_text(JSContext* ctx, JSValueConst this_val) {
    auto* state = get_response_state(ctx, this_val);
    if (!state) return JS_NewString(ctx, "");
    return JS_NewString(ctx, state->status_text.c_str());
}

static JSValue js_response_get_url(JSContext* ctx, JSValueConst this_val) {
    auto* state = get_response_state(ctx, this_val);
    if (!state) return JS_NewString(ctx, "");
    return JS_NewString(ctx, state->url.c_str());
}

static JSValue js_response_get_type(JSContext* ctx, JSValueConst /*this_val*/) {
    return JS_NewString(ctx, "basic");
}

static JSValue js_response_get_body_used(JSContext* /*ctx*/, JSValueConst /*this_val*/) {
    // We allow repeated reads for simplicity
    return JS_FALSE;
}

static JSValue js_response_get_headers(JSContext* ctx, JSValueConst this_val) {
    auto* state = get_response_state(ctx, this_val);
    if (!state) return JS_NULL;
    return create_headers_object(ctx, state->headers);
}

// ---- Response methods ----

// response.text() -> Promise<string>
static JSValue js_response_text(JSContext* ctx, JSValueConst this_val,
                                 int /*argc*/, JSValueConst* /*argv*/) {
    auto* state = get_response_state(ctx, this_val);
    if (!state) return JS_ThrowTypeError(ctx, "Invalid Response object");

    JSValue resolving_funcs[2];
    JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
    if (JS_IsException(promise)) return promise;

    JSValue text = JS_NewString(ctx, state->body.c_str());
    JSValue ret = JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 1, &text);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, text);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);

    return promise;
}

// response.json() -> Promise<object>
static JSValue js_response_json(JSContext* ctx, JSValueConst this_val,
                                 int /*argc*/, JSValueConst* /*argv*/) {
    auto* state = get_response_state(ctx, this_val);
    if (!state) return JS_ThrowTypeError(ctx, "Invalid Response object");

    JSValue resolving_funcs[2];
    JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
    if (JS_IsException(promise)) return promise;

    // Parse the body as JSON
    JSValue parsed = JS_ParseJSON(ctx, state->body.c_str(), state->body.size(), "<json>");
    if (JS_IsException(parsed)) {
        // Reject the promise with the parse error
        JSValue err = JS_GetException(ctx);
        JSValue ret = JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &err);
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, err);
    } else {
        // Resolve the promise with the parsed object
        JSValue ret = JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 1, &parsed);
        JS_FreeValue(ctx, ret);
    }
    JS_FreeValue(ctx, parsed);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);

    return promise;
}

// response.clone() -> new Response with same data
static JSValue js_response_clone(JSContext* ctx, JSValueConst this_val,
                                  int /*argc*/, JSValueConst* /*argv*/) {
    auto* state = get_response_state(ctx, this_val);
    if (!state) return JS_ThrowTypeError(ctx, "Invalid Response object");

    JSValue obj = JS_NewObjectClass(ctx, static_cast<int>(response_class_id));
    if (JS_IsException(obj)) return obj;

    auto* new_state = new ResponseState();
    new_state->status = state->status;
    new_state->status_text = state->status_text;
    new_state->body = state->body;
    new_state->headers = state->headers;
    new_state->url = state->url;
    new_state->ok = state->ok;
    JS_SetOpaque(obj, new_state);

    return obj;
}

// response.arrayBuffer() -> Promise<ArrayBuffer> (stub: returns empty ArrayBuffer)
static JSValue js_response_array_buffer(JSContext* ctx, JSValueConst this_val,
                                         int /*argc*/, JSValueConst* /*argv*/) {
    auto* state = get_response_state(ctx, this_val);
    if (!state) return JS_ThrowTypeError(ctx, "Invalid Response object");

    JSValue resolving_funcs[2];
    JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
    if (JS_IsException(promise)) return promise;

    // Create an ArrayBuffer with the body data
    JSValue ab = JS_NewArrayBufferCopy(ctx,
        reinterpret_cast<const uint8_t*>(state->body.data()),
        state->body.size());
    JSValue ret = JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 1, &ab);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, ab);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);

    return promise;
}

// response.blob() -> Promise<Blob>
static JSValue js_response_blob(JSContext* ctx, JSValueConst this_val,
                                 int /*argc*/, JSValueConst* /*argv*/) {
    auto* state = get_response_state(ctx, this_val);
    if (!state) return JS_ThrowTypeError(ctx, "Invalid Response object");

    JSValue resolving_funcs[2];
    JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
    if (JS_IsException(promise)) return promise;

    // Get the Blob constructor from globalThis
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue blob_ctor = JS_GetPropertyStr(ctx, global, "Blob");

    if (JS_IsFunction(ctx, blob_ctor)) {
        // Create parts array: [bodyText]
        JSValue parts = JS_NewArray(ctx);
        JSValue body_str = JS_NewString(ctx, state->body.c_str());
        JS_SetPropertyUint32(ctx, parts, 0, body_str);

        // Create options object: {type: content-type}
        JSValue options = JS_NewObject(ctx);
        auto ct_it = state->headers.find("content-type");
        if (ct_it == state->headers.end()) ct_it = state->headers.find("Content-Type");
        std::string content_type = (ct_it != state->headers.end()) ? ct_it->second : "";
        JS_SetPropertyStr(ctx, options, "type", JS_NewString(ctx, content_type.c_str()));

        // Call new Blob(parts, options)
        JSValue args[2] = { parts, options };
        JSValue blob = JS_CallConstructor(ctx, blob_ctor, 2, args);

        if (JS_IsException(blob)) {
            JSValue err = JS_GetException(ctx);
            JSValue rr = JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &err);
            JS_FreeValue(ctx, rr);
            JS_FreeValue(ctx, err);
        } else {
            JSValue rr = JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 1, &blob);
            JS_FreeValue(ctx, rr);
        }
        JS_FreeValue(ctx, blob);
        JS_FreeValue(ctx, parts);
        JS_FreeValue(ctx, options);
    } else {
        // Blob not available — reject with error
        JSValue err = JS_NewString(ctx, "Blob constructor not available");
        JSValue rr = JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &err);
        JS_FreeValue(ctx, rr);
        JS_FreeValue(ctx, err);
    }

    JS_FreeValue(ctx, blob_ctor);
    JS_FreeValue(ctx, global);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);

    return promise;
}

// response.formData() -> Promise<FormData>
// Parses the response body according to the Content-Type header and returns a
// populated FormData object.  Supports application/x-www-form-urlencoded and
// multipart/form-data.
static JSValue js_response_form_data(JSContext* ctx, JSValueConst this_val,
                                      int /*argc*/, JSValueConst* /*argv*/) {
    auto* state = get_response_state(ctx, this_val);
    if (!state) return JS_ThrowTypeError(ctx, "Invalid Response object");

    JSValue resolving_funcs[2];
    JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
    if (JS_IsException(promise)) return promise;

    // Determine Content-Type (case-insensitive lookup)
    std::string content_type;
    for (const auto& [k, v] : state->headers) {
        std::string lower_key = k;
        for (char& ch : lower_key) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (lower_key == "content-type") {
            content_type = v;
            break;
        }
    }

    // Lower-case the content-type for comparison
    std::string ct_lower = content_type;
    for (char& ch : ct_lower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

    // Build the FormData entries
    std::vector<std::pair<std::string, std::string>> entries;

    if (ct_lower.find("application/x-www-form-urlencoded") != std::string::npos) {
        // Split on '&', then each token on '='
        const std::string& body = state->body;
        size_t start = 0;
        while (start <= body.size()) {
            size_t amp = body.find('&', start);
            if (amp == std::string::npos) amp = body.size();
            std::string token = body.substr(start, amp - start);
            if (!token.empty()) {
                size_t eq = token.find('=');
                std::string key, val;
                if (eq != std::string::npos) {
                    key = url_decode_value(token.substr(0, eq));
                    val = url_decode_value(token.substr(eq + 1));
                } else {
                    key = url_decode_value(token);
                }
                entries.emplace_back(std::move(key), std::move(val));
            }
            start = amp + 1;
        }
    } else if (ct_lower.find("multipart/form-data") != std::string::npos) {
        // Extract boundary from Content-Type header value
        // e.g. "multipart/form-data; boundary=----FormBoundary123"
        std::string boundary;
        const std::string bnd_token = "boundary=";
        size_t bpos = ct_lower.find(bnd_token);
        if (bpos != std::string::npos) {
            size_t bstart = bpos + bnd_token.size();
            // boundary value may be quoted
            if (bstart < content_type.size() && content_type[bstart] == '"') {
                ++bstart;
                size_t bend = content_type.find('"', bstart);
                if (bend == std::string::npos) bend = content_type.size();
                boundary = content_type.substr(bstart, bend - bstart);
            } else {
                size_t bend = content_type.find(';', bstart);
                if (bend == std::string::npos) bend = content_type.size();
                // trim trailing whitespace
                while (bend > bstart && std::isspace(static_cast<unsigned char>(content_type[bend - 1])))
                    --bend;
                boundary = content_type.substr(bstart, bend - bstart);
            }
        }

        if (!boundary.empty()) {
            const std::string& body = state->body;
            const std::string delimiter = "--" + boundary;
            const std::string close_delimiter = "--" + boundary + "--";

            size_t pos = 0;
            while (pos < body.size()) {
                // Find the next delimiter
                size_t delim_pos = body.find(delimiter, pos);
                if (delim_pos == std::string::npos) break;

                // Skip past delimiter
                size_t after_delim = delim_pos + delimiter.size();

                // Check for closing delimiter
                if (after_delim + 2 <= body.size() &&
                    body[after_delim] == '-' && body[after_delim + 1] == '-') {
                    break; // end of multipart
                }

                // Skip CRLF after delimiter
                if (after_delim < body.size() && body[after_delim] == '\r') ++after_delim;
                if (after_delim < body.size() && body[after_delim] == '\n') ++after_delim;

                // Find blank line separating headers from body
                size_t header_end = body.find("\r\n\r\n", after_delim);
                if (header_end == std::string::npos) {
                    header_end = body.find("\n\n", after_delim);
                    if (header_end == std::string::npos) break;
                    header_end += 2;
                } else {
                    header_end += 4;
                }

                // Parse part headers to get Content-Disposition name
                std::string part_headers = body.substr(after_delim, header_end - after_delim);
                std::string part_name;
                {
                    // Find Content-Disposition line
                    std::string ph_lower = part_headers;
                    for (char& ch : ph_lower)
                        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                    size_t cd_pos = ph_lower.find("content-disposition:");
                    if (cd_pos != std::string::npos) {
                        size_t cd_end = part_headers.find('\n', cd_pos);
                        if (cd_end == std::string::npos) cd_end = part_headers.size();
                        std::string cd_line = part_headers.substr(cd_pos, cd_end - cd_pos);
                        // Extract name="..."
                        std::string cd_lower = cd_line;
                        for (char& ch : cd_lower)
                            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                        const std::string name_token = "name=\"";
                        size_t npos = cd_lower.find(name_token);
                        if (npos != std::string::npos) {
                            size_t nstart = npos + name_token.size();
                            size_t nend = cd_line.find('"', nstart);
                            if (nend == std::string::npos) nend = cd_line.size();
                            part_name = cd_line.substr(nstart, nend - nstart);
                        }
                    }
                }

                // Find the part body (up to the next delimiter)
                size_t next_delim = body.find(delimiter, header_end);
                if (next_delim == std::string::npos) next_delim = body.size();

                // Strip trailing CRLF before delimiter
                size_t part_body_end = next_delim;
                if (part_body_end >= 2 &&
                    body[part_body_end - 2] == '\r' && body[part_body_end - 1] == '\n') {
                    part_body_end -= 2;
                } else if (part_body_end >= 1 && body[part_body_end - 1] == '\n') {
                    --part_body_end;
                }

                std::string part_body = body.substr(header_end, part_body_end - header_end);

                if (!part_name.empty()) {
                    entries.emplace_back(part_name, part_body);
                }

                pos = next_delim;
            }
        }
    }
    // (Any other / missing Content-Type yields an empty FormData.)

    // Create a FormData JS object and populate it with entries
    JSValue formdata_proto = JS_GetClassProto(ctx, formdata_class_id);
    JSValue fd_obj = JS_NewObjectProtoClass(ctx, formdata_proto, formdata_class_id);
    JS_FreeValue(ctx, formdata_proto);

    if (JS_IsException(fd_obj)) {
        // Reject the promise
        JSValue err = JS_GetException(ctx);
        JSValue ret = JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &err);
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, err);
        JS_FreeValue(ctx, resolving_funcs[0]);
        JS_FreeValue(ctx, resolving_funcs[1]);
        return promise;
    }

    auto* fd_state = new FormDataState();
    fd_state->entries = std::move(entries);
    JS_SetOpaque(fd_obj, fd_state);

    // Resolve the promise with the FormData object
    JSValue ret = JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 1, &fd_obj);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, fd_obj);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);

    return promise;
}

static const JSCFunctionListEntry response_proto_funcs[] = {
    // Property getters
    JS_CGETSET_DEF("ok", js_response_get_ok, nullptr),
    JS_CGETSET_DEF("status", js_response_get_status, nullptr),
    JS_CGETSET_DEF("statusText", js_response_get_status_text, nullptr),
    JS_CGETSET_DEF("url", js_response_get_url, nullptr),
    JS_CGETSET_DEF("type", js_response_get_type, nullptr),
    JS_CGETSET_DEF("bodyUsed", js_response_get_body_used, nullptr),
    JS_CGETSET_DEF("headers", js_response_get_headers, nullptr),

    // Methods
    JS_CFUNC_DEF("text", 0, js_response_text),
    JS_CFUNC_DEF("json", 0, js_response_json),
    JS_CFUNC_DEF("clone", 0, js_response_clone),
    JS_CFUNC_DEF("arrayBuffer", 0, js_response_array_buffer),
    JS_CFUNC_DEF("blob", 0, js_response_blob),
    JS_CFUNC_DEF("formData", 0, js_response_form_data),
};

// =========================================================================
// Response JS constructor: new Response(body?, init?)
// =========================================================================

static JSValue js_response_constructor(JSContext* ctx, JSValueConst /*new_target*/,
                                        int argc, JSValueConst* argv) {
    JSValue obj = JS_NewObjectClass(ctx, static_cast<int>(response_class_id));
    if (JS_IsException(obj)) return obj;

    auto* state = new ResponseState();

    // body (optional string)
    if (argc >= 1 && JS_IsString(argv[0])) {
        const char* body_cstr = JS_ToCString(ctx, argv[0]);
        if (body_cstr) {
            state->body = body_cstr;
            JS_FreeCString(ctx, body_cstr);
        }
    }

    // init (optional object with status, statusText, headers)
    state->status = 200;
    state->status_text = "OK";
    state->ok = true;

    if (argc >= 2 && JS_IsObject(argv[1])) {
        JSValue status_val = JS_GetPropertyStr(ctx, argv[1], "status");
        if (JS_IsNumber(status_val)) {
            int32_t s = 0;
            JS_ToInt32(ctx, &s, status_val);
            state->status = s;
            state->ok = (s >= 200 && s <= 299);
        }
        JS_FreeValue(ctx, status_val);

        JSValue st_val = JS_GetPropertyStr(ctx, argv[1], "statusText");
        if (JS_IsString(st_val)) {
            const char* st = JS_ToCString(ctx, st_val);
            if (st) { state->status_text = st; JS_FreeCString(ctx, st); }
        }
        JS_FreeValue(ctx, st_val);

        JSValue headers_val = JS_GetPropertyStr(ctx, argv[1], "headers");
        if (JS_IsObject(headers_val)) {
            JSPropertyEnum* tab = nullptr;
            uint32_t len = 0;
            if (JS_GetOwnPropertyNames(ctx, &tab, &len, headers_val,
                                         JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) == 0) {
                for (uint32_t i = 0; i < len; i++) {
                    const char* key = JS_AtomToCString(ctx, tab[i].atom);
                    JSValue val = JS_GetProperty(ctx, headers_val, tab[i].atom);
                    const char* val_str = JS_ToCString(ctx, val);
                    if (key && val_str) state->headers[key] = val_str;
                    if (val_str) JS_FreeCString(ctx, val_str);
                    if (key) JS_FreeCString(ctx, key);
                    JS_FreeValue(ctx, val);
                    JS_FreeAtom(ctx, tab[i].atom);
                }
                js_free(ctx, tab);
            }
        }
        JS_FreeValue(ctx, headers_val);
    }

    JS_SetOpaque(obj, state);
    return obj;
}

// =========================================================================
// Helper: create a Response JS object from an HTTP response
// =========================================================================

static JSValue create_response_object(JSContext* ctx,
                                       const clever::net::Response& resp,
                                       const std::string& request_url) {
    JSValue obj = JS_NewObjectClass(ctx, static_cast<int>(response_class_id));
    if (JS_IsException(obj)) return obj;

    auto* state = new ResponseState();
    state->status = resp.status;
    state->status_text = resp.status_text;
    state->body = resp.body_as_string();
    state->url = resp.url.empty() ? request_url : resp.url;
    state->ok = (resp.status >= 200 && resp.status <= 299);

    // Copy headers
    for (const auto& [name, value] : resp.headers) {
        state->headers[name] = value;
    }

    JS_SetOpaque(obj, state);
    return obj;
}

// Helper: URL-encode a string (application/x-www-form-urlencoded style)
static std::string url_encode_value(const std::string& s, bool encode_space_as_plus = true) {
    std::string out;
    out.reserve(s.size() * 3);
    for (unsigned char c : s) {
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') || c == '-' || c == '_' || c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else if (c == ' ' && encode_space_as_plus) {
            out += '+';
        } else {
            char buf[4];
            std::snprintf(buf, sizeof(buf), "%%%02X", c);
            out += buf;
        }
    }
    return out;
}

// Helper: URL-decode a string (application/x-www-form-urlencoded style)
// Handles + as space and %XX hex sequences.
static std::string url_decode_value(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '+') {
            out += ' ';
        } else if (c == '%' && i + 2 < s.size()) {
            char h1 = s[i + 1];
            char h2 = s[i + 2];
            auto hex_digit = [](char ch) -> int {
                if (ch >= '0' && ch <= '9') return ch - '0';
                if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
                if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
                return -1;
            };
            int hi = hex_digit(h1);
            int lo = hex_digit(h2);
            if (hi >= 0 && lo >= 0) {
                out += static_cast<char>((hi << 4) | lo);
                i += 2;
            } else {
                out += c;
            }
        } else {
            out += c;
        }
    }
    return out;
}

// Serialize FormData entries as multipart/form-data body
static std::string formdata_to_multipart(const std::vector<std::pair<std::string, std::string>>& entries,
                                          std::string& boundary_out) {
    static std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<uint64_t> dist;
    std::string boundary = "----FormBoundary";
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx",
                  static_cast<unsigned long long>(dist(rng)));
    boundary += buf;
    boundary_out = boundary;

    std::string body;
    for (const auto& [name, value] : entries) {
        body += "--" + boundary + "\r\n";
        body += "Content-Disposition: form-data; name=\"" + name + "\"\r\n";
        body += "\r\n";
        body += value;
        body += "\r\n";
    }
    body += "--" + boundary + "--\r\n";
    return body;
}

// Serialize FormData entries as application/x-www-form-urlencoded body
static std::string formdata_to_urlencoded(const std::vector<std::pair<std::string, std::string>>& entries) {
    std::string out;
    for (size_t i = 0; i < entries.size(); i++) {
        if (i > 0) out += '&';
        out += url_encode_value(entries[i].first, true);
        out += '=';
        out += url_encode_value(entries[i].second, true);
    }
    return out;
}

// =========================================================================
// Global fetch(url, options?) -> Promise<Response>
// =========================================================================

static JSValue js_global_fetch(JSContext* ctx, JSValueConst /*this_val*/,
                                int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "fetch requires a URL argument");
    }

    // Accept string or URL object (via .href property or toString)
    const char* url_cstr = JS_ToCString(ctx, argv[0]);
    if (!url_cstr) return JS_EXCEPTION;
    std::string url_str(url_cstr);
    JS_FreeCString(ctx, url_cstr);

    // Parse options if provided
    std::string method = "GET";
    std::map<std::string, std::string> req_headers;
    std::string body;
    FetchCredentialsMode credentials_mode = FetchCredentialsMode::SameOrigin;

    if (argc >= 2 && JS_IsObject(argv[1])) {
        // method
        JSValue method_val = JS_GetPropertyStr(ctx, argv[1], "method");
        if (JS_IsString(method_val)) {
            const char* m = JS_ToCString(ctx, method_val);
            if (m) {
                method = m;
                JS_FreeCString(ctx, m);
            }
        }
        JS_FreeValue(ctx, method_val);

        // headers - support object with string values
        JSValue headers_val = JS_GetPropertyStr(ctx, argv[1], "headers");
        if (JS_IsObject(headers_val)) {
            JSPropertyEnum* tab = nullptr;
            uint32_t len = 0;
            if (JS_GetOwnPropertyNames(ctx, &tab, &len, headers_val,
                                         JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) == 0) {
                for (uint32_t i = 0; i < len; i++) {
                    JSValue key = JS_AtomToString(ctx, tab[i].atom);
                    JSValue val = JS_GetProperty(ctx, headers_val, tab[i].atom);

                    const char* key_str = JS_ToCString(ctx, key);
                    const char* val_str = JS_ToCString(ctx, val);

                    if (key_str && val_str) {
                        req_headers[key_str] = val_str;
                    }

                    if (key_str) JS_FreeCString(ctx, key_str);
                    if (val_str) JS_FreeCString(ctx, val_str);
                    JS_FreeValue(ctx, key);
                    JS_FreeValue(ctx, val);
                }
                JS_FreePropertyEnum(ctx, tab, len);
            }
        }
        JS_FreeValue(ctx, headers_val);

        // body — support string, FormData, URLSearchParams
        JSValue body_val = JS_GetPropertyStr(ctx, argv[1], "body");
        if (JS_IsString(body_val)) {
            const char* b = JS_ToCString(ctx, body_val);
            if (b) {
                body = b;
                JS_FreeCString(ctx, b);
            }
        } else if (JS_IsObject(body_val)) {
            // Check if it's a FormData instance (has opaque data with formdata_class_id)
            auto* fd_state = static_cast<FormDataState*>(
                JS_GetOpaque(body_val, formdata_class_id));
            if (fd_state) {
                // Serialize as multipart/form-data
                std::string boundary;
                body = formdata_to_multipart(fd_state->entries, boundary);
                // Set Content-Type header if not already set (overwrite)
                if (req_headers.find("content-type") == req_headers.end() &&
                    req_headers.find("Content-Type") == req_headers.end()) {
                    req_headers["Content-Type"] =
                        "multipart/form-data; boundary=" + boundary;
                }
            } else {
                // Check if it's a URLSearchParams-like object (has _params array and toString)
                JSValue to_str_fn = JS_GetPropertyStr(ctx, body_val, "toString");
                JSValue params_val = JS_GetPropertyStr(ctx, body_val, "_params");
                bool is_usp = JS_IsArray(ctx, params_val);
                JS_FreeValue(ctx, params_val);

                if (is_usp && JS_IsFunction(ctx, to_str_fn)) {
                    // Serialize URLSearchParams via toString()
                    JSValue serialized = JS_Call(ctx, to_str_fn, body_val, 0, nullptr);
                    if (JS_IsString(serialized)) {
                        const char* s = JS_ToCString(ctx, serialized);
                        if (s) {
                            body = s;
                            JS_FreeCString(ctx, s);
                        }
                    }
                    JS_FreeValue(ctx, serialized);
                    // Set Content-Type header if not already set
                    if (req_headers.find("content-type") == req_headers.end() &&
                        req_headers.find("Content-Type") == req_headers.end()) {
                        req_headers["Content-Type"] =
                            "application/x-www-form-urlencoded";
                    }
                } else {
                    // Fallback: try JSON.stringify or toString
                    if (JS_IsFunction(ctx, to_str_fn)) {
                        JSValue str_val = JS_Call(ctx, to_str_fn, body_val, 0, nullptr);
                        if (JS_IsString(str_val)) {
                            const char* s = JS_ToCString(ctx, str_val);
                            if (s) {
                                body = s;
                                JS_FreeCString(ctx, s);
                            }
                        }
                        JS_FreeValue(ctx, str_val);
                    }
                }
                JS_FreeValue(ctx, to_str_fn);
            }
        }
        JS_FreeValue(ctx, body_val);

        // credentials
        JSValue credentials_val = JS_GetPropertyStr(ctx, argv[1], "credentials");
        if (JS_IsString(credentials_val)) {
            const char* credentials_cstr = JS_ToCString(ctx, credentials_val);
            if (credentials_cstr) {
                std::string credentials = credentials_cstr;
                if (credentials == "omit") {
                    credentials_mode = FetchCredentialsMode::Omit;
                } else if (credentials == "include") {
                    credentials_mode = FetchCredentialsMode::Include;
                } else {
                    credentials_mode = FetchCredentialsMode::SameOrigin;
                }
                JS_FreeCString(ctx, credentials_cstr);
            }
        }
        JS_FreeValue(ctx, credentials_val);

        // signal (AbortSignal) — check if already aborted before making request
        JSValue signal_val = JS_GetPropertyStr(ctx, argv[1], "signal");
        if (JS_IsObject(signal_val)) {
            JSValue aborted_val = JS_GetPropertyStr(ctx, signal_val, "aborted");
            bool already_aborted = JS_ToBool(ctx, aborted_val) > 0;
            JS_FreeValue(ctx, aborted_val);
            if (already_aborted) {
                // Fetch aborted before it started — reject with AbortError
                JSValue resolving_funcs[2];
                JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
                if (!JS_IsException(promise)) {
                    // Build a DOMException-like AbortError from the signal.reason
                    JSValue reason_val = JS_GetPropertyStr(ctx, signal_val, "reason");
                    JSValue err_to_reject = JS_IsUndefined(reason_val)
                        ? JS_NewError(ctx)
                        : JS_DupValue(ctx, reason_val);
                    JS_FreeValue(ctx, reason_val);
                    if (JS_IsUndefined(err_to_reject) || !JS_IsObject(err_to_reject)) {
                        JS_FreeValue(ctx, err_to_reject);
                        err_to_reject = JS_NewError(ctx);
                        JS_SetPropertyStr(ctx, err_to_reject, "message",
                            JS_NewString(ctx, "The operation was aborted."));
                        JS_SetPropertyStr(ctx, err_to_reject, "name",
                            JS_NewString(ctx, "AbortError"));
                    }
                    JSValue ret = JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &err_to_reject);
                    JS_FreeValue(ctx, ret);
                    JS_FreeValue(ctx, err_to_reject);
                    JS_FreeValue(ctx, resolving_funcs[0]);
                    JS_FreeValue(ctx, resolving_funcs[1]);
                }
                JS_FreeValue(ctx, signal_val);
                return promise;
            }
        }
        JS_FreeValue(ctx, signal_val);
    }

    // Build the Request
    clever::net::Request req;
    req.url = url_str;
    req.method = clever::net::string_to_method(method);

    for (const auto& [name, value] : req_headers) {
        req.headers.set(name, value);
    }

    if (!body.empty()) {
        req.body.assign(
            reinterpret_cast<const uint8_t*>(body.data()),
            reinterpret_cast<const uint8_t*>(body.data()) + body.size());
    }

    req.parse_url();
    const std::string document_origin = current_document_origin(ctx);
    const bool enforce_cors_request_policy =
        cors::has_enforceable_document_origin(document_origin) || document_origin == "null";
    const bool request_url_eligible = cors::is_cors_eligible_request_url(url_str);
    const bool cross_origin = cors::is_cross_origin(document_origin, url_str);
    const bool credentials_requested = (credentials_mode == FetchCredentialsMode::Include);
    const bool should_send_cookies =
        request_url_eligible &&
        (credentials_mode == FetchCredentialsMode::Include ||
         (credentials_mode == FetchCredentialsMode::SameOrigin && !cross_origin));

    cors::normalize_outgoing_origin_header(req.headers, document_origin, url_str);

    // Attach cookies from shared cookie jar
    if (should_send_cookies) {
        auto& jar = clever::net::CookieJar::shared();
        std::string cookies = jar.get_cookie_header(req.host, req.path, req.use_tls);
        if (!cookies.empty() && !req.headers.has("cookie")) {
            req.headers.set("Cookie", cookies);
        }
    }

    bool cors_blocked = false;
    std::optional<clever::net::Response> resp;

    if (enforce_cors_request_policy && !request_url_eligible) {
        cors_blocked = true;
    } else {
        // Perform synchronous HTTP request
        clever::net::HttpClient client;
        client.set_timeout(std::chrono::seconds(30));
        resp = client.fetch(req);
    }

    if (resp.has_value()) {
        const bool cors_allowed =
            cors::cors_allows_response(document_origin, url_str, resp->headers, credentials_requested);
        if (!cors_allowed) {
            cors_blocked = true;
            resp.reset();
        }
    }

    // Store Set-Cookie from response
    if (resp.has_value() && should_send_cookies) {
        auto set_cookie = resp->headers.get("set-cookie");
        if (set_cookie.has_value()) {
            auto& jar = clever::net::CookieJar::shared();
            jar.set_from_header(*set_cookie, req.host);
        }
    }

    // Create the Promise
    JSValue resolving_funcs[2];
    JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
    if (JS_IsException(promise)) return promise;

    if (resp.has_value()) {
        // Create Response object and resolve
        JSValue response_obj = create_response_object(ctx, *resp, url_str);
        if (JS_IsException(response_obj)) {
            // Reject with the exception
            JSValue err = JS_GetException(ctx);
            JSValue ret = JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &err);
            JS_FreeValue(ctx, ret);
            JS_FreeValue(ctx, err);
        } else {
            JSValue ret = JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 1, &response_obj);
            JS_FreeValue(ctx, ret);
        }
        JS_FreeValue(ctx, response_obj);
    } else {
        // Network error: reject the promise with a TypeError
        JSValue err = JS_NewError(ctx);
        if (cors_blocked) {
            JS_SetPropertyStr(ctx, err, "message",
                              JS_NewString(ctx, "TypeError: Failed to fetch (CORS blocked)"));
        } else {
            JS_SetPropertyStr(ctx, err, "message",
                              JS_NewString(ctx, "NetworkError: fetch failed"));
        }
        JSValue ret = JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &err);
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, err);
    }

    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);

    return promise;
}

// =========================================================================
// Helper: Execute all pending Promise microtasks
// =========================================================================

static void flush_promise_jobs(JSContext* ctx) {
    JSContext* ctx1 = nullptr;
    while (JS_ExecutePendingJob(JS_GetRuntime(ctx), &ctx1) > 0) {
        // Keep executing until no more pending jobs
    }
}

// =========================================================================
// =========================================================================
//
//   WEBSOCKET API
//
// =========================================================================
// =========================================================================

static JSClassID ws_class_id = 0;

struct WebSocketState {
    std::string url;
    std::atomic<int> readyState {0}; // 0=CONNECTING, 1=OPEN, 2=CLOSING, 3=CLOSED
    std::string protocol;
    std::atomic<int> socket_fd {-1};
    bool is_tls = false;
    JSRuntime* runtime = nullptr;
    // Event handlers (stored as JSValue, must be freed)
    JSValue onopen = JS_UNDEFINED;
    JSValue onmessage = JS_UNDEFINED;
    JSValue onclose = JS_UNDEFINED;
    JSValue onerror = JS_UNDEFINED;
    // Buffered messages
    std::vector<std::string> message_queue;
    std::mutex message_queue_mutex_;
    std::thread receive_thread_;
    std::atomic<bool> should_close_thread_ {false};
    std::atomic<bool> receive_thread_running_ {false};
    // TLS socket (heap-allocated so we can null-check)
    clever::net::TlsSocket* tls = nullptr;
};

constexpr int k_ws_recv_timeout_ms = 1000;
constexpr int k_ws_thread_join_timeout_ms = 2000;
constexpr int k_ws_thread_join_poll_interval_ms = 5;

// ---- Helpers ----

static WebSocketState* get_ws_state(JSValueConst this_val) {
    return static_cast<WebSocketState*>(JS_GetOpaque(this_val, ws_class_id));
}

static JSContext* ws_get_callback_context(WebSocketState* state) {
    if (!state || !state->runtime) return nullptr;
    auto* engine = static_cast<JSEngine*>(JS_GetRuntimeOpaque(state->runtime));
    if (!engine) return nullptr;
    return engine->context();
}

static void ws_fire_error_event(WebSocketState* state, const std::string& message) {
    JSContext* ctx = ws_get_callback_context(state);
    if (!ctx) return;

    JSValue handler = JS_UNDEFINED;
    {
        std::lock_guard<std::mutex> lock(state->message_queue_mutex_);
        handler = JS_DupValue(ctx, state->onerror);
    }

    if (JS_IsFunction(ctx, handler)) {
        JSValue event = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, event, "type", JS_NewString(ctx, "error"));
        JS_SetPropertyStr(ctx, event, "message", JS_NewString(ctx, message.c_str()));
        JSValue ret = JS_Call(ctx, handler, JS_UNDEFINED, 1, &event);
        if (JS_IsException(ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, event);
    }

    JS_FreeValue(ctx, handler);
}

static void ws_fire_message_event(WebSocketState* state, const std::string& data) {
    JSContext* ctx = ws_get_callback_context(state);
    if (!ctx) return;

    JSValue handler = JS_UNDEFINED;
    {
        std::lock_guard<std::mutex> lock(state->message_queue_mutex_);
        handler = JS_DupValue(ctx, state->onmessage);
    }

    if (JS_IsFunction(ctx, handler)) {
        JSValue event = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, event, "type", JS_NewString(ctx, "message"));
        JS_SetPropertyStr(ctx, event, "data", JS_NewString(ctx, data.c_str()));
        JSValue ret = JS_Call(ctx, handler, JS_UNDEFINED, 1, &event);
        if (JS_IsException(ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, event);
    }

    JS_FreeValue(ctx, handler);
}

static void ws_fire_close_event(WebSocketState* state, uint16_t code,
                                 const std::string& reason, bool was_clean) {
    JSContext* ctx = ws_get_callback_context(state);
    if (!ctx) return;

    JSValue handler = JS_UNDEFINED;
    {
        std::lock_guard<std::mutex> lock(state->message_queue_mutex_);
        handler = JS_DupValue(ctx, state->onclose);
    }

    if (JS_IsFunction(ctx, handler)) {
        JSValue event = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, event, "type", JS_NewString(ctx, "close"));
        JS_SetPropertyStr(ctx, event, "code", JS_NewInt32(ctx, code));
        JS_SetPropertyStr(ctx, event, "reason", JS_NewString(ctx, reason.c_str()));
        JS_SetPropertyStr(ctx, event, "wasClean", was_clean ? JS_TRUE : JS_FALSE);
        JSValue ret = JS_Call(ctx, handler, JS_UNDEFINED, 1, &event);
        if (JS_IsException(ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, event);
    }

    JS_FreeValue(ctx, handler);
}

static void ws_close_tls(WebSocketState* state) {
    clever::net::TlsSocket* tls = nullptr;
    {
        std::lock_guard<std::mutex> lock(state->message_queue_mutex_);
        tls = state->tls;
        state->tls = nullptr;
    }
    if (tls) {
        tls->close();
        delete tls;
    }
}

static void ws_shutdown_socket_fd(WebSocketState* state) {
    int fd = state->socket_fd.load();
    if (fd >= 0) {
        ::shutdown(fd, SHUT_RDWR);
    }
}

static void ws_close_socket_fd(WebSocketState* state) {
    int fd = state->socket_fd.exchange(-1);
    if (fd >= 0) {
        ::shutdown(fd, SHUT_RDWR);
        ::close(fd);
    }
}

static void ws_stop_receive_thread(WebSocketState* state, int timeout_ms) {
    if (!state) return;
    state->should_close_thread_.store(true);
    ws_shutdown_socket_fd(state);

    if (!state->receive_thread_.joinable()) return;
    if (state->receive_thread_.get_id() == std::this_thread::get_id()) return;

    auto deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(timeout_ms);
    while (state->receive_thread_running_.load() &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(k_ws_thread_join_poll_interval_ms));
    }
    state->receive_thread_.join();
}

// Generate a random 16-byte base64-encoded WebSocket key
static std::string generate_ws_key() {
    static std::mt19937 rng(std::random_device{}());
    uint8_t bytes[16];
    for (auto& b : bytes) {
        b = static_cast<uint8_t>(rng() & 0xFF);
    }
    // Base64 encode 16 bytes -> 24 chars
    static const char b64[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve(24);
    for (int i = 0; i < 16; i += 3) {
        uint32_t triple = (static_cast<uint32_t>(bytes[i]) << 16);
        if (i + 1 < 16) triple |= (static_cast<uint32_t>(bytes[i + 1]) << 8);
        if (i + 2 < 16) triple |= static_cast<uint32_t>(bytes[i + 2]);
        result += b64[(triple >> 18) & 0x3F];
        result += b64[(triple >> 12) & 0x3F];
        if (i + 1 < 16) result += b64[(triple >> 6) & 0x3F];
        else result += '=';
        if (i + 2 < 16) result += b64[triple & 0x3F];
        else result += '=';
    }
    return result;
}

// Parse ws:// or wss:// URL into host, port, path
static bool parse_ws_url(const std::string& url,
                          bool& use_tls,
                          std::string& host,
                          uint16_t& port,
                          std::string& path) {
    size_t pos = 0;
    if (url.starts_with("wss://")) {
        use_tls = true;
        pos = 6;
    } else if (url.starts_with("ws://")) {
        use_tls = false;
        pos = 5;
    } else {
        return false;
    }

    // Find host end (: or / or end)
    size_t host_end = url.find_first_of(":/", pos);
    if (host_end == std::string::npos) {
        host = url.substr(pos);
        port = use_tls ? 443 : 80;
        path = "/";
        return !host.empty();
    }

    host = url.substr(pos, host_end - pos);
    if (host.empty()) return false;

    if (url[host_end] == ':') {
        // Parse port
        size_t port_end = url.find('/', host_end + 1);
        std::string port_str;
        if (port_end == std::string::npos) {
            port_str = url.substr(host_end + 1);
            path = "/";
        } else {
            port_str = url.substr(host_end + 1, port_end - host_end - 1);
            path = url.substr(port_end);
        }
        try {
            int p = std::stoi(port_str);
            if (p <= 0 || p > 65535) return false;
            port = static_cast<uint16_t>(p);
        } catch (...) {
            return false;
        }
    } else {
        // url[host_end] == '/'
        port = use_tls ? 443 : 80;
        path = url.substr(host_end);
    }

    if (path.empty()) path = "/";
    return true;
}

// TCP connect helper (blocking with timeout)
static int ws_connect_to(const std::string& host, uint16_t port,
                          int timeout_ms = 10000) {
    struct addrinfo hints {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    std::string port_str = std::to_string(port);
    struct addrinfo* result = nullptr;
    int rv = ::getaddrinfo(host.c_str(), port_str.c_str(), &hints, &result);
    if (rv != 0 || result == nullptr) return -1;

    int fd = -1;
    for (auto* rp = result; rp != nullptr; rp = rp->ai_next) {
        fd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) continue;

        int flags = ::fcntl(fd, F_GETFL, 0);
        if (flags >= 0) ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        rv = ::connect(fd, rp->ai_addr, rp->ai_addrlen);
        if (rv == 0) {
            if (flags >= 0) ::fcntl(fd, F_SETFL, flags);
            break;
        }

        if (errno == EINPROGRESS) {
            struct pollfd pfd {};
            pfd.fd = fd;
            pfd.events = POLLOUT;
            int poll_rv = ::poll(&pfd, 1, timeout_ms);
            if (poll_rv > 0 && (pfd.revents & POLLOUT)) {
                int sock_err = 0;
                socklen_t err_len = sizeof(sock_err);
                ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &sock_err, &err_len);
                if (sock_err == 0) {
                    if (flags >= 0) ::fcntl(fd, F_SETFL, flags);
                    break;
                }
            }
        }

        ::close(fd);
        fd = -1;
    }

    ::freeaddrinfo(result);
    return fd;
}

// Send raw bytes over socket (plain or TLS)
static bool ws_send_raw(WebSocketState* state, const uint8_t* data, size_t len) {
    if (!state) return false;

    if (state->is_tls) {
        std::lock_guard<std::mutex> lock(state->message_queue_mutex_);
        if (!state->tls) return false;
        return state->tls->send(data, len);
    }

    int fd = state->socket_fd.load();
    if (fd >= 0) {
        size_t sent = 0;
        while (sent < len) {
            ssize_t n = ::send(fd, data + sent, len - sent, 0);
            if (n < 0) {
                if (errno == EINTR) continue;
                return false;
            }
            if (n == 0) return false;
            sent += static_cast<size_t>(n);
        }
        return true;
    }

    return false;
}

// Receive raw bytes (blocking with timeout)
static std::vector<uint8_t> ws_recv_raw(WebSocketState* state, int timeout_ms = 10000) {
    std::vector<uint8_t> buffer;
    if (!state) return buffer;

    int fd = state->socket_fd.load();
    if (fd < 0) return buffer;

    struct pollfd pfd {};
    pfd.fd = fd;
    pfd.events = POLLIN;

    int poll_rv = ::poll(&pfd, 1, timeout_ms);
    if (poll_rv <= 0) return buffer;
    if ((pfd.revents & POLLNVAL) || (pfd.revents & POLLERR)) {
        ws_close_socket_fd(state);
        return buffer;
    }

    if (state->is_tls) {
        std::optional<std::vector<uint8_t>> chunk;
        {
            std::lock_guard<std::mutex> lock(state->message_queue_mutex_);
            if (!state->tls) return buffer;
            chunk = state->tls->recv();
        }
        if (!chunk.has_value()) {
            ws_close_socket_fd(state);
            return buffer;
        }
        if (!chunk->empty()) {
            buffer = std::move(*chunk);
        } else if (pfd.revents & POLLHUP) {
            ws_close_socket_fd(state);
        }
    } else {
        uint8_t tmp[8192];
        ssize_t n = ::recv(fd, tmp, sizeof(tmp), 0);
        if (n > 0) {
            buffer.assign(tmp, tmp + n);
        } else if (n == 0) {
            ws_close_socket_fd(state);
        } else if (n < 0 && errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
            ws_close_socket_fd(state);
        }
    }
    return buffer;
}

enum class WsParseResult {
    Complete,
    NeedMoreData,
    Error,
};

struct WsParsedFrame {
    bool fin = true;
    uint8_t opcode = 0;
    std::vector<uint8_t> payload;
    size_t bytes_consumed = 0;
};

static WsParseResult ws_parse_frame(const std::vector<uint8_t>& raw,
                                     size_t offset,
                                     WsParsedFrame& frame,
                                     std::string& error) {
    frame = WsParsedFrame{};
    error.clear();

    if (offset >= raw.size()) return WsParseResult::NeedMoreData;
    if (raw.size() - offset < 2) return WsParseResult::NeedMoreData;

    const uint8_t byte0 = raw[offset];
    const uint8_t byte1 = raw[offset + 1];

    const bool fin = (byte0 & 0x80) != 0;
    const uint8_t rsv = static_cast<uint8_t>(byte0 & 0x70);
    const uint8_t opcode = static_cast<uint8_t>(byte0 & 0x0F);
    const bool masked = (byte1 & 0x80) != 0;
    uint64_t payload_len = static_cast<uint64_t>(byte1 & 0x7F);

    if (rsv != 0) {
        error = "WebSocket frame RSV bits are not supported";
        return WsParseResult::Error;
    }

    if (opcode != 0x0 && opcode != 0x1 && opcode != 0x2 &&
        opcode != 0x8 && opcode != 0x9 && opcode != 0xA) {
        error = "WebSocket frame has unsupported opcode";
        return WsParseResult::Error;
    }

    size_t pos = offset + 2;
    if (payload_len == 126) {
        if (raw.size() - pos < 2) return WsParseResult::NeedMoreData;
        payload_len = (static_cast<uint64_t>(raw[pos]) << 8) |
                      static_cast<uint64_t>(raw[pos + 1]);
        pos += 2;
    } else if (payload_len == 127) {
        if (raw.size() - pos < 8) return WsParseResult::NeedMoreData;
        if (raw[pos] & 0x80) {
            error = "WebSocket frame payload length has invalid MSB";
            return WsParseResult::Error;
        }
        payload_len = 0;
        for (int i = 0; i < 8; ++i) {
            payload_len = (payload_len << 8) | static_cast<uint64_t>(raw[pos + i]);
        }
        pos += 8;
    }

    if (payload_len > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
        error = "WebSocket frame payload is too large";
        return WsParseResult::Error;
    }

    uint8_t mask_key[4] = {0, 0, 0, 0};
    if (masked) {
        if (raw.size() - pos < 4) return WsParseResult::NeedMoreData;
        mask_key[0] = raw[pos];
        mask_key[1] = raw[pos + 1];
        mask_key[2] = raw[pos + 2];
        mask_key[3] = raw[pos + 3];
        pos += 4;
    }

    const size_t payload_size = static_cast<size_t>(payload_len);
    if (raw.size() - pos < payload_size) return WsParseResult::NeedMoreData;

    const bool control_frame = opcode == 0x8 || opcode == 0x9 || opcode == 0xA;
    if (control_frame) {
        if (!fin) {
            error = "WebSocket control frames must not be fragmented";
            return WsParseResult::Error;
        }
        if (payload_len > 125) {
            error = "WebSocket control frame payload is too large";
            return WsParseResult::Error;
        }
    }

    frame.fin = fin;
    frame.opcode = opcode;
    frame.bytes_consumed = (pos - offset) + payload_size;
    frame.payload.assign(raw.begin() + static_cast<std::ptrdiff_t>(pos),
                         raw.begin() + static_cast<std::ptrdiff_t>(pos + payload_size));

    if (masked) {
        for (size_t i = 0; i < frame.payload.size(); ++i) {
            frame.payload[i] ^= mask_key[i % 4];
        }
    }

    return WsParseResult::Complete;
}

// ---- WebSocket frame building ----

// Build a masked WebSocket text frame
static std::vector<uint8_t> ws_build_text_frame(const std::string& payload) {
    std::vector<uint8_t> frame;

    // Byte 0: FIN=1, opcode=0x1 (text)
    frame.push_back(0x81);

    // Byte 1: MASK=1 | length
    size_t len = payload.size();
    if (len < 126) {
        frame.push_back(static_cast<uint8_t>(0x80 | len));
    } else if (len <= 0xFFFF) {
        frame.push_back(static_cast<uint8_t>(0x80 | 126));
        frame.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(len & 0xFF));
    } else {
        frame.push_back(static_cast<uint8_t>(0x80 | 127));
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<uint8_t>((len >> (8 * i)) & 0xFF));
        }
    }

    // 4-byte masking key
    static std::mt19937 rng(std::random_device{}());
    uint8_t mask[4];
    for (auto& m : mask) m = static_cast<uint8_t>(rng() & 0xFF);
    frame.insert(frame.end(), mask, mask + 4);

    // Masked payload
    for (size_t i = 0; i < len; ++i) {
        frame.push_back(static_cast<uint8_t>(payload[i]) ^ mask[i % 4]);
    }

    return frame;
}

// Build a masked WebSocket close frame
static std::vector<uint8_t> ws_build_close_frame(uint16_t code, const std::string& reason) {
    std::vector<uint8_t> payload;
    payload.push_back(static_cast<uint8_t>((code >> 8) & 0xFF));
    payload.push_back(static_cast<uint8_t>(code & 0xFF));
    for (char c : reason) {
        payload.push_back(static_cast<uint8_t>(c));
    }

    std::vector<uint8_t> frame;
    // Byte 0: FIN=1, opcode=0x8 (close)
    frame.push_back(0x88);

    size_t len = payload.size();
    if (len < 126) {
        frame.push_back(static_cast<uint8_t>(0x80 | len));
    } else {
        frame.push_back(static_cast<uint8_t>(0x80 | 126));
        frame.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(len & 0xFF));
    }

    // 4-byte masking key
    static std::mt19937 rng(std::random_device{}());
    uint8_t mask[4];
    for (auto& m : mask) m = static_cast<uint8_t>(rng() & 0xFF);
    frame.insert(frame.end(), mask, mask + 4);

    // Masked payload
    for (size_t i = 0; i < len; ++i) {
        frame.push_back(payload[i] ^ mask[i % 4]);
    }

    return frame;
}

static void ws_receive_loop(WebSocketState* state) {
    if (!state) return;

    state->receive_thread_running_.store(true);

    std::vector<uint8_t> recv_buffer;
    std::vector<uint8_t> fragmented_payload;
    uint8_t fragmented_opcode = 0;
    bool has_fragmented_message = false;

    auto enqueue_and_dispatch_message = [&](const std::vector<uint8_t>& payload) {
        std::string message(payload.begin(), payload.end());
        {
            std::lock_guard<std::mutex> lock(state->message_queue_mutex_);
            state->message_queue.push_back(message);
        }
        ws_fire_message_event(state, message);
    };

    while (!state->should_close_thread_.load() &&
           state->readyState.load() != 3) {
        auto chunk = ws_recv_raw(state, k_ws_recv_timeout_ms);
        if (state->should_close_thread_.load()) break;

        if (chunk.empty()) {
            if (state->socket_fd.load() < 0) {
                int previous_state = state->readyState.exchange(3);
                if (previous_state != 3) {
                    ws_fire_close_event(state, 1006, "", false);
                }
                break;
            }
            continue;
        }

        recv_buffer.insert(recv_buffer.end(), chunk.begin(), chunk.end());

        size_t offset = 0;
        bool should_break_loop = false;
        while (offset < recv_buffer.size()) {
            WsParsedFrame frame;
            std::string parse_error;
            WsParseResult parse_result = ws_parse_frame(recv_buffer, offset, frame, parse_error);

            if (parse_result == WsParseResult::NeedMoreData) {
                break;
            }

            if (parse_result == WsParseResult::Error) {
                ws_fire_error_event(state, parse_error);
                recv_buffer.clear();
                has_fragmented_message = false;
                fragmented_payload.clear();
                fragmented_opcode = 0;
                offset = 0;
                break;
            }

            offset += frame.bytes_consumed;

            if (frame.opcode == 0x0) { // Continuation
                if (!has_fragmented_message) {
                    ws_fire_error_event(state, "Unexpected continuation frame");
                    continue;
                }
                fragmented_payload.insert(
                    fragmented_payload.end(), frame.payload.begin(), frame.payload.end());
                if (frame.fin) {
                    if (fragmented_opcode == 0x1 || fragmented_opcode == 0x2) {
                        enqueue_and_dispatch_message(fragmented_payload);
                    } else {
                        ws_fire_error_event(state,
                            "Invalid fragmented message opcode");
                    }
                    has_fragmented_message = false;
                    fragmented_payload.clear();
                    fragmented_opcode = 0;
                }
                continue;
            }

            if (frame.opcode == 0x1 || frame.opcode == 0x2) { // Text / binary
                if (has_fragmented_message) {
                    ws_fire_error_event(state,
                        "Received non-continuation frame during fragmented message");
                    has_fragmented_message = false;
                    fragmented_payload.clear();
                    fragmented_opcode = 0;
                }

                if (frame.fin) {
                    enqueue_and_dispatch_message(frame.payload);
                } else {
                    has_fragmented_message = true;
                    fragmented_opcode = frame.opcode;
                    fragmented_payload = frame.payload;
                }
                continue;
            }

            if (frame.opcode == 0x8) { // Close
                uint16_t close_code = 1000;
                std::string close_reason;
                if (frame.payload.size() >= 2) {
                    close_code = static_cast<uint16_t>(
                        (static_cast<uint16_t>(frame.payload[0]) << 8) |
                        static_cast<uint16_t>(frame.payload[1]));
                    if (frame.payload.size() > 2) {
                        close_reason.assign(frame.payload.begin() + 2, frame.payload.end());
                    }
                }

                if (state->readyState.load() == 1) {
                    auto close_frame = ws_build_close_frame(close_code, close_reason);
                    ws_send_raw(state, close_frame.data(), close_frame.size());
                }

                state->should_close_thread_.store(true);
                int previous_state = state->readyState.exchange(3);
                ws_close_socket_fd(state);
                ws_close_tls(state);

                if (previous_state != 3) {
                    ws_fire_close_event(state, close_code, close_reason, true);
                }
                should_break_loop = true;
                break;
            }

            if (frame.opcode == 0x9) { // Ping -> Pong
                std::vector<uint8_t> pong_frame;
                pong_frame.push_back(0x8A); // FIN + pong

                const size_t len = frame.payload.size();
                if (len < 126) {
                    pong_frame.push_back(static_cast<uint8_t>(0x80 | len));
                } else if (len <= 0xFFFF) {
                    pong_frame.push_back(static_cast<uint8_t>(0x80 | 126));
                    pong_frame.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
                    pong_frame.push_back(static_cast<uint8_t>(len & 0xFF));
                } else {
                    pong_frame.push_back(static_cast<uint8_t>(0x80 | 127));
                    for (int i = 7; i >= 0; --i) {
                        pong_frame.push_back(
                            static_cast<uint8_t>((len >> (8 * i)) & 0xFF));
                    }
                }

                static std::mt19937 rng(std::random_device{}());
                uint8_t mask[4];
                for (auto& m : mask) m = static_cast<uint8_t>(rng() & 0xFF);
                pong_frame.insert(pong_frame.end(), mask, mask + 4);

                for (size_t i = 0; i < len; ++i) {
                    pong_frame.push_back(frame.payload[i] ^ mask[i % 4]);
                }

                if (!ws_send_raw(state, pong_frame.data(), pong_frame.size())) {
                    ws_fire_error_event(state, "Failed to send pong frame");
                }
                continue;
            }

            // Pong frame (0xA) ignored.
        }

        if (offset > 0 && offset <= recv_buffer.size()) {
            recv_buffer.erase(
                recv_buffer.begin(),
                recv_buffer.begin() + static_cast<std::ptrdiff_t>(offset));
        }

        if (should_break_loop) break;
    }

    state->receive_thread_running_.store(false);
}

// ---- WebSocket JS methods ----

// ws.send(data)
static JSValue js_ws_send(JSContext* ctx, JSValueConst this_val,
                           int argc, JSValueConst* argv) {
    auto* state = get_ws_state(this_val);
    if (!state) return JS_ThrowTypeError(ctx, "Invalid WebSocket object");

    if (state->readyState.load() != 1) { // Not OPEN
        return JS_ThrowTypeError(ctx,
            "WebSocket is not in the OPEN state");
    }

    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "WebSocket.send requires 1 argument");
    }

    const char* data_str = JS_ToCString(ctx, argv[0]);
    if (!data_str) return JS_EXCEPTION;

    std::string payload(data_str);
    JS_FreeCString(ctx, data_str);

    auto frame = ws_build_text_frame(payload);
    if (!ws_send_raw(state, frame.data(), frame.size())) {
        ws_fire_error_event(state, "WebSocket send failed");
        return JS_ThrowTypeError(ctx, "WebSocket send failed");
    }

    return JS_UNDEFINED;
}

// ws.close([code [, reason]])
static JSValue js_ws_close(JSContext* ctx, JSValueConst this_val,
                            int argc, JSValueConst* argv) {
    auto* state = get_ws_state(this_val);
    if (!state) return JS_ThrowTypeError(ctx, "Invalid WebSocket object");

    int current_state = state->readyState.load();
    if (current_state == 2 || current_state == 3) {
        // Already CLOSING or CLOSED — no-op
        return JS_UNDEFINED;
    }

    uint16_t code = 1000; // Normal closure
    std::string reason;

    if (argc >= 1) {
        int32_t c = 0;
        JS_ToInt32(ctx, &c, argv[0]);
        code = static_cast<uint16_t>(c);
    }
    if (argc >= 2) {
        const char* r = JS_ToCString(ctx, argv[1]);
        if (r) {
            reason = r;
            JS_FreeCString(ctx, r);
        }
    }

    state->readyState.store(2); // CLOSING

    // Send close frame (best effort)
    if (state->socket_fd.load() >= 0) {
        auto frame = ws_build_close_frame(code, reason);
        ws_send_raw(state, frame.data(), frame.size());
    }

    ws_stop_receive_thread(state, k_ws_thread_join_timeout_ms);
    ws_close_tls(state);
    ws_close_socket_fd(state);

    int previous_state = state->readyState.exchange(3); // CLOSED

    if (previous_state != 3) {
        ws_fire_close_event(state, code, reason, true);
    }

    return JS_UNDEFINED;
}

// ---- Property getters ----

static JSValue js_ws_get_readyState(JSContext* ctx, JSValueConst this_val) {
    auto* state = get_ws_state(this_val);
    if (!state) return JS_NewInt32(ctx, 3); // CLOSED
    return JS_NewInt32(ctx, state->readyState.load());
}

static JSValue js_ws_get_url(JSContext* ctx, JSValueConst this_val) {
    auto* state = get_ws_state(this_val);
    if (!state) return JS_NewString(ctx, "");
    return JS_NewString(ctx, state->url.c_str());
}

static JSValue js_ws_get_protocol(JSContext* ctx, JSValueConst this_val) {
    auto* state = get_ws_state(this_val);
    if (!state) return JS_NewString(ctx, "");
    return JS_NewString(ctx, state->protocol.c_str());
}

static JSValue js_ws_get_bufferedAmount(JSContext* ctx, JSValueConst /*this_val*/) {
    return JS_NewInt32(ctx, 0); // stub
}

// ---- Event handler getters/setters ----

static JSValue js_ws_get_onopen(JSContext* ctx, JSValueConst this_val) {
    auto* state = get_ws_state(this_val);
    if (!state) return JS_NULL;
    std::lock_guard<std::mutex> lock(state->message_queue_mutex_);
    return JS_DupValue(ctx, state->onopen);
}

static JSValue js_ws_set_onopen(JSContext* ctx, JSValueConst this_val, JSValueConst val) {
    auto* state = get_ws_state(this_val);
    if (!state) return JS_EXCEPTION;
    JSValue old_handler = JS_UNDEFINED;
    {
        std::lock_guard<std::mutex> lock(state->message_queue_mutex_);
        old_handler = state->onopen;
        state->onopen = JS_DupValue(ctx, val);
    }
    JS_FreeValue(ctx, old_handler);
    return JS_UNDEFINED;
}

static JSValue js_ws_get_onmessage(JSContext* ctx, JSValueConst this_val) {
    auto* state = get_ws_state(this_val);
    if (!state) return JS_NULL;
    std::lock_guard<std::mutex> lock(state->message_queue_mutex_);
    return JS_DupValue(ctx, state->onmessage);
}

static JSValue js_ws_set_onmessage(JSContext* ctx, JSValueConst this_val, JSValueConst val) {
    auto* state = get_ws_state(this_val);
    if (!state) return JS_EXCEPTION;
    JSValue old_handler = JS_UNDEFINED;
    {
        std::lock_guard<std::mutex> lock(state->message_queue_mutex_);
        old_handler = state->onmessage;
        state->onmessage = JS_DupValue(ctx, val);
    }
    JS_FreeValue(ctx, old_handler);
    return JS_UNDEFINED;
}

static JSValue js_ws_get_onclose(JSContext* ctx, JSValueConst this_val) {
    auto* state = get_ws_state(this_val);
    if (!state) return JS_NULL;
    std::lock_guard<std::mutex> lock(state->message_queue_mutex_);
    return JS_DupValue(ctx, state->onclose);
}

static JSValue js_ws_set_onclose(JSContext* ctx, JSValueConst this_val, JSValueConst val) {
    auto* state = get_ws_state(this_val);
    if (!state) return JS_EXCEPTION;
    JSValue old_handler = JS_UNDEFINED;
    {
        std::lock_guard<std::mutex> lock(state->message_queue_mutex_);
        old_handler = state->onclose;
        state->onclose = JS_DupValue(ctx, val);
    }
    JS_FreeValue(ctx, old_handler);
    return JS_UNDEFINED;
}

static JSValue js_ws_get_onerror(JSContext* ctx, JSValueConst this_val) {
    auto* state = get_ws_state(this_val);
    if (!state) return JS_NULL;
    std::lock_guard<std::mutex> lock(state->message_queue_mutex_);
    return JS_DupValue(ctx, state->onerror);
}

static JSValue js_ws_set_onerror(JSContext* ctx, JSValueConst this_val, JSValueConst val) {
    auto* state = get_ws_state(this_val);
    if (!state) return JS_EXCEPTION;
    JSValue old_handler = JS_UNDEFINED;
    {
        std::lock_guard<std::mutex> lock(state->message_queue_mutex_);
        old_handler = state->onerror;
        state->onerror = JS_DupValue(ctx, val);
    }
    JS_FreeValue(ctx, old_handler);
    return JS_UNDEFINED;
}

// ---- Finalizer ----

static void js_ws_finalizer(JSRuntime* rt, JSValue val) {
    auto* state = static_cast<WebSocketState*>(JS_GetOpaque(val, ws_class_id));
    if (!state) return;

    ws_stop_receive_thread(state, k_ws_thread_join_timeout_ms);
    ws_close_tls(state);
    ws_close_socket_fd(state);

    // Free event handler JSValues
    JS_FreeValueRT(rt, state->onopen);
    JS_FreeValueRT(rt, state->onmessage);
    JS_FreeValueRT(rt, state->onclose);
    JS_FreeValueRT(rt, state->onerror);

    delete state;
}

// GC mark callback — tell QuickJS about our stored JSValue event handlers
static void js_ws_gc_mark(JSRuntime* rt, JSValueConst val,
                           JS_MarkFunc* mark_func) {
    auto* state = static_cast<WebSocketState*>(JS_GetOpaque(val, ws_class_id));
    if (!state) return;
    std::lock_guard<std::mutex> lock(state->message_queue_mutex_);
    JS_MarkValue(rt, state->onopen, mark_func);
    JS_MarkValue(rt, state->onmessage, mark_func);
    JS_MarkValue(rt, state->onclose, mark_func);
    JS_MarkValue(rt, state->onerror, mark_func);
}

static JSClassDef ws_class_def = {
    "WebSocket",
    js_ws_finalizer,
    js_ws_gc_mark,
    nullptr, nullptr
};

// ---- Constructor: new WebSocket(url [, protocols]) ----

static JSValue js_ws_constructor(JSContext* ctx, JSValueConst new_target,
                                  int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx,
            "Failed to construct 'WebSocket': 1 argument required");
    }

    const char* url_cstr = JS_ToCString(ctx, argv[0]);
    if (!url_cstr) return JS_EXCEPTION;
    std::string url_str(url_cstr);
    JS_FreeCString(ctx, url_cstr);

    // Parse URL
    bool use_tls = false;
    std::string host;
    uint16_t port = 0;
    std::string path;

    if (!parse_ws_url(url_str, use_tls, host, port, path)) {
        return JS_ThrowTypeError(ctx,
            "Failed to construct 'WebSocket': The URL '%s' is invalid",
            url_str.c_str());
    }

    // Parse optional protocols argument
    std::string requested_protocol;
    if (argc >= 2 && JS_IsString(argv[1])) {
        const char* p = JS_ToCString(ctx, argv[1]);
        if (p) {
            requested_protocol = p;
            JS_FreeCString(ctx, p);
        }
    }

    // Create the JS object
    JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto)) return JS_EXCEPTION;

    JSValue obj = JS_NewObjectProtoClass(ctx, proto, ws_class_id);
    JS_FreeValue(ctx, proto);
    if (JS_IsException(obj)) return JS_EXCEPTION;

    auto* state = new WebSocketState();
    state->url = url_str;
    state->is_tls = use_tls;
    state->protocol = requested_protocol;
    state->runtime = JS_GetRuntime(ctx);
    JS_SetOpaque(obj, state);

    // TCP connect
    int fd = ws_connect_to(host, port);
    if (fd < 0) {
        state->readyState.store(3); // CLOSED
        ws_fire_error_event(state, "WebSocket TCP connect failed");
        return obj; // Return the object in CLOSED state
    }

    state->socket_fd.store(fd);

    // TLS handshake if needed
    if (use_tls) {
        {
            std::lock_guard<std::mutex> lock(state->message_queue_mutex_);
            state->tls = new clever::net::TlsSocket();
        }
        clever::net::TlsSocket* tls = nullptr;
        {
            std::lock_guard<std::mutex> lock(state->message_queue_mutex_);
            tls = state->tls;
        }
        if (!tls || !tls->connect(host, port, fd)) {
            ws_close_tls(state);
            ws_close_socket_fd(state);
            state->readyState.store(3); // CLOSED
            return obj;
        }
    }

    // Build and send WebSocket upgrade request
    std::string ws_key = generate_ws_key();
    std::string host_header = host;
    if ((use_tls && port != 443) || (!use_tls && port != 80)) {
        host_header += ":" + std::to_string(port);
    }

    std::string upgrade_request =
        "GET " + path + " HTTP/1.1\r\n"
        "Host: " + host_header + "\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: " + ws_key + "\r\n"
        "Sec-WebSocket-Version: 13\r\n";

    if (!requested_protocol.empty()) {
        upgrade_request += "Sec-WebSocket-Protocol: " + requested_protocol + "\r\n";
    }
    upgrade_request += "\r\n";

    auto* req_data = reinterpret_cast<const uint8_t*>(upgrade_request.data());
    if (!ws_send_raw(state, req_data, upgrade_request.size())) {
        ws_close_tls(state);
        ws_close_socket_fd(state);
        state->readyState.store(3);
        return obj;
    }

    // Read upgrade response
    auto response_data = ws_recv_raw(state, 10000);
    if (response_data.empty()) {
        ws_close_tls(state);
        ws_close_socket_fd(state);
        state->readyState.store(3);
        return obj;
    }

    // Verify 101 Switching Protocols
    std::string response_str(response_data.begin(), response_data.end());
    if (response_str.find("101") == std::string::npos) {
        ws_close_tls(state);
        ws_close_socket_fd(state);
        state->readyState.store(3);
        return obj;
    }

    // Connection established!
    state->readyState.store(1); // OPEN

    state->should_close_thread_.store(false);
    state->receive_thread_ = std::thread([state] {
        ws_receive_loop(state);
    });

    // Fire onopen event immediately (synchronous engine)
    JSValue onopen_handler = JS_UNDEFINED;
    {
        std::lock_guard<std::mutex> lock(state->message_queue_mutex_);
        onopen_handler = JS_DupValue(ctx, state->onopen);
    }
    if (JS_IsFunction(ctx, onopen_handler)) {
        JSValue event = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, event, "type", JS_NewString(ctx, "open"));
        JSValue ret = JS_Call(ctx, onopen_handler, JS_UNDEFINED, 1, &event);
        if (JS_IsException(ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, event);
    }
    JS_FreeValue(ctx, onopen_handler);

    return obj;
}

// ---- Prototype function list ----

static const JSCFunctionListEntry ws_proto_funcs[] = {
    JS_CFUNC_DEF("send", 1, js_ws_send),
    JS_CFUNC_DEF("close", 0, js_ws_close),

    // Read-only property getters
    JS_CGETSET_DEF("readyState", js_ws_get_readyState, nullptr),
    JS_CGETSET_DEF("url", js_ws_get_url, nullptr),
    JS_CGETSET_DEF("protocol", js_ws_get_protocol, nullptr),
    JS_CGETSET_DEF("bufferedAmount", js_ws_get_bufferedAmount, nullptr),

    // Event handler getters/setters
    JS_CGETSET_DEF("onopen", js_ws_get_onopen, js_ws_set_onopen),
    JS_CGETSET_DEF("onmessage", js_ws_get_onmessage, js_ws_set_onmessage),
    JS_CGETSET_DEF("onclose", js_ws_get_onclose, js_ws_set_onclose),
    JS_CGETSET_DEF("onerror", js_ws_get_onerror, js_ws_set_onerror),
};

// =========================================================================
// FormData class
// =========================================================================

static void js_formdata_finalizer(JSRuntime* /*rt*/, JSValue val) {
    auto* state = static_cast<FormDataState*>(JS_GetOpaque(val, formdata_class_id));
    delete state;
}

static JSClassDef formdata_class_def = {
    "FormData",
    js_formdata_finalizer,
    nullptr, nullptr, nullptr
};

static FormDataState* get_formdata_state(JSValueConst this_val) {
    return static_cast<FormDataState*>(JS_GetOpaque(this_val, formdata_class_id));
}

// FormData.prototype.append(name, value)
static JSValue js_formdata_append(JSContext* ctx, JSValueConst this_val,
                                    int argc, JSValueConst* argv) {
    auto* state = get_formdata_state(this_val);
    if (!state) return JS_ThrowTypeError(ctx, "not a FormData");
    if (argc < 2) return JS_ThrowTypeError(ctx, "append requires 2 arguments");

    const char* name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_EXCEPTION;
    const char* value = JS_ToCString(ctx, argv[1]);
    if (!value) { JS_FreeCString(ctx, name); return JS_EXCEPTION; }

    state->entries.emplace_back(name, value);
    JS_FreeCString(ctx, name);
    JS_FreeCString(ctx, value);
    return JS_UNDEFINED;
}

// FormData.prototype.get(name)
static JSValue js_formdata_get(JSContext* ctx, JSValueConst this_val,
                                int argc, JSValueConst* argv) {
    auto* state = get_formdata_state(this_val);
    if (!state) return JS_ThrowTypeError(ctx, "not a FormData");
    if (argc < 1) return JS_NULL;

    const char* name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_EXCEPTION;

    for (const auto& [k, v] : state->entries) {
        if (k == name) {
            JS_FreeCString(ctx, name);
            return JS_NewString(ctx, v.c_str());
        }
    }
    JS_FreeCString(ctx, name);
    return JS_NULL;
}

// FormData.prototype.getAll(name)
static JSValue js_formdata_get_all(JSContext* ctx, JSValueConst this_val,
                                     int argc, JSValueConst* argv) {
    auto* state = get_formdata_state(this_val);
    if (!state) return JS_ThrowTypeError(ctx, "not a FormData");
    if (argc < 1) return JS_NewArray(ctx);

    const char* name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_EXCEPTION;

    JSValue arr = JS_NewArray(ctx);
    uint32_t idx = 0;
    for (const auto& [k, v] : state->entries) {
        if (k == name) {
            JS_SetPropertyUint32(ctx, arr, idx++,
                JS_NewString(ctx, v.c_str()));
        }
    }
    JS_FreeCString(ctx, name);
    return arr;
}

// FormData.prototype.has(name)
static JSValue js_formdata_has(JSContext* ctx, JSValueConst this_val,
                                int argc, JSValueConst* argv) {
    auto* state = get_formdata_state(this_val);
    if (!state) return JS_ThrowTypeError(ctx, "not a FormData");
    if (argc < 1) return JS_FALSE;

    const char* name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_EXCEPTION;

    for (const auto& [k, v] : state->entries) {
        if (k == name) {
            JS_FreeCString(ctx, name);
            return JS_TRUE;
        }
    }
    JS_FreeCString(ctx, name);
    return JS_FALSE;
}

// FormData.prototype.delete(name)
static JSValue js_formdata_delete(JSContext* ctx, JSValueConst this_val,
                                    int argc, JSValueConst* argv) {
    auto* state = get_formdata_state(this_val);
    if (!state) return JS_ThrowTypeError(ctx, "not a FormData");
    if (argc < 1) return JS_UNDEFINED;

    const char* name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_EXCEPTION;

    auto it = state->entries.begin();
    while (it != state->entries.end()) {
        if (it->first == name) {
            it = state->entries.erase(it);
        } else {
            ++it;
        }
    }
    JS_FreeCString(ctx, name);
    return JS_UNDEFINED;
}

// FormData.prototype.set(name, value)
static JSValue js_formdata_set(JSContext* ctx, JSValueConst this_val,
                                int argc, JSValueConst* argv) {
    auto* state = get_formdata_state(this_val);
    if (!state) return JS_ThrowTypeError(ctx, "not a FormData");
    if (argc < 2) return JS_ThrowTypeError(ctx, "set requires 2 arguments");

    const char* name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_EXCEPTION;
    const char* value = JS_ToCString(ctx, argv[1]);
    if (!value) { JS_FreeCString(ctx, name); return JS_EXCEPTION; }

    // Remove all existing entries with the same name
    bool found = false;
    auto it = state->entries.begin();
    while (it != state->entries.end()) {
        if (it->first == name) {
            if (!found) {
                // Replace the first occurrence
                it->second = value;
                found = true;
                ++it;
            } else {
                it = state->entries.erase(it);
            }
        } else {
            ++it;
        }
    }

    // If not found, append
    if (!found) {
        state->entries.emplace_back(name, value);
    }

    JS_FreeCString(ctx, name);
    JS_FreeCString(ctx, value);
    return JS_UNDEFINED;
}

// FormData.prototype.entries()
static JSValue js_formdata_entries(JSContext* ctx, JSValueConst this_val,
                                     int /*argc*/, JSValueConst* /*argv*/) {
    auto* state = get_formdata_state(this_val);
    if (!state) return JS_ThrowTypeError(ctx, "not a FormData");

    JSValue arr = JS_NewArray(ctx);
    uint32_t idx = 0;
    for (const auto& [k, v] : state->entries) {
        JSValue pair = JS_NewArray(ctx);
        JS_SetPropertyUint32(ctx, pair, 0, JS_NewString(ctx, k.c_str()));
        JS_SetPropertyUint32(ctx, pair, 1, JS_NewString(ctx, v.c_str()));
        JS_SetPropertyUint32(ctx, arr, idx++, pair);
    }
    return arr;
}

// FormData.prototype.keys()
static JSValue js_formdata_keys(JSContext* ctx, JSValueConst this_val,
                                  int /*argc*/, JSValueConst* /*argv*/) {
    auto* state = get_formdata_state(this_val);
    if (!state) return JS_ThrowTypeError(ctx, "not a FormData");

    JSValue arr = JS_NewArray(ctx);
    uint32_t idx = 0;
    for (const auto& [k, v] : state->entries) {
        JS_SetPropertyUint32(ctx, arr, idx++, JS_NewString(ctx, k.c_str()));
    }
    return arr;
}

// FormData.prototype.values()
static JSValue js_formdata_values(JSContext* ctx, JSValueConst this_val,
                                    int /*argc*/, JSValueConst* /*argv*/) {
    auto* state = get_formdata_state(this_val);
    if (!state) return JS_ThrowTypeError(ctx, "not a FormData");

    JSValue arr = JS_NewArray(ctx);
    uint32_t idx = 0;
    for (const auto& [k, v] : state->entries) {
        JS_SetPropertyUint32(ctx, arr, idx++, JS_NewString(ctx, v.c_str()));
    }
    return arr;
}

// FormData.prototype.forEach(callback)
static JSValue js_formdata_forEach(JSContext* ctx, JSValueConst this_val,
                                     int argc, JSValueConst* argv) {
    auto* state = get_formdata_state(this_val);
    if (!state) return JS_ThrowTypeError(ctx, "not a FormData");
    if (argc < 1 || !JS_IsFunction(ctx, argv[0]))
        return JS_ThrowTypeError(ctx, "forEach requires a function argument");

    for (const auto& [k, v] : state->entries) {
        JSValue args[3] = {
            JS_NewString(ctx, v.c_str()),
            JS_NewString(ctx, k.c_str()),
            JS_DupValue(ctx, this_val)
        };
        JSValue ret = JS_Call(ctx, argv[0], JS_UNDEFINED, 3, args);
        JS_FreeValue(ctx, args[0]);
        JS_FreeValue(ctx, args[1]);
        JS_FreeValue(ctx, args[2]);
        if (JS_IsException(ret)) return ret;
        JS_FreeValue(ctx, ret);
    }
    return JS_UNDEFINED;
}

// Constructor: new FormData() or new FormData(formElement)
static JSValue js_formdata_constructor(JSContext* ctx, JSValueConst new_target,
                                         int argc, JSValueConst* argv) {
    JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto)) return JS_EXCEPTION;

    JSValue obj = JS_NewObjectProtoClass(ctx, proto, formdata_class_id);
    JS_FreeValue(ctx, proto);
    if (JS_IsException(obj)) return obj;

    auto* state = new FormDataState();
    JS_SetOpaque(obj, state);

    // If a form element was passed, collect all named form controls
    if (argc >= 1 && JS_IsObject(argv[0])) {
        // Use querySelectorAll on the form element to gather input/textarea/select elements.
        // We call the JS method directly on the element object.
        JSValue form = argv[0];

        // Helper lambda: call form.querySelectorAll(selector) and iterate
        // We use a JS eval approach to extract form field data safely.
        const char* collect_src = R"JS(
(function(form) {
    var result = [];
    if (!form || typeof form.querySelectorAll !== 'function') return result;
    var inputs = form.querySelectorAll('input, textarea, select');
    for (var i = 0; i < inputs.length; i++) {
        var el = inputs[i];
        var name = el.name || el.getAttribute('name');
        if (!name || el.disabled) continue;
        var tag = (el.tagName || '').toLowerCase();
        var type = (el.type || '').toLowerCase();
        if (tag === 'input') {
            if (type === 'checkbox' || type === 'radio') {
                if (el.checked) {
                    result.push([name, el.value || 'on']);
                }
            } else if (type !== 'submit' && type !== 'image' && type !== 'button' && type !== 'file') {
                result.push([name, el.value || '']);
            }
        } else if (tag === 'textarea') {
            result.push([name, el.value || el.textContent || '']);
        } else if (tag === 'select') {
            var opts = el.options || el.querySelectorAll('option');
            for (var j = 0; j < (opts ? opts.length : 0); j++) {
                if (opts[j].selected) {
                    result.push([name, opts[j].value || opts[j].textContent || '']);
                    break;
                }
            }
        }
    }
    return result;
})
)JS";
        JSValue collect_fn = JS_Eval(ctx, collect_src, std::strlen(collect_src),
                                      "<formdata-collect>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsFunction(ctx, collect_fn)) {
            JSValue result = JS_Call(ctx, collect_fn, JS_UNDEFINED, 1, &form);
            if (JS_IsArray(ctx, result)) {
                JSValue len_val = JS_GetPropertyStr(ctx, result, "length");
                int32_t len = 0;
                JS_ToInt32(ctx, &len, len_val);
                JS_FreeValue(ctx, len_val);
                for (int32_t i = 0; i < len; i++) {
                    JSValue pair = JS_GetPropertyUint32(ctx, result, static_cast<uint32_t>(i));
                    if (JS_IsArray(ctx, pair)) {
                        JSValue kv = JS_GetPropertyUint32(ctx, pair, 0);
                        JSValue vv = JS_GetPropertyUint32(ctx, pair, 1);
                        const char* k = JS_ToCString(ctx, kv);
                        const char* v = JS_ToCString(ctx, vv);
                        if (k && v) {
                            state->entries.emplace_back(k, v);
                        }
                        if (k) JS_FreeCString(ctx, k);
                        if (v) JS_FreeCString(ctx, v);
                        JS_FreeValue(ctx, kv);
                        JS_FreeValue(ctx, vv);
                    }
                    JS_FreeValue(ctx, pair);
                }
            }
            JS_FreeValue(ctx, result);
        }
        JS_FreeValue(ctx, collect_fn);
    }

    return obj;
}

static const JSCFunctionListEntry formdata_proto_funcs[] = {
    JS_CFUNC_DEF("append", 2, js_formdata_append),
    JS_CFUNC_DEF("get", 1, js_formdata_get),
    JS_CFUNC_DEF("getAll", 1, js_formdata_get_all),
    JS_CFUNC_DEF("has", 1, js_formdata_has),
    JS_CFUNC_DEF("set", 2, js_formdata_set),
    JS_CFUNC_DEF("entries", 0, js_formdata_entries),
    JS_CFUNC_DEF("keys", 0, js_formdata_keys),
    JS_CFUNC_DEF("values", 0, js_formdata_values),
    JS_CFUNC_DEF("forEach", 1, js_formdata_forEach),
};

// Note: "delete" is a C++ reserved word, so we register it via JS_SetPropertyStr
// in install rather than in the function list.

} // anonymous namespace

// =========================================================================
// Public API
// =========================================================================

void install_fetch_bindings(JSContext* ctx) {
    JSRuntime* rt = JS_GetRuntime(ctx);

    // ---- Register XMLHttpRequest class (once per runtime) ----
    if (xhr_class_id == 0) {
        JS_NewClassID(&xhr_class_id);
    }
    if (!JS_IsRegisteredClass(rt, xhr_class_id)) {
        JS_NewClass(rt, xhr_class_id, &xhr_class_def);
    }

    // Create the prototype object with methods and getters
    JSValue proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, xhr_proto_funcs,
                                sizeof(xhr_proto_funcs) / sizeof(xhr_proto_funcs[0]));

    // Create the constructor function
    JSValue ctor = JS_NewCFunction2(ctx, js_xhr_constructor,
                                     "XMLHttpRequest", 0,
                                     JS_CFUNC_constructor, 0);

    // Set prototype on constructor
    JS_SetConstructor(ctx, ctor, proto);
    JS_SetClassProto(ctx, xhr_class_id, proto);

    // Add static constants to the constructor
    JS_SetPropertyStr(ctx, ctor, "UNSENT", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, ctor, "OPENED", JS_NewInt32(ctx, 1));
    JS_SetPropertyStr(ctx, ctor, "HEADERS_RECEIVED", JS_NewInt32(ctx, 2));
    JS_SetPropertyStr(ctx, ctor, "LOADING", JS_NewInt32(ctx, 3));
    JS_SetPropertyStr(ctx, ctor, "DONE", JS_NewInt32(ctx, 4));

    // ---- Register Headers class ----
    if (headers_class_id == 0) {
        JS_NewClassID(&headers_class_id);
    }
    if (!JS_IsRegisteredClass(rt, headers_class_id)) {
        JS_NewClass(rt, headers_class_id, &headers_class_def);
    }

    JSValue headers_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, headers_proto, headers_proto_funcs,
                                sizeof(headers_proto_funcs) / sizeof(headers_proto_funcs[0]));
    JS_SetClassProto(ctx, headers_class_id, headers_proto);

    // ---- Register Response class ----
    if (response_class_id == 0) {
        JS_NewClassID(&response_class_id);
    }
    if (!JS_IsRegisteredClass(rt, response_class_id)) {
        JS_NewClass(rt, response_class_id, &response_class_def);
    }

    JSValue response_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, response_proto, response_proto_funcs,
                                sizeof(response_proto_funcs) / sizeof(response_proto_funcs[0]));

    JSValue response_ctor = JS_NewCFunction2(ctx, js_response_constructor,
                                              "Response", 0,
                                              JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, response_ctor, response_proto);
    JS_SetClassProto(ctx, response_class_id, response_proto);

    // ---- Register WebSocket class ----
    if (ws_class_id == 0) {
        JS_NewClassID(&ws_class_id);
    }
    if (!JS_IsRegisteredClass(rt, ws_class_id)) {
        JS_NewClass(rt, ws_class_id, &ws_class_def);
    }

    JSValue ws_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ws_proto, ws_proto_funcs,
                                sizeof(ws_proto_funcs) / sizeof(ws_proto_funcs[0]));

    JSValue ws_ctor = JS_NewCFunction2(ctx, js_ws_constructor,
                                        "WebSocket", 1,
                                        JS_CFUNC_constructor, 0);

    JS_SetConstructor(ctx, ws_ctor, ws_proto);
    JS_SetClassProto(ctx, ws_class_id, ws_proto);

    // Add static constants to WebSocket constructor
    JS_SetPropertyStr(ctx, ws_ctor, "CONNECTING", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, ws_ctor, "OPEN", JS_NewInt32(ctx, 1));
    JS_SetPropertyStr(ctx, ws_ctor, "CLOSING", JS_NewInt32(ctx, 2));
    JS_SetPropertyStr(ctx, ws_ctor, "CLOSED", JS_NewInt32(ctx, 3));

    // ---- Register FormData class ----
    if (formdata_class_id == 0) {
        JS_NewClassID(&formdata_class_id);
    }
    if (!JS_IsRegisteredClass(rt, formdata_class_id)) {
        JS_NewClass(rt, formdata_class_id, &formdata_class_def);
    }

    JSValue formdata_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, formdata_proto, formdata_proto_funcs,
                                sizeof(formdata_proto_funcs) / sizeof(formdata_proto_funcs[0]));
    // Register 'delete' method separately (reserved word in C++)
    JS_SetPropertyStr(ctx, formdata_proto, "delete",
        JS_NewCFunction(ctx, js_formdata_delete, "delete", 1));

    JSValue formdata_ctor = JS_NewCFunction2(ctx, js_formdata_constructor,
                                              "FormData", 0,
                                              JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, formdata_ctor, formdata_proto);
    JS_SetClassProto(ctx, formdata_class_id, formdata_proto);

    // ---- Register on globalThis ----
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "XMLHttpRequest", ctor);
    JS_SetPropertyStr(ctx, global, "fetch",
        JS_NewCFunction(ctx, js_global_fetch, "fetch", 1));
    JS_SetPropertyStr(ctx, global, "WebSocket", ws_ctor);
    JS_SetPropertyStr(ctx, global, "FormData", formdata_ctor);
    JS_SetPropertyStr(ctx, global, "Response", response_ctor);
    JS_FreeValue(ctx, global);

    // ------------------------------------------------------------------
    // FormData.prototype[Symbol.iterator] — enables for...of iteration
    // Also add FormData.prototype.size getter
    // ------------------------------------------------------------------
    {
        const char* fd_iter_src = R"JS(
(function() {
    if (typeof FormData !== 'undefined' && FormData.prototype) {
        // Symbol.iterator — iterates [key, value] pairs (same as entries())
        FormData.prototype[Symbol.iterator] = function() {
            var arr = this.entries();
            var idx = 0;
            return {
                next: function() {
                    if (idx < arr.length) {
                        return { value: arr[idx++], done: false };
                    }
                    return { value: undefined, done: true };
                },
                [Symbol.iterator]: function() { return this; }
            };
        };
        // size getter
        Object.defineProperty(FormData.prototype, 'size', {
            get: function() { return this.keys().length; },
            configurable: true
        });
    }
})();
)JS";
        JSValue fd_iter_ret = JS_Eval(ctx, fd_iter_src, std::strlen(fd_iter_src),
                                       "<formdata-iterator>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(fd_iter_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, fd_iter_ret);
    }

    // ------------------------------------------------------------------
    // WebSocket.prototype.addEventListener / removeEventListener / binaryType
    // ------------------------------------------------------------------
    {
        const char* ws_ext_src = R"JS(
(function() {
    if (typeof WebSocket !== 'undefined' && WebSocket.prototype) {
        WebSocket.prototype.addEventListener = function(type, handler) {
            if (type === 'open') this.onopen = handler;
            else if (type === 'message') this.onmessage = handler;
            else if (type === 'close') this.onclose = handler;
            else if (type === 'error') this.onerror = handler;
        };
        WebSocket.prototype.removeEventListener = function(type) {
            if (type === 'open') this.onopen = null;
            else if (type === 'message') this.onmessage = null;
            else if (type === 'close') this.onclose = null;
            else if (type === 'error') this.onerror = null;
        };
        Object.defineProperty(WebSocket.prototype, 'binaryType', {
            get: function() { return this._binaryType || 'blob'; },
            set: function(v) { this._binaryType = v; },
            configurable: true
        });
    }
})();
)JS";
        JSValue ws_ext_ret = JS_Eval(ctx, ws_ext_src, std::strlen(ws_ext_src),
                                      "<ws-extensions>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(ws_ext_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, ws_ext_ret);
    }

    // ------------------------------------------------------------------
    // Response.body (ReadableStream stub)
    // ------------------------------------------------------------------
    {
        const char* body_stub_src = R"JS(
        (function() {
            if (typeof Response !== 'undefined' && Response.prototype) {
                Object.defineProperty(Response.prototype, 'body', {
                    get: function() {
                        return {
                            locked: false,
                            cancel: function() { return Promise.resolve(); },
                            getReader: function() {
                                return {
                                    read: function() {
                                        return Promise.resolve({ done: true, value: undefined });
                                    },
                                    cancel: function() { return Promise.resolve(); },
                                    releaseLock: function() {},
                                    closed: Promise.resolve()
                                };
                            },
                            pipeThrough: function(transform) { return transform.readable; },
                            pipeTo: function(dest) { return Promise.resolve(); },
                            tee: function() { return [this, this]; }
                        };
                    },
                    configurable: true
                });
            }
        })();
)JS";
        JSValue bd_ret = JS_Eval(ctx, body_stub_src, std::strlen(body_stub_src),
                                  "<response-body>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(bd_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, bd_ret);
    }

    // ------------------------------------------------------------------
    // XMLHttpRequest.responseXML (returns null) and .upload (stub object)
    // ------------------------------------------------------------------
    {
        const char* xhr_ext_src = R"JS(
(function() {
    if (typeof XMLHttpRequest !== 'undefined' && XMLHttpRequest.prototype) {
        Object.defineProperty(XMLHttpRequest.prototype, 'responseXML', {
            get: function() { return null; },
            configurable: true
        });
        Object.defineProperty(XMLHttpRequest.prototype, 'upload', {
            get: function() {
                return {
                    addEventListener: function() {},
                    removeEventListener: function() {},
                    onprogress: null,
                    onload: null,
                    onerror: null,
                    onabort: null,
                    ontimeout: null,
                    onloadstart: null,
                    onloadend: null
                };
            },
            configurable: true
        });
    }
})();
)JS";
        JSValue xhr_ext_ret = JS_Eval(ctx, xhr_ext_src, std::strlen(xhr_ext_src),
                                       "<xhr-extensions>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(xhr_ext_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, xhr_ext_ret);
    }
}

void flush_fetch_promise_jobs(JSContext* ctx) {
    flush_promise_jobs(ctx);
}

} // namespace clever::js
