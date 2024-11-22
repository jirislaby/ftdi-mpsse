/*
 * Licensed under the GPLv2
 */
#ifndef INTERNAL_H
#define INTERNAL_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "ftdi_mpsse.h"

#if __GNUC__ >= 4
  #define __local	__attribute__((visibility("hidden")))
#else
  #define __local
#endif

#define ARRAY_SIZE(x)		(sizeof(x) / sizeof(*x))
#define div_round_up(n, div)	(((n) + (div) - 1) / (div))
#define min(x, y)		((x) < (y) ? (x) : (y))
#define max(x, y)		((x) > (y) ? (x) : (y))

static inline void ftdi_mpsse_enqueue(struct ftdi_mpsse *ftdi_mpsse, uint8_t c)
{
	if (ftdi_mpsse->obuf_cnt >= ARRAY_SIZE(ftdi_mpsse->obuf)) {
		fprintf(stderr, "%s (%d): buffer overflow\n", __func__, __LINE__);
		return;
	}
	ftdi_mpsse->obuf[ftdi_mpsse->obuf_cnt++] = c;
}

int __local ftdi_mpsse_store_error(struct ftdi_mpsse *ftdi_mpsse, int ret,
				   bool ftdi_error, const char *fmt, ...);

int __local ftdi_mpsse_init(struct ftdi_mpsse *ftdi_mpsse,
			    const struct ftdi_mpsse_config *conf);
void __local ftdi_mpsse_set_speed(struct ftdi_mpsse *ftdi_mpsse, unsigned int speed,
				  bool three_phase);
int __local ftdi_mpsse_flush(struct ftdi_mpsse *ftdi_mpsse);
void __local ftdi_mpsse_set_pins(struct ftdi_mpsse *ftdi_mpsse, uint8_t bits,
				 uint8_t output);
void __local ftdi_mpsse_close(struct ftdi_mpsse *ftdi_mpsse);

#endif
