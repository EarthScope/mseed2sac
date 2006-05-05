/***************************************************************************
 * mseed2sac.c
 *
 * Convert Mini-SEED waveform data to SAC
 *
 * Written by Chad Trabant, IRIS Data Management Center
 *
 * modified 2006.124
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>

#include <libmseed.h>

#include "tidecalc.h"
#include "sacformat.h"

#define VERSION "0.1"
#define PACKAGE "mseed2sac"

static int writesac (double *data, int datacnt);
static int parameter_proc (int argcount, char **argvec);
static char *getoptval (int argcount, char **argvec, int argopt);
static void usage (void);

static int   verbose     = 0;
static int   packreclen  = -1;
static int   encoding    = 11;
static int   byteorder   = -1;
static int   srateblkt   = 0;
static int   syear       = 0;
static int   sday        = 0;
static int   shour       = 0;
static int   eyear       = 0;
static int   eday        = 0;
static int   ehour       = 0;
static double interval   = 1.0;
static double latitude   = 0.0;
static double longitude  = 0.0;
static flag  dataformat  = 0; /* 0 = Mini-SEED, 1 = SAC */
static char *network     = "XX";
static char *station     = "TEST";
static char *location    = 0;
static char *channel     = "TID";
static char *outputfile  = 0;
static FILE *ofp         = 0;
static char  nanometers  = 0;
static char  difflag     = 0;
static char  gravflag    = 0;
static char  revflag     = 0;
static long long int datascaling = 0;
static hptime_t starttime  = 0;

int
main (int argc, char **argv)
{
  double *data = 0;
  int datacnt = 0;
  int idx;
  
  /* Process given parameters (command line and parameter file) */
  if (parameter_proc (argc, argv) < 0)
    return -1;
  
  /* Open the output file if specified */
  if ( outputfile )
    {
      if ( strcmp (outputfile, "-") == 0 )
        {
          ofp = stdout;
        }
      else if ( (ofp = fopen (outputfile, "wb")) == NULL )
        {
          fprintf (stderr, "Cannot open output file: %s (%s)\n",
                   outputfile, strerror(errno));
          return -1;
        }
    }
  
  fprintf (stderr, "Calculating tides from %d,%d,%d to %d,%d,%d at %g hour intervals\n",
	   syear, sday, shour, eyear, eday, ehour, interval);
  fprintf (stderr, "For latitude: %g and longitude: %g\n", latitude, longitude);
  
  starttime = ms_time2hptime(syear, sday, shour, 0, 0, 0);
  
  /* Calculate tides */
  if ( tidecalc (&data, &datacnt, latitude, longitude,
		 syear, sday, shour, eyear, eday, ehour,
		 interval, gravflag, verbose) )
    {
      fprintf (stderr, "Error with tidecalc()\n");
      return -1;
    }
  
  /* Convert sample values to nanometers if requested */
  if ( nanometers )
    {
      fprintf (stderr, "Converting sample values from meters to nanometers\n");
      
      for (idx=0; idx < datacnt; idx++)
	data[idx] *= 1e9;
    }
  
  /* Reverse polarity of time-series if requested */
  if ( revflag )
    {
      fprintf (stderr, "Reversing polarity of time-series\n");
      
      for (idx=0; idx < datacnt; idx++)
	data[idx] *= -1.0;
    }
  
  /* Differentiate displacement data if requested */
  if ( difflag )
    {
      fprintf (stderr, "Differentiating time-series\n");
      
      /* Perform two-point differentiation, step size in seconds */
      datacnt = differentiate2 (data, datacnt, (interval * 3600.0), data);
      
      /* Shift the start time by 1/2 the step size */
      starttime += (0.5 * interval * 3600.0) * HPTMODULUS;
    }
  
  if ( dataformat == 1 )
    writesac (data, datacnt);
  else
    writemseed (data, datacnt);
  
  if ( ofp )
    fclose (ofp);
  
  return 0;
}  /* End of main() */


/***************************************************************************
 * writesac:
 * 
 * Write data buffer to output file as binary SAC.
 *
 * Returns the number of samples written.
 ***************************************************************************/
