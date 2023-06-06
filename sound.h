#pragma once

#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

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
public:
	double frequency { 100. };
	double amplitude { 0.   };

	double t         { 0.   };
	double delta_t   { 0.   };

	enum { ST_SIN } type;

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
