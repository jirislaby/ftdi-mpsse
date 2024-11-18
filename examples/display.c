/*
 * Licensed under the GPLv2
 *
 * LCD2004 behind PCF8574 expander (4bit communication): LCD display
 */
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <ftdi_mpsse.h>

#include "font8x8_basic.h"

#define RS		BIT(0)
#define READ		BIT(1)
#define E		BIT(2)
#define BACKLIGHT	BIT(3)
#define DATA(x)		((x) << 4)

static void send_pcf4(struct ftdi_mpsse *ftdi_mpsse, uint8_t rs_rw, uint8_t nibble)
{
	uint8_t to_send = rs_rw | DATA(nibble);

	//printf("%s: %.2lx\n", __func__, to_send | E);
	ftdi_i2c_send_check_ack(ftdi_mpsse, to_send | E);
	//printf("%s: %.2x\n", __func__, to_send);
	ftdi_i2c_send_check_ack(ftdi_mpsse, to_send);

	usleep(50);
}

void send_pcf(struct ftdi_mpsse *ftdi_mpsse, uint8_t rs_rw, uint8_t byte)
{
	uint8_t high = byte >> 4;
	uint8_t low = byte & 0xf;

	//printf("%s: rs_rw=%.2x byte=%.2x\n", __func__, rs_rw, byte);
	send_pcf4(ftdi_mpsse, rs_rw | BACKLIGHT, high);
	send_pcf4(ftdi_mpsse, rs_rw | BACKLIGHT, low);
}

static __attribute__((unused)) void read_pcf(struct ftdi_mpsse *ftdi_mpsse)
{
	uint8_t msb, lsb, wr = BACKLIGHT | READ | DATA(0b1111);

	ftdi_i2c_send_check_ack(ftdi_mpsse, wr);
	usleep(1);
	ftdi_i2c_send_check_ack(ftdi_mpsse, wr | E);
	ftdi_i2c_end(ftdi_mpsse);
	usleep(10);
	ftdi_i2c_begin(ftdi_mpsse, 0x27, false);
	ftdi_i2c_recv_send_ack(ftdi_mpsse, &msb, 1, true);
	ftdi_i2c_end(ftdi_mpsse);
	usleep(50);

	ftdi_i2c_begin(ftdi_mpsse, 0x27, true);
	ftdi_i2c_send_check_ack(ftdi_mpsse, wr);
	ftdi_i2c_end(ftdi_mpsse);
	usleep(1);
	ftdi_i2c_begin(ftdi_mpsse, 0x27, true);
	ftdi_i2c_send_check_ack(ftdi_mpsse, wr | E);
	ftdi_i2c_end(ftdi_mpsse);
	usleep(10);
	ftdi_i2c_begin(ftdi_mpsse, 0x27, false);
	ftdi_i2c_recv_send_ack(ftdi_mpsse, &lsb, 1, true);
	ftdi_i2c_begin(ftdi_mpsse, 0x27, true);
	usleep(10);
	//ftdi_i2c_send_check_ack(ftdi_mpsse, BACKLIGHT);
	printf("busy=%.8x\n", (msb & 0xf0) | (lsb >> 4));
}

int main()
{
	struct ftdi_mpsse ftdi_mpsse;
	int ret;
	struct ftdi_mpsse_config conf = {
		  .iface = INTERFACE_ANY,
		  .speed = FTDI_I2C_SPD_STD,
	};

	ret = ftdi_i2c_init(&ftdi_mpsse, &conf);
	if (ret < 0)
		errx(EXIT_FAILURE, "%s (%d): %s\n", __func__, __LINE__,
		     ftdi_mpsse_get_error(&ftdi_mpsse));

	ret = ftdi_i2c_begin(&ftdi_mpsse, 0x27, true);
	if (ret < 0)
		errx(EXIT_FAILURE, "%s (%d): %s\n", __func__, __LINE__,
		     ftdi_mpsse_get_error(&ftdi_mpsse));
	send_pcf4(&ftdi_mpsse, 0, 0b0011);
	usleep(5000);
	send_pcf4(&ftdi_mpsse, 0, 0b0011);
	send_pcf4(&ftdi_mpsse, 0, 0b0011);
	send_pcf4(&ftdi_mpsse, 0, 0b0010); /* 4bit */

	send_pcf(&ftdi_mpsse, 0, 0b00101000); /* 4bit, 2 lines */
	send_pcf(&ftdi_mpsse, 0, 0b00001000); /* OFF */
	send_pcf(&ftdi_mpsse, 0, 0b00000001);  // Clear display
	usleep(2000);                        // Wait 2ms for display to clear
	send_pcf(&ftdi_mpsse, 0, 0b00000110);  // Entry mode set: cursor moves right
	send_pcf(&ftdi_mpsse, 0, 0b00000010);  /* CUR HOME */
	send_pcf(&ftdi_mpsse, 0, 0b00001110); /* ON, CUR ON */

	for (unsigned i = 0; i < 10; i++) {
		//read_pcf(&ftdi_mpsse);
		send_pcf(&ftdi_mpsse, RS, 'A' + i);
	}
	send_pcf(&ftdi_mpsse, 0, 0b01000000 | 0);
	send_pcf(&ftdi_mpsse, RS, 0b01010);
	send_pcf(&ftdi_mpsse, RS, 0b00100);
	send_pcf(&ftdi_mpsse, RS, 0b01110);
	send_pcf(&ftdi_mpsse, RS, 0b10000);
	send_pcf(&ftdi_mpsse, RS, 0b10000);
	send_pcf(&ftdi_mpsse, RS, 0b10001);
	send_pcf(&ftdi_mpsse, RS, 0b01110);
	send_pcf(&ftdi_mpsse, RS, 0b00000);
	send_pcf(&ftdi_mpsse, 0, 0b10000000 | 40);
	send_pcf(&ftdi_mpsse, RS, 0);
	const char text[] = "asto";
	for (unsigned i = 0; i < sizeof(text) - 1; i++) {
		send_pcf(&ftdi_mpsse, RS, text[i]);
	}
	ftdi_i2c_end(&ftdi_mpsse);

	ftdi_i2c_close(&ftdi_mpsse);

	return 0;
}

