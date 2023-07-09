all:
	gcc flashcp.c h2b.c -o flashcp
	gcc read_from_flash.c -o read_from_flash

clean:
	rm -rf flashcp read_from_flash
