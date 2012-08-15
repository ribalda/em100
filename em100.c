/*
 * Copyright (C) 2012 The Chromium OS Authors.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <libusb.h>

/* SPI flash chips parameters definition */
#include "em100pro_chips.h"

struct em100 {
	libusb_device_handle *dev;
	libusb_context *ctx;
};

int check_status(libusb_device_handle *dev);
int em100_attach(struct em100 *em100)
{
	libusb_device **devs;
	libusb_device_handle *dev;
	libusb_context *ctx = NULL;

	if (libusb_init(&ctx) < 0) {
		printf("Could not init libusb.\n");
		return 0;
	}

	libusb_set_debug(ctx, 3);

	if (libusb_get_device_list(ctx, &devs) < 0) {
		printf("Could not find USB devices.\n");
		return 0;
	}

	dev = libusb_open_device_with_vid_pid(ctx, 0x4b4, 0x1235);
	if (!dev) {
		printf("Could not find em100pro.\n");
		return 0;
	}

	libusb_free_device_list(devs, 1);

	if (libusb_kernel_driver_active(dev, 0) == 1) {
		if (libusb_detach_kernel_driver(dev, 0) != 0) {
			printf("Could not detach kernel driver.\n");
			return 0;
		}
	}

	if (libusb_claim_interface(dev, 0) < 0) {
		printf("Could not claim interface.\n");
		return 0;
	}

	if (!check_status(dev)) {
		printf("Device status unknown.\n");
		return 0;
	}

	em100->dev = dev;
	em100->ctx = ctx;
	return 1;
}

int em100_detach(struct em100 *em100)
{
	if (libusb_release_interface(em100->dev, 0) != 0) {
		printf("releasing interface failed.\n");
		return 1;
	}

	libusb_close(em100->dev);
	libusb_exit(em100->ctx);

	return 0;
}

int send_cmd(libusb_device_handle *dev, void *data)
{
	int actual;
	int length = 16; /* haven't seen any other length yet */
	libusb_bulk_transfer(dev, 1 | LIBUSB_ENDPOINT_OUT, data, length, &actual, 0);
	return (actual == length);
}

int get_response(libusb_device_handle *dev, void *data, int length)
{
	int actual;
	libusb_bulk_transfer(dev, 2 | LIBUSB_ENDPOINT_IN, data, length, &actual, 0);
	return actual;
}

int set_chip_type(libusb_device_handle *dev, const chipdesc *desc)
{
	unsigned char cmd[16];
	/* result counts unsuccessful send_cmd()s.
         * These are then converted in a boolean success value
         */
	int result = 0;
	int i = 0;
	memset(cmd, 0, 16);
	cmd[0] = 0x23;

	cmd[1] = 0x32;
	cmd[2] = desc->x23[i++];
	cmd[3] = desc->x23[i++];
	result += !send_cmd(dev, cmd);

	cmd[1] = 0x3a;
	cmd[2] = desc->x23[i++];
	cmd[3] = desc->x23[i++];
	result += !send_cmd(dev, cmd);

	cmd[1] = 0x38;
	cmd[2] = desc->x23[i++];
	cmd[3] = desc->x23[i++];
	result += !send_cmd(dev, cmd);

	cmd[1] = 0x40;
	cmd[2] = desc->x23[i++];
	cmd[3] = desc->x23[i++];
	result += !send_cmd(dev, cmd);

	cmd[1] = 0x42;
	cmd[2] = desc->x23[i++];
	cmd[3] = desc->x23[i++];
	result += !send_cmd(dev, cmd);

	cmd[1] = 0x44;
	cmd[2] = desc->x23[i++];
	cmd[3] = desc->x23[i++];
	result += !send_cmd(dev, cmd);

	cmd[1] = 0x46;
	cmd[2] = desc->x23[i++];
	cmd[3] = desc->x23[i++];
	result += !send_cmd(dev, cmd);

	cmd[1] = 0x48;
	cmd[2] = desc->x23[i++];
	cmd[3] = desc->x23[i++];
	result += !send_cmd(dev, cmd);

	i = 0;
	cmd[0] = 0x11;

	cmd[1] = 0x02;
	cmd[2] = desc->x11[i++];
	cmd[3] = desc->x11[i++];
	result += !send_cmd(dev, cmd);

	cmd[1] = 0x03;
	cmd[2] = desc->x11[i++];
	cmd[3] = desc->x11[i++];
	result += !send_cmd(dev, cmd);

	cmd[1] = 0x04;
	cmd[2] = desc->x11[i++];
	cmd[3] = desc->x11[i++];
	result += !send_cmd(dev, cmd);

	return !result;
}

