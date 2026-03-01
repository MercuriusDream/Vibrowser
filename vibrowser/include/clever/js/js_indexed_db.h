#ifndef CLEVER_JS_INDEXED_DB_H
#define CLEVER_JS_INDEXED_DB_H

extern "C" {
    struct JSContext;
}

namespace clever::js {

void install_indexed_db_bindings(JSContext* ctx);

} // namespace clever::js

#endif
