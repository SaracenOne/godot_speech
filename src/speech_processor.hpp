#ifndef SPEECH_PROCESSOR_HPP
#define SPEECH_PROCESSOR_HPP

#include <Godot.hpp>
#include <Node.hpp>
#include <Engine.hpp>
#include <AudioServer.hpp>
#include <ProjectSettings.hpp>
#include <Mutex.hpp>

#include <AudioEffectStream.hpp>
#include <StreamAudio.hpp>

#include <AudioStreamPlayer.hpp>
#include <AudioStreamMicrophone.hpp>

#include <stdlib.h>
#include <functional>

#include "samplerate.h"
#include "opus_codec.hpp"

#include "speech_decoder.hpp"

namespace godot {

class SpeechProcessor : public Node {
	GODOT_CLASS(SpeechProcessor, Node)
	Ref<Mutex> mutex;
	//
public:
	static const uint32_t VOICE_SAMPLE_RATE = 48000;
	static const uint32_t CHANNEL_COUNT = 1;
	static const uint32_t MILLISECONDS_PER_PACKET = 100;
	static const uint32_t BUFFER_FRAME_COUNT = VOICE_SAMPLE_RATE / MILLISECONDS_PER_PACKET;
	static const uint32_t BUFFER_BYTE_COUNT = sizeof(uint16_t);
	static const uint32_t PCM_BUFFER_SIZE = BUFFER_FRAME_COUNT * BUFFER_BYTE_COUNT * CHANNEL_COUNT;

private:
	OpusCodec<VOICE_SAMPLE_RATE, CHANNEL_COUNT, MILLISECONDS_PER_PACKET> *opus_codec;
    
private:
	uint32_t record_mix_frames_processed = 0;

	AudioServer *audio_server = NULL;
	StreamAudio *stream_audio = NULL;
	AudioStreamPlayer *audio_stream_player = NULL;
	
	uint32_t mix_rate;
	PoolByteArray mix_byte_array;

	PoolRealArray mono_real_array;
	PoolRealArray resampled_real_array;
	uint32_t resampled_real_array_offset = 0;

	PoolByteArray pcm_byte_array_cache;

	// LibResample
	SRC_STATE *libresample_state;
	int libresample_error;
public:
	struct SpeechInput {
		PoolByteArray *pcm_byte_array = NULL;
		float volume = 0.0;
	};

	struct CompressedSpeechBuffer {
		PoolByteArray *compressed_byte_array = NULL;
		int buffer_size = 0;
	};

	std::function<void(SpeechInput *)> speech_processed;
	void register_speech_processed(const std::function<void(SpeechInput *)> &callback)
	{
		speech_processed = callback;
	}

	static void _register_methods();

	uint32_t _resample_audio_buffer(const float *p_src,
		const uint32_t p_src_frame_count,
		const uint32_t p_src_samplerate,
		const uint32_t p_target_samplerate,
		float *p_dst);

	void start();
	void stop();

	uint32_t get_audio_server_mix_frames();

	static void _get_capture_block(
		AudioServer *p_audio_server,
		const uint32_t &p_mix_frame_count,
		const float *p_process_buffer_in,
		float *p_process_buffer_out);

	void _mix_audio(const float *p_process_buffer_in);

	static bool _16_pcm_mono_to_real_stereo(const PoolByteArray *p_src_buffer, PoolVector2Array *p_dst_buffer);

	virtual bool compress_buffer_internal(const PoolByteArray *p_pcm_byte_array, CompressedSpeechBuffer *p_output_buffer) {
		p_output_buffer->buffer_size = opus_codec->encode_buffer(p_pcm_byte_array, p_output_buffer->compressed_byte_array);
		if(p_output_buffer->buffer_size != -1) {
			return true;
		}

		return false;
	}

	virtual bool decompress_buffer_internal(
		SpeechDecoder *speech_decoder,
		const PoolByteArray *p_read_byte_array,
		const int p_read_size,
		PoolVector2Array *p_write_vec2_array) {
		if (opus_codec->decode_buffer(speech_decoder, p_read_byte_array, &pcm_byte_array_cache, p_read_size, PCM_BUFFER_SIZE)) {
			if(_16_pcm_mono_to_real_stereo(&pcm_byte_array_cache, p_write_vec2_array)) {
				return true;
			}
		}
		return true;
	}

	virtual Dictionary compress_buffer(
		const PoolByteArray &p_pcm_byte_array,
		Dictionary p_output_buffer);

	virtual PoolVector2Array decompress_buffer(
		Ref<SpeechDecoder> p_speech_decoder,
		const PoolByteArray &p_read_byte_array,
		const int p_read_size,
		PoolVector2Array p_write_vec2_array);

	Ref<SpeechDecoder> get_speech_decoder() {
		if(opus_codec) {
			return opus_codec->get_speech_decoder();
		} else {
			return NULL;
		}
	}

	void set_streaming_bus(const String &p_name);
	void set_microphone_bus(const String &p_name);

	void set_stream(Ref<AudioStream> p_audio_stream);

	void set_process_all(bool p_active);

	void _setup();

	void _init();
	void _ready();
	void _notification(int p_what);

	SpeechProcessor();
	~SpeechProcessor();
};

}; // namespace godot

#endif // SPEECH_PROCESSOR_HPP
