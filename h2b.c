/*
 * Copyright (c) 2d3D, Inc.
 * Written by djkabutar <d.kabutarwala@yahoo.com>
 * All rights reserved.
 */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <getopt.h>

#include "h2b.h"

int convert_to_bin(const char* inp, const char* out)
{
	FILE *input;
	FILE *output;
	char *line = NULL;
	size_t len = 0;
	ssize_t nread;
	char *data;
	uint8_t *array;
	unsigned int i = 0;
	unsigned int count = 0;
	int length;
	char c;
	size_t elements_written;
	data = malloc(sizeof(char) * 5);
	array = malloc(sizeof(uint8_t) * 3560000);

	input = fopen(inp, "r");
	if (input == NULL) {
		printf("Unable to open %s\n", inp);
		goto err;
	}

	while(!feof(input))
	{
		c = fgetc(input);
		if(c == '\n')
		{
			count++;
		}
	}
	fclose(input);

	input = fopen(inp, "r");
	if (input == NULL) {
		printf("Unable to open %s\n", inp);
		goto err;
	}

	output = fopen(out, "wb");
	if (output == NULL) {
		printf("Unable to open %s\n", out);
		goto err;
	}

	while ((nread = getline(&line, &len, input)) != -1) {
		strcpy(data, "0x");
		strcat(data, line);
		if (nread != 3) {
			printf("File is not properly formatted: %s\n", data);
			goto err;
		}
		data[strcspn(data, "\n")] = 0;
		array[i] = (uint8_t) strtol(data, NULL, 16);
		i++;
	}

	elements_written = fwrite(array, sizeof(uint8_t), count, output);

	if (elements_written != count) {
		printf("File is not properly written!\n");
		goto err;
	}

	free(line);
	free(data);
	free(array);
	fclose(input);
	fclose(output);
	return 0;
err:
	free(line);
	free(data);
	free(array);
	fclose(input);
	fclose(output);
	return -1;
}

/*
int main (int argc,char *argv[])
{
	const char* inp = NULL;
	const char* out = NULL;
	int ret;

	if (argc != 3) {
		printf("Specify inputs and outputs properly\n");
		exit(EXIT_FAILURE);
	}

	inp = argv[1];
	out = argv[2];

	ret = convert_to_bin(inp, out);
	if (ret < 0) 
		exit(EXIT_FAILURE);
	exit(EXIT_SUCCESS);
}
*/
