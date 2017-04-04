# mseed2sac - Convert miniSEED time series data to SAC

## Documentation

For usage infromation see the [mseed2sac manual](doc/mseed2sac.md) in the
'doc' directory.

## Downloading and building

The [releases](https://github.com/iris-edu/mseed2sac/releases) area
contains release versions.

In most Unix/Linux environments a simple 'make' will build the program.

The CC and CFLAGS environment variables can be used to configure
the build parameters.

If your system does not have zlib you can compile the program without
support for ZIP archive output: first type 'make' in the main
directory (the build will fail), then go to the 'src' directory and
type 'make nozip'.

In the Win32 environment the Makefile.win can be used with the nmake
build tool included with Visual Studio.

## Licensing

GNU GPL version 3.  See included LICENSE file for details.

Copyright (c) 2017 Chad Trabant
