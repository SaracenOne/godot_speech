#include <Godot.hpp>
#include <String.hpp>

#include "speech_processor.hpp"
#include "speech_decoder.hpp"
#include "godot_speech.hpp"
#include "opus_codec.hpp"

#ifdef __GNUC__
__attribute__((noreturn))
#endif

extern "C" void celt_fatal(const char *str, const char *file, int line) {
	godot::Godot::print_error(godot::String(str), __FUNCTION__, file, line);
#if defined(_MSC_VER)
	_set_abort_behavior( 0, _WRITE_ABORT_MSG);
#endif
	abort();
}

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
