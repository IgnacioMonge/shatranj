# Read-only dump TAP. Not linked into product overlays.
ZCC ?= zcc
BUILD ?= ../../build/journal_dump

.PHONY: all clean

all: $(BUILD)/DUMPJRNL.tap

$(BUILD)/DUMPJRNL.tap: dump.c esx_dump_fileio.asm
	mkdir -p $(BUILD)
	$(ZCC) +zx -vn -startup=31 -clib=sdcc_iy -compiler=sdcc \
		-Cs--no-reg-params -SO3 \
		dump.c esx_dump_fileio.asm \
		-o $(BUILD)/DUMPJRNL -create-app

clean:
	rm -rf $(BUILD)
