/***************************************************************************
 * mseed2sac.c
 *
 * Convert Mini-SEED waveform data to SAC
 *
 * Written by Chad Trabant, IRIS Data Management Center
 *
 * modified 2013.271
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <math.h>

#include <libmseed.h>

#include "sacformat.h"

#ifndef NOFDZIP
  #include "fdzipstream.h"
#endif

#if defined(WIN32) || defined(WIN64)
  #include <io.h>
  #define access _access
#endif

#define VERSION "2.0"
#define PACKAGE "mseed2sac"

/* An undefined value for double values */
#define DUNDEF -999.0

/* Maximum number of metadata fields per line */
#define MAXMETAFIELDS 17

/* Macro to test floating point number equality within 10 decimal places */
#define FLTEQUAL(F1,F2) (fabs(F1-F2) < 1.0E-10 * (fabs(F1) + fabs(F2) + 1.0))

struct listnode {
  char *key;
  char *data;
  struct listnode *next;
};

struct metanode {
  char *metafields[MAXMETAFIELDS];
  hptime_t starttime;
  hptime_t endtime;
};

static int writesac (MSTrace *mst);
static int writebinarysac (struct SACHeader *sh, float *fdata, int npts,
			   char *outfile);
static int writealphasac (struct SACHeader *sh, float *fdata, int npts,
			  char *outfile);
static int swapsacheader (struct SACHeader *sh);
static int insertmetadata (struct SACHeader *sh, hptime_t sacstarttime);
static int delaz (double lat1, double lon1, double lat2, double lon2,
		  double *delta, double *dist, double *azimuth, double *backazimuth);
static int parameter_proc (int argcount, char **argvec);
static char *getoptval (int argcount, char **argvec, int argopt, int dasharg);
static int readlistfile (char *listfile);
static int readmetadata (char *metafile);
static struct listnode *addnode (struct listnode **listroot, void *key, int keylen,
				 void *data, int datalen);
static void usage (int level);

static int   verbose      = 0;
static int   reclen       = -1;
static int   overwrite    = 0;
static int   deriverate   = 0;
static int   indifile     = 0;
static int   indichannel  = 0;
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
static char  *eventname   = 0;

static char *zipfile      = 0;
#ifndef NOFDZIP
static int zipmethod      = -1;
static ZIPstream *zstream = 0;
#endif

struct listnode *filelist = 0;      /* List of input files */
static Selections *selections = 0;  /* List of data selections */
struct listnode *metadata = 0;      /* List of stations and coordinates, etc. */
static int seedinc = 0;             /* SEED component inclination flag */

