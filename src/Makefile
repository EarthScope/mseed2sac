
# Standard compiler parameters
CFLAGS = -O -I../libmseed

# Options specific for GCC
GCC = gcc
GCCFLAGS = -O2 -Wall -I../libmseed

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

gcc64:
	@$(MAKE) "CC=$(GCC)" "CFLAGS=-m64 $(GCCFLAGS)"

static:
	@$(MAKE) "CC=$(CC)" "CFLAGS=-static $(GCCFLAGS)"

debug:
	$(MAKE) "CFLAGS=-g $(CFLAGS)"

gccdebug:
	$(MAKE) "CC=$(GCC)" "CFLAGS=-g $(GCCFLAGS)"

install:
	@echo
	@echo "No install target, copy the executable(s) yourself"
	@echo
