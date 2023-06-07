#pragma once

#include <thread>

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

class sound_parameters;

void configure_pipewire(sound_parameters *const target);
void init_pipewire(int *argc, char **argv[]);
