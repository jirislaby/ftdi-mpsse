#ifndef UTILS_H
#define UTILS_H

#include <err.h>
#include <stdlib.h>

static inline void __check_err_or_exit(struct ftdi_mpsse *ftdi_mpsse, int error, const char *func,
				int line)
{
	if (error < 0)
		errx(EXIT_FAILURE, "%s (%d): %s\n", func, line, ftdi_mpsse_get_error(ftdi_mpsse));
}

#define check_err_or_exit(ftdi_mpsse, err) \
	__check_err_or_exit(ftdi_mpsse, err, __func__, __LINE__)

#endif
