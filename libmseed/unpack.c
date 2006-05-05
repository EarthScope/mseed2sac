/***************************************************************************
 * unpack.c:
 *
 * Generic routines to unpack Mini-SEED records.
 *
 * Appropriate values from the record header will be byte-swapped to
 * the host order.  The purpose of this code is to provide a portable
 * way of accessing common SEED data record header information.  All
 * data structures in SEED 2.4 data records are supported.  The data
 * samples are optionally decompressed/unpacked.
 *
 * Written by Chad Trabant,
 *   ORFEUS/EC-Project MEREDIAN
 *   IRIS Data Management Center
 *
 * modified: 2005.269
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libmseed.h"
#include "unpackdata.h"

/* Function(s) internal to this file */
static int msr_unpack_data (MSrecord * msr, int swapflag, int verbose);
static int check_environment (int verbose);

/* Header and data byte order flags controlled by environment variables */
/* -2 = not checked, -1 = checked but not set, or 0 = LE and 1 = BE */
static flag headerbyteorder = -2;
static flag databyteorder   = -2;

/* Data encoding format/fallback controlled by environment variable */
/* -2 = not checked, -1 = checked but not set, or = encoding */
static int encodingformat   = -2;
static int encodingfallback = -2;

/***************************************************************************
 * msr_unpack:
 *
 * Unpack a SEED data record header/blockettes and populate a MSrecord
 * struct. All approriate fields are byteswapped, if needed, and
 * pointers to structured data are setup in addition to setting the
 * common header fields.
 *
 * If 'dataflag' is true the data samples are unpacked/decompressed
 * and the MSrecord->datasamples pointer is set appropriately.  The
 * data samples will be either 32-bit integers, 32-bit floats or
 * 64-bit floats with the same byte order as the host machine.  The
 * MSrecord->numsamples will be set to the actual number of samples
 * unpacked/decompressed, MSrecord->sampletype will indicated the
 * sample type and MSrecord->unpackerr will be set to indicate any
 * errors encountered during unpacking/decompression (MS_NOERROR if no
 * errors).
 *
 * All appropriate values will be byte-swapped to the host order,
 * including the data samples.
 *
 * All header values, blockette values and data samples will be
 * overwritten by subsequent calls to this function.
 *
 * If the msr struct is NULL it will be allocated.
 * 
 * Returns a pointer to the MSrecord struct populated on success or
 * NULL on error.
 ***************************************************************************/
