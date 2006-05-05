
# Standard compiler parameters
CFLAGS = -O 

# Options specific for GCC
GCC = gcc
GCCFLAGS = -O2 -Wall

LDFLAGS =
LDLIBS =

LIB_OBJS = fileutils.o genutils.o gswap.o lmplatform.o lookup.o \
           msrutils.o pack.o packdata.o traceutils.o unpack.o \
           unpackdata.o

CURRENT_VER = 1.3
COMPAT_VER = 1.3

LIB_A = libmseed.a
LIB_SO = libmseed.so.$(CURRENT_VER)
LIB_SO_ALIAS = libmseed.so
LIB_DYN = libslink.$(CURRENT_VER).dylib
LIB_DYN_ALIAS = libslink.dylib

all: static

static: $(LIB_A)

shared: $(LIB_SO)

dynamic: $(LIB_DYN)

$(LIB_A): $(LIB_OBJS)
	rm -f $(LIB_A)
	ar -csq $(LIB_A) $(LIB_OBJS)

$(LIB_SO): $(LIB_OBJS)
	rm -f $(LIB_SO) $(LIB_SO_ALIAS)
	$(CC) -shared -Wl,-soname -Wl,$(LIB_SO_ALIAS) -o $(LIB_SO) $(LIB_OBJS)
	ln -s $(LIB_SO) $(LIB_SO_ALIAS)

$(LIB_DYN): $(LIB_OBJS)
	$(CC) -dynamiclib -compatibility_version $(COMPAT_VER) -current_version $(CURRENT_VER) -install_name $(LIB_DYN_ALIAS) -o $(LIB_DYN) $(LIB_OBJS)
	ln -s $(LIB_DYN) $(LIB_DYN_ALIAS)

clean:
	rm -f $(LIB_OBJS) $(LIB_A) $(LIB_SO) $(LIB_SO_ALIAS) $(LIB_DYN) $(LIB_DYN_ALIAS)

cc:
	@$(MAKE) "CC=$(CC)" "CFLAGS=$(CFLAGS)"

gcc:
	@$(MAKE) "CC=$(GCC)" "CFLAGS=$(GCCFLAGS)"

gcc64:
	@$(MAKE) "CC=$(GCC)" "CFLAGS=-m64 $(GCCFLAGS)"

debug:
	$(MAKE) "CC=$(CC)" "CFLAGS=-g $(CFLAGS)"

gccdebug:
	$(MAKE) "CC=$(GCC)" "CFLAGS=-g $(GCCFLAGS)"

install:
	@echo
	@echo "No install target, copy the library and header yourself"
	@echo
