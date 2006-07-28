
# Standard compiler parameters
CFLAGS = -O -I..

# Options specific for GCC
GCC = gcc
GCCFLAGS = -O2 -Wall -I..

LDFLAGS = -L..
LDLIBS = -lmseed

all: msview msrepack

msview: msview.o
	$(CC) $(CFLAGS) -o $@ msview.o $(LDFLAGS) $(LDLIBS)

msrepack: msrepack.o
	$(CC) $(CFLAGS) -o $@ msrepack.o $(LDFLAGS) $(LDLIBS)

clean:
	rm -f msview.o msview msrepack.o msrepack mstest.o mstest

cc:
	@$(MAKE) "CC=$(CC)" "CFLAGS=$(CFLAGS)"

gcc:
	@$(MAKE) "CC=$(GCC)" "CFLAGS=$(GCCFLAGS)"

gcc32:
	@$(MAKE) "CC=$(GCC)" "CFLAGS=-m32 $(GCCFLAGS)"

gcc64:
	@$(MAKE) "CC=$(GCC)" "CFLAGS=-m64 $(GCCFLAGS)"

debug:
	$(MAKE) "CFLAGS=-g $(CFLAGS)"

gccdebug:
	$(MAKE) "CC=$(GCC)" "CFLAGS=-g $(GCCFLAGS)"

gcc32debug:
	$(MAKE) "CC=$(GCC)" "CFLAGS=-g -m32 $(GCCFLAGS)"

gcc64debug:
	$(MAKE) "CC=$(GCC)" "CFLAGS=-g -m64 $(GCCFLAGS)"

install:
	@echo
	@echo "No install target, copy the executable(s) yourself"
	@echo
