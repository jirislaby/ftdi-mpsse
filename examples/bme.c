/*
 * Licensed under the GPLv2
 *
 * BME280: humidity & pressure sensor
 */
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <ftdi_mpsse.h>

#include "utils.h"

struct BME280_compensation {
	union {
		struct {
			uint16_t dig_T1; // 0x88 / 0x89 -- [7:0] / [15:8]
			int16_t dig_T2; // 0x8A / 0x8B -- [7:0] / [15:8]
			int16_t dig_T3; // 0x8C / 0x8D -- [7:0] / [15:8]
			uint16_t dig_P1; // 0x8E / 0x8F -- [7:0] / [15:8]
			int16_t dig_P2; // 0x90 / 0x91 -- [7:0] / [15:8]
			int16_t dig_P3; // 0x92 / 0x93 -- [7:0] / [15:8]
			int16_t dig_P4; // 0x94 / 0x95 -- [7:0] / [15:8]
			int16_t dig_P5; // 0x96 / 0x97 -- [7:0] / [15:8]
			int16_t dig_P6; // 0x98 / 0x99 -- [7:0] / [15:8]
			int16_t dig_P7; // 0x9A / 0x9B -- [7:0] / [15:8]
			int16_t dig_P8; // 0x9C / 0x9D -- [7:0] / [15:8]
			int16_t dig_P9; // 0x9E / 0x9F -- [7:0] / [15:8]
			uint8_t dig_H1; // 0xA1 -- [7:0]
			int16_t dig_H2; // 0xE1 / 0xE2 -- [7:0] / [15:8]
			uint8_t dig_H3; // 0xE3 -- [7:0]
			int16_t dig_H4; // 0xE4 / 0xE5[3:0] -- [11:4] / [3:0]
			int16_t dig_H5; // 0xE5[7:4] / 0xE6 -- [3:0] / [11:4]
			int8_t dig_H6; // 0xE7
			int t_fine;
		};
		struct {
			uint16_t reg_88_9f[12];
			uint8_t reg_a1;

		};
	};
};

static int BME280_get_compensation(struct ftdi_mpsse *ftdi_mpsse,
				    struct BME280_compensation *comp)
{
	uint8_t msb, lsb;
	int ret;

	for (unsigned i = 0; i < sizeof(comp->reg_88_9f) / sizeof(*comp->reg_88_9f); i++) {
		ftdi_i2c_recv_send_ack(ftdi_mpsse, &lsb, 1, false);
		ftdi_i2c_recv_send_ack(ftdi_mpsse, &msb, 1, false);
		comp->reg_88_9f[i] = (msb << 8U) | lsb;
	}

	ftdi_i2c_recv_send_ack(ftdi_mpsse, &msb, 1, false); // a0
	ftdi_i2c_recv_send_ack(ftdi_mpsse, &comp->reg_a1, 1, true);

	ret = ftdi_i2c_begin(ftdi_mpsse, 0x76, true);
	if (ret < 0)
		return ret;
	ftdi_i2c_send_check_ack(ftdi_mpsse, 0xe1);

	ret = ftdi_i2c_begin(ftdi_mpsse, 0x76, false);
	if (ret < 0)
		return ret;
	ftdi_i2c_recv_send_ack(ftdi_mpsse, &lsb, 1, false); // e1
	ftdi_i2c_recv_send_ack(ftdi_mpsse, &msb, 1, false); // e2
	comp->dig_H2 = (msb << 8U) | lsb;
	ftdi_i2c_recv_send_ack(ftdi_mpsse, &comp->dig_H3, 1, false); // e3
	ftdi_i2c_recv_send_ack(ftdi_mpsse, &lsb, 1, false); // e4
	ftdi_i2c_recv_send_ack(ftdi_mpsse, &msb, 1, false); // e5
	comp->dig_H4 = (lsb << 4U) | (msb & 0xf);
	ftdi_i2c_recv_send_ack(ftdi_mpsse, &lsb, 1, false); // e6
	comp->dig_H5 = (msb >> 4U) | (lsb << 4U);
	ftdi_i2c_recv_send_ack(ftdi_mpsse, &lsb, 1, true); // e7
	ftdi_i2c_end(ftdi_mpsse);

#ifdef DEBUG_COMP
	printf("T1=%u T2=%d T3=%d\n", comp->dig_T1, comp->dig_T2, comp->dig_T3);
	printf("P1=%u P2=%d P3=%d P4=%d P5=%d P6=%d P7=%d P8=%d P9=%d\n",
	       comp->dig_P1, comp->dig_P2, comp->dig_P3, comp->dig_P4,
	       comp->dig_P5, comp->dig_P6, comp->dig_P7, comp->dig_P8,
	       comp->dig_P9);
	printf("H1=%u H2=%d H3=%u H4=%d/0x%.2x H5=%d/0x%.2x H6=%d\n",
	       comp->dig_H1, comp->dig_H2, comp->dig_H3, comp->dig_H4,
	       comp->dig_H4, comp->dig_H5, comp->dig_H5, comp->dig_H6);
#endif