static int
writesac (double *data, int datacnt)
{
  struct SACHeader sh = NullSACHeader;
  BTime btime;
  float *fdata = (float *) data;
  int idx;

  /* Set time-series source parameters */
  if ( network )
    strncpy (sh.knetwk, network, 8);
  if ( station )
    strncpy (sh.kstnm, station, 8);
  if ( location )
    strncpy (sh.khole, location, 8);
  if ( channel )
    strncpy (sh.kcmpnm, channel, 8);
  
  /* Set misc. header variables */
  sh.nvhdr = 6;                 /* Header version = 6 */
  sh.leven = 1;                 /* Evenly spaced data */
  sh.iftype = ITIME;            /* Data is time-series */
  sh.b = 0.0;                   /* First sample, offset time */
  sh.e = (interval-1) * 3600.0; /* Last sample, offset time */
  strncpy (sh.kevnm, "Earth tide", 16);
  
  /* Set station coordinates */
  sh.stla = latitude;
  sh.stlo = longitude;
  sh.stel = 0.0;
  sh.stdp = 0.0;
  
  /* Set start time */
  ms_hptime2btime (starttime, &btime);
  sh.nzyear = btime.year;
  sh.nzjday = btime.day;
  sh.nzhour = btime.hour;
  sh.nzmin = btime.min;
  sh.nzsec = btime.sec;
  sh.nzmsec = btime.fract / 10;

  /* Set sampling interval (seconds), sample count */
  sh.delta = interval * 3600.0;
  sh.npts = datacnt;
  
  //msr->byteorder = byteorder;
  
  /* Convert data buffer to floats, scaling if specified */
  /* Use the same buffer (floats are smaller than doubles) */
  if ( datascaling )
    {
      fprintf (stderr, "Scaling data by %lld and converting to float\n",
	       datascaling);
      
      for (idx=0; idx < datacnt; idx++)
	fdata[idx] = data[idx] * datascaling;
    }
  else
    {
      fprintf (stderr, "Converting data to float\n");
      
      for (idx=0; idx < datacnt; idx++)
	fdata[idx] = data[idx];
    }
  
  /* Write SAC header to output file */
  if ( fwrite (&sh, sizeof(struct SACHeader), 1, ofp) != 1 )
    {
      fprintf (stderr, "Error writing SAC header to output file\n");
      return -1;
    }
  
  /* Write float data to output file */
  if ( fwrite (fdata, sizeof(float), datacnt, ofp) != datacnt )
    {
      fprintf (stderr, "Error writing SAC data to output file\n");
      return -1;
    }
  
  fprintf (stderr, "Wrote %d samples\n", datacnt);
  
  return datacnt;
}  /* End of writesac() */


/***************************************************************************
 * parameter_proc:
 * Process the command line parameters.
 *
 * Returns 0 on success, and -1 on failure.
 ***************************************************************************/
