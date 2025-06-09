# Build environment can be configured the following
# environment variables:
#   CC : Specify the C compiler to use
#   CFLAGS : Specify compiler options to use
#   NOZIP : Set to disable ZIP archive support and zlib dependency

# Required compiler parameters
CFLAGS += -I../libmseed

LDFLAGS += -L../libmseed
LDLIBS += -lm -lmseed

BIN = mseed2sac

# Conditionally include zip support based on NOZIP environment variable
ifdef NOZIP
    # NOZIP is set, disable zip support
    CFLAGS += -DNOFDZIP
    SRCS = mseed2sac.c
else
    # NOZIP is not set, include zip support
    SRCS = mseed2sac.c fdzipstream.c
    LDLIBS += -lz
endif

OBJS = $(SRCS:.c=.o)

.PHONY: all clean install

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o ../$(BIN) $(OBJS) $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(OBJS) ../$(BIN)

install:
	@echo
	@echo "No install target, copy the executable(s) to desired location"
	@echo

# Rule for building object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
