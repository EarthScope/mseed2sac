/***************************************************************************
 * fileutils.c:
 *
 * Routines to manage files of Mini-SEED.
 *
 * Written by Chad Trabant, ORFEUS/EC-Project MEREDIAN
 *
 * modified: 2005.335
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "libmseed.h"

static int myfread (char *buf, int size, int num, FILE *stream);
static int ateof (FILE *stream);


/***************************************************************************
 * ms_readmsr:
 *
 * This routine will open and read, with subsequent calls, all
 * Mini-SEED records in specified file.  It is not thread safe.  It
 * cannot be used to read more that one file at a time.
 *
 * If reclen is 0 the length of the first record is automatically
 * detected, all subsequent records are then expected to have the same
 * length as the first.
 *
 * If reclen is negative the length of every record is automatically
 * detected.
 *
 * For auto detection of record length the record must include a 1000
 * blockette.  This routine will search up to 8192 bytes into the
 * record for the 1000 blockette.
 *
 * If *fpos is not NULL it will be updated to reflect the file
 * position (offset from the beginning in bytes) from where the
 * returned record was read.
 *
 * If *last is not NULL it will be set to 1 when the last record in
 * the file is being returned, otherwise it will be 0.
 *
 * If the skipnotdata flag is true any data chunks read that do not
 * have vald data record indicators (D, R, Q, etc.) will be skipped.
 *
 * dataflag will be passed directly to msr_unpack().
 *
 * After reading all the records in a file the controlling program can
 * call it one last time with msfile set to NULL.  This will close the
 * file and free allocated memory.
 *
 * Returns the next read record or NULL on EOF, error or cleanup.
 ***************************************************************************/
MSrecord *
ms_readmsr (char *msfile, int reclen, off_t *fpos, int *last,
	    flag skipnotdata, flag dataflag, flag verbose)
{
  static MSrecord *msr = NULL;
  static FILE *fp = NULL;
  static char *rawrec = NULL;
  static int autodet = 1;
  static int readlen = MINRECLEN;
  int autodetexp = 8;
  int prevreadlen;
  int detsize;
  
  /* When cleanup is requested */
  if ( msfile == NULL )
    {
      msr_free (&msr);

      if ( fp != NULL )
	fclose (fp);

      if ( rawrec != NULL )
	free (rawrec);

      fp = NULL;
      rawrec = NULL;
      autodet = 1;
      readlen = MINRECLEN;
      
      return NULL;
    }
  
  /* Open the file if needed, redirect to stdin if file is "-" */
  if ( fp == NULL )
    {
      if ( strcmp (msfile, "-") == 0 )
	{
	  fp = stdin;
	}
      else if ( (fp = fopen (msfile, "rb")) == NULL )
	{
	  fprintf (stderr, "Error opening file: %s (%s)\n",
		   msfile, strerror (errno));
	  
	  msr_free (&msr);
	  
	  return NULL;
	}
    }
  
  /* Force the record length if specified */
  if ( reclen > 0 && autodet )
    {
      readlen = reclen;
      autodet = 0;
      
      rawrec = (char *) malloc (readlen);
    }
  
  /* If reclen is negative reset readlen for autodetection */
  if ( reclen < 0 )
    readlen = (unsigned int) 1 << autodetexp;
  
  /* Zero the last record indicator */
  if ( last )
    *last = 0;
  
  /* Autodetect the record length */
  if ( autodet || reclen < 0 )
    {
      detsize = 0;
      prevreadlen = 0;
      
      while ( detsize <= 0 && readlen <= 8192 )
	{
	  rawrec = (char *) realloc (rawrec, readlen);
	  
	  if ( fpos != NULL )
	    *fpos = lmp_ftello (fp);
	  
	  if ( (myfread (rawrec + prevreadlen, 1, (readlen - prevreadlen), fp)) < (readlen - prevreadlen) )
	    {
	      if ( ! feof (fp) )
		fprintf (stderr, "Short read at %d bytes during length detection\n", readlen);
	      
	      fclose (fp); fp = NULL; msr_free (&msr);
	      free (rawrec); rawrec = NULL;
	      return NULL;
	    }
	  
	  /* Test for data record and return record length */
	  if ( (detsize = ms_find_reclen (rawrec, readlen)) > 0 )
	    {
	      break;
	    }
	  
	  /* Skip if data record not detected */
	  if ( skipnotdata && detsize == -1 )
	    {
	      detsize = -1;

	      if ( verbose > 1 )
		{
		  if ( fpos != NULL )
		    fprintf (stderr, "Skipped non-data record at byte offset %lld\n", (long long) *fpos);
		  else
		    fprintf (stderr, "Skipped non-data record\n");		
		}
	    }
	  /* Otherwise increase read length to the next record size up */
	  else
	    {
	      prevreadlen = readlen;
	      autodetexp++;
	      readlen = (unsigned int) 1 << autodetexp;
	    }
	}
      
      if ( detsize <= 0 )
	{
	  fprintf (stderr, "Cannot detect record length: %s\n", msfile);
	  
	  fclose (fp); fp = NULL; msr_free (&msr);
	  free (rawrec); rawrec = NULL;
	  return NULL;
	}
      
      autodet = 0;
      
      if ( verbose > 0 )
	fprintf (stderr, "Detected record length of %d bytes\n", detsize);
      
      if ( detsize < MINRECLEN || detsize > MAXRECLEN )
	{
	  fprintf (stderr, "Detected record length is out of range: %d\n", detsize);
	  
	  fclose (fp); fp = NULL; msr_free (&msr);
	  free (rawrec); rawrec = NULL;
	  return NULL;
	}
            
      rawrec = (char *) realloc (rawrec, detsize);
      
      /* Read the rest of the first record */
      if ( (detsize - readlen) > 0 )
	{
	  if ( (myfread (rawrec+readlen, 1, detsize-readlen, fp)) < (detsize-readlen) )
	    {
	      fclose (fp); fp = NULL; msr_free (&msr);
	      free (rawrec); rawrec = NULL;
	      return NULL;
	    }
	}
      
      if ( last )
	if ( ateof (fp) )
	  *last = 1;
      
      readlen = detsize;
      msr_free (&msr);
      
      if ( msr_unpack (rawrec, readlen, &msr, dataflag, verbose) == NULL )
	{
	  fclose (fp); fp = NULL; msr_free (&msr);
	  free (rawrec); rawrec = NULL;
	  return NULL;
	}
      
      /* Set record length if it was not already done */
      if ( msr->reclen == 0 )
	msr->reclen = readlen;
      
      return msr;
    }
  
  /* Read subsequent records */
  for (;;)
    {
      if ( fpos != NULL )
	*fpos = lmp_ftello (fp);
      
      if ( (myfread (rawrec, 1, readlen, fp)) < readlen )
	{
	  fclose (fp); fp = NULL; msr_free (&msr);
	  free (rawrec); rawrec = NULL;
	  return NULL;
	}
      
      if ( last )
	if ( ateof (fp) )
	  *last = 1;
      
      if ( skipnotdata )
	{
	  if ( MS_ISDATAINDICATOR(*(rawrec+6)) )
	    {
	      break;
	    }
	  else if ( verbose > 1 )
	    {
	      if ( fpos != NULL )
		fprintf (stderr, "Skipped non-data record at byte offset %lld\n", (long long) *fpos);
	      else
		fprintf (stderr, "Skipped non-data record\n");		
	    }
	}
      else
	break;
    }
      
  if ( msr_unpack (rawrec, readlen, &msr, dataflag, verbose) == NULL )
    {
      fclose (fp); fp = NULL; msr_free (&msr);
      free (rawrec); rawrec = NULL;
      return NULL;
    }
  
  /* Set record length if it was not already done */
  if ( msr->reclen == 0 )
    {
      msr->reclen = readlen;
    }
  /* Test that any detected record length is the same as the read length */
  else if ( msr->reclen != readlen )
    {
      fprintf (stderr, "Error: detected record length (%d) != read length (%d)\n",
	       msr->reclen, readlen);
    }
  
  return msr;
}  /* End of ms_readmsr() */


