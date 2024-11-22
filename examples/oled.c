/*
 * Licensed under the GPLv2
 *
 * SSD1306: OLED display
 */
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <ftdi_mpsse.h>

#include "font8x8_basic.h"

int main(int argc, char **argv)
{
	struct ftdi_mpsse ftdi_mpsse;
	struct ftdi_mpsse_config conf = {
		  .iface = INTERFACE_ANY,
		  /* the specs say FAST (400 kHz), but HIGH (3.4 MHz) works for me */
		  .speed = FTDI_I2C_SPD_HIGH,
	};
	bool scroll = false;
	int ret;

	while ((ret = getopt(argc, argv, "s")) >= 0) {
		switch (ret) {
		case 's':
			scroll = true;
			break;
		case '?':
			return EXIT_FAILURE;
		}
	}


	ret = ftdi_i2c_init(&ftdi_mpsse, &conf);
	if (ret < 0)
		errx(EXIT_FAILURE, "%s (%d): %s\n", __func__, __LINE__,
		     ftdi_mpsse_get_error(&ftdi_mpsse));

	ret = ftdi_i2c_begin(&ftdi_mpsse, 0x3c, true);
	if (ret < 0)
		errx(EXIT_FAILURE, "%s (%d): %s\n", __func__, __LINE__,
		     ftdi_mpsse_get_error(&ftdi_mpsse));

	ftdi_i2c_send_check_ack(&ftdi_mpsse, 0x00); /* col start addr */

	ftdi_i2c_send_check_ack(&ftdi_mpsse, 0x8d); /* charge pump ON */
	ftdi_i2c_send_check_ack(&ftdi_mpsse, 0x14);

	ftdi_i2c_send_check_ack(&ftdi_mpsse, 0xaf); /* disp ON */

	ftdi_i2c_send_check_ack(&ftdi_mpsse, 0x20); /* memmode */
	ftdi_i2c_send_check_ack(&ftdi_mpsse, 0x00);
	ftdi_i2c_send_check_ack(&ftdi_mpsse, 0xa1); /* segremap */
	ftdi_i2c_send_check_ack(&ftdi_mpsse, 0xc8); /* COM SCAN */
	ftdi_i2c_send_check_ack(&ftdi_mpsse, 0xd3); /* DISP offset */
	ftdi_i2c_send_check_ack(&ftdi_mpsse, 0x00);
	ftdi_i2c_send_check_ack(&ftdi_mpsse, 0x40); /* start line */
	ftdi_i2c_send_check_ack(&ftdi_mpsse, 0x21); /* col start-stop */
	ftdi_i2c_send_check_ack(&ftdi_mpsse, 0x00);
	ftdi_i2c_send_check_ack(&ftdi_mpsse, 0x7f);
	ftdi_i2c_send_check_ack(&ftdi_mpsse, 0x22); /* pg start-stop */
	ftdi_i2c_send_check_ack(&ftdi_mpsse, 0x00);
	ftdi_i2c_send_check_ack(&ftdi_mpsse, 0x07);

	/* scroll dis */
	ftdi_i2c_send_check_ack(&ftdi_mpsse, 0x2e);

	/* fade */
	ftdi_i2c_send_check_ack(&ftdi_mpsse, 0x23);
	ftdi_i2c_send_check_ack(&ftdi_mpsse, (0b00 << 4) | 0b0000);
	ftdi_i2c_end(&ftdi_mpsse);

	ret = ftdi_i2c_begin(&ftdi_mpsse, 0x3c, true);
	if (ret < 0)
		errx(EXIT_FAILURE, "%s (%d): %s\n", __func__, __LINE__,
		     ftdi_mpsse_get_error(&ftdi_mpsse));
	ftdi_i2c_send_check_ack(&ftdi_mpsse, 0x40);
	srand(time(NULL));
	for (unsigned r = 0; r < 64 / 8; r++) {
		unsigned char pat = rand();
		for (unsigned a = 0; a < 128; a++)
			ftdi_i2c_enqueue_writebyte(&ftdi_mpsse, pat);
	}
	for (unsigned cnt = 0; cnt < 4; cnt++)
		for (unsigned let = 'A'; let < 'A' + 32; let++)
			for (unsigned col = 0; col < 8; col++) {
				unsigned char draw = 0;
				for (unsigned row = 0; row < 8; row++)
					draw |= !!(font8x8_basic[let + (cnt % 2) * 32][row] & BIT(col)) << row;
				ftdi_i2c_enqueue_writebyte(&ftdi_mpsse, draw);
			}
	ftdi_i2c_end(&ftdi_mpsse);

	if (scroll) {
		ret = ftdi_i2c_begin(&ftdi_mpsse, 0x3c, true);
		if (ret < 0)
			errx(EXIT_FAILURE, "%s (%d): %s\n", __func__, __LINE__,
			     ftdi_mpsse_get_error(&ftdi_mpsse));
		/* scroll */
		ftdi_i2c_send_check_ack(&ftdi_mpsse, 0x27);
		ftdi_i2c_send_check_ack(&ftdi_mpsse, 0x00);
		ftdi_i2c_send_check_ack(&ftdi_mpsse, 0b000);
		ftdi_i2c_send_check_ack(&ftdi_mpsse, 0b111);
		ftdi_i2c_send_check_ack(&ftdi_mpsse, 0b111);
		ftdi_i2c_send_check_ack(&ftdi_mpsse, 0x00);
		ftdi_i2c_send_check_ack(&ftdi_mpsse, 0x7f);
		/* scroll en */
		ftdi_i2c_send_check_ack(&ftdi_mpsse, 0x2f);
		ftdi_i2c_end(&ftdi_mpsse);
	}

	ftdi_i2c_close(&ftdi_mpsse);

	return 0;
}

