# <p >mseed2sac - miniSEED to SAC converter</p>

1. [Name](#)
1. [Synopsis](#synopsis)
1. [Description](#description)
1. [Options](#options)
1. [Metadata Files](#metadata-files)
1. [Selection File](#selection-file)
1. [Input List Files](#input-list-files)
1. [About Sac](#about-sac)
1. [Author](#author)

## <a id='synopsis'>Synopsis</a>

<pre >
mseed2sac [options] file1 [file2 file3 ...]
</pre>

## <a id='description'>Description</a>

<p ><b>mseed2sac</b> converts miniSEED waveform data to SAC format.  The output SAC file can be either ASCII or binary (either byte-order), the default is binary with the same byte-order as the host computer.  By default all aspects of the input files are automatically detected.</p>

<p >If an input file name is prefixed with an '@' character the file is assumed to contain a list of input data files, see <i>INPUT LIST FILES</i> below.</p>

<p >A separate output file is written for each continuous time-series in the input data.  Output file names are of the form:</p>

<pre >
"Net.Sta.Loc.Chan.Qual.YYYY,DDD,HHMMSS.SAC"

For example:
"TA.ELFS..LHZ.R.2006,123,153619.SAC"
</pre>

<p >Files that would have the same file name due to having the same start time will be kept separate by adding a digit to file name.  The <i>-O</i> argument changes this behavior to allow overwriting of existing file names.</p>

<p >If the input file name is "-" input miniSEED records will be read from standard input.</p>

<p >The SAC header variable KHOLE is used synonymously with the SEED location code.  Any location codes found in the input miniSEED or metadata file are put into the KHOLE variable.</p>

## <a id='options'>Options</a>

<b>-V</b>

<p style="padding-left: 30px;">Print program version and exit.</p>

<b>-h</b>

<p style="padding-left: 30px;">Print program usage and exit.</p>

<b>-H</b>

<p style="padding-left: 30px;">Print extended program usage with all options and exit.</p>

<b>-v</b>

<p style="padding-left: 30px;">Be more verbose.  This flag can be used multiple times ("-v -v" or "-vv") for more verbosity.</p>

<b>-O</b>

<p style="padding-left: 30px;">Overwrite an existing files instead of generating new file names.</p>

<b>-k </b><i>lat/lon</i>

<p style="padding-left: 30px;">Specify station coordinates to put into the output SAC file(s). The argument format is "Latitude/Longitude" e.g. "47.66/-122.31". Coordinates specified with this option override any coordinates found in the metadata file.</p>

<b>-m </b><i>metafile</i>

<p style="padding-left: 30px;">Specify a file containing metadata such as coordinates, elevation, component orientation, scaling factor, etc.  For each time-series written any matching metadata will be added to the SAC header.  See <i>METADATA FILES</i> below.</p>

<b>-M </b><i>metaline</i>

<p style="padding-left: 30px;">Specify a "line" of metadata in the same format as expected for the <i>METADATA FILES</i>.  This option may be specified multiple times.</p>

<b>-msi</b>

<p style="padding-left: 30px;">Convert any component inclination values in a metadata file from SEED (dip) to SAC convention, this is a simple matter of adding 90 degrees.</p>

<b>-E </b><i>event</i>

<p style="padding-left: 30px;">Specify event parameters to add to the SAC file in the following format:</p>

<pre style="padding-left: 30px;">
"Time[/Lat][/Lon][/Depth][/Name]"

For example:
"2006,123,15:27:08.7/-20.33/-174.03/65.5/Tonga"
</pre>

<p >The parameters later in the string are optional.</p>

<b>-l </b><i>selectfile</i>

<p style="padding-left: 30px;">Limit to miniSEED records that match a selection in the specified file.  The selection file contains parameters to match the network, station, location, channel, quality and time range for input records. This option only trims data to SEED record granularity, not sample granularity.  For more details see the <b>SELECTION FILE</b> section below.</p>

<b>-f </b><i>format</i>

<p style="padding-left: 30px;">The default output format is binary SAC with the same byte order as the host computer.  This option forces the format for every output file:</p>

<pre style="padding-left: 30px;">
1 : Alphanumeric SAC format
2 : Binary SAC format, host byte order (default)
3 : Binary SAC format, little-endian
4 : Binary SAC format, big-endian
</pre>

<b>-N </b><i>netcode</i>

<p style="padding-left: 30px;">Specify the network code to use, overriding any network code in the input miniSEED.</p>

<b>-S </b><i>station</i>

<p style="padding-left: 30px;">Specify the station code to use, overriding any station code in the input miniSEED.</p>

<b>-L </b><i>location</i>

<p style="padding-left: 30px;">Specify the location code to use, overriding any location code in the input miniSEED.</p>

<b>-C </b><i>channel</i>

<p style="padding-left: 30px;">Specify the channel code to use, overriding any channel code in the input miniSEED.</p>

<b>-r </b><i>bytes</i>

<p style="padding-left: 30px;">Specify the miniSEED record length in <i>bytes</i>, by default this is autodetected.</p>

<b>-i</b>

<p style="padding-left: 30px;">Process each input file individually.  By default all input files are read and all data is buffered in memory before SAC files are written. This allows time-series spanning mutilple input files to be merged and written in a single SAC file.  The intention is to use this option when processing large amounts of data in order to keep memory usage within reasonable limits.</p>

<b>-ic</b>

<p style="padding-left: 30px;">Process each input channel individually.  Similar to the <i>-i</i> option, except this instructs the program to create write SAC files for each channel (determined when the input channel changes).  Data should be well ordered by channel for best results.  This option can be used to reduce memory usage for very large input files containing many channels.</p>

<b>-dr</b>

<p style="padding-left: 30px;">Use the sampling rate derived from the start and end times and the number of samples instead of the rate specified in the input data. This is useful when the sample rate in the input data does not have enough resolution to represent the true rate.</p>

<b>-z </b><i>zipfile</i>

<p style="padding-left: 30px;">Create a ZIP archive containing all SAC files instead of writing individual files.  Each file is compressed with the deflate method. Specify <b>"-"</b> (dash) to write ZIP archive to stdout.</p>

<b>-z0 </b><i>zipfile</i>

<p style="padding-left: 30px;">Same as <i>"-z"</i> except do not compress the SAC files.  Specify <b>"-"</b> (dash) to write ZIP archive to stdout.</p>

## <a id='metadata-files'>Metadata Files</a>

<p >A metadata file contains a list of station parameters, some of which can be stored in SAC but not in miniSEED.  Each line in a metadata file should be a list of parameters in the order shown below.  Each parameter should be separated with a comma or a vertical bar (|). <b>DIP CONVENTION:</b> When comma separators are used the dip field (CMPINC) is assumed to be in the SAC convention (degrees down from vertical up/outward), if vertical bars are used the dip field is assumed to be in the SEED convention (degrees down from horizontal) and converted to SAC convention.</p>

<p ><b>Metdata fields</b>:</p>
<pre >
Network (KNETWK)
Station (KSTNM)
Location (KHOLE)
Channel (KCMPNM)
Latitude (STLA)
Longitude (STLO)
Elevation (STEL), in meters [not currently used by SAC]
Depth (STDP), in meters [not currently used by SAC]
Component Azimuth (CMPAZ), degrees clockwise from north
Component Incident Angle (CMPINC), degrees from vertical
Instrument Name (KINST), up to 8 characters
Scale Factor (SCALE)
Scale Frequency, unused
Scale Units, unused
Sampling rate, unused
Start time, used for matching
End time, used for matching

Example with comma separators (with SAC convention dip):

------------------
#net,sta,loc,chan,lat,lon,elev,depth,azimuth,SACdip,instrument,scale,scalefreq,scaleunits,samplerate,start,end
IU,ANMO,00,BH1,34.945981,-106.457133,1671,145,328,90,Geotech KS-54000,3456610000,0.02,M/S,20,2008-06-30T20:00:00,2599-12-31T23:59:59
IU,ANMO,00,BH2,34.945981,-106.457133,1671,145,58,90,Geotech KS-54000,3344370000,0.02,M/S,20,2008-06-30T20:00:00,2599-12-31T23:59:59
IU,ANMO,00,BHZ,34.945981,-106.457133,1671,145,0,0,Geotech KS-54000,3275080000,0.02,M/S,20,2008-06-30T20:00:00,2599-12-31T23:59:59
IU,ANMO,10,BH1,34.945913,-106.457122,1767.2,48.8,64,90,Guralp CMG3-T,32805600000,0.02,M/S,40,2008-06-30T20:00:00,2599-12-31T23:59:59
IU,ANMO,10,BH2,34.945913,-106.457122,1767.2,48.8,154,90,Guralp CMG3-T,32655000000,0.02,M/S,40,2008-06-30T20:00:00,2599-12-31T23:59:59
IU,ANMO,10,BHZ,34.945913,-106.457122,1767.2,48.8,0,0,Guralp CMG3-T,33067200000,0.02,M/S,40,2008-06-30T20:00:00,2599-12-31T23:59:59
------------------

Example with vertical bar separators (with SEED convention dip):

------------------
#net|sta|loc|chan|lat|lon|elev|depth|azimuth|SEEDdip|instrument|scale|scalefreq|scaleunits|samplerate|start|end
IU|ANMO|00|BH1|34.945981|-106.457133|1671|145|328|0|Geotech KS-54000|3456610000|0.02|M/S|20|2008-06-30T20:00:00|2599-12-31T23:59:59
IU|ANMO|00|BH2|34.945981|-106.457133|1671|145|58|0|Geotech KS-54000|3344370000|0.02|M/S|20|2008-06-30T20:00:00|2599-12-31T23:59:59
IU|ANMO|00|BHZ|34.945981|-106.457133|1671|145|0|-90|Geotech KS-54000|3275080000|0.02|M/S|20|2008-06-30T20:00:00|2599-12-31T23:59:59
------------------

As a special case '--' can be used to match an empty location code.
</pre>

<p >For each time-series written, metadata from the first line with matching source name parameters (network, station, location and channel) and time window (if specified) will be inserted into the SAC header.  All parameters are optional except for the first four fields specifying the source name parameters.</p>

<p >Simple wildcarding: for the source name parameters that will be matched a '*' character in a field will match anything.  The BHZ metadata lines above, for example, can be (almost) summarized as:</p>

<pre >
IU,ANMO,*,BHZ,34.9459,-106.4571,1671,145,0,0,Geotech KS-54000,3456610000,0.02,M/S,20,2008-06-30T20:00:00,2599-12-31T23:59:59
</pre>

## <a id='selection-file'>Selection File</a>

<p >A selection file is used to match input data records based on network, station, location and channel information.  Optionally a quality and time range may also be specified for more refined selection.  The non-time fields may use the '*' wildcard to match multiple characters and the '?' wildcard to match single characters.  Character sets may also be used, for example '[ENZ]' will match either E, N or Z. The '#' character indicates the remaining portion of the line will be ignored.</p>

<p >Example selection file entires (the first four fields are required)</p>
<pre >
#net sta  loc  chan  qual  start             end
IU   ANMO *    BH?
II   *    *    *     Q
IU   COLA 00   LH[ENZ] R
IU   COLA 00   LHZ   *     2008,100,10,00,00 2008,100,10,30,00
</pre>

## <a id='input-list-files'>Input List Files</a>

<p >If an input file is prefixed with an '@' character the file is assumed to contain a list of file for input.  Multiple list files can be combined with multiple input files on the command line.  The last, space separated field on each line is assumed to be the file name to be read.</p>

<p >An example of a simple text list:</p>

<pre >
TA.ELFS..LHE.R.mseed
TA.ELFS..LHN.R.mseed
TA.ELFS..LHZ.R.mseed
</pre>

## <a id='about-sac'>About Sac</a>

<p >Seismic Analysis Code (SAC) is a general purpose interactive program designed for the study of sequential signals, especially timeseries data.  Originally developed at the Lawrence Livermore National Laboratory the SAC software package is also available from IRIS.</p>

## <a id='author'>Author</a>

<pre >
Chad Trabant
IRIS Data Management Center
</pre>


(man page 2017/09/29)
