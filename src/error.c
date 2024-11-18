/*
 * Licensed under the GPLv2
 */
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "internal.h"

int ftdi_mpsse_store_error(struct ftdi_mpsse *ftdi_mpsse, int error,
			   bool ftdi_error, const char *fmt, ...)
{
	size_t len, sz = sizeof(ftdi_mpsse->error_buf);
	va_list va;

	va_start(va, fmt);
	vsnprintf(ftdi_mpsse->error_buf, sz, fmt, va);
	va_end(va);

	ftdi_mpsse->error_buf[sz - 1] = 0;
	len = strlen(ftdi_mpsse->error_buf);

	snprintf(ftdi_mpsse->error_buf + len, sz - len, " (error=%d)", error);
	ftdi_mpsse->error_buf[sz - 1] = 0;
	len = strlen(ftdi_mpsse->error_buf);
	if (ftdi_error) {
		snprintf(ftdi_mpsse->error_buf + len, sz - len, ": %s",
			 ftdi_get_error_string(&ftdi_mpsse->ftdic));
		ftdi_mpsse->error_buf[sz - 1] = 0;
	}

	return error;
}
