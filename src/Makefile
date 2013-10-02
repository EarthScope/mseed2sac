
# Build environment can be configured the following
# environment variables:
#   CC : Specify the C compiler to use
#   CFLAGS : Specify compiler options to use

# Required compiler parameters
REQCFLAGS = -I../libmseed

LOCALFLAGS = 

BIN = mseed2sac

LDFLAGS = -L../libmseed
LDLIBS = -lm -lmseed

OBJS = $(BIN).o

nozip: LOCALFLAGS = -DNOFDZIP

all: $(BIN)

$(BIN): $(OBJS) fdzipstream.o
	$(CC) $(CFLAGS) -o ../$(BIN) $(OBJS) fdzipstream.o $(LDFLAGS) $(LDLIBS) -lz

nozip: $(OBJS)
	$(CC) $(CFLAGS) -o ../$(BIN) $(OBJS) $(LOCALFLAGS) $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(OBJS) fdzipstream.o ../$(BIN)

# Implicit rule for building object files
%.o: %.c
	$(CC) $(CFLAGS) $(REQCFLAGS) $(LOCALFLAGS) -c $<

install:
	@echo
	@echo "No install target, copy the executable(s) to desired location"
	@echo