MSrecord *
msr_unpack ( char *record, int reclen, MSrecord **ppmsr,
	     flag dataflag, flag verbose )
{
  flag headerswapflag = 0;
  flag dataswapflag = 0;
  
  MSrecord *msr = NULL;
  char sequence_number[7];
  
  /* For blockette parsing */
  BlktLink *blkt_link;
  uint16_t blkt_type;
  uint16_t next_blkt;
  uint32_t blkt_offset;
  uint32_t blkt_length;
  
  if ( ! ppmsr )
    {
      fprintf (stderr, "msr_unpack(): ppmsr argument cannot be NULL\n");
      return NULL;
    }
  
  if ( reclen < MINRECLEN || reclen > MAXRECLEN )
    {
      fprintf (stderr, "msr_pack(): record length is out of range: %d\n", reclen);
      return NULL;
    }
  
  /* Initialize the MSrecord */
  msr = *ppmsr;

  if ( ! (msr = msr_init (msr)) )
    {
      *ppmsr = NULL;
      return NULL;
    }
  
  msr->record = record;
  
  msr->drec_indicator = *(record+6);
  
  msr->reclen = reclen;

  /* Check environment variables if necessary */
  if ( headerbyteorder == -2 ||
       databyteorder == -2 ||
       encodingformat == -2 ||
       encodingfallback == -2 )
    if ( check_environment(verbose) )
      return NULL;
  
  /* Verify record indicator, allocate and populate fixed section of header */
  if ( MS_ISDATAINDICATOR(msr->drec_indicator) )
    {
      msr->fsdh = realloc (msr->fsdh, sizeof (struct fsdh_s));
      memcpy (msr->fsdh, record, sizeof (struct fsdh_s));
    }
  else
    {
      fprintf (stderr, "Record header indicator unrecognized: '%c'\n",
	       msr->drec_indicator);
      fprintf (stderr, "This is not a valid Mini-SEED record\n");
      
      msr_free(&msr);
      *ppmsr = NULL;
      return NULL;
    }
  
  /* Check to see if byte swapping is needed by testing the year */
  if ( (msr->fsdh->start_time.year < 1920) ||
       (msr->fsdh->start_time.year > 2020) )
    headerswapflag = dataswapflag = 1;
  
  /* Check if byte order is forced */
  if ( headerbyteorder >= 0 )
    {
      headerswapflag = ( ms_bigendianhost() != headerbyteorder ) ? 1 : 0;
    }
  
  if ( databyteorder >= 0 )
    {
      dataswapflag = ( ms_bigendianhost() != databyteorder ) ? 1 : 0;
    }
  
  if ( verbose > 2 )
    {
      if ( headerswapflag )
	fprintf (stderr, "Byte swapping needed for unpacking of header\n");
      else
	fprintf (stderr, "Byte swapping NOT needed for unpacking of header\n");
    }
  
  /* Swap byte order? */
  if ( headerswapflag )
    {
      SWAPBTIME (&msr->fsdh->start_time);
      gswap2a (&msr->fsdh->numsamples);
      gswap2a (&msr->fsdh->samprate_fact);
      gswap2a (&msr->fsdh->samprate_mult);
      gswap4a (&msr->fsdh->time_correct);
      gswap2a (&msr->fsdh->data_offset);
      gswap2a (&msr->fsdh->blockette_offset);
    }
  
  /* Populate some of the common header fields */
  ms_strncpclean (sequence_number, msr->fsdh->sequence_number, 6);
  msr->sequence_number = (int32_t) strtol (sequence_number, NULL, 10);
  ms_strncpclean (msr->network, msr->fsdh->network, 2);
  ms_strncpclean (msr->station, msr->fsdh->station, 5);
  ms_strncpclean (msr->location, msr->fsdh->location, 2);
  ms_strncpclean (msr->channel, msr->fsdh->channel, 3);
  msr->samplecnt = msr->fsdh->numsamples;

  /* Traverse the blockettes */
  blkt_offset = msr->fsdh->blockette_offset;
  
  while ((blkt_offset != 0) &&
	 (blkt_offset < reclen) &&
	 (blkt_offset < MAXRECLEN))
    {
      /* Every blockette has a similar 4 byte header: type and next */
      memcpy (&blkt_type, record + blkt_offset, 2);
      blkt_offset += 2;
      memcpy (&next_blkt, record + blkt_offset, 2);
      blkt_offset += 2;
      
      if ( headerswapflag )
	{
	  gswap2 (&blkt_type);
	  gswap2 (&next_blkt);
	}
      
      /* Get blockette length */
      blkt_length = get_blktlen (blkt_type,
				 record + blkt_offset - 4,
				 headerswapflag);
      
      if ( blkt_length == 0 )
	{
	  fprintf (stderr, "Unknown blockette length for type %d\n", blkt_type);
	  break;
	}
      
      /* Make sure blockette is contained within the msrecord buffer */
      if ( (blkt_offset - 4 + blkt_length) > reclen )
	{
	  fprintf (stderr, "Blockette %d extends beyond record size, truncated?\n",
		   blkt_type);
	  break;
	}
      
      if ( blkt_type == 100 )
	{			/* Found a Blockette 100 */
	  struct blkt_100_s *blkt_100;
	  
	  blkt_link = msr_addblockette (msr, record + blkt_offset,
					sizeof (struct blkt_100_s),
					blkt_type, 0);
	  if ( ! blkt_link )
	    break;
	  
	  blkt_link->next_blkt = next_blkt;
	  
	  blkt_100 = (struct blkt_100_s *) blkt_link->blktdata;
	  
	  if ( headerswapflag )
	    {
	      gswap4 (&blkt_100->samprate);
	    }
	  
	  msr->samprate = msr->Blkt100->samprate;
	}

      else if ( blkt_type == 200 )
	{			/* Found a Blockette 200 */
	  struct blkt_200_s *blkt_200;
	  
	  blkt_link = msr_addblockette (msr, record + blkt_offset,
					sizeof (struct blkt_200_s),
					blkt_type, 0);
	  if ( ! blkt_link )
	    break;
	  
	  blkt_link->next_blkt = next_blkt;
	  
	  blkt_200 = (struct blkt_200_s *) blkt_link->blktdata;
	  
	  if ( headerswapflag )
	    {
	      gswap4 (&blkt_200->amplitude);
	      gswap4 (&blkt_200->period);
	      gswap4 (&blkt_200->background_estimate);
	      SWAPBTIME (&blkt_200->time);
	    }
	}

      else if ( blkt_type == 201 )
	{			/* Found a Blockette 201 */
	  struct blkt_201_s *blkt_201;
	  
	  blkt_link = msr_addblockette (msr, record + blkt_offset,
					sizeof (struct blkt_201_s),
					blkt_type, 0);
	  if ( ! blkt_link )
	    break;
	  
	  blkt_link->next_blkt = next_blkt;
	  
	  blkt_201 = (struct blkt_201_s *) blkt_link->blktdata;
	  
	  if ( headerswapflag )
	    {
	      gswap4 (&blkt_201->amplitude);
	      gswap4 (&blkt_201->period);
	      gswap4 (&blkt_201->background_estimate);
	      SWAPBTIME (&blkt_201->time);
	    }
	}
      
      else if ( blkt_type == 300 )
	{			/* Found a Blockette 300 */
	  struct blkt_300_s *blkt_300;
	  
	  blkt_link = msr_addblockette (msr, record + blkt_offset,
					sizeof (struct blkt_300_s),
					blkt_type, 0);
	  if ( ! blkt_link )
	    break;
	  
	  blkt_link->next_blkt = next_blkt;
	  
	  blkt_300 = (struct blkt_300_s *) blkt_link->blktdata;
	  
	  if ( headerswapflag )
	    {
	      SWAPBTIME (&blkt_300->time);
	      gswap4 (&blkt_300->step_duration);
	      gswap4 (&blkt_300->interval_duration);
	      gswap4 (&blkt_300->amplitude);
	      gswap4 (&blkt_300->reference_amplitude);
	    }
	}
      
      else if ( blkt_type == 310 )
	{			/* Found a Blockette 310 */
	  struct blkt_310_s *blkt_310;
	  
	  blkt_link = msr_addblockette (msr, record + blkt_offset,
					sizeof (struct blkt_310_s),
					blkt_type, 0);
	  if ( ! blkt_link )
	    break;
	  
	  blkt_link->next_blkt = next_blkt;
	  
	  blkt_310 = (struct blkt_310_s *) blkt_link->blktdata;
	  
	  if ( headerswapflag )
	    {
	      SWAPBTIME (&blkt_310->time);
	      gswap4 (&blkt_310->duration);
	      gswap4 (&blkt_310->period);
	      gswap4 (&blkt_310->amplitude);
	      gswap4 (&blkt_310->reference_amplitude);
	    }
	}
      
      else if ( blkt_type == 320 )
	{			/* Found a Blockette 320 */
	  struct blkt_320_s *blkt_320;
	  
	  blkt_link = msr_addblockette (msr, record + blkt_offset,
					sizeof (struct blkt_320_s),
					blkt_type, 0);
	  if ( ! blkt_link )
	    break;
	  
	  blkt_link->next_blkt = next_blkt;
	  
	  blkt_320 = (struct blkt_320_s *) blkt_link->blktdata;
	  
	  if ( headerswapflag )
	    {
	      SWAPBTIME (&blkt_320->time);
	      gswap4 (&blkt_320->duration);
	      gswap4 (&blkt_320->ptp_amplitude);
	      gswap4 (&blkt_320->reference_amplitude);
	    }
	}

      else if ( blkt_type == 390 )
	{			/* Found a Blockette 390 */
	  struct blkt_390_s *blkt_390;
	  
	  blkt_link = msr_addblockette (msr, record + blkt_offset,
					sizeof (struct blkt_390_s),
					blkt_type, 0);
	  if ( ! blkt_link )
	    break;
	  
	  blkt_link->next_blkt = next_blkt;
	  
	  blkt_390 = (struct blkt_390_s *) blkt_link->blktdata;
	  
	  if ( headerswapflag )
	    {
	      SWAPBTIME (&blkt_390->time);
	      gswap4 (&blkt_390->duration);
	      gswap4 (&blkt_390->amplitude);
	    }
	}

      else if ( blkt_type == 395 )
	{			/* Found a Blockette 395 */
	  struct blkt_395_s *blkt_395;
	  
	  blkt_link = msr_addblockette (msr, record + blkt_offset,
					sizeof (struct blkt_395_s),
					blkt_type, 0);
	  if ( ! blkt_link )
	    break;
	  
	  blkt_link->next_blkt = next_blkt;
	  
	  blkt_395 = (struct blkt_395_s *) blkt_link->blktdata;
	  
	  if ( headerswapflag )
	    {
	      SWAPBTIME (&blkt_395->time);
	    }
	}
      
      else if ( blkt_type == 400 )
	{			/* Found a Blockette 400 */
	  struct blkt_400_s *blkt_400;
	  
	  blkt_link = msr_addblockette (msr, record + blkt_offset,
					sizeof (struct blkt_400_s),
					blkt_type, 0);
	  if ( ! blkt_link )
	    break;
	  
	  blkt_link->next_blkt = next_blkt;
	  
	  blkt_400 = (struct blkt_400_s *) blkt_link->blktdata;
	  
	  if ( headerswapflag )
	    {
	      gswap4 (&blkt_400->azimuth);
	      gswap4 (&blkt_400->slowness);
	      gswap2 (&blkt_400->configuration);
	    }
	}
      
      else if ( blkt_type == 405 )
	{			/* Found a Blockette 405 */
	  struct blkt_405_s *blkt_405;
	  
	  blkt_link = msr_addblockette (msr, record + blkt_offset,
					sizeof (struct blkt_405_s),
					blkt_type, 0);
	  if ( ! blkt_link )
	    break;
	  
	  blkt_link->next_blkt = next_blkt;
	  
	  blkt_405 = (struct blkt_405_s *) blkt_link->blktdata;
	  
	  if ( headerswapflag )
	    {
	      gswap2 (&blkt_405->delay_values);
	    }

	  if ( verbose > 0 )
	    {
	      fprintf (stderr, "msr_unpack(): Blockette 405 cannot be fully supported\n");
	    }
	}
      
      else if ( blkt_type == 500 )
	{			/* Found a Blockette 500 */
	  struct blkt_500_s *blkt_500;
	  
	  blkt_link = msr_addblockette (msr, record + blkt_offset,
					sizeof (struct blkt_500_s),
					blkt_type, 0);
	  if ( ! blkt_link )
	    break;
	  
	  blkt_link->next_blkt = next_blkt;
	  
	  blkt_500 = (struct blkt_500_s *) blkt_link->blktdata;
	  
	  if ( headerswapflag )
	    {
	      gswap4 (&blkt_500->vco_correction);
	      SWAPBTIME (&blkt_500->time);
	      gswap4 (&blkt_500->exception_count);
	    }
	}
      
      else if ( blkt_type == 1000 )
	{			/* Found a Blockette 1000 */
	  struct blkt_1000_s *blkt_1000;
	  
	  blkt_link = msr_addblockette (msr, record + blkt_offset,
					sizeof (struct blkt_1000_s),
					blkt_type, 0);
	  if ( ! blkt_link )
	    break;
	  
	  blkt_link->next_blkt = next_blkt;

	  blkt_1000 = (struct blkt_1000_s *) blkt_link->blktdata;
	  
	  /* Calculate record length in bytes as 2^(blkt_1000->reclen) */
	  msr->reclen = (unsigned int) 1 << blkt_1000->reclen;
	  
	  /* Compare against the specified length */
	  if ( msr->reclen != reclen )
	    {
	      fprintf (stderr, "Record length in Blockette 1000 (%d) != specified length (%d)\n",
		       msr->reclen, reclen);
	      
	      msr->reclen = reclen;
	    }
	  
	  msr->encoding = blkt_1000->encoding;
	  msr->byteorder = blkt_1000->byteorder;
	}
      
      else if ( blkt_type == 1001 )
	{			/* Found a Blockette 1001 */
	  blkt_link = msr_addblockette (msr, record + blkt_offset,
					sizeof (struct blkt_1001_s),
					blkt_type, 0);
	  if ( ! blkt_link )
	    break;
	  
	  blkt_link->next_blkt = next_blkt;
	}
      
      else if ( blkt_type == 2000 )
	{			/* Found a Blockette 2000 */
	  struct blkt_2000_s *blkt_2000;
	  uint16_t b2klen;
	  
	  /* Read the blockette length from blockette */
	  memcpy (&b2klen, record + blkt_offset, 2);
	  if ( headerswapflag ) gswap2 (&b2klen);
	  
	  /* Minus four bytes for the blockette type and next fields */
	  b2klen -= 4;
	  
	  blkt_link = msr_addblockette (msr, record + blkt_offset,
					b2klen, blkt_type, 0);
	  if ( ! blkt_link )
	    break;
	  
	  blkt_link->next_blkt = next_blkt;
	  
	  blkt_2000 = (struct blkt_2000_s *) blkt_link->blktdata;
	  
	  if ( headerswapflag )
	    {
	      gswap2 (&blkt_2000->length);
	      gswap2 (&blkt_2000->data_offset);
	      gswap4 (&blkt_2000->recnum);
	    }
	}
      
      else
	{                      /* Unknown blockette type */
	  if ( blkt_length >= 4 )
	    {
	      blkt_link = msr_addblockette (msr, record + blkt_offset,
					    blkt_length - 4,
					    blkt_type, 0);
	      
	      if ( ! blkt_link )
		break;
	      
	      blkt_link->next_blkt = next_blkt;
	    }
	}
      
      /* Check that the offset increases */
      if ( next_blkt && next_blkt <= blkt_offset )
	{
	  fprintf (stderr, "Offset to next blockette (%d) from type %d did not increase\n",
		   next_blkt, blkt_type);
	  
	  blkt_offset = 0;
	}
      /* Check that the offset is within record length */
      else if ( next_blkt && next_blkt > reclen )
	{
	  fprintf (stderr, "Offset to next blockette (%d) from type %d is beyond record length\n",
		   next_blkt, blkt_type);
	  
	  blkt_offset = 0;
	}
      else
	{
	  blkt_offset = next_blkt;
	}
    }  /* End of while looping through blockettes */
  
  if ( msr->Blkt1000 == 0 )
    {
      msr->unpackerr = MS_NOBLKT1000;
      
      if ( verbose > 0 )
	{
	  fprintf (stderr, "No Blockette 1000 found: %s_%s_%s_%s\n",
		   msr->network, msr->station, msr->location, msr->channel);
	}
    }
  
  /* Populate remaining common header fields */
  msr->starttime = msr_starttime (msr);
  msr->samprate = msr_samprate (msr);
  
  /* Set MSrecord->byteorder if data byte order is forced */
  if ( databyteorder >= 0 )
    {
      msr->byteorder = databyteorder;
    }
  
  /* Check if encoding format is forced */
  if ( encodingformat >= 0 )
    {
      msr->encoding = encodingformat;
    }
  
  /* Use encoding format fallback if defined and no encoding is set,
   * also make sure the byteorder is set by default to big endian */
  if ( encodingfallback >= 0 && msr->encoding == -1 )
    {
      msr->encoding = encodingfallback;
      
      if ( msr->byteorder == -1 )
	{
	  msr->byteorder = 1;
	}
    }
  
  /* Unpack the data samples if requested */
  if ( dataflag && msr->samplecnt > 0 )
    {
      flag dswapflag = headerswapflag;
      flag bigendianhost = ms_bigendianhost();
      
      /* Determine byte order of the data and set the dswapflag as
	 needed; if no Blkt1000 or UNPACK_DATA_BYTEORDER environment
	 variable setting assume the order is the same as the header */
      if ( msr->Blkt1000 != 0 && databyteorder < 0 )
	{
	  dswapflag = 0;
	  
	  if ( bigendianhost && msr->byteorder == 0 )
	    dswapflag = 1;
	  else if ( !bigendianhost && msr->byteorder == 1 )
	    dswapflag = 1;
	}
      else if ( databyteorder >= 0 )
	{
	  dswapflag = dataswapflag;
	}
      
      if ( verbose > 2 && dswapflag )
	fprintf (stderr, "Byte swapping needed for unpacking of data samples\n");
      else if ( verbose > 2 )
	fprintf (stderr, "Byte swapping NOT needed for unpacking of data samples \n");
      
      msr->numsamples = msr_unpack_data (msr, dswapflag, verbose);
    }
  else
    {
      if ( msr->datasamples )
	free (msr->datasamples);
      
      msr->datasamples = 0;
      msr->numsamples = 0;
    }
  
  /* Re-direct the original pointer and return the new */
  *ppmsr = msr;
  return msr;  
} /* End of msr_unpack() */


