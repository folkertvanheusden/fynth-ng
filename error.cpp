#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

void error_exit(const bool se, const char *const format, ...)
{
	int     e = errno;
	va_list ap;

	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);

	fprintf(stderr, "\n");

	if (se)
		fprintf(stderr, "\nerrno: %d (%s)\n", e, strerror(e));

	exit(EXIT_FAILURE);
}