int
main (int argc, char **argv)
{
  MSTraceGroup *mstg = 0;
  MSTrace *mst = 0;
  MSRecord *msr = 0;
  
  struct listnode *flp;
  
  char srcname[50];
  char prevsrcname[50];
  char starttime[50];
  hptime_t recendtime;
  
  int retcode;
  int64_t totalrecs = 0;
  int64_t totalsamps = 0;
  int totalfiles = 0;
  
#ifndef NOFDZIP
  FILE *zipfp;
  int zipfd;
  ssize_t writestatus = 0;
#endif  /* NOFDZIP */
  
  /* Process given parameters (command line and parameter file) */
  if (parameter_proc (argc, argv) < 0)
    return -1;
  
  /* Init MSTraceGroup */
  mstg = mst_initgroup (mstg);
  
#ifndef NOFDZIP
  /* Open & intialize output ZIP archive if needed */
  if ( zipfile )
    {
      if ( ! strcmp (zipfile, "-") ) /* Write ZIP to stdout */
	{
	  if ( verbose )
	    fprintf (stderr, "Writing ZIP archive to stdout\n");
	  
          zipfd = fileno(stdout);
	}
      else if ( (zipfp = fopen (zipfile, "wb")) == NULL ) /* Open output ZIP file */
	{
	  fprintf (stderr, "Cannot open output file: %s (%s)\n",
		   zipfile, strerror(errno));
	  return -1;
	}
      else
	{
	  if ( verbose )
	    fprintf (stderr, "Writing ZIP archive to %s\n", zipfile);
	  
	  zipfd = fileno(zipfp);
	}
      
      /* Initialize ZIP container */
      if ( (zstream = zs_init (zipfd, zstream)) == NULL )
	{
	  fprintf (stderr, "Error in zs_init()\n");
	  return 1;
	}
    }
#endif  /* NOFDZIP */
  
  /* Read input miniSEED files into MSTraceGroup */
  flp = filelist;
  while ( flp != 0 )
    {
      if ( verbose )
        fprintf (stderr, "Reading %s\n", flp->data);
      
      while ( (retcode = ms_readmsr(&msr, flp->data, reclen, NULL, NULL,
                                    1, 1, verbose-1)) == MS_NOERROR )
	{
	  /* Generate source name if needed for tests */
          if ( selections || indichannel )
            {
	      msr_srcname (msr, srcname, 1);
	    }
	  
	  /* Check if record is matched by selection */
          if ( selections )
            {
	      recendtime = msr_endtime (msr);
	      
              if ( ! ms_matchselect (selections, srcname, msr->starttime, recendtime, NULL) )
                {
                  if ( verbose >= 2 )
                    {
                      ms_hptime2seedtimestr (msr->starttime, starttime, 1);
                      ms_log (1, "Skipping (selection) %s, %s\n", srcname, starttime);
                    }
		  
		  continue;
                }
            }
	  
	  /* If this is a new channel write previous data (if individual channel writing) */
	  if ( indichannel )
	    {
	      if ( totalrecs > 0 )
		{
		  if ( strncmp (prevsrcname, srcname, sizeof(prevsrcname)) )
		    {
		      mst = mstg->traces;
		      while ( mst )
			{
			  writesac (mst);
			  mst = mst->next;
			}
		      
		      mstg = mst_initgroup (mstg);
		      
		      strncpy (prevsrcname, srcname, sizeof (prevsrcname));
		    }
		}
	      else
		{ 
		  strncpy (prevsrcname, srcname, sizeof (prevsrcname));
		}
	    }
	  
	  if ( verbose >= 2 )
	    msr_print (msr, verbose - 2);
	  
	  mst_addmsrtogroup (mstg, msr, 1, -1.0, -1.0);
	  
	  totalrecs++;
	  totalsamps += msr->samplecnt;
	}
      
      if ( retcode != MS_ENDOFFILE )
	fprintf (stderr, "Error reading %s: %s\n", flp->data, ms_errorstr(retcode));
      
      /* Make sure everything is cleaned up */
      ms_readmsr (&msr, NULL, 0, NULL, NULL, 0, 0, 0);
      
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
  
#ifndef NOFDZIP
  /* Finish output ZIP archive if needed */
  if ( zipfile )
    {
      if ( zs_finish (zstream, &writestatus) )
	{
	  fprintf (stderr, "Error finishing ZIP archive, write status: %lld\n",
		   (long long int) writestatus);
	}
      
      zs_free (zstream);
    }
#endif  /* NOFDZIP */
  
  /* Make sure everything is cleaned up */
  mst_freegroup (&mstg);
  
  if ( verbose )
    fprintf (stderr, "Files: %d, Records: %lld, Samples: %lld\n",
    totalfiles, (long long int) totalrecs, (long long int) totalsamps);
  
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
  char baseoutfile[1024];
  char *sacnetwork;
  char *sacstation;
  char *saclocation;
  char *sacchannel;
  
  float *fdata = 0;
  double *ddata = 0;
  int32_t *idata = 0;
  hptime_t submsec;
  int idx;
  int rv;
  
  if ( ! mst )
    return -1;
  
  if ( mst->numsamples == 0 || mst->samprate == 0.0 )
    return 0;
  
  /* Check reported versus derived sampling rates */
  if ( mst->starttime < mst->endtime )
    {
      hptime_t hptimeshift;
      hptime_t hpdelta;
      double samprate;
      
      /* Calculate difference between end time of last miniSEED record and the end time
       * as calculated based on the start time, reported sample rate and number of samples. */
      hptimeshift = llabs (mst->endtime - mst->starttime - (hptime_t)((mst->numsamples - 1) * HPTMODULUS / mst->samprate));
      
      /* Calculate high-precision sample period using reported sample rate */
      hpdelta = (hptime_t)(( mst->samprate ) ? (HPTMODULUS / mst->samprate) : 0.0);
      
      /* Test if time shift is beyond half a sample period */
      if ( hptimeshift > (hpdelta * 0.5) )
	{
	  /* Derive sample rate from start and end times and number of samples */
	  samprate = (double) (mst->numsamples - 1) * HPTMODULUS / (mst->endtime - mst->starttime);
	  
	  if ( deriverate )
	    {
	      if ( verbose )
		fprintf (stderr, "Using derived sample rate of %g over reported rate of %g\n",
			 samprate, mst->samprate);
	      
	      mst->samprate = samprate;
	    }
	  else
	    {
	      fprintf (stderr, "[%s.%s.%s.%s] Reported sample rate different than derived rate (%g versus %g)\n",
		       mst->network, mst->station, mst->location, mst->channel,
		       mst->samprate, samprate);
	      fprintf (stderr, "   Consider using the -dr option to use the sample rate derived from the series\n");
	    }
	}
    }
  
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
  
  if ( verbose )
    fprintf (stderr, "Writing SAC for %.8s.%.8s.%.8s.%.8s\n",
	     sacnetwork, sacstation, saclocation, sacchannel);
  
  /* Set misc. header variables */
  sh.nvhdr = 6;                 /* Header version = 6 */
  sh.leven = 1;                 /* Evenly spaced data */
  sh.iftype = ITIME;            /* Data is time-series */

  /* Set sampling interval (seconds), sample count */
  sh.delta = 1 / mst->samprate;
  sh.npts = mst->numsamples;
  
  /* Insert metadata */
  if ( metadata )
    {
      rv = insertmetadata (&sh, mst->starttime);
      
      if ( rv == -1 )
	fprintf (stderr, "Error inserting metadata for %.8s.%.8s.%.8s.%.8s\n",
		 sacnetwork, sacstation, saclocation, sacchannel);
      else if ( rv == 1 )
	fprintf (stderr, "No metadata found for %.8s.%.8s.%.8s.%.8s\n",
		 sacnetwork, sacstation, saclocation, sacchannel);
    }
  
  /* Set station coordinates specified on command line */
  if ( latitude != DUNDEF ) sh.stla = latitude;
  if ( longitude != DUNDEF ) sh.stlo = longitude;
  
  /* Set event parameters */
  if ( eventtime )
    sh.o = (float) MS_HPTIME2EPOCH((eventtime - mst->starttime));
  if ( eventlat != DUNDEF )
    sh.evla = (float) eventlat;
  if ( eventlon != DUNDEF )
    sh.evlo = (float) eventlon;
  if ( eventdepth != DUNDEF )
    sh.evdp = (float) eventdepth;
  if ( eventname )
    strncpy (sh.kevnm, eventname, 16);
  
  /* Calculate delta, distance and azimuths if both event and station coordiantes are known */
  if ( sh.evla != FUNDEF && sh.evlo != FUNDEF &&
       sh.stla != FUNDEF && sh.stlo != FUNDEF )
    {
      double delta, dist, azimuth, backazimuth;
      
      if ( ! delaz (sh.evla, sh.evlo, sh.stla, sh.stlo, &delta, &dist, &azimuth, &backazimuth) )
	{
	  sh.az = (float) azimuth;
	  sh.baz = (float) backazimuth;
	  sh.gcarc = (float) delta;
	  sh.dist = (float) dist;
	  
	  if ( verbose )
	    fprintf (stderr, "Inserting variables: AZ: %g, BAZ: %g, GCARC: %g, DIST: %g\n",
		     sh.az, sh.baz, sh.gcarc, sh.dist);
	}
    }
  
  /* Set reference time */
  ms_hptime2btime (mst->starttime, &btime);
  sh.nzyear = btime.year;
  sh.nzjday = btime.day;
  sh.nzhour = btime.hour;
  sh.nzmin = btime.min;
  sh.nzsec = btime.sec;
  sh.nzmsec = btime.fract / 10;
  
  /* Determine any sub-millisecond portion of the start time in HP time */
  submsec = (mst->starttime - 
	     ms_time2hptime (sh.nzyear, sh.nzjday, sh.nzhour,
			     sh.nzmin, sh.nzsec, sh.nzmsec * 1000));
  
  /* Set begin and end offsets from reference time for first and last sample,
   * any sub-millisecond start time is stored in these offsets. */
  sh.b = ((float)submsec / HPTMODULUS);
  sh.e = (mst->numsamples - 1) * (1 / mst->samprate) + ((float)submsec / HPTMODULUS);
  
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
      fprintf (stderr, "Error, unrecognized sample type: '%c'\n", mst->sampletype);
      return -1;
    }
  
  /* Create base output file name: Net.Sta.Loc.Chan.Qual.Year.Day.HourMinSec */
  snprintf (baseoutfile, sizeof(baseoutfile), "%s.%s.%s.%s.%c.%04d.%03d.%02d%02d%02d",
	    sacnetwork, sacstation, saclocation, sacchannel,
	    mst->dataquality, btime.year, btime.day, btime.hour,
	    btime.min, btime.sec);
  
  /* Find unused file name */
#define MAXDUPBASE 1000
  for ( idx = 0; idx <= MAXDUPBASE; idx++ )
    {
      if ( idx == MAXDUPBASE )
	{
	  fprintf (stderr, "Error, over %d files with a base of %s ????, giving up...\n",
		   MAXDUPBASE, baseoutfile);
	  return -1;
	}
      
      if ( idx == 0 )
	snprintf (outfile, sizeof(outfile), "%s.SAC%s", baseoutfile, (sacformat==1)?"A":"");
      else
	snprintf (outfile, sizeof(outfile), "%s-%d.SAC%s", baseoutfile, idx, (sacformat==1)?"A":"");
      
      if ( zipfile )  /* Trap door for ZIP output, first file name always used */
	break;
      
      if ( access(outfile, F_OK) )
	{
	  if ( errno == ENOENT )
	    break;
	  else
	    {
	      fprintf (stderr, "Error, Cannot write output file %s: %s\n", outfile, strerror(errno));
	      return -1;
	    }
	}
      else if ( overwrite )
	{
	  break;
	}
    }
  
  if ( sacformat >= 2 && sacformat <= 4 )
    {
      /* Byte swap the data header and data if needed */
      if ( (sacformat == 3 && ms_bigendianhost()) ||
	   (sacformat == 4 && ! ms_bigendianhost()) )
	{
	  if ( verbose )
	    fprintf (stderr, "Byte swapping SAC header and data\n");
	  
	  swapsacheader (&sh);
	  
	  for (idx=0; idx < mst->numsamples; idx++)
	    {
	      ms_gswap4 (fdata + idx);
	    }
	}
      
      if ( verbose > 1 )
	fprintf (stderr, "Writing binary SAC file: %s\n", outfile);
      
      if ( writebinarysac (&sh, fdata, mst->numsamples, outfile) )
	return -1;
    }
  else if ( sacformat == 1 )
    {
      if ( verbose > 1 )
	fprintf (stderr, "Writing alphanumeric SAC file: %s\n", outfile);
      
      if ( writealphasac (&sh, fdata, mst->numsamples, outfile) )
	return -1;
    }
  else
    {
      fprintf (stderr, "Error, unrecognized format: '%d'\n", sacformat);
    }
  
  if ( fdata && mst->sampletype != 'f' )
    free (fdata);
  
  fprintf (stderr, "Wrote %lld samples to %s\n", (long long int)mst->numsamples, outfile);
  
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

#ifndef NOFDZIP
  ZIPentry *zentry = 0;
  ssize_t writestatus = 0;
#endif  /* NOFDZIP */
  
  if ( ! zipfile )
    {
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
    }
  else
    {
#ifndef NOFDZIP
      /* Begin ZIP entry */
      if ( ! (zentry = zs_entrybegin (zstream, outfile, time(NULL),
				      zipmethod, &writestatus)) )
	{
	  fprintf (stderr, "Cannot begin ZIP entry, write status: %lld\n",
		   (long long int) writestatus);
	  return -1;
	}
      
      /* Write SAC header to ZIP */
      if ( ! zs_entrydata (zstream, zentry, (unsigned char *) sh,
			   sizeof(struct SACHeader), 0, &writestatus) )
	{
	  fprintf (stderr, "Error adding entry data for %s to output ZIP, write status: %lld\n",
		   outfile, (long long int) writestatus);
	  return -1;
	}
      
      /* Write float data to ZIP */
      if ( ! zs_entrydata (zstream, zentry, (unsigned char *) fdata,
			   npts * sizeof(float), 1, &writestatus) )
	{
	  fprintf (stderr, "Error adding entry data for %s to output ZIP, write status: %lld\n",
		   outfile, (long long int) writestatus);
	  return -1;
	}
      
      /* End ZIP entry */
      if ( ! zs_entryend (zstream, zentry, &writestatus) )
	{
	  fprintf (stderr, "Error ending ZIP entry for %s, write status: %lld\n",
		   outfile, (long long int) writestatus);
	  return 1;
	}
#endif  /* NOFDZIP */
    }
  
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
  char buffer[2000];
  char *bp;
  int idx, fidx;
  
  /* Declare and set up pointers to header variable type sections */
  float   *fhp = (float *) sh;
  int32_t *ihp = (int32_t *) sh + (NUMFLOATHDR);
  char    *shp = (char *) sh + (NUMFLOATHDR * 4 + NUMINTHDR * 4);
  
#ifndef NOFDZIP
  ZIPentry *zentry = 0;
  ssize_t writestatus = 0;
  int flush;
#endif  /* NOFDZIP */
  
  /* Generate header in buffer */
  bp = buffer;
  
  /* Write SAC header float variables to output file, 5 variables per line */
  for (idx=0; idx < NUMFLOATHDR; idx += 5)
    {
      for (fidx=idx; fidx < (idx+5) && fidx < NUMFLOATHDR; fidx++)
	{
	  sprintf (bp, "%#15.7g", *(fhp + fidx));
	  bp += 15;
	}
      
      sprintf (bp, "\n");
      bp += 1;
    }
  
  /* Write SAC header integer variables to output file, 5 variables per line */
  for (idx=0; idx < NUMINTHDR; idx += 5)
    {
      for (fidx=idx; fidx < (idx+5) && fidx < NUMINTHDR; fidx++)
	{
	  sprintf (bp, "%10d", *(ihp + fidx));
	  bp += 10;
	}
      
      sprintf (bp, "\n");
      bp += 1;
    }
  
  /* Write SAC header string variables to output file, 3 variables per line */
  for (idx=0; idx < (NUMSTRHDR+1); idx += 3)
    {
      if ( idx == 0 )
	{
	  sprintf (bp, "%-8.8s%-16.16s", shp, shp + 8);
	  bp += 24;
	}
      else
	{
	  for (fidx=idx; fidx < (idx+3) && fidx < (NUMSTRHDR+1); fidx++)
	    {
	      sprintf (bp, "%-8.8s", shp+(fidx*8));
	      bp += 8;
	    }
	}
      
      sprintf (bp, "\n");
      bp += 1;
    }
  
  if ( ! zipfile )
    {
      /* Open output file */
      if ( (ofp = fopen (outfile, "wb")) == NULL )
	{
	  fprintf (stderr, "Cannot open output file: %s (%s)\n",
		   outfile, strerror(errno));
	  return -1;
	}
      
      /* Write SAC header to output file */
      if ( fwrite (buffer, (bp-buffer), 1, ofp) != 1 )
	{
	  fprintf (stderr, "Error writing SAC header to output file\n");
	  return -1;
	}
      
      /* Write float data to output file, 5 values per line */
      for (idx=0; idx < npts; idx += 5)
	{
	  for (fidx=idx; fidx < (idx+5) && fidx < npts && fidx >= 0; fidx++)
	    fprintf (ofp, "%#15.7g", *(fdata + fidx));
	  
	  fprintf (ofp, "\n");
	}
      
      fclose (ofp);
    }
  else
    {
#ifndef NOFDZIP
      /* Begin ZIP entry */
      if ( ! (zentry = zs_entrybegin (zstream, outfile, time(NULL),
	                              zipmethod, &writestatus)) )
	{
	  fprintf (stderr, "Cannot begin ZIP entry, write status: %lld\n",
		   (long long int) writestatus);
	  return -1;
	}
      
      /* Write SAC header to ZIP */
      if ( ! zs_entrydata (zstream, zentry, (unsigned char *) buffer,
	                   (bp-buffer), 0, &writestatus) )
	{
	  fprintf (stderr, "Error adding entry data for %s to output ZIP, write status: %lld\n",
		   outfile, (long long int) writestatus);
	  return -1;
	}
      
      /* Write float data to buffer and then to ZIP */
      bp = buffer;
      for (idx=0; idx < npts; idx += 5)
	{
          for (fidx=idx; fidx < (idx+5) && fidx < npts && fidx >= 0; fidx++)
	    {
	      sprintf (bp, "%#15.7g", *(fdata + fidx));
	      bp += 15;
	    }
	  
	  sprintf (bp, "\n");
	  bp += 1;
	  
	  /* Write float data to ZIP */
	  flush = (idx+5 < npts) ? 0 : 1;
	  if ( ! zs_entrydata (zstream, zentry, (unsigned char *) buffer,
			       (bp-buffer), flush, &writestatus) )
	    {
	      fprintf (stderr, "Error adding entry data for %s to output ZIP, write status: %lld\n",
		       outfile, (long long int) writestatus);
	      return -1;
	    }
	  
	  bp = buffer;
	}
      
      /* End ZIP entry */
      if ( ! zs_entryend (zstream, zentry, &writestatus) )
	{
	  fprintf (stderr, "Error ending ZIP entry for %s, write status: %lld\n",
		   outfile, (long long int) writestatus);
	  return 1;
	}
#endif  /* NOFDZIP */
    }
  
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
      ms_gswap4 (ip);
    }
  
  return 0;
}  /* End of swapsacheader() */


