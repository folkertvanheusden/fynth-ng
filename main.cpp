#include <unistd.h>

#include "pipewire.h"
#include "sound.h"


int main(int argc, char *argv[])
{
	int sample_rate = 44100;

	init_pipewire(&argc, &argv);

	sound_parameters sp;

	sound test1(sample_rate, 440., 1., { 0, 1 });

	sp.sounds.push_back(test1);

	configure_pipewire(sample_rate, 16, 2, &sp);

	for(;;)
		sleep(1);

	return 0;
}
