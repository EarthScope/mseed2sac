
# Build environment can be configured the following
# environment variables:
#   CC : Specify the C compiler to use
#   CFLAGS : Specify compiler options to use

# Options specific for GCC
GCC = gcc
GCCFLAGS = -O2 -Wall -I../libmseed

# Required compiler parameters
REQCFLAGS = -I../libmseed

BIN = mseed2sac

LDFLAGS = -L../libmseed
LDLIBS = -lm -lmseed

OBJS = $(BIN).o

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o ../$@ $(OBJS) $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(OBJS) ../$(BIN)

cc:
	@$(MAKE) "CC=$(CC)" "CFLAGS=$(CFLAGS)"

gcc:
	@$(MAKE) "CC=$(GCC)" "CFLAGS=$(GCCFLAGS)"

gcc32:
	@$(MAKE) "CC=$(GCC)" "CFLAGS=-m32 $(GCCFLAGS)"

gcc64:
	@$(MAKE) "CC=$(GCC)" "CFLAGS=-m64 $(GCCFLAGS)"

static:
	@$(MAKE) "CC=$(CC)" "CFLAGS=-static $(GCCFLAGS)"

debug:
	$(MAKE) "CFLAGS=-g $(CFLAGS)"

gccdebug:
	$(MAKE) "CC=$(GCC)" "CFLAGS=-g $(GCCFLAGS)"

gcc32debug:
	$(MAKE) "CC=$(GCC)" "CFLAGS=-g -m32 $(GCCFLAGS)"

gcc64debug:
	$(MAKE) "CC=$(GCC)" "CFLAGS=-g -m64 $(GCCFLAGS)"

# Implicit rule for building object files
%.o: %.c
	$(CC) $(CFLAGS) $(REQCFLAGS) -c $<

install:
	@echo
	@echo "No install target, copy the executable(s) yourself"
	@echo
