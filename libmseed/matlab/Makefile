
# Matlab/GNU Octave MEX build

# Location of Matlab's mex
MEX = /Applications/MATLAB74/bin/mex

# Location of GNU Octave's mkoctfile
MEX = /usr/local/bin/mkoctfile --mex

LIBSRC = ../libmseed.a

all:
	$(MEX) mexMsReadTracesNative.c $(LIBSRC)
	$(MEX) mexMsReadTraces.c $(LIBSRC)

clean:
	rm -f *.mex* *.o
