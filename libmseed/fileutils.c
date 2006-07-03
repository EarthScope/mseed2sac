/***************************************************************************
 *
 * Routines to manage files of Mini-SEED.
 *
 * Written by Chad Trabant, ORFEUS/EC-Project MEREDIAN
 *
 * modified: 2006.172
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>

#include "libmseed.h"

/* Byte stream length for read-ahead header fingerprinting */
#define NEXTHDRLEN 48

static int readpackinfo (int chksumlen, int hdrlen, int sizelen, FILE *stream);
static int myfread (char *buf, int size, int num, FILE *stream);
static int ateof (FILE *stream);


/* Check SEED data record header values at known byte offsets to
 * determine if the memory contains a valid record.
 * 
 * Offset = Value
 * [0-5]  = Digits, SEED sequence number
 *     6  = Data record quality indicator
 *     7  = Space or NULL [not valid SEED]
 *     24 = Start hour (0-23)
 *     25 = Start minute (0-59)
 *     26 = Start second (0-60)
 *
 * Usage: MS_ISVALIDHEADER (char *X)
 */
#define MS_ISVALIDHEADER(X) (isdigit ((unsigned char) *(X)) &&              \
			     isdigit ((unsigned char) *(X+1)) &&            \
			     isdigit ((unsigned char) *(X+2)) &&            \
			     isdigit ((unsigned char) *(X+3)) &&            \
			     isdigit ((unsigned char) *(X+4)) &&            \
			     isdigit ((unsigned char) *(X+5)) &&            \
			     MS_ISDATAINDICATOR(*(X+6)) &&                  \
			     (*(X+7) == ' ' || *(X+7) == '\0') &&           \
			     (int)(*(X+24)) >= 0 && (int)(*(X+24)) <= 23 && \
			     (int)(*(X+25)) >= 0 && (int)(*(X+25)) <= 59 && \
			     (int)(*(X+26)) >= 0 && (int)(*(X+26)) <= 60)


/**********************************************************************
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
 * After reading all the records in a file the controlling program
 * should call it one last time with msfile set to NULL.  This will
 * close the file and free allocated memory.
 *
 * Returns MS_NOERROR and populates an MSRecord struct at *ppmsr on
 * successful read, returns MS_ENDOFFILE on EOF, otherwise returns a
 * libmseed error code (listed in libmseed.h) and *ppmsr is set to
 * NULL.
 *********************************************************************/
