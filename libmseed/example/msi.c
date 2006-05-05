/***************************************************************************
 * msi.c - Mini-SEED Inspector
 *
 * A rather useful example of using the Mini-SEED record library.
 *
 * Opens a user specified file, parses the Mini-SEED records and prints
 * details for each record, trace list or gap list.
 *
 * Written by Chad Trabant, IRIS Data Management Center.
 *
 * modified 2005.271
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>

#ifndef WIN32
  #include <signal.h>
  static void term_handler (int sig);
#endif

#include <libmseed.h>

static int parameter_proc (int argcount, char **argvec);
static char *getoptval (int argcount, char **argvec, int argopt);
static int lisnumber (char *number);
static void addfile (char *filename);
static void usage (void);
static void term_handler (int sig);

#define VERSION "[libmseed " LIBMSEED_VERSION " example]"
#define PACKAGE "msi"

static flag    verbose      = 0;
static flag    ppackets     = 0;    /* Controls printing of header/blockettes */
static flag    printdata    = 0;    /* Controls printing of sample values */
static flag    printoffset  = 0;    /* Controls printing offset into input file */
static flag    basicsum     = 0;    /* Controls printing of basic summary */
static flag    tracegapsum  = 0;    /* Controls printing of trace or gap list */
static flag    tracegaponly = 0;    /* Controls printing of trace or gap list only */
static flag    tracegaps    = 0;    /* Controls printing of gaps with a trace list */
static flag    timeformat   = 0;    /* Time string format for trace or gap lists */
static double  mingap       = 0;    /* Minimum gap/overlap seconds when printing gap list */
static double *mingapptr    = NULL;
static double  maxgap       = 0;    /* Maximum gap/overlap seconds when printing gap list */
static double *maxgapptr    = NULL;
static flag    traceheal    = 0;    /* Controls healing of trace group */
static int     reccntdown   = -1;
static int     reclen       = 0;
static char   *encodingstr  = 0;
static char   *binfile      = 0;
static char   *outfile      = 0;
static hptime_t starttime   = HPTERROR;
static hptime_t endtime     = HPTERROR;

struct filelink {
  char *filename;
  struct filelink *next;
};

struct filelink *filelist = 0;


