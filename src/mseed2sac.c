/***************************************************************************
 * mseed2sac.c
 *
 * Convert Mini-SEED waveform data to SAC
 *
 * Written by Chad Trabant, IRIS Data Management Center
 *
 * modified 2006.127
 ***************************************************************************/

// Add -C option for station coordinate list file

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <libmseed.h>

#include "sacformat.h"

#define VERSION "0.2"
#define PACKAGE "mseed2sac"

#define DUNDEF -999.0

struct listnode {
  char *key;
  char *data;
  struct listnode *next;
};

static int writesac (MSTrace *mst);
static int writebinarysac (struct SACHeader *sh, float *fdata, int npts,
			   char *outfile);
static int writealphasac (struct SACHeader *sh, float *fdata, int npts,
			  char *outfile);
static int swapsacheader (struct SACHeader *sh);
static int parameter_proc (int argcount, char **argvec);
static char *getoptval (int argcount, char **argvec, int argopt);
static int readlistfile (char *listfile);
static void addnode (struct listnode **listroot, char *key, char *data);
static void usage (void);

static int   verbose      = 0;
static int   reclen       = -1;
static int   encoding     = 11;
static int   indifile     = 0;
static int   sacformat    = 2;
static double latitude    = DUNDEF;
static double longitude   = DUNDEF;
static char *network      = 0;
static char *station      = 0;
static char *location     = 0;
static char *channel      = 0;

static hptime_t eventtime = 0;
static double eventlat    = DUNDEF;
static double eventlon    = DUNDEF;
static double eventdepth  = DUNDEF;

/* A list of input files */
struct listnode *filelist = 0;

/* A list of station and coordinates */
struct listnode *stationcoords;

int
main (int argc, char **argv)
{
  MSTraceGroup *mstg = 0;
  MSTrace *mst;
  MSRecord *msr;

  struct listnode *flp;

  int totalrecs = 0;
  int totalsamps = 0;
  int totalfiles = 0;
  
  /* Process given parameters (command line and parameter file) */
  if (parameter_proc (argc, argv) < 0)
    return -1;

  /* Init MSTraceGroup */
  mstg = mst_initgroup (mstg);
  
  /* Read input miniSEED files into MSTraceGroup */
  flp = filelist;
  while ( flp != 0 )
    {
      if ( verbose )
        fprintf (stderr, "Reading %s\n", flp->data);
      
      while ( (msr = ms_readmsr(flp->data, reclen, NULL, NULL, 1, 1, verbose-1)) )
	{
	  if ( verbose > 1)
	    msr_print (msr, verbose - 2);
	  
	  mst_addmsrtogroup (mstg, msr, 1, -1.0, -1.0);
	  
	  totalrecs++;
	  totalsamps += msr->samplecnt;
	}
      
      /* Make sure everything is cleaned up */
      ms_readmsr (NULL, 0, NULL, NULL, 0, 0, 0);
      
      /* If processing each file individually, write SAC and reset */
      if ( indifile )
	{
	  mst = mstg->traces;
	  while ( mst )
	    {
	      writesac (mst);
	      mst = mst->next;
	    }
	  
	  mstg = mst_initgroup (mstg);
	}
      
      totalfiles++;
      flp = flp->next;
    }
  
  if ( ! indifile )
    {
      mst = mstg->traces;
      while ( mst )
	{
	  writesac (mst);
	  mst = mst->next;
	}
    }
  
  /* Make sure everything is cleaned up */
  mst_freegroup (&mstg);
  
  if ( verbose )
    printf ("Files: %d, Records: %d, Samples: %d\n", totalfiles, totalrecs, totalsamps);
  
  return 0;
}  /* End of main() */


/***************************************************************************
 * writesac:
 * 
 * Write data buffer to output file as binary SAC.
 *
 * Returns the number of samples written or -1 on error.
 ***************************************************************************/
