#pragma once

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <map>
#include <math.h>
#include <mutex>
#include <optional>
#include <set>
#include <shared_mutex>
#include <vector>

#include "filter.h"
#include "pipewire-audio.h"


double f_to_delta_t(const double frequency, const int sample_rate);

class sound_control
{
public:
	// name
	enum { cm_continuous_controller } cm_mode;
	uint8_t cm_index;  // midi index (for 0xb0: data-1)
	int index;  // internal index
	std::string name;
	// how to transform a value
	double divide_by;
	double add;
	// last transformed value
	double current_setting;
};

class sound
{
protected:
	int    sample_rate { 44100 };
	double frequency { 100. };

	double pitchbend { 1.   };

	double t         { 0.   };
	double delta_t   { 0.   };

	std::optional<uint64_t> has_ended_ts;
	double volume_at_end_start { 0. };

	// input channel, { output channel, volume }
	std::vector<std::map<int, double> > input_output_matrix;

	std::vector<sound_control> controls;

public:
	sound(const int sample_rate, const double frequency) :
		sample_rate(sample_rate),
		frequency(frequency)
	{
	}

	std::optional<uint64_t> get_has_ended_ts()
	{
		return has_ended_ts;
	}	

	double get_volume_at_end_start()
	{
		return volume_at_end_start;
	}

	void set_has_ended()
	{
		has_ended_ts = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

		volume_at_end_start = get_avg_volume();
	}

	void unset_has_ended()
	{
		has_ended_ts.reset();
	}

	virtual std::vector<sound_control> get_controls()
	{
		return controls;
	}

	virtual void set_control(const int nr, const int value)
	{
		printf("set control %d to %d: ", nr, value);
		controls.at(nr).current_setting = value / controls.at(nr).divide_by + controls.at(nr).add;
		printf("%f\n", controls.at(nr).current_setting);
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

	void set_volume(const double v)
	{
		for(size_t from=0; from<input_output_matrix.size(); from++) {
			for(auto & to: input_output_matrix.at(from))
				to.second = v;
		}
	}

	double get_avg_volume()
	{
		double v = 0;
		int n = 0;

		for(size_t from=0; from<input_output_matrix.size(); from++) {
			for(auto & to: input_output_matrix.at(from)) {
				v += to.second;
				n++;
			}
		}

		return v / n;
	}

	double get_volume(const int from, const int to)
	{
		return input_output_matrix[from][to];
	}

	virtual size_t get_n_channels() = 0;

	// sample, output-channels
	virtual std::pair<double, std::map<int, double> > get_sample(const size_t channel_nr) = 0;

	virtual void tick()
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
protected:
	const int n { 1 };

	const bool adjust_is_mul { false };

	std::vector<double> delta_t;
	std::vector<double> t;

	void update_delta_t()
	{
		double work_frequency = frequency;

		for(int i=0; i<n; i++) {
			delta_t.at(i) = f_to_delta_t(work_frequency, sample_rate);

			if (adjust_is_mul)
				work_frequency *= controls.at(0).current_setting * 3;
			else
				work_frequency += controls.at(0).current_setting * 3;
		}
	}

public:
	sound_square_wave(const int sample_rate, const double frequency, const int n, const bool adjust_is_mul) :
		sound(sample_rate, frequency),
		n(n),
		adjust_is_mul(adjust_is_mul)
	{
		controls = {
			{ sound_control::cm_continuous_controller, 0x0d, 0, "distance", 1, 0, 0 },
			{ sound_control::cm_continuous_controller, 0x0c, 1, "threshold", 127, 0, 0 }
		};

		delta_t.resize(n);

		t.resize(n);

		input_output_matrix.resize(1);

		update_delta_t();
	}

	virtual size_t get_n_channels() override
	{
		return 1;
	}

	virtual void set_control(const int nr, const int value) override
	{
		sound::set_control(nr, value);

		update_delta_t();
	}

	// sample, output-channels
	virtual std::pair<double, std::map<int, double> > get_sample(const size_t channel_nr) override
	{
		double th_plus = controls.at(1).current_setting;
		double th_min  = -th_plus;
		double max = 1. / n;
		double v_out = 0.;

		for(int i=0; i<n; i++) {
			double v = sin(t.at(i));

			v_out += v > th_plus ? max : (v < th_min ? -max : 0);
		}

		return { v_out, input_output_matrix[channel_nr] };
	}

	virtual void tick() override
	{
		for(int i=0; i<n; i++)
			t.at(i) += delta_t.at(i) * pitchbend;
	}

	std::string get_name() const override
	{
		return "square_wave";
	}
};

class sound_pwm : public sound_square_wave
{
private:
	std::vector<double> error;
	std::vector<bool> state;

public:
	sound_pwm(const int sample_rate, const double frequency, const int n, const bool adjust_is_mul) :
		sound_square_wave(sample_rate, frequency, n, false)
	{
		error.resize(n);
		state.resize(n);
	}