int
main (int argc, char **argv)
{
  struct filelink *flp;
  MSrecord *msr = 0;
  TraceGroup *mstg = 0;
  FILE *bfp = 0;
  FILE *ofp = 0;

  char envvariable[100];
  int dataflag   = 0;
  int totalrecs  = 0;
  int totalsamps = 0;
  off_t filepos  = 0;

#ifndef WIN32
  /* Signal handling, use POSIX calls with standardized semantics */
  struct sigaction sa;

  sa.sa_flags = SA_RESTART;
  sigemptyset (&sa.sa_mask);
  
  sa.sa_handler = term_handler;
  sigaction (SIGINT, &sa, NULL);
  sigaction (SIGQUIT, &sa, NULL);
  sigaction (SIGTERM, &sa, NULL);
  
  sa.sa_handler = SIG_IGN;
  sigaction (SIGHUP, &sa, NULL);
  sigaction (SIGPIPE, &sa, NULL);
#endif

  /* Process given parameters (command line and parameter file) */
  if (parameter_proc (argc, argv) < 0)
    return -1;
  
  /* Setup encoding environment variable if specified, ugly kludge */
  if ( encodingstr )
    {
      snprintf (envvariable, sizeof(envvariable), "UNPACK_DATA_FORMAT=%s", encodingstr);
      
      if ( putenv (envvariable) )
	{
	  fprintf (stderr, "Error setting environment variable UNPACK_DATA_FORMAT\n");
	  return -1;
	}
    }
  
  /* Open the integer output file if specified */
  if ( binfile )
    {
      if ( strcmp (binfile, "-") == 0 )
	{
	  bfp = stdout;
	}
      else if ( (bfp = fopen (binfile, "wb")) == NULL )
	{
	  fprintf (stderr, "Cannot open binary data output file: %s (%s)\n",
		   binfile, strerror(errno));
	  return -1;
	}
    }

  /* Open the output file if specified */
  if ( outfile )
    {
      if ( strcmp (outfile, "-") == 0 )
	{
	  ofp = stdout;
	}
      else if ( (ofp = fopen (outfile, "wb")) == NULL )
	{
	  fprintf (stderr, "Cannot open output file: %s (%s)\n",
		   outfile, strerror(errno));
	  return -1;
	}
    }
  
  if ( printdata || binfile )
    dataflag = 1;
  
  if ( tracegapsum || tracegaponly )
    mstg = mst_initgroup (NULL);
  
  flp = filelist;

  while ( flp != 0 )
    {
      if ( verbose >= 2 )
	fprintf (stderr, "Processing: %s\n", flp->filename);
      
      /* Loop over the input file */
      while ( reccntdown != 0 )
	{
	  if ( ! (msr = ms_readmsr (flp->filename, reclen, &filepos, NULL, 1, dataflag, verbose)))
	    break;
	  
	  /* Check if record matches start/end time criteria */
	  if ( starttime != HPTERROR && (msr->starttime < starttime) )
	    {
	      if ( verbose >= 3 )
		{
		  char srcname[100], stime[100];
		  msr_srcname (msr, srcname);
		  ms_hptime2seedtimestr (msr->starttime, stime);
		  fprintf (stderr, "Skipping %s, %s\n", srcname, stime);
		}
	      continue;
	    }
	  
	  if ( endtime != HPTERROR && (msr_endtime(msr) > endtime) )
	    {
	      if ( verbose >= 3 )
		{
		  char srcname[100], stime[100];
		  msr_srcname (msr, srcname);
		  ms_hptime2seedtimestr (msr->starttime, stime);
		  fprintf (stderr, "Skipping %s, %s\n", srcname, stime);
		}
	      continue;
	    }
	  
	  if ( reccntdown > 0 )
	    reccntdown--;
	  
	  totalrecs++;
	  totalsamps += msr->samplecnt;
	  
	  if ( ! tracegaponly )
	    {
	      if ( printoffset )
		printf ("%-10lld", (long long) filepos);
	      
	      msr_print (msr, ppackets);
	    }
	  
	  if ( tracegapsum || tracegaponly )
	    mst_addmsrtogroup (mstg, msr, -1.0, -1.0);
	  
	  if ( dataflag )
	    {
	      if ( printdata && ! tracegaponly )
		{
		  int line, col, cnt, samplesize;
		  int lines = (msr->numsamples / 6) + 1;
		  void *sptr;
		  
		  if ( (samplesize = get_samplesize(msr->sampletype)) == 0 )
		    {
		      fprintf (stderr, "Unrecognized sample type: %c\n", msr->sampletype);
		    }

		  if ( msr->sampletype == 'a' )
		    printf ("ASCII Data:\n%.*s\n", msr->numsamples, (char *)msr->datasamples);
		  else
		    for ( cnt = 0, line = 0; line < lines; line++ )
		      {
			for ( col = 0; col < 6 ; col ++ )
			  {
			    if ( cnt < msr->numsamples )
			      {
				sptr = (char*)msr->datasamples + (cnt * samplesize);
				
				if ( msr->sampletype == 'i' )
				  printf ("%10d  ", *(int32_t *)sptr);
				
				else if ( msr->sampletype == 'f' )
				  printf ("%10.8g  ", *(float *)sptr);
				
				else if ( msr->sampletype == 'd' )
				  printf ("%10.10g  ", *(double *)sptr);
				
				cnt++;
			      }
			  }
			printf ("\n");
		      }
		}
	      if ( binfile )
		{
		  fwrite (msr->datasamples, 4, msr->numsamples, bfp);
		}
	    }

	  if ( outfile )
	    {
	      fwrite (msr->record, 1, msr->reclen, ofp);
	    }
	}
      
      /* Make sure everything is cleaned up */
      ms_readmsr (NULL, 0, NULL, NULL, 0, 0, 0);

      flp = flp->next;
    } /* End of looping over file list */

  if ( binfile )
    fclose (bfp);

  if ( outfile )
    fclose (ofp);
  
  if ( basicsum )
    printf ("Records: %d, Samples: %d\n", totalrecs, totalsamps);
    
  if ( tracegapsum || tracegaponly )
    {
      mst_groupsort (mstg);
      
      if ( traceheal )
	mst_heal (mstg, -1.0, -1.0);
      
      if ( tracegapsum == 1 || tracegaponly == 1 )
	{
	  mst_printtracelist (mstg, timeformat, 1, tracegaps);
	}
      if ( tracegapsum == 2 || tracegaponly == 2 )
	{
	  mst_printgaplist (mstg, timeformat, mingapptr, maxgapptr);
	}
    }
  
  return 0;
}  /* End of main() */


