#
# Wmake File - for Watcom's wmake
# Use 'wmake -f Makefile.wat'

.BEFORE
	@set INCLUDE=.;$(%watcom)\H;$(%watcom)\H\NT
	@set LIB=.;$(%watcom)\LIB386

cc     = wcc386
cflags = -zq 
lflags = OPT quiet OPT map LIBRARY ..\libmseed\libmseed.lib
cvars  = $+$(cvars)$- -DWIN32 -DNOFDZIP

BIN = ..\mseed2sac.exe

INCS = -I..\libmseed

all: $(BIN)

$(BIN):	mseed2sac.obj
	wlink $(lflags) name $(BIN) file {mseed2sac.obj}

# Source dependencies:
mseed2sac.obj:	mseed2sac.c sacformat.h

# How to compile sources:
.c.obj:
	$(cc) $(cflags) $(cvars) $(INCS) $[@ -fo=$@

# Clean-up directives:
clean:	.SYMBOLIC
	del *.obj *.map $(BIN)
