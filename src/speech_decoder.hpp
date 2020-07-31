#ifndef SPEECH_DECODER_HPP
#define SPEECH_DECODER_HPP

#include <Godot.hpp>
#include <Reference.hpp>

#include "macros.hpp"

#if SPEECH_DECODER_POLYMORPHISM
#else
#include <opus.h>
#endif

namespace godot {

#if SPEECH_DECODER_POLYMORPHISM
class SpeechDecoder : public Reference {
	GODOT_CLASS(SpeechDecoder, Reference)
public:
	static void _register_methods() {
		register_method("_init", &SpeechDecoder::_init);
	}

	SpeechDecoder() {}
	virtual ~SpeechDecoder() {}

	bool process(
		const PoolByteArray *p_compressed_buffer,
		PoolByteArray *p_pcm_output_buffer,
		const int p_compressed_buffer_size,
		const int p_pcm_output_buffer_size,
		const int p_buffer_frame_count) {return false;}

	void _init() {}
};
#else
class SpeechDecoder : public Reference {
	GODOT_CLASS(SpeechDecoder, Reference)
public:
	static void _register_methods() {
		register_method("_init", &SpeechDecoder::_init);
	}
private:
	::OpusDecoder *decoder = NULL;
public:
	SpeechDecoder() {
	}
	~SpeechDecoder() {
		set_decoder(NULL);
	}

	void _init() {}

	void set_decoder(::OpusDecoder *p_decoder) {
		if (!decoder) {
			opus_decoder_destroy(decoder);
		}
		decoder = p_decoder;
	}

	virtual bool process(
		const PoolByteArray *p_compressed_buffer,
		PoolByteArray *p_pcm_output_buffer,
		const int p_compressed_buffer_size,
		const int p_pcm_output_buffer_size,
		const int p_buffer_frame_count)
	{
		if (decoder) {
			opus_int16 *output_buffer_pointer = reinterpret_cast<opus_int16 *>(p_pcm_output_buffer->write().ptr());
			const unsigned char *opus_buffer_pointer = reinterpret_cast<const unsigned char *>(p_compressed_buffer->read().ptr());

			opus_int32 ret_value = opus_decode(decoder, opus_buffer_pointer, p_compressed_buffer_size, output_buffer_pointer, p_buffer_frame_count, 0);
			return true;
		}

		return false;
	}
};
#endif

};
#endif //SPEECH_DECODER_HPP