static int
writesac (MSTrace *mst)
{
  struct SACHeader sh = NullSACHeader;
  BTime btime;
  
  char outfile[1024];
  char *sacnetwork;
  char *sacstation;
  char *saclocation;
  char *sacchannel;
  
  float *fdata = 0;
  double *ddata = 0;
  int32_t *idata = 0;
  int idx;
  
  if ( ! mst )
    return -1;
  
  if ( mst->numsamples == 0 || mst->samprate == 0.0 )
    return 0;
  
  sacnetwork = ( network ) ? network : mst->network;
  sacstation = ( station ) ? station : mst->station;
  saclocation = ( location ) ? location : mst->location;
  sacchannel = ( channel ) ? channel : mst->channel;
  
  /* Set time-series source parameters */
  if ( sacnetwork )
    if ( *sacnetwork != '\0' )
      strncpy (sh.knetwk, sacnetwork, 8);
  if ( sacstation )
    if ( *sacstation != '\0' )
      strncpy (sh.kstnm, sacstation, 8);
  if ( saclocation )
    if ( *saclocation != '\0' )
      strncpy (sh.khole, saclocation, 8);
  if ( sacchannel )
    if ( *sacchannel != '\0' )
      strncpy (sh.kcmpnm, sacchannel, 8);
  
  /* Set misc. header variables */
  sh.nvhdr = 6;                 /* Header version = 6 */
  sh.leven = 1;                 /* Evenly spaced data */
  sh.iftype = ITIME;            /* Data is time-series */
  sh.b = 0.0;                   /* First sample, offset time */
  sh.e = (mst->numsamples - 1) * (1 / mst->samprate); /* Last sample, offset time */
  //strncpy (sh.kevnm, "Event name", 16);
  
  /* Set station coordinates */
  if ( latitude != DUNDEF ) sh.stla = latitude;
  if ( longitude != DUNDEF ) sh.stlo = longitude;
  
  /* Set event parameters */
  if ( eventtime )
    sh.o = (float) ( MS_HPTIME2EPOCH(eventtime) - 
		     MS_HPTIME2EPOCH(mst->starttime) );
  if ( eventlat != DUNDEF )
    sh.evla = (float) eventlat;
  if ( eventlon != DUNDEF )
    sh.evlo = (float) eventlon;
  if ( eventdepth != DUNDEF )
    sh.evdp = (float) eventdepth;
  
  /* Set start time */
  ms_hptime2btime (mst->starttime, &btime);
  sh.nzyear = btime.year;
  sh.nzjday = btime.day;
  sh.nzhour = btime.hour;
  sh.nzmin = btime.min;
  sh.nzsec = btime.sec;
  sh.nzmsec = btime.fract / 10;
  
  /* Set sampling interval (seconds), sample count */
  sh.delta = 1 / mst->samprate;
  sh.npts = mst->numsamples;
  
  /* Convert data buffer to floats */
  if ( mst->sampletype == 'f' )
    {
      fdata = (float *) mst->datasamples;
    }
  else if ( mst->sampletype == 'i' )
    {
      idata = (int32_t *) mst->datasamples;

      fdata = (float *) malloc (mst->numsamples * sizeof(float));

      if ( fdata == NULL )
	{
	  fprintf (stderr, "Error allocating memory\n");
	  return -1;
	}
      
      for (idx=0; idx < mst->numsamples; idx++)
	fdata[idx] = (float) idata[idx];
    }
  else if ( mst->sampletype == 'd' )
    {
      ddata = (double *) mst->datasamples;
      
      fdata = (float *) malloc (mst->numsamples * sizeof(float));
      
      if ( fdata == NULL )
	{
	  fprintf (stderr, "Error allocating memory\n");
	  return -1;
	}
      
      for (idx=0; idx < mst->numsamples; idx++)
	fdata[idx] = (float) ddata[idx];
    }
  else
    {
      fprintf (stderr, "Error, unrecognized sample type: '%c'\n",
	       mst->sampletype);
      return -1;
    }
  
  if ( sacformat >= 2 && sacformat <= 4 )
    {
      /* Create output file name: Net.Sta.Loc.Chan.Qual.Year.Day.Hour.Min.Sec.SAC */
      snprintf (outfile, sizeof(outfile), "%s.%s.%s.%s.%c.%d,%d,%d:%d:%d.SAC",
		sacnetwork, sacstation, saclocation, sacchannel,
		mst->dataquality, btime.year, btime.day, btime.hour,
		btime.min, btime.sec);
      
      /* Byte swap the data header and data if needed */
      if ( (sacformat == 3 && ms_bigendianhost()) ||
	   (sacformat == 4 && ! ms_bigendianhost()) )
	{
	  if ( verbose )
	    fprintf (stderr, "Byte swapping SAC header and data\n");

	  swapsacheader (&sh);
	  
	  for (idx=0; idx < mst->numsamples; idx++)
	    {
	      gswap4 (fdata + idx);
	    }
	}
	   
      if ( writebinarysac (&sh, fdata, mst->numsamples, outfile) )
	return -1;
    }
  else if ( sacformat == 1 )
    {
      /* Create output file name: Net.Sta.Loc.Chan.Qual.Year.Day.Hour.Min.Sec.SACA */
      snprintf (outfile, sizeof(outfile), "%s.%s.%s.%s.%c.%d,%d,%d:%d:%d.SACA",
		sacnetwork, sacstation, saclocation, sacchannel,
		mst->dataquality, btime.year, btime.day, btime.hour,
		btime.min, btime.sec);
      
      if ( writealphasac (&sh, fdata, mst->numsamples, outfile) )
	return -1;
    }
  else
    {
      fprintf (stderr, "Error, unrecognized format: '%d'\n", sacformat);
    }
  
  if ( fdata && mst->sampletype != 'f' )
    free (fdata);
  
  fprintf (stderr, "Wrote %d samples to %s\n", mst->numsamples, outfile);
  
  return mst->numsamples;
}  /* End of writesac() */


