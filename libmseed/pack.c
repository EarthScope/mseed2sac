/***************************************************************************
 * pack.c:
 *
 * Generic routines to pack Mini-SEED records using an MSrecord as a
 * header template and data source.
 *
 * Written by Chad Trabant,
 *   IRIS Data Management Center
 *
 * modified: 2006.124
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libmseed.h"
#include "packdata.h"

/* Function(s) internal to this file */
static int msr_pack_header_raw (MSRecord *msr, char *rawrec, int maxheaderlen,
				flag swapflag, flag verbose);
static int msr_update_header (MSRecord * msr, char *rawrec, flag swapflag,
			      flag verbose);
static int msr_pack_data (void *dest, void *src,
			  int maxsamples, int maxdatabytes, int *packsamples,
			  char sampletype, flag encoding, flag swapflag,
			  flag verbose);

/* Header and data byte order flags controlled by environment variables */
/* -2 = not checked, -1 = checked but not set, or 0 = LE and 1 = BE */
static flag headerbyteorder = -2;
static flag databyteorder = -2;


/***************************************************************************
 * msr_pack:
 *
 * Pack data into SEED data records.  Using the record header values
 * in the MSRecord as a template the common header fields are packed
 * into the record header, blockettes in the blockettes chain are
 * packed and data samples are packed in the encoding format indicated
 * by the MSRecord->encoding field.  A Blockette 1000 will be added if
 * one is not present.
 *
 * The MSRecord->datasamples array and MSRecord->numsamples value will
 * not be changed by this routine.  It is the responsibility of the
 * calling routine to adjust the data buffer if desired.
 *
 * As each record is filled and finished they are passed to
 * record_handler along with their length in bytes.  It is the
 * responsibility of record_handler to process the record, the memory
 * will be re-used or freed when record_handler returns.
 *
 * If the flush flag != 0 all of the data will be packed into data
 * records even though the last one will probably not be filled.
 *
 * Default values are: data record & quality indicator = 'D', record
 * length = 4096, encoding = 11 (Steim2) and byteorder = 1 (MSBF).
 * The defaults are triggered when the the msr->dataquality is 0 or
 * msr->reclen, msr->encoding and msr->byteorder are -1 respectively.
 *
 * Returns the number of records created on success and -1 on error.
 ***************************************************************************/