/***************************************************************************
 * parameter_proc():
 * Process the command line parameters.
 *
 * Returns 0 on success, and -1 on failure
 ***************************************************************************/
static int
parameter_proc (int argcount, char **argvec)
{
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
      else if (strncmp (argvec[optind], "-p", 2) == 0)
	{
	  ppackets += strspn (&argvec[optind][1], "p");
	}
      else if (strcmp (argvec[optind], "-a") == 0)
	{
	  reclen = -1;
	}
      else if (strcmp (argvec[optind], "-O") == 0)
	{
	  printoffset = 1;
	}
      else if (strcmp (argvec[optind], "-s") == 0)
	{
	  basicsum = 1;
	}
      else if (strcmp (argvec[optind], "-t") == 0)
	{
	  tracegapsum = 1;
	}
      else if (strcmp (argvec[optind], "-T") == 0)
	{
	  tracegaponly = 1;
	}
      else if (strcmp (argvec[optind], "-tg") == 0)
	{
	  tracegaps = 1;
	}
      else if (strcmp (argvec[optind], "-g") == 0)
	{
	  tracegapsum = 2;
	}
      else if (strcmp (argvec[optind], "-G") == 0)
	{
	  tracegaponly = 2;
	}
      else if (strcmp (argvec[optind], "-min") == 0)
	{
	  mingap = strtod (getoptval(argcount, argvec, optind++), NULL);
	  mingapptr = &mingap;
	}
      else if (strcmp (argvec[optind], "-max") == 0)
	{
	  maxgap = strtod (getoptval(argcount, argvec, optind++), NULL);
	  maxgapptr = &maxgap;
	}
      else if (strcmp (argvec[optind], "-H") == 0)
	{
	  traceheal = 1;
	}
      else if (strcmp (argvec[optind], "-tf") == 0)
	{
	  timeformat = strtol (getoptval(argcount, argvec, optind++), NULL, 10);
	}
      else if (strcmp (argvec[optind], "-ts") == 0)
	{
	  starttime = ms_seedtimestr2hptime (getoptval(argcount, argvec, optind++));
	  if ( starttime == HPTERROR )
	    return -1;
	}
      else if (strcmp (argvec[optind], "-te") == 0)
	{
	  endtime = ms_seedtimestr2hptime (getoptval(argcount, argvec, optind++));
	  if ( endtime == HPTERROR )
	    return -1;
	}
      else if (strcmp (argvec[optind], "-n") == 0)
	{
	  reccntdown = strtol (getoptval(argcount, argvec, optind++), NULL, 10);
	}
      else if (strcmp (argvec[optind], "-r") == 0)
	{
	  reclen = strtol (getoptval(argcount, argvec, optind++), NULL, 10);
	}
      else if (strcmp (argvec[optind], "-e") == 0)
	{
	  encodingstr = getoptval(argcount, argvec, optind++);
	}
      else if (strcmp (argvec[optind], "-d") == 0)
	{
	  printdata = 1;
	}
      else if (strcmp (argvec[optind], "-b") == 0)
	{
	  binfile = getoptval(argcount, argvec, optind++);
	}
      else if (strcmp (argvec[optind], "-o") == 0)
	{
	  outfile = getoptval(argcount, argvec, optind++);
	}
      else if (strncmp (argvec[optind], "-", 1) == 0 &&
	       strlen (argvec[optind]) > 1 )
	{
	  fprintf(stderr, "Unknown option: %s\n", argvec[optind]);
	  exit (1);
	}
      else
	{
	  addfile (argvec[optind]);
	}
    }

  /* Make sure input file were specified */
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
    return NULL;
  }
  
  /* Special case of '-o -' usage */
  if ( (argopt+1) < argcount && strcmp (argvec[argopt], "-o") == 0 )
    if ( strcmp (argvec[argopt+1], "-") == 0 )
      return argvec[argopt+1];
  
  /* Special cases of '-min' and '-max' with negative numbers */
  if ( (argopt+1) < argcount &&
       (strcmp (argvec[argopt], "-min") == 0 || (strcmp (argvec[argopt], "-max") == 0)))
    if ( lisnumber(argvec[argopt+1]) )
      return argvec[argopt+1];
  
  if ( (argopt+1) < argcount && *argvec[argopt+1] != '-' )
    return argvec[argopt+1];
  
  fprintf (stderr, "Option %s requires a value\n", argvec[argopt]);
  exit (1);
  return NULL;
}  /* End of getoptval() */


