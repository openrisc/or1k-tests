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

.PHONY: all all-asm all-c clean lib
all: lib all-asm all-c

all-asm: $(STARGETS)
all-c: $(CTARGETS)

lib:
	$(MAKE) --directory=lib

$(BUILDDIR)/%: %.S lib/libsupport.a
	@mkdir -p $(dir $@)
	$(CC) -nostartfiles -Iinclude -Iinclude/$(TARGET) -Llib $< -lsupport -o $@

$(BUILDDIR)/%: %.c lib/libsupport.a
	@mkdir -p $(dir $@)
	$(CC) -Iinclude -Iinclude/$(TARGET) $(CFLAGS) -Llib $< -lsupport -o $@

clean:
	make -C lib/ clean
	rm -rf $(BUILDDIR)
