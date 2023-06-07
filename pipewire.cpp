#include <pipewire/pipewire.h>


void init_pipewire(int *argc, char **argv[])
{
	pw_log_set_level(SPA_LOG_LEVEL_TRACE);

        pw_init(argc, argv);

	printf("Compiled with libpipewire %s, linked with libpipewire %s\n", pw_get_headers_version(), pw_get_library_version());
}
