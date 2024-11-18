/*
 * Licensed under the GPLv2
 */
#include <err.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ftdi_mpsse.h>

#define max(x, y)	((x) < (y) ? (y) : (x))

static bool _strtol_and_check(unsigned int *val, const char *what, const char *from)
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

static void usage(const char *prgname)
{
	fprintf(stderr, "Usage: %s [-c <channel>] [-g <gpio_settings>] <commands>\n",
		prgname);
	fprintf(stderr, "\n");
	fprintf(stderr, "Commands:\n");
	fprintf(stderr, "\ta<address> -- set address to <address>\n");
	fprintf(stderr, "\tc -- commit stored W values below (multiwrite)\n");
	fprintf(stderr, "\tr<count> -- read <count> values \n");
	fprintf(stderr, "\tw<value> -- single write of <value>\n");
	fprintf(stderr, "\tW<value> -- store <value> to a buffer for committing later\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Example:\n");
	fprintf(stderr, "\ta0x57 W0x00 W0x00 c r128 a0x68 w0x00 r0x13\n");
	fprintf(stderr, "Translates into:\n");
	fprintf(stderr, "\twrite 0x00 and 0x00 to 0x57,\n");
	fprintf(stderr, "\tread 128 values from (previous) 0x57,\n");
	fprintf(stderr, "\twrite 0x00 to 0x68, and\n");
	fprintf(stderr, "\tread 0x13 values still from 0x68\n");
}

static void hex_dump(const char *head, const void *buf, unsigned int len)
{
	const uint8_t *buf8 = buf;

	printf(head);
	for (unsigned i = 0; i < len; i++) {
		if (!(i % 16))
			printf("\n  0x%.2x:", i);
		printf(" %.2x", buf8[i]);
	}
	puts("");
}

static bool i2c_read(struct ftdi_mpsse *ftdi_mpsse, uint8_t address,
		     unsigned int count)
{
	int ret = ftdi_i2c_begin(ftdi_mpsse, address, false);
	if (ret < 0) {
		warnx("%s (%d): %s\n", __func__, __LINE__, ftdi_mpsse_get_error(ftdi_mpsse));
		return false;
	}

	uint8_t buf[count];
	ret = ftdi_i2c_recv_send_ack(ftdi_mpsse, buf, sizeof(buf), true);
	if (ret < 0) {
		warnx("%s (%d): %s\n", __func__, __LINE__, ftdi_mpsse_get_error(ftdi_mpsse));
		return false;
	}

	hex_dump("Read:", buf, sizeof(buf));

	ret = ftdi_i2c_end(ftdi_mpsse);
	if (ret < 0) {
		warnx("%s (%d): %s\n", __func__, __LINE__, ftdi_mpsse_get_error(ftdi_mpsse));
		return false;
	}

	return true;
}

static bool i2c_write(struct ftdi_mpsse *ftdi_mpsse, uint8_t address,
		     uint8_t *buf, unsigned int count)
{
	int ret = ftdi_i2c_begin(ftdi_mpsse, address, true);
	if (ret < 0) {
		warnx("%s (%d): %s\n", __func__, __LINE__, ftdi_mpsse_get_error(ftdi_mpsse));
		return false;
	}

	hex_dump("Write:", buf, count);
	for (unsigned i = 0; i < count; i++) {
		ret = ftdi_i2c_send_check_ack(ftdi_mpsse, buf[i]);
		if (ret < 0) {
			warnx("%s (%d): %s\n", __func__, __LINE__,
			      ftdi_mpsse_get_error(ftdi_mpsse));
			return false;
		}
	}

	ret = ftdi_i2c_end(ftdi_mpsse);
	if (ret < 0) {
		warnx("%s (%d): %s\n", __func__, __LINE__, ftdi_mpsse_get_error(ftdi_mpsse));
		return false;
	}

	return true;
}

int main(int argc, char **argv)
{
	const struct option longopts[] = {
		{ "gpio", 1, NULL, 'g' },
		{ "gpio-dir", 1, NULL, 'G' },
		{ "interface", 1, NULL, 'i' },
		{ "loops-after-read-ack", 1, NULL, 'l' },
		{ "speed", 1, NULL, 's' },
		{ "verbose", 1, NULL, 'v' },
		{}
	};
	struct ftdi_mpsse ftdi_mpsse;
	struct ftdi_mpsse_config conf = {
		  .iface = INTERFACE_ANY,
		  .speed = FTDI_I2C_SPD_STD,
	};
	unsigned int address = 0;
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
		case 'l':
			unsigned int loops;

			if (!strtol_and_check(loops, optarg))
				return EXIT_FAILURE;
			conf.loops_after_read_ack = loops;
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

	if (verbose)
		printf("channel=%u gpio=0x%x speed=%u\n",
		       conf.iface, conf.gpio, conf.speed);

	ret = ftdi_i2c_init(&ftdi_mpsse, &conf);
	if (ret < 0)
		errx(EXIT_FAILURE, "%s (%d): %s\n", __func__, __LINE__,
		     ftdi_mpsse_get_error(&ftdi_mpsse));

	uint8_t *wbuf = NULL;
	unsigned int wbuf_size = 0;
	unsigned int wbuf_count = 0;

	for (int i = 0; i < argc; i++) {
		const char *cur = argv[i];
		switch (cur[0]) {
		case 'a':
			if (!strtol_and_check(address, cur + 1))
				return EXIT_FAILURE;
			break;
		case 'r':
			if (!address)
				errx(EXIT_FAILURE, "address not set yet at index %u", i);

			unsigned int count;

			if (!strtol_and_check(count, cur + 1))
				errx(EXIT_FAILURE, "at index %u", i);

			if (!i2c_read(&ftdi_mpsse, address, count))
				return EXIT_FAILURE;
			break;
		case 'W':
			if (!address)
				errx(EXIT_FAILURE, "address not set yet at index %u", i);

			unsigned int W_val;

			if (!strtol_and_check(W_val, cur + 1))
				errx(EXIT_FAILURE, "at index %u (\"%s\")", i, cur);

			if (wbuf_count >= wbuf_size) {
				wbuf_size = max(wbuf_size * 2, 16);
				wbuf = realloc(wbuf, wbuf_size);
				if (!wbuf)
					err(EXIT_FAILURE, "cannot allocate memory");
			}

			wbuf[wbuf_count++] = W_val;

			break;
		case 'c':
			if (!wbuf_count)
				errx(EXIT_FAILURE, "nothing to write yet at index %u", i);

			if (!i2c_write(&ftdi_mpsse, address, wbuf, wbuf_count))
				return EXIT_FAILURE;

			wbuf_count = 0;
			break;
		case 'w': {
			unsigned int w_val;
			if (!strtol_and_check(w_val, cur + 1))
				return EXIT_FAILURE;
			uint8_t val8 = w_val;
			if (!i2c_write(&ftdi_mpsse, address, &val8, 1))
				return EXIT_FAILURE;
			break;
		}
		default:
			  errx(EXIT_FAILURE, "invalid command \"%s\" at index %u",
			       cur, i);
		}

	}

	free(wbuf);

	ftdi_i2c_close(&ftdi_mpsse);

	return EXIT_SUCCESS;
}
