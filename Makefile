CC := clang
CFLAGS := -Wall -Wextra

SRC := flashcp.c h2b.c

all: fcp

fcp: $(SRC)
	@$(CC) $(CFLAGS) $(SRC) -o fcp || \
	gcc $(CFLAGS) $(SRC) -o fcp

clean:
	rm -rf fcp
