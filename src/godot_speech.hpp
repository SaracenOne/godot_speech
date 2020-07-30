#ifndef GODOTSPEECH_HPP
#define GODOTSPEECH_HPP

#include <Godot.hpp>
#include <Array.hpp>
#include <Dictionary.hpp>
#include <Node.hpp>
#include <Engine.hpp>
#include <AudioServer.hpp>
#include <ProjectSettings.hpp>
#include <Mutex.hpp>

#include "mutex_lock.hpp"
#include "voice_manager.hpp"

namespace godot {

class GodotSpeech : public Node {
	GODOT_CLASS(GodotSpeech, Node)
	
	static const int MAX_AUDIO_BUFFER_ARRAY_SIZE = 10;
	
	PoolByteArray input_byte_array;
	float volume;

	Ref<Mutex> audio_mutex;

	int input_audio_sent_id = 0;

	Node *voice_controller; // TODO: rewrite this in C++
	VoiceManager *mic_input_processor = NULL;

	struct InputPacket {
		PoolByteArray compressed_byte_array;
		int buffer_size = 0;
		float loudness = 0.0;
	};


	int current_input_size = 0;
	PoolByteArray compression_output_byte_array;
	InputPacket input_audio_buffer_array[MAX_AUDIO_BUFFER_ARRAY_SIZE];
	// This the area for fixed size dictionaries representing the audio output data as a means of accounting for the fact that PoolArrays duplicate
	Dictionary output_audio_buffer_array[MAX_AUDIO_BUFFER_ARRAY_SIZE];
	//
private:
	// Assigns the memory to the fixed audio buffer arrays
	void preallocate_buffers() {
		input_byte_array.resize(VoiceManager::PCM_BUFFER_SIZE);
		compression_output_byte_array.resize(VoiceManager::PCM_BUFFER_SIZE);
		for (int i = 0; i < MAX_AUDIO_BUFFER_ARRAY_SIZE; i++) {
			input_audio_buffer_array[i].compressed_byte_array.resize(VoiceManager::PCM_BUFFER_SIZE);
			
			PoolByteArray pool_byte_array;
			pool_byte_array.resize(VoiceManager::PCM_BUFFER_SIZE);

			output_audio_buffer_array[i]["byte_array"] = pool_byte_array;
			output_audio_buffer_array[i]["buffer_size"] = 0;
			output_audio_buffer_array[i]["loudness"] = 0.0f;
		}
	}

	// Assigns a callback from the mic_input_processor to this object.
	void setup_connections() {
		if(mic_input_processor) {
			mic_input_processor->register_mic_input_processed(
				std::function<void(VoiceManager::MicInput *)>(
					std::bind(&GodotSpeech::mic_input_processed, this, std::placeholders::_1)
				)
			);
		}
	}

	// Returns a pointer to the first valid input packet
	// If the current_input_size has exceeded MAX_AUDIO_BUFFER_ARRAY_SIZE,
	// The front packet will be popped from the queue back recursively
	// copying from the back.
	InputPacket *get_next_valid_input_packet() {
		if(current_input_size < MAX_AUDIO_BUFFER_ARRAY_SIZE) {
			InputPacket *input_packet = &input_audio_buffer_array[current_input_size];
			current_input_size++;
			return input_packet;
		} else {
			for(int i = MAX_AUDIO_BUFFER_ARRAY_SIZE-1; i > 0; i--) {
				memcpy(input_audio_buffer_array[i-1].compressed_byte_array.write().ptr(), 
				input_audio_buffer_array[i].compressed_byte_array.read().ptr(),
				VoiceManager::PCM_BUFFER_SIZE);

				input_audio_buffer_array[i-1].buffer_size = input_audio_buffer_array[i].buffer_size;
			}
			return &input_audio_buffer_array[MAX_AUDIO_BUFFER_ARRAY_SIZE-1];
		}
	}

