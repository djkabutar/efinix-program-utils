/*
 * Copyright (c) 2d3D, Inc.
 * Written by djkabutar <d.kabutarwala@yahoo.com>
 * All rights reserved.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>

int convert_to_bin(const char *inp, const char *out)
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
	int ret = 0;
	char c;
	size_t elements_written;
	data = malloc(sizeof(char) * 5);
	array = malloc(sizeof(uint8_t) * 3560000);

	if (!inp) {
		printf("Specify filename!\n");
		ret = -1;
		goto err_input;
	}

	input = fopen(inp, "r");
	if (!input) {
		printf("Unable to open %s\n", inp);
		ret = -1;
		goto err_input;
	}

	while (!feof(input)) {
		c = fgetc(input);
		if (c == '\n') {
			count++;
		}
	}
	fseek(input, 0, SEEK_SET);

	output = fopen(out, "wb");
	if (output == NULL) {
		printf("Unable to open %s\n", out);
		ret = -1;
		goto err_output;
	}

	while ((nread = getline(&line, &len, input)) != -1) {
		strcpy(data, "0x");
		strcat(data, line);
		if (nread != 3) {
			printf("File is not properly formatted: %s\n", data);
			ret = -1;
			goto err_read;
		}
		data[strcspn(data, "\n")] = 0;
		array[i] = (uint8_t)strtol(data, NULL, 16);
		i++;
	}

	elements_written = fwrite(array, sizeof(uint8_t), count, output);

	if (elements_written != count) {
		printf("File is not properly written!\n");
		ret = -1;
	}

err_read:
	free(line);
	fclose(output);
err_output:
	fclose(input);
err_input:
	free(data);
	free(array);

	return ret;
}
