/*
 * Licensed under the GPLv2
 */
#ifndef FTDI_I2C_H
#define FTDI_I2C_H

#ifndef FTDI_MPSSE_H
#error include ftdi_mpsse.h instead
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum ftdi_i2c_speed {
	FTDI_I2C_SPD_MIN	=      458 * 2 / 3,
	FTDI_I2C_SPD_STD	=   100000,
	FTDI_I2C_SPD_FAST	=   400000,
	FTDI_I2C_SPD_FASTP	=  1000000,
	FTDI_I2C_SPD_HIGH	=  3400000,
	FTDI_I2C_SPD_MAX	= 30000000 * 2 / 3,
};

int ftdi_i2c_init(struct ftdi_mpsse *ftdi_mpsse,
		  const struct ftdi_mpsse_config *conf);
void ftdi_i2c_close(struct ftdi_mpsse *ftdi_mpsse);

int ftdi_i2c_begin(struct ftdi_mpsse *ftdi_mpsse, uint8_t address,
		   bool write);
int ftdi_i2c_enqueue_writebyte(struct ftdi_mpsse *ftdi_mpsse, uint8_t c);
int ftdi_i2c_send_check_ack(struct ftdi_mpsse *ftdi_mpsse, uint8_t c);
int ftdi_i2c_recv_send_ack(struct ftdi_mpsse *ftdi_mpsse, uint8_t *buf,
			   size_t count, bool last_nack);
int ftdi_i2c_end(struct ftdi_mpsse *ftdi_mpsse);

#endif
