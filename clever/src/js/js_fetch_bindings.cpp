#include <clever/js/js_fetch_bindings.h>
#include <clever/js/cors_policy.h>

extern "C" {
#include <quickjs.h>
}

#include <clever/net/http_client.h>
#include <clever/net/cookie_jar.h>
#include <clever/net/request.h>
#include <clever/net/response.h>
#include <clever/net/tls_socket.h>

#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <netdb.h>
#include <optional>
#include <poll.h>
#include <random>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace clever::js {

namespace {

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

    if (cors::should_attach_origin_header(document_origin, state->url) &&
        !req.headers.has("origin")) {
        req.headers.set("Origin", document_origin);
    }

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

static JSValue js_xhr_abort(JSContext* ctx, JSValueConst this_val,
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

        // body
        JSValue body_val = JS_GetPropertyStr(ctx, argv[1], "body");
        if (JS_IsString(body_val)) {
            const char* b = JS_ToCString(ctx, body_val);
            if (b) {
                body = b;
                JS_FreeCString(ctx, b);
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

    if (cors::should_attach_origin_header(document_origin, url_str) &&
        !req.headers.has("origin")) {
        req.headers.set("Origin", document_origin);
    }

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
    int readyState = 0; // 0=CONNECTING, 1=OPEN, 2=CLOSING, 3=CLOSED
    std::string protocol;
    int socket_fd = -1;
    bool is_tls = false;
    // Event handlers (stored as JSValue, must be freed)
    JSValue onopen = JS_UNDEFINED;
    JSValue onmessage = JS_UNDEFINED;
    JSValue onclose = JS_UNDEFINED;
    JSValue onerror = JS_UNDEFINED;
    // Buffered messages
    std::vector<std::string> message_queue;
    // TLS socket (heap-allocated so we can null-check)
    clever::net::TlsSocket* tls = nullptr;
};

// ---- Helpers ----

static WebSocketState* get_ws_state(JSValueConst this_val) {
    return static_cast<WebSocketState*>(JS_GetOpaque(this_val, ws_class_id));
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
    if (state->is_tls && state->tls) {
        return state->tls->send(data, len);
    } else if (state->socket_fd >= 0) {
        size_t sent = 0;
        while (sent < len) {
            ssize_t n = ::send(state->socket_fd, data + sent, len - sent, 0);
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
    if (state->socket_fd < 0) return buffer;

    struct pollfd pfd {};
    pfd.fd = state->socket_fd;
    pfd.events = POLLIN;

    int poll_rv = ::poll(&pfd, 1, timeout_ms);
    if (poll_rv <= 0) return buffer;

    if (state->is_tls && state->tls) {
        auto chunk = state->tls->recv();
        if (chunk.has_value() && !chunk->empty()) {
            buffer = std::move(*chunk);
        }
    } else {
        uint8_t tmp[8192];
        ssize_t n = ::recv(state->socket_fd, tmp, sizeof(tmp), 0);
        if (n > 0) {
            buffer.assign(tmp, tmp + n);
        }
    }
    return buffer;
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

// ---- WebSocket JS methods ----

// ws.send(data)
static JSValue js_ws_send(JSContext* ctx, JSValueConst this_val,
                           int argc, JSValueConst* argv) {
    auto* state = get_ws_state(this_val);
    if (!state) return JS_ThrowTypeError(ctx, "Invalid WebSocket object");

    if (state->readyState != 1) { // Not OPEN
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
        // Fire onerror
        if (JS_IsFunction(ctx, state->onerror)) {
            JSValue event = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, event, "type",
                JS_NewString(ctx, "error"));
            JSValue ret = JS_Call(ctx, state->onerror, JS_UNDEFINED, 1, &event);
            JS_FreeValue(ctx, ret);
            JS_FreeValue(ctx, event);
        }
        return JS_ThrowTypeError(ctx, "WebSocket send failed");
    }

    return JS_UNDEFINED;
}

// ws.close([code [, reason]])
static JSValue js_ws_close(JSContext* ctx, JSValueConst this_val,
                            int argc, JSValueConst* argv) {
    auto* state = get_ws_state(this_val);
    if (!state) return JS_ThrowTypeError(ctx, "Invalid WebSocket object");

    if (state->readyState == 2 || state->readyState == 3) {
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

    state->readyState = 2; // CLOSING

    // Send close frame (best effort)
    if (state->socket_fd >= 0) {
        auto frame = ws_build_close_frame(code, reason);
        ws_send_raw(state, frame.data(), frame.size());
    }

    // Close the socket
    if (state->tls) {
        state->tls->close();
        delete state->tls;
        state->tls = nullptr;
    }
    if (state->socket_fd >= 0) {
        ::close(state->socket_fd);
        state->socket_fd = -1;
    }

    state->readyState = 3; // CLOSED

    // Fire onclose
    if (JS_IsFunction(ctx, state->onclose)) {
        JSValue event = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, event, "type", JS_NewString(ctx, "close"));
        JS_SetPropertyStr(ctx, event, "code", JS_NewInt32(ctx, code));
        JS_SetPropertyStr(ctx, event, "reason",
            JS_NewString(ctx, reason.c_str()));
        JS_SetPropertyStr(ctx, event, "wasClean", JS_TRUE);
        JSValue ret = JS_Call(ctx, state->onclose, JS_UNDEFINED, 1, &event);
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, event);
    }

    return JS_UNDEFINED;
}

// ---- Property getters ----

static JSValue js_ws_get_readyState(JSContext* ctx, JSValueConst this_val) {
    auto* state = get_ws_state(this_val);
    if (!state) return JS_NewInt32(ctx, 3); // CLOSED
    return JS_NewInt32(ctx, state->readyState);
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
    return JS_DupValue(ctx, state->onopen);
}

static JSValue js_ws_set_onopen(JSContext* ctx, JSValueConst this_val, JSValueConst val) {
    auto* state = get_ws_state(this_val);
    if (!state) return JS_EXCEPTION;
    JS_FreeValue(ctx, state->onopen);
    state->onopen = JS_DupValue(ctx, val);
    return JS_UNDEFINED;
}

static JSValue js_ws_get_onmessage(JSContext* ctx, JSValueConst this_val) {
    auto* state = get_ws_state(this_val);
    if (!state) return JS_NULL;
    return JS_DupValue(ctx, state->onmessage);
}

static JSValue js_ws_set_onmessage(JSContext* ctx, JSValueConst this_val, JSValueConst val) {
    auto* state = get_ws_state(this_val);
    if (!state) return JS_EXCEPTION;
    JS_FreeValue(ctx, state->onmessage);
    state->onmessage = JS_DupValue(ctx, val);
    return JS_UNDEFINED;
}

static JSValue js_ws_get_onclose(JSContext* ctx, JSValueConst this_val) {
    auto* state = get_ws_state(this_val);
    if (!state) return JS_NULL;
    return JS_DupValue(ctx, state->onclose);
}

static JSValue js_ws_set_onclose(JSContext* ctx, JSValueConst this_val, JSValueConst val) {
    auto* state = get_ws_state(this_val);
    if (!state) return JS_EXCEPTION;
    JS_FreeValue(ctx, state->onclose);
    state->onclose = JS_DupValue(ctx, val);
    return JS_UNDEFINED;
}

static JSValue js_ws_get_onerror(JSContext* ctx, JSValueConst this_val) {
    auto* state = get_ws_state(this_val);
    if (!state) return JS_NULL;
    return JS_DupValue(ctx, state->onerror);
}

static JSValue js_ws_set_onerror(JSContext* ctx, JSValueConst this_val, JSValueConst val) {
    auto* state = get_ws_state(this_val);
    if (!state) return JS_EXCEPTION;
    JS_FreeValue(ctx, state->onerror);
    state->onerror = JS_DupValue(ctx, val);
    return JS_UNDEFINED;
}

// ---- Finalizer ----

static void js_ws_finalizer(JSRuntime* rt, JSValue val) {
    auto* state = static_cast<WebSocketState*>(JS_GetOpaque(val, ws_class_id));
    if (!state) return;

    // Free event handler JSValues
    JS_FreeValueRT(rt, state->onopen);
    JS_FreeValueRT(rt, state->onmessage);
    JS_FreeValueRT(rt, state->onclose);
    JS_FreeValueRT(rt, state->onerror);

    // Clean up socket resources
    if (state->tls) {
        state->tls->close();
        delete state->tls;
    }
    if (state->socket_fd >= 0) {
        ::close(state->socket_fd);
    }

    delete state;
}

// GC mark callback — tell QuickJS about our stored JSValue event handlers
static void js_ws_gc_mark(JSRuntime* rt, JSValueConst val,
                           JS_MarkFunc* mark_func) {
    auto* state = static_cast<WebSocketState*>(JS_GetOpaque(val, ws_class_id));
    if (!state) return;
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
    JS_SetOpaque(obj, state);

    // TCP connect
    int fd = ws_connect_to(host, port);
    if (fd < 0) {
        state->readyState = 3; // CLOSED
        // Fire onerror event
        if (JS_IsFunction(ctx, state->onerror)) {
            JSValue event = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, event, "type",
                JS_NewString(ctx, "error"));
            JSValue ret = JS_Call(ctx, state->onerror, JS_UNDEFINED, 1, &event);
            JS_FreeValue(ctx, ret);
            JS_FreeValue(ctx, event);
        }
        return obj; // Return the object in CLOSED state
    }

    state->socket_fd = fd;

    // TLS handshake if needed
    if (use_tls) {
        state->tls = new clever::net::TlsSocket();
        if (!state->tls->connect(host, port, fd)) {
            delete state->tls;
            state->tls = nullptr;
            ::close(fd);
            state->socket_fd = -1;
            state->readyState = 3; // CLOSED
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
        if (state->tls) {
            state->tls->close();
            delete state->tls;
            state->tls = nullptr;
        }
        ::close(fd);
        state->socket_fd = -1;
        state->readyState = 3;
        return obj;
    }

    // Read upgrade response
    auto response_data = ws_recv_raw(state, 10000);
    if (response_data.empty()) {
        if (state->tls) {
            state->tls->close();
            delete state->tls;
            state->tls = nullptr;
        }
        ::close(fd);
        state->socket_fd = -1;
        state->readyState = 3;
        return obj;
    }

    // Verify 101 Switching Protocols
    std::string response_str(response_data.begin(), response_data.end());
    if (response_str.find("101") == std::string::npos) {
        if (state->tls) {
            state->tls->close();
            delete state->tls;
            state->tls = nullptr;
        }
        ::close(fd);
        state->socket_fd = -1;
        state->readyState = 3;
        return obj;
    }

    // Connection established!
    state->readyState = 1; // OPEN

    // Fire onopen event immediately (synchronous engine)
    if (JS_IsFunction(ctx, state->onopen)) {
        JSValue event = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, event, "type", JS_NewString(ctx, "open"));
        JSValue ret = JS_Call(ctx, state->onopen, JS_UNDEFINED, 1, &event);
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, event);
    }

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

static JSClassID formdata_class_id = 0;

struct FormDataState {
    std::vector<std::pair<std::string, std::string>> entries;
};

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

// Constructor: new FormData()
static JSValue js_formdata_constructor(JSContext* ctx, JSValueConst new_target,
                                         int /*argc*/, JSValueConst* /*argv*/) {
    JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto)) return JS_EXCEPTION;

    JSValue obj = JS_NewObjectProtoClass(ctx, proto, formdata_class_id);
    JS_FreeValue(ctx, proto);
    if (JS_IsException(obj)) return obj;

    auto* state = new FormDataState();
    JS_SetOpaque(obj, state);
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
    // Response.formData() stub — returns Promise resolving to new FormData()
    // ------------------------------------------------------------------
    {
        const char* formdata_stub_src = R"JS(
        (function() {
            if (typeof Response !== 'undefined' && Response.prototype) {
                Response.prototype.formData = function() {
                    return Promise.resolve(new FormData());
                };
            }
        })();
)JS";
        JSValue fd_ret = JS_Eval(ctx, formdata_stub_src, std::strlen(formdata_stub_src),
                                  "<response-formdata>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(fd_ret)) {
            JSValue exc = JS_GetException(ctx);
            JS_FreeValue(ctx, exc);
        }
        JS_FreeValue(ctx, fd_ret);
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
