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
#include "speech_processor.hpp"

namespace godot {

class GodotSpeech : public Node {
	GODOT_CLASS(GodotSpeech, Node)
	
	static const int MAX_AUDIO_BUFFER_ARRAY_SIZE = 10;
	
	PoolByteArray input_byte_array;
	float volume;

	Ref<Mutex> audio_mutex;

	int input_audio_sent_id = 0;

	Node *voice_controller; // TODO: rewrite this in C++
	SpeechProcessor *speech_processor = NULL;

	struct InputPacket {
		PoolByteArray compressed_byte_array;
		int buffer_size = 0;
		float loudness = 0.0;
	};

	int current_input_size = 0;
	PoolByteArray compression_output_byte_array;
	InputPacket input_audio_buffer_array[MAX_AUDIO_BUFFER_ARRAY_SIZE];
	//
private:
	// Assigns the memory to the fixed audio buffer arrays
	void preallocate_buffers() {
		input_byte_array.resize(SpeechProcessor::PCM_BUFFER_SIZE);
		compression_output_byte_array.resize(SpeechProcessor::PCM_BUFFER_SIZE);
		for (int i = 0; i < MAX_AUDIO_BUFFER_ARRAY_SIZE; i++) {
			input_audio_buffer_array[i].compressed_byte_array.resize(SpeechProcessor::PCM_BUFFER_SIZE);
		}
	}

	// Assigns a callback from the speech_processor to this object.
	void setup_connections() {
		if(speech_processor) {
			speech_processor->register_speech_processed(
				std::function<void(SpeechProcessor::SpeechInput *)>(
					std::bind(&GodotSpeech::speech_processed, this, std::placeholders::_1)
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
				SpeechProcessor::PCM_BUFFER_SIZE);

				input_audio_buffer_array[i-1].buffer_size = input_audio_buffer_array[i].buffer_size;
				input_audio_buffer_array[i-1].loudness = input_audio_buffer_array[i].loudness;
			}
			return &input_audio_buffer_array[MAX_AUDIO_BUFFER_ARRAY_SIZE-1];
		}
	}

	// Is responsible for recieving packets from the SpeechProcessor and then compressing them
	void speech_processed(SpeechProcessor::SpeechInput *p_mic_input) {
		// Copy the raw PCM data from the SpeechInput packet to the input byte array
		PoolByteArray *mic_input_byte_array = p_mic_input->pcm_byte_array;
		memcpy(input_byte_array.write().ptr(), mic_input_byte_array->read().ptr(), SpeechProcessor::PCM_BUFFER_SIZE);

		// Create a new SpeechProcessor::CompressedBufferInput to be passed into the compressor
		// and assign it the compressed_byte_array from the input packet
		SpeechProcessor::CompressedSpeechBuffer compressed_buffer_input;
		compressed_buffer_input.compressed_byte_array = &compression_output_byte_array;

		// Compress the packet
		speech_processor->compress_buffer_internal(&input_byte_array, &compressed_buffer_input);
		{
			// Lock
			MutexLock mutex_lock(audio_mutex.ptr());

			// Find the next valid input packet in the queue
			InputPacket *input_packet = get_next_valid_input_packet();
			// Copy the buffer size from the compressed_buffer_input back into the input packet
			memcpy(
				input_packet->compressed_byte_array.write().ptr(),
				compressed_buffer_input.compressed_byte_array->read().ptr(),
				SpeechProcessor::PCM_BUFFER_SIZE);

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

		register_method("get_speech_decoder", &GodotSpeech::get_speech_decoder);

		register_method("start_recording", &GodotSpeech::start_recording);
		register_method("end_recording", &GodotSpeech::end_recording);

		register_method("set_streaming_bus", &GodotSpeech::set_streaming_bus);
		register_method("set_microphone_bus", &GodotSpeech::set_microphone_bus);

		register_property<GodotSpeech, int>("input_audio_sent_id", &GodotSpeech::input_audio_sent_id, 0);
		register_method("assign_voice_controller", &GodotSpeech::assign_voice_controller);
	}

	virtual PoolVector2Array decompress_buffer(Ref<SpeechDecoder> p_speech_decoder, const PoolByteArray p_read_byte_array, const int p_read_size, PoolVector2Array p_write_vec2_array) {
		if(p_read_byte_array.size() < p_read_size) {
			Godot::print_error("PoolVector2Array: read byte_array size!", __FUNCTION__, __FILE__, __LINE__);
			return PoolVector2Array();
		}

		if (speech_processor->decompress_buffer_internal(p_speech_decoder.ptr(), &p_read_byte_array, p_read_size, &p_write_vec2_array)) {
			return p_write_vec2_array;
		}

		return PoolVector2Array();
	}


	// Copys all the input buffers to the output buffers
	// Returns the amount of buffers
	Array copy_and_clear_buffers() {
		MutexLock mutex_lock(audio_mutex.ptr());
		
		Array output_array;
		output_array.resize(current_input_size);

		for (int i = 0; i < current_input_size; i++) {
			Dictionary dict;

			dict["byte_array"] = input_audio_buffer_array[i].compressed_byte_array;
			dict["buffer_size"] = input_audio_buffer_array[i].buffer_size;
			dict["loudness"] = input_audio_buffer_array[i].loudness;

			output_array[i] = dict;
		}
		current_input_size = 0;

		return output_array;
	}

	Ref<SpeechDecoder> get_speech_decoder() {
		if(speech_processor) {
			return speech_processor->get_speech_decoder();
		} else {
			return NULL;
		}
	}

	bool start_recording() {
		if (speech_processor) {
			speech_processor->start();
			input_audio_sent_id = 0;
			return true;
		}

		return false;
	}

	void end_recording() {
		if (speech_processor) {
			speech_processor->stop();
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
			speech_processor = SpeechProcessor::_new();
			audio_mutex.instance();
		}
	}

	void _ready() {
		if (!Engine::get_singleton()->is_editor_hint()) {
			setup_connections();

			add_child(speech_processor);
		}
	}

	void _notification(int p_what) {
		if (!Engine::get_singleton()->is_editor_hint()) {
			switch(p_what) {
				NOTIFICATION_EXIT_TREE:
					speech_processor->queue_free();
					break;
			}
		}
	}

	void set_streaming_bus(const String p_name) {
		if(speech_processor) {
			speech_processor->set_streaming_bus(p_name);
		}
	}

	void set_microphone_bus(const String p_name) {
		if(speech_processor) {
			speech_processor->set_microphone_bus(p_name);
		}
	}

	GodotSpeech() {};
	~GodotSpeech() {
	};
};

}; // namespace godot

#endif // GODOTSPEECH_HPP