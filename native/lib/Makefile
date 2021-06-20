CROSS_COMPILE?=or1k-elf-
CFLAGS?=-Wall -O2 -g
CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar
RANLIB = $(CROSS_COMPILE)ranlib

SSRC = 	cache.S \
	mmu.S 	\
	stack.S
CSRC = utils.c
SOBJ=$(SSRC:.S=.o)
COBJ=$(CSRC:.c=.o)
OBJS=$(COBJ) $(SOBJ)

libsupport.a: $(OBJS)
	$(AR) cru $@ $^
	$(RANLIB) $@

$(SOBJ): %.o: %.S
	$(CC) -c $< -o $@

$(COBJ): %.o: %.c
	$(CC) -I../include $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o *.a *~
