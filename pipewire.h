#include "sound.h"

void configure_pipewire(const int sample_rate, const int bits_per_sample, const int n_channels, sound_parameters *const target);
void init_pipewire(int *argc, char **argv[]);