/***************************************************************************
 * ms_readtraces:
 *
 * This routine will open and read all Mini-SEED records in specified
 * file and populate a trace group.  It is not thread safe.  It cannot
 * be used to read more that one file at a time.
 *
 * If reclen is 0 the length of the first record is automatically
 * detected, all subsequent records are then expected to have the same
 * length as the first.
 *
 * If reclen is negative the length of every record is automatically
 * detected.
 *
 * Returns the populated TraceGroup or NULL error.
 ***************************************************************************/
TraceGroup *
ms_readtraces (char *msfile, int reclen, double timetol, double sampratetol,
	       flag skipnotdata, flag dataflag, flag verbose)
{
  MSrecord *msr;
  TraceGroup *mstg;
  
  mstg = mst_initgroup (NULL);
  
  if ( ! mstg )
    return NULL;
  
  /* Loop over the input file */
  while ( (msr = ms_readmsr (msfile, reclen, NULL, NULL, skipnotdata, dataflag, verbose)))
    {
      mst_addmsrtogroup (mstg, msr, timetol, sampratetol);
    }
  
  ms_readmsr (NULL, 0, NULL, NULL, 0, 0, 0);
  
  return mstg;
}  /* End of ms_readtraces() */


/***************************************************************************
 * myfread:
 *
 * A wrapper for fread that handles EOF and error conditions.
 *
 * Returns the return value from fread.
 ***************************************************************************/
static int
myfread (char *buf, int size, int num, FILE *stream)
{
  int read = 0;
  
  read = fread (buf, size, num, stream);
  
  if ( read <= 0 && size && num )
    {
      if ( ferror (stream) )
	fprintf (stderr, "Error reading input file\n");
      
      else if ( ! feof (stream) )
	fprintf (stderr, "Unknown return from fread()\n");
    }
  else if ( read < num && size )
    {
      fprintf (stderr, "Premature end of file, only read %d of %d bytes\n",
	       (size * read), (size * num));
      fprintf (stderr, "Either this is a partial record or the file is not SEED\n");
    }
  
  return read;
}  /* End of myfread() */


/***************************************************************************
 * ateof:
 *
 * Check if stream is at the end-of-file by reading a single character
 * and unreading it if necessary.
 *
 * Returns 1 if stream is at EOF otherwise 0.
 ***************************************************************************/
static int
ateof (FILE *stream)
{
  int c;
  
  c = getc (stream);
  
  if ( c == EOF )
    {
      if ( ferror (stream) )
	fprintf (stderr, "ateof(): Error reading next character from stream\n");
      
      else if ( feof (stream) )
	return 1;
      
      else
	fprintf (stderr, "ateof(): Unknown error reading next character from stream\n");
    }
  else
    {
      if ( ungetc (c, stream) == EOF )
	fprintf (stderr, "ateof(): Error ungetting character\n");
    }
  
  return 0;
}  /* End of ateof() */