/***************************************************************************
 * lisnumber:
 *
 * Test if the string is all digits allowing an initial minus sign.
 *
 * Return 0 if not a number otherwise 1.
 ***************************************************************************/
static int
lisnumber (char *number)
{
  int idx = 0;
  
  while ( *(number+idx) )
    {
      if ( idx == 0 && *(number+idx) == '-' )
	{
	  idx++;
	  continue;
	}

      if ( ! isdigit ((int) *(number+idx)) )
	{
	  return 0;
	}

      idx++;
    }
  
  return 1;      
}  /* End of lisnumber() */


/***************************************************************************
 * addfile:
 *
 * Add file to end of the global file list (filelist).
 ***************************************************************************/
static void
addfile (char *filename)
{
  struct filelink *lastlp, *newlp;
  
  if ( filename == NULL )
    {
      fprintf (stderr, "addfile(): No file name specified\n");
      return;
    }
  
  lastlp = filelist;
  while ( lastlp != 0 )
    {
      if ( lastlp->next == 0 )
	break;
      
      lastlp = lastlp->next;
    }
  
  newlp = (struct filelink *) malloc (sizeof (struct filelink));
  newlp->filename = strdup(filename);
  newlp->next = 0;
  
  if ( lastlp == 0 )
    filelist = newlp;
  else
    lastlp->next = newlp;
  
}  /* End of addfile() */


/***************************************************************************
 * usage():
 * Print the usage message and exit.
 ***************************************************************************/
static void
usage (void)
{
  fprintf (stderr, "%s - Mini-SEED Inspector version: %s\n\n", PACKAGE, VERSION);
  fprintf (stderr, "Usage: %s [options] file1 [file2] [file3] ...\n\n", PACKAGE);
  fprintf (stderr,
	   " ## Options ##\n"
	   " -V           Report program version\n"
	   " -h           Show this usage message\n"
	   " -v           Be more verbose, multiple flags can be used\n"
	   " -p           Print details of header, multiple flags can be used\n"
	   " -a           Autodetect every record length, only needed with mixed lengths\n"
	   " -O           Include offset into file when printing header details\n"
	   " -s           Print a basic summary after processing file(s)\n"
	   " -t           Print a sorted trace list after processing file(s)\n"
	   " -T           Only print a sorted trace list\n"
	   " -tg          Include gap estimates when printing trace list\n"
	   " -g           Print a sorted gap/overlap list after processing file(s)\n"
	   " -G           Only print a sorted gap/overlap list\n"
	   " -min secs    Only report gaps/overlaps larger or equal to specified seconds\n"
	   " -max secs    Only report gaps/overlaps smaller or equal to specified seconds\n"
	   " -H           Heal trace segments, for out of time order data\n"
	   " -tf format   Specify a time string format for trace and gap lists\n"
	   "                format: 0 = SEED time, 1 = ISO time, 2 = epoch time\n"
	   " -ts time     Limit to records that start after time\n"
	   " -te time     Limit to records that end before time\n"
	   "                time format: 'YYYY[,DDD,HH,MM,SS,FFFFFF]' delimiters: [,:.]\n"
	   " -n count     Only process count number of records\n"
	   " -r bytes     Specify record length in bytes, required if no 1000 Blockettes\n"
	   " -e encoding  Specify encoding format of data samples\n"
	   " -d           Unpack/decompress data and print samples\n"
	   " -b binfile   Unpack/decompress data and write binary samples to binfile\n"
	   " -o outfile   Write processed records to outfile\n"
	   "\n"
	   " file#        File of Mini-SEED records\n"
	   "\n");
}  /* End of usage() */


#ifndef WIN32
/***************************************************************************
 * term_handler:
 * Signal handler routine for termination.
 ***************************************************************************/
static void
term_handler (int sig)
{
  exit (0);
}
#endif
