#include <math.h>
#include <unistd.h>

#include "pipewire.h"
#include "sound.h"


double f_to_delta_t(const double frequency, const int sample_rate)
{
	return 2 * M_PI * frequency / sample_rate;
}

int main(int argc, char *argv[])
{
	init_pipewire(&argc, &argv);

	sound_parameters sp;

	sound test { 440., 1., 0., f_to_delta_t(440., 44100), sound::ST_SIN, { 0, 1, 2 } };

	sp.sounds.push_back(test);

	configure_pipewire(44100, 16, 2, &sp);

	for(;;)
		sleep(1);

	return 0;
}