/***************************************************************************
 * writebinarysac:
 * Write binary SAC file.
 *
 * Returns 0 on success, and -1 on failure.
 ***************************************************************************/
static int
writebinarysac (struct SACHeader *sh, float *fdata, int npts, char *outfile)
{
  FILE *ofp;
  
  /* Open output file */
  if ( (ofp = fopen (outfile, "wb")) == NULL )
    {
      fprintf (stderr, "Cannot open output file: %s (%s)\n",
	       outfile, strerror(errno));
      return -1;
    }
  
  /* Write SAC header to output file */
  if ( fwrite (sh, sizeof(struct SACHeader), 1, ofp) != 1 )
    {
      fprintf (stderr, "Error writing SAC header to output file\n");
      return -1;
    }
  
  /* Write float data to output file */
  if ( fwrite (fdata, sizeof(float), npts, ofp) != npts )
    {
      fprintf (stderr, "Error writing SAC data to output file\n");
      return -1;
    }
  
  fclose (ofp);
  
  return 0;
}  /* End of writebinarysac() */


/***************************************************************************
 * writealphasac:
 * Write alphanumeric SAC file.
 *
 * Returns 0 on success, and -1 on failure.
 ***************************************************************************/
static int
writealphasac (struct SACHeader *sh, float *fdata, int npts, char *outfile)
{
  FILE *ofp;
  int idx, fidx;
  
  /* Declare and set up pointers to header variable type sections */
  float   *fhp = (float *) sh;
  int32_t *ihp = (int32_t *) sh + (NUMFLOATHDR);
  char    *shp = (char *) sh + (NUMFLOATHDR * 4 + NUMINTHDR * 4);
  
  /* Open output file */
  if ( (ofp = fopen (outfile, "wb")) == NULL )
    {
      fprintf (stderr, "Cannot open output file: %s (%s)\n",
	       outfile, strerror(errno));
      return -1;
    }
  
  /* Write SAC header float variables to output file, 5 variables per line */
  for (idx=0; idx < NUMFLOATHDR; idx += 5)
    {
      for (fidx=idx; fidx < (idx+5) && fidx < NUMFLOATHDR; fidx++)
	fprintf (ofp, "%#15.7g", *(fhp + fidx));
      
      fprintf (ofp, "\n");
    }
  
  /* Write SAC header integer variables to output file, 5 variables per line */
  for (idx=0; idx < NUMINTHDR; idx += 5)
    {
      for (fidx=idx; fidx < (idx+5) && fidx < NUMINTHDR; fidx++)
	fprintf (ofp, "%10d", *(ihp + fidx));
      
      fprintf (ofp, "\n");
    }
  
  /* Write SAC header string variables to output file, 3 variables per line */
  for (idx=0; idx < NUMSTRHDR; idx += 3)
    {
      if ( idx == 0 )
	fprintf (ofp, "%-8.8s%-16.16s", shp, shp + 8);
      else
	fprintf (ofp, "%-8.8s%-8.8s%-8.8s", shp+(idx*8), shp+((idx+1)*8), shp+((idx+2)*8));
      
      fprintf (ofp, "\n");
    }
  
  /* Write float data to output file, 5 values per line */
  for (idx=0; idx < npts; idx += 5)
    {
      for (fidx=idx; fidx < (idx+5) && fidx < npts && fidx >= 0; fidx++)
	fprintf (ofp, "%#15.7g", *(fdata + fidx));
      
      fprintf (ofp, "\n");
    }
  
  fclose (ofp);
  
  return 0;
}  /* End of writealphasac() */


