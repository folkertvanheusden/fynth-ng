#pragma once

#include <math.h>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

double f_to_delta_t(const double frequency, const int sample_rate);

class pipewire_data
{
public:
	std::thread       *th            { nullptr };

        pw_main_loop      *loop          { nullptr };
        pw_stream         *stream        { nullptr };
	spa_pod_builder    b;
        const spa_pod     *params[1]     { nullptr };
        uint8_t            buffer[1024]  { 0       };
	spa_audio_info_raw saiw          { SPA_AUDIO_FORMAT_UNKNOWN };
	pw_stream_events   stream_events { 0       };
};

class sound
{
private:
	double frequency { 100. };
	double amplitude { 0.   };

	double t         { 0.   };
	double delta_t   { 0.   };

public:
	sound(const int sample_rate, const double frequency, const double amplitude, const std::vector<int> channels) :
		frequency(frequency),
		amplitude(amplitude),
       		channels(channels) {
		delta_t = f_to_delta_t(frequency, sample_rate);
	}

	double tick() {
		double v = sin(t) * amplitude;

		t += delta_t;

		return v;
	}

	std::vector<int> channels;
};

class sound_parameters
{
public:
	int sample_rate     { 0 };
	int n_channels      { 0 };
	int bits_per_sample { 0 };

	pipewire_data pw;

	std::unique_lock<std::shared_mutex> sounds_lock;
	std::vector<sound> sounds;
};

void on_process(void *userdata);