	return 0;
}

static double BME280_compensate_T(int adc_T, struct BME280_compensation *comp)
{
	double var1, var2;

	var1 = adc_T / 16384.0 - comp->dig_T1 / 1024.0;
	var1 *= comp->dig_T2;
	var2 = adc_T / 131072.0 - comp->dig_T1 / 8192.0;
	var2 *= var2 * comp->dig_T3;

	comp->t_fine = var1 + var2;

	return (var1 + var2) / 5120.0;
}

static double BME280_compensate_P(int adc_P, const struct BME280_compensation *comp)
{
	double var1, var2, p;

	var1 = comp->t_fine / 2.0 - 64000.0;

	var2 = var1 * var1 * comp->dig_P6 / 32768.0;
	var2 += var1 * comp->dig_P5 * 2.0;
	var2 /= 4.0;
	var2 += comp->dig_P4 * 65536.0;

	var1 = comp->dig_P3 * var1 * var1 / 524288.0 + comp->dig_P2 * var1;
	var1 /= 524288.0;
	var1 /= 32768.0;
	var1 += 1.0;
	var1 *= comp->dig_P1;
	if (var1 == 0.0)
		return 0;

	p = 1048576.0 - adc_P;
	p = (p - var2 / 4096.0) * 6250.0 / var1;
	var1 = comp->dig_P9 * p * p / 2147483648.0;
	var2 = p * comp->dig_P8 / 32768.0;
	p += (var1 + var2 + comp->dig_P7) / 16.0;

	return p;
}

static double BME280_compensate_H(int adc_H, const struct BME280_compensation *comp)
{
	double var_H = comp->t_fine - 76800.0;

	var_H = (adc_H - (comp->dig_H4 * 64.0 + comp->dig_H5 / 16384.0 * var_H)) *
		(comp->dig_H2 / 65536.0 * (1.0 + comp->dig_H6 / 67108864.0 * var_H * (1.0 + comp->dig_H3 / 67108864.0 * var_H)));
	var_H *= 1.0 - comp->dig_H1 * var_H / 524288.0;

	if (var_H > 100.0)
		return 100.0;
	if (var_H < 0.0)
		return 0.0;

	return var_H;
}

