#include <math.h>

#include "sound.h"


double f_to_delta_t(const double frequency, const int sample_rate)
{
	return 2 * M_PI * frequency / sample_rate;
}

void on_process(void *userdata)
{
	sound_parameters *sp = reinterpret_cast<sound_parameters *>(userdata);

	// TODO lock sp

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

	// printf("latency: %.2fms\n", period_size * 1000.0 / sp->sample_rate);

	try {
		for(int i=0; i<period_size; i++) {
			double *current_sample_base = &temp_buffer[i * sp->n_channels];

			for(auto & sound : sp->sounds)
			{
				double v = sound.tick();

				for(int channel : sound.channels) {
					if (channel < sp->n_channels)
						current_sample_base[channel] += v;
				}
			}
		}
	}
	catch(...) {
		printf("Exception\n");
	}

	// TODO unlock sp

again:
	double *dest = reinterpret_cast<double *>(buf->datas[0].data);

	if (dest) {
		memcpy(dest, temp_buffer, period_size * sp->n_channels * sizeof(double));

		buf->datas[0].chunk->offset = 0;
		buf->datas[0].chunk->stride = stride;
		buf->datas[0].chunk->size   = period_size * stride;

		if (pw_stream_queue_buffer(sp->pw.stream, b))
			printf("pw_stream_queue_buffer failed\n");
	}
	else
		printf("no buffer\n");

	delete [] temp_buffer;
}
