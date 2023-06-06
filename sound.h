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
	std::vector<std::map<int, double> > input_output_matrix;

public:
	sound(const int sample_rate, const double frequency) : frequency(frequency)
	{
	}

	void add_mapping(const int from, const int to, const double volume)
	{
		// note that 'from' is ignored here as this object has only 1 generator
		input_output_matrix[from].insert({ to, volume });
	}

	void remove_mapping(const int from, const int to)
	{
		input_output_matrix[from].erase(to);
	}

	void set_pitch_bend(const double pb)
	{
		pitchbend = pb;
	}

	void set_volume(const int from, const int to, const double v)
	{
		input_output_matrix[from][to] = v;
	}

	double get_volume(const int from, const int to)
	{
		return input_output_matrix[from][to];
	}

	virtual size_t get_n_channels() = 0;

	// sample, output-channels
	virtual std::pair<double, std::map<int, double> > get_sample(const size_t channel_nr) = 0;

	void tick()
	{
		t += delta_t * pitchbend;
	}

	virtual std::string get_name() const = 0;
};

class sound_sine : public sound
{
public:
	sound_sine(const int sample_rate, const double frequency) : sound(sample_rate, frequency)
	{
		delta_t = f_to_delta_t(frequency, sample_rate);

		input_output_matrix.resize(1);
	}

	virtual size_t get_n_channels() override
	{
		return 1;
	}

	// sample, output-channels
	virtual std::pair<double, std::map<int, double> > get_sample(const size_t channel_nr) override
	{
		return { sin(t), input_output_matrix[channel_nr] };
	}

	std::string get_name() const override
	{
		return "sine";
	}
};

class sound_square_wave : public sound
{
public:
	sound_square_wave(const int sample_rate, const double frequency) : sound(sample_rate, frequency)
	{
		delta_t = f_to_delta_t(frequency, sample_rate);

		input_output_matrix.resize(1);
	}

	virtual size_t get_n_channels() override
	{
		return 1;
	}

	// sample, output-channels
	virtual std::pair<double, std::map<int, double> > get_sample(const size_t channel_nr) override
	{
		double v = sin(t);

		double v_out = v > 0 ? 1 : (v < 0 ? -1 : 0);

		return { v_out, input_output_matrix[channel_nr] };
	}

	std::string get_name() const override
	{
		return "square_wave";
	}
};

class sound_triangle : public sound
{
public:
	sound_triangle(const int sample_rate, const double frequency) : sound(sample_rate, frequency)
	{
		delta_t = f_to_delta_t(frequency, sample_rate);

		input_output_matrix.resize(1);
	}

	virtual size_t get_n_channels() override
	{
		return 1;
	}

	// sample, output-channels
	virtual std::pair<double, std::map<int, double> > get_sample(const size_t channel_nr) override
	{
		double v_out = (2 / M_PI) * asin(sin(t));

		return { v_out, input_output_matrix[channel_nr] };
	}

	std::string get_name() const override
	{
		return "triangle";
	}
};

class sound_sample : public sound
{
private:
	std::vector<std::vector<double> > samples;

public:
	sound_sample(const int sample_rate, const std::string & file_name);

	size_t get_n_channels() override
	{
		return samples.at(0).size();
	}

	std::pair<double, std::map<int, double> > get_sample(const size_t channel_nr) override;

	std::string get_name() const override
	{
		return "sample";
	}
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
