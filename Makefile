all:
	gcc flashcp.c h2b.c -o flashcp
	gcc read_from_flash.c -o read_from_flash
