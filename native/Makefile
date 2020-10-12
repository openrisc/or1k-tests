CROSS_COMPILE?=or1k-elf-
TARGET?=mor1kx_cappuccino
CFLAGS=-Wall -O2
AS = $(CROSS_COMPILE)as
CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)gcc

CTESTS = $(shell cat $(TARGET).tests | grep \\.c)
STESTS = $(shell cat $(TARGET).tests | grep .S)
STARGETS = $(STESTS:%.S=$(BUILDDIR)/%)
CTARGETS = $(CTESTS:%.c=$(BUILDDIR)/%)

BUILDDIR=build

.PHONY: all all-asm all-c clean lib help
all: all-asm all-c

all-asm: lib $(STARGETS)
all-c: lib $(CTARGETS)

lib:
	$(MAKE) --directory=$@

$(BUILDDIR)/%: %.S
	@mkdir -p $(dir $@)
	$(CC) -nostartfiles -Iinclude -Iinclude/$(TARGET) $(CFLAGS) -Llib $< -lsupport -o $@

$(BUILDDIR)/%: %.c
	@mkdir -p $(dir $@)
	$(CC) -Iinclude -Iinclude/$(TARGET) $(CFLAGS) -Llib $< -lsupport -o $@

clean:
	make -C lib/ clean
	rm -rf $(BUILDDIR)
