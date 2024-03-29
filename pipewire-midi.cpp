#include <pipewire/filter.h>
#include <pipewire/pipewire.h>
#include <spa/control/control.h>
#include <spa/pod/iter.h>

#include "pipewire-midi.h"


constexpr int supersaw_width = 7;

static float midi_note_to_frequency(const uint8_t note)
{
        return pow(2.0, (double(note) - 69.0) / 12.0) * 440.0;
}

// this can be faster/more efficient
void update_continuous_controller_settings(pipewire_data_midi *const pw_data)
{
	for(auto & ch_settings : pw_data->settings_continuous_controller) {
		for(auto & sound : pw_data->sp->sounds) {
			if (sound.first.first != ch_settings.first)
				continue;

			for(auto & cs_sub : ch_settings.second) {
				for(auto & control : sound.second->get_controls()) {
					if (control.cm_mode == sound_control::cm_continuous_controller && control.cm_index == cs_sub.first) {
						printf("set %s to %d\n", control.name.c_str(), cs_sub.second);
						sound.second->set_control(control.index, cs_sub.second);
						break;
					}
				}
			}
		}
	}
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

		printf("[ %02x %02x %02x ]\n", data[0], data[1], data[2]);

		if (cmd == 0x80 || cmd == 0x90) {  // note on/off
			int note     = data[1];
			int velocity = data[2];

			double velocity_float = velocity / 127.;

			std::unique_lock lck(pw_data->sp->sounds_lock);

			auto it = pw_data->sp->sounds.find({ ch, note });

			if (velocity) {
				if (it == pw_data->sp->sounds.end()) {
					double frequency = midi_note_to_frequency(note);

					sound *sample { nullptr };

					int instrument = pw_data->instrument_selection[ch] % 6;  // sample & mandelsine are currently broken

					if (instrument == 0)
						sample = new sound_sine(pw_data->sp->sample_rate, frequency);
					else if (instrument == 1)
						sample = new sound_square_wave(pw_data->sp->sample_rate, frequency, supersaw_width, false);
					else if (instrument == 2)
						sample = new sound_pwm(pw_data->sp->sample_rate, frequency, supersaw_width, false);
					else if (instrument == 3)
						sample = new sound_triangle(pw_data->sp->sample_rate, frequency);
					else if (instrument == 4)
						sample = new sound_concave_triangle(pw_data->sp->sample_rate, frequency);
					else if (instrument == 5)
						sample = new sound_sample(pw_data->sp->sample_rate, "the_niz.wav");
					else if (instrument == 6)
						sample = new sound_mandelsine(pw_data->sp->sample_rate, frequency);
					else
						printf("internal error\n");

					sample->add_mapping(0, 0, velocity_float);  // mono -> left
					sample->add_mapping(0, 1, velocity_float);  // mono -> right

					printf("%s at %fHz\n", sample->get_name().c_str(), frequency);

					pw_data->sp->sounds.insert({ { ch, note }, sample });

					sample->set_pitch_bend(pw_data->pitch_bend[ch]);

					update_continuous_controller_settings(pw_data);
				}
				else {
					it->second->set_volume(velocity_float);

					it->second->unset_has_ended();
				}
			}

			if (velocity == 0 || cmd == 0x80) {
				if (it != pw_data->sp->sounds.end()) {
					it->second->set_has_ended();

					pw_data->sp->note_end_cv.notify_all();
				}
			}
		}
		else if (cmd == 0xb0) {
			printf("CONTROL 0xb0 %02x %02x\n", data[1], data[2]);
			std::unique_lock lck(pw_data->sp->sounds_lock);

			auto it = pw_data->settings_continuous_controller.find(ch);

			if (it == pw_data->settings_continuous_controller.end()) {
				pw_data->settings_continuous_controller.insert({ ch, { } });

				it = pw_data->settings_continuous_controller.find(ch);
			}

			auto it_sub = it->second.find(data[1]);

			if (it_sub == it->second.end())
				it->second.insert({ data[1], data[2] });
			else
				it_sub->second = data[2];

			update_continuous_controller_settings(pw_data);
		}
		else if (cmd == 0xc0) {  // select instrument
			pw_data->instrument_selection[ch] = data[1];

			printf("instrument nr %d\n", pw_data->instrument_selection[ch]);
		}
		else if (cmd == 0xe0) {  // pitch bend
			pw_data->pitch_bend[ch] = ((data[2] << 7) | (data[1])) / 8192.0;

			std::unique_lock lck(pw_data->sp->sounds_lock);

			for(auto & sound : pw_data->sp->sounds) {
				if (sound.first.first == ch)
					sound.second->set_pitch_bend(pw_data->pitch_bend[ch]);
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

	target->instrument_selection.resize(16);

	for(int i=0; i<16; i++)
		target->pitch_bend.push_back(1.);

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