/***************************************************************************
 * insertmetadata:
 *
 * Search the metadata list for the first matching source and insert
 * the metadata into the SAC header if found.  The source names (net,
 * sta, loc, chan) are used to find a match.  If metadata list entries
 * include a '*' they will match everything, for example if the
 * channel field is '*' all channels for the specified network,
 * station and location will match the list entry.
 *
 * The metadata list should be populated with an array of pointers to:
 *  0:  Network (knetwk)
 *  1:  Station (kstnm)
 *  2:  Location (khole)
 *  3:  Channel (kcmpnm)
 *  4:  Latitude (stla)
 *  5:  Longitude (stlo)
 *  6:  Elevation (stel) [not currently used by SAC]
 *  7:  Depth (stdp) [not currently used by SAC]
 *  8:  Component Azimuth (cmpaz), degrees clockwise from north
 *  9:  Component Incident Angle (cmpinc), degrees from vertical
 *  10: Instrument Name (kinst)
 *  11: Scale Factor (scale)
 *  12: Scale Frequency, unused
 *  13: Scale Units, unused
 *  14: Sampling rate, unused
 *  15: Start time, used for matching
 *  16: End time, used for matching
 *
 * Returns 0 on sucess, 1 when no matching metadata found and -1 on failure.
 ***************************************************************************/
