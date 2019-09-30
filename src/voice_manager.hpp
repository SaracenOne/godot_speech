#ifndef VOICE_MANAGER_HPP
#define VOICE_MANAGER_HPP

#include <Godot.hpp>
#include <Node.hpp>
#include <Engine.hpp>
#include <AudioServer.hpp>
#include <ProjectSettings.hpp>
#include <Mutex.hpp>

#include "opus_codec.hpp"

namespace godot {

class VoiceManager : public Node {
	GODOT_CLASS(VoiceManager, Node)
	Ref<Mutex> mutex;
	//
	static const uint32_t MIX_BUFFER_COUNT = 4;

	static const uint32_t VOICE_SAMPLE_RATE = 48000;
	static const uint32_t CHANNEL_COUNT = 1;
	static const uint32_t MILLISECONDS_PER_PACKET = 100;
	static const uint32_t BUFFER_FRAME_COUNT = VOICE_SAMPLE_RATE / MILLISECONDS_PER_PACKET;

	OpusCodec<VOICE_SAMPLE_RATE, CHANNEL_COUNT, MILLISECONDS_PER_PACKET> *opus_codec;
    
	const uint32_t BUFFER_BYTE_COUNT = sizeof(uint16_t);
	bool active = false;
private:
	AudioServer *audio_server = NULL;
	PoolByteArray mix_buffer;
	uint32_t current_mix_buffer_position = 0;

	uint32_t capture_ofs = 0;
public:
	static void _register_methods();
	static PoolByteArray _get_buffer_copy(const PoolByteArray p_mix_buffer);

	void start();
	void stop();

	uint32_t get_audio_server_mix_frames();

	void _mix_audio();
	static uint32_t _mix_internal(AudioServer *p_audio_server, const uint32_t &p_frame_count, const uint32_t p_buffer_size, int8_t *p_buffer_out, uint32_t &p_buffer_position_out, uint32_t &p_capture_offset_out);

	static PoolVector2Array _16_pcm_mono_to_real_stereo(const PoolByteArray p_src_buffer);

	PoolByteArray compress_buffer(const PoolByteArray p_pcm_buffer);
	PoolVector2Array decompress_buffer(const PoolByteArray p_compressed_buffer);

	void _init();
	void _ready();
	void _notification(int p_what);

	VoiceManager();
	~VoiceManager();
};

}; // namespace godot

#endif // VOICE_MANAGER_HPP