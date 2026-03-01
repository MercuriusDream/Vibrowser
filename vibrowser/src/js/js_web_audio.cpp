#include <clever/js/js_web_audio.h>

extern "C" {
#include <quickjs.h>
}

#include <cstring>
#include <cstdint>
#include <string>

namespace clever::js {

namespace {

struct AudioContextState {
    double sample_rate = 48000.0;
    double current_time = 0.0;
    std::string state = "running";
};

struct OscillatorNodeState {
    std::string type = "sine";
    double frequency = 440.0;
    double detune = 0.0;
    bool started = false;
    bool stopped = false;
};

struct GainNodeState {
    double gain = 1.0;
};

struct AnalyserNodeState {
    int32_t fft_size = 2048;
};

struct AudioBufferSourceNodeState {
    bool loop = false;
    bool has_buffer = false;
    double playback_rate = 1.0;
};

struct AudioBufferState {
    int32_t length = 0;
    double sample_rate = 48000.0;
    int32_t number_of_channels = 1;
};

struct AudioDestinationNodeState {
    int32_t max_channel_count = 32;
};

static JSClassID audio_context_class_id = 0;
static JSClassID oscillator_node_class_id = 0;
static JSClassID gain_node_class_id = 0;
static JSClassID analyser_node_class_id = 0;
static JSClassID audio_buffer_source_node_class_id = 0;
static JSClassID audio_buffer_class_id = 0;
static JSClassID audio_destination_node_class_id = 0;

static void define_readonly_property(JSContext* ctx, JSValue obj,
                                    const char* name, JSValue getter) {
    JSAtom atom = JS_NewAtom(ctx, name);
    JS_DefinePropertyGetSet(ctx, obj, atom, getter, JS_UNDEFINED,
                           JS_PROP_ENUMERABLE);
    JS_FreeAtom(ctx, atom);
}

static JSValue create_audio_param(JSContext* ctx, double value) {
    JSValue param = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, param, "value", JS_NewFloat64(ctx, value));
    return param;
}

static JSValue make_audio_destination_node(JSContext* ctx);

static JSValue make_oscillator_node(JSContext* ctx);
static JSValue make_gain_node(JSContext* ctx);
static JSValue make_analyser_node(JSContext* ctx);
static JSValue make_audio_buffer_source_node(JSContext* ctx);
static JSValue make_audio_buffer_object(JSContext* ctx, int32_t channels,
                                       int32_t length, double sample_rate);

static JSValue audio_node_connect(JSContext* ctx, JSValueConst this_val,
                                 int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    return JS_DupValue(ctx, this_val);
}

static JSValue audio_node_disconnect(JSContext* ctx, JSValueConst /*this_val*/,
                                    int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    (void)ctx;
    return JS_UNDEFINED;
}

static JSValue audio_destination_connect(JSContext* /*ctx*/, JSValueConst /*this_val*/,
                                       int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    return JS_UNDEFINED;
}

static JSValue audio_destination_disconnect(JSContext* ctx, JSValueConst this_val,
                                          int argc, JSValueConst* argv) {
    (void)this_val;
    (void)argc;
    (void)argv;
    (void)ctx;
    return JS_UNDEFINED;
}

static JSValue audio_context_create_oscillator(JSContext* ctx,
                                               JSValueConst /*this_val*/,
                                               int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    return make_oscillator_node(ctx);
}

static JSValue audio_context_create_gain(JSContext* ctx,
                                        JSValueConst /*this_val*/,
                                        int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    return make_gain_node(ctx);
}

static JSValue audio_context_create_analyser(JSContext* ctx,
                                            JSValueConst /*this_val*/,
                                            int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    return make_analyser_node(ctx);
}

static JSValue audio_context_create_buffer_source(JSContext* ctx,
                                                 JSValueConst /*this_val*/,
                                                 int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    return make_audio_buffer_source_node(ctx);
}

static JSValue audio_context_create_buffer(JSContext* ctx, JSValueConst /*this_val*/,
                                          int argc, JSValueConst* argv) {
    int32_t channels = 1;
    int32_t length = 0;
    double sample_rate = 48000.0;

    if (argc > 0 && JS_ToInt32(ctx, &channels, argv[0]) < 0) channels = 1;
    if (argc > 1 && JS_ToInt32(ctx, &length, argv[1]) < 0) length = 0;
    if (argc > 2 && JS_ToFloat64(ctx, &sample_rate, argv[2]) < 0) sample_rate = 48000.0;
    if (channels < 1) channels = 1;
    if (length < 0) length = 0;
    if (sample_rate < 1.0) sample_rate = 48000.0;

    return make_audio_buffer_object(ctx, channels, length, sample_rate);
}

static JSValue audio_context_decode_audio_data(JSContext* ctx,
                                              JSValueConst this_val,
                                              int argc, JSValueConst* argv) {
    (void)this_val;
    if (argc > 1 && JS_IsFunction(ctx, argv[1])) {
        JSValue decoded_buffer = make_audio_buffer_object(ctx, 1, 0, 48000.0);
        JSValue callback_args[1] = { decoded_buffer };
        JSValue callback_result = JS_Call(ctx, argv[1], this_val, 1, callback_args);
        if (JS_IsException(callback_result)) {
            JS_FreeValue(ctx, JS_GetException(ctx));
        } else {
            JS_FreeValue(ctx, callback_result);
        }
        JS_FreeValue(ctx, decoded_buffer);
    }
    return JS_UNDEFINED;
}

static JSValue audio_context_resume(JSContext* /*ctx*/, JSValueConst this_val,
                                   int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    auto* state = static_cast<AudioContextState*>(
        JS_GetOpaque(this_val, audio_context_class_id));
    if (state) {
        state->state = "running";
    }
    return JS_UNDEFINED;
}

static JSValue audio_context_suspend(JSContext* /*ctx*/, JSValueConst this_val,
                                    int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    auto* state = static_cast<AudioContextState*>(
        JS_GetOpaque(this_val, audio_context_class_id));
    if (state) {
        state->state = "suspended";
    }
    return JS_UNDEFINED;
}

static JSValue audio_context_close(JSContext* /*ctx*/, JSValueConst this_val,
                                  int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    auto* state = static_cast<AudioContextState*>(
        JS_GetOpaque(this_val, audio_context_class_id));
    if (state) {
        state->state = "closed";
    }
    return JS_UNDEFINED;
}

static JSValue audio_context_get_sample_rate(JSContext* ctx, JSValueConst this_val,
                                            int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    auto* state = static_cast<AudioContextState*>(
        JS_GetOpaque(this_val, audio_context_class_id));
    double sample_rate = 48000.0;
    if (state) {
        sample_rate = state->sample_rate;
    }
    return JS_NewFloat64(ctx, sample_rate);
}

static JSValue audio_context_get_current_time(JSContext* ctx, JSValueConst this_val,
                                            int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    auto* state = static_cast<AudioContextState*>(
        JS_GetOpaque(this_val, audio_context_class_id));
    double current_time = 0.0;
    if (state) {
        current_time = state->current_time;
    }
    return JS_NewFloat64(ctx, current_time);
}

static JSValue audio_context_get_state(JSContext* ctx, JSValueConst this_val,
                                      int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    auto* state = static_cast<AudioContextState*>(
        JS_GetOpaque(this_val, audio_context_class_id));
    const char* state_value = "running";
    if (state) {
        state_value = state->state.c_str();
    }
    return JS_NewString(ctx, state_value);
}

static JSValue audio_context_get_destination(JSContext* ctx, JSValueConst this_val,
                                           int argc, JSValueConst* argv) {
    (void)this_val;
    (void)argc;
    (void)argv;
    return make_audio_destination_node(ctx);
}

static JSValue oscillator_get_type(JSContext* ctx, JSValueConst this_val,
                                  int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    auto* state = static_cast<OscillatorNodeState*>(
        JS_GetOpaque(this_val, oscillator_node_class_id));
    const char* type = "sine";
    if (state) {
        type = state->type.c_str();
    }
    return JS_NewString(ctx, type);
}

static JSValue oscillator_set_type(JSContext* ctx, JSValueConst this_val,
                                  int argc, JSValueConst* argv) {
    auto* state = static_cast<OscillatorNodeState*>(
        JS_GetOpaque(this_val, oscillator_node_class_id));
    if (state && argc > 0) {
        const char* type = JS_ToCString(ctx, argv[0]);
        if (type) {
            state->type = type;
            JS_FreeCString(ctx, type);
        }
    }
    return JS_UNDEFINED;
}

static JSValue oscillator_get_frequency(JSContext* ctx, JSValueConst this_val,
                                      int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    auto* state = static_cast<OscillatorNodeState*>(
        JS_GetOpaque(this_val, oscillator_node_class_id));
    double value = 440.0;
    if (state) {
        value = state->frequency;
    }
    return create_audio_param(ctx, value);
}

static JSValue oscillator_get_detune(JSContext* ctx, JSValueConst this_val,
                                    int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    auto* state = static_cast<OscillatorNodeState*>(
        JS_GetOpaque(this_val, oscillator_node_class_id));
    double value = 0.0;
    if (state) {
        value = state->detune;
    }
    return create_audio_param(ctx, value);
}

static JSValue oscillator_start(JSContext* ctx, JSValueConst /*this_val*/,
                               int argc, JSValueConst* argv) {
    (void)ctx;
    (void)argc;
    (void)argv;
    return JS_UNDEFINED;
}

static JSValue oscillator_stop(JSContext* ctx, JSValueConst /*this_val*/,
                              int argc, JSValueConst* argv) {
    (void)ctx;
    (void)argc;
    (void)argv;
    return JS_UNDEFINED;
}

static JSValue gain_get_gain(JSContext* ctx, JSValueConst this_val,
                            int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    auto* state = static_cast<GainNodeState*>(
        JS_GetOpaque(this_val, gain_node_class_id));
    double value = 1.0;
    if (state) {
        value = state->gain;
    }
    return create_audio_param(ctx, value);
}

static JSValue analyser_get_fft_size(JSContext* ctx, JSValueConst this_val,
                                    int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    auto* state = static_cast<AnalyserNodeState*>(
        JS_GetOpaque(this_val, analyser_node_class_id));
    int32_t value = 2048;
    if (state) {
        value = state->fft_size;
    }
    return JS_NewInt32(ctx, value);
}

static JSValue analyser_set_fft_size(JSContext* ctx, JSValueConst this_val,
                                    int argc, JSValueConst* argv) {
    auto* state = static_cast<AnalyserNodeState*>(
        JS_GetOpaque(this_val, analyser_node_class_id));
    if (state && argc > 0) {
        int32_t value = state->fft_size;
        if (JS_ToInt32(ctx, &value, argv[0]) == 0) {
            state->fft_size = value;
        }
    }
    return JS_UNDEFINED;
}

static JSValue analyser_get_byte_frequency_data(JSContext* ctx, JSValueConst this_val,
                                               int argc, JSValueConst* argv) {
    (void)this_val;
    if (argc > 0) {
        size_t byte_length = 0;
        uint8_t* data = JS_GetArrayBuffer(ctx, &byte_length, argv[0]);
        if (data && byte_length > 0) {
            std::memset(data, 0, byte_length);
        }
    }
    return JS_UNDEFINED;
}

static JSValue analyser_get_byte_time_domain_data(JSContext* ctx, JSValueConst this_val,
                                                 int argc, JSValueConst* argv) {
    (void)this_val;
    if (argc > 0) {
        size_t byte_length = 0;
        uint8_t* data = JS_GetArrayBuffer(ctx, &byte_length, argv[0]);
        if (data && byte_length > 0) {
            std::memset(data, 0, byte_length);
        }
    }
    return JS_UNDEFINED;
}

static JSValue audio_buffer_source_get_buffer(JSContext* ctx, JSValueConst this_val,
                                             int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    auto* state = static_cast<AudioBufferSourceNodeState*>(
        JS_GetOpaque(this_val, audio_buffer_source_node_class_id));
    if (!state || !state->has_buffer) {
        return JS_NULL;
    }
    return make_audio_buffer_object(ctx, 1, 0, 48000.0);
}

static JSValue audio_buffer_source_set_buffer(JSContext* /*ctx*/, JSValueConst this_val,
                                             int argc, JSValueConst* argv) {
    auto* state = static_cast<AudioBufferSourceNodeState*>(
        JS_GetOpaque(this_val, audio_buffer_source_node_class_id));
    if (state && argc > 0) {
        state->has_buffer = JS_IsObject(argv[0]);
    }
    return JS_UNDEFINED;
}

static JSValue audio_buffer_source_get_playback_rate(JSContext* ctx, JSValueConst this_val,
                                                    int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    auto* state = static_cast<AudioBufferSourceNodeState*>(
        JS_GetOpaque(this_val, audio_buffer_source_node_class_id));
    double rate = 1.0;
    if (state) {
        rate = state->playback_rate;
    }
    return create_audio_param(ctx, rate);
}

static JSValue audio_buffer_source_get_loop(JSContext* ctx, JSValueConst this_val,
                                           int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    auto* state = static_cast<AudioBufferSourceNodeState*>(
        JS_GetOpaque(this_val, audio_buffer_source_node_class_id));
    bool loop = false;
    if (state) {
        loop = state->loop;
    }
    return JS_NewBool(ctx, loop);
}

static JSValue audio_buffer_source_set_loop(JSContext* ctx, JSValueConst this_val,
                                           int argc, JSValueConst* argv) {
    auto* state = static_cast<AudioBufferSourceNodeState*>(
        JS_GetOpaque(this_val, audio_buffer_source_node_class_id));
    if (state && argc > 0) {
        state->loop = JS_ToBool(ctx, argv[0]);
    }
    return JS_UNDEFINED;
}

static JSValue audio_buffer_source_start(JSContext* ctx, JSValueConst /*this_val*/,
                                        int argc, JSValueConst* argv) {
    (void)ctx;
    (void)argc;
    (void)argv;
    return JS_UNDEFINED;
}

static JSValue audio_buffer_source_stop(JSContext* ctx, JSValueConst /*this_val*/,
                                       int argc, JSValueConst* argv) {
    (void)ctx;
    (void)argc;
    (void)argv;
    return JS_UNDEFINED;
}

static JSValue audio_buffer_get_length(JSContext* ctx, JSValueConst this_val,
                                      int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    auto* state = static_cast<AudioBufferState*>(
        JS_GetOpaque(this_val, audio_buffer_class_id));
    int32_t value = 0;
    if (state) {
        value = state->length;
    }
    return JS_NewInt32(ctx, value);
}

static JSValue audio_buffer_get_sample_rate(JSContext* ctx, JSValueConst this_val,
                                           int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    auto* state = static_cast<AudioBufferState*>(
        JS_GetOpaque(this_val, audio_buffer_class_id));
    double value = 48000.0;
    if (state) {
        value = state->sample_rate;
    }
    return JS_NewFloat64(ctx, value);
}

static JSValue audio_buffer_get_channels(JSContext* ctx, JSValueConst this_val,
                                        int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    auto* state = static_cast<AudioBufferState*>(
        JS_GetOpaque(this_val, audio_buffer_class_id));
    int32_t value = 1;
    if (state) {
        value = state->number_of_channels;
    }
    return JS_NewInt32(ctx, value);
}

static JSValue audio_destination_get_max_channel_count(JSContext* ctx, JSValueConst this_val,
                                                     int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    auto* state = static_cast<AudioDestinationNodeState*>(
        JS_GetOpaque(this_val, audio_destination_node_class_id));
    int32_t value = 32;
    if (state) {
        value = state->max_channel_count;
    }
    return JS_NewInt32(ctx, value);
}

static void audio_context_finalizer(JSRuntime* /*rt*/, JSValue val) {
    auto* state = static_cast<AudioContextState*>(JS_GetOpaque(val, audio_context_class_id));
    if (state) {
        delete state;
    }
}

static void oscillator_finalizer(JSRuntime* /*rt*/, JSValue val) {
    auto* state = static_cast<OscillatorNodeState*>(
        JS_GetOpaque(val, oscillator_node_class_id));
    if (state) {
        delete state;
    }
}

static void gain_finalizer(JSRuntime* /*rt*/, JSValue val) {
    auto* state = static_cast<GainNodeState*>(JS_GetOpaque(val, gain_node_class_id));
    if (state) {
        delete state;
    }
}

static void analyser_finalizer(JSRuntime* /*rt*/, JSValue val) {
    auto* state = static_cast<AnalyserNodeState*>(JS_GetOpaque(val, analyser_node_class_id));
    if (state) {
        delete state;
    }
}

static void audio_buffer_source_finalizer(JSRuntime* /*rt*/, JSValue val) {
    auto* state = static_cast<AudioBufferSourceNodeState*>(
        JS_GetOpaque(val, audio_buffer_source_node_class_id));
    if (state) {
        delete state;
    }
}

static void audio_buffer_finalizer(JSRuntime* /*rt*/, JSValue val) {
    auto* state = static_cast<AudioBufferState*>(JS_GetOpaque(val, audio_buffer_class_id));
    if (state) {
        delete state;
    }
}

static void destination_finalizer(JSRuntime* /*rt*/, JSValue val) {
    auto* state = static_cast<AudioDestinationNodeState*>(
        JS_GetOpaque(val, audio_destination_node_class_id));
    if (state) {
        delete state;
    }
}

static JSClassDef audio_context_class_def = {
    "AudioContext", audio_context_finalizer, nullptr, nullptr, nullptr,
};

static JSClassDef oscillator_node_class_def = {
    "OscillatorNode", oscillator_finalizer, nullptr, nullptr, nullptr,
};

static JSClassDef gain_node_class_def = {
    "GainNode", gain_finalizer, nullptr, nullptr, nullptr,
};

static JSClassDef analyser_node_class_def = {
    "AnalyserNode", analyser_finalizer, nullptr, nullptr, nullptr,
};

static JSClassDef audio_buffer_source_node_class_def = {
    "AudioBufferSourceNode", audio_buffer_source_finalizer, nullptr, nullptr, nullptr,
};

static JSClassDef audio_buffer_class_def = {
    "AudioBuffer", audio_buffer_finalizer, nullptr, nullptr, nullptr,
};

static JSClassDef audio_destination_node_class_def = {
    "AudioDestinationNode", destination_finalizer, nullptr, nullptr, nullptr,
};

static void ensure_web_audio_classes(JSContext* ctx) {
    if (audio_context_class_id == 0) {
        JS_NewClassID(&audio_context_class_id);
        JS_NewClass(JS_GetRuntime(ctx), audio_context_class_id, &audio_context_class_def);
    }
    if (oscillator_node_class_id == 0) {
        JS_NewClassID(&oscillator_node_class_id);
        JS_NewClass(JS_GetRuntime(ctx), oscillator_node_class_id, &oscillator_node_class_def);
    }
    if (gain_node_class_id == 0) {
        JS_NewClassID(&gain_node_class_id);
        JS_NewClass(JS_GetRuntime(ctx), gain_node_class_id, &gain_node_class_def);
    }
    if (analyser_node_class_id == 0) {
        JS_NewClassID(&analyser_node_class_id);
        JS_NewClass(JS_GetRuntime(ctx), analyser_node_class_id, &analyser_node_class_def);
    }
    if (audio_buffer_source_node_class_id == 0) {
        JS_NewClassID(&audio_buffer_source_node_class_id);
        JS_NewClass(JS_GetRuntime(ctx),
                    audio_buffer_source_node_class_id,
                    &audio_buffer_source_node_class_def);
    }
    if (audio_buffer_class_id == 0) {
        JS_NewClassID(&audio_buffer_class_id);
        JS_NewClass(JS_GetRuntime(ctx), audio_buffer_class_id, &audio_buffer_class_def);
    }
    if (audio_destination_node_class_id == 0) {
        JS_NewClassID(&audio_destination_node_class_id);
        JS_NewClass(JS_GetRuntime(ctx),
                    audio_destination_node_class_id,
                    &audio_destination_node_class_def);
    }
}

static JSValue audio_context_constructor(JSContext* ctx, JSValueConst /*new_target*/,
                                        int argc, JSValueConst* argv) {
    (void)argc;
    (void)argv;
    ensure_web_audio_classes(ctx);

    JSValue context = JS_NewObjectClass(ctx, audio_context_class_id);
    auto* state = new AudioContextState();
    JS_SetOpaque(context, state);

    JS_SetPropertyStr(ctx, context, "createOscillator",
                      JS_NewCFunction(ctx, audio_context_create_oscillator,
                                      "createOscillator", 0));
    JS_SetPropertyStr(ctx, context, "createGain",
                      JS_NewCFunction(ctx, audio_context_create_gain, "createGain", 0));
    JS_SetPropertyStr(ctx, context, "createAnalyser",
                      JS_NewCFunction(ctx, audio_context_create_analyser,
                                      "createAnalyser", 0));
    JS_SetPropertyStr(ctx, context, "createBufferSource",
                      JS_NewCFunction(ctx, audio_context_create_buffer_source,
                                      "createBufferSource", 0));
    JS_SetPropertyStr(ctx, context, "createBuffer",
                      JS_NewCFunction(ctx, audio_context_create_buffer,
                                      "createBuffer", 3));
    JS_SetPropertyStr(ctx, context, "decodeAudioData",
                      JS_NewCFunction(ctx, audio_context_decode_audio_data,
                                      "decodeAudioData", 3));
    JS_SetPropertyStr(ctx, context, "resume",
                      JS_NewCFunction(ctx, audio_context_resume, "resume", 0));
    JS_SetPropertyStr(ctx, context, "suspend",
                      JS_NewCFunction(ctx, audio_context_suspend, "suspend", 0));
    JS_SetPropertyStr(ctx, context, "close",
                      JS_NewCFunction(ctx, audio_context_close, "close", 0));

    define_readonly_property(ctx, context, "sampleRate",
                            JS_NewCFunction(ctx, audio_context_get_sample_rate,
                                            "sampleRate", 0));
    define_readonly_property(ctx, context, "currentTime",
                            JS_NewCFunction(ctx, audio_context_get_current_time,
                                            "currentTime", 0));
    define_readonly_property(ctx, context, "state",
                            JS_NewCFunction(ctx, audio_context_get_state,
                                            "state", 0));
    define_readonly_property(ctx, context, "destination",
                            JS_NewCFunction(ctx, audio_context_get_destination,
                                            "destination", 0));
    return context;
}

static JSValue make_oscillator_node(JSContext* ctx) {
    ensure_web_audio_classes(ctx);
    JSValue node = JS_NewObjectClass(ctx, oscillator_node_class_id);
    auto* state = new OscillatorNodeState();
    JS_SetOpaque(node, state);

    JS_SetPropertyStr(ctx, node, "connect",
                      JS_NewCFunction(ctx, audio_node_connect, "connect", 1));
    JS_SetPropertyStr(ctx, node, "disconnect",
                      JS_NewCFunction(ctx, audio_node_disconnect, "disconnect", 0));
    JS_SetPropertyStr(ctx, node, "start",
                      JS_NewCFunction(ctx, oscillator_start, "start", 1));
    JS_SetPropertyStr(ctx, node, "stop",
                      JS_NewCFunction(ctx, oscillator_stop, "stop", 1));
    define_readonly_property(ctx, node, "frequency",
                            JS_NewCFunction(ctx, oscillator_get_frequency,
                                            "frequency", 0));
    define_readonly_property(ctx, node, "detune",
                            JS_NewCFunction(ctx, oscillator_get_detune,
                                            "detune", 0));
    JSValue type_getter = JS_NewCFunction(ctx, oscillator_get_type, "type", 0);
    JSValue type_setter = JS_NewCFunction(ctx, oscillator_set_type, "type", 1);
    JSAtom type_atom = JS_NewAtom(ctx, "type");
    JS_DefinePropertyGetSet(ctx, node, type_atom, type_getter, type_setter,
                            JS_PROP_ENUMERABLE);
    JS_FreeAtom(ctx, type_atom);
    return node;
}

static JSValue make_gain_node(JSContext* ctx) {
    ensure_web_audio_classes(ctx);
    JSValue node = JS_NewObjectClass(ctx, gain_node_class_id);
    auto* state = new GainNodeState();
    JS_SetOpaque(node, state);

    JS_SetPropertyStr(ctx, node, "connect",
                      JS_NewCFunction(ctx, audio_node_connect, "connect", 1));
    JS_SetPropertyStr(ctx, node, "disconnect",
                      JS_NewCFunction(ctx, audio_node_disconnect, "disconnect", 0));
    define_readonly_property(ctx, node, "gain",
                            JS_NewCFunction(ctx, gain_get_gain, "gain", 0));
    return node;
}

static JSValue make_analyser_node(JSContext* ctx) {
    ensure_web_audio_classes(ctx);
    JSValue node = JS_NewObjectClass(ctx, analyser_node_class_id);
    auto* state = new AnalyserNodeState();
    JS_SetOpaque(node, state);

    JS_SetPropertyStr(ctx, node, "connect",
                      JS_NewCFunction(ctx, audio_node_connect, "connect", 1));
    JS_SetPropertyStr(ctx, node, "disconnect",
                      JS_NewCFunction(ctx, audio_node_disconnect, "disconnect", 0));
    JS_SetPropertyStr(ctx, node, "getByteFrequencyData",
                      JS_NewCFunction(ctx, analyser_get_byte_frequency_data,
                                      "getByteFrequencyData", 1));
    JS_SetPropertyStr(ctx, node, "getByteTimeDomainData",
                      JS_NewCFunction(ctx, analyser_get_byte_time_domain_data,
                                      "getByteTimeDomainData", 1));
    JSValue fft_getter = JS_NewCFunction(ctx, analyser_get_fft_size, "fftSize", 0);
    JSValue fft_setter = JS_NewCFunction(ctx, analyser_set_fft_size, "fftSize", 1);
    JSAtom fft_atom = JS_NewAtom(ctx, "fftSize");
    JS_DefinePropertyGetSet(ctx, node, fft_atom, fft_getter, fft_setter,
                            JS_PROP_ENUMERABLE);
    JS_FreeAtom(ctx, fft_atom);
    return node;
}

static JSValue make_audio_buffer_source_node(JSContext* ctx) {
    ensure_web_audio_classes(ctx);
    JSValue node = JS_NewObjectClass(ctx, audio_buffer_source_node_class_id);
    auto* state = new AudioBufferSourceNodeState();
    JS_SetOpaque(node, state);

    JS_SetPropertyStr(ctx, node, "connect",
                      JS_NewCFunction(ctx, audio_node_connect, "connect", 1));
    JS_SetPropertyStr(ctx, node, "disconnect",
                      JS_NewCFunction(ctx, audio_node_disconnect, "disconnect", 0));
    JS_SetPropertyStr(ctx, node, "start",
                      JS_NewCFunction(ctx, audio_buffer_source_start, "start", 3));
    JS_SetPropertyStr(ctx, node, "stop",
                      JS_NewCFunction(ctx, audio_buffer_source_stop, "stop", 1));
    JSValue buffer_getter = JS_NewCFunction(ctx, audio_buffer_source_get_buffer, "buffer", 0);
    JSValue buffer_setter = JS_NewCFunction(ctx, audio_buffer_source_set_buffer, "buffer", 1);
    JSAtom buffer_atom = JS_NewAtom(ctx, "buffer");
    JS_DefinePropertyGetSet(ctx, node, buffer_atom, buffer_getter, buffer_setter,
                            JS_PROP_ENUMERABLE);
    JS_FreeAtom(ctx, buffer_atom);

    define_readonly_property(ctx, node, "playbackRate",
                            JS_NewCFunction(ctx, audio_buffer_source_get_playback_rate,
                                            "playbackRate", 0));
    JSValue loop_getter = JS_NewCFunction(ctx, audio_buffer_source_get_loop, "loop", 0);
    JSValue loop_setter = JS_NewCFunction(ctx, audio_buffer_source_set_loop, "loop", 1);
    JSAtom loop_atom = JS_NewAtom(ctx, "loop");
    JS_DefinePropertyGetSet(ctx, node, loop_atom, loop_getter, loop_setter,
                            JS_PROP_ENUMERABLE);
    JS_FreeAtom(ctx, loop_atom);
    return node;
}

static JSValue make_audio_destination_node(JSContext* ctx) {
    ensure_web_audio_classes(ctx);
    JSValue node = JS_NewObjectClass(ctx, audio_destination_node_class_id);
    auto* state = new AudioDestinationNodeState();
    JS_SetOpaque(node, state);

    JS_SetPropertyStr(ctx, node, "connect",
                      JS_NewCFunction(ctx, audio_destination_connect,
                                      "connect", 1));
    JS_SetPropertyStr(ctx, node, "disconnect",
                      JS_NewCFunction(ctx, audio_destination_disconnect,
                                      "disconnect", 0));
    define_readonly_property(ctx, node, "maxChannelCount",
                            JS_NewCFunction(ctx, audio_destination_get_max_channel_count,
                                            "maxChannelCount", 0));
    return node;
}

static JSValue make_audio_buffer_object(JSContext* ctx, int32_t channels,
                                       int32_t length, double sample_rate) {
    ensure_web_audio_classes(ctx);
    JSValue buffer = JS_NewObjectClass(ctx, audio_buffer_class_id);
    auto* state = new AudioBufferState();
    state->length = length;
    state->number_of_channels = channels;
    state->sample_rate = sample_rate;
    JS_SetOpaque(buffer, state);

    define_readonly_property(ctx, buffer, "length",
                            JS_NewCFunction(ctx, audio_buffer_get_length, "length", 0));
    define_readonly_property(ctx, buffer, "sampleRate",
                            JS_NewCFunction(ctx, audio_buffer_get_sample_rate, "sampleRate", 0));
    define_readonly_property(ctx, buffer, "numberOfChannels",
                            JS_NewCFunction(ctx, audio_buffer_get_channels,
                                            "numberOfChannels", 0));
    return buffer;
}

} // namespace

void install_web_audio_bindings(JSContext* ctx) {
    ensure_web_audio_classes(ctx);

    JSValue global = JS_GetGlobalObject(ctx);
    JSValue ctor = JS_NewCFunction2(ctx, audio_context_constructor,
                                    "AudioContext", 0, JS_CFUNC_constructor, 0);
    JS_SetPropertyStr(ctx, global, "AudioContext", ctor);
    JS_SetPropertyStr(ctx, global, "webkitAudioContext", JS_DupValue(ctx, ctor));
    JS_FreeValue(ctx, ctor);
    JS_FreeValue(ctx, global);
}

} // namespace clever::js
