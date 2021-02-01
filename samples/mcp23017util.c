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

static char *i2cDevice_pG = NULL;
static bool freeDeviceString_G = false;
static uint8_t i2cAddr_G = 0x20;
static bool altRegAddr_G = false;
static bool run_G = true;

static void usage (char *cmd_p);
static bool process_cmdline_args (int argc, char *argv[]);
static void print_menu (void);
static void process_cmd (void);
static bool reset (void);
static void cleanup (void);

int
main (int argc, char *argv[])
{
	int fd;

	if (!process_cmdline_args(argc, argv)) {
		printf("cmdline error\n");
		return 1;
	}

	// setup GPIO#4 (on RPi) as /RESET
	fd = open("/sys/class/gpio/export", O_WRONLY);
	if (fd == -1) {
		perror("open /sys/class/gpio/export");
		goto done;
	}
	if (write(fd, "4", 1) != 1) {
		perror("writing 4 to gpio export");
		close(fd);
		goto done;
	}
	close(fd);
	usleep(200000);
	fd = open("/sys/class/gpio/gpio4/direction", O_WRONLY);
	if (fd == -1) {
		perror("open /sys/class/gpio/gpio4/direction");
		goto done;
	}
	if (write(fd, "out", 3) != 3) {
		perror("writing direction");
		close(fd);
		goto done;
	}
	close(fd);

	if (!reset()) {
		fprintf(stderr, "reset error\n");
		goto done;
	}

	while (run_G) {
		print_menu();
		process_cmd();
	}

done:
	cleanup();
	return 0;
}

static bool
reset (void)
{
	int fd;
	uint8_t val1, val2;
	uint8_t reg1, reg2;
	unsigned int usleepTime;

	mcp23017__cleanup();

	if (!mcp23017__init(i2cDevice_pG, &i2cAddr_G, altRegAddr_G)) {
		fprintf(stderr, "mcp23017 library init error\n");
		return false;
	}

	fd = open("/sys/class/gpio/gpio4/value", O_WRONLY);
	if (fd == -1) {
		perror("open /sys/class/gpio4/value");
		return false;
	}

	reg1 = 0x00;
	reg2 = altRegAddr_G? 0x09 : 0x12;
	usleepTime = 1;
	do {
		write(fd, "0", 1);
		usleep(usleepTime);
		write(fd, "1", 1);
		usleep(usleepTime);

		mcp23017__get_reg(reg1, &val1);
		mcp23017__get_reg(reg2, &val2);

		++usleepTime;
	} while ((val1 != 0xff) && (val2 != 0x00));
	close(fd);

	if (!mcp23017__set_output_pins(0xff, 0xff)) {
		fprintf(stderr, "output pin setting error\n");
		return false;
	}

	return true;
}

static void
cleanup (void)
{
	int fd;

	if (freeDeviceString_G)
		free(i2cDevice_pG);

	fd = open("/sys/class/gpio/unexport", O_WRONLY);
	if (fd == -1) {
		perror("open unexport");
		return;
	}
	if (write(fd, "4", 1) != 1)
		perror("write unexport");
	close(fd);
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
	printf(" 6  - set bit\n");
	printf(" 7  - clear bit\n");
	printf(" 9  - reset\n");
}

static void
process_cmd (void)
{
	int ch;
	bool bret;
	char buf[32];
	uint8_t reg, val;
	Mcp23017Bit_e bit;

	fgets(buf, sizeof(buf), stdin);
	sscanf(buf, "%i", &ch);
	switch (ch) {
		case 0:
			run_G = false;
			break;

		case 1:
		case 2:
			fflush(stdin);
			printf("please enter a value: ");
			if (fgets(buf, sizeof(buf), stdin) == NULL) {
				perror("fgets() error");
				break;
			}
			if (sscanf(buf, "%hhi", &val) != 1) {
				fprintf(stderr, "sscanf() error\n");
				break;
			}
			if (ch == 1)
				bret = mcp23017__write_portA(val);
			else
				bret = mcp23017__write_portB(val);
			if (!bret)
				printf("write error\n");
			break;

		case 3:
		case 4:
			if (ch == 3)
				bret = mcp23017__get_portA(&val);
			else
				bret = mcp23017__get_portB(&val);
			if (bret)
				printf("GPIO%c: 0x%02x\n", (ch==3?'A':'B'), val);
			else
				printf("get error\n");
			break;

		case 5:
			fflush(stdin);
			printf("register: ");
			if (fgets(buf, sizeof(buf), stdin) == NULL) {
				perror("fgets() error");
				break;
			}
			if (sscanf(buf, "%hhi", &reg) != 1) {
				fprintf(stderr, "sscanf() error\n");
				break;
			}
			if (mcp23017__get_reg(reg, &val))
				printf("register: 0x%02x is 0x%02x\n", reg, val);
			break;

		case 6:
		case 7:
			fflush(stdin);
			printf("bit: ");
			if (fgets(buf, sizeof(buf), stdin) == NULL) {
				perror("fgets() error");
				break;
			}
			if (sscanf(buf, "GP%c%hhd", &reg, &val) != 2) {
				fprintf(stderr, "sscanf() error\n");
				break;
			}
			switch (reg) {
				case 'A':
					bit = GPA0;
					break;
				case 'B':
					bit = GPB0;
					break;
				default:
					fprintf(stderr,
						"specify register as \"GPA\" or \"GPB\" not 0x%02x\n", reg);
					return;
			}
			if (val > 7) {
				fprintf(stderr, "index out of range (0 ≤ %hhd ≤ 7)\n", val);
				break;
			}
			bit += val;
			if (ch == 6)
				if (!mcp23017__set_bit(bit))
					fprintf(stderr, "set bit error\n");
			if (ch == 7)
				if (!mcp23017__clear_bit(bit))
					fprintf(stderr, "clear bit error\n");
			break;

		case 9:
			reset();
			break;

		default:
			printf("unknown cmd: %c\n", ch);
	}
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
				if (sscanf(optarg, "%hhi", &tmp) != 1) {
					fprintf(stderr, "conversion error\n");
					return false;
				}
				i2cAddr_G = (uint8_t)tmp;
				break;

			case '1':
				altRegAddr_G = true;
				break;

			default:
				printf("getopt error: %c (0x%x)\n", c, c);
				break;
		}
	}

	return true;
}
