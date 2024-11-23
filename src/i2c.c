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

#define PIN_SCL		BIT(0)
#define PIN_SDA		BIT(1)

int ftdi_i2c_init(struct ftdi_mpsse *ftdi_mpsse,
		  const struct ftdi_mpsse_config *conf)
{
	int ret;

	if (conf->speed < FTDI_I2C_SPD_MIN || conf->speed > FTDI_I2C_SPD_MAX)
		return ftdi_mpsse_store_error(ftdi_mpsse, -1, false,
					      "invalid speed: %d <= %u <= %d", FTDI_I2C_SPD_MIN,
					      conf->speed, FTDI_I2C_SPD_MAX);

	ret = ftdi_mpsse_init(ftdi_mpsse, conf);
	if (ret < 0)
		return ret;

	ftdi_mpsse->i2c.loops_after_read_ack = conf->loops_after_read_ack;

	usleep(50000);

	ftdi_mpsse_enqueue(ftdi_mpsse, CMD_CLK_DIV5_DIS);
	ftdi_mpsse_enqueue(ftdi_mpsse, CMD_CLK_ADAPTIVE_DIS);
	ftdi_mpsse_enqueue(ftdi_mpsse, CMD_CLK_3PHASE_EN);
	/*
	 * this is recommended for i2c in the datasheet but breaks bme and oled
	 * (high speed transfers likely)
	 */
#if 0
	if (ftdi_mpsse->ftdic.type == TYPE_232H) {
		ftdi_mpsse_enqueue(ftdi_mpsse, CMD_DRIVE_ONLY_ZERO);
		ftdi_mpsse_enqueue(ftdi_mpsse, 0x03);
		ftdi_mpsse_enqueue(ftdi_mpsse, 0x00);
	}
#endif

	ret = ftdi_mpsse_flush(ftdi_mpsse);
	if (ret < 0)
		goto close;

	ftdi_mpsse_set_pins(ftdi_mpsse, PIN_SCL | PIN_SDA, PIN_SCL | PIN_SDA);

	ftdi_mpsse_set_speed(ftdi_mpsse, conf->speed, true);

	ftdi_mpsse_enqueue(ftdi_mpsse, CMD_LOOPBACK_DIS);

	ret = ftdi_mpsse_flush(ftdi_mpsse);
	if (ret < 0)
		goto close;

	return 0;

close:
	ftdi_mpsse_close(ftdi_mpsse);
	return ret;
}

#define FIRST_CYCLES	10
#define SECOND_CYCLES	20
#define STOP_CYCLES	10

static void ftdi_i2c_enqueue_start(struct ftdi_mpsse *ftdi_mpsse)
{
	unsigned int a;

	/* repeat to last for >= 600ns */
	for (a = 0; a < FIRST_CYCLES; a++)
		ftdi_mpsse_set_pins(ftdi_mpsse, PIN_SCL | PIN_SDA, PIN_SCL | PIN_SDA);

	for (a = 0; a < SECOND_CYCLES; a++)
		ftdi_mpsse_set_pins(ftdi_mpsse, PIN_SCL, PIN_SCL | PIN_SDA);

	ftdi_mpsse_set_pins(ftdi_mpsse, 0, PIN_SCL | PIN_SDA);
}

static void ftdi_i2c_enqueue_stop(struct ftdi_mpsse *ftdi_mpsse)
{
	unsigned int a;

	for (a = 0; a < STOP_CYCLES; a++)
		ftdi_mpsse_set_pins(ftdi_mpsse, 0, PIN_SCL | PIN_SDA);

	for (a = 0; a < STOP_CYCLES; a++)
		ftdi_mpsse_set_pins(ftdi_mpsse, PIN_SCL, PIN_SCL | PIN_SDA);

	for (a = 0; a < STOP_CYCLES; a++)
		ftdi_mpsse_set_pins(ftdi_mpsse, PIN_SCL | PIN_SDA, PIN_SCL | PIN_SDA);

	/* set to input mode so they are in tristate (high impedance) */
	ftdi_mpsse_set_pins(ftdi_mpsse, 0, 0);
}