static int
insertmetadata (struct SACHeader *sh, hptime_t sacstarttime)
{
  struct listnode *mlp = metadata;
  struct metanode *mn = NULL;
  hptime_t sacendtime;
  char *endptr;
  char sacnetwork[9];
  char sacstation[9];
  char saclocation[9];
  char sacchannel[9];
  int retval = 1;
  
  if ( ! mlp || ! sh )
    return -1;
  
  /* Determine source name parameters for comparison, as a special case if the 
   * location code is not set it will match '--' */
  if ( strncmp (sh->knetwk, SUNDEF, 8) ) ms_strncpclean (sacnetwork, sh->knetwk, 8);
  else sacnetwork[0] = '\0';
  if ( strncmp (sh->kstnm, SUNDEF, 8) ) ms_strncpclean (sacstation, sh->kstnm, 8);
  else sacstation[0] = '\0';
  if ( strncmp (sh->khole, SUNDEF, 8) ) ms_strncpclean (saclocation, sh->khole, 8);
  else { saclocation[0] = '-'; saclocation[1] = '-'; saclocation[2] = '\0'; }
  if ( strncmp (sh->kcmpnm, SUNDEF, 8) ) ms_strncpclean (sacchannel, sh->kcmpnm, 8);
  else sacchannel[0] = '\0';
  
  /* Calculate end time of SAC data */
  sacendtime = sacstarttime + (((sh->npts - 1) * sh->delta) * HPTMODULUS);
  
  while ( mlp )
    {
      mn = (struct metanode *) mlp->data;
      
      /* Sanity check that source name fields are present */
      if ( ! mn->metafields[0] || ! mn->metafields[1] || 
	   ! mn->metafields[2] || ! mn->metafields[3] )
	{
	  fprintf (stderr, "insertmetadata(): error, source name fields not all present\n");
	}
      /* Test if network, station, location and channel; also handle simple wildcards */
      else if ( ( ! strncmp (sacnetwork, mn->metafields[0], 8) || (*(mn->metafields[0]) == '*') ) &&
		( ! strncmp (sacstation, mn->metafields[1], 8) || (*(mn->metafields[1]) == '*') ) &&
		( ! strncmp (saclocation, mn->metafields[2], 8) || (*(mn->metafields[2]) == '*') ) &&
		( ! strncmp (sacchannel, mn->metafields[3], 8) || (*(mn->metafields[3]) == '*') ) )
	{
	  /* Check time window match */
	  if ( mn->starttime != HPTERROR || mn->endtime != HPTERROR )
	    {
	      /* Check for overlap with metadata window */
	      if ( mn->starttime != HPTERROR && mn->endtime != HPTERROR )
		{
		  if ( ! (sacendtime >= mn->starttime && sacstarttime <= mn->endtime) )
		    {
		      mlp = mlp->next;
		      continue;
		    }
		}
	      /* Check if data after start time */
	      else if ( mn->starttime != HPTERROR )
		{
		  if ( sacendtime < mn->starttime )
		    {
		      mlp = mlp->next;
		      continue;
		    }
		}
	      /* Check if data before end time */
	      else if ( mn->endtime != HPTERROR )
		{
		  if ( sacstarttime > mn->endtime )
		    {
		      mlp = mlp->next;
		      continue;
		    }
		}
	    }
	  
	  if ( verbose )
	    fprintf (stderr, "Inserting metadata for N: '%s', S: '%s', L: '%s', C: '%s' (%s - %s)\n",
		     sacnetwork, sacstation, saclocation, sacchannel,
		     (mn->metafields[15])?mn->metafields[15]:"NONE",
		     (mn->metafields[16])?mn->metafields[16]:"NONE");
	  
	  /* Insert metadata into SAC header */
	  if ( mn->metafields[4] ) sh->stla = (float) strtod (mn->metafields[4], &endptr);
	  if ( mn->metafields[5] ) sh->stlo = (float) strtod (mn->metafields[5], &endptr);
	  if ( mn->metafields[6] ) sh->stel = (float) strtod (mn->metafields[6], &endptr);
	  if ( mn->metafields[7] ) sh->stdp = (float) strtod (mn->metafields[7], &endptr);
	  if ( mn->metafields[8] ) sh->cmpaz = (float) strtod (mn->metafields[8], &endptr);
	  if ( mn->metafields[9] ) {
	    sh->cmpinc = (float) strtod (mn->metafields[9], &endptr);
	    if ( seedinc ) sh->cmpinc += 90;
	  }
	  if ( mn->metafields[10] ) strncpy (sh->kinst, mn->metafields[10], 8);
	  if ( mn->metafields[11] ) sh->scale = (float) strtod (mn->metafields[11], &endptr);
	  
	  retval = 0;
	  break;
	}
      
      mlp = mlp->next;
    }
  
  return retval;
}  /* End of insertmetadata() */


