#pragma once

#include <algorithm>
#include <map>
#include <math.h>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <vector>

#include "pipewire.h"


double f_to_delta_t(const double frequency, const int sample_rate);

class sound
{
private:
	double frequency { 100. };

protected:
	double pitchbend { 1.   };

	double t         { 0.   };
	double delta_t   { 0.   };

	// input channel, { output channel, volume }
	std::map<int, std::map<int, double> > input_output_matrix;

public:
	sound(const int sample_rate, const double frequency) : frequency(frequency)
	{
		delta_t = f_to_delta_t(frequency, sample_rate);
	}

	void add_mapping(const int from, const int to, const double volume)
	{
		// note that 'from' is ignored here as this object has only 1 generator
		auto it = input_output_matrix.find(from);

		if (it == input_output_matrix.end())
		{
			std::map<int, double> entry;
			entry.insert({ to, volume });

			input_output_matrix.insert({ from, entry });
		}
		else
		{
			it->second.insert({ to, volume });
		}
	}

	void remove_mapping(const int from, const int to)
	{
		auto it = input_output_matrix.find(from);

		if (it == input_output_matrix.end())
			return;

		it->second.erase(to);
	}

	void set_pitch_bend(const double pb)
	{
		pitchbend = pb;
	}

	void set_volume(const int from, const int to, const double v)
	{
		auto it = input_output_matrix.find(from);

		it->second[to] = v;
	}

	double get_volume(const int from, const int to)
	{
		auto it = input_output_matrix.find(from);

		return it->second[to];
	}

	virtual size_t get_n_channels()
	{
		return 1;
	}

	// sample, output-channels
	virtual std::pair<double, std::map<int, double> > get_sample(const size_t channel_nr)
	{
		return { sin(t), input_output_matrix.find(int(channel_nr))->second };
	}

	void tick()
	{
		t += delta_t * pitchbend;
	}
};

class sound_sample : public sound
{
private:
	std::vector<std::vector<double> > samples;

public:
	sound_sample(const int sample_rate, const double frequency, const std::string & file_name);

	size_t get_n_channels() override
	{
		return samples.at(0).size();
	}

	std::pair<double, std::map<int, double> > get_sample(const size_t channel_nr) override;
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
