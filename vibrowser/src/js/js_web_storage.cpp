#include <clever/js/js_web_storage.h>

extern "C" {
#include <quickjs.h>
}

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <sstream>
#include <unistd.h>

namespace clever::js {

namespace {

// =========================================================================
// Storage state management
// =========================================================================

struct StorageState {
    std::map<std::string, std::string> data;
    std::string origin;
    bool is_local_storage = false;
};

// Per-context session storage (cleared when context is destroyed)
static std::map<uintptr_t, std::map<std::string, std::string>> session_storage;

// =========================================================================
// File I/O helpers for localStorage persistence
// =========================================================================

static std::string get_storage_dir() {
    const char* home = std::getenv("HOME");
    if (!home) home = "/tmp";
    std::string dir = std::string(home) + "/.vibrowser/storage";
    return dir;
}

static std::string sanitize_origin(const std::string& origin) {
    std::string result = origin;
    for (auto& c : result) {
        if (c == '/' || c == ':' || c == '?') {
            c = '_';
        }
    }
    return result;
}

static std::string get_storage_file(const std::string& origin) {
    std::string dir = get_storage_dir();
    return dir + "/" + sanitize_origin(origin) + ".json";
}

static void ensure_storage_dir() {
    std::string dir = get_storage_dir();
    std::system(("mkdir -p '" + dir + "'").c_str());
}

static std::string quote_json_string(const std::string& str) {
    std::string result = "\"";
    for (unsigned char c : str) {
        if (c == '"') result += "\\\"";
        else if (c == '\\') result += "\\\\";
        else if (c == '\n') result += "\\n";
        else if (c == '\r') result += "\\r";
        else if (c == '\t') result += "\\t";
        else if (c >= 32 && c < 127) result += c;
        else {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\u%04x", c);
            result += buf;
        }
    }
    result += "\"";
    return result;
}

static bool load_storage_from_file(const std::string& origin,
                                   std::map<std::string, std::string>& data) {
    std::string filepath = get_storage_file(origin);
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    // Simple JSON parser for {"key":"value",...}
    data.clear();
    size_t pos = 0;
    while (pos < content.size()) {
        // Find next quote (key start)
        pos = content.find('"', pos);
        if (pos == std::string::npos) break;
        pos++;

        // Find closing quote (key end)
        size_t key_end = content.find('"', pos);
        if (key_end == std::string::npos) break;

        std::string key = content.substr(pos, key_end - pos);
        pos = key_end + 1;

        // Find colon
        pos = content.find(':', pos);
        if (pos == std::string::npos) break;
        pos++;

        // Find opening quote for value
        pos = content.find('"', pos);
        if (pos == std::string::npos) break;
        pos++;

        // Find closing quote for value (handling escaped quotes)
        std::string value;
        while (pos < content.size()) {
            if (content[pos] == '\\' && pos + 1 < content.size()) {
                pos++;
                if (content[pos] == 'n') value += '\n';
                else if (content[pos] == 'r') value += '\r';
                else if (content[pos] == 't') value += '\t';
                else if (content[pos] == '"') value += '"';
                else if (content[pos] == '\\') value += '\\';
                else value += content[pos];
                pos++;
            } else if (content[pos] == '"') {
                break;
            } else {
                value += content[pos];
                pos++;
            }
        }

        data[key] = value;
        pos++;

        // Find comma or end
        pos = content.find(',', pos);
        if (pos == std::string::npos) break;
        pos++;
    }

    return true;
}

static void save_storage_to_file(const std::string& origin,
                                 const std::map<std::string, std::string>& data) {
    ensure_storage_dir();
    std::string filepath = get_storage_file(origin);

    std::string json = "{";
    bool first = true;
    for (const auto& [key, value] : data) {
        if (!first) json += ",";
        json += quote_json_string(key) + ":" + quote_json_string(value);
        first = false;
    }
    json += "}";

    std::ofstream file(filepath);
    file << json;
    file.close();
}

// =========================================================================
// Storage object methods
// =========================================================================

static JSValue storage_get_item(JSContext* ctx, JSValueConst this_val,
                                int argc, JSValueConst* argv) {
    auto* state = static_cast<StorageState*>(JS_GetOpaque(this_val, 0));
    if (!state) {
        return JS_NULL;
    }

    if (argc < 1) {
        return JS_NULL;
    }

    const char* key = JS_ToCString(ctx, argv[0]);
    if (!key) {
        return JS_NULL;
    }

    std::string key_str(key);
    JS_FreeCString(ctx, key);

    auto it = state->data.find(key_str);
    if (it == state->data.end()) {
        return JS_NULL;
    }

    return JS_NewString(ctx, it->second.c_str());
}

static JSValue storage_set_item(JSContext* ctx, JSValueConst this_val,
                                int argc, JSValueConst* argv) {
    auto* state = static_cast<StorageState*>(JS_GetOpaque(this_val, 0));
    if (!state) {
        return JS_UNDEFINED;
    }

    if (argc < 2) {
        return JS_UNDEFINED;
    }

    const char* key_cstr = JS_ToCString(ctx, argv[0]);
    const char* value_cstr = JS_ToCString(ctx, argv[1]);

    if (!key_cstr || !value_cstr) {
        if (key_cstr) JS_FreeCString(ctx, key_cstr);
        if (value_cstr) JS_FreeCString(ctx, value_cstr);
        return JS_UNDEFINED;
    }

    std::string key(key_cstr);
    std::string value(value_cstr);
    JS_FreeCString(ctx, key_cstr);
    JS_FreeCString(ctx, value_cstr);

    state->data[key] = value;

    if (state->is_local_storage) {
        save_storage_to_file(state->origin, state->data);
    }

    return JS_UNDEFINED;
}

static JSValue storage_remove_item(JSContext* ctx, JSValueConst this_val,
                                   int argc, JSValueConst* argv) {
    auto* state = static_cast<StorageState*>(JS_GetOpaque(this_val, 0));
    if (!state) {
        return JS_UNDEFINED;
    }

    if (argc < 1) {
        return JS_UNDEFINED;
    }

    const char* key = JS_ToCString(ctx, argv[0]);
    if (!key) {
        return JS_UNDEFINED;
    }

    std::string key_str(key);
    JS_FreeCString(ctx, key);

    state->data.erase(key_str);

    if (state->is_local_storage) {
        save_storage_to_file(state->origin, state->data);
    }

    return JS_UNDEFINED;
}

static JSValue storage_clear(JSContext* ctx, JSValueConst this_val,
                             int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    (void)ctx;

    auto* state = static_cast<StorageState*>(JS_GetOpaque(this_val, 0));
    if (!state) {
        return JS_UNDEFINED;
    }

    state->data.clear();

    if (state->is_local_storage) {
        save_storage_to_file(state->origin, state->data);
    }

    return JS_UNDEFINED;
}

static JSValue storage_key(JSContext* ctx, JSValueConst this_val,
                           int argc, JSValueConst* argv) {
    auto* state = static_cast<StorageState*>(JS_GetOpaque(this_val, 0));
    if (!state) {
        return JS_NULL;
    }

    if (argc < 1) {
        return JS_NULL;
    }

    int32_t index;
    if (JS_ToInt32(ctx, &index, argv[0]) < 0) {
        return JS_NULL;
    }

    if (index < 0 || index >= static_cast<int32_t>(state->data.size())) {
        return JS_NULL;
    }

    auto it = state->data.begin();
    std::advance(it, index);
    return JS_NewString(ctx, it->first.c_str());
}

static JSValue storage_get_length(JSContext* ctx, JSValueConst this_val,
                                  int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;

    auto* state = static_cast<StorageState*>(JS_GetOpaque(this_val, 0));
    if (!state) {
        return JS_NewInt32(ctx, 0);
    }

    return JS_NewInt32(ctx, static_cast<int32_t>(state->data.size()));
}

static void storage_finalizer(JSRuntime* /*rt*/, JSValue val) {
    auto* state = static_cast<StorageState*>(JS_GetOpaque(val, 0));
    if (state) delete state;
}

// =========================================================================
// Helper to create a storage object
// =========================================================================

static JSValue create_storage_object(JSContext* ctx, const std::string& origin,
                                     bool is_local_storage) {
    JSValue storage = JS_NewObject(ctx);

    auto* state = new StorageState();
    state->origin = origin;
    state->is_local_storage = is_local_storage;

    if (is_local_storage) {
        // Try to load from disk
        load_storage_from_file(origin, state->data);
    }

    // Store opaque pointer (simplified: using class ID 0 since we don't register a class)
    JS_SetOpaque(storage, state);

    // Add methods
    JS_SetPropertyStr(ctx, storage, "getItem",
        JS_NewCFunction(ctx, storage_get_item, "getItem", 1));
    JS_SetPropertyStr(ctx, storage, "setItem",
        JS_NewCFunction(ctx, storage_set_item, "setItem", 2));
    JS_SetPropertyStr(ctx, storage, "removeItem",
        JS_NewCFunction(ctx, storage_remove_item, "removeItem", 1));
    JS_SetPropertyStr(ctx, storage, "clear",
        JS_NewCFunction(ctx, storage_clear, "clear", 0));
    JS_SetPropertyStr(ctx, storage, "key",
        JS_NewCFunction(ctx, storage_key, "key", 1));

    // Add length as a getter-only property
    {
        JSAtom length_atom = JS_NewAtom(ctx, "length");
        JSValue length_getter = JS_NewCFunction(ctx, storage_get_length, "get length", 0);
        JS_DefinePropertyGetSet(ctx, storage, length_atom, length_getter, JS_UNDEFINED,
                               JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, length_atom);
    }

    return storage;
}

} // namespace

// =========================================================================
// Public API
// =========================================================================

void install_web_storage_bindings(JSContext* ctx, const std::string& origin) {
    JSValue global = JS_GetGlobalObject(ctx);

    // Create localStorage (persisted)
    JSValue local_storage = create_storage_object(ctx, origin, true);
    JS_SetPropertyStr(ctx, global, "localStorage", local_storage);

    // Create sessionStorage (in-memory)
    JSValue session_storage = create_storage_object(ctx, origin, false);
    JS_SetPropertyStr(ctx, global, "sessionStorage", session_storage);

    JS_FreeValue(ctx, global);
}

} // namespace clever::js