int
ms_readmsr (MSRecord **ppmsr, char *msfile, int reclen, off_t *fpos,
	    int *last, flag skipnotdata, flag dataflag, flag verbose)
{
  static FILE *fp = NULL;
  static char *rawrec = NULL;
  static char filename[512];
  static int autodet = 1;
  static int readlen = MINRECLEN;
  static int packinfolen = 0;
  static off_t packinfooffset = 0;
  static off_t filepos = 0;
  static int recordcount = 0;
  int packdatasize;
  int autodetexp = 8;
  int prevreadlen;
  int detsize;
  int retcode = MS_NOERROR;
  
  if ( ! ppmsr )
    return MS_GENERROR;
  
  /* When cleanup is requested */
  if ( msfile == NULL )
    {
      msr_free (ppmsr);
      
      if ( fp != NULL )
	fclose (fp);
      
      if ( rawrec != NULL )
	free (rawrec);
      
      fp = NULL;
      rawrec = NULL;
      autodet = 1;
      readlen = MINRECLEN;
      packinfolen = 0;
      packinfooffset = 0;
      filepos = 0;
      recordcount = 0;
      
      return MS_NOERROR;
    }
  
  /* Sanity check: track if we are reading the same file */
  if ( fp && strcmp (msfile, filename) )
    {
      fprintf (stderr, "ms_readmsr() called with a different file name before being reset\n");
      
      /* Close previous file and reset needed variables */
      if ( fp != NULL )
	fclose (fp);
      
      fp = NULL;
      autodet = 1;
      readlen = MINRECLEN;
      packinfolen = 0;
      packinfooffset = 0;
      filepos = 0;
      recordcount = 0;
    }
  
  /* Open the file if needed, redirect to stdin if file is "-" */
  if ( fp == NULL )
    {
      strncpy (filename, msfile, sizeof(filename) - 1);
      filename[sizeof(filename) - 1] = '\0';
      
      if ( strcmp (msfile, "-") == 0 )
	{
	  fp = stdin;
	}
      else if ( (fp = fopen (msfile, "rb")) == NULL )
	{
	  fprintf (stderr, "Error opening file: %s (%s)\n",
		   msfile, strerror (errno));
	  
	  msr_free (ppmsr);
	  
	  return MS_GENERROR;
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
	  
	  /* Read packed file info */
	  if ( packinfolen && filepos == packinfooffset )
	    {
	      if ( (packdatasize = readpackinfo (8, packinfolen, 8, fp)) <= 0 )
		{
		  if ( fp )
		    { fclose (fp); fp = NULL; }
		  msr_free (ppmsr);
		  free (rawrec); rawrec = NULL;
		  
		  if ( packdatasize == 0 )
		    return MS_ENDOFFILE;
		  else
		    return MS_GENERROR;
		}
	      
	      filepos = lmp_ftello (fp);
	      
	      /* File position + data size */
	      packinfooffset = filepos + packdatasize;
	      
	      if ( verbose > 1 )
		fprintf (stderr, "Read packed file info at offset %lld (%d bytes follow)\n",
			 (long long int) (filepos - packinfolen - 8), packdatasize);
	    }
	  
	  /* Read data into record buffer */
	  if ( (myfread (rawrec + prevreadlen, 1, (readlen - prevreadlen), fp)) < (readlen - prevreadlen) )
	    {
	      if ( ! feof (fp) )
		{
		  fprintf (stderr, "Short read at %d bytes during length detection\n", readlen);
		  retcode = MS_GENERROR;
		}
	      else
		{
		  retcode = MS_ENDOFFILE;
		}

	      if ( recordcount == 0 )
		{
		  if ( verbose )
		    fprintf (stderr, "%s: No data records read, not SEED?\n", msfile);
		  retcode = MS_NOTSEED;
		}
	      
	      if ( fp )
		{ fclose (fp); fp = NULL; }
	      msr_free (ppmsr);
	      free (rawrec); rawrec = NULL;
	      
	      return retcode;
	    }
	  
	  filepos = lmp_ftello (fp);
	  
	  /* Determine record length:
	   * If packed file and we are at the next info, length is implied.
	   * Otherwise use ms_find_reclen() */
	  if ( packinfolen && packinfooffset == filepos )
	    {
	      detsize = readlen;
	      break;
	    }
	  else if ( (detsize = ms_find_reclen (rawrec, readlen, fp)) > 0 )
	    {
	      break;
	    }
	  
	  /* Test for packed file signature at the beginning of the file */
	  if ( *rawrec == 'P' && filepos == MINRECLEN && detsize == -1 )
	    {
	      int packtype;
	      
	      packinfolen = 0;
	      packtype = 0;
	      
	      /* Set pack spacer length according to type */
	      if ( ! memcmp ("PED", rawrec, 3) )
		{ packinfolen = 8; packtype = 1; }
	      else if ( ! memcmp ("PSD", rawrec, 3) )
		{ packinfolen = 11; packtype = 2; }
	      else if ( ! memcmp ("PLC", rawrec, 3) )
		{ packinfolen = 13; packtype = 6; }
	      else if ( ! memcmp ("PQI", rawrec, 3) )
		{ packinfolen = 15; packtype = 7; }
	      
	      /* Read first pack info section, compensate for "pack identifier" (10 bytes) */
	      if ( packinfolen )
		{
		  char infostr[30];
		  
		  if ( verbose )
		    fprintf (stderr, "Detected packed file (%3.3s: type %d)\n", rawrec, packtype);
		  
		  /* Assuming data size length is 8 bytes at the end of the pack info */
		  sprintf (infostr, "%8.8s", rawrec + (packinfolen + 10 - 8));
		  sscanf (infostr, " %d", &packdatasize);
		  
		  /* Pack ID + pack info + data size */
		  packinfooffset = 10 + packinfolen + packdatasize;
		  
		  if ( verbose > 1 )
		    fprintf (stderr, "Read packed file info at beginning of file (%d bytes follow)\n",
			     packdatasize);
		}
	    }
	  
	  /* Skip if data record or packed file not detected */
	  if ( detsize == -1 && skipnotdata && ! packinfolen )
	    {
	      if ( verbose > 1 )
		fprintf (stderr, "Skipped non-data record at byte offset %lld\n",
			 (long long) filepos - readlen);
	    }
	  /* Otherwise read more */
	  else
	    {
	      /* Compensate for first packed file info section */
	      if ( filepos == MINRECLEN && packinfolen )
		{
		  /* Shift first data record to beginning of buffer */
		  memmove (rawrec, rawrec + (packinfolen + 10), readlen - (packinfolen + 10));
		  
		  prevreadlen = readlen - (packinfolen + 10);
		}
	      /* Increase read length to the next record size up */
	      else
		{
		  prevreadlen = readlen;
		  autodetexp++;
		  readlen = (unsigned int) 1 << autodetexp;
		}
	    }
	}
      
      if ( detsize <= 0 )
	{
	  fprintf (stderr, "Cannot detect record length at byte offset %lld: %s\n",
		   (long long) filepos - readlen, msfile);
	  
	  if ( fp )
	    { fclose (fp); fp = NULL; }
	  msr_free (ppmsr);
	  free (rawrec); rawrec = NULL;
	  return MS_NOTSEED;
	}
      
      autodet = 0;
      
      if ( verbose > 0 )
	fprintf (stderr, "Detected record length of %d bytes\n", detsize);
      
      if ( detsize < MINRECLEN || detsize > MAXRECLEN )
	{
	  fprintf (stderr, "Detected record length is out of range: %d\n", detsize);
	  
	  if ( fp )
	    { fclose (fp); fp = NULL; }
	  msr_free (ppmsr);
	  free (rawrec); rawrec = NULL;
	  return MS_OUTOFRANGE;
	}
      
      rawrec = (char *) realloc (rawrec, detsize);
      
      /* Read the rest of the first record */
      if ( (detsize - readlen) > 0 )
	{
	  if ( (myfread (rawrec+readlen, 1, detsize-readlen, fp)) < (detsize-readlen) )
	    {
	      if ( ! feof (fp) )
		{
		  fprintf (stderr, "Short read at %d bytes during length detection\n", readlen);
		  retcode = MS_GENERROR;
		}
	      else
		{
		  retcode = MS_ENDOFFILE;
		}

	      if ( recordcount == 0 )
		{
		  if ( verbose )
		    fprintf (stderr, "%s: No data records read, not SEED?\n", msfile);
		  retcode = MS_NOTSEED;
		}
	      
	      if ( fp )
		{ fclose (fp); fp = NULL; }
	      msr_free (ppmsr);
	      free (rawrec); rawrec = NULL;
	      
	      return retcode;
	    }
	  
	  filepos = lmp_ftello (fp);
	}
      
      /* Set file position offset for beginning of record */
      if ( fpos != NULL )
	*fpos = filepos - detsize;

      /* Test if this is the last record */
      if ( last )
	if ( ateof (fp) )
	  *last = 1;
      
      readlen = detsize;
      msr_free (ppmsr);
      
      if ( (retcode = msr_unpack (rawrec, readlen, ppmsr, dataflag, verbose)) != MS_NOERROR )
	{
	  if ( fp )
	    { fclose (fp); fp = NULL; }
	  msr_free (ppmsr);
	  free (rawrec); rawrec = NULL;

	  return retcode;
	}
      
      /* Set record length if it was not already done */
      if ( (*ppmsr)->reclen == 0 )
	(*ppmsr)->reclen = readlen;
      
      recordcount++;
      return MS_NOERROR;
    }
  
  /* Read subsequent records */
  for (;;)
    {
      /* Read packed file info */
      if ( packinfolen && filepos == packinfooffset )
	{
	  if ( (packdatasize = readpackinfo (8, packinfolen, 8, fp)) == 0 )
	    {
	      if ( fp )
		{ fclose (fp); fp = NULL; }
	      msr_free (ppmsr);
	      free (rawrec); rawrec = NULL;
	      
	      if ( packdatasize == 0 )
		return MS_ENDOFFILE;
	      else
		return MS_GENERROR;
	    }
	  
	  filepos = lmp_ftello (fp);
	  
	  /* File position + data size */
	  packinfooffset = filepos + packdatasize;
	  
	  if ( verbose > 1 )
	    fprintf (stderr, "Read packed file info at offset %lld (%d bytes follow)\n",
		     (long long int) (filepos - packinfolen - 8), packdatasize);
	}
      
      /* Read data into record buffer */
      if ( (myfread (rawrec, 1, readlen, fp)) < readlen )
	{
	  if ( ! feof (fp) )
	    {
	      fprintf (stderr, "Short read at %d bytes during length detection\n", readlen);
	      retcode = MS_GENERROR;
	    }
	  else
	    {
	      retcode = MS_ENDOFFILE;
	    }
	  
	  if ( recordcount == 0 )
	    {
	      if ( verbose )
		fprintf (stderr, "%s: No data records read, not SEED?\n", msfile);
	      retcode = MS_NOTSEED;
	    }
	  
	  if ( fp )
	    { fclose (fp); fp = NULL; }
	  msr_free (ppmsr);
	  free (rawrec); rawrec = NULL;
	  
	  return retcode;
	}
      
      filepos = lmp_ftello (fp);
      
      /* Set file position offset for beginning of record */
      if ( fpos != NULL )
	*fpos = filepos - readlen;
      
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
	      fprintf (stderr, "Skipped non-data record at byte offset %lld\n",
		       (long long) filepos - readlen);
	    }
	}
      else
	break;
    }
  
  if ( (retcode = msr_unpack (rawrec, readlen, ppmsr, dataflag, verbose)) != MS_NOERROR )
    {
      if ( fp )
	{ fclose (fp); fp = NULL; }
      msr_free (ppmsr);
      free (rawrec); rawrec = NULL;
      
      return retcode;
    }
  
  /* Set record length if it was not already done */
  if ( (*ppmsr)->reclen == 0 )
    {
      (*ppmsr)->reclen = readlen;
    }
  /* Test that any detected record length is the same as the read length */
  else if ( (*ppmsr)->reclen != readlen )
    {
      fprintf (stderr, "Error: detected record length (%d) != read length (%d)\n",
	       (*ppmsr)->reclen, readlen);

      return MS_WRONGLENGTH;
    }
  
  recordcount++;
  return MS_NOERROR;
}  /* End of ms_readmsr() */