	// Is responsible for recieving packets from the VoiceManager and then compressing them
	void mic_input_processed(VoiceManager::MicInput *p_mic_input) {
		// Copy the raw PCM data from the MicInput packet to the input byte array
		PoolByteArray *mic_input_byte_array = p_mic_input->pcm_byte_array;
		memcpy(input_byte_array.write().ptr(), mic_input_byte_array->read().ptr(), VoiceManager::PCM_BUFFER_SIZE);

		// Create a new VoiceManager::CompressedBufferInput to be passed into the compressor
		// and assign it the compressed_byte_array from the input packet
		VoiceManager::CompressedBufferInput compressed_buffer_input;
		compressed_buffer_input.compressed_byte_array = &compression_output_byte_array;

		// Compress the packet
		mic_input_processor->compress_buffer_internal(&input_byte_array, &compressed_buffer_input);
		{
			// Lock
			MutexLock mutex_lock(audio_mutex.ptr());

			// Find the next valid input packet in the queue
			InputPacket *input_packet = get_next_valid_input_packet();
			// Copy the buffer size from the compressed_buffer_input back into the input packet
			memcpy(input_packet->compressed_byte_array.write().ptr(), compressed_buffer_input.compressed_byte_array->read().ptr(), VoiceManager::PCM_BUFFER_SIZE);
			input_packet->buffer_size = compressed_buffer_input.buffer_size;
			input_packet->loudness = p_mic_input->volume;
		}
	}
public:
	static void _register_methods() {
		register_method("_init", &GodotSpeech::_init);
		register_method("_ready", &GodotSpeech::_ready);
		register_method("_notification", &GodotSpeech::_notification);

		register_method("decompress_buffer", &GodotSpeech::decompress_buffer);

		register_method("copy_and_clear_buffers", &GodotSpeech::copy_and_clear_buffers);

		register_method("start_recording", &GodotSpeech::start_recording);
		register_method("end_recording", &GodotSpeech::end_recording);

		register_method("set_streaming_bus", &GodotSpeech::set_streaming_bus);
		register_method("set_microphone_bus", &GodotSpeech::set_microphone_bus);

		register_property<GodotSpeech, int>("input_audio_sent_id", &GodotSpeech::input_audio_sent_id, 0);
		register_method("assign_voice_controller", &GodotSpeech::assign_voice_controller);
	}

	virtual PoolVector2Array decompress_buffer(const PoolByteArray p_read_byte_array, const int p_read_size, PoolVector2Array p_write_vec2_array) {
		if(p_read_byte_array.size() < p_read_size) {
			Godot::print_error("VoiceManager: read byte_array size!", __FUNCTION__, __FILE__, __LINE__);
			return PoolVector2Array();
		}

		if (mic_input_processor->decompress_buffer_internal(&p_read_byte_array, p_read_size, &p_write_vec2_array)) {
			return p_write_vec2_array;
		}

		return PoolVector2Array();
	}


	// Copys all the input buffers to the output buffers
	// Returns the amount of buffers
	Array copy_and_clear_buffers() {
		MutexLock mutex_lock(audio_mutex.ptr());
		
		Array array;
		array.resize(current_input_size);

		for (int i = 0; i < current_input_size; i++) {
			output_audio_buffer_array[i]["byte_array"] = input_audio_buffer_array[i].compressed_byte_array;
			output_audio_buffer_array[i]["buffer_size"] = input_audio_buffer_array[i].buffer_size;
			output_audio_buffer_array[i]["loudness"] = input_audio_buffer_array[i].loudness;

			array[i] = output_audio_buffer_array[i];
		}
		current_input_size = 0;

		return array;
	}

	bool start_recording() {
		if (mic_input_processor) {
			mic_input_processor->start();
			input_audio_sent_id = 0;
			return true;
		}

		return false;
	}

	void end_recording() {
		if (mic_input_processor) {
			mic_input_processor->stop();
			input_audio_sent_id = 0;
		}
		if(voice_controller) {
			if(voice_controller->has_method("clear_all_player_audio")) {
				voice_controller->call("clear_all_player_audio");
			}
		}
	}

	// TODO: replace this with a C++ class, must be assigned externally for now
	void assign_voice_controller(Node *p_voice_controller) {
		voice_controller = p_voice_controller;
	}

	void _init() {
		if (!Engine::get_singleton()->is_editor_hint()) {
			preallocate_buffers();
			mic_input_processor = VoiceManager::_new();
			audio_mutex.instance();
		}
	}

	void _ready() {
		if (!Engine::get_singleton()->is_editor_hint()) {
			setup_connections();

			add_child(mic_input_processor);
		}
	}

	void _notification(int p_what) {
		if (!Engine::get_singleton()->is_editor_hint()) {
			switch(p_what) {
				NOTIFICATION_EXIT_TREE:
					mic_input_processor->queue_free();
					break;
			}
		}
	}

	void set_streaming_bus(const String p_name) {
		if(mic_input_processor) {
			mic_input_processor->set_streaming_bus(p_name);
		}
	}

	void set_microphone_bus(const String p_name) {
		if(mic_input_processor) {
			mic_input_processor->set_microphone_bus(p_name);
		}
	}

	GodotSpeech() {};
	~GodotSpeech() {
	};
};

}; // namespace godot

#endif // GODOTSPEECH_HPP