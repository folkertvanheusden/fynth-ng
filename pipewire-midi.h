#pragma once

#include <thread>

#include <pipewire/filter.h>
#include <pipewire/pipewire.h>

#include "sound.h"


class pipewire_data_midi
{
public:
	std::thread       *th            { nullptr };

	sound_parameters  *sp            { nullptr };

        pw_main_loop      *loop          { nullptr };
        pw_filter         *filter        { nullptr };
        void              *in_port       { nullptr };
        int64_t            clock_time    { 0       };

	uint8_t            last_command  { 0       };
};

void configure_pipewire_midi(pipewire_data_midi *const target);