/*********************************************************************
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
 * Returns MS_NOERROR and populates an MSTraceGroup struct at *ppmstg
 * on successful read, returns MS_ENDOFFILE on EOF, otherwise returns
 * a libmseed error code (listed in libmseed.h).
 *********************************************************************/
int
ms_readtraces (MSTraceGroup **ppmstg, char *msfile, int reclen,
	       double timetol, double sampratetol, flag dataquality,
	       flag skipnotdata, flag dataflag, flag verbose)
{
  MSRecord *msr = 0;
  int retcode = MS_NOERROR;
  
  if ( ! ppmstg )
    return MS_GENERROR;
  
  /* Initialize MSTraceGroup if needed */
  if ( ! *ppmstg )
    {
      *ppmstg = mst_initgroup (*ppmstg);
      
      if ( ! *ppmstg )
	return MS_GENERROR;
    }
  
  /* Loop over the input file */
  while ( (retcode = ms_readmsr (&msr, msfile, reclen, NULL, NULL,
				 skipnotdata, dataflag, verbose)) == MS_NOERROR)
    {
      mst_addmsrtogroup (*ppmstg, msr, dataquality, timetol, sampratetol);
    }
  
  ms_readmsr (&msr, NULL, 0, NULL, NULL, 0, 0, 0);
  
  return retcode;
}  /* End of ms_readtraces() */


