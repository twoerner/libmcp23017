/*
 * Copyright (C) 2021  Trevor Woerner <twoerner@gmail.com>
 * SPDX-License-Identifier: OSL-3.0
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>

#include "mcp23017.h"
#include "config.h"

uint8_t IODIRA;
uint8_t IODIRB;
uint8_t GPIOA;
uint8_t GPIOB;
uint8_t OLATA;
uint8_t OLATB;

static char *i2cDevice_pG = "/dev/i2c-1";
static bool freeDeviceString_G = false;
static uint8_t i2cAddr_G = 0x20;
static int i2cFd_G = 0;
static bool libInit_G = false;

void
mcp23017__cleanup (void)
{
	// preconds
	if (!libInit_G)
		return;

	if (freeDeviceString_G)
		free(i2cDevice_pG);
	if (i2cFd_G > 0)
		close(i2cFd_G);

	libInit_G = false;
}

bool
mcp23017__init (const char *devFile_p, uint8_t *i2cAddr_p, bool altRegAddr)
{
	int ret;
	unsigned long funcs;
	uint8_t val;

	// preconds
	if (libInit_G)
		return false;

	// open i2c device
	if (devFile_p != NULL) {
		i2cDevice_pG = strdup(devFile_p);
		if (i2cDevice_pG == NULL) {
			perror("strdup on device filename");
			return false;
		}
		freeDeviceString_G = true;
	}
	i2cFd_G = open(i2cDevice_pG, O_RDWR);
	if (i2cFd_G < 0) {
		perror("open(i2c device)");
		return false;
	}

	// check/verify i2c functionality on device
	ret = ioctl(i2cFd_G, I2C_FUNCS, &funcs);
	if (ret < 0) {
		perror("can't get i2c functionality");
		goto err1;
	}
	if (!(funcs & I2C_FUNC_SMBUS_WRITE_BYTE_DATA)) {
		fprintf(stderr, "I2C_FUNC_SMBUS_WRITE_BYTE_DATA not available\n");
		goto err1;
	}
	if (!(funcs & I2C_FUNC_SMBUS_READ_BYTE_DATA)) {
		fprintf(stderr, "I2C_FUNC_SMBUS_READ_BYTE_DATA not available\n");
		goto err1;
	}

	// set device
	if (i2cAddr_p != NULL)
		i2cAddr_G = *i2cAddr_p;
	ret = ioctl(i2cFd_G, I2C_SLAVE, i2cAddr_G);
	if (ret < 0) {
		perror("can't set i2c slave address");
		goto err1;
	}

	// set IOCON.BANK → 1
	// assume initially bank is set to 0, meaning IOCON is at 0x0a/0x0b
	if (altRegAddr) {
		ret = i2c_smbus_read_byte_data(i2cFd_G, 0x0a);
		if (ret == -1)
			return false;
		val = (uint8_t)ret;
		val |= 0x80;
		i2c_smbus_write_byte_data(i2cFd_G, 0x0a, val);

		IODIRA = 0x00;
		IODIRB = 0x10;
		GPIOA = 0x09;
		GPIOB = 0x19;
		OLATA = 0x0a;
		OLATB = 0x1a;
	}
	else {
		IODIRA = 0x00;
		IODIRB = 0x01;
		GPIOA = 0x12;
		GPIOB = 0x13;
		OLATA = 0x14;
		OLATB = 0x15;
	}

	ret = atexit(mcp23017__cleanup);
	if (ret != 0)
		perror("atexit()");

	libInit_G = true;
	return true;
err1:
	close(i2cFd_G);
	if (freeDeviceString_G)
		free(i2cDevice_pG);
	return false;
}

static bool
is_reg_valid (uint8_t reg)
{
	if ((reg == IODIRA) |
		(reg == IODIRB) |
		(reg == GPIOA) |
		(reg == GPIOB) |
		(reg == OLATA) |
		(reg == OLATB)) {
		return true;
	}

	fprintf(stderr, "invalid reg: 0x%02x\n", reg);
	return false;
}

static bool
set_ones (uint8_t reg, uint8_t val)
{
	int32_t ret;
	uint8_t newval;

	if (!is_reg_valid(reg))
		return false;
	if (val == 0)
		return true;

	ret = i2c_smbus_read_byte_data(i2cFd_G, reg);
	if (ret == -1) {
		perror("set_ones() read byte");
		return false;
	}

	newval = (uint8_t)ret;
	newval |= val;

	ret = i2c_smbus_write_byte_data(i2cFd_G, reg, newval);
	if (ret != 0) {
		perror("set_ones() write byte");
		return false;
	}

	return true;
}

static bool
set_zeros (uint8_t reg, uint8_t val)
{
	int32_t ret;
	uint8_t newval;

	if (!is_reg_valid(reg))
		return false;
	if (val == 0)
		return true;

	ret = i2c_smbus_read_byte_data(i2cFd_G, reg);
	if (ret == -1) {
		perror("set_zeros() read byte");
		return false;
	}

	newval = (uint8_t)ret;
	newval &= (uint8_t)(~(uint8_t)val);

	ret = i2c_smbus_write_byte_data(i2cFd_G, reg, newval);
	if (ret != 0) {
		perror("set_zeros() write byte");
		return false;
	}

	return true;
}

/**
 * set to '1' any pins you want set as output
 */
