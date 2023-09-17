all:
	clang flashcp.c h2b.c -o fcp
	clang read_from_flash.c -o read_from_flash

clean:
	rm -rf fcp read_from_flash