int ftdi_i2c_begin(struct ftdi_mpsse *ftdi_mpsse, uint8_t address, bool write)
{
	if (address & 0x80) {
		return ftdi_mpsse_store_error(ftdi_mpsse, -1, false,
					      "wrong address (containing R/W bit?)");
	}

	ftdi_i2c_enqueue_start(ftdi_mpsse);
	ftdi_mpsse->i2c.address = address;

	return ftdi_i2c_send_check_ack(ftdi_mpsse, address << 1 | !write);
}

static int ftdi_i2c_check_ack(struct ftdi_mpsse *ftdi_mpsse, bool check_all)
{
	unsigned int acks = ftdi_mpsse->i2c.acks;
	uint8_t ibuf[acks + 16];
	int ret;

	ret = ftdi_mpsse_read_dev(ftdi_mpsse, ibuf, sizeof(ibuf), acks, check_all);
	if (ret <= 0)
		return ret;

	ftdi_mpsse->i2c.acks -= ret;

	for (int a = 0; a < ret; a++) {
		if (ibuf[a] & BIT(0)) {
			return ftdi_mpsse_store_error(ftdi_mpsse, -1, false,
						      "i2c-%x: received NACK at offset %u",
						      ftdi_mpsse->i2c.address, a);
		}
	}

	return 0;
}

static int ftdi_i2c_check_rx(struct ftdi_mpsse *ftdi_mpsse, uint8_t *ibuf, size_t size,
			     bool check_all)
{
	unsigned int bytes = ftdi_mpsse->i2c.bytes;
	int ret;

	ret = ftdi_mpsse_read_dev(ftdi_mpsse, ibuf, size, bytes, check_all);
	if (ret <= 0)
		return ret;

	ftdi_mpsse->i2c.bytes -= ret;

	return ret;
}

static int ftdi_i2c_check_bufs(struct ftdi_mpsse *ftdi_mpsse, uint8_t *ibuf, size_t size)
{
	if (ftdi_mpsse->i2c.acks + ftdi_mpsse->i2c.bytes < 3 * MPSSE_RX_BUFSIZE / 4 &&
	    ftdi_mpsse->obuf_cnt < 3 * MPSSE_TX_BUFSIZE / 4)
		return 0;

	if (ftdi_mpsse->debug & MPSSE_DEBUG_FLUSHING)
		fprintf(stderr, "%s: flushing acks=%u bytes=%u obuf_cnt=%u\n", __func__,
			ftdi_mpsse->i2c.acks, ftdi_mpsse->i2c.bytes, ftdi_mpsse->obuf_cnt);

	ftdi_mpsse_enqueue(ftdi_mpsse, CMD_SEND_IMMEDIATE);
	int ret = ftdi_mpsse_flush(ftdi_mpsse);
	if (ret < 0)
		return ret;

	ret = ftdi_i2c_check_ack(ftdi_mpsse, false);
	if (ret < 0)
		return ret;

	return ftdi_i2c_check_rx(ftdi_mpsse, ibuf, size, false);
}

int ftdi_i2c_enqueue_writebyte(struct ftdi_mpsse *ftdi_mpsse, uint8_t c)
{
	ftdi_mpsse_set_pins(ftdi_mpsse, PIN_SDA, PIN_SCL | PIN_SDA);

	ftdi_mpsse_enqueue(ftdi_mpsse, CMD(CMD_OUT_FALLING, CMD_BIT, CMD_MSB, CMD_OUT));
	ftdi_mpsse_enqueue(ftdi_mpsse, 0x07);
	ftdi_mpsse_enqueue(ftdi_mpsse, c);

	ftdi_mpsse_set_pins(ftdi_mpsse, 0, PIN_SCL);

	ftdi_mpsse_enqueue(ftdi_mpsse, CMD(CMD_IN_RISING, CMD_BIT, CMD_MSB, CMD_IN));
	/* len = 0 means 1 bit */
	ftdi_mpsse_enqueue(ftdi_mpsse, 0x00);
	ftdi_mpsse->i2c.acks++;

	return ftdi_i2c_check_bufs(ftdi_mpsse, NULL, 0);
}

