#include "voice_manager.hpp"

#include <algorithm>

using namespace godot;

#define SET_BUFFER_16_BIT(buffer, buffer_pos, sample) ((int16_t *)buffer)[buffer_pos] = sample >> 16;
#define STEREO_CHANNEL_COUNT 2

#define SIGNED_32_BIT_SIZE 2147483647
#define UNSIGNED_32_BIT_SIZE 4294967295
#define SIGNED_16_BIT_SIZE 32767
#define UNSIGNED_16_BIT_SIZE 65536

#define RECORD_MIX_FRAMES 1024

void VoiceManager::_register_methods() {
	register_method("_init", &VoiceManager::_init);
	register_method("_ready", &VoiceManager::_ready);
	register_method("_notification", &VoiceManager::_notification);
    
	register_method("start", &VoiceManager::start);
	register_method("stop", &VoiceManager::stop);

	register_method("compress_buffer", &VoiceManager::compress_buffer);
	register_method("decompress_buffer", &VoiceManager::decompress_buffer);

	register_signal<VoiceManager>("audio_packet_processed", "packet", GODOT_VARIANT_TYPE_POOL_BYTE_ARRAY);
}

uint32_t VoiceManager::_resample_audio_buffer(
	const float *p_src,
	const uint32_t p_src_frame_count,
	const uint32_t p_src_samplerate,
	const uint32_t p_target_samplerate,
	float *p_dst) {

	if (p_src_samplerate != p_target_samplerate) {
		SRC_DATA src_data;

		src_data.data_in = p_src;
		src_data.data_out = p_dst;

		src_data.input_frames = p_src_frame_count;
		src_data.output_frames = p_src_frame_count * 4;

		src_data.src_ratio = (double)p_target_samplerate / (double)p_src_samplerate;
		src_data.end_of_input = 0;

		int error = src_process(libresample_state, &src_data);
		if (error != 0) {
			Godot::print_error("resample_error!", __FUNCTION__, __FILE__, __LINE__);
			return 0;
		}
		return src_data.output_frames_gen;
	} else {
		memcpy(p_dst, p_src, p_src_frame_count * sizeof(float));
		return p_src_frame_count;
	}
}

void VoiceManager::_get_capture_block(AudioServer *p_audio_server,
	const uint32_t &p_mix_frame_count,
	const float *p_process_buffer_in,
	float *p_process_buffer_out)
{

	// 0.1 second based on the internal sample rate
	//uint32_t playback_delay = std::min<uint32_t>(((50 * mix_rate) / 1000) * 2, capture_buffer.size() >> 1);

	uint32_t capture_offset = 0;
	{
		for (int i = 0; i < p_mix_frame_count; i++) {
			{
				float mono = 0.0f;
				for (int j = 0; j < STEREO_CHANNEL_COUNT; j++) {
					mono += p_process_buffer_in[capture_offset] * 0.5f;
					capture_offset++;
				}
				p_process_buffer_out[i] = mono;
			}
		}
	}
}

void VoiceManager::_mix_audio(const float *p_incoming_buffer) {
	mutex->lock();
	{
		int8_t *write_buffer = reinterpret_cast<int8_t *>(mix_buffer.write().ptr());
		if (audio_server) {
			_get_capture_block(audio_server, RECORD_MIX_FRAMES, p_incoming_buffer, mono_buffer.write().ptr());
			
			uint32_t resampled_frame_count = resampled_buffer_offset + _resample_audio_buffer(
				mono_buffer.read().ptr(), // Pointer to source buffer
				RECORD_MIX_FRAMES, // Size of source buffer * sizeof(float)
				mix_rate, // Source sample rate
				VOICE_SAMPLE_RATE, // Target sample rate
				resampled_buffer.write().ptr() + resampled_buffer_offset);
			
			resampled_buffer_offset = 0;

			const float *resampled_buffer_read_ptr = resampled_buffer.read().ptr();
			while (resampled_buffer_offset < resampled_frame_count - BUFFER_FRAME_COUNT) {
				for (int i = 0; i < BUFFER_FRAME_COUNT; i++) {
					float frame_float = resampled_buffer_read_ptr[resampled_buffer_offset + i];
					int frame_integer = int32_t(frame_float * (float)SIGNED_32_BIT_SIZE);

					write_buffer[i*2] = SET_BUFFER_16_BIT(write_buffer, i, frame_integer);
				}

				emit_signal("audio_packet_processed", mix_buffer);
				resampled_buffer_offset += BUFFER_FRAME_COUNT;
			}

			{
				float *resampled_buffer_write_ptr = resampled_buffer.write().ptr();
				uint32_t remaining_resampled_buffer_frames = (resampled_frame_count - resampled_buffer_offset);
				
				// Copy the remaining frames to the beginning of the buffer for the next around
				if (remaining_resampled_buffer_frames > 0) {
					memcpy(resampled_buffer_write_ptr, resampled_buffer_read_ptr + resampled_buffer_offset, remaining_resampled_buffer_frames * sizeof(float));
				}
				resampled_buffer_offset = remaining_resampled_buffer_frames;
			}
		}
	}
	mutex->unlock();
}

PoolByteArray VoiceManager::_get_buffer_copy(const PoolByteArray p_mix_buffer) {
	PoolByteArray out;

	uint32_t mix_buffer_size = p_mix_buffer.size();
	if(mix_buffer_size > 0) {
		out.resize(mix_buffer_size);
		memcpy(out.write().ptr(), p_mix_buffer.read().ptr(), mix_buffer_size);
	}

	return out;
}

