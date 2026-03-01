#include <clever/js/js_indexed_db.h>

extern "C" {
#include <quickjs.h>
}

#include <cstdint>
#include <string>

namespace clever::js {

namespace {

struct IDBFactoryState {};

struct IDBOpenDBRequestState {
    std::string db_name;
    int32_t db_version = 1;
};

struct IDBRequestState {};
struct IDBDeleteDBRequestState {};

struct IDBDatabaseState {
    std::string name;
    int32_t version = 1;
};

struct IDBTransactionState {
    std::string mode = "readonly";
};

struct IDBObjectStoreState {
    std::string name;
};

static JSClassID idb_factory_class_id = 0;
static JSClassID idb_open_request_class_id = 0;
static JSClassID idb_request_class_id = 0;
static JSClassID idb_delete_request_class_id = 0;
static JSClassID idb_database_class_id = 0;
static JSClassID idb_transaction_class_id = 0;
static JSClassID idb_object_store_class_id = 0;

static void define_readonly_property(JSContext* ctx, JSValue obj,
                                    const char* name, JSValue getter) {
    JSAtom atom = JS_NewAtom(ctx, name);
    JS_DefinePropertyGetSet(ctx, obj, atom, getter, JS_UNDEFINED,
                           JS_PROP_ENUMERABLE);
    JS_FreeAtom(ctx, atom);
}

static void invoke_request_handler(JSContext* ctx, JSValue target,
                                  const char* property_name, JSValue event) {
    JSValue handler = JS_GetPropertyStr(ctx, target, property_name);
    if (JS_IsFunction(ctx, handler)) {
        JSValue result = JS_Call(ctx, handler, target, 1, &event);
        if (JS_IsException(result)) {
            JS_FreeValue(ctx, JS_GetException(ctx));
        } else {
            JS_FreeValue(ctx, result);
        }
    }
    JS_FreeValue(ctx, handler);
}

static JSValue make_event(JSContext* ctx, const char* type, JSValue target,
                          JSValue result) {
    JSValue event = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, event, "type", JS_NewString(ctx, type));
    JS_SetPropertyStr(ctx, event, "target", JS_DupValue(ctx, target));
    JS_SetPropertyStr(ctx, event, "currentTarget", JS_DupValue(ctx, target));
    if (!JS_IsUndefined(result)) {
        JS_SetPropertyStr(ctx, event, "result", JS_DupValue(ctx, result));
    }
    return event;
}

static JSValue create_idb_database_object(JSContext* ctx, const std::string& name,
                                          int32_t version);
static JSValue create_idb_transaction_object(JSContext* ctx,
                                            const std::string& mode);
static JSValue create_idb_object_store_object(JSContext* ctx,
                                             const std::string& store_name);
static JSValue create_idb_request_object(JSContext* ctx);
static JSValue create_idb_delete_request_object(JSContext* ctx);
static JSValue create_idb_open_request_object(JSContext* ctx, const std::string& name,
                                             int32_t version);

static void idb_factory_finalizer(JSRuntime* /*rt*/, JSValue val) {
    auto* state = static_cast<IDBFactoryState*>(JS_GetOpaque(val, idb_factory_class_id));
    if (state) {
        delete state;
    }
}

static void idb_open_request_finalizer(JSRuntime* /*rt*/, JSValue val) {
    auto* state = static_cast<IDBOpenDBRequestState*>(JS_GetOpaque(val, idb_open_request_class_id));
    if (state) {
        delete state;
    }
}

static void idb_request_finalizer(JSRuntime* /*rt*/, JSValue val) {
    auto* state = static_cast<IDBRequestState*>(JS_GetOpaque(val, idb_request_class_id));
    if (state) {
        delete state;
    }
}

static void idb_delete_request_finalizer(JSRuntime* /*rt*/, JSValue val) {
    auto* state = static_cast<IDBDeleteDBRequestState*>(
        JS_GetOpaque(val, idb_delete_request_class_id));
    if (state) {
        delete state;
    }
}

static void idb_database_finalizer(JSRuntime* /*rt*/, JSValue val) {
    auto* state = static_cast<IDBDatabaseState*>(JS_GetOpaque(val, idb_database_class_id));
    if (state) {
        delete state;
    }
}

static void idb_transaction_finalizer(JSRuntime* /*rt*/, JSValue val) {
    auto* state = static_cast<IDBTransactionState*>(JS_GetOpaque(val, idb_transaction_class_id));
    if (state) {
        delete state;
    }
}

static void idb_object_store_finalizer(JSRuntime* /*rt*/, JSValue val) {
    auto* state = static_cast<IDBObjectStoreState*>(JS_GetOpaque(val, idb_object_store_class_id));
    if (state) {
        delete state;
    }
}

static JSClassDef idb_factory_class_def = {
    "IDBFactory", idb_factory_finalizer, nullptr, nullptr, nullptr,
};

static JSClassDef idb_open_request_class_def = {
    "IDBOpenDBRequest", idb_open_request_finalizer, nullptr, nullptr, nullptr,
};

static JSClassDef idb_request_class_def = {
    "IDBRequest", idb_request_finalizer, nullptr, nullptr, nullptr,
};

static JSClassDef idb_delete_request_class_def = {
    "IDBDeleteDBRequest", idb_delete_request_finalizer, nullptr, nullptr, nullptr,
};

static JSClassDef idb_database_class_def = {
    "IDBDatabase", idb_database_finalizer, nullptr, nullptr, nullptr,
};

static JSClassDef idb_transaction_class_def = {
    "IDBTransaction", idb_transaction_finalizer, nullptr, nullptr, nullptr,
};

static JSClassDef idb_object_store_class_def = {
    "IDBObjectStore", idb_object_store_finalizer, nullptr, nullptr, nullptr,
};

static void ensure_indexed_db_classes(JSContext* ctx) {
    if (idb_factory_class_id == 0) {
        JS_NewClassID(&idb_factory_class_id);
        JS_NewClass(JS_GetRuntime(ctx), idb_factory_class_id, &idb_factory_class_def);
    }
    if (idb_open_request_class_id == 0) {
        JS_NewClassID(&idb_open_request_class_id);
        JS_NewClass(JS_GetRuntime(ctx), idb_open_request_class_id, &idb_open_request_class_def);
    }
    if (idb_request_class_id == 0) {
        JS_NewClassID(&idb_request_class_id);
        JS_NewClass(JS_GetRuntime(ctx), idb_request_class_id, &idb_request_class_def);
    }
    if (idb_delete_request_class_id == 0) {
        JS_NewClassID(&idb_delete_request_class_id);
        JS_NewClass(JS_GetRuntime(ctx), idb_delete_request_class_id,
                    &idb_delete_request_class_def);
    }
    if (idb_database_class_id == 0) {
        JS_NewClassID(&idb_database_class_id);
        JS_NewClass(JS_GetRuntime(ctx), idb_database_class_id, &idb_database_class_def);
    }
    if (idb_transaction_class_id == 0) {
        JS_NewClassID(&idb_transaction_class_id);
        JS_NewClass(JS_GetRuntime(ctx), idb_transaction_class_id, &idb_transaction_class_def);
    }
    if (idb_object_store_class_id == 0) {
        JS_NewClassID(&idb_object_store_class_id);
        JS_NewClass(JS_GetRuntime(ctx), idb_object_store_class_id, &idb_object_store_class_def);
    }
}

static JSValue idb_object_store_put(JSContext* ctx, JSValueConst this_val,
                                   int argc, JSValueConst* argv) {
    (void)this_val;
    (void)argc;
    (void)argv;
    JSValue request = create_idb_request_object(ctx);
    JSValue success_event = make_event(ctx, "success", request, JS_UNDEFINED);
    invoke_request_handler(ctx, request, "onsuccess", success_event);
    JS_FreeValue(ctx, success_event);
    return request;
}

static JSValue idb_object_store_get(JSContext* ctx, JSValueConst this_val,
                                   int argc, JSValueConst* argv) {
    (void)this_val;
    (void)argc;
    (void)argv;
    JSValue request = create_idb_request_object(ctx);
    JSValue success_event = make_event(ctx, "success", request, JS_UNDEFINED);
    invoke_request_handler(ctx, request, "onsuccess", success_event);
    JS_FreeValue(ctx, success_event);
    return request;
}

static JSValue idb_object_store_delete(JSContext* ctx, JSValueConst this_val,
                                      int argc, JSValueConst* argv) {
    (void)this_val;
    (void)argc;
    (void)argv;
    JSValue request = create_idb_request_object(ctx);
    JSValue success_event = make_event(ctx, "success", request, JS_UNDEFINED);
    invoke_request_handler(ctx, request, "onsuccess", success_event);
    JS_FreeValue(ctx, success_event);
    return request;
}

static JSValue idb_object_store_get_all(JSContext* ctx, JSValueConst this_val,
                                       int argc, JSValueConst* argv) {
    (void)this_val;
    (void)argc;
    (void)argv;
    JSValue request = create_idb_request_object(ctx);
    JSValue success_event = make_event(ctx, "success", request, JS_UNDEFINED);
    invoke_request_handler(ctx, request, "onsuccess", success_event);
    JS_FreeValue(ctx, success_event);
    return request;
}

static JSValue idb_object_store_clear(JSContext* ctx, JSValueConst this_val,
                                     int argc, JSValueConst* argv) {
    (void)this_val;
    (void)argc;
    (void)argv;
    JSValue request = create_idb_request_object(ctx);
    JSValue success_event = make_event(ctx, "success", request, JS_UNDEFINED);
    invoke_request_handler(ctx, request, "onsuccess", success_event);
    JS_FreeValue(ctx, success_event);
    return request;
}

static JSValue idb_transaction_object_store(JSContext* ctx, JSValueConst this_val,
                                           int argc, JSValueConst* argv) {
    (void)this_val;
    const char* store = nullptr;
    bool has_store = false;
    if (argc >= 1) {
        store = JS_ToCString(ctx, argv[0]);
        if (store) {
            has_store = true;
        }
    }

    std::string store_name = store ? std::string(store) : std::string();
    if (has_store) {
        JS_FreeCString(ctx, store);
    }
    return create_idb_object_store_object(ctx, store_name);
}

static JSValue idb_transaction_get_mode(JSContext* ctx, JSValueConst this_val,
                                       int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    auto* state = static_cast<IDBTransactionState*>(
        JS_GetOpaque(this_val, idb_transaction_class_id));
    if (!state) {
        return JS_NewString(ctx, "readonly");
    }
    return JS_NewString(ctx, state->mode.c_str());
}

static JSValue idb_database_transaction(JSContext* ctx, JSValueConst this_val,
                                       int argc, JSValueConst* argv) {
    (void)this_val;
    (void)argc;
    (void)argv;
    auto* state = static_cast<IDBDatabaseState*>(
        JS_GetOpaque(this_val, idb_database_class_id));
    if (!state) {
        return JS_UNDEFINED;
    }

    std::string mode = "readonly";
    if (argc >= 2) {
        const char* mode_cstr = JS_ToCString(ctx, argv[1]);
        if (mode_cstr) {
            mode = mode_cstr;
            JS_FreeCString(ctx, mode_cstr);
        }
    }
    return create_idb_transaction_object(ctx, mode);
}

static JSValue idb_database_get_name(JSContext* ctx, JSValueConst this_val,
                                    int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    auto* state = static_cast<IDBDatabaseState*>(
        JS_GetOpaque(this_val, idb_database_class_id));
    if (!state) {
        return JS_NewString(ctx, "");
    }
    return JS_NewString(ctx, state->name.c_str());
}

static JSValue idb_database_get_version(JSContext* ctx, JSValueConst this_val,
                                       int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    auto* state = static_cast<IDBDatabaseState*>(
        JS_GetOpaque(this_val, idb_database_class_id));
    if (!state) {
        return JS_NewInt32(ctx, 1);
    }
    return JS_NewInt32(ctx, state->version);
}

static JSValue idb_open_request_result(JSContext* ctx, JSValueConst this_val,
                                      int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    auto* state = static_cast<IDBOpenDBRequestState*>(
        JS_GetOpaque(this_val, idb_open_request_class_id));
    if (!state) {
        return JS_UNDEFINED;
    }
    return create_idb_database_object(ctx, state->db_name, state->db_version);
}

static JSValue idb_request_result(JSContext* ctx, JSValueConst this_val,
                                 int argc, JSValueConst* argv) {
    (void)ctx;
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_UNDEFINED;
}

static JSValue idb_factory_open(JSContext* ctx, JSValueConst this_val,
                               int argc, JSValueConst* argv) {
    (void)this_val;
    const char* name_cstr = nullptr;
    if (argc >= 1) {
        name_cstr = JS_ToCString(ctx, argv[0]);
    }

    std::string name = name_cstr ? std::string(name_cstr) : "";
    if (name_cstr) {
        JS_FreeCString(ctx, name_cstr);
    }

    int32_t version = 1;
    if (argc >= 2) {
        if (JS_ToInt32(ctx, &version, argv[1]) < 0) {
            version = 1;
        }
    }
    if (version < 1) {
        version = 1;
    }

    JSValue request = create_idb_open_request_object(ctx, name, version);
    JSValue db = create_idb_database_object(ctx, name, version);
    JSValue upgrade_event = make_event(ctx, "upgradeneeded", request, db);
    invoke_request_handler(ctx, request, "onupgradeneeded", upgrade_event);
    JS_FreeValue(ctx, upgrade_event);
    JSValue success_event = make_event(ctx, "success", request, db);
    invoke_request_handler(ctx, request, "onsuccess", success_event);
    JS_FreeValue(ctx, success_event);
    JS_FreeValue(ctx, db);
    return request;
}

static JSValue idb_factory_delete_database(JSContext* ctx, JSValueConst this_val,
                                          int argc, JSValueConst* argv) {
    (void)this_val;
    (void)argc;
    (void)argv;
    JSValue request = create_idb_delete_request_object(ctx);
    JSValue success_event = make_event(ctx, "success", request, JS_UNDEFINED);
    invoke_request_handler(ctx, request, "onsuccess", success_event);
    JS_FreeValue(ctx, success_event);
    return request;
}

static JSValue create_idb_open_request_object(JSContext* ctx, const std::string& name,
                                             int32_t version) {
    JSValue request = JS_NewObjectClass(ctx, idb_open_request_class_id);
    auto* state = new IDBOpenDBRequestState();
    state->db_name = name;
    state->db_version = version;
    JS_SetOpaque(request, state);

    JS_SetPropertyStr(ctx, request, "onsuccess", JS_NULL);
    JS_SetPropertyStr(ctx, request, "onerror", JS_NULL);
    JS_SetPropertyStr(ctx, request, "onupgradeneeded", JS_NULL);

    define_readonly_property(ctx, request, "result",
                            JS_NewCFunction(ctx, idb_open_request_result,
                                            "result", 0));
    return request;
}

static JSValue create_idb_request_object(JSContext* ctx) {
    JSValue request = JS_NewObjectClass(ctx, idb_request_class_id);
    auto* state = new IDBRequestState();
    JS_SetOpaque(request, state);
    JS_SetPropertyStr(ctx, request, "onsuccess", JS_NULL);
    JS_SetPropertyStr(ctx, request, "onerror", JS_NULL);
    define_readonly_property(ctx, request, "result",
                            JS_NewCFunction(ctx, idb_request_result, "result", 0));
    return request;
}

static JSValue create_idb_delete_request_object(JSContext* ctx) {
    JSValue request = JS_NewObjectClass(ctx, idb_delete_request_class_id);
    auto* state = new IDBDeleteDBRequestState();
    JS_SetOpaque(request, state);
    JS_SetPropertyStr(ctx, request, "onsuccess", JS_NULL);
    JS_SetPropertyStr(ctx, request, "onerror", JS_NULL);
    define_readonly_property(ctx, request, "result",
                            JS_NewCFunction(ctx, idb_request_result, "result", 0));
    return request;
}

static JSValue create_idb_database_object(JSContext* ctx, const std::string& name,
                                         int32_t version) {
    JSValue database = JS_NewObjectClass(ctx, idb_database_class_id);
    auto* state = new IDBDatabaseState();
    state->name = name;
    state->version = version;
    JS_SetOpaque(database, state);

    JS_SetPropertyStr(ctx, database, "transaction",
                      JS_NewCFunction(ctx, idb_database_transaction, "transaction", 2));
    define_readonly_property(ctx, database, "name",
                            JS_NewCFunction(ctx, idb_database_get_name, "name", 0));
    define_readonly_property(ctx, database, "version",
                            JS_NewCFunction(ctx, idb_database_get_version,
                                            "version", 0));
    return database;
}

static JSValue create_idb_transaction_object(JSContext* ctx,
                                            const std::string& mode) {
    JSValue transaction = JS_NewObjectClass(ctx, idb_transaction_class_id);
    auto* state = new IDBTransactionState();
    state->mode = mode.empty() ? "readonly" : mode;
    JS_SetOpaque(transaction, state);

    JS_SetPropertyStr(ctx, transaction, "objectStore",
                      JS_NewCFunction(ctx, idb_transaction_object_store, "objectStore", 1));
    define_readonly_property(ctx, transaction, "mode",
                            JS_NewCFunction(ctx, idb_transaction_get_mode,
                                            "mode", 0));
    return transaction;
}

static JSValue create_idb_object_store_object(JSContext* ctx,
                                             const std::string& store_name) {
    JSValue store = JS_NewObjectClass(ctx, idb_object_store_class_id);
    auto* state = new IDBObjectStoreState();
    state->name = store_name;
    JS_SetOpaque(store, state);

    JS_SetPropertyStr(ctx, store, "put",
                      JS_NewCFunction(ctx, idb_object_store_put, "put", 2));
    JS_SetPropertyStr(ctx, store, "get",
                      JS_NewCFunction(ctx, idb_object_store_get, "get", 1));
    JS_SetPropertyStr(ctx, store, "delete",
                      JS_NewCFunction(ctx, idb_object_store_delete, "delete", 1));
    JS_SetPropertyStr(ctx, store, "getAll",
                      JS_NewCFunction(ctx, idb_object_store_get_all, "getAll", 0));
    JS_SetPropertyStr(ctx, store, "clear",
                      JS_NewCFunction(ctx, idb_object_store_clear, "clear", 0));
    return store;
}

} // namespace

void install_indexed_db_bindings(JSContext* ctx) {
    ensure_indexed_db_classes(ctx);

    JSValue global = JS_GetGlobalObject(ctx);
    JSValue indexed_db = JS_NewObjectClass(ctx, idb_factory_class_id);
    auto* factory_state = new IDBFactoryState();
    JS_SetOpaque(indexed_db, factory_state);

    JS_SetPropertyStr(ctx, indexed_db, "open",
                      JS_NewCFunction(ctx, idb_factory_open, "open", 2));
    JS_SetPropertyStr(ctx, indexed_db, "deleteDatabase",
                      JS_NewCFunction(ctx, idb_factory_delete_database,
                                      "deleteDatabase", 1));

    JS_SetPropertyStr(ctx, global, "indexedDB", indexed_db);
    JS_FreeValue(ctx, global);
}

} // namespace clever::js
