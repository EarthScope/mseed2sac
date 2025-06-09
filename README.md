# mseed2sac - Convert miniSEED time series data to SAC format

## Documentation

For usage infromation see the [mseed2sac manual](doc/mseed2sac.md) in the
'doc' directory.

## Downloading and building

The [releases](https://github.com/EarthScope/mseed2sac/releases) area
contains release versions.

In most Unix/Linux environments a simple `make` will build the program.

The `CC` and `CFLAGS` environment variables can be used to configure
the build parameters.

In the Win32 environment the Makefile.win can be used with the nmake
build tool included with Visual Studio.

## Building without zlib support

If your system does not have zlib you can compile the program without
support for ZIP archive output by defining the NOZIP environment variable:

```Console
NOZIP=1 make
```

## Licensing

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

[http://www.apache.org/licenses/LICENSE-2.0](http://www.apache.org/licenses/LICENSE-2.0)

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Copyright (C) 2025 Chad Trabant, EarthScope Data Services