/************************************************************************
 *  msr_unpack_data:
 *
 *  Unpack Mini-SEED data samples for a given MSrecord.  The packed
 *  data is accessed in the record indicated by MSrecord->record and
 *  the unpacked samples are placed in MSrecord->datasamples.  The
 *  resulting data samples are either 32-bit integers, 32-bit floats
 *  or 64-bit floats in host byte order.
 *
 *  Return number of samples unpacked or -1 on error.
 ************************************************************************/
static int
msr_unpack_data ( MSrecord *msr, int swapflag, int verbose )
{
  int     datasize;             /* byte size of data samples in record 	*/
  int     nsamples;		/* number of samples unpacked		*/
  int     unpacksize;		/* byte size of unpacked samples	*/
  int     samplesize = 0;       /* size of the data samples in bytes    */
  const char *dbuf;
  int32_t    *diffbuff;
  int32_t     x0, xn;
  
  /* Reset the error flag */
  msr->unpackerr = MS_NOERROR;
  
  /* Sanity check the encoding and record length */
  if ( msr->encoding == -1 )
    {
      fprintf (stderr, "msr_unpack_data(): Encoding format unknown\n");
      return -1;
    }
  if ( msr->reclen == -1 )
    {
      fprintf (stderr, "msr_unpack_data(): Record size unknown\n");
      return -1;
    }
  
  switch (msr->encoding)
    {
    case ASCII:
      samplesize = 1;
    case INT16:
    case INT32:
    case FLOAT32:
    case STEIM1:
    case STEIM2:
      samplesize = 4;
    case FLOAT64:
      samplesize = 8;
    }
  
  /* Calculate buffer size needed for unpacked samples */
  unpacksize = msr->samplecnt * samplesize;
  
  /* (Re)Allocate space for the unpacked data */
  msr->datasamples = realloc (msr->datasamples, unpacksize);
  
  if ( msr->datasamples == NULL )
    {
      fprintf (stderr, "msr_unpack_data(): Error (re)allocating memory\n");
      return -1;
    }
  
  datasize = msr->reclen - msr->fsdh->data_offset;
  dbuf = msr->record + msr->fsdh->data_offset;
  
  if ( verbose > 2 )
    fprintf (stderr, "Unpacking %d samples\n", msr->samplecnt);
  
  /* Decide if this is a encoding that we can decode */
  switch (msr->encoding)
    {
      
    case ASCII:
      if ( verbose > 1 )
	fprintf (stderr, "Found ASCII data\n");
      
      nsamples = msr->samplecnt;
      memcpy (msr->datasamples, dbuf, nsamples);
      msr->sampletype = 'a';      
      break;
      
    case INT16:
      if ( verbose > 1 )
	fprintf (stderr, "Unpacking INT-16 data samples\n");
      
      nsamples = msr_unpack_int_16 ((int16_t *)dbuf, msr->samplecnt,
				    msr->samplecnt, msr->datasamples,
				    &msr->unpackerr, swapflag);
      msr->sampletype = 'i';
      break;
      
    case INT32:
      if ( verbose > 1 )
	fprintf (stderr, "Unpacking INT-32 data samples\n");

      nsamples = msr_unpack_int_32 ((int32_t *)dbuf, msr->samplecnt,
				    msr->samplecnt, msr->datasamples,
				    &msr->unpackerr, swapflag);
      msr->sampletype = 'i';
      break;
      
    case FLOAT32:
      if ( verbose > 1 )
	fprintf (stderr, "Unpacking FLOAT-32 data samples\n");
      
      nsamples = msr_unpack_float_32 ((float *)dbuf, msr->samplecnt,
				      msr->samplecnt, msr->datasamples,
				      &msr->unpackerr, swapflag);
      msr->sampletype = 'f';
      break;
      
    case FLOAT64:
      if ( verbose > 1 )
	fprintf (stderr, "Unpacking FLOAT-64 data samples\n");
      
      nsamples = msr_unpack_float_64 ((double *)dbuf, msr->samplecnt,
				      msr->samplecnt, msr->datasamples,
				      &msr->unpackerr, swapflag);
      msr->sampletype = 'd';
      break;
      
    case STEIM1:
      diffbuff = (int32_t *) malloc(unpacksize);
      if ( diffbuff == NULL )
	{
	  fprintf (stderr, "unable to malloc diff buffer in msr_unpack_data()\n");
	  return -1;
	}
      
      if ( verbose > 1 )
	fprintf (stderr, "Unpacking Steim-1 data frames\n");
      
      nsamples = msr_unpack_steim1 ((FRAME *)dbuf, datasize, msr->samplecnt,
				    msr->samplecnt, msr->datasamples, diffbuff, 
				    &x0, &xn, &msr->unpackerr, swapflag, verbose);
      msr->sampletype = 'i';
      free (diffbuff);
      break;
      
    case STEIM2:
      diffbuff = (int32_t *) malloc(unpacksize);
      if ( diffbuff == NULL )
	{
	  fprintf (stderr, "unable to malloc diff buffer in msr_unpack_data()\n");
	  return -1;
	}
      
      if ( verbose > 1 )
	fprintf (stderr, "Unpacking Steim-2 data frames\n");

      nsamples = msr_unpack_steim2 ((FRAME *)dbuf, datasize, msr->samplecnt,
				    msr->samplecnt, msr->datasamples, diffbuff,
				    &x0, &xn, &msr->unpackerr, swapflag, verbose);
      msr->sampletype = 'i';
      free (diffbuff);
      break;
      
    default:
      fprintf (stderr, "Unable to unpack encoding format %d for %s_%s_%s_%s\n",
	       msr->encoding,
	       msr->network, msr->station,
	       msr->location, msr->channel);
      
      msr->unpackerr = MS_UNKNOWNFORMAT;
      return -1;
    }
  
  return nsamples;
} /* End of msr_unpack_data() */