void VoiceManager::start() {
	if (!ProjectSettings::get_singleton()->get("audio/enable_audio_input")) {
		Godot::print_warning("Need to enable Project settings > Audio > Enable Audio Input option to use capturing.", __FUNCTION__, __FILE__, __LINE__);
		return;
	}
    
	if(!audio_stream_player || !stream_audio) {
		return;
	}

	audio_stream_player->play();
	stream_audio->clear();
}

void VoiceManager::stop() {
	if(!audio_stream_player) {
		return;
	}
	audio_stream_player->stop();
}

PoolVector2Array VoiceManager::_16_pcm_mono_to_real_stereo(const PoolByteArray p_src_buffer) {
	PoolVector2Array real_audio_frames;
	uint32_t buffer_size = p_src_buffer.size();

	ERR_FAIL_COND_V(buffer_size % 2, real_audio_frames);

	uint32_t frame_count = buffer_size / 2;
	real_audio_frames.resize(frame_count);
    
	const int16_t *src_buffer_ptr = reinterpret_cast<const int16_t *>(p_src_buffer.read().ptr());
	real_t *real_buffer_ptr = reinterpret_cast<real_t *>(real_audio_frames.write().ptr());

	for(int i = 0; i < frame_count; i++) {
		float value = ((float)*src_buffer_ptr) / 32768.0f;

		*(real_buffer_ptr+0) = value;
		*(real_buffer_ptr+1) = value;

		real_buffer_ptr+=2;
		src_buffer_ptr++;
	}

	return real_audio_frames;
}

PoolByteArray VoiceManager::compress_buffer(const PoolByteArray p_pcm_buffer) {
	return opus_codec->encode_buffer(p_pcm_buffer);
}

PoolVector2Array VoiceManager::decompress_buffer(const PoolByteArray p_compressed_buffer) {
	return _16_pcm_mono_to_real_stereo(opus_codec->decode_buffer(p_compressed_buffer));
}

void VoiceManager::_init() {
	Godot::print(String("VoiceManager::_init"));

	mutex.instance();
}

void VoiceManager::_ready() {
	if (!Engine::get_singleton()->is_editor_hint()) {
		if (audio_server) {
			mix_rate = audio_server->get_mix_rate();
			int bus_count = audio_server->get_bus_count();
			for (int i = 0; i < bus_count; i++) {
				String bus_name = audio_server->get_bus_name(i);
				if (bus_name == "Mic") {
					int effect_count = audio_server->get_bus_effect_count(i);
					for (int j = 0; j < effect_count; j++) {
						Ref<AudioEffect> audio_effect = audio_server->get_bus_effect(i, j);
						Ref<AudioEffectStream> audio_effect_stream = audio_effect;
						if (audio_effect_stream.is_valid()) {
							stream_audio = StreamAudio::_new();
							stream_audio->set_name("StreamAudio");
							stream_audio->set_audio_effect_stream(i, j);
							add_child(stream_audio);

							Ref<AudioStreamMicrophone> audio_stream_microphone = AudioStreamMicrophone::_new();

							audio_stream_player = AudioStreamPlayer::_new();
							audio_stream_player->set_name("AudioStreamPlayer");
							audio_stream_player->set_stream(audio_stream_microphone);
							audio_stream_player->set_bus("Mic");
							add_child(audio_stream_player);

							set_process(true);
						}
					}
				}
			}
		}
	}
}

void VoiceManager::_notification(int p_what) {
	switch(p_what) {
		case NOTIFICATION_ENTER_TREE:
			if(!Engine::get_singleton()->is_editor_hint()) {
				audio_server = AudioServer::get_singleton();
				if(audio_server != NULL) {
					mutex->lock();
					mix_buffer.resize(BUFFER_FRAME_COUNT * BUFFER_BYTE_COUNT);
					mutex->unlock();
				}
			}
		break;
		case NOTIFICATION_EXIT_TREE:
			if(!Engine::get_singleton()->is_editor_hint()) {
				stop();
				mix_buffer.resize(0);
				
				audio_server = NULL;
			}
		break;
		case NOTIFICATION_PROCESS:
			if(!Engine::get_singleton()->is_editor_hint()) {
				if (stream_audio && audio_stream_player->is_playing()) {
					PoolRealArray audio_frames = stream_audio->get_audio_frames(RECORD_MIX_FRAMES);
					if (audio_frames.size() > 0) {
						_mix_audio(audio_frames.read().ptr());
					}
				}
			}
		break;
	}
}

VoiceManager::VoiceManager() {
	Godot::print(String("VoiceManager::VoiceManager"));
	opus_codec = new OpusCodec<VOICE_SAMPLE_RATE, CHANNEL_COUNT, MILLISECONDS_PER_PACKET>();

	mono_buffer.resize(RECORD_MIX_FRAMES);
	resampled_buffer.resize(RECORD_MIX_FRAMES * 4);
	libresample_state = src_new(SRC_SINC_BEST_QUALITY, CHANNEL_COUNT, &libresample_error);
}

VoiceManager::~VoiceManager() {
	libresample_state = src_delete(libresample_state);

	Godot::print(String("VoiceManager::~VoiceManager"));
	delete opus_codec;
}