int
msr_pack ( MSRecord * msr, void (*record_handler) (char *, int),
	   int *packedsamples, flag flush, flag verbose )
{
  uint16_t *HPnumsamples;
  uint16_t *HPdataoffset;
  char *rawrec;
  char *envvariable;
  
  flag headerswapflag = 0;
  flag dataswapflag = 0;
  flag packret;
  
  int samplesize;
  int headerlen;
  int dataoffset;
  int maxdatabytes;
  int maxsamples;
  int recordcnt = 0;
  int totalpackedsamples;
  int packsamples, packoffset;
  
  if ( ! msr )
    return -1;
  
  if ( ! record_handler )
    {
      fprintf (stderr, "msr_pack(): record_handler() function pointer not set!\n");
      return -1;
    }
  
  /* Read possible environmental variables that force byteorder */
  if ( headerbyteorder == -2 )
    {
      if ( (envvariable = getenv("PACK_HEADER_BYTEORDER")) )
	{
	  if ( *envvariable != '0' && *envvariable != '1' )
	    {
	      fprintf (stderr, "Environment variable PACK_HEADER_BYTEORDER must be set to '0' or '1'\n");
	      return -1;
	    }
	  else if ( *envvariable == '0' )
	    {
	      headerbyteorder = 0;
	      if ( verbose > 2 )
		fprintf (stderr, "PACK_HEADER_BYTEORDER=0, packing little-endian header\n");
	    }
	  else
	    {
	      headerbyteorder = 1;
	      if ( verbose > 2 )
		fprintf (stderr, "PACK_HEADER_BYTEORDER=1, packing big-endian header\n");
	    }
	}
      else
	{
	  headerbyteorder = -1;
	}
    }
  if ( databyteorder == -2 )
    {
      if ( (envvariable = getenv("PACK_DATA_BYTEORDER")) )
	{
	  if ( *envvariable != '0' && *envvariable != '1' )
	    {
	      fprintf (stderr, "Environment variable PACK_DATA_BYTEORDER must be set to '0' or '1'\n");
	      return -1;
	    }
	  else if ( *envvariable == '0' )
	    {
	      databyteorder = 0;
	      if ( verbose > 2 )
		fprintf (stderr, "PACK_DATA_BYTEORDER=0, packing little-endian data samples\n");
	    }
	  else
	    {
	      databyteorder = 1;
	      if ( verbose > 2 )
		fprintf (stderr, "PACK_DATA_BYTEORDER=1, packing big-endian data samples\n");
	    }
	}
      else
	{
	  databyteorder = -1;
	}
    }

  /* Set default indicator, record length, byte order and encoding if needed */
  if ( msr->dataquality == 0 ) msr->dataquality = 'D';
  if ( msr->reclen == -1 ) msr->reclen = 4096;
  if ( msr->byteorder == -1 )  msr->byteorder = 1;
  if ( msr->encoding == -1 ) msr->encoding = STEIM2;
  
  /* Cleanup/reset sequence number */
  if ( msr->sequence_number <= 0 || msr->sequence_number > 999999)
    msr->sequence_number = 1;
  
  if ( msr->reclen < MINRECLEN || msr->reclen > MAXRECLEN )
    {
      fprintf (stderr, "msr_pack(): Record length is out of range: %d\n",
	       msr->reclen);
      return -1;
    }
  
  if ( msr->numsamples <= 0 )
    {
      fprintf (stderr, "msr_pack(): No samples to pack\n");
      return -1;
    }
  
  samplesize = get_samplesize (msr->sampletype);
  
  if ( ! samplesize )
    {
      fprintf (stderr, "msr_pack(): Unknown sample type '%c'\n",
	       msr->sampletype);
      return -1;
    }
  
  /* Sanity check for msr/quality indicator */
  if ( ! MS_ISDATAINDICATOR(msr->dataquality) )
    {
      fprintf (stderr, "Record header & quality indicator unrecognized: '%c'\n",
	       msr->dataquality);
      fprintf (stderr, "Packing failed.\n");
      return -1;
    }
  
  /* Allocate space for data record */
  rawrec = (char *) malloc (msr->reclen);
  
  if ( rawrec == NULL )
    {
      fprintf (stderr, "msr_pack(): Error allocating memory\n");
      return -1;
    }
  
  /* Set header pointers to known offsets into FSDH */
  HPnumsamples = (uint16_t *) (rawrec + 30);
  HPdataoffset = (uint16_t *) (rawrec + 44);
  
  /* Check to see if byte swapping is needed */
  if ( msr->byteorder != ms_bigendianhost() )
    headerswapflag = dataswapflag = 1;
  
  /* Check if byte order is forced */
  if ( headerbyteorder >= 0 )
    {
      headerswapflag = ( msr->byteorder != headerbyteorder ) ? 1 : 0;
    }
  
  if ( databyteorder >= 0 )
    {
      dataswapflag = ( msr->byteorder != databyteorder ) ? 1 : 0;
    }
  
  if ( verbose > 2 )
    {
      if ( headerswapflag && dataswapflag )
	fprintf (stderr, "Byte swapping needed for packing of header and data samples\n");
      else if ( headerswapflag )
	fprintf (stderr, "Byte swapping needed for packing of header\n");
      else if ( dataswapflag )
	fprintf (stderr, "Byte swapping needed for packing of data samples\n");
      else
	fprintf (stderr, "Byte swapping NOT needed for packing\n");
    }

  /* Add a blank 1000 Blockette if one is not present, the blockette values
     will be populated in msr_pack_header_raw() */
  if ( ! msr->Blkt1000 )
    {
      struct blkt_1000_s blkt1000;
      memset (&blkt1000, 0, sizeof (struct blkt_1000_s));
      
      if ( verbose > 2 )
	fprintf (stderr, "Adding 1000 Blockette\n");
      
      if ( ! msr_addblockette (msr, (char *) &blkt1000, sizeof(struct blkt_1000_s), 1000, 0) )
	{
	  fprintf (stderr, "msr_pack(): Error adding 1000 Blockette\n");
	  return -1;
	}
    }
  
  headerlen = msr_pack_header_raw (msr, rawrec, msr->reclen, headerswapflag, verbose);
  
  if ( headerlen == -1 )
    {
      fprintf (stderr, "msr_pack(): Error packing header\n");
      return -1;
    }

  /* Determine offset to encoded data */
  if ( msr->encoding == STEIM1 || msr->encoding == STEIM2 )
    {
      dataoffset = 64;
      while ( dataoffset < headerlen )
	dataoffset += 64;
      
      /* Zero memory between blockettes and data if any */
      memset (rawrec + headerlen, 0, dataoffset - headerlen);
    }
  else
    {
      dataoffset = headerlen;
    }
  
  *HPdataoffset = (uint16_t) dataoffset;
  if ( headerswapflag ) gswap2 (HPdataoffset);
  
  /* Determine the max data bytes and sample count */
  maxdatabytes = msr->reclen - dataoffset;
  
  if ( msr->encoding == STEIM1 )
    {
      maxsamples = (int) (maxdatabytes/64) * STEIM1_FRAME_MAX_SAMPLES;
    }
  else if ( msr->encoding == STEIM2 )
    {
      maxsamples = (int) (maxdatabytes/64) * STEIM2_FRAME_MAX_SAMPLES;
    }
  else
    {
      maxsamples = maxdatabytes / samplesize;
    }
  
  /* Pack samples into records */
  *HPnumsamples = 0;
  totalpackedsamples = 0;
  if ( packedsamples ) *packedsamples = 0;
  packoffset = 0;
  
  while ( (msr->numsamples - totalpackedsamples) > maxsamples || flush )
    {
      packret = msr_pack_data (rawrec + dataoffset,
			       (char *) msr->datasamples + packoffset,
			       (msr->numsamples - totalpackedsamples), maxdatabytes,
			       &packsamples, msr->sampletype,
			       msr->encoding, dataswapflag, verbose);
      
      if ( packret )
	{
	  fprintf (stderr, "msr_pack(): Error packing record\n");
	  return -1;
	}
      
      packoffset += packsamples * samplesize;
      
      /* Update number of samples */
      *HPnumsamples = (uint16_t) packsamples;
      if ( headerswapflag ) gswap2 (HPnumsamples);
      
      if ( verbose > 0 )
	fprintf (stderr, "Packed %d samples for %s_%s_%s_%s\n", packsamples,
		 msr->network, msr->station, msr->location, msr->channel);
      
      /* Send record to handler */
      record_handler (rawrec, msr->reclen);
      
      totalpackedsamples += packsamples;
      if ( packedsamples ) *packedsamples = totalpackedsamples;
      
      /* Update record header for next record */
      msr->sequence_number = ( msr->sequence_number >= 999999) ? 1 : msr->sequence_number + 1;
      msr->starttime += (double) packsamples / msr->samprate * HPTMODULUS;
      msr_update_header (msr, rawrec, headerswapflag, verbose);

      recordcnt++;
      
      if ( totalpackedsamples >= msr->numsamples )
	break;
    }

  if ( verbose > 2 )
    fprintf (stderr, "Packed %d total samples for %s_%s_%s_%s\n", totalpackedsamples,
	     msr->network, msr->station, msr->location, msr->channel);

  free (rawrec);
  
  return recordcnt;
} /* End of msr_pack() */