int ftdi_i2c_send_check_ack(struct ftdi_mpsse *ftdi_mpsse, uint8_t c)
{
	int ret;

	ret = ftdi_i2c_enqueue_writebyte(ftdi_mpsse, c);
	if (ret < 0)
		return ret;

	//ftdi_mpsse_enqueue(ftdi_mpsse, CMD_SEND_IMMEDIATE);

	ret = ftdi_mpsse_flush(ftdi_mpsse);
	if (ret < 0)
		return ret;

	return ftdi_i2c_check_ack(ftdi_mpsse, true);
}

static void ftdi_i2c_enqueue_readbyte(struct ftdi_mpsse *ftdi_mpsse)
{
	ftdi_mpsse_set_pins(ftdi_mpsse, 0, PIN_SCL);

	ftdi_mpsse_enqueue(ftdi_mpsse, CMD(CMD_IN_RISING, CMD_BIT, CMD_MSB, CMD_IN));
	ftdi_mpsse_enqueue(ftdi_mpsse, 0x07);
	ftdi_mpsse->i2c.bytes++;
}

static void ftdi_i2c_enqueue_ack(struct ftdi_mpsse *ftdi_mpsse, bool ack)
{
	ftdi_mpsse_set_pins(ftdi_mpsse, 0, PIN_SCL | PIN_SDA);

	ftdi_mpsse_enqueue(ftdi_mpsse, CMD(CMD_OUT_FALLING, CMD_BIT, CMD_MSB, CMD_OUT));
	ftdi_mpsse_enqueue(ftdi_mpsse, 0x00);
	ftdi_mpsse_enqueue(ftdi_mpsse, ack ? 0x00 : 0x80);
}

int ftdi_i2c_recv_send_ack(struct ftdi_mpsse *ftdi_mpsse, uint8_t *buf,
			   size_t count, bool last_nack)
{
	unsigned int rd = 0;
	int ret;

	for (size_t i = 0; i < count; i++) {
		ftdi_i2c_enqueue_readbyte(ftdi_mpsse);

		bool nack = last_nack && i == count - 1;
		ftdi_i2c_enqueue_ack(ftdi_mpsse, !nack);

		/* wait a bit, some implementations are slow to catch up after an ACK */
		for (unsigned int a = 0; a < ftdi_mpsse->i2c.loops_after_read_ack; a++) {
			ftdi_mpsse_set_pins(ftdi_mpsse, PIN_SDA, PIN_SCL | PIN_SDA);
			ftdi_mpsse_set_pins(ftdi_mpsse, 0, PIN_SCL | PIN_SDA);
		}

		ret = ftdi_i2c_check_bufs(ftdi_mpsse, buf + rd, count - rd);
		if (ret < 0)
			return ret;
		rd += ret;
	}

	if (rd == count)
		return rd;

	ftdi_mpsse_enqueue(ftdi_mpsse, CMD_SEND_IMMEDIATE);

	ret = ftdi_mpsse_flush(ftdi_mpsse);
	if (ret < 0)
		return ret;

	return ftdi_i2c_check_rx(ftdi_mpsse, buf + rd, count - rd, true);
}

int ftdi_i2c_end(struct ftdi_mpsse *ftdi_mpsse)
{
	int ret;

	ftdi_i2c_enqueue_stop(ftdi_mpsse);

	ret = ftdi_mpsse_flush(ftdi_mpsse);
	if (ret < 0)
		return ret;

	ret = ftdi_i2c_check_ack(ftdi_mpsse, true);
	if (ret < 0)
		return ret;

	return 0;
}

void ftdi_i2c_close(struct ftdi_mpsse *ftdi_mpsse)
{
	ftdi_mpsse_close(ftdi_mpsse);
}