/***************************************************************************
 * delaz:
 *
 * Calculate the angular distance (and approximately equivalent
 * kilometers), azimuth and back azimuth for specified coordinates.
 * Latitudes are converted to geocentric latitudes using the WGS84
 * spheriod to correct for ellipticity.
 *
 * delta       : angular distance (degrees)
 * dist        : distance (kilometers, 111.19 km/deg)
 * azimuth     : azimuth from 1 to 2 (degrees)
 * backazimuth : azimuth from 2 to 1 (degrees)
 *
 * Returns 0 on sucess and -1 on failure.
 ***************************************************************************/
static int
delaz (double lat1, double lon1, double lat2, double lon2,
       double *delta, double *dist, double *azimuth, double *backazimuth)
{
  /* Major and minor axies for WGS84 spheriod */
  const double semimajor = 6378137.0;
  const double semiminor = 6356752.3142;
  
  double ratio2, pirad, halfpi, nlat1, nlat2, gamma, a, b, sita, bsita;
  
  ratio2 = ((semiminor * semiminor) / (semimajor * semimajor));
  
  pirad  = acos(-1.0) / 180.0;
  halfpi = acos(-1.0) / 2.0;
  
  /* Convert latitude to geocentric coordinates */
  nlat1 = atan (ratio2 * tan (lat1 * pirad));
  nlat2 = atan (ratio2 * tan (lat2 * pirad));
  
  /* Great circle calculation for delta and azimuth */
  gamma = (lon2 - lon1) * pirad;
  a = (halfpi - nlat2);
  b = (halfpi - nlat1);
  
  if ( a == 0.0 )
    sita = 1.0;
  else if ( nlat2 == 0.0 )
    sita = 0.0;
  else
    sita = sin(b) / tan(a);

  if ( b == 0.0 )
    bsita = 1.0;
  else if ( nlat1 == 0.0 )
    bsita = 0.0;
  else
    bsita = sin(a) / tan(b);
  
  *delta = acos (cos(a) * cos(b) + sin(a) * sin(b) * cos(gamma)) / pirad;
  if ( FLTEQUAL(*delta,0.0) ) *delta = 0.0;
  
  /* 111.19 km/deg */
  *dist = *delta * 111.19;
  if ( FLTEQUAL(*dist,0.0) ) *dist = 0.0;
  
  *azimuth = atan2 (sin(gamma), sita - cos(gamma) * cos(b)) / pirad;
  if ( FLTEQUAL(*azimuth,0.0) ) *azimuth = 0.0;
  else if ( *azimuth < 0.0 ) *azimuth += 360;
  
  *backazimuth = atan2 (-sin(gamma), bsita - cos(gamma) * cos(a)) / pirad;
  if ( FLTEQUAL(*backazimuth,0.0) ) *backazimuth = 0.0;
  else if ( *backazimuth < 0.0 ) *backazimuth += 360;
  
  return 0;
}  /* End of delaz() */


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
  char *metafile = 0;
  char *eventstr = 0;
  char *selectfile = 0;
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
	  usage (0);
	  exit (0);
	}
      else if (strcmp (argvec[optind], "-H") == 0)
	{
	  usage (1);
	  exit (0);
	}
      else if (strncmp (argvec[optind], "-v", 2) == 0)
	{
	  verbose += strspn (&argvec[optind][1], "v");
	}
      else if (strcmp (argvec[optind], "-O") == 0)
	{
	  overwrite = 1;
	}
      else if (strcmp (argvec[optind], "-k") == 0)
	{
	  coorstr = getoptval(argcount, argvec, optind++, 1);
	}
      else if (strcmp (argvec[optind], "-m") == 0)
	{
	  metafile = getoptval(argcount, argvec, optind++, 0);
	}
      else if (strcmp (argvec[optind], "-msi") == 0)
	{
	  seedinc = 1;
	}
      else if (strcmp (argvec[optind], "-E") == 0)
	{
	  eventstr = getoptval(argcount, argvec, optind++, 0);
	}
      else if (strcmp (argvec[optind], "-l") == 0)
	{
	  selectfile = getoptval(argcount, argvec, optind++, 0);
	}
      else if (strcmp (argvec[optind], "-f") == 0)
	{
	  sacformat = strtoul (getoptval(argcount, argvec, optind++, 0), NULL, 10);
	}
      else if (strcmp (argvec[optind], "-N") == 0)
	{
	  network = getoptval(argcount, argvec, optind++, 0);
	}
      else if (strcmp (argvec[optind], "-S") == 0)
	{
	  station = getoptval(argcount, argvec, optind++, 0);
	}
      else if (strcmp (argvec[optind], "-L") == 0)
	{
	  location = getoptval(argcount, argvec, optind++, 0);
	}
      else if (strcmp (argvec[optind], "-C") == 0)
	{
	  channel = getoptval(argcount, argvec, optind++, 0);
	}
      else if (strcmp (argvec[optind], "-r") == 0)
	{
	  reclen = strtoul (getoptval(argcount, argvec, optind++, 0), NULL, 10);
	}
      else if (strcmp (argvec[optind], "-dr") == 0)
	{
	  deriverate = 1;
	}
      else if (strcmp (argvec[optind], "-i") == 0)
	{
	  indifile = 1;
	}
      else if (strcmp (argvec[optind], "-ic") == 0)
	{
	  indichannel = 1;
	}
