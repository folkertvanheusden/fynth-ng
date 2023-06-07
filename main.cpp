#include <unistd.h>

#include "effects.h"
#include "pipewire.h"
#include "pipewire-audio.h"
#include "pipewire-midi.h"
#include "sound.h"


int main(int argc, char *argv[])
{
	int sample_rate = 44100;

	init_pipewire(&argc, &argv);

	sound_parameters sp_audio(sample_rate, 2, 22000);

	configure_pipewire_audio(&sp_audio);

	pipewire_data_midi sp_midi;
	configure_pipewire_midi(&sp_midi);

	sp_midi.sp = &sp_audio;

	std::thread th([&sp_audio] { end_notes(&sp_audio); });

	for(;;)
		usleep(10000);

	return 0;
}
