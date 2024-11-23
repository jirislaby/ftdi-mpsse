/*
 * Licensed under the GPLv2
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ftdi_mpsse.h"
#include "internal.h"
#include "mpsse_reg.h"

/*
 * Synchronize the MPSSE interface by sending bad command 0xAA. The chip shall
 * respond with an echo command followed by bad command 0xAA. This will make
 * sure the MPSSE interface is enabled and synchronized successfully.
 */
static int ftdi_mpsse_synchronize(struct ftdi_mpsse *ftdi_mpsse)
{
	unsigned char ibuf[8];
	int ret;

	ftdi_mpsse_enqueue(ftdi_mpsse, CMD_ECHO1);
	ftdi_mpsse_enqueue(ftdi_mpsse, CMD_SEND_IMMEDIATE);
	ret = ftdi_mpsse_flush(ftdi_mpsse);
	if (ret < 0)
		return ret;

	unsigned int a, rd = 0;
	for (a = 0; a < 5; a++) {
		ret = ftdi_read_data(&ftdi_mpsse->ftdic, ibuf, sizeof(ibuf));
		if (ret < 0)
			return ftdi_mpsse_store_error(ftdi_mpsse, ret, true,
						      "ftdi_read_data(sync reply)");
		if (ret > 0) {
			rd = ret;
			break;
		}
	}
	if (!rd)
		return ftdi_mpsse_store_error(ftdi_mpsse, -1, false,
					      "cannot sync the chip (no data)");

	if (ftdi_mpsse->debug & MPSSE_VERBOSE) {
		fprintf(stderr, "%s: sync received %dB:", __func__, rd);
		for (unsigned a = 0; a < (unsigned)rd; a++)
			fprintf(stderr, " %02x", ibuf[a]);
		fprintf(stderr, "\n");
	}

	for (a = 0; a < rd - 1; a++) {
		if (ibuf[a] == CMD_INVALID && ibuf[a + 1] == CMD_ECHO1) {
			if (ftdi_mpsse->debug & MPSSE_VERBOSE)
				fprintf(stderr, "Chip synchronized\n");
			break;
		}
	}

	if (a >= rd)
		return ftdi_mpsse_store_error(ftdi_mpsse, -1, false,
					      "cannot sync the chip (invalid data)");

	return 0;
}

int ftdi_mpsse_init(struct ftdi_mpsse *ftdi_mpsse,
		    const struct ftdi_mpsse_config *conf)
{
	uint8_t ibuf[64];
	int ret;

	memset(ftdi_mpsse, 0, sizeof(*ftdi_mpsse));
	ftdi_mpsse->speed = conf->speed;
	ftdi_mpsse->debug = conf->debug;
	const char *debug = getenv("FTDI_MPSSE_DEBUG");
	if (debug)
		ftdi_mpsse->debug = strtol(debug, NULL, 0);

	ftdi_mpsse_set_gpio(ftdi_mpsse, conf->gpio);

	ret = ftdi_init(&ftdi_mpsse->ftdic);
	if (ret < 0)
		return ftdi_mpsse_store_error(ftdi_mpsse, ret, true, "ftdi_init");

	ftdi_set_interface(&ftdi_mpsse->ftdic, conf->iface);

	struct ftdi_device_list *devlist;
	ret = ftdi_usb_find_all(&ftdi_mpsse->ftdic, &devlist, conf->id_vendor, conf->id_product);
	if (ret < 0) {
		ftdi_mpsse_store_error(ftdi_mpsse, ret, true, "failed to find a device");
		goto deinit;
	}
	if (ret == 0) {
		ftdi_mpsse_store_error(ftdi_mpsse, -1, false,
				       "cannot find vendor (%.4x) and/or product (%.4x)",
				       conf->id_vendor, conf->id_product);
		ftdi_list_free(&devlist);
		goto deinit;
	}
	if (ret > 1 && (ftdi_mpsse->debug & MPSSE_VERBOSE))
		fprintf(stderr, "More than one device found, taking the first one\n");

	ret = ftdi_usb_open_dev(&ftdi_mpsse->ftdic, devlist->dev);
	ftdi_list_free(&devlist);
	if (ret < 0) {
		ftdi_mpsse_store_error(ftdi_mpsse, ret, true, "ftdi_usb_open");
		goto deinit;
	}

	if (ftdi_mpsse->debug & MPSSE_VERBOSE)
		fprintf(stderr, "Port opened, resetting device...\n");

	ret = ftdi_usb_reset(&ftdi_mpsse->ftdic);
	if (ret < 0) {
		ftdi_mpsse_store_error(ftdi_mpsse, ret, true, "ftdi_usb_reset");
		goto close;
	}

	ret = ftdi_tcioflush(&ftdi_mpsse->ftdic);
	if (ret < 0) {
		ftdi_mpsse_store_error(ftdi_mpsse, ret, true, "ftdi_tcioflush");
		goto close;
	}

	/* Set MPSSE mode */
	ftdi_set_bitmode(&ftdi_mpsse->ftdic, 0, BITMODE_RESET);
	ftdi_set_bitmode(&ftdi_mpsse->ftdic, conf->gpio_dir, BITMODE_MPSSE);

	/* the flush is not enough, there might be USB data */
	do {
		ret = ftdi_read_data(&ftdi_mpsse->ftdic, ibuf, sizeof(ibuf));
		if (ret < 0) {
			ftdi_mpsse_store_error(ftdi_mpsse, ret, true, "ftdi_read_data(emptying)");
			goto close;
		}

		if ((ftdi_mpsse->debug & MPSSE_VERBOSE) && ret) {
			fprintf(stderr, "%s: dropping stalled data (%dB)\n", __func__, ret);
			if (ftdi_mpsse->debug & MPSSE_DEBUG_READS) {
				for (unsigned a = 0; a < (unsigned)ret; a++)
					fprintf(stderr, " %02x", ibuf[a]);
				fprintf(stderr, "\n");
			}
		}
	} while (ret > 0);

	ret = ftdi_mpsse_synchronize(ftdi_mpsse);
	if (ret < 0)
		goto close;

	return 0;
close:
	ftdi_usb_close(&ftdi_mpsse->ftdic);
deinit:
	ftdi_deinit(&ftdi_mpsse->ftdic);
	return ret;
}

