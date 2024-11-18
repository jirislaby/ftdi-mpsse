/*
 * Licensed under the GPLv2
 *
 * DS3231: RTC module
 */
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <ftdi_mpsse.h>

static unsigned int bcd2hex(unsigned int bcd)
{
	return ((bcd & 0xf0) >> 4) * 10 + (bcd & 0x0f);
}

static unsigned int hex2bcd(unsigned int hex)
{
	return ((hex / 10) << 4) | (hex % 10);
}

static const char * const days[] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

int main(int argc, char **argv)
{
	struct ftdi_mpsse ftdi_mpsse;
	struct ftdi_mpsse_config conf = {
		  .iface = INTERFACE_ANY,
		  .speed = FTDI_I2C_SPD_FAST,
	};
	uint8_t addr = 0x68;
	bool read_eeprom = false;
	bool write_eeprom = false;
	bool set_time = false;
	int ret;

	while ((ret = getopt(argc, argv, "a:rsw")) >= 0) {
		switch (ret) {
		case 'a':
			addr = strtoul(optarg, NULL, 0);
			break;
		case 'r':
			read_eeprom = true;
			break;
		case 's':
			set_time = true;
			break;
		case 'w':
			write_eeprom = true;
			break;
		case '?':
			return EXIT_FAILURE;
		}
	}

	ret = ftdi_i2c_init(&ftdi_mpsse, &conf);
	if (ret < 0)
		errx(EXIT_FAILURE, "%s (%d): %s\n", __func__, __LINE__,
		     ftdi_mpsse_get_error(&ftdi_mpsse));

	if (write_eeprom) {
		ret = ftdi_i2c_begin(&ftdi_mpsse, 0x57, true);
                if (ret < 0)
			errx(EXIT_FAILURE, "%s (%d): %s\n", __func__, __LINE__,
			     ftdi_mpsse_get_error(&ftdi_mpsse));

                ftdi_i2c_send_check_ack(&ftdi_mpsse, 0x00);
                ftdi_i2c_send_check_ack(&ftdi_mpsse, 0x00);

		for (unsigned i = 0; i < 32; i++)
			ftdi_i2c_send_check_ack(&ftdi_mpsse, i);

		ftdi_i2c_end(&ftdi_mpsse);
		puts("EEPROM written");
		usleep(30000);
	}

	if (read_eeprom) {
		ret = ftdi_i2c_begin(&ftdi_mpsse, 0x57, true);
                if (ret < 0)
			errx(EXIT_FAILURE, "%s (%d): %s\n", __func__, __LINE__,
			     ftdi_mpsse_get_error(&ftdi_mpsse));
                ftdi_i2c_send_check_ack(&ftdi_mpsse, 0x00);
                ftdi_i2c_send_check_ack(&ftdi_mpsse, 0x00);

		ret = ftdi_i2c_begin(&ftdi_mpsse, 0x57, false);
		if (ret < 0)
			errx(EXIT_FAILURE, "%s (%d): %s\n", __func__, __LINE__,
			     ftdi_mpsse_get_error(&ftdi_mpsse));

		uint8_t eeprom[128];
		ftdi_i2c_recv_send_ack(&ftdi_mpsse, eeprom, sizeof(eeprom), true);

		printf("EEPROM:");
		for (unsigned i = 0; i < sizeof(eeprom); i++) {
			if (!(i % 16))
			    printf("\n0x%.2x:", i);
			printf(" %.2x", eeprom[i]);
		}
		puts("");
		puts("");

		ftdi_i2c_end(&ftdi_mpsse);
	}


	if (set_time) {
		time_t now;
		time(&now);

		struct tm *tm = localtime(&now);

		ret = ftdi_i2c_begin(&ftdi_mpsse, addr, true);
		if (ret < 0)
			errx(EXIT_FAILURE, "%s (%d): %s\n", __func__, __LINE__,
			     ftdi_mpsse_get_error(&ftdi_mpsse));
		ftdi_i2c_send(&ftdi_mpsse, 0x00);
		ftdi_i2c_send(&ftdi_mpsse, hex2bcd(tm->tm_sec));
		ftdi_i2c_send(&ftdi_mpsse, hex2bcd(tm->tm_min));
		ftdi_i2c_send(&ftdi_mpsse, hex2bcd(tm->tm_hour));
		ftdi_i2c_send(&ftdi_mpsse, tm->tm_wday + 1);
		ftdi_i2c_send(&ftdi_mpsse, hex2bcd(tm->tm_mday));
		ftdi_i2c_send(&ftdi_mpsse, hex2bcd(tm->tm_mon + 1));
		ftdi_i2c_send(&ftdi_mpsse, hex2bcd(tm->tm_year % 100));
		ftdi_i2c_end(&ftdi_mpsse);
	}

	for (unsigned cnt = 0; cnt < 10; cnt++) {
		ret = ftdi_i2c_begin(&ftdi_mpsse, addr, true);
		if (ret < 0)
			errx(EXIT_FAILURE, "%s (%d): %s\n", __func__, __LINE__,
			     ftdi_mpsse_get_error(&ftdi_mpsse));
		ret = ftdi_i2c_send_check_ack(&ftdi_mpsse, 0x00);
		if (ret < 0)
			errx(EXIT_FAILURE, "%s (%d): %s\n", __func__, __LINE__,
			     ftdi_mpsse_get_error(&ftdi_mpsse));

		ret = ftdi_i2c_begin(&ftdi_mpsse, addr, false);
		if (ret < 0)
			errx(EXIT_FAILURE, "%s (%d): %s\n", __func__, __LINE__,
			     ftdi_mpsse_get_error(&ftdi_mpsse));
		uint8_t regs[0x13];
		ftdi_i2c_recv_send_ack(&ftdi_mpsse, regs, sizeof(regs), true);

		for (unsigned reg = 7; reg <= 0x10; reg++)
			printf("  [0x%.2x]=0x%.2x", reg, regs[reg]);
		puts("");

		printf("rtc: %2u. %2u. %4u (%s) %.2u:%.2u:%.2u %d.%uC\n",
		       bcd2hex(regs[4]), bcd2hex(regs[5] & 0b11111),
		       2000 + 100 * !!(regs[5] & 0b10000000) + bcd2hex(regs[6]),
		       days[regs[3] - 1], bcd2hex(regs[2] & 0b111111),
		       bcd2hex(regs[1]), bcd2hex(regs[0]),
		       (int)regs[0x11], 25 * (regs[0x12] >> 6));

		time_t now;
		time(&now);
		struct tm *tm = localtime(&now);
		printf("loc: %2u. %2u. %4u (%s) %.2u:%.2u:%.2u\n",
		       tm->tm_mday, tm->tm_mon + 1, 1900 + tm->tm_year,
		       days[tm->tm_wday],
		       tm->tm_hour, tm->tm_min, tm->tm_sec);

		ftdi_i2c_end(&ftdi_mpsse);
		sleep(1);
		puts("");
	}

	ftdi_i2c_close(&ftdi_mpsse);

	return 0;
}

