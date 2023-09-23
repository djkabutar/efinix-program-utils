CC := clang
CFLAGS := -Wall -Wextra

SRC := flashcp.c h2b.c
SRC_READ := read_from_flash.c

all: fcp read_from_flash

fcp: $(SRC)
	@$(CC) $(CFLAGS) $(SRC) -o fcp || \
	gcc $(CFLAGS) $(SRC) -o fcp

read_from_flash: $(SRC_READ)
	@$(CC) $(CFLAGS) $(SRC_READ) -o read_from_flash || \
	gcc $(CFLAGS) $(SRC_READ) -o read_from_flash

clean:
	rm -rf fcp read_from_flash