/***************************************************************************
 * swapsacheader:
 *
 * Byte swap all multi-byte quantities (floats and ints) in SAC header
 * struct.
 *
 * Returns 0 on sucess and -1 on failure.
 ***************************************************************************/
static int
swapsacheader (struct SACHeader *sh)
{
  int32_t *ip;
  int idx;
  
  if ( ! sh )
    return -1;
  
  for ( idx=0; idx < (NUMFLOATHDR + NUMINTHDR); idx++ )
    {
      ip = (int32_t *) sh + idx;
      gswap4 (ip);
    }
  
  return 0;
}  /* End of swapsacheader() */


/***************************************************************************
 * parameter_proc:
 * Process the command line parameters.
 *
 * Returns 0 on success, and -1 on failure.
 ***************************************************************************/
static int
parameter_proc (int argcount, char **argvec)
{
  char *coorstr = 0;
  char *eventstr = 0;
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
	  reclen = strtoul (getoptval(argcount, argvec, optind++), NULL, 10);
	}
      else if (strcmp (argvec[optind], "-e") == 0)
	{
	  encoding = strtoul (getoptval(argcount, argvec, optind++), NULL, 10);
	}
      else if (strcmp (argvec[optind], "-i") == 0)
	{
	  indifile = 1;
	}
      else if (strcmp (argvec[optind], "-k") == 0)
	{
	  coorstr = getoptval(argcount, argvec, optind++);
	}
      else if (strcmp (argvec[optind], "-E") == 0)
	{
	  eventstr = getoptval(argcount, argvec, optind++);
	}
      else if (strcmp (argvec[optind], "-f") == 0)
	{
	  sacformat = strtoul (getoptval(argcount, argvec, optind++), NULL, 10);
	}
      else if (strncmp (argvec[optind], "-", 1) == 0 &&
               strlen (argvec[optind]) > 1 )
        {
          fprintf(stderr, "Unknown option: %s\n", argvec[optind]);
          exit (1);
        }
      else
        {
          addnode (&filelist, NULL, argvec[optind]);
        }
    }

  /* Make sure an input files were specified */
  if ( filelist == 0 )
    {
      fprintf (stderr, "No input files were specified\n\n");
      fprintf (stderr, "%s version %s\n\n", PACKAGE, VERSION);
      fprintf (stderr, "Try %s -h for usage\n", PACKAGE);
      exit (1);
    }

  /* Report the program version */
  if ( verbose )
    fprintf (stderr, "%s version: %s\n", PACKAGE, VERSION);
  
  /* Check the input files for any list files, if any are found
   * remove them from the list and add the contained list */
  if ( filelist )
    {
      struct listnode *prevln, *ln;
      char *lfname;

      prevln = ln = filelist;
      while ( ln != 0 )
        {
          lfname = ln->data;

          if ( *lfname == '@' )
            {
              /* Remove this node from the list */
              if ( ln == filelist )
                filelist = ln->next;
              else
                prevln->next = ln->next;

              /* Skip the '@' first character */
              if ( *lfname == '@' )
                lfname++;

              /* Read list file */
              readlistfile (lfname);

              /* Free memory for this node */
              if ( ln->key )
                free (ln->key);
              free (ln->data);
              free (ln);
            }
          else
            {
              prevln = ln;
            }
	  
          ln = ln->next;
        }
    }

  /* Parse coordinates */
  if ( coorstr )
    {
      char *lat,*lon;
      char *endptr = 0;
      
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
      
      if ( lat )
	if ( *lat )
	  if ( (latitude = strtod (lat, &endptr)) == 0.0 && endptr == lat )
	    {
	      fprintf (stderr, "Error parsing station latitude: '%s'\n", lat);
	      return -1;
	    }
      if ( lon )
	if ( *lon )
	  if ( (longitude = strtod (lon, &endptr)) == 0.0 && endptr == lon )
	    {
	      fprintf (stderr, "Error parsing station longitude: '%s'\n", lon);
	      return -1;
	    }
    }
  
  /* Parse event information */
  if ( eventstr )
    {
      char *etime,*elat,*elon,*edepth;
      char *endptr = 0;
      
      etime = eventstr;
      elat = elon = edepth = 0;
      
      if ( (elat = strchr (etime, '/')) )
	{
	  *elat++ = '\0';
	  
	  if ( (elon = strchr (elat, '/')) )
	    {
	      *elon++ = '\0';
	      
	      if ( (edepth = strchr (elon, '/')) )
		{
		  *edepth++ = '\0';
		}
	    }
	}
      
      eventtime = ms_seedtimestr2hptime (etime);
      
      fprintf (stderr, "DB: event time '%lld'\n", (long long) eventtime);

      if ( eventtime == HPTERROR )
	{
	  fprintf (stderr, "Error parsing event time: '%s'\n", etime);
	  fprintf (stderr, "Try %s -h for usage\n", PACKAGE);
	  return -1;
	}
      
      if ( elat )
	if ( *elat )
	  if ( (eventlat = strtod (elat, &endptr)) == 0.0 && endptr == elat )
	    {
	      fprintf (stderr, "Error parsing event latitude: '%s'\n", elat);
	      return -1;
	    }
      if ( elon )
	if ( *elon )
	  if ( (eventlon = strtod (elon, &endptr)) == 0.0 && endptr == elon )
	    {
	      fprintf (stderr, "Error parsing event longitude: '%s'\n", elon);
	      return -1;
	    }
      if ( edepth )
	if ( *edepth )
	  if ( (eventdepth = strtod (edepth, &endptr))== 0.0 && endptr == edepth )
	    {
	      fprintf (stderr, "Error parsing event depth: '%s'\n", edepth);
	      return -1;
	    }
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
 * readlistfile:
 *
 * Read a list of files from a file and add them to the filelist for
 * input data.  The filename is expected to be the last
 * space-separated field on the line.
 *
 * Returns the number of file names parsed from the list or -1 on error.
 ***************************************************************************/
static int
readlistfile (char *listfile)
{
  FILE *fp;
  char  line[1024];
  char *ptr;
  int   filecnt = 0;

  char  filename[1024];
  char *lastfield = 0;
  int   fields = 0;
  int   wspace;

  /* Open the list file */
  if ( (fp = fopen (listfile, "rb")) == NULL )
    {
      if (errno == ENOENT)
        {
          fprintf (stderr, "Could not find list file %s\n", listfile);
          return -1;
        }
      else
        {
          fprintf (stderr, "Error opening list file %s: %s\n",
                   listfile, strerror (errno));
          return -1;
        }
    }
  if ( verbose )
    fprintf (stderr, "Reading list of input files from %s\n", listfile);

  while ( (fgets (line, sizeof(line), fp)) !=  NULL)
    {
      /* Truncate line at first \r or \n, count space-separated fields
       * and track last field */
      fields = 0;
      wspace = 0;
      ptr = line;
      while ( *ptr )
        {
          if ( *ptr == '\r' || *ptr == '\n' || *ptr == '\0' )
            {
              *ptr = '\0';
              break;
            }
          else if ( *ptr != ' ' )
            {
              if ( wspace || ptr == line )
                {
                  fields++; lastfield = ptr;
                }
              wspace = 0;
            }
          else
            {
              wspace = 1;
            }

          ptr++;
        }

      /* Skip empty lines */
      if ( ! lastfield )
        continue;

      if ( fields >= 1 && fields <= 3 )
        {
          fields = sscanf (lastfield, "%s", filename);

          if ( fields != 1 )
            {
              fprintf (stderr, "Error parsing file name from: %s\n", line);
              continue;
            }

          if ( verbose > 1 )
            fprintf (stderr, "Adding '%s' to input file list\n", filename);

          addnode (&filelist, NULL, filename);
          filecnt++;

          continue;
        }
    }

  fclose (fp);

  return filecnt;
}  /* End readlistfile() */


/***************************************************************************
 * addnode:
 *
 * Add node to the specified list.
 ***************************************************************************/
static void
addnode (struct listnode **listroot, char *key, char *data)
{
  struct listnode *lastlp, *newlp;

  if ( data == NULL )
    {
      fprintf (stderr, "addnode(): No file name specified\n");
      return;
    }

  lastlp = *listroot;
  while ( lastlp != 0 )
    {
      if ( lastlp->next == 0 )
        break;

      lastlp = lastlp->next;
    }

  newlp = (struct listnode *) malloc (sizeof (struct listnode));
  memset (newlp, 0, sizeof (struct listnode));
  if ( key ) newlp->key = strdup(key);
  else newlp->key = key;
  if ( data) newlp->data = strdup(data);
  else newlp->data = data;
  newlp->next = 0;

  if ( lastlp == 0 )
    *listroot = newlp;
  else
    lastlp->next = newlp;

}  /* End of addnode() */


/***************************************************************************
 * usage:
 * Print the usage message and exit.
 ***************************************************************************/
static void
usage (void)
{
  fprintf (stderr, "%s version: %s\n\n", PACKAGE, VERSION);
  fprintf (stderr, "Convert Mini-SEED data to SAC\n\n");
  fprintf (stderr, "Usage: %s [options] input1.mseed [input2.mseed ...]\n\n", PACKAGE);
  fprintf (stderr,
	   " ## Options ##\n"
	   " -V             Report program version\n"
	   " -h             Show this usage message\n"
	   " -v             Be more verbose, multiple flags can be used\n"
	   " -n network     Specify the network code, overrides any value in the SEED\n"
	   " -s station     Specify the station code, overrides any value in the SEED\n"
	   " -l location    Specify the location code, overrides any value in the SEED\n"
	   " -c channel     Specify the channel code, overrides any value in the SEED\n"
	   " -r bytes       Specify SEED record length in bytes, default: 4096\n"
	   " -e encoding    Specify SEED encoding format for packing, default: 11 (Steim2)\n"
	   " -i             Process each input file individually instead of merged\n"
	   " -k lat/lon     Specify coordinates as 'Latitude/Longitude' in degrees\n"
	   " -C coordfile   File containing station coordinates\n"
	   " -E hypo        Specify event hypocenter as 'Time[/Lat][/Lon][/Depth]'\n"
	   "                  e.g. '2006,123,15:26:35.19/-20.035/-174.227/5000'\n"
	   " -f format      Specify SAC file format (default is 2:binary):\n"
           "                  1=alpha, 2=binary (host byte order),\n"
           "                  3=binary (little-endian), 4=binary (big-endian)\n"
	   "\n");
}  /* End of usage() */