	virtual size_t get_n_channels() override
	{
		return 1;
	}

	// sample, output-channels
	virtual std::pair<double, std::map<int, double> > get_sample(const size_t channel_nr) override
	{
		double max = 1. / n;
		double v_out = 0.;

		for(int i=0; i<n; i++) {
			error.at(i) += delta_t.at(i) / 2.;

			while(error.at(i) >= 1.0) {
				error.at(i) -= 1.0;

				state.at(i) = !state.at(i);
			}

			v_out += state.at(i) ? max : -max;
		}

		return { v_out, input_output_matrix[channel_nr] };
	}

	std::string get_name() const override
	{
		return "pwm";
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

class sound_concave_triangle : public sound_triangle
{
public:
	sound_concave_triangle(const int sample_rate, const double frequency) : sound_triangle(sample_rate, frequency)
	{
	}

	virtual std::pair<double, std::map<int, double> > get_sample(const size_t channel_nr) override
	{
		double v_out = pow((2 / M_PI) * asin(sin(t)), 2.0);

		return { v_out, input_output_matrix[channel_nr] };
	}

	std::string get_name() const override
	{
		return "concave_triangle";
	}
};

class sound_mandelsine : public sound_sine
{
private:
	double xc { 0. };
	double yc { 0. };
	double x  { 0. };
	double y  { 0. };

public:
	sound_mandelsine(const int sample_rate, const double frequency) : sound_sine(sample_rate, frequency)
	{
		// do some trickery to make the frequency into an angle
		double normalized_frequency = frequency / 12544.;  // highest midi frequency is a tad lower than 12544

		xc = sin(normalized_frequency * M_PI) * 2. - 1.;
		yc = cos(normalized_frequency * M_PI) * 2. - 1.;

		controls = {
			{ sound_control::cm_continuous_controller, 0x0d, 0, "factor scaling", 64, -1, 1 }
		};
	}

	virtual std::pair<double, std::map<int, double> > get_sample(const size_t channel_nr) override
	{
		double xkw = x * x;
		double ykw = y * y;

		double factor = 1.0;

		if (xkw + ykw >= 4.0) {
			xkw = x = 0.;
			ykw = y = 0.;
		}
		else {
			factor = xkw > ykw ? ykw / xkw : xkw / ykw;
		}

		factor *= controls.at(0).current_setting;

		double temp = xkw - ykw + xc;
		y = 2. * x * y + yc;
		x = temp;

//		printf("%f %f | { %f,%f } %f,%f | %f\n", t, factor, xkw, ykw, x, y, cos(factor));

		double v_out = sin(t) * sin(pow(t, factor));

		return { v_out, input_output_matrix[channel_nr] };
	}

	std::string get_name() const override
	{
		return "mandelsine";
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
	sound_parameters(const int sample_rate, const int n_channels, const double lp_freq) :
       		sample_rate(sample_rate),
		n_channels(n_channels) {
		for(int i=0; i<n_channels; i++)
			lp_filters.push_back(new FilterButterworth(lp_freq, sample_rate, false, sqrt(2.)));
	}

	virtual ~sound_parameters() {
		for(auto & filter_instance : lp_filters)
			delete filter_instance;
	}

	int sample_rate     { 0 };
	int n_channels      { 0 };

	pipewire_data_audio pw;

	std::shared_mutex sounds_lock;
	// channel, note
	std::map<std::pair<int, int>, sound *> sounds;

	std::mutex filters_lock;
	std::vector<FilterButterworth *> lp_filters;

	std::condition_variable_any note_end_cv;

	// not one, to prevent clipping
	double global_volume { 0.25 };
	enum { C_FACTOR, C_ATAN  } clipping { C_FACTOR };
};

void on_process_audio(void *userdata);

void end_notes(sound_parameters *const sp);