int set_state(libusb_device_handle *dev, int run)
{
	unsigned char cmd[16];
	memset(cmd, 0, 16);
	cmd[0] = 0x23;
	cmd[1] = 0x28;
	cmd[3] = run & 1;
	return send_cmd(dev, cmd);
}

int get_state(libusb_device_handle *dev)
{
	unsigned char cmd[16];
	unsigned char data[3];
	memset(cmd, 0, 16);
	cmd[0] = 0x22;
	cmd[1] = 0x28;
	if (!send_cmd(dev, cmd)) {
		return -1; /* error */
	}
	if (!get_response(dev, data, 3)) {
		return -2; /* error */
	}
	if ((data[0] == 0x02) && (data[1] == 0x00)) {
		return data[2]; /* success */
	}
	return -3; /* error */
}

int check_status(libusb_device_handle *dev)
{
	unsigned char cmd[16];
	unsigned char data[512];
	memset(cmd, 0, 16);
	cmd[0] = 0x30; /* status */
	if (!send_cmd(dev, cmd)) {
		return 0;
	}
	int len = get_response(dev, data, 512);
	if ((len == 3) && (data[0] == 0x20) && (data[1] == 0x20) && (data[2] == 0x15))
		return 1;
	return 0;
}

int get_version(libusb_device_handle *dev, int *mcu, int *fpga)
{
	unsigned char cmd[16];
	unsigned char data[512];
	memset(cmd, 0, 16);
	cmd[0] = 0x10; /* version */
	if (!send_cmd(dev, cmd)) {
		return 0;
	}
	int len = get_response(dev, data, 512);
	if ((len == 5) && (data[0] == 4)) {
		*mcu = (data[3]*100) + data[4];
		*fpga = (data[1]*100) + data[2];
		return 1;
	}
	return 0;
}

int read_data(libusb_device_handle *dev, void *data, int length)
{
	int actual;
	unsigned char cmd[16];
	memset(cmd, 0, 16);
	cmd[0] = 0x41; /* em100-to-host eeprom data */
	/* cmd 1-3 might be start address? Hard code 0 for now, haven't seen anything else in the logs */
	/* when doing cmd 1-3, check if 4-6 are end address or length */
	cmd[4] = length & 0xff;
	cmd[5] = (length >> 8) & 0xff;
	cmd[6] = (length >> 16) & 0xff;
	if (!send_cmd(dev, cmd)) {
		printf("error initiating host-to-em100 transfer.\n");
		return 0;
	}
	libusb_bulk_transfer(dev, 2 | LIBUSB_ENDPOINT_IN, data, length, &actual, 0);
	printf("tried reading %d bytes, got %d\n", length, actual);
	return (actual == length);
}

int send_data(libusb_device_handle *dev, void *data, int length)
{
	int actual;
	unsigned char cmd[16];
	memset(cmd, 0, 16);
	cmd[0] = 0x40; /* host-to-em100 eeprom data */
	/* cmd 1-3 might be start address? Hard code 0 for now, haven't seen anything else in the logs */
	/* when doing cmd 1-3, check if 4-6 are end address or length */
	cmd[4] = length & 0xff;
	cmd[5] = (length >> 8) & 0xff;
	cmd[6] = (length >> 16) & 0xff;
	if (!send_cmd(dev, cmd)) {
		printf("error initiating host-to-em100 transfer.\n");
		return 0;
	}
	libusb_bulk_transfer(dev, 1 | LIBUSB_ENDPOINT_OUT, data, length, &actual, 0);
	printf("tried sending %d bytes, sent %d\n", length, actual);
	return (actual == length);
}

