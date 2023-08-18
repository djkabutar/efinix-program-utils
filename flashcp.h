#include <errno.h>
#include <fcntl.h>
#include <mtd/mtd-user.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <linux/version.h>
#include <unistd.h>

#define PROGRAM_NAME "flashcp"
#define VERSION "1.0"

/* error levels */
#define LOG_NORMAL 1
#define LOG_ERROR 2

/* for tagging functions that always exit */
#if defined(__GNUC__) || defined(__clang__)
#define NORETURN __attribute__((noreturn))
#else
#define NORETURN
#endif

#define RESET_GPIO "509"
#define CONDONE_GPIO "510"

static NORETURN void log_failure(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fflush(stderr);

	exit(EXIT_FAILURE);
}

static int verbose = 0;
static void log_verbose(const char *fmt, ...)
{
	va_list ap;

	if (!verbose)
		return;

	va_start(ap, fmt);
	vfprintf(stdout, fmt, ap);
	va_end(ap);
	fflush(stdout);
}

static int safe_open(const char *pathname, int flags)
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

static void safe_read(int fd, const char *filename, void *buf, size_t count)
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

static void safe_write(int fd, const void *buf, size_t count, size_t written,
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

static off_t safe_lseek(int fd, off_t offset, int whence, const char *filename)
{
	off_t off;

	off = lseek(fd, offset, whence);
	if (off < 0) {
		log_failure("While seeking on %s: %m\n", filename);
	}

	return off;
}

static void safe_rewind(int fd, const char *filename)
{
	safe_lseek(fd, 0L, SEEK_SET, filename);
}

static void safe_memerase(int fd, const char *device,
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

static int gpio_export(const char *pin)
{
	const char *exp = "/sys/class/gpio/export";

	int fd = -1;

	fd = safe_open(exp, O_WRONLY);
	if (fd < 0) {
		log_failure("Failed to open %s\n", exp);
		return fd;
	}
	write(fd, pin, 3);

	close(fd);

	return 0;
}

static int gpio_unexport(const char *pin)
{
	const char *unexport = "/sys/class/gpio/unexport";

	int fd = -1;

	fd = safe_open(unexport, O_WRONLY);
	if (fd < 0) {
		log_failure("Failed to open %s\n", unexport);
		return fd;
	}

	write(fd, pin, 3);

	close(fd);

	return 0;
}

static void gpio_set_direction(const char *pin, const char *direction)
{
	int fd = -1;
	char *gpio_direction = NULL;

	// 64 is enough for "/sys/class/gpio/gpioXXX/direction"
	gpio_direction = (char *)malloc(64);
	if (!gpio_direction)
		log_failure("Failed to allocate memory for gpio_direction\n");

	strcpy(gpio_direction, "/sys/class/gpio/gpio");
	strcat(gpio_direction, pin);
	strcat(gpio_direction, "/direction");

	if (access(gpio_direction, F_OK) == -1)
		goto free_gpio;

	fd = safe_open(gpio_direction, O_WRONLY);
	if (fd < 0) {
		free(gpio_direction);
		log_failure("Failed to open %s\n", gpio_direction);
	}

	write(fd, direction, 3);

	close(fd);
free_gpio:
	free(gpio_direction);
}

static void gpio_set_value(const char *pin, const char *value)
{
	const char *gpio_path = "/sys/class/gpio";
	const char *default_gpio = "/sys/class/gpio/gpio";
	char *gpio_value;
	int ret = 0, fd = -1;

	// 64 is enough for "/sys/class/gpio/gpioXXX/value"
	gpio_value = (char *)malloc(64);
	if (!gpio_value)
		log_failure("Failed to allocate memory for gpio_value\n");

	strcpy(gpio_value, default_gpio);
	strcat(gpio_value, pin);
	strcat(gpio_value, "/value");

	ret = gpio_export(pin);
	if (ret) {
		log_verbose("Failed to export gpio %s\n", pin);
		goto free_gpio;
	}

	gpio_set_direction(pin, "out");

	fd = safe_open(gpio_value, O_WRONLY);
	if (fd < 0) {
		free(gpio_value);
		log_failure("Failed to open %s\n", gpio_value);
	}

	write(fd, value, 1);

	close(fd);
free_gpio:
	free(gpio_value);
}