#ifndef NOFDZIP
      else if (strcmp (argvec[optind], "-z") == 0)
	{
	  zipfile = getoptval(argcount, argvec, optind++, 1);
	  zipmethod = ZS_DEFLATE;
	}
      else if (strcmp (argvec[optind], "-z0") == 0)
	{
	  zipfile = getoptval(argcount, argvec, optind++, 1);
	  zipmethod = ZS_STORE;
	}
#endif
      else if (strncmp (argvec[optind], "-", 1) == 0 &&
               strlen (argvec[optind]) > 1 )
        {
          fprintf(stderr, "Unknown option: %s\n", argvec[optind]);
          exit (1);
        }
      else
        {
	  /* Add the file name to the intput file list */
          if ( ! addnode (&filelist, NULL, 0, argvec[optind], strlen(argvec[optind])+1) )
	    {
	      fprintf (stderr, "Error adding file name to list\n");
	    }
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
	  fprintf (stderr, "Error parsing coordinates (LAT/LON): '%s'\n", coorstr);
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
      char *etime,*elat,*elon,*edepth, *ename;
      char *endptr = 0;
      
      etime = eventstr;
      elat = elon = edepth = ename = 0;
      
      if ( (elat = strchr (etime, '/')) )
	{
	  *elat++ = '\0';
	  
	  if ( (elon = strchr (elat, '/')) )
	    {
	      *elon++ = '\0';
	      
	      if ( (edepth = strchr (elon, '/')) )
		{
		  *edepth++ = '\0';
		  
		  if ( (ename = strchr (edepth, '/')) )
		    {
		      *ename++ = '\0';
		    }
		}
	    }
	}
      
      /* Parse event time */
      eventtime = ms_seedtimestr2hptime (etime);
      
      if ( eventtime == HPTERROR )
	{
	  fprintf (stderr, "Error parsing event time: '%s'\n", etime);
	  fprintf (stderr, "Try %s -h for usage\n", PACKAGE);
	  return -1;
	}
      
      /* Process remaining event information */
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
      if ( ename )
	if ( *ename )
	  eventname = ename;
    }
  
  /* Read data selection file */
  if ( selectfile )
    {
      if ( ms_readselectionsfile (&selections, selectfile) < 0 )
        {
          fprintf (stderr, "Cannot read data selection file\n");
          return -1;
	}
      
      if ( verbose > 1 )
	ms_printselections (selections);
    }
  
  /* Read metadata file if specified */
  if ( metafile )
    {
      if ( readmetadata (metafile) )
	{
	  fprintf (stderr, "Error reading metadata file\n");
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
getoptval (int argcount, char **argvec, int argopt, int dasharg)
{
  if ( argvec == NULL || argvec[argopt] == NULL ) {
    fprintf (stderr, "getoptval(): NULL option requested\n");
    exit (1);
    return 0;
  }
  
  /* When the value potentially starts with a dash (-) */
  if ( (argopt+1) < argcount && dasharg )
    return argvec[argopt+1];
  
  /* Otherwise check that the value is not another option */
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

	  /* Add file name to the intput file list */
	  if ( ! addnode (&filelist, NULL, 0, filename, strlen(filename)+1) )
	    {
	      fprintf (stderr, "Error adding file name to list\n");
	    }
	  
          filecnt++;

          continue;
        }
    }

  fclose (fp);

  return filecnt;
}  /* End readlistfile() */


