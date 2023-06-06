#include <sndfile.h>
#include <string>
#include <vector>

#include "error.h"


void load_sample(const std::string & filename, std::vector<std::vector<double> > *const samples, unsigned int *const sample_rate)
{
        SF_INFO si = { 0 };
        SNDFILE *sh = sf_open(filename.c_str(), SFM_READ, &si);

	if (!sh)
		error_exit(true, "Cannot access file %s", filename.c_str());

	*sample_rate = si.samplerate;

	samples->clear();

	constexpr int load_buffer_size = 4096;
	double *buffer = new double[load_buffer_size * si.channels];

	for(;;) {
		sf_count_t cur_n = sf_readf_double(sh, buffer, load_buffer_size);
		if (cur_n == 0)
			break;

		for(sf_count_t i=0; i<cur_n; i++) {
			int offset = i * si.channels;

			std::vector<double> row;

			for(int j=offset; j<offset + si.channels; j++)
				row.push_back(buffer[j]);

			samples->push_back(row);
		}
	}

	sf_close(sh);

	delete [] buffer;
}
