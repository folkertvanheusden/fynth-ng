#include <math.h>

#include "sample.h"
#include "sound.h"


double f_to_delta_t(const double frequency, const int sample_rate)
{
	return 2 * M_PI * frequency / sample_rate;
}

void on_process_audio(void *userdata)
{
	sound_parameters *sp = reinterpret_cast<sound_parameters *>(userdata);

	pw_buffer *b = pw_stream_dequeue_buffer(sp->pw.stream);

	if (b == nullptr) {
		printf("pw_stream_dequeue_buffer failed\n");
		pw_log_warn("out of buffers: %m");

		return;
	}

	spa_buffer *buf = b->buffer;

	int stride      = sizeof(double) * sp->n_channels;
	int period_size = std::min(buf->datas[0].maxsize / stride, uint32_t(sp->sample_rate / 200));

	double *temp_buffer = new double[sp->n_channels * period_size]();

	// printf("latency: %.2fms, channel count: %d\n", period_size * 1000.0 / sp->sample_rate, sp->n_channels);

	{
		std::shared_lock lck(sp->sounds_lock);

		for(int t=0; t<period_size; t++) {
			double *current_sample_base = &temp_buffer[t * sp->n_channels];

			for(auto & sound : sp->sounds) {
				size_t n_source_channels = sound.second->get_n_channels();

				for(size_t ch=0; ch<n_source_channels; ch++) {
					auto rc = sound.second->get_sample(ch);

					double value = rc.first;

					for(auto mapping : rc.second)
						current_sample_base[mapping.first] += value * mapping.second;
				}

				sound.second->tick();
			}
		}
	}

	double *dest = reinterpret_cast<double *>(buf->datas[0].data);

	if (dest) {
		std::unique_lock lck(sp->filters_lock);

		for(int t=0; t<period_size; t++) {
			double *current_sample_base_in = &temp_buffer[t * sp->n_channels];
			double *current_sample_base_out = &dest[t * sp->n_channels];

			if (sp->clipping == sound_parameters::C_ATAN) {
				for(int c=0; c<sp->n_channels; c++)
					current_sample_base_out[c] = atan(sp->lp_filters[c]->apply(current_sample_base_in[c])) / M_PI;
			}
			else if (sp->clipping == sound_parameters::C_FACTOR) {
				for(int c=0; c<sp->n_channels; c++) {
					current_sample_base_out[c] = sp->lp_filters[c]->apply(current_sample_base_in[c]) * sp->global_volume;

					if (current_sample_base_out[c] < -1.)
						current_sample_base_out[c] = -1.;
					else if (current_sample_base_out[c] > 1.)
						current_sample_base_out[c] = 1.;
				}
			}
		}

		lck.unlock();

		buf->datas[0].chunk->offset = 0;
		buf->datas[0].chunk->stride = stride;
		buf->datas[0].chunk->size   = period_size * stride;

		if (pw_stream_queue_buffer(sp->pw.stream, b))
			printf("pw_stream_queue_buffer failed\n");
	}
	else {
		printf("no buffer\n");
	}

	delete [] temp_buffer;
}

sound_sample::sound_sample(const int sample_rate, const std::string & filename) :
	sound(sample_rate, sample_rate / 2)
{
	unsigned sample_sample_rate = 0;

	auto rc = load_sample(filename);

	samples = rc.first;
	sample_sample_rate = rc.second;

	delta_t = sample_sample_rate / double(sample_rate);

	input_output_matrix.resize(samples->at(0).size());

	printf("Sample %s has %zu channel(s) and is sampled at %uHz\n", filename.c_str(), input_output_matrix.size(), sample_sample_rate);
}

std::pair<double, std::map<int, double> > sound_sample::get_sample(const size_t channel_nr)
{
	double use_t = t;

	if (use_t < 0)
		use_t += ceil(fabs(use_t) / samples->size()) * samples->size();

	size_t offset = fmod(use_t, samples->size());

	return { samples->at(offset).at(channel_nr), input_output_matrix[channel_nr] };
}

void end_notes(sound_parameters *const sp)
{
	bool have_any = false;

	for(;;) {
		std::unique_lock lck(sp->sounds_lock);

		if (have_any) {
			have_any = false;
		}
		else {
			sp->note_end_cv.wait(lck);
		}

		std::vector<std::pair<int, int> > to_erase;

		uint64_t ts_now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

		for(auto & sound : sp->sounds) {
			if (sound.second->get_has_ended_ts().has_value() == false)
				continue;

			uint64_t ts_end_start = sound.second->get_has_ended_ts().value();

			double volume_at_end_start = sound.second->get_volume_at_end_start();

			uint64_t ts_diff = ts_now - ts_end_start;

			double volume_steps = volume_at_end_start / 50;  // 50ms

			double new_volume = volume_at_end_start - ts_diff * volume_steps;

			if (new_volume <= 0.)
				to_erase.push_back(sound.first);
			else {
				sound.second->set_volume(new_volume);

				have_any = true;
			}
		}

		if (!to_erase.empty())
			printf("delete\n");

		for(auto & item : to_erase) {
			auto it = sp->sounds.find(item);

			if (it != sp->sounds.end()) {
				delete it->second;

				sp->sounds.erase(it);
			}
		}
	}
}
