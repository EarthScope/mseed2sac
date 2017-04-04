#
# THIS FILE IS DEPRECATED AND WILL BE REMOVED IN A FUTURE RELEASE
#
# Wmake file - For Watcom's wmake
# Use 'wmake -f Makefile.wat'

all: .SYMBOLIC
	cd libmseed
	wmake -f Makefile.wat
	cd ..\src
	wmake -f Makefile.wat
	cd ..

clean: .SYMBOLIC
	cd libmseed
	wmake -f Makefile.wat clean
	cd ..\src
	wmake -f Makefile.wat clean
	cd ..
