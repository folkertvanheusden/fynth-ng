#pragma once

#include <map>
#include <math.h>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include "pipewire.h"


double f_to_delta_t(const double frequency, const int sample_rate);

class sound
{
private:
	double frequency { 100. };
	double amplitude { 0.   };

	double pitchbend { 1.   };

	double t         { 0.   };
	double delta_t   { 0.   };

public:
	sound(const int sample_rate, const double frequency, const double amplitude, const std::vector<std::pair<int, double> > & channels) :
		frequency(frequency),
		amplitude(amplitude),
       		channels(channels) {
		delta_t = f_to_delta_t(frequency, sample_rate);
	}

	void set_pitch_bend(const double pb)
	{
		pitchbend = pb;
	}

	double tick() {
		double v = sin(t) * amplitude;

		t += delta_t * pitchbend;

		return v;
	}

	// channel, volume in that channel
	std::vector<std::pair<int, double> > channels;
};

class sound_parameters
{
public:
	int sample_rate     { 0 };
	int n_channels      { 0 };
	int bits_per_sample { 0 };

	pipewire_data pw;

	std::shared_mutex sounds_lock;
	// channel, note
	std::map<std::pair<int, int>, sound *> sounds;
};

void on_process(void *userdata);