static const struct option longopts[] = {
	{"set", 1, 0, 'c'},
	{"download", 1, 0, 'd'},
	{"start", 0, 0, 'r'},
	{"stop", 0, 0, 's'},
	{"verify", 0, 0, 'v'},
	{"help", 0, 0, 'h'},
	{NULL, 0, 0, 0}
};

void usage(void)
{
	printf("em100: em100 client utility\n\n"
		"-c CHIP|--set CHIP: select CHIP emulation\n"
		"-d[ownload] FILE:   upload FILE into em100\n"
		"-r|--start:         em100 shall run\n"
		"-s|--stop:          em100 shall stop\n"
		"-v|--verify:        verify EM100 content matches the file\n"
		"-h|--help:          this help text\n");
}

/* get MCU and FPGA version, *100 encoded */
int main(int argc, char **argv)
{
	int opt, idx;
	const char *desiredchip = NULL;
	const char *filename = NULL;
	int do_start = 0, do_stop = 0;
        int verify = 0;
	while ((opt = getopt_long(argc, argv, "c:d:rsvh",
				  longopts, &idx)) != -1) {
		switch (opt) {
		case 'c':
			desiredchip = optarg;
			break;
		case 'd':
			filename = optarg;
			/* TODO: check that file exists */
			break;
		case 'r':
			do_start = 1;
			break;
		case 's':
			do_stop = 0;
			break;
		case 'v':
			verify = 1;
			break;
		case 'h':
			usage();
			return 0;
		}
	}

	const chipdesc *chip = chips;
	if (desiredchip) {
		do {
			if (strcmp(desiredchip, chip->name) == 0) {
				printf("will emulate '%s'\n", chip->name);
				break;
			}
		} while ((++chip)->name);

		if (chip->name == NULL) {
			printf("Supported chips:\n");
			chip = chips;
			do {
				printf("%s ", chip->name);
			} while ((++chip)->name);
			printf("\n\nCould not find emulation for '%s'.\n", desiredchip);

			return 1;
		}
	}

	struct em100 em100;
	if (!em100_attach(&em100)) {
		return 1;
	}

	int mcu, fpga;
	if (!get_version(em100.dev, &mcu, &fpga)) {
		printf("Failed fetching version information.\n");
		return 0;
	}

	printf("MCU version: %d.%d\nFPGA version: %d.%d\n", mcu/100, mcu%100, fpga/100, fpga%100);

	if (do_stop) {
		set_state(em100.dev, 0);
	}

	if (desiredchip) {
		if (!set_chip_type(em100.dev, chip)) {
			printf("Failed configuring chip type.\n");
			return 0;
		}
	}

	if (filename) {
		int maxlen = 0x800000; /* largest size, right? */
		void *data = malloc(maxlen);
		if (data == NULL) {
			printf("FATAL: couldn't allocate memory\n");
			return 1;
		}
		FILE *fdata = fopen(filename, "rb");
		if (!fdata) {
			perror("Could not open upload file");
			return 1;
		}

		int length = 0;
		while ((!feof(fdata)) && (length < maxlen)) {
			int blocksize = 65536;
			length += blocksize * fread(data+length, blocksize, 1, fdata);
		}
		fclose(fdata);

		if (length > maxlen) {
			printf("FATAL: length > maxlen\n");
			return 1;
		}

		send_data(em100.dev, data, length);
		if (verify) {
			int done;
			void *readback = malloc(length);
			if (data == NULL) {
				printf("FATAL: couldn't allocate memory\n");
				return 1;
			}
			done = read_data(em100.dev, readback, length);
			if (done && (memcmp(data, readback, length) == 0))
				printf("Verify: PASS\n");
			else
				printf("Verify: FAIL\n");
			free(readback);
		}

		free(data);
	}

	if (do_start) {
		set_state(em100.dev, 1);
	}

	return em100_detach(&em100);
}
