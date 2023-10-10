/*
 * Copyright (c) 2d3D, Inc.
 * Written by Abraham vd Merwe <abraham@2d3d.co.za>
 * All rights reserved.
 *
 * Copyright (c) 2023 Vicharak Computer LLP.
 * Refactored by UtsavBalar1231 <utsavbalar1231@gmail.com> and
 * djkabutar <d.kabutarwala@yahoo.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	  notice, this list of conditions and the following disclaimer in the
 *	  documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of other contributors
 *	  may be used to endorse or promote products derived from this software
 *	  without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "flashcp.h"
#include "h2b.h"
#include <getopt.h>
#include <sys/stat.h>

/* for debugging purposes only */
#ifdef DEBUG
#undef DEBUG
#define DEBUG(fmt, args...)                        \
	{                                          \
		fprintf(stderr, "%d: ", __LINE__); \
		fprintf(stderr, fmt, ##args);      \
	}
#else
#undef DEBUG
#define DEBUG(fmt, args...)
#endif

#define KB(x) ((x) / 1024)
#define PERCENTAGE(x, total) (((x)*100) / (total))
#define DELETE_MODULE(name, flags) syscall(__NR_delete_module, name, flags)

/* cmd-line flags */
#define FLAG_NONE 0x00
#define FLAG_HELP 0x02
#define FLAG_FILENAME 0x04
#define FLAG_DEVICE 0x08
#define FLAG_ERASE_ALL 0x10
#define FLAG_PARTITION 0x20

static void show_usage()
{
	printf("Usage: %s [OPTIONS] [FILE]\n", PROGRAM_NAME);
	printf("Copy data to an MTD flash device.\n");
	printf("\nOptions:\n");
	printf("  -h, --help            Show this help message and exit.\n");
	printf("  -v, --verbose         Enable verbose mode.\n");
	printf("  -p, --partition       Copy to a specific partition.\n");
	printf("  -A, --erase-all       Erase the entire device before copying.\n");
	printf("  -V, --version         Display the program version.\n");
	printf("  -r, --read_from_flash Give flash access to FPGA.\n");
	printf("  -e, --external_cable  Program FPGA from the external cable.\n");
	printf("\nArguments:\n");
	printf("  FILE                  The input file to copy to the flash device.\n");
	printf("\nExamples:\n");
	printf("  %s -p input.bin       Copy input.bin to the flash partition.\n",
	       PROGRAM_NAME);
	printf("  %s -A firmware.bin    Copy and erase firmware.bin to the entire device.\n",
	       PROGRAM_NAME);
	printf("\n");
}

/******************************************************************************/

static int dev_fd = -1, fil_fd = -1;

static void cleanup(void)
{
	if (dev_fd > 0)
		close(dev_fd);
	if (fil_fd > 0)
		close(fil_fd);
}


/**
 * @brief Check if a kernel module is loaded.
 *
 * @param module_name The name of the kernel module to check.
 * @return 1 if the module is loaded, 0 if not.
 */
int is_module_loaded(const char *module_name) {
    FILE *fp;
    char buffer[256];
    int loaded = 0;

    // Run 'lsmod' and check if the module is listed
    fp = popen("lsmod", "r");
    if (fp == NULL) {
        return 0;
    }

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        if (strstr(buffer, module_name) != NULL) {
            loaded = 1;
            break;
        }
    }

    pclose(fp);
    return loaded;
}

/**
 * @brief Configure the SPI flash access for writing to an MTD device with retries.
 *
 * This function prepares the system for writing to an MTD flash device by
 * ensuring the necessary permissions, loading the required kernel module,
 * and toggling the SPI flash access between the processor and FPGA if needed.
 *
 * @param device The path to the MTD device file (e.g., "/dev/mtd0").
 * @return 0 if the configuration is successful, -1 if an error occurs.
 */
