#include "flashcp.h"

int main(int argc, char *argv[])
{
	gpio_set_value(RESET_GPIO, "1");
	gpio_set_value(CONDONE_GPIO, "1");

	exit(EXIT_SUCCESS);
}