/************************************************************************
 *  check_environment:
 *
 *  Check environment variables and set global variables approriately.
 *  
 *  Return 0 on success and -1 on error.
 ************************************************************************/
static int
check_environment (int verbose)
{
  char *envvariable;

  /* Read possible environmental variables that force byteorder */
  if ( headerbyteorder == -2 )
    {
      if ( (envvariable = getenv("UNPACK_HEADER_BYTEORDER")) )
	{
	  if ( *envvariable != '0' && *envvariable != '1' )
	    {
	      fprintf (stderr, "Environment variable UNPACK_HEADER_BYTEORDER must be set to '0' or '1'\n");
	      return -1;
	    }
	  else if ( *envvariable == '0' )
	    {
	      headerbyteorder = 0;
	      if ( verbose > 2 )
		fprintf (stderr, "UNPACK_HEADER_BYTEORDER=0, unpacking little-endian header\n");
	    }
	  else
	    {
	      headerbyteorder = 1;
	      if ( verbose > 2 )
		fprintf (stderr, "UNPACK_HEADER_BYTEORDER=1, unpacking big-endian header\n");
	    }
	}
      else
	{
	  headerbyteorder = -1;
	}
    }

  if ( databyteorder == -2 )
    {
      if ( (envvariable = getenv("UNPACK_DATA_BYTEORDER")) )
	{
	  if ( *envvariable != '0' && *envvariable != '1' )
	    {
	      fprintf (stderr, "Environment variable UNPACK_DATA_BYTEORDER must be set to '0' or '1'\n");
	      return -1;
	    }
	  else if ( *envvariable == '0' )
	    {
	      databyteorder = 0;
	      if ( verbose > 2 )
		fprintf (stderr, "UNPACK_DATA_BYTEORDER=0, unpacking little-endian data samples\n");
	    }
	  else
	    {
	      databyteorder = 1;
	      if ( verbose > 2 )
		fprintf (stderr, "UNPACK_DATA_BYTEORDER=1, unpacking big-endian data samples\n");
	    }
	}
      else
	{
	  databyteorder = -1;
	}
    }
  
  /* Read possible environmental variable that forces encoding format */
  if ( encodingformat == -2 )
    {
      if ( (envvariable = getenv("UNPACK_DATA_FORMAT")) )
	{
	  encodingformat = (int) strtol (envvariable, NULL, 10);
	  
	  if ( encodingformat < 0 || encodingformat > 33 )
	    {
	      fprintf (stderr, "Environment variable UNPACK_DATA_FORMAT set to invalid value: '%d'\n", encodingformat);
	      return -1;
	    }
	  else if ( verbose > 2 )
	    fprintf (stderr, "UNPACK_DATA_FORMAT, unpacking data in encoding format %d\n", encodingformat);
	}
      else
	{
	  encodingformat = -1;
	}
    }
  
  /* Read possible environmental variable to be used as a fallback encoding format */
  if ( encodingfallback == -2 )
    {
      if ( (envvariable = getenv("UNPACK_DATA_FORMAT_FALLBACK")) )
	{
	  encodingfallback = (int) strtol (envvariable, NULL, 10);
	  
	  if ( encodingfallback < 0 || encodingfallback > 33 )
	    {
	      fprintf (stderr, "Environment variable UNPACK_DATA_FORMAT_FALLBACK set to invalid value: '%d'\n", encodingfallback);
	      return -1;
	    }
	  else if ( verbose > 2 )
	    fprintf (stderr, "UNPACK_DATA_FORMAT_FALLBACK, unpacking data in encoding format %d\n", encodingfallback);
	}
      else
	{
	  encodingfallback = 10;  /* Default fallback is Steim-1 encoding */
	}
    }
  
  return 0;
} /* End of check_environment() */
