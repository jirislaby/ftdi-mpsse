/*
 * Licensed under the GPLv2
 */
#ifndef UTILS_H
#define UTILS_H

#include <err.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define max(x, y)	((x) < (y) ? (y) : (x))

static inline bool _strtol_and_check(unsigned int *val, const char *what, const char *from)
{
	unsigned int _val;
	char *eptr;
	size_t from_len = strlen(from);

	if (!from_len) {
		warnx("empty %s\n", what);
		return false;
	}

	_val = strtol(from, &eptr, 0);

	if (eptr != from + from_len) {
		warnx("invalid %s: \"%s\", parsing failed at: \"%s\"\n",
			what, from, eptr);
		return false;
	}

	*val = _val;

	return true;
}

#define strtol_and_check(what, from)	_strtol_and_check(&what, #what, from)

#endif
