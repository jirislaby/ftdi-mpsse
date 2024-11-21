/*
 * Licensed under the GPLv2
 */
#ifndef FTDI_SPI_H
#define FTDI_SPI_H

#ifndef FTDI_MPSSE_H
#error include ftdi_mpsse.h instead
#endif

#include <stdint.h>

enum ftdi_spi_speed {
	FTDI_SPI_SPD_MIN	=      458,
	FTDI_SPI_SPD_MAX	= 30000000,
};

int ftdi_spi_init(struct ftdi_mpsse *ftdi_mpsse,
		  const struct ftdi_mpsse_config *conf);
int ftdi_spi_sendrecv(struct ftdi_mpsse *ftdi_mpsse, uint8_t *c);
int ftdi_spi_recv(struct ftdi_mpsse *ftdi_mpsse, uint8_t *c);
int ftdi_spi_send(struct ftdi_mpsse *ftdi_mpsse, uint8_t c);
void ftdi_spi_close(struct ftdi_mpsse *ftdi_mpsse);

#endif
