/***************************************************************************
 * msview.c
 *
 * A simple example of using libmseed.
 *
 * Opens a user specified file, parses the Mini-SEED records and prints
 * details for each record.
 *
 * Written by Chad Trabant, ORFEUS/EC-Project MEREDIAN
 *
 * modified 2006.107
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#ifndef WIN32
  #include <signal.h>
  static void term_handler (int sig);
#endif

#include <libmseed.h>

#define VERSION "[libmseed " LIBMSEED_VERSION " example]"
#define PACKAGE "msview"

static short int verbose   = 0;
static short int ppackets  = 0;
static short int basicsum  = 0;
static int   reclen        = -1;
static char *inputfile     = 0;

static int parameter_proc (int argcount, char **argvec);
static void usage (void);
static void term_handler (int sig);

int
main (int argc, char **argv)
{
  MSRecord *msr = 0;

  int dataflag   = 0;
  int totalrecs  = 0;
  int totalsamps = 0;

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
      
  /* Loop over the input file */
  while ( (msr = ms_readmsr (inputfile, reclen, NULL, NULL, 1, dataflag, verbose)) )
    {      
      totalrecs++;
      totalsamps += msr->samplecnt;
      
      msr_print (msr, ppackets);
    }
  
  /* Make sure everything is cleaned up */
  ms_readmsr (NULL, 0, NULL, NULL, 0, 0, 0);
  
  if ( basicsum )
    printf ("Records: %d, Samples: %d\n", totalrecs, totalsamps);
  
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
      else if (strcmp (argvec[optind], "-s") == 0)
	{
	  basicsum = 1;
	}
      else if (strcmp (argvec[optind], "-r") == 0)
	{
	  reclen = atoi (argvec[++optind]);
	}
      else if (strncmp (argvec[optind], "-", 1) == 0 &&
	       strlen (argvec[optind]) > 1 )
	{
	  fprintf(stderr, "Unknown option: %s\n", argvec[optind]);
	  exit (1);
	}
      else if ( inputfile == 0 )
	{
	  inputfile = argvec[optind];
	}
      else
	{
	  fprintf(stderr, "Unknown option: %s\n", argvec[optind]);
	  exit (1);
	}
    }

  /* Make sure an inputfile was specified */
  if ( ! inputfile )
    {
      fprintf (stderr, "No input file was specified\n\n");
      fprintf (stderr, "%s version %s\n\n", PACKAGE, VERSION);
      fprintf (stderr, "Try %s -h for usage\n", PACKAGE);
      exit (1);
    }

  /* Report the program version */
  if ( verbose )
    printf ("%s version: %s\n", PACKAGE, VERSION);

  return 0;
}  /* End of parameter_proc() */


/***************************************************************************
 * usage():
 * Print the usage message and exit.
 ***************************************************************************/
static void
usage (void)
{
  fprintf (stderr, "%s version: %s\n\n", PACKAGE, VERSION);
  fprintf (stderr, "Usage: %s [options] file\n\n", PACKAGE);
  fprintf (stderr,
	   " ## Options ##\n"
	   " -V             Report program version\n"
	   " -h             Show this usage message\n"
	   " -v             Be more verbose, multiple flags can be used\n"
	   " -p             Print details of header, multiple flags can be used\n"
	   " -s             Print a basic summary after processing a file\n"
	   " -r bytes       Specify record length in bytes, required if no Blockette 1000\n"
	   "\n"
	   " file           File of Mini-SEED records\n"
	   "\n");
}  /* End of usage() */


#ifndef WIN32
/***************************************************************************
 * term_handler:
 * Signal handler routine. 
 ***************************************************************************/
static void
term_handler (int sig)
{
  exit (0);
}
#endif