bool
mcp23017__set_output_pins (uint8_t portAmask, uint8_t portBmask)
{
	// preconds
	if (!libInit_G)
		return false;
	if (i2cFd_G <= 0)
		return false;

	if (!set_zeros(IODIRA, portAmask))
		return false;
	return set_zeros(IODIRB, portBmask);
}

/**
 * set to '1' any pins you want to set as input
 */
bool
mcp23017__set_input_pins (uint8_t portAmask, uint8_t portBmask)
{
	// preconds
	if (!libInit_G)
		return false;
	if (i2cFd_G < 0)
		return false;

	if (!set_ones(IODIRA, portAmask))
		return false;
	return set_ones(IODIRB, portBmask);
}

static bool
write_port (uint8_t reg, uint8_t val)
{
	int32_t ret;

	// preconds
	if (!libInit_G)
		return false;
	if (i2cFd_G < 0)
		return false;
	if (!is_reg_valid(reg))
		return false;

	ret = i2c_smbus_write_byte_data(i2cFd_G, reg, val);
	if (ret != 0)
		return false;
	return true;
}

bool
mcp23017__write_portA (uint8_t val)
{
	return write_port(GPIOA, val);
}

bool
mcp23017__write_portB (uint8_t val)
{
	return write_port(GPIOB, val);
}

bool
mcp23017__get_reg (uint8_t reg, uint8_t *val_p)
{
	int32_t ret;

	// preconds
	if (!libInit_G)
		return false;
	if (i2cFd_G < 0)
		return false;
	if (!is_reg_valid(reg))
		return false;
	if (val_p == NULL)
		return false;

	ret = i2c_smbus_read_byte_data(i2cFd_G, reg);
	if (ret == -1)
		return false;
	*val_p = (uint8_t)ret;
	return true;
}

bool
mcp23017__get_portA (uint8_t *val_p)
{
	return mcp23017__get_reg(GPIOA, val_p);
}

bool
mcp23017__get_portB (uint8_t *val_p)
{
	return mcp23017__get_reg(GPIOB, val_p);
}

static bool
is_output_bit (Mcp23017Bit_e bit)
{
	uint8_t val;
	uint8_t mask;

	if (!libInit_G)
		return false;
	if (i2cFd_G < 0)
		return false;
	if ((bit <= INVALID) || (bit >= END))
		return false;

	if (bit < GPB0) {
		if (!mcp23017__get_reg(IODIRA, &val))
			return false;
		mask = 1 << (bit - GPA0);
	}
	else {
		if (!mcp23017__get_reg(IODIRB, &val))
			return false;
		mask = 1 < (bit - GPB0);
	}

	// output bits are 0
	if ((mask & val) != 0) {
		fprintf(stderr, "bit %d is not an output (mask:0x%02x val:0x%02x)\n",
				bit, mask, val);
		return false;
	}

	return true;
}

bool
mcp23017__set_bit (Mcp23017Bit_e bit)
{
	uint8_t val;
	uint8_t mask = 0;

	// preconds
	if (!libInit_G)
		return false;
	if (i2cFd_G < 0)
		return false;
	if ((bit <= INVALID) || (bit >= END))
		return false;

	// check if direction bit is output
	if (!is_output_bit(bit))
		return false;

	// set bit
	if (bit < GPB0) {
		if (!mcp23017__get_reg(OLATA, &val)) {
			fprintf(stderr, "set_bit(): can't get olatA\n");
			return false;
		}
		mask = 1 << (bit - GPA0);
		val |= mask;
		if (!mcp23017__write_portA(val)) {
			fprintf(stderr, "set_bit(): can't write portA\n");
			return false;
		}
	}
	else {
		if (!mcp23017__get_reg(OLATB, &val)) {
			fprintf(stderr, "set_bit(): can't get olatB\n");
			return false;
		}
		mask = 1 << (bit - GPB0);
		val |= mask;
		if (!mcp23017__write_portB(val)) {
			fprintf(stderr, "set_bit(): can't write portB\n");
			return false;
		}
	}

	return true;
}

bool
mcp23017__clear_bit (Mcp23017Bit_e bit)
{
	uint8_t val;
	uint8_t mask;

	// preconds
	if (!libInit_G)
		return false;
	if (i2cFd_G < 0)
		return false;
	if ((bit <= INVALID) || (bit >= END))
		return false;

	// check if direction is output
	if (!is_output_bit(bit))
		return false;

	// clear bit
	if (bit < GPB0) {
		if (!mcp23017__get_reg(OLATA, &val)) {
			fprintf(stderr, "clear_bit(): can't get olatA\n");
			return false;
		}
		mask = (uint8_t)~(1 << (bit - GPA0));
		val &= mask;
		if (!mcp23017__write_portA(val)) {
			fprintf(stderr, "clear_bit(): can't write portA\n");
			return false;
		}
	}
	else {
		if (!mcp23017__get_reg(OLATB, &val)) {
			fprintf(stderr, "clear_bit(): can't get olatB\n");
			return false;
		}
		mask = (uint8_t)~(1 << (bit - GPB0));
		val &= mask;
		if (!mcp23017__write_portB(val)) {
			fprintf(stderr, "clear_bit(): can't write portB\n");
			return false;
		}
	}

	return true;
}