int main()
{
	struct ftdi_mpsse ftdi_mpsse;
	struct ftdi_mpsse_config conf = {
		  .iface = INTERFACE_ANY,
		  /*
		   * HIGH (3.4 MHz) is supposed to work, but does not
		   * (perhaps due to ACK stretching not implemented)
		   */
		  /* .speed = FTDI_I2C_SPD_HIGH, */
		  .speed = FTDI_I2C_SPD_HIGH / 2,
	};
	uint8_t val;
	int ret;

	ret = ftdi_i2c_init(&ftdi_mpsse, &conf);
	if (ret < 0)
		errx(EXIT_FAILURE, "%s (%d): %s\n", __func__, __LINE__,
			ftdi_mpsse_get_error(&ftdi_mpsse));

	check_err_or_exit(&ftdi_mpsse, ftdi_i2c_begin(&ftdi_mpsse, 0x76, true));
	check_err_or_exit(&ftdi_mpsse, ftdi_i2c_send_check_ack(&ftdi_mpsse, 0xd0)); // ID
	check_err_or_exit(&ftdi_mpsse, ftdi_i2c_begin(&ftdi_mpsse, 0x76, false));
	check_err_or_exit(&ftdi_mpsse, ftdi_i2c_recv_send_ack(&ftdi_mpsse, &val, 1, true));
	printf("id=%.2x\n", val);
	check_err_or_exit(&ftdi_mpsse, ftdi_i2c_end(&ftdi_mpsse));
	if (val != 0x60)
		return EXIT_FAILURE;

	check_err_or_exit(&ftdi_mpsse, ftdi_i2c_begin(&ftdi_mpsse, 0x76, true));
	check_err_or_exit(&ftdi_mpsse, ftdi_i2c_send_check_ack(&ftdi_mpsse, 0x88)); // CALIB
	check_err_or_exit(&ftdi_mpsse, ftdi_i2c_begin(&ftdi_mpsse, 0x76, false));
	struct BME280_compensation comp;
	check_err_or_exit(&ftdi_mpsse, BME280_get_compensation(&ftdi_mpsse, &comp));

	check_err_or_exit(&ftdi_mpsse, ftdi_i2c_begin(&ftdi_mpsse, 0x76, true));
	check_err_or_exit(&ftdi_mpsse, ftdi_i2c_send_check_ack(&ftdi_mpsse, 0xf2)); // HUM
	check_err_or_exit(&ftdi_mpsse, ftdi_i2c_send_check_ack(&ftdi_mpsse, 0b00000001));
	check_err_or_exit(&ftdi_mpsse, ftdi_i2c_send_check_ack(&ftdi_mpsse, 0xf4)); // MEAS
	check_err_or_exit(&ftdi_mpsse, ftdi_i2c_send_check_ack(&ftdi_mpsse, 0b00100111));
	check_err_or_exit(&ftdi_mpsse, ftdi_i2c_send_check_ack(&ftdi_mpsse, 0xf5)); // CONF
	check_err_or_exit(&ftdi_mpsse, ftdi_i2c_send_check_ack(&ftdi_mpsse, 0b00100000));
	check_err_or_exit(&ftdi_mpsse, ftdi_i2c_end(&ftdi_mpsse));

	check_err_or_exit(&ftdi_mpsse, ftdi_i2c_begin(&ftdi_mpsse, 0x76, true));
	check_err_or_exit(&ftdi_mpsse, ftdi_i2c_send_check_ack(&ftdi_mpsse, 0xf3)); // STAT
	check_err_or_exit(&ftdi_mpsse, ftdi_i2c_begin(&ftdi_mpsse, 0x76, false));
	check_err_or_exit(&ftdi_mpsse, ftdi_i2c_recv_send_ack(&ftdi_mpsse, &val, 1, false));
	printf("stat=%.2x\n", val);
	check_err_or_exit(&ftdi_mpsse, ftdi_i2c_recv_send_ack(&ftdi_mpsse, &val, 1, false));
	printf("ctrl_meas=%.2x\n", val);
	check_err_or_exit(&ftdi_mpsse, ftdi_i2c_recv_send_ack(&ftdi_mpsse, &val, 1, true));
	printf("config=%.2x\n", val);
	check_err_or_exit(&ftdi_mpsse, ftdi_i2c_end(&ftdi_mpsse));

	for (unsigned cnt = 0; cnt < 4; cnt++) {
		unsigned int temp, press, hum;
		check_err_or_exit(&ftdi_mpsse, ftdi_i2c_begin(&ftdi_mpsse, 0x76, true));
		ftdi_i2c_send_check_ack(&ftdi_mpsse, 0xf7);
		check_err_or_exit(&ftdi_mpsse, ftdi_i2c_begin(&ftdi_mpsse, 0x76, false));
		check_err_or_exit(&ftdi_mpsse, ftdi_i2c_recv_send_ack(&ftdi_mpsse, &val, 1, false)); // f7
		press = val << 12U;
		check_err_or_exit(&ftdi_mpsse, ftdi_i2c_recv_send_ack(&ftdi_mpsse, &val, 1, false));
		press |= val << 4U;
		check_err_or_exit(&ftdi_mpsse, ftdi_i2c_recv_send_ack(&ftdi_mpsse, &val, 1, false));
		press |= val >> 4U;

		check_err_or_exit(&ftdi_mpsse, ftdi_i2c_recv_send_ack(&ftdi_mpsse, &val, 1, false)); // fa
		temp = val << 12U;
		check_err_or_exit(&ftdi_mpsse, ftdi_i2c_recv_send_ack(&ftdi_mpsse, &val, 1, false));
		temp |= val << 4U;
		check_err_or_exit(&ftdi_mpsse, ftdi_i2c_recv_send_ack(&ftdi_mpsse, &val, 1, false));
		temp |= val >> 4U;

		check_err_or_exit(&ftdi_mpsse, ftdi_i2c_recv_send_ack(&ftdi_mpsse, &val, 1, false)); // fd
		hum = val << 8U;
		check_err_or_exit(&ftdi_mpsse, ftdi_i2c_recv_send_ack(&ftdi_mpsse, &val, 1, true));
		hum |= val;

		printf("temp  = 0x%06x -> %10.2lf\n", temp, BME280_compensate_T(temp, &comp));
		printf("press = 0x%06x -> %10.2lf\n", press, BME280_compensate_P(press, &comp));
		printf("hum   = 0x%06x -> %10.2lf\n", hum, BME280_compensate_H(hum, &comp));

		check_err_or_exit(&ftdi_mpsse, ftdi_i2c_end(&ftdi_mpsse));

		sleep(2);
	}

	ftdi_i2c_close(&ftdi_mpsse);

	return 0;
}