/***************************************************************************
 * readmetadata:
 *
 * Read a file of metadata into a structured list, each line should
 * contain the following fields (comma or bar separated) in this order:
 *
 * The metadata list should be populated with an array of pointers to:
 *  0:  Network (knetwk)
 *  1:  Station (kstnm)
 *  2:  Location (khole)
 *  3:  Channel (kcmpnm)
 *  4:  Latitude (stla)
 *  5:  Longitude (stlo)
 *  6:  Elevation (stel) [not currently used by SAC]
 *  7:  Depth (stdp) [not currently used by SAC]
 *  8:  Component Azimuth (cmpaz), degrees clockwise from north
 *  9:  Component Incident Angle (cmpinc), degrees from vertical
 *  10: Instrument Name (kinst)
 *  11: Scale Factor (scale)
 *  12: Scale Frequency, unused
 *  13: Scale Units, unused
 *  14: Sampling rate, unused
 *  15: Start time, used for matching
 *  16: End time, used for matching
 *
 * Any lines not containing at least 3 separators (commas or vertical
 * bars) are skipped.  If fields are not specified the values are set
 * to NULL with the execption that the first 4 fields (net, sta, loc &
 * chan) cannot be empty.
 *
 * If the separators are commas the component inclination is assumed
 * to be in the SAC convention.  If the separators are vertical bars
 * (|) the component inclination is assumed to be a SEED dip and the
 * seedinc variable will be set to 1.
 *
 * Any lines beginning with '#' are skipped, think comments.
 *
 * Returns 0 on sucess and -1 on failure.
 ***************************************************************************/
static int
readmetadata (char *metafile)
{
  struct metanode mn;
  FILE *mfp;
  char line[1024];
  char *lineptr;
  char *fp;
  char delim;
  int commas = 0;
  int bars = 0;
  int idx;
  int linecount = 0;
  
  if ( ! metafile )
    return -1;
  
  if ( (mfp = fopen (metafile, "rb")) == NULL )
    {
      fprintf (stderr, "Cannot open metadata output file: %s (%s)\n",
	       metafile, strerror(errno));
      return -1;
    }

  if ( verbose )
    fprintf (stderr, "Reading station/channel metadata from %s\n", metafile);
  
  while ( fgets (line, sizeof(line), mfp) )
    {
      linecount++;

      /* Truncate at line return if any */
      if ( (fp = strchr (line, '\n')) )
	*fp = '\0';
      
      /* Count the number of commas */
      commas = 0;
      fp = line;
      while ( (fp = strchr (fp, ',')) )
	{
	  commas++;
	  fp++;
	}
      /* Count the number of vertical bars */
      bars = 0;
      fp = line;
      while ( (fp = strchr (fp, '|')) )
	{
	  bars++;
	  fp++;
	}
      
      if ( bars > 0 )
	{
	  delim = '|';
	  seedinc = 1;
	}
      else
	{
	  delim = ',';
	}

      /* Must have at least 3 separators for Net, Sta, Loc, Chan ... */
      if ( (( delim == '|' ) ? bars : commas) < 3 )
	{
	  if ( verbose > 1 )
	    fprintf (stderr, "Skipping metadata line: %s\n", line);
	  continue;
	}
      
      /* Check for comment line beginning with '#' */
      if ( line[0] == '#' )
	{
	  if ( verbose > 1 )
	    fprintf (stderr, "Skipping comment line: %s\n", line);
	  continue;
	}
      
      /* Create a copy of the line */
      lineptr = strdup (line);
      
      mn.metafields[0] = fp = lineptr;
      mn.starttime = HPTERROR;
      mn.endtime = HPTERROR;
      
      /* Separate line on delimiter and index in metafields array */
      for (idx = 1; idx < MAXMETAFIELDS; idx++)
	{
	  mn.metafields[idx] = NULL;
	  
	  if ( fp )
	    {
	      if ( (fp = strchr (fp, delim)) )
		{
		  *fp++ = '\0';
		  
		  if ( *fp != ',' && *fp != '\0' )
		    mn.metafields[idx] = fp;
		}
	    }
	}
      
      /* Trim last field if more fields exist */
      if ( fp && (fp = strchr (fp, ',')) )
	*fp = '\0';
      
      /* Sanity check, source name fields must be populated */
      for (idx = 0; idx <= 3; idx++)
	{
	  if ( mn.metafields[idx] == NULL )
	    {
	      fprintf (stderr, "Error, field %d cannot be empty in metadata file line %d\n",
		       idx+1, linecount);
	      fprintf (stderr, "Perhaps a wildcard character (*) was the intention?\n");
	      
	      exit (1);
	    }
	}
      
      /* Parse and convert start time */
      if ( mn.metafields[15] )
	{
	  if ( (mn.starttime = ms_timestr2hptime (mn.metafields[15])) == HPTERROR )
	    {
	      fprintf (stderr, "Error parsing metadata start time: '%s'\n", mn.metafields[15]);
	      exit (1);
	    }
	}
      
      /* Parse and convert end time */
      if ( mn.metafields[16] )
	{
	  if ( (mn.endtime = ms_timestr2hptime (mn.metafields[16])) == HPTERROR )
	    {
	      fprintf (stderr, "Error parsing metadata end time: '%s'\n", mn.metafields[16]);
	      exit (1);
	    }
	}
      
      /* Add the metanode to the metadata list */
      if ( ! addnode (&metadata, NULL, 0, &mn, sizeof(struct metanode)) )
	{
	  fprintf (stderr, "Error adding metadata fields to list\n");
	}
    }
  
  fclose (mfp);
  
  return 0;
}  /* End of readmetadata() */


