CC = gcc
CFLAGS = -O2 -Wall

ARCH=x86_64

RANLIB=ranlib
CFLAGS += -arch $(ARCH)

# main library build
objects = usdt.o usdt_dof_file.o usdt_tracepoints.o usdt_probe.o usdt_dof.o usdt_dof_sections.o
headers = usdt.h usdt_internal.h

.c.o: $(headers)

all: libusdt.a libusdt.dylib

libusdt.dylib: $(objects)
	$(CC) $(CFLAGS) -dynamiclib -o $@ $^

libusdt.a: $(objects) $(headers)
	rm -f libusdt.a
	$(AR) cru libusdt.a $(objects) 
	$(RANLIB) libusdt.a

# Tracepoints build. 
#
# Otherwise, just choose the appropriate asm file based on the build
# architecture.
usdt_tracepoints_x86_64.o: usdt_tracepoints_x86_64.s
	$(CC) -arch x86_64 -o usdt_tracepoints_x86_64.o -c usdt_tracepoints_x86_64.s

usdt_tracepoints.o: usdt_tracepoints_x86_64.s
	$(CC) $(CFLAGS) -o usdt_tracepoints.o -c usdt_tracepoints_x86_64.s

clean:
	rm -f *.gch
	rm -f *.o
	rm -f libusdt.a
	rm -f libusdt.dylib
	rm -f test_usdt
	rm -f test_usdt64
	rm -f test_mem_usage

.PHONY: clean test

# testing

test_mem_usage: libusdt.a test_mem_usage.o
	$(CC) $(CFLAGS) -o test_mem_usage test_mem_usage.o libusdt.a 

test_usdt: libusdt.a test_usdt.o
	$(CC) $(CFLAGS) -o test_usdt test_usdt.o libusdt.a 

test: test_usdt
	sudo prove test.pl
