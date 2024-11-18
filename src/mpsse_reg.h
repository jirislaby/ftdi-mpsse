/*
 * Licensed under the GPLv2
 */
#ifndef MPSSE_H
#define MPSSE_H

#define MPSSE_RX_BUFSIZE	1024
#define MPSSE_TX_BUFSIZE	1024

#define CMD_MSB_FALLING_EDGE_BYTE_OUT		0x11
#define CMD_MSB_FALLING_EDGE_BIT_OUT		0x13
#define CMD_MSB_RISING_EDGE_BYTE_IN		0x20
#define CMD_MSB_RISING_EDGE_BIT_IN		0x22
#define CMD_SET_BITS_LOW			0x80
#define CMD_LOOPBACK_EN				0x84
#define CMD_LOOPBACK_DIS			0x85
#define CMD_SET_CLK_DIVISOR			0x86
#define CMD_SEND_IMMEDIATE			0x87
#define CMD_CLK_DIV5_DIS			0x8a
#define CMD_CLK_DIV5_EN				0x8b
#define CMD_CLK_3PHASE_EN			0x8c
#define CMD_CLK_3PHASE_DIS			0x8d
#define CMD_CLK_ADAPTIVE_EN			0x96
#define CMD_CLK_ADAPTIVE_DIS			0x97
#define CMD_DRIVE_ONLY_ZERO			0x9e
#define CMD_ECHO1				0xaa
#define CMD_INVALID				0xfa

#endif
