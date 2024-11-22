/*
 * Licensed under the GPLv2
 */
#ifndef FTDI_MPSSE_H
#define FTDI_MPSSE_H

#include <stdbool.h>
#include <stdint.h>

#include <ftdi.h>

#define BIT(x)		(1U << (x))

enum ftdi_mpsse_debug {
	MPSSE_VERBOSE		= BIT(0),
	MPSSE_DEBUG_READS	= BIT(1),
	MPSSE_DEBUG_WRITES	= BIT(2),
	MPSSE_DEBUG_ACKS	= BIT(3),
	MPSSE_DEBUG_CLOCK	= BIT(4),
	MPSSE_DEBUG_FLUSHING	= BIT(5),
};

struct ftdi_mpsse {
	struct ftdi_context ftdic;
	char error_buf[128];
	uint8_t obuf[2048];
	unsigned int obuf_cnt;
	unsigned int speed;
	unsigned int debug;
	uint8_t gpio;
	union {
		struct {
			unsigned int loops_after_read_ack;
			unsigned int acks;
			unsigned int bytes;
			uint8_t address;
		} i2c;
	};
};

struct ftdi_mpsse_config {
	enum ftdi_interface iface;
	uint16_t id_vendor;
	uint16_t id_product;
	unsigned int speed;
	unsigned int loops_after_read_ack;
	unsigned int debug;
	uint8_t gpio;
	uint8_t gpio_dir;
};

static inline const char *ftdi_mpsse_get_error(const struct ftdi_mpsse *ftdi_mpsse)
{
	return ftdi_mpsse->error_buf;
}

static inline void ftdi_mpsse_set_gpio(struct ftdi_mpsse *ftdi_mpsse, uint8_t gpio)
{
	ftdi_mpsse->gpio = gpio & 0xf0;
}

#include <ftdi_i2c.h>

#endif
