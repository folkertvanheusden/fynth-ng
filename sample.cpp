#include <map>
#include <sndfile.h>
#include <string>
#include <vector>

#include "error.h"


static std::map<std::string, std::pair<std::vector<std::vector<double> > *, unsigned int> > sample_cache;

std::pair<std::vector<std::vector<double> > *, unsigned int> load_sample(const std::string & filename)
{
	auto it = sample_cache.find(filename);
	if (it != sample_cache.end())
		return it->second;

        SF_INFO si = { 0 };
        SNDFILE *sh = sf_open(filename.c_str(), SFM_READ, &si);

	if (!sh)
		error_exit(true, "Cannot access file %s", filename.c_str());

	auto *samples = new std::vector<std::vector<double> >();

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

	sample_cache.insert({ filename, { samples, si.samplerate } });

	return { samples, si.samplerate };
}

void unload_sample_cache()
{
	for(auto & entry : sample_cache)
		delete [] entry.second.first;

	sample_cache.clear();
}
