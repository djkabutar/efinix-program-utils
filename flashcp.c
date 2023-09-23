/*
 * Copyright (c) 2023 Vicharak Computer LLP.
 */

#include "flashcp.h"

NORETURN void log_failure(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fflush(stderr);

	exit(EXIT_FAILURE);
}

static int verbose = 0;

void set_verbose(int v)
{
	if (v)
		verbose = 1;
}

int get_verbose(void)
{
	return verbose;
}

void log_verbose(const char *fmt, ...)
{
	va_list ap;

	if (!verbose)
		return;

	va_start(ap, fmt);
	vfprintf(stdout, fmt, ap);
	va_end(ap);
	fflush(stdout);
}

int safe_open(const char *pathname, int flags)
{
	const char *access = "unknown";
	int fd;

	if (!pathname)
		log_failure("No filename specified\n");

	fd = open(pathname, flags);
	if (fd < 0) {
		if (flags & O_RDWR)
			access = "read/write";
		else if (flags & O_RDONLY)
			access = "read";
		else if (flags & O_WRONLY)
			access = "write";

		log_failure("While trying to open %s for %s access: %m\n",
			    pathname, access);
	}

	return (fd);
}

void safe_read(int fd, const char *filename, void *buf, size_t count)
{
	ssize_t result;

	result = read(fd, buf, count);
	if (count != result) {
		log_verbose("\n");
		if (result < 0) {
			log_failure("While reading data from %s: %m\n",
				    filename);
		}
		log_failure("Short read count returned while reading from %s\n",
			    filename);
	}
}

void safe_write(int fd, const void *buf, size_t count, size_t written,
		       unsigned long long to_write, const char *device)
{
	ssize_t result;

	/* write to device */
	result = write(fd, buf, count);
	if (count != result) {
		log_verbose("\n");
		if (result < 0) {
			log_failure(
				"While writing data to 0x%.8lx-0x%.8lx on %s: %m\n",
				written, written + count, device);
		}
		log_failure(
			"Short write count returned while writing to x%.8zx-0x%.8zx on %s: %zu/%llu bytes written to flash\n",
			written, written + count, device, written + result,
			to_write);
	}
}

off_t safe_lseek(int fd, off_t offset, int whence, const char *filename)
{
	off_t off;

	off = lseek(fd, offset, whence);
	if (off < 0) {
		log_failure("While seeking on %s: %m\n", filename);
	}

	return off;
}

void safe_rewind(int fd, const char *filename)
{
	safe_lseek(fd, 0L, SEEK_SET, filename);
}

void safe_memerase(int fd, const char *device,
			  struct erase_info_user *erase)
{
	if (ioctl(fd, MEMERASE, erase) < 0) {
		log_verbose("\n");
		log_failure("While erasing blocks 0x%.8x-0x%.8x on %s: %m\n",
			    (unsigned int)erase->start,
			    (unsigned int)(erase->start + erase->length),
			    device);
	}
}

int gpio_export(const char *pin)
{
	const char *exp = "/sys/class/gpio/export";

	int fd = -1;

	fd = open(exp, O_WRONLY);
	if (fd < 0) {
		log_verbose("Failed to open %s\n", exp);
		return fd;
	}

	if (write(fd, pin, strlen(pin)) == -1)
		log_verbose("Failed to export pin %s to %s\n", pin, exp);

	close(fd);

	return 0;
}

int gpio_unexport(const char *pin)
{
	const char *unexport = "/sys/class/gpio/unexport";

	int fd = -1;

	fd = open(unexport, O_WRONLY);
	if (fd < 0) {
		log_verbose("Failed to open %s\n", unexport);
		return fd;
	}

	if (write(fd, pin, strlen(pin)) == -1)
		log_failure("Failed to unexport pin %s\n", pin);

	close(fd);

	return 0;
}

void gpio_set_direction(const char *pin, const char *direction)
{
	int fd = -1;
	char *gpio_direction = NULL;
	size_t gpio_direction_len;

	// Calculate the required length for gpio_direction
	gpio_direction_len = strlen("/sys/class/gpio/gpio") + strlen(pin) +
			     strlen("/direction") + 1;

	gpio_direction = (char *)malloc(gpio_direction_len);
	if (!gpio_direction)
		log_failure("Failed to allocate memory for gpio_direction\n");

	snprintf(gpio_direction, gpio_direction_len,
		 "/sys/class/gpio/gpio%s/direction", pin);

	if (access(gpio_direction, F_OK) == -1)
		goto free_gpio;

	fd = open(gpio_direction, O_WRONLY);
	if (fd < 0) {
		free(gpio_direction);
		log_failure("Failed to open %s\n", gpio_direction);
	}

	if (write(fd, direction, strlen(direction)) == -1)
		log_verbose("Failed to write direction to %s\n",
			    gpio_direction);

	close(fd);
free_gpio:
	free(gpio_direction);
}

void gpio_set_value(const char *pin, const char *value)
{
	const char *gpio_path = "/sys/class/gpio";
	const char *default_gpio = "/sys/class/gpio/gpio";
	char *gpio_value = NULL;
	size_t gpio_value_len;
	int ret = 0, fd = -1;

	// Calculate the required length for gpio_value
	gpio_value_len =
		strlen(default_gpio) + strlen(pin) + strlen("/value") + 1;

	gpio_value = (char *)malloc(gpio_value_len);
	if (!gpio_value)
		log_failure("Failed to allocate memory for gpio_value\n");

	snprintf(gpio_value, gpio_value_len, "%s%s/value", default_gpio, pin);

	ret = gpio_export(pin);
	if (ret)
		goto free_gpio;

	gpio_set_direction(pin, "out");

	fd = open(gpio_value, O_WRONLY);
	if (fd < 0)
		goto free_gpio;

	if (write(fd, value, 1) == -1)
		log_verbose("Failed to write value to %s\n", gpio_value);

	close(fd);
free_gpio:
	free(gpio_value);
}

void flash_access_to_processor(void)
{
	gpio_set_value(RESET_GPIO, "0");
	gpio_set_value(CONDONE_GPIO, "0");
}

void flash_access_to_fpga(void)
{
	gpio_set_value(RESET_GPIO, "1");
	gpio_set_value(CONDONE_GPIO, "1");
}