/********************************************************************
 * ms_find_reclen:
 *
 * Determine SEED data record length with the following steps:
 *
 * 1) determine that the buffer contains a SEED data record by
 * verifying known signatures (fields with known limited values)
 *
 * 2) search the record up to recbuflen bytes for a 1000 blockette.
 *
 * 3) If no blockette 1000 is found and fileptr is not NULL, read the
 * next 48 bytes from the file and determine if it is the fixed second
 * of another record, thereby implying the record length is recbuflen.
 * The original read position of the file is restored.
 *
 * Returns:
 * -1 : data record not detected or error
 *  0 : data record detected but could not determine length
 * >0 : size of the record in bytes
 *********************************************************************/
int
ms_find_reclen ( const char *recbuf, int recbuflen, FILE *fileptr )
{
  uint16_t blkt_offset;    /* Byte offset for next blockette */
  uint8_t swapflag  = 0;   /* Byte swapping flag */
  uint8_t foundlen = 0;    /* Found record length */
  int32_t reclen = -1;     /* Size of record in bytes */
  
  uint16_t blkt_type;
  uint16_t next_blkt;
  
  struct fsdh_s *fsdh;
  struct blkt_1000_s *blkt_1000;
  char nextfsdh[NEXTHDRLEN];
  
  /* Check for valid fixed section of header */
  if ( ! MS_ISVALIDHEADER(recbuf) )
    return -1;
  
  fsdh = (struct fsdh_s *) recbuf;
  
  /* Check to see if byte swapping is needed (bogus year makes good test) */
  if ( (fsdh->start_time.year < 1900) ||
       (fsdh->start_time.year > 2050) )
    swapflag = 1;
  
  blkt_offset = fsdh->blockette_offset;
  
  /* Swap order of blkt_offset if needed */
  if ( swapflag ) gswap2 (&blkt_offset);
  
  /* Loop through blockettes as long as number is non-zero and viable */
  while ((blkt_offset != 0) &&
         (blkt_offset <= recbuflen))
    {
      memcpy (&blkt_type, recbuf + blkt_offset, 2);
      memcpy (&next_blkt, recbuf + blkt_offset + 2, 2);
      
      if ( swapflag )
	{
	  gswap2 (&blkt_type);
	  gswap2 (&next_blkt);
	}
      
      if (blkt_type == 1000)  /* Found the 1000 blockette */
        {
          blkt_1000 = (struct blkt_1000_s *) (recbuf + blkt_offset + 4);
	  
          foundlen = 1;
	  
          /* Calculate record size in bytes as 2^(blkt_1000->reclen) */
	  reclen = (unsigned int) 1 << blkt_1000->reclen;
	  
	  break;
        }
      
      blkt_offset = next_blkt;
    }
  
  if ( reclen == -1 && fileptr )
    {
      /* Read data into record buffer */
      if ( (myfread (nextfsdh, 1, NEXTHDRLEN, fileptr)) < NEXTHDRLEN )
	{
	  /* If no the EOF an error occured (short read) */
	  if ( ! feof (fileptr) )
	    {
	      fprintf (stderr, "ms_find_reclen(): Error reading file\n");
	      return -1;
	    }
	  /* If EOF the record length is recbuflen */
	  else
	    {
	      foundlen = 1;
	      reclen = recbuflen;
	    }
	}
      else
	{
	  /* Rewind file read pointer */
	  if ( lmp_fseeko (fileptr, -NEXTHDRLEN, SEEK_CUR) )
	    {
	      fprintf (stderr, "ms_find_reclen(): %s\n", strerror(errno));
	      return -1;
	    }
	  
	  /* Check for fixed header */
	  if ( MS_ISVALIDHEADER((char *)nextfsdh) )
	    {
	      foundlen = 1;
	      reclen = recbuflen;
	    }
	}
    }
  
  if ( ! foundlen )
    return 0;
  else
    return reclen;
}  /* End of ms_find_reclen() */


