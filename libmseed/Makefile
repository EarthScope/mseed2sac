
# Build environment can be configured the following
# environment variables:
#   CC : Specify the C compiler to use
#   CFLAGS : Specify compiler options to use

# Options specific for GCC
GCC = gcc
GCCFLAGS = -O2 -Wall -fPIC

LIB_OBJS = fileutils.o genutils.o gswap.o lmplatform.o lookup.o \
           msrutils.o pack.o packdata.o traceutils.o unpack.o \
           unpackdata.o logging.o

CURRENT_VER = 2.0
COMPAT_VER = 2.0

LIB_A = libmseed.a
LIB_SO = libmseed.so.$(CURRENT_VER)
LIB_SO_ALIAS = libmseed.so
LIB_DYN = libmseed.$(CURRENT_VER).dylib
LIB_DYN_ALIAS = libmseed.dylib

all: static

static: $(LIB_A)

shared: gcc $(LIB_SO)

dynamic: gcc $(LIB_DYN)

$(LIB_A): $(LIB_OBJS)
	rm -f $(LIB_A)
	ar -csq $(LIB_A) $(LIB_OBJS)

$(LIB_SO): $(LIB_OBJS)
	rm -f $(LIB_SO) $(LIB_SO_ALIAS)
	$(GCC) $(GCCFLAGS) -shared -Wl,-soname -Wl,$(LIB_SO_ALIAS) -o $(LIB_SO) $(LIB_OBJS)
	ln -s $(LIB_SO) $(LIB_SO_ALIAS)

$(LIB_DYN): $(LIB_OBJS)
	rm -f $(LIB_DYN) $(LIB_DYN_ALIAS)
	$(GCC) $(GCCFLAGS) -dynamiclib -compatibility_version $(COMPAT_VER) -current_version $(CURRENT_VER) -install_name $(LIB_DYN_ALIAS) -o $(LIB_DYN) $(LIB_OBJS)
	ln -sf $(LIB_DYN) $(LIB_DYN_ALIAS)

clean:
	rm -f $(LIB_OBJS) $(LIB_A) $(LIB_SO) $(LIB_SO_ALIAS) $(LIB_DYN) $(LIB_DYN_ALIAS)

cc:
	@$(MAKE) "CC=$(CC)" "CFLAGS=$(CFLAGS)"

gcc:
	@$(MAKE) "CC=$(GCC)" "CFLAGS=$(GCCFLAGS)"

gcc32:
	@$(MAKE) "CC=$(GCC)" "CFLAGS=-m32 $(GCCFLAGS)"

gcc64:
	@$(MAKE) "CC=$(GCC)" "CFLAGS=-m64 $(GCCFLAGS)"

debug:
	$(MAKE) "CC=$(CC)" "CFLAGS=-g $(CFLAGS)"

gccdebug:
	$(MAKE) "CC=$(GCC)" "CFLAGS=-g $(GCCFLAGS)"

gcc32debug:
	$(MAKE) "CC=$(GCC)" "CFLAGS=-g -m32 $(GCCFLAGS)"

gcc64debug:
	$(MAKE) "CC=$(GCC)" "CFLAGS=-g -m64 $(GCCFLAGS)"

gcc32gprof:
	$(MAKE) "CC=$(GCC)" "CFLAGS=-pg -m32 $(GCCFLAGS)"

gcc64gprof:
	$(MAKE) "CC=$(GCC)" "CFLAGS=-pg -m64 $(GCCFLAGS)"

install:
	@echo
	@echo "No install target, copy the library and header yourself"
	@echo
