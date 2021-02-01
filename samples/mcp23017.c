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
static int valueFd_G = -1;
static bool run_G = true;
static int bank_G = 0;

static void usage (char *cmd_p);
static bool process_cmdline_args (int argc, char *argv[]);

static bool mcp23017__init (void);
static void mcp23017__cleanup (void);

static void print_menu (void);
static void process_cmd (void);

int
main (int argc, char *argv[])
{
	if (!process_cmdline_args(argc, argv)) {
		printf("cmdline error\n");
		return 1;
	}

	switch (bank_G) {
		case 0:
			IODIRA = 0x00;
			IODIRB = 0x01;
			GPIOA = 0x12;
			GPIOB = 0x13;
			OLATA = 0x14;
			OLATB = 0x15;
			break;

		case 1:
			IODIRA = 0x00;
			IODIRB = 0x10;
			GPIOA = 0x09;
			GPIOB = 0x19;
			OLATA = 0x0a;
			OLATB = 0x1a;
			break;

		default:
			printf("invalid bank_G: %d\n", bank_G);
			return 1;
	}

	if (!mcp23017__init()) {
		printf("init error\n");
		return 1;
	}

	while (run_G) {
		print_menu();
		process_cmd();
	}

	mcp23017__cleanup();
	return 0;
}

static void
print_menu (void)
{
	printf("Menu\n");
	printf("^^^^\n");
	printf(" 0  - exit\n");
	printf(" 1  - set gpio A\n");
	printf(" 2  - set gpio B\n");
	printf(" 3  - get gpio A\n");
	printf(" 4  - get gpio B\n");
	printf(" 5  - read register\n");
	printf(" 9  - reset\n");
}

static void
process_cmd (void)
{
	int ch;
	int ret;
	uint8_t val;
	char buf[32];

	fgets(buf, sizeof(buf), stdin);
	sscanf(buf, "%i", &ch);
	switch (ch) {
		case 0:
			run_G = false;
			break;

		case 1:
		case 2:
			fflush(stdin);
			printf("please enter a value for 0x%02x: ", (ch==1?GPIOA:GPIOB));
			if (fgets(buf, sizeof(buf), stdin) == NULL) {
				perror("fgets() error");
				break;
			}
			if (sscanf(buf, "%hhi", &val) != 1) {
				fprintf(stderr, "sscanf() error\n");
				break;
			}
			i2c_smbus_write_byte_data(i2cFd_G, (ch == 1? GPIOA:GPIOB), val);
			break;

		case 3:
		case 4:
			ret = i2c_smbus_read_byte_data(i2cFd_G, (ch == 3? GPIOA:GPIOB));
			printf("GPIO%c: 0x%08x\n", (ch==3?'A':'B'), ret);
			ret = i2c_smbus_read_byte_data(i2cFd_G, (ch == 3? OLATA:OLATB));
			printf("OLAT%c: 0x%08x\n", (ch==3?'A':'B'), ret);
			break;

		case 5:
			fflush(stdin);
			printf("register: ");
			if (fgets(buf, sizeof(buf), stdin) == NULL) {
				perror("fgets() error");
				break;
			}
			if (sscanf(buf, "%hhi", &val) != 1) {
				fprintf(stderr, "sscanf() error\n");
				break;
			}
			i2c_smbus_write_byte_data(i2cFd_G, (ch == 1? GPIOA:GPIOB), val);
			ret = i2c_smbus_read_byte_data(i2cFd_G, val);
			printf("register: 0x%02x is 0x%02x\n", val, (uint8_t)ret);
			break;

		case 9:
			mcp23017__cleanup();
			mcp23017__init();
			break;

		default:
			printf("unknown cmd: %c\n", ch);
	}
}

