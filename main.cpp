#include <unistd.h>

#include "effects.h"
#include "pipewire.h"
#include "sound.h"


int main(int argc, char *argv[])
{
	int sample_rate = 44100;

	init_pipewire(&argc, &argv);

	sound_parameters sp;

	sound *sine = new sound(sample_rate, 440., 1., { { 0, 1. }, { 1, -1. } });

	sp.sounds.insert({ { 0, 0 }, sine });

	configure_pipewire(sample_rate, 16, 2, &sp);

	for(;;) {
		for(int i=0; i<360; i++) {
			double x = sin(i * M_PI / 180);
			double y = cos(i * M_PI / 180);

			double left  = 0.;
			double right = 0.;
			double back  = 0.;

			encode_surround(1.0, x, y, &left, &right, &back);

			double vl = left - back;
                        double vr = right + back;

			std::unique_lock lck(sp.sounds_lock);
			sine->channels[0].second = vl;
			sine->channels[1].second = vr;

			lck.unlock();

			usleep(10000);
		}
	}

	return 0;
}
