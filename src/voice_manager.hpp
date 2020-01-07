#ifndef VOICE_MANAGER_HPP
#define VOICE_MANAGER_HPP

#include <Godot.hpp>
#include <Node.hpp>
#include <Engine.hpp>
#include <AudioServer.hpp>
#include <ProjectSettings.hpp>
#include <Mutex.hpp>

#include "samplerate.h"
#include "opus_codec.hpp"


namespace godot {

class VoiceManager : public Node {
	GODOT_CLASS(VoiceManager, Node)
	Ref<Mutex> mutex;
	//

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

	PoolRealArray mono_buffer;
	PoolRealArray resampled_buffer;
	uint32_t resampled_buffer_offset = 0;

	// LibResample
	SRC_STATE *libresample_state;
	int libresample_error;

	uint32_t capture_ofs = 0;
public:
	static void _register_methods();

	uint32_t _resample_audio_buffer(const float *p_src, const uint32_t p_src_frame_count, float *p_dst, const uint32_t p_dst_frame_count, double p_src_ratio);
	static PoolByteArray _get_buffer_copy(const PoolByteArray p_mix_buffer);

	void start();
	void stop();

	uint32_t get_audio_server_mix_frames();

	static uint32_t _get_capture_block(
		AudioServer *p_audio_server,
		const uint32_t &p_mix_frame_count,
		float *p_process_buffer_out,
		uint32_t &p_capture_offset_out);

	void _mix_audio();

	static PoolVector2Array _16_pcm_mono_to_real_stereo(const PoolByteArray p_src_buffer);

	// Using PoolVectors directly on register method types seems to cause a memory leak!
	virtual PoolByteArray compress_buffer(PoolByteArray p_pcm_buffer);
	virtual PoolVector2Array decompress_buffer(PoolByteArray p_compressed_buffer);

	void _init();
	void _ready();
	void _notification(int p_what);

	VoiceManager();
	~VoiceManager();
};

}; // namespace godot

#endif // VOICE_MANAGER_HPP