static int
parameter_proc (int argcount, char **argvec)
{
  char *timestr = 0;
  char *coorstr = 0;
  int optind;

  /* Process all command line arguments */
  for (optind = 1; optind < argcount; optind++)
    {
      if (strcmp (argvec[optind], "-V") == 0)
	{
	  fprintf (stderr, "%s version: %s\n", PACKAGE, VERSION);
	  exit (0);
	}
      else if (strcmp (argvec[optind], "-h") == 0)
	{
	  usage();
	  exit (0);
	}
      else if (strncmp (argvec[optind], "-v", 2) == 0)
	{
	  verbose += strspn (&argvec[optind][1], "v");
	}
      else if (strcmp (argvec[optind], "-n") == 0)
	{
	  network = getoptval(argcount, argvec, optind++);
	}
      else if (strcmp (argvec[optind], "-s") == 0)
	{
	  station = getoptval(argcount, argvec, optind++);
	}
      else if (strcmp (argvec[optind], "-l") == 0)
	{
	  location = getoptval(argcount, argvec, optind++);
	}
      else if (strcmp (argvec[optind], "-c") == 0)
	{
	  channel = getoptval(argcount, argvec, optind++);
	}
      else if (strcmp (argvec[optind], "-r") == 0)
	{
	  packreclen = strtoul (getoptval(argcount, argvec, optind++), NULL, 10);
	}
      else if (strcmp (argvec[optind], "-S") == 0)
	{
	  srateblkt = 1;
	}
      else if (strcmp (argvec[optind], "-e") == 0)
	{
	  encoding = strtoul (getoptval(argcount, argvec, optind++), NULL, 10);
	}
      else if (strcmp (argvec[optind], "-b") == 0)
	{
	  byteorder = strtoul (getoptval(argcount, argvec, optind++), NULL, 10);
	}
      else if (strcmp (argvec[optind], "-g") == 0)
	{
	  datascaling = strtoll (getoptval(argcount, argvec, optind++), NULL, 10);
	}
      else if (strcmp (argvec[optind], "-u") == 0)
	{
	  gravflag = 1;
	}
      else if (strcmp (argvec[optind], "-D") == 0)
	{
	  difflag = 1;
	}
      else if (strcmp (argvec[optind], "-R") == 0)
	{
	  revflag = 1;
	}
      else if (strcmp (argvec[optind], "-nm") == 0)
	{
	  nanometers = 1;
	}
      else if (strcmp (argvec[optind], "-SAC") == 0)
	{
	  dataformat = 1;
	}
      else if (strcmp (argvec[optind], "-o") == 0)
	{
	  outputfile = getoptval(argcount, argvec, optind++);
	}
      else if (strcmp (argvec[optind], "-T") == 0)
	{
	  timestr = getoptval(argcount, argvec, optind++);
	}
      else if (strcmp (argvec[optind], "-C") == 0)
	{
	  coorstr = getoptval(argcount, argvec, optind++);
	}
      else
	{
	  fprintf(stderr, "Unknown option: %s\n", argvec[optind]);
	  exit (1);
	}
    }
  
  /* Make sure a time range was specified */
  if ( ! timestr )
    {
      fprintf (stderr, "No time range specified with the -T option\n\n");
      fprintf (stderr, "%s version %s\n\n", PACKAGE, VERSION);
      fprintf (stderr, "Try %s -h for usage\n", PACKAGE);
      exit (1);
    }
  
  if ( ! outputfile )
    fprintf (stderr, "Warning: no output file specified\n\n");
  
  if ( ! coorstr )
    fprintf (stderr, "Warning: no coordinates specified, defaulting to 0.0/0.0\n");
  
  /* Report the program version */
  if ( verbose )
    fprintf (stderr, "%s version: %s\n", PACKAGE, VERSION);
  
  /* Parse time range */
  if ( timestr )
    {
      char *start,*end,*inter;
      int count;

      start = timestr;
      end = inter = 0;
      if ( (end = strchr (start, '/')) )
	{
	  *end++ = '\0';
	}
      else
	{
	  fprintf (stderr, "Error parsing time range: '%s'\n", timestr);
	  fprintf (stderr, "Try %s -h for usage\n", PACKAGE);
	  return -1;
	}
      if ( (inter = strchr (end, '/')) )
	*inter++ = '\0';
      
      if ( start )
	{
	  count = sscanf (start, "%d,%d,%d", &syear, &sday, &shour);
	  if ( count == 1 )
	    {
	      sday = 1; shour = 0;
	    }
	  else if ( count == 2 )
	    {
	      shour = 0;
	    }
	  else if ( count != 3 )
	    {
	      fprintf (stderr, "Error parsing start time: '%s'\n", start);
	      return -1;
	    }
	}
      if ( end )
	{
	  count = sscanf (end, "%d,%d,%d", &eyear, &eday, &ehour);
	  if ( count == 1 )
	    {
	      eday = 1; ehour = 0;
	    }
	  else if ( count == 2 )
	    {
	      ehour = 0;
	    }
	  else if ( count != 3 )
	    {
	      fprintf (stderr, "Error parsing end time: '%s'\n", end);
	      return -1;
	    }
	}
      if ( inter )
	if ( (interval = strtod (inter, NULL)) == 0.0 )
	  {
	    fprintf (stderr, "Error parsing interval: '%s'\n", inter);
	  }
    }

  /* Parse coordinates */
  if ( coorstr )
    {
      char *lat,*lon;

      lat = coorstr;
      lon = 0;
      
      if ( (lon = strchr (lat, '/')) )
	{
	  *lon++ = '\0';
	}
      else
	{
	  fprintf (stderr, "Error parsing coordinates: '%s'\n", coorstr);
	  fprintf (stderr, "Try %s -h for usage\n", PACKAGE);
	  return -1;
	}
      
      latitude = strtod (lat, NULL);
      longitude = strtod (lon, NULL);
    }
  
  return 0;
}  /* End of parameter_proc() */


