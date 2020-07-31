#include <Godot.hpp>

#include "speech_processor.hpp"
#include "speech_decoder.hpp"
#include "godot_speech.hpp"
#include "opus_codec.hpp"

extern "C" void GDN_EXPORT godot_speech_gdnative_init(godot_gdnative_init_options *o) {
	godot::Godot::gdnative_init(o);
}

extern "C" void GDN_EXPORT godot_speech_gdnative_terminate(godot_gdnative_terminate_options *o) {
	godot::Godot::gdnative_terminate(o);
}

extern "C" void GDN_EXPORT godot_speech_gdnative_singleton() {
}

extern "C" void GDN_EXPORT godot_speech_nativescript_init(void *handle) {
	godot::Godot::nativescript_init(handle);

	godot::register_class<godot::SpeechProcessor>();
	godot::register_class<godot::SpeechDecoder>();
	godot::register_class<godot::GodotSpeech>();
}