int ftdi_mpsse_read_dev(struct ftdi_mpsse *ftdi_mpsse, uint8_t *ibuf, size_t size, size_t count,
			bool check_all)
{
	unsigned int to = 100;
	unsigned int rd = 0;

	if (!count)
		return 0;

	while (1) {
		int now_rd = ftdi_read_data(&ftdi_mpsse->ftdic, ibuf + rd, size - rd);
		if (now_rd < 0)
			return ftdi_mpsse_store_error(ftdi_mpsse, now_rd, true, "ftdi_read_data");

		if (now_rd == 0 && to < 90)
			fprintf(stderr, "no input (rd=%d, count=%zu), trying (%u)\n",
				rd, count, to);
		rd += now_rd;
		if (rd >= count)
			break;
		if (!to--)
			return ftdi_mpsse_store_error(ftdi_mpsse, -1, false, "TIMEOUT");

		if (!check_all)
			break;

		usleep(10000);
	}

	if (ftdi_mpsse->debug & MPSSE_DEBUG_READS) {
		fprintf(stderr, "%s: asked %zuB, received %uB (expected %zuB, c_a=%u):",
			__func__, size, rd, count, check_all);
		for (unsigned a = 0; a < rd; a++)
			fprintf(stderr, " %02x", ibuf[a]);
		fprintf(stderr, "\n");
	}

	return rd;
}

void ftdi_mpsse_set_speed(struct ftdi_mpsse *ftdi_mpsse, unsigned int speed, bool three_phase)
{
	unsigned int div = 60000000;
	unsigned int div_divisor = speed * 2;

	if (three_phase) {
		div *= 2;
		div_divisor *= 3;
	}

	div = div_round_up(div, div_divisor) - 1;

	if (div & ~0xffff)
		fprintf(stderr, "%s: divisor overflow: speed=%u 3phase=%u div=0x%x\n", __func__,
			speed, three_phase, div);

	if (ftdi_mpsse->debug & MPSSE_DEBUG_CLOCK)
		fprintf(stderr, "%s: speed=%u 3phase=%u -> divisor=%.4x\n", __func__,
			speed, three_phase, div);

	ftdi_mpsse_enqueue(ftdi_mpsse, CMD_SET_CLK_DIVISOR);
	ftdi_mpsse_enqueue(ftdi_mpsse, div & 0xff);
	ftdi_mpsse_enqueue(ftdi_mpsse, div >> 8);
}

int ftdi_mpsse_flush(struct ftdi_mpsse *ftdi_mpsse)
{
	if (ftdi_mpsse->debug & MPSSE_DEBUG_WRITES) {
		fprintf(stderr, "%s: sending %u:", __func__, ftdi_mpsse->obuf_cnt);
		for (unsigned a = 0; a < ftdi_mpsse->obuf_cnt; a++)
			fprintf(stderr, " %02x", ftdi_mpsse->obuf[a]);
		fprintf(stderr, "\n");
	}

	int ret = ftdi_write_data(&ftdi_mpsse->ftdic, ftdi_mpsse->obuf, ftdi_mpsse->obuf_cnt);
	if (ret != (int)ftdi_mpsse->obuf_cnt) {
		return ftdi_mpsse_store_error(ftdi_mpsse, ret < 0 ? ret : -1, ret < 0,
					      "%s: cannot write: ret (%d) != %u", __func__,
					      ret, ftdi_mpsse->obuf_cnt);
	}

	ftdi_mpsse->obuf_cnt = 0;

	return ret;
}

void ftdi_mpsse_set_pins(struct ftdi_mpsse *ftdi_mpsse, uint8_t bits,
			 uint8_t output)
{
	unsigned char val = (ftdi_mpsse->gpio << 4) | bits;
	unsigned char dir = 0xf0 | output;

	ftdi_mpsse_enqueue(ftdi_mpsse, CMD_SET_BITS_LOW);
	ftdi_mpsse_enqueue(ftdi_mpsse, val);
	ftdi_mpsse_enqueue(ftdi_mpsse, dir);
}

void ftdi_mpsse_close(struct ftdi_mpsse *ftdi_mpsse)
{
	ftdi_usb_close(&ftdi_mpsse->ftdic);
	ftdi_deinit(&ftdi_mpsse->ftdic);
}
