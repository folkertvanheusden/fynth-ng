#include <unistd.h>

#include "effects.h"
#include "pipewire.h"
#include "sound.h"


int main(int argc, char *argv[])
{
	int sample_rate = 44100;

	init_pipewire(&argc, &argv);

	sound_parameters sp;

	sound *sine = new sound(sample_rate, 440.);
	sine->add_mapping(0, 0, 1.0);  // mono -> left
	sine->add_mapping(0, 1, 1.0);  // mono -> right

	sp.sounds.insert({ { 0, 0 }, sine });

	sound *sample = new sound_sample(sample_rate, "the_niz.wav");
	sample->add_mapping(0, 0, 1.0);  // mono -> left
	sample->add_mapping(0, 1, 1.0);  // mono -> right

	sp.sounds.insert({ { 1, 0 }, sample });

	configure_pipewire(sample_rate, 16, 2, &sp);

	for(;;) {
		for(int i=0; i<360; i++) {
			{
				double x = sin(i * M_PI / 180);
				double y = cos(i * M_PI / 180);

				double left  = 0.;
				double right = 0.;
				double back  = 0.;

				encode_surround(1.0, x, y, &left, &right, &back);

				double vl = left - back;
				double vr = right + back;

				std::unique_lock lck(sp.sounds_lock);

				sine->set_volume(0, 0, vl);
				sine->set_volume(0, 1, vr);

				sine->set_pitch_bend(y - x);

				lck.unlock();
			}

			{
				double x = sin((i + 45) * M_PI / 180);
				double y = cos((i + 45) * M_PI / 180);

				double left  = 0.;
				double right = 0.;
				double back  = 0.;

				encode_surround(1.0, x, y, &left, &right, &back);

				double vl = left - back;
				double vr = right + back;

				std::unique_lock lck(sp.sounds_lock);

				sample->set_volume(0, 0, vl);
				sample->set_volume(0, 1, vr);

				sample->set_pitch_bend(y - x);

				lck.unlock();
			}

			usleep(10000);
		}
	}

	return 0;
}
