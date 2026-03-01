#ifndef CLEVER_JS_WEB_AUDIO_H
#define CLEVER_JS_WEB_AUDIO_H

extern "C" {
    struct JSContext;
}

namespace clever::js {

void install_web_audio_bindings(JSContext* ctx);

} // namespace clever::js

#endif