/***************************************************************************
 * msr_pack_header:
 *
 * Pack data header/blockettes into the SEED record at
 * MSRecord->record.  Unlike msr_pack no default values are applied,
 * the header structures are expected to be self describing and no
 * Blockette 1000 will be added.  This routine is only useful for
 * re-packing a record header.
 *
 * Returns the header length in bytes on success and -1 on error.
 ***************************************************************************/
int
msr_pack_header ( MSRecord *msr, flag verbose )
{
  char *envvariable;
  flag headerswapflag = 0;
  int headerlen;
  int maxheaderlen;
  
  if ( ! msr )
    return -1;
  
  /* Read possible environmental variables that force byteorder */
  if ( headerbyteorder == -2 )
    {
      if ( (envvariable = getenv("PACK_HEADER_BYTEORDER")) )
	{
	  if ( *envvariable != '0' && *envvariable != '1' )
	    {
	      fprintf (stderr, "Environment variable PACK_HEADER_BYTEORDER must be set to '0' or '1'\n");
	      return -1;
	    }
	  else if ( *envvariable == '0' )
	    {
	      headerbyteorder = 0;
	      if ( verbose > 2 )
		fprintf (stderr, "PACK_HEADER_BYTEORDER=0, packing little-endian header\n");
	    }
	  else
	    {
	      headerbyteorder = 1;
	      if ( verbose > 2 )
		fprintf (stderr, "PACK_HEADER_BYTEORDER=1, packing big-endian header\n");
	    }
	}
      else
	{
	  headerbyteorder = -1;
	}
    }

  if ( msr->reclen < MINRECLEN || msr->reclen > MAXRECLEN )
    {
      fprintf (stderr, "msr_pack_header(): record length is out of range: %d\n",
	       msr->reclen);
      return -1;
    }
  
  if ( msr->byteorder != 0 && msr->byteorder != 1 )
    {
      fprintf (stderr, "msr_pack_header(): byte order is not defined correctly: %d\n",
	       msr->byteorder);
      return -1;
    }
    
  if ( msr->fsdh )
    {
      maxheaderlen = (msr->fsdh->data_offset > 0) ?
	msr->fsdh->data_offset :
	msr->reclen;
    }
  else
    {
      maxheaderlen = msr->reclen;
    }
    
  /* Check to see if byte swapping is needed */
  if ( msr->byteorder != ms_bigendianhost() )
    headerswapflag = 1;
  
  /* Check if byte order is forced */
  if ( headerbyteorder >= 0 )
    {
      headerswapflag = ( msr->byteorder != headerbyteorder ) ? 1: 0;
    }
  
  if ( verbose > 2 )
    {
      if ( headerswapflag )
	fprintf (stderr, "Byte swapping needed for packing of header\n");
      else
	fprintf (stderr, "Byte swapping NOT needed for packing of header\n");
    }
  
  headerlen = msr_pack_header_raw (msr, msr->record, maxheaderlen,
				   headerswapflag, verbose);
  
  return headerlen;
}