static bool
mcp23017__init (void)
{
	int ret, ret1;
	unsigned long funcs;
	int exportFd;
	int directionFd;
	uint8_t val;
	unsigned int usleepTime;

	// preconds
	if (libInit_G)
		return false;

	// open i2c device
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
	if (!(funcs & I2C_FUNC_SMBUS_WRITE_I2C_BLOCK)) {
		fprintf(stderr, "I2C_FUNC_SMBUS_WRITE_I2C_BLOCK not available\n");
		goto err1;
	}

	// set device
	ret = ioctl(i2cFd_G, I2C_SLAVE, i2cAddr_G);
	if (ret < 0) {
		perror("I2C_SLAVE");
		goto err1;
	}

	// configure GPIO04 to be the reset
	exportFd = open("/sys/class/gpio/export", O_WRONLY);
	if (exportFd == -1) {
		perror("open export");
		goto err1;
	}
	if (write(exportFd, "4", 1) != 1) {
		perror("write export");
		close(exportFd);
		goto err1;
	}
	close(exportFd);

	usleep(200000);

	directionFd = open("/sys/class/gpio/gpio4/direction", O_WRONLY);
	if (directionFd == -1) {
		perror("open direction");
		goto err1;
	}
	if (write(directionFd, "out", 3) != 3) {
		perror("write direction");
		close(directionFd);
		goto err1;
	}
	close(directionFd);

	valueFd_G = open("/sys/class/gpio/gpio4/value", O_WRONLY);
	if (valueFd_G == -1) {
		perror("open value");
		goto err1;
	}

	// reset
	usleepTime = 1;
	do {
		write(valueFd_G, "0", 1);
		usleep(usleepTime);
		write(valueFd_G, "1", 1);
		usleep(usleepTime);

		ret  = i2c_smbus_read_byte_data(i2cFd_G, IODIRA);
		ret1 = i2c_smbus_read_byte_data(i2cFd_G, GPIOA);
		++usleepTime;
	} while ((ret != 0xff) && (ret1 != 0x00));
	printf("usleepTime: %u\n", usleepTime);

	// set IOCON.BANK → 1
	// initially bank is set to 0, meaning IOCON is at 0x0a/0x0b
	if (bank_G == 1) {
		ret = i2c_smbus_read_byte_data(i2cFd_G, 0x0a);
		val = (uint8_t)ret;
		val |= 0x80;
		i2c_smbus_write_byte_data(i2cFd_G, 0x0a, val);
	}

	// set IODIRA → output
	ret = i2c_smbus_write_byte_data(i2cFd_G, IODIRA, 0);
	if (ret == -1) {
		perror("can't set IODIRA → output\n");
		return false;
	}
	// set IODIRB → output
	ret = i2c_smbus_write_byte_data(i2cFd_G, IODIRB, 0);
	if (ret == -1) {
		perror("can't set IODIRB → output\n");
		return false;
	}

	libInit_G = true;
	return true;
err1:
	close(i2cFd_G);
	return false;
}

static void
mcp23017__cleanup (void)
{
	int unexportFd;

	// preconds
	if (!libInit_G)
		return;

	if (freeDeviceString_G)
		free(i2cDevice_pG);
	if (i2cFd_G > 0)
		close(i2cFd_G);

	unexportFd = open("/sys/class/gpio/unexport", O_WRONLY);
	if (unexportFd == -1) {
		perror("open unexport");
		return;
	}
	if (write(unexportFd, "4", 1) != 1)
		perror("write unexport");
	close(unexportFd);

	if (valueFd_G != -1)
		close(valueFd_G);

	libInit_G = false;
}

static void
usage (char *cmd_p)
{
	printf("%s\n\n", PACKAGE_STRING);
	if (cmd_p != NULL)
		printf("%s [options]\n", cmd_p);
	printf("  options\n");
	printf(" -h|--help         Print usage help and exit successfully\n");
	printf(" -d|--device <d>   Use i2c device <d> (default:/dev/i2c-1)\n");
	printf(" -a|--address <a>  Use i2c device address <a> (default:0x20)\n");
	printf(" -1|--bank1        Use IOCON.BANK=1 (default:IOCON.BANK=0)\n");
}

static bool
process_cmdline_args (int argc, char *argv[])
{
	int c;
	uint8_t tmp;
	struct option longOpts[] = {
		{"help",    no_argument,       NULL, 'h'},
		{"device",  required_argument, NULL, 'd'},
		{"address", required_argument, NULL, 'a'},
		{"bank1",   no_argument,       NULL, '1'},
		{NULL,      0,                 NULL,  0},
	};

	while (1) {
		c = getopt_long(argc, argv, "hd:a:1", longOpts, NULL);
		if (c == -1)
			break;
		switch (c) {
			case 'h':
				usage(argv[0]);
				exit(EXIT_SUCCESS);
				break;

			case 'd':
				i2cDevice_pG = strdup(optarg);
				if (i2cDevice_pG == NULL) {
					perror("strdup()");
					return false;
				}
				freeDeviceString_G = true;
				break;

			case 'a':
				if (sscanf(optarg, "%hhd", &tmp) != 1) {
					fprintf(stderr, "conversion error\n");
					return false;
				}
				i2cAddr_G = (uint8_t)tmp;
				break;

			case '1':
				bank_G = 1;
				break;

			default:
				printf("getopt error: %c (0x%x)\n", c, c);
				break;
		}
	}

	return true;
}
