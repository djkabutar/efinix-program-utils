#define PROGRAM_NAME "flashcp"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <mtd/mtd-user.h>
#include <getopt.h>
#include <sys/syscall.h>
#include <errno.h>

#include "h2b.h"

#define RESET_GPIO "509"
#define CONDONE_GPIO "510"

static int gpio_set_value(const char *pin, const char *value)
{
	const char *gpio_path = "/sys/class/gpio";
	const char *export = "/sys/class/gpio/export";
	const char *unexport = "/sys/class/gpio/unexport";
	const char *default_gpio = "/sys/class/gpio/gpio";

	char *gpio_direction;
	gpio_direction = malloc(strlen(default_gpio) + 14);
	strcpy(gpio_direction, default_gpio);
	strcat(gpio_direction, pin);
	strcat(gpio_direction, "/direction");

	char *gpio_value;
	gpio_value = malloc(strlen(default_gpio) + 10);
	strcpy(gpio_value, default_gpio);
	strcat(gpio_value, pin);
	strcat(gpio_value, "/value");

	int fd = open(export, O_WRONLY);
	if (fd == -1) {
		printf("Unable to open gpio export\n");
	}

	if (write(fd, pin, 3) != 3) {
		printf("Error writing to Export\n");
	}

	close(fd);

	fd = open(gpio_direction, O_WRONLY);
	if (fd == -1) {
		printf("Unable to open %s\n", gpio_direction);
	}

	if (write(fd, "out", 3) != 3) {
		printf("Error writing to %s\n", gpio_direction);
	}

	close(fd);

	fd = open(gpio_value, O_WRONLY);
	if (fd == -1) {
		printf("Unable to open %s\n", gpio_value);
	}

	if (write(fd, value, 1) != 1) {
		printf("Error writing to %s\n", gpio_value);
	}

	close(fd);

	fd = open(unexport, O_WRONLY);
	if (fd == -1) {
		printf("Unable to open %s\n", unexport);
	}

	if (write(fd, pin, 3) != 3) {
		printf("Error writing to %s\n", unexport);
	}

	close(fd);

	return 0;
}

int main(int argc, char *argv[])
{
	gpio_set_value(RESET_GPIO, "1");
	gpio_set_value(CONDONE_GPIO, "1");

	exit(EXIT_SUCCESS);
}