/***************************************************************************
 * msr_pack_header_raw:
 *
 * Pack data header/blockettes into the specified SEED data record.
 *
 * Returns the header length in bytes on success or -1 on error.
 ***************************************************************************/
static int
msr_pack_header_raw ( MSRecord *msr, char *rawrec, int maxheaderlen,
		      flag swapflag, flag verbose )
{
  struct blkt_link_s *cur_blkt;
  char seqnum[7];
  int16_t offset;
  int blktcnt = 0;
  int nextoffset;
  int reclenexp = 0;
  int reclenfind;
  
  struct fsdh_s *fsdh;
  
  if ( ! msr || ! rawrec )
    return -1;
  
  if ( verbose > 2 )
    fprintf (stderr, "Packing fixed section of data header\n");
  
  if ( maxheaderlen > msr->reclen )
    {
      fprintf (stderr, "msr_pack_header_raw(): maxheaderlen of %d is beyond record length of %d\n",
	       maxheaderlen, msr->reclen);
      return -1;
    }
  
  if ( maxheaderlen < 48 )
    {
      fprintf (stderr, "msr_pack_header_raw(): maxheaderlen of %d is too small\n",
	       maxheaderlen);
      return -1;
    }
  
  fsdh = (struct fsdh_s *) rawrec;
  offset = 48;
  
  /* Roll-over sequence number if necessary */
  if ( msr->sequence_number > 999999 )
    msr->sequence_number = 1;
  
  /* Use any existing msr->fsdh fixed section as a base template */
  if ( msr->fsdh )
    memcpy (fsdh, msr->fsdh, sizeof(struct fsdh_s));
  else
    memset (fsdh, 0, sizeof(struct fsdh_s));
  
  /* Pack values into the fixed section of header */
  snprintf (seqnum, 7, "%06d", msr->sequence_number);
  memcpy (fsdh->sequence_number, seqnum, 6);
  fsdh->dataquality = msr->dataquality;
  fsdh->reserved = ' ';
  ms_strncpopen (fsdh->network, msr->network, 2);
  ms_strncpopen (fsdh->station, msr->station, 5);
  ms_strncpopen (fsdh->location, msr->location, 2);
  ms_strncpopen (fsdh->channel, msr->channel, 3);
  ms_hptime2btime (msr->starttime, &(fsdh->start_time));
  ms_genfactmult (msr->samprate, &(fsdh->samprate_fact), &(fsdh->samprate_mult));
  
  if ( msr->blkts )
    fsdh->blockette_offset = offset;
  else
    fsdh->blockette_offset = 0;
  
  /* Swap byte order? */
  if ( swapflag )
    {
      SWAPBTIME (&fsdh->start_time);
      gswap2 (&fsdh->samprate_fact);
      gswap2 (&fsdh->samprate_mult);
      gswap4 (&fsdh->time_correct);
      gswap2 (&fsdh->blockette_offset);
    }
  
  /* Traverse blockette chain and pack blockettes at 'offset' */
  cur_blkt = msr->blkts;
  
  while ( cur_blkt && offset < maxheaderlen )
    {
      /* Check that the blockette fits */
      if ( (offset + 4 + cur_blkt->blktdatalen) > maxheaderlen )
	{
	  fprintf (stderr, "msr_pack_header_raw(): header exceeds maxheaderlen of %d\n",
		   maxheaderlen);
	  break;
	}
      
      /* Pack blockette type and leave space for next offset */
      memcpy (rawrec + offset, &cur_blkt->blkt_type, 2);
      if ( swapflag ) gswap2 (rawrec + offset);
      nextoffset = offset + 2;
      offset += 4;
      
      if ( cur_blkt->blkt_type == 100 )
	{
	  struct blkt_100_s *blkt_100 = (struct blkt_100_s *) (rawrec + offset);
	  memcpy (blkt_100, cur_blkt->blktdata, sizeof (struct blkt_100_s));
	  offset += sizeof (struct blkt_100_s);
	  
	  blkt_100->samprate = msr->samprate;

	  if ( swapflag )
	    {
	      gswap4 (&blkt_100->samprate);
	    }
	}
      
      else if ( cur_blkt->blkt_type == 200 )
	{
	  struct blkt_200_s *blkt_200 = (struct blkt_200_s *) (rawrec + offset);
	  memcpy (blkt_200, cur_blkt->blktdata, sizeof (struct blkt_200_s));
	  offset += sizeof (struct blkt_200_s);
	  
	  if ( swapflag )
	    {
	      gswap4 (&blkt_200->amplitude);
	      gswap4 (&blkt_200->period);
	      gswap4 (&blkt_200->background_estimate);
	      SWAPBTIME (&blkt_200->time);
	    }
	}
      
      else if ( cur_blkt->blkt_type == 201 )
	{
	  struct blkt_201_s *blkt_201 = (struct blkt_201_s *) (rawrec + offset);
	  memcpy (blkt_201, cur_blkt->blktdata, sizeof (struct blkt_201_s));
	  offset += sizeof (struct blkt_201_s);
	  
	  if ( swapflag )
	    {
	      gswap4 (&blkt_201->amplitude);
	      gswap4 (&blkt_201->period);
	      gswap4 (&blkt_201->background_estimate);
	      SWAPBTIME (&blkt_201->time);
	    }
	}

      else if ( cur_blkt->blkt_type == 300 )
	{
	  struct blkt_300_s *blkt_300 = (struct blkt_300_s *) (rawrec + offset);
	  memcpy (blkt_300, cur_blkt->blktdata, sizeof (struct blkt_300_s));
	  offset += sizeof (struct blkt_300_s);
	  
	  if ( swapflag )
	    {
	      SWAPBTIME (&blkt_300->time);
	      gswap4 (&blkt_300->step_duration);
	      gswap4 (&blkt_300->interval_duration);
	      gswap4 (&blkt_300->amplitude);
	      gswap4 (&blkt_300->reference_amplitude);
	    }
	}

      else if ( cur_blkt->blkt_type == 310 )
	{
	  struct blkt_310_s *blkt_310 = (struct blkt_310_s *) (rawrec + offset);
	  memcpy (blkt_310, cur_blkt->blktdata, sizeof (struct blkt_310_s));
	  offset += sizeof (struct blkt_310_s);
	  
	  if ( swapflag )
	    {
	      SWAPBTIME (&blkt_310->time);
	      gswap4 (&blkt_310->duration);
	      gswap4 (&blkt_310->period);
	      gswap4 (&blkt_310->amplitude);
	      gswap4 (&blkt_310->reference_amplitude);
	    }
	}
      
      else if ( cur_blkt->blkt_type == 320 )
	{
	  struct blkt_320_s *blkt_320 = (struct blkt_320_s *) (rawrec + offset);
	  memcpy (blkt_320, cur_blkt->blktdata, sizeof (struct blkt_320_s));
	  offset += sizeof (struct blkt_320_s);
	  
	  if ( swapflag )
	    {
	      SWAPBTIME (&blkt_320->time);
	      gswap4 (&blkt_320->duration);
	      gswap4 (&blkt_320->ptp_amplitude);
	      gswap4 (&blkt_320->reference_amplitude);
	    }
	}

      else if ( cur_blkt->blkt_type == 390 )
	{
	  struct blkt_390_s *blkt_390 = (struct blkt_390_s *) (rawrec + offset);
	  memcpy (blkt_390, cur_blkt->blktdata, sizeof (struct blkt_390_s));
	  offset += sizeof (struct blkt_390_s);
	  
	  if ( swapflag )
	    {
	      SWAPBTIME (&blkt_390->time);
	      gswap4 (&blkt_390->duration);
	      gswap4 (&blkt_390->amplitude);
	    }
	}
      
      else if ( cur_blkt->blkt_type == 395 )
	{
	  struct blkt_395_s *blkt_395 = (struct blkt_395_s *) (rawrec + offset);
	  memcpy (blkt_395, cur_blkt->blktdata, sizeof (struct blkt_395_s));
	  offset += sizeof (struct blkt_395_s);
	  
	  if ( swapflag )
	    {
	      SWAPBTIME (&blkt_395->time);
	    }
	}

      else if ( cur_blkt->blkt_type == 400 )
	{
	  struct blkt_400_s *blkt_400 = (struct blkt_400_s *) (rawrec + offset);
	  memcpy (blkt_400, cur_blkt->blktdata, sizeof (struct blkt_400_s));
	  offset += sizeof (struct blkt_400_s);
	  
	  if ( swapflag )
	    {
	      gswap4 (&blkt_400->azimuth);
	      gswap4 (&blkt_400->slowness);
	      gswap2 (&blkt_400->configuration);
	    }
	}

      else if ( cur_blkt->blkt_type == 405 )
	{
	  struct blkt_405_s *blkt_405 = (struct blkt_405_s *) (rawrec + offset);
	  memcpy (blkt_405, cur_blkt->blktdata, sizeof (struct blkt_405_s));
	  offset += sizeof (struct blkt_405_s);
	  
	  if ( swapflag )
	    {
	      gswap2 (&blkt_405->delay_values);
	    }

	  if ( verbose > 0 )
	    {
	      fprintf (stderr, "msr_pack(): Blockette 405 cannot be fully supported\n");
	    }
	}

      else if ( cur_blkt->blkt_type == 500 )
	{
	  struct blkt_500_s *blkt_500 = (struct blkt_500_s *) (rawrec + offset);
	  memcpy (blkt_500, cur_blkt->blktdata, sizeof (struct blkt_500_s));
	  offset += sizeof (struct blkt_500_s);
	  
	  if ( swapflag )
	    {
	      gswap4 (&blkt_500->vco_correction);
	      SWAPBTIME (&blkt_500->time);
	      gswap4 (&blkt_500->exception_count);
	    }
	}
      
      else if ( cur_blkt->blkt_type == 1000 )
	{
	  struct blkt_1000_s *blkt_1000 = (struct blkt_1000_s *) (rawrec + offset);
	  memcpy (blkt_1000, cur_blkt->blktdata, sizeof (struct blkt_1000_s));
	  offset += sizeof (struct blkt_1000_s);
	  
	  if ( databyteorder >= 0 )
	    blkt_1000->byteorder = databyteorder;
	  else
	    blkt_1000->byteorder = msr->byteorder;
	  
	  blkt_1000->encoding = msr->encoding;

	  /* Calculate the record length as an exponent of 2 */
	  for (reclenfind=1, reclenexp=1; reclenfind <= MAXRECLEN; reclenexp++)
	    {
	      reclenfind *= 2;
	      if ( reclenfind == msr->reclen ) break;
	    }
	  
	  if ( reclenfind != msr->reclen )
	    {
	      fprintf (stderr, "msr_pack_header_raw(): Record length %d is not a power of 2\n",
		       msr->reclen);
	      return -1;
	    }
	  
	  blkt_1000->reclen = reclenexp;
	}
      
      else if ( cur_blkt->blkt_type == 1001 )
	{
	  hptime_t sec, usec;
	  struct blkt_1001_s *blkt_1001 = (struct blkt_1001_s *) (rawrec + offset);
	  memcpy (blkt_1001, cur_blkt->blktdata, sizeof (struct blkt_1001_s));
	  offset += sizeof (struct blkt_1001_s);
	  
	  /* Insert microseconds offset */
	  sec = msr->starttime / (HPTMODULUS / 10000);
	  usec = msr->starttime - (sec * (HPTMODULUS / 10000));
	  usec /= (HPTMODULUS / 1000000);
	  
	  blkt_1001->usec = (int8_t) usec;
	}

      else if ( cur_blkt->blkt_type == 2000 )
	{
	  struct blkt_2000_s *blkt_2000 = (struct blkt_2000_s *) (rawrec + offset);
	  memcpy (blkt_2000, cur_blkt->blktdata, cur_blkt->blktdatalen);
	  offset += cur_blkt->blktdatalen;
	  
	  if ( swapflag )
	    {
	      gswap2 (&blkt_2000->length);
	      gswap2 (&blkt_2000->data_offset);
	      gswap4 (&blkt_2000->recnum);
	    }
	  
	  /* Nothing done to pack the opaque headers and data, they should already
	     be packed into the blockette payload */
	}
      
      else
	{
	  memcpy (rawrec + offset, cur_blkt->blktdata, cur_blkt->blktdatalen);
	  offset += cur_blkt->blktdatalen;
	}
      
      /* Pack the offset to the next blockette */
      if ( cur_blkt->next )
	{
	  memcpy (rawrec + nextoffset, &offset, 2);
	  if ( swapflag ) gswap2 (rawrec + nextoffset);
	}
      else
	{
	  memset (rawrec + nextoffset, 0, 2);
	}
      
      blktcnt++;
      cur_blkt = cur_blkt->next;
    }
  
  fsdh->numblockettes = blktcnt;
  
  if ( verbose > 2 )
    fprintf (stderr, "Packed %d blockettes\n", blktcnt);
  
  return offset;
}


