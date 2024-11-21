/*
 * Licensed under the GPLv2
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <ftdi.h>

#include "ftdi_mpsse.h"
#include "internal.h"
#include "mpsse_reg.h"

#define PIN_SCLK		BIT(0)
#define PIN_MOSI		BIT(1)
#define PIN_MISO		BIT(2)
#define PIN_CS			BIT(3)

static void ftdi_spi_set_pins(struct ftdi_mpsse *ftdi_mpsse, bool cs)
{
	uint8_t cs_bit = cs ? PIN_CS : 0;

	ftdi_mpsse_set_pins(ftdi_mpsse, PIN_MOSI | cs_bit, PIN_SCLK | PIN_MOSI | PIN_CS);
}

int ftdi_spi_init(struct ftdi_mpsse *ftdi_mpsse,
		  const struct ftdi_mpsse_config *conf)
{
	int ret;

	if (conf->speed < FTDI_SPI_SPD_MIN || conf->speed > FTDI_SPI_SPD_MAX)
		return ftdi_mpsse_store_error(ftdi_mpsse, -1, false,
					      "invalid speed: %d <= %u <= %d", FTDI_SPI_SPD_MIN,
					      conf->speed, FTDI_SPI_SPD_MAX);

	ret = ftdi_mpsse_init(ftdi_mpsse, conf);
	if (ret < 0)
		return ret;

	usleep(50000);

	ftdi_mpsse_enqueue(ftdi_mpsse, CMD_CLK_DIV5_DIS);
	ftdi_mpsse_enqueue(ftdi_mpsse, CMD_CLK_ADAPTIVE_DIS);
	ftdi_mpsse_enqueue(ftdi_mpsse, CMD_CLK_3PHASE_DIS);
	if (ftdi_mpsse->ftdic.type == TYPE_232H) {
		ftdi_mpsse_enqueue(ftdi_mpsse, CMD_DRIVE_ONLY_ZERO);
		ftdi_mpsse_enqueue(ftdi_mpsse, 0x00);
		ftdi_mpsse_enqueue(ftdi_mpsse, 0x00);
	}

	ret = ftdi_mpsse_flush(ftdi_mpsse);
	if (ret <= 0) {
		ftdi_mpsse_store_error(ftdi_mpsse, -1, false, "cannot flush (3PHASE)");
		goto close;
	}

	ftdi_spi_set_pins(ftdi_mpsse, true);

	ftdi_mpsse_set_speed(ftdi_mpsse, conf->speed, false);

	ftdi_mpsse_enqueue(ftdi_mpsse, CMD_LOOPBACK_DIS);

	ret = ftdi_mpsse_flush(ftdi_mpsse);
	if (ret <= 0) {
		ftdi_mpsse_store_error(ftdi_mpsse, -1, false, "cannot flush (LOOPBACK)");
		goto close;
	}

	return 0;

close:
	ftdi_mpsse_close(ftdi_mpsse);
	return ret;
}

static int __ftdi_spi_sendrecv(struct ftdi_mpsse *ftdi_mpsse, uint8_t *c, uint8_t rw)
{
	unsigned int rise_fall = 0;

	if (rw & CMD_IN)
		rise_fall |= CMD_IN_RISING;
	if (rw & CMD_OUT)
		rise_fall |= CMD_OUT_FALLING;

	ftdi_spi_set_pins(ftdi_mpsse, false);

	ftdi_mpsse_enqueue(ftdi_mpsse, CMD(rise_fall, CMD_BIT, CMD_MSB, rw));
	ftdi_mpsse_enqueue(ftdi_mpsse, 0x07);
	if (rw & CMD_OUT)
		ftdi_mpsse_enqueue(ftdi_mpsse, *c);

	ftdi_spi_set_pins(ftdi_mpsse, true);

	if (rw & CMD_IN)
		ftdi_mpsse_enqueue(ftdi_mpsse, CMD_SEND_IMMEDIATE);

	int ret = ftdi_mpsse_flush(ftdi_mpsse);
	if (ret < 0)
		return ret;

	if (rw & CMD_IN) {
		ret = ftdi_mpsse_read_dev(ftdi_mpsse, c, 1, 1, true);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int ftdi_spi_sendrecv(struct ftdi_mpsse *ftdi_mpsse, uint8_t *c)
{
	return __ftdi_spi_sendrecv(ftdi_mpsse, c, CMD_IN | CMD_OUT);
}

int ftdi_spi_recv(struct ftdi_mpsse *ftdi_mpsse, uint8_t *c)
{
	return __ftdi_spi_sendrecv(ftdi_mpsse, c, CMD_IN);
}

int ftdi_spi_send(struct ftdi_mpsse *ftdi_mpsse, uint8_t c)
{
	return __ftdi_spi_sendrecv(ftdi_mpsse, &c, CMD_OUT);
}

void ftdi_spi_close(struct ftdi_mpsse *ftdi_mpsse)
{
	ftdi_mpsse_close(ftdi_mpsse);
}