int vicharak_flash_configuration(const char *device) {
	const int max_retries = 10;

    // Check if the user has root privileges (requires sudo)
    if (geteuid() != 0) {
        log_verbose("Please run this program with sudo.\n");
        return -1;
    }

    int retries = 0;

    while (retries < max_retries) {
        int ret = access(device, F_OK);

        if (ret == 0) {
            // MTD device file exists
            return 0;
        } else {
            // Ensure that flash access is granted to the processor
            flash_access_to_processor();

            // Attempt to remove the spi_rockchip module
			if (is_module_loaded("spi_rockchip"))
				DELETE_MODULE("spi_rockchip", O_TRUNC);

            // Load the spi_rockchip module
            system("modprobe spi_rockchip");

            sleep(1);

            retries++;
        }
    }

    log_failure("Flash configuration failed after %d retries.\n", max_retries);
}

int main(int argc, char *argv[])
{
	const char *filename = NULL, *device = "/dev/mtd0";
	int i, flags = FLAG_NONE;
	size_t size, written;
	struct mtd_info_user mtd;
	struct erase_info_user erase;
	struct stat filestat;
	unsigned char *src, *dest;
	int ret;
	char *bin_filename = NULL;

	/*********************
	 * parse cmd-line
	 *****************/
	for (;;) {
		int option_index = 0;
		static const char *short_options = "hvpAVre";
		static const struct option long_options[] = {
			{ "help", no_argument, 0, 'h' },
			{ "verbose", no_argument, 0, 'v' },
			{ "partition", no_argument, 0, 'p' },
			{ "erase-all", no_argument, 0, 'A' },
			{ "version", no_argument, 0, 'V' },
			{ "read_from_flash", no_argument, 0, 'r' },
			{ "external_cable", no_argument, 0, 'e' },
			{ 0, 0, 0, 0 },
		};

		int c = getopt_long(argc, argv, short_options, long_options,
				    &option_index);
		if (c == EOF) {
			break;
		}

		switch (c) {
		case 'h':
			flags |= FLAG_HELP;
			DEBUG("Got FLAG_HELP\n");
			break;
		case 'v':
			set_verbose(1);
			DEBUG("Got FLAG_VERBOSE\n");
			break;
		case 'p':
			flags |= FLAG_PARTITION;
			DEBUG("Got FLAG_PARTITION");
			break;
		case 'A':
			flags |= FLAG_ERASE_ALL;
			DEBUG("Got FLAG_ERASE_ALL\n");
			break;
		case 'V':
			printf("%s: Version: %s\n", PROGRAM_NAME, VERSION);
			exit(EXIT_SUCCESS);
			break;
		case 'r':
			gpio_set_value(RESET_GPIO, "1");
			gpio_set_value(CONDONE_GPIO, "1");
			exit(EXIT_SUCCESS);
			break;
		case 'e':
			gpio_set_value(RESET_GPIO, "0");
			gpio_set_value(CONDONE_GPIO, "0");
			exit(EXIT_SUCCESS);
			break;
		default:
			DEBUG("Unknown parameter: %s\n", argv[option_index]);
			show_usage();
		}
	}

	if (flags & FLAG_HELP || device == NULL) {
		show_usage();
		exit(EXIT_SUCCESS);
	}

	if (optind + 1 == argc) {
		ret = vicharak_flash_configuration(device);
		if (ret < 0)
			log_failure("vicharak_flash_configuration failed\n");

		flags |= FLAG_FILENAME;
		filename = argv[optind];
		DEBUG("Got filename: %s\n", filename);
		bin_filename = malloc(strlen(filename) + 5);
		snprintf(bin_filename, strlen(filename) + 5, "%s.bin",
			 filename);

		flags |= FLAG_DEVICE;
	}

	if (flags & FLAG_PARTITION && flags & FLAG_ERASE_ALL)
		log_failure(
			"Option --partition does not support --erase-all\n");

	atexit(cleanup);

	// Convert the provided hexfile to binary
	if (flags & FLAG_FILENAME) {
		ret = convert_to_bin(filename, bin_filename);
		if (ret < 0)
			log_failure("Convert to binary problem.\n");
	}

	/* get some info about the flash device */
	dev_fd = safe_open(device, O_SYNC | O_RDWR);
	if (ioctl(dev_fd, MEMGETINFO, &mtd) < 0) {
		DEBUG("ioctl(): %m\n");
		log_failure(
			"This doesn't seem to be a valid MTD flash device!\n");
	}

	/* get some info about the file we want to copy */
	fil_fd = safe_open(bin_filename, O_RDONLY);
	if (fstat(fil_fd, &filestat) < 0)
		log_failure("While trying to get the file status of %s: %m\n",
			    bin_filename);

	/* does it fit into the device/partition? */
	if (filestat.st_size > mtd.size)
		log_failure("%s won't fit into %s!\n", bin_filename, device);

	src = malloc(mtd.erasesize);
	if (!src)
		log_failure("Malloc failed");

	dest = malloc(mtd.erasesize);
	if (!dest)
		log_failure("Malloc failed");

	/* diff block flashcp */
	if (flags & FLAG_PARTITION) {
		goto DIFF_BLOCKS;
	}

	/*****************************************************
	 * erase enough blocks so that we can write the file *
	 *****************************************************/

#warning "Check for smaller erase regions"

	erase.start = 0;

	if (flags & FLAG_ERASE_ALL) {
		erase.length = mtd.size;
	} else {
		erase.length =
			(filestat.st_size + mtd.erasesize - 1) / mtd.erasesize;
		erase.length *= mtd.erasesize;
	}

	if (get_verbose()) {
		/* if the user wants verbose output, erase 1 block at a time and show him/her what's going on */
		int blocks = erase.length / mtd.erasesize;
		erase.length = mtd.erasesize;
		log_verbose("Erasing blocks: 0/%d (0%%)", blocks);
		for (i = 1; i <= blocks; i++) {
			log_verbose("\rErasing blocks: %d/%d (%d%%)", i, blocks,
				    PERCENTAGE(i, blocks));
			safe_memerase(dev_fd, device, &erase);
			erase.start += mtd.erasesize;
		}
		log_verbose("\rErasing blocks: %d/%d (100%%)\n", blocks,
			    blocks);
	} else {
		/* if not, erase the whole chunk in one shot */
		safe_memerase(dev_fd, device, &erase);
	}
	DEBUG("Erased %u / %luk bytes\n", erase.length, filestat.st_size);

	/**********************************
	 * write the entire file to flash *
	 **********************************/

	log_verbose("Writing data: 0k/%lluk (0%%)",
		    KB((unsigned long long)filestat.st_size));
	size = filestat.st_size;
	i = mtd.erasesize;
	written = 0;
	while (size) {
		if (size < mtd.erasesize)
			i = size;
		log_verbose("\rWriting data: %dk/%lluk (%llu%%)",
			    KB(written + i),
			    KB((unsigned long long)filestat.st_size),
			    PERCENTAGE((unsigned long long)written + i,
				       (unsigned long long)filestat.st_size));

		/* read from filename */
		safe_read(fil_fd, bin_filename, src, i);

		/* write to device */
		safe_write(dev_fd, src, i, written,
			   (unsigned long long)filestat.st_size, device);

		written += i;
		size -= i;
	}
	log_verbose("\rWriting data: %lluk/%lluk (100%%)\n",
		    KB((unsigned long long)filestat.st_size),
		    KB((unsigned long long)filestat.st_size));
	DEBUG("Wrote %d / %lluk bytes\n", written,
	      (unsigned long long)filestat.st_size);

	/**********************************
	 * verify that flash == file data *
	 **********************************/

	safe_rewind(fil_fd, bin_filename);
	safe_rewind(dev_fd, device);
	size = filestat.st_size;
	i = mtd.erasesize;
	written = 0;
	log_verbose("Verifying data: 0k/%lluk (0%%)",
		    KB((unsigned long long)filestat.st_size));
	while (size) {
		if (size < mtd.erasesize)
			i = size;
		log_verbose("\rVerifying data: %luk/%lluk (%llu%%)",
			    KB(written + i),
			    KB((unsigned long long)filestat.st_size),
			    PERCENTAGE((unsigned long long)written + i,
				       (unsigned long long)filestat.st_size));

		/* read from filename */
		safe_read(fil_fd, bin_filename, src, i);

		/* read from device */
		safe_read(dev_fd, device, dest, i);

		/* compare buffers */
		if (memcmp(src, dest, i))
			log_failure(
				"File does not seem to match flash data. First mismatch at 0x%.8zx-0x%.8zx\n",
				written, written + i);

		written += i;
		size -= i;
	}
	log_verbose("\rVerifying data: %lluk/%lluk (100%%)\n",
		    KB((unsigned long long)filestat.st_size),
		    KB((unsigned long long)filestat.st_size));
	DEBUG("Verified %d / %lluk bytes\n", written,
	      (unsigned long long)filestat.st_size);

	// Cleanup device handler and file handler
	cleanup();
	usleep(10000);

	// Remove the spi_rockchip module
	ret = DELETE_MODULE("spi_rockchip", O_TRUNC);
	if (ret != 0)
		log_verbose("rmmod failed with return code: %d\n", ret);

	// Toggle the SPI flash access to the FPGA
	flash_access_to_fpga();

	// Free memory allocated to the file
	free(bin_filename);

	exit(EXIT_SUCCESS);

	/*********************************************
	 * Copy different blocks from file to device *
	 ********************************************/
DIFF_BLOCKS:
	safe_rewind(fil_fd, bin_filename);
	safe_rewind(dev_fd, device);
	size = filestat.st_size;
	i = mtd.erasesize;
	erase.start = 0;
	erase.length = (filestat.st_size + mtd.erasesize - 1) / mtd.erasesize;
	erase.length *= mtd.erasesize;
	written = 0;
	unsigned long current_dev_block = 0;
	int diffBlock = 0;
	int blocks = erase.length / mtd.erasesize;
	erase.length = mtd.erasesize;

	log_verbose("\rProcessing blocks: 0/%d (%d%%)", blocks,
		    PERCENTAGE(0, blocks));
	for (int s = 1; s <= blocks; s++) {
		if (size < mtd.erasesize)
			i = size;
		log_verbose("\rProcessing blocks: %d/%d (%d%%)", s, blocks,
			    PERCENTAGE(s, blocks));

		/* read from filename */
		safe_read(fil_fd, bin_filename, src, i);

		/* read from device */
		current_dev_block = safe_lseek(dev_fd, 0, SEEK_CUR, device);
		safe_read(dev_fd, device, dest, i);

		/* compare buffers, if not the same, erase and write the block */
		if (memcmp(src, dest, i)) {
			diffBlock++;
			/* erase block */
			safe_lseek(dev_fd, current_dev_block, SEEK_SET, device);
			safe_memerase(dev_fd, device, &erase);

			/* write to device */
			safe_lseek(dev_fd, current_dev_block, SEEK_SET, device);
			safe_write(dev_fd, src, i, written,
				   (unsigned long long)filestat.st_size,
				   device);

			/* read from device */
			safe_lseek(dev_fd, current_dev_block, SEEK_SET, device);
			safe_read(dev_fd, device, dest, i);

			/* compare buffers for write success */
			if (memcmp(src, dest, i))
				log_failure(
					"File does not seem to match flash data. First mismatch at 0x%.8zx-0x%.8zx\n",
					written, written + i);
		}

		erase.start += i;
		written += i;
		size -= i;
	}

	log_verbose("\ndiff blocks: %d\n", diffBlock);

	exit(EXIT_SUCCESS);
}