/***************************************************************************
 * addnode:
 *
 * Add node to the specified list.  Copies of the key and data are created.
 *
 * Return a pointer to the added node on success and NULL on error.
 ***************************************************************************/
static struct listnode *
addnode (struct listnode **listroot, void *key, int keylen,
	 void *data, int datalen)
{
  struct listnode *lastlp, *newlp;
  
  if ( data == NULL )
    {
      fprintf (stderr, "addnode(): No data specified\n");
      return NULL;
    }
  
  lastlp = *listroot;
  while ( lastlp != 0 )
    {
      if ( lastlp->next == 0 )
        break;

      lastlp = lastlp->next;
    }

  /* Create new listnode */
  newlp = (struct listnode *) malloc (sizeof (struct listnode));
  memset (newlp, 0, sizeof (struct listnode));
  
  if ( key )
    {
      newlp->key = malloc (keylen);
      memcpy (newlp->key, key, keylen);
    }
  
  if ( data)
    {
      newlp->data = malloc (datalen);
      memcpy (newlp->data, data, datalen);
    }
  
  newlp->next = 0;
  
  if ( lastlp == 0 )
    *listroot = newlp;
  else
    lastlp->next = newlp;
  
  return newlp;
}  /* End of addnode() */


/***************************************************************************
 * usage:
 * Print the usage message and exit.
 ***************************************************************************/
static void
usage (int level)
{
  fprintf (stderr, "%s version: %s\n\n", PACKAGE, VERSION);
  fprintf (stderr, "Convert Mini-SEED data to SAC\n\n");
  fprintf (stderr, "Usage: %s [options] input1.mseed [input2.mseed ...]\n\n", PACKAGE);
  fprintf (stderr,
	   " ## Options ##\n"
	   " -V             Report program version\n"
	   " -h             Show this usage message\n"
	   " -H             Print an extended usage message\n"
	   " -v             Be more verbose, multiple flags can be used\n"
	   " -O             Overwrite existing output files, default creates new file names\n"
	   "\n"
	   " -k lat/lon     Specify station coordinates as 'Latitude/Longitude' in degrees\n"
	   " -m metafile    File containing station metadata (coordinates and more)\n"
	   " -msi           Convert component inclination/dip from SEED to SAC convention\n"
	   " -E event       Specify event parameters as 'Time[/Lat][/Lon][/Depth][/Name]'\n"
	   "                  e.g. '2006,123,15:27:08.7/-20.33/-174.03/65.5/Tonga'\n"
	   " -l selectfile  Read a list of selections from file, used for subsetting\n"
	   "\n"
	   " -f format      Specify SAC file format (default is 2:binary):\n"
           "                  1=alpha, 2=binary (host byte order),\n"
           "                  3=binary (little-endian), 4=binary (big-endian)\n"
	   "\n"
	   " More options are available, to see their description use the -H option\n"
	   "\n");
  
  if ( level >= 1 )
    {
      fprintf (stderr,
	       " -N network     Specify the network code, overrides any value in the SEED\n"
	       " -S station     Specify the station code, overrides any value in the SEED\n"
	       " -L location    Specify the location code, overrides any value in the SEED\n"
	       " -C channel     Specify the channel code, overrides any value in the SEED\n"
	       " -r bytes       Specify SEED record length in bytes, autodetected by default\n"
	       " -i             Process each input file individually instead of merged\n"
	       " -ic            Process each channel individually, data should be well ordered\n"
	       " -dr            Use the sampling rate derived from the time stamps instead\n"
	       "                  of the sample rate denoted in the input data\n");
#ifndef NOFDZIP
      fprintf (stderr,
	       " -z zipfile     Write all SAC files to a ZIP archive, use '-' for stdout\n"
	       " -z0 zipfile    Same as -z but do not compress archive entries\n");
#endif
      
      fprintf (stderr, "\n");
    }
  
}  /* End of usage() */