/***************************************************************************
 * msr_update_header:
 *
 * Update the header values that change between records: start time,
 * sequence number, etc.
 *
 * Returns 0 on success or -1 on error.
 ***************************************************************************/
static int
msr_update_header ( MSRecord *msr, char *rawrec, flag swapflag,
		    flag verbose )
{
  struct fsdh_s *fsdh;
  char seqnum[7];
  
  if ( ! msr || ! rawrec )
    return -1;
  
  if ( verbose > 2 )
    fprintf (stderr, "Updating fixed section of data header\n");
  
  fsdh = (struct fsdh_s *) rawrec;
  
  /* Pack values into the fixed section of header */
  snprintf (seqnum, 7, "%06d", msr->sequence_number);
  memcpy (fsdh->sequence_number, seqnum, 6);

  ms_hptime2btime (msr->starttime, &(fsdh->start_time));
  
  /* Swap byte order? */
  if ( swapflag )
    {
      SWAPBTIME (&fsdh->start_time);
    }
  
  return 0;
}


/************************************************************************
 *  msr_pack_data:
 *
 *  Pack Mini-SEED data samples.  The input data samples specified as
 *  'src' will be packed with 'encoding' format and placed in 'dest'.
 *  The number of samples packed will be placed in 'packsamples' and
 *  the number of bytes packed will be placed in 'packbytes'.
 *
 *  Return 0 on success and a negative number on error.
 ************************************************************************/