/***************************************************************************
 * getoptval:
 * Return the value to a command line option; checking that the value is 
 * itself not an option (starting with '-') and is not past the end of
 * the argument list.
 *
 * argcount: total arguments in argvec
 * argvec: argument list
 * argopt: index of option to process, value is expected to be at argopt+1
 *
 * Returns value on success and exits with error message on failure
 ***************************************************************************/
static char *
getoptval (int argcount, char **argvec, int argopt)
{
  if ( argvec == NULL || argvec[argopt] == NULL ) {
    fprintf (stderr, "getoptval(): NULL option requested\n");
    exit (1);
    return 0;
  }
  
  /* Special case of '-o -' usage */
  if ( (argopt+1) < argcount && strcmp (argvec[argopt], "-o") == 0 )
    if ( strcmp (argvec[argopt+1], "-") == 0 )
      return argvec[argopt+1];
  
  if ( (argopt+1) < argcount && *argvec[argopt+1] != '-' )
    return argvec[argopt+1];
  
  fprintf (stderr, "Option %s requires a value\n", argvec[argopt]);
  exit (1);
  return 0;
}  /* End of getoptval() */


/***************************************************************************
 * usage:
 * Print the usage message and exit.
 ***************************************************************************/
static void
usage (void)
{
  fprintf (stderr, "%s version: %s\n\n", PACKAGE, VERSION);
  fprintf (stderr, "Convert Mini-SEED data to SAC\n\n");
  fprintf (stderr, "Usage: %s [options] -o ouputfile\n\n", PACKAGE);
  fprintf (stderr,
	   " ## Options ##\n"
	   " -V             Report program version\n"
	   " -h             Show this usage message\n"
	   " -v             Be more verbose, multiple flags can be used\n"
	   " -n network     Specify the network code, default is XX\n"
	   " -s station     Specify the station code, default is TEST\n"
	   " -l location    Specify the location code, default is blank\n"
	   " -c channel     Specify the channel code, default is TID\n"
	   " -r bytes       Specify SEED record length in bytes, default: 4096\n"
	   " -e encoding    Specify SEED encoding format for packing, default: 11 (Steim2)\n"
	   " -S             Include SEED blockette 100 for very irrational sample rates\n"
	   " -b byteorder   Specify byte order for packing, MSBF: 1 (default), LSBF: 0\n"
	   " -g factor      Specify scaling factor for sample values, default is none\n"
	   " -u             Calculate gravity in microgals instead of displacement\n"
	   " -D             Differentiate to get velocity instead of displacement\n"
	   " -R             Reverse signal polarity (multiply time-series by -1.0)\n"
	   " -nm            Output in nanometers instead of meters\n"
	   " -SAC           Write binary SAC, default is Mini-SEED\n"
	   " -o outfile     Specify the output file, required\n"
	   "\n"
	   " -T start/end[/interval]\n"
	   "                Specify start time, end time in YYYY,DDD,HH format and\n"
	   "                 optionally the interval in hours, default inteval is 1 hour\n"
	   " -C lat/lon     Specify coordinates as Latitude/Longitude in degrees\n" 
	   "\n"
	   "Supported Mini-SEED encoding formats:\n"
           " 3  : 32-bit integers\n"
           " 4  : 32-bit floats (C float)\n"
	   " 5  : 64-bit floats (C double)\n"
           " 10 : Steim 1 compression of 32-bit integers\n"
           " 11 : Steim 2 compression of 32-bit integers\n"
	   "\n");
}  /* End of usage() */