/*********************************************************************
 * readpackinfo:
 *
 * Read packed file info: chksum and header, parse and return the size
 * in bytes for the following data records.
 *
 * In general a pack file includes a packed file identifier at the
 * very beginning, followed by pack info for a data block, followed by
 * the data block, followed by a chksum for the data block.  The
 * packinfo, data block and chksum are then repeated for each data
 * block in the file:
 *
 *   ID    INFO     DATA    CHKSUM    INFO     DATA    CHKSUM
 * |----|--------|--....--|--------|--------|--....--|--------| ...
 *
 *      |_________ repeats ________|
 *
 * The INFO section contains fixed width ASCII fields identifying the
 * data in the next section and it's length in bytes.  With this
 * information the offset of the next CHKSUM and INFO are completely
 * predictable.
 *
 * This routine's purpose is to read the CHKSUM and INFO bytes in
 * between the DATA sections and parse the size of the data section
 * from the info section.
 *
 * chksumlen = length in bytes of chksum following data blocks, skipped
 * infolen   = length of the info section
 * sizelen   = length of the size field at the end of the info section
 *
 * Returns the data size of the block that follows, 0 on EOF or -1
 * error.
 *********************************************************************/
static int
readpackinfo (int chksumlen, int infolen, int sizelen, FILE *stream)
{
  char infostr[30];
  int datasize;

  /* Skip CHKSUM section if expected */
  if ( chksumlen )
    if ( lmp_fseeko (stream, chksumlen, SEEK_CUR) )
      {
	return -1;
      }
  
  if ( ateof (stream) )
    return 0;
  
  /* Read INFO section */
  if ( (myfread (infostr, 1, infolen, stream)) < infolen )
    {
      return -1;
    }
  
  sprintf (infostr, "%8.8s", &infostr[infolen - sizelen]);
  sscanf (infostr, " %d", &datasize);
  
  return datasize;
}  /* End of readpackinfo() */


/*********************************************************************
 * myfread:
 *
 * A wrapper for fread that handles EOF and error conditions.
 *
 * Returns the return value from fread.
 *********************************************************************/
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
  
  return read;
}  /* End of myfread() */


/*********************************************************************
 * ateof:
 *
 * Check if stream is at the end-of-file by reading a single character
 * and unreading it if necessary.
 *
 * Returns 1 if stream is at EOF otherwise 0.
 *********************************************************************/
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