static int
msr_pack_data (void *dest, void *src,
	       int maxsamples, int maxdatabytes, int *packsamples,
	       char sampletype, flag encoding, flag swapflag, flag verbose)
{
  int retval;
  int nframes;
  int npacked;
  int32_t *intbuff;
  int32_t *diffbuff;
  
  /* Decide if this is a format that we can decode */
  switch (encoding)
    {
      
    case ASCII:
      if ( sampletype != 'a' )
	{
	  fprintf (stderr, "Sample type must be ascii (a) for ASCII encoding not '%c'\n",
		   sampletype);
	  return -1;
	}
      
      if ( verbose > 1 )
	fprintf (stderr, "Packing ASCII data\n");
      
      retval = msr_pack_text (dest, src, maxsamples, maxdatabytes, 1,
			      &npacked, packsamples);
      
      break;
      
    case INT16:
      if ( sampletype != 'i' )
	{
	  fprintf (stderr, "Sample type must be integer (i) for integer-16 encoding not '%c'\n",
		   sampletype);
	  return -1;
	}
      
      if ( verbose > 1 )
	fprintf (stderr, "Packing INT-16 data samples\n");
      
      retval = msr_pack_int_16 (dest, src, maxsamples, maxdatabytes, 1,
				&npacked, packsamples, swapflag);
      
      break;
      
    case INT32:
      if ( sampletype != 'i' )
	{
	  fprintf (stderr, "Sample type must be integer (i) for integer-32 encoding not '%c'\n",
		   sampletype);
	  return -1;
	}

      if ( verbose > 1 )
	fprintf (stderr, "Packing INT-32 data samples\n");
      
      retval = msr_pack_int_32 (dest, src, maxsamples, maxdatabytes, 1,
				&npacked, packsamples, swapflag);
      
      break;
      
    case FLOAT32:
      if ( sampletype != 'f' )
	{
	  fprintf (stderr, "Sample type must be float (f) for float-32 encoding not '%c'\n",
		   sampletype);
	  return -1;
	}

      if ( verbose > 1 )
	fprintf (stderr, "Packing FLOAT-32 data samples\n");
      
      retval = msr_pack_float_32 (dest, src, maxsamples, maxdatabytes, 1,
				  &npacked, packsamples, swapflag);

      break;
      
    case FLOAT64:
      if ( sampletype != 'd' )
	{
	  fprintf (stderr, "Sample type must be double (d) for float-64 encoding not '%c'\n",
		   sampletype);
	  return -1;
	}

      if ( verbose > 1 )
	fprintf (stderr, "Packing FLOAT-64 data samples\n");
            
      retval = msr_pack_float_64 (dest, src, maxsamples, maxdatabytes, 1,
				  &npacked, packsamples, swapflag);

      break;
      
    case STEIM1:
      if ( sampletype != 'i' )
	{
	  fprintf (stderr, "Sample type must be integer (i) for Steim-1 compression not '%c'\n",
		   sampletype);
	  return -1;
	}
      
      intbuff = (int32_t *) src;
      
      /* Allocate and populate the difference buffer */
      diffbuff = (int32_t *) malloc (maxsamples * sizeof(int32_t));
      if ( diffbuff == NULL )
	{
	  fprintf (stderr, "msr_pack_data(): Unable to malloc diff buffer\n");
	  return -1;
	}
      
      diffbuff[0] = 0;
      for (npacked=1; npacked < maxsamples; npacked++)
	diffbuff[npacked] = intbuff[npacked] - intbuff[npacked-1];
      
      if ( verbose > 1 )
	fprintf (stderr, "Packing Steim-1 data frames\n");
      
      nframes = maxdatabytes / 64;
      
      retval = msr_pack_steim1 (dest, src, diffbuff, maxsamples, nframes, 1,
				&npacked, packsamples, swapflag);
      
      free (diffbuff);
      break;
      
    case STEIM2:
      if ( sampletype != 'i' )
	{
	  fprintf (stderr, "Sample type must be integer (i) for Steim-2 compression not '%c'\n",
		   sampletype);
	  return -1;
	}
      
      intbuff = (int32_t *) src;

      /* Allocate and populate the difference buffer */
      diffbuff = (int32_t *) malloc (maxsamples * sizeof(int32_t));
      if ( diffbuff == NULL )
	{
	  fprintf (stderr, "msr_pack_data(): Unable to malloc diff buffer\n");
	  return -1;
	}
      
      diffbuff[0] = 0;
      for (npacked=1; npacked < maxsamples; npacked++)
	diffbuff[npacked] = intbuff[npacked] - intbuff[npacked-1];
      
      if ( verbose > 1 )
	fprintf (stderr, "Packing Steim-2 data frames\n");
      
      nframes = maxdatabytes / 64;
      
      retval = msr_pack_steim2 (dest, src, diffbuff, maxsamples, nframes, 1,
				&npacked, packsamples, swapflag);
      
      free (diffbuff);
      break;
      
    default:
      fprintf (stderr, "Unable to pack format %d\n", encoding);
      
      return -1;
    }
    
  return retval;
} /* End of msr_pack_data() */
