#include <pipewire/filter.h>
#include <pipewire/pipewire.h>
#include <spa/control/control.h>
#include <spa/pod/iter.h>

#include "pipewire-midi.h"


static float midi_note_to_frequency(const uint8_t note)
{
        return pow(2.0, (double(note) - 69.0) / 12.0) * 440.0;
}

static void on_process_midi(void *data, struct spa_io_position *position)
{
	pipewire_data_midi *pw_data = reinterpret_cast<pipewire_data_midi *>(data);

	pw_buffer *b = pw_filter_dequeue_buffer(pw_data->in_port);
        if (b == nullptr)
                return;

	spa_buffer *buf = b->buffer;
	spa_data *d = &buf->datas[0];

	if (d->data == nullptr)
		return;

	spa_pod *pod = reinterpret_cast<spa_pod *>(spa_pod_from_data(d->data, d->maxsize, d->chunk->offset, d->chunk->size));
	if (pod == nullptr)
		return;

	if (!spa_pod_is_sequence(pod))
                return;

	spa_pod_control *c = nullptr;
	SPA_POD_SEQUENCE_FOREACH((struct spa_pod_sequence *)pod, c) {
		if (c->type != SPA_CONTROL_Midi)
                        continue;

		uint8_t *data = reinterpret_cast<uint8_t *>(SPA_POD_BODY(&c->value));
		uint32_t size = SPA_POD_BODY_SIZE(&c->value);

		int cmd = data[0] & 0xf0;
		int ch  = data[0] & 0x0f;

		if (data[0] < 128) {
			cmd = pw_data->last_command & 0xf0;
			ch  = pw_data->last_command & 0x0f;
		}
		else {
			pw_data->last_command = data[0];
		}

		if (cmd == 0x80 || cmd == 0x90) {
			int note     = data[1];
			int velocity = data[2];

			double velocity_float = velocity / 127.;

			std::unique_lock lck(pw_data->sp->sounds_lock);

			auto it = pw_data->sp->sounds.find({ ch, note });

			if (velocity == 0) {
				if (it != pw_data->sp->sounds.end()) {
					delete it->second;

					pw_data->sp->sounds.erase(it);
				}
			}
			else {
				if (it == pw_data->sp->sounds.end()) {
					double frequency = midi_note_to_frequency(note);

					sound *sample = new sound_sine(pw_data->sp->sample_rate, frequency);
					sample->add_mapping(0, 0, velocity_float);  // mono -> left
					sample->add_mapping(0, 1, velocity_float);  // mono -> right

					pw_data->sp->sounds.insert({ { ch, note }, sample });
				}
				else {
					it->second->set_volume(velocity_float);
				}
			}
		}
	}

	pw_filter_queue_buffer(pw_data->in_port, b);
}

static const struct pw_filter_events filter_events = {
        PW_VERSION_FILTER_EVENTS,
        .process = on_process_midi,
};

void configure_pipewire_midi(pipewire_data_midi *const target)
{
	const char prog_name[] = "fynth-ng";

	target->th = new std::thread([prog_name, target]() {
			target->loop = pw_main_loop_new(nullptr);

			if (!target->loop)
				fprintf(stderr, "pw_main_loop_new failed\n");

			target->filter = pw_filter_new_simple(
					pw_main_loop_get_loop(target->loop),
					prog_name,
					pw_properties_new(
						PW_KEY_MEDIA_TYPE, "Midi",
						PW_KEY_MEDIA_CATEGORY, "Record",
						PW_KEY_MEDIA_ROLE, "DSP",
						nullptr),
					&filter_events,
					target);

			target->in_port = pw_filter_add_port(target->filter,
					PW_DIRECTION_INPUT,
					PW_FILTER_PORT_FLAG_MAP_BUFFERS,
					2048,
					pw_properties_new(
						PW_KEY_FORMAT_DSP, "8 bit raw midi",
						PW_KEY_PORT_NAME, "input",
						nullptr),
					nullptr, 0);

			if (pw_filter_connect(target->filter, PW_FILTER_FLAG_RT_PROCESS, NULL, 0) != 0)
				fprintf(stderr, "can't connect\n");

			pw_main_loop_run(target->loop);

			printf("pipewire thread terminating\n");
	});
}
