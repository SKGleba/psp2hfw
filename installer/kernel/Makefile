PLUGIN_OBJS = kernel.o
HEADERS = $(wildcard *.h)

PLUGIN_LIBS = -lSceSysmemForDriver_stub -lSceIofilemgrForDriver_stub -lSceThreadmgrForDriver_stub -lSceSysclibForDriver_stub -lSceSysrootForDriver_stub

PREFIX  = arm-vita-eabi
CC      = $(PREFIX)-gcc
CFLAGS  = -Wl,-q -Wall -O3
ASFLAGS = $(CFLAGS)

all: psp2hfwik.skprx

psp2hfwik.skprx: psp2hfwik.velf
	vita-make-fself -c $< $@

psp2hfwik.velf: psp2hfwik.elf
	vita-elf-create -e exports.yml $< $@

psp2hfwik.elf: $(PLUGIN_OBJS)
	$(CC) $(CFLAGS) $^ $(PLUGIN_LIBS) -o $@ -nostdlib
