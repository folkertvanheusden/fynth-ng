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

	// channel, list of settings for that channel
	class midi_setting {
	public:
		uint8_t group;  // continuous controller usually
		uint8_t sub_group;  // for continuous controller, e.g. pitch bend does not have this
		int value;  // can be e.g. 14b for pitch bend
	};

	// ch, sub, value
	std::map<int, std::map<uint8_t, int> > settings_continuous_controller;
};

void configure_pipewire_midi(pipewire_data_midi *const target);
