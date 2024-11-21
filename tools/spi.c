/*
 * Licensed under the GPLv2
 */
#include <err.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>

#include <ftdi_mpsse.h>

#include "utils.h"

static void usage(const char *prgname)
{
	fprintf(stderr, "Usage: %s [-c <channel>] [-g <gpio_settings>] <value>\n", prgname);
}

int main(int argc, char **argv)
{
	const struct option longopts[] = {
		{ "gpio", 1, NULL, 'g' },
		{ "gpio-dir", 1, NULL, 'G' },
		{ "interface", 1, NULL, 'i' },
		{ "speed", 1, NULL, 's' },
		{ "verbose", 1, NULL, 'v' },
		{}
	};
	struct ftdi_mpsse ftdi_mpsse;
	struct ftdi_mpsse_config conf = {
		  .iface = INTERFACE_ANY,
		  .speed = FTDI_I2C_SPD_STD,
	};
	bool verbose = false;
	const char *prgname = argv[0];
	int ret;

	while ((ret = getopt_long(argc, argv, "g:G:i:l:s:v", longopts, NULL)) >= 0) {
		switch (ret) {
		case 'g':
			unsigned int gpio;

			if (!strtol_and_check(gpio, optarg))
				return EXIT_FAILURE;
			conf.gpio = gpio;
			break;
		case 'G':
			unsigned int gpio_dir;

			if (!strtol_and_check(gpio_dir, optarg))
				return EXIT_FAILURE;
			conf.gpio_dir = gpio_dir;
			break;
		case 'i':
			unsigned int interface;

			if (!strtol_and_check(interface, optarg))
				return EXIT_FAILURE;
			conf.iface = interface;
			break;
		case 's':
			unsigned int speed;

			if (!strtol_and_check(speed, optarg))
				return EXIT_FAILURE;

			conf.speed = speed;
			break;
		case 'v':
			verbose = true;
			break;
		case -1:
			break;
		default:
			usage(prgname);
			return EXIT_FAILURE;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1) {
		usage(prgname);
		return EXIT_FAILURE;
	}

	unsigned int val;

	if (!strtol_and_check(val, argv[0]))
		return EXIT_FAILURE;

	if (verbose)
		printf("channel=%u gpio=0x%x speed=%u value=%.2x\n",
		       conf.iface, conf.gpio, conf.speed, val);

	ret = ftdi_spi_init(&ftdi_mpsse, &conf);
	if (ret < 0)
		errx(EXIT_FAILURE, "%s (%d): %s\n", __func__, __LINE__,
		     ftdi_mpsse_get_error(&ftdi_mpsse));

	uint8_t c = val;
	ret = ftdi_spi_sendrecv(&ftdi_mpsse, &c);
	if (ret < 0)
		errx(EXIT_FAILURE, "%s (%d): %s\n", __func__, __LINE__,
		     ftdi_mpsse_get_error(&ftdi_mpsse));

	ftdi_spi_close(&ftdi_mpsse);

	printf("Wrote: 0x%.2x\n", val);
	printf("Read:  0x%.2x\n", c);

	return EXIT_SUCCESS;
}
