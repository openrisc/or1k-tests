CROSS_COMPILE?=or1k-elf-
TARGET?=mor1kx_cappuccino
CFLAGS?=-O2 -nostdlib
AS = $(CROSS_COMPILE)as
CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)gcc

CTESTS = $(shell cat $(TARGET).tests | grep .c)
STESTS = $(shell cat $(TARGET).tests | grep .S)
STARGETS = $(STESTS:%.S=$(BUILDDIR)/%)
CTARGETS = $(CTESTS:%.c=$(BUILDDIR)/%)

BUILDDIR=build

.PHONY: all clean libs
all: libs $(STARGETS)

libs:
	make -C lib/

$(STARGETS): $(BUILDDIR)/%: %.S
	@mkdir -p $(dir $@)
	$(CC) -Iinclude -Iinclude/$(TARGET) $(CFLAGS) -Llib $< -lsupport -o $@

clean:
	make -C lib/ clean
	rm -rf $(BUILDDIR)