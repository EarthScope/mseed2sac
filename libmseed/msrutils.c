/***************************************************************************
 * msrutils.c:
 *
 * Generic routines to operate on Mini-SEED records.
 *
 * Written by Chad Trabant, ORFEUS/EC-Project MEREDIAN
 *
 * modified: 2006.122
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libmseed.h"

/* A simple bitwise AND test to return 0 or 1 */
#define bit(x,y) (x&y)?1:0


/***************************************************************************
 * msr_init:
 *
 * Initialize and return an MSRecord struct, allocating memory if
 * needed.  If memory for the fsdh and datasamples fields has been
 * allocated the pointers will be retained for reuse.  If a blockette
 * chain is present all associated memory will be released.
 *
 * Returns a pointer to a MSRecord struct on success or NULL on error.
 ***************************************************************************/
MSRecord *
msr_init ( MSRecord *msr )
{
  void *fsdh = 0;
  void *datasamples = 0;
  
  if ( ! msr )
    {
      msr = (MSRecord *) malloc (sizeof(MSRecord));
    }
  else
    {
      fsdh = msr->fsdh;
      datasamples = msr->datasamples;
      
      if ( msr->blkts )
        msr_free_blktchain (msr);
    }
  
  if ( msr == NULL )
    {
      fprintf (stderr, "msr_init(): error allocating memory\n");
      return NULL;
    }
  
  memset (msr, 0, sizeof (MSRecord));
  
  msr->fsdh = fsdh;
  msr->datasamples = datasamples;
  
  msr->reclen = -1;
  msr->samplecnt = -1;
  msr->byteorder = -1;
  msr->encoding = -1;
  msr->unpackerr = MS_NOERROR;
  
  return msr;
} /* End of msr_init() */


/***************************************************************************
 * msr_free:
 *
 * Free all memory associated with a MSRecord struct.
 ***************************************************************************/
void
msr_free ( MSRecord **ppmsr )
{
  if ( ppmsr != NULL && *ppmsr != 0 )
    {      
      /* Free fixed section header if populated */
      if ( (*ppmsr)->fsdh )
        free ((*ppmsr)->fsdh);
      
      /* Free blockette chain if populated */
      if ( (*ppmsr)->blkts )
        msr_free_blktchain (*ppmsr);
      
      /* Free datasamples if present */
      if ( (*ppmsr)->datasamples )
	free ((*ppmsr)->datasamples);
      
      free (*ppmsr);
      
      *ppmsr = NULL;
    }
} /* End of msr_free() */


/***************************************************************************
 * msr_free_blktchain:
 *
 * Free all memory associated with a blockette chain in a MSRecord
 * struct and set MSRecord->blkts to NULL.  Also reset the shortcut
 * blockette pointers.
 ***************************************************************************/
void
msr_free_blktchain ( MSRecord *msr )
{
  if ( msr )
    {
      if ( msr->blkts )
        {
          BlktLink *bc = msr->blkts;
          BlktLink *nb = NULL;
          
          while ( bc )
	    {
	      nb = bc->next;
	      
	      if ( bc->blktdata )
		free (bc->blktdata);
	      
	      free (bc);
	      
	      bc = nb;
	    }
          
          msr->blkts = 0;
        }

      msr->Blkt100  = 0;
      msr->Blkt1000 = 0;
      msr->Blkt1001 = 0;      
    }
} /* End of msr_free_blktchain() */


/***************************************************************************
 * msr_addblockette:
 *
 * Add a blockette to the blockette chain of an MSRecord.  'blktdata'
 * should be the body of the blockette type 'blkttype' of 'length'
 * bytes without the blockette header (type and next offsets).  The
 * 'chainpos' value controls which end of the chain the blockette is
 * added to.  If 'chainpos' is 0 the blockette will be added to the
 * end of the chain (last blockette), other wise it will be added to
 * the beginning of the chain (first blockette).
 *
 * Returns a pointer to the BlktLink added to the chain on success and
 * NULL on error.
 ***************************************************************************/
BlktLink *
msr_addblockette (MSRecord *msr, char *blktdata, int length, int blkttype,
		  int chainpos)
{
  BlktLink *blkt;
  
  if ( ! msr )
    return NULL;
  
  blkt = msr->blkts;
  
  if ( blkt )
    {
      if ( chainpos != 0 )
	{
	  blkt = (BlktLink *) malloc (sizeof(BlktLink));
	  
	  blkt->next = msr->blkts;
	  msr->blkts = blkt;
	}
      else
	{
	  /* Find the last blockette */
	  while ( blkt && blkt->next )
	    {
	      blkt = blkt->next;
	    }
	  
	  blkt->next = (BlktLink *) malloc (sizeof(BlktLink));
	  
	  blkt = blkt->next;
	  blkt->next = 0;
	}
      
      if ( blkt == NULL )
	{
	  fprintf (stderr, "msr_addblockette(): Error allocating memory\n");
	  return NULL;
	}
    }
  else
    {
      msr->blkts = (BlktLink *) malloc (sizeof(BlktLink));
      
      if ( msr->blkts == NULL )
	{
	  fprintf (stderr, "msr_addblockette(): Error allocating memory\n");
	  return NULL;
	}
      
      blkt = msr->blkts;
      blkt->next = 0;
    }
  
  blkt->blkt_type = blkttype;
  blkt->next_blkt = 0;
  
  blkt->blktdata = (char *) malloc (length);
  
  if ( blkt->blktdata == NULL )
    {
      fprintf (stderr, "msr_addblockette(): Error allocating memory\n");
      return NULL;
    }
  
  memcpy (blkt->blktdata, blktdata, length);
  blkt->blktdatalen = length;
  
  /* Setup the shortcut pointer for common blockettes */
  switch ( blkttype )
    {
    case 100:
      msr->Blkt100 = blkt->blktdata;
      break;
    case 1000:
      msr->Blkt1000 = blkt->blktdata;
      break;
    case 1001:
      msr->Blkt1001 = blkt->blktdata;
      break;
    }
  
  return blkt;
} /* End of msr_addblockette() */


/***************************************************************************
 * msr_samprate:
 *
 * Calculate and return a double precision sample rate for the
 * specified MSRecord.  If a Blockette 100 was included and parsed,
 * the "Actual sample rate" (field 3) will be returned, otherwise a
 * nominal sample rate will be calculated from the sample rate factor
 * and multiplier in the fixed section data header.
 *
 * Returns the positive sample rate on success and -1.0 on error.
 ***************************************************************************/
double
msr_samprate (MSRecord *msr)
{
  if ( ! msr )
    return -1.0;
  
  if ( msr->Blkt100 )
    return (double) msr->Blkt100->samprate;
  else
    return msr_nomsamprate (msr);  
} /* End of msr_samprate() */


/***************************************************************************
 * msr_nomsamprate:
 *
 * Calculate a double precision nominal sample rate from the sample
 * rate factor and multiplier in the FSDH struct of the specified
 * MSRecord.
 *
 * Returns the positive sample rate on success and -1.0 on error.
 ***************************************************************************/
double
msr_nomsamprate (MSRecord *msr)
{
  double samprate = 0.0;
  int factor;
  int multiplier;
  
  if ( ! msr )
    return -1.0;
  
  /* Calculate the nominal sample rate */
  factor = msr->fsdh->samprate_fact;
  multiplier = msr->fsdh->samprate_mult;
  
  if ( factor > 0 )
    samprate = (double) factor;
  else if ( factor < 0 )
    samprate = -1.0 / (double) factor;
  if ( multiplier > 0 )
    samprate = samprate * (double) multiplier;
  else if ( multiplier < 0 )
    samprate = -1.0 * (samprate / (double) multiplier);
  
  return samprate;
} /* End of msr_nomsamprate() */


/***************************************************************************
 * msr_starttime:
 *
 * Convert a btime struct of a FSDH struct of a MSRecord (the record
 * start time) into a high precision epoch time and apply time
 * corrections if any are specified in the header and bit 1 of the
 * activity flags indicates that it has not already been applied.  If
 * a Blockette 1001 is included and has been parsed the microseconds
 * of field 4 are also applied.
 *
 * Returns a high precision epoch time on success and HPTERROR on
 * error.
 ***************************************************************************/
hptime_t
msr_starttime (MSRecord *msr)
{
  double starttime = msr_starttime_uc (msr);
  
  if ( ! msr || starttime == HPTERROR )
    return HPTERROR;
  
  /* Check if a correction is included and if it has been applied,
     bit 1 of activity flags indicates if it has been appiled */
  
  if ( msr->fsdh->time_correct != 0 &&
       ! (msr->fsdh->act_flags & 0x02) )
    {
      starttime += (hptime_t) msr->fsdh->time_correct * (HPTMODULUS / 10000);
    }
  
  /* Apply microsecond precision in a parsed Blockette 1001 */
  if ( msr->Blkt1001 )
    {
      starttime += (hptime_t) msr->Blkt1001->usec * (HPTMODULUS / 1000000);
    }
  
  return starttime;
} /* End of msr_starttime() */


/***************************************************************************
 * msr_starttime_uc:
 *
 * Convert a btime struct of a FSDH struct of a MSRecord (the record
 * start time) into a high precision epoch time.  This time has no
 * correction(s) applied to it.
 *
 * Returns a high precision epoch time on success and HPTERROR on
 * error.
 ***************************************************************************/
hptime_t
msr_starttime_uc (MSRecord *msr)
{
  if ( ! msr )
    return HPTERROR;

  if ( ! msr->fsdh )
    return HPTERROR;
  
  return ms_btime2hptime (&msr->fsdh->start_time);
} /* End of msr_starttime_uc() */


/***************************************************************************
 * msr_endtime:
 *
 * Calculate the time of the last sample in the record; this is the
 * actual last sample time and *not* the time "covered" by the last
 * sample.
 *
 * Returns the time of the last sample as a high precision epoch time
 * on success and HPTERROR on error.
 ***************************************************************************/
hptime_t
msr_endtime (MSRecord *msr)
{
  hptime_t span = 0;
  
  if ( ! msr )
    return HPTERROR;

  if ( msr->samprate > 0.0 && msr->samplecnt > 0 )
    span = ((double) (msr->samplecnt - 1) / msr->samprate * HPTMODULUS) + 0.5;
  
  return (msr->starttime + span);
} /* End of msr_endtime() */


/***************************************************************************
 * msr_srcname:
 *
 * Generate a source name string for a specified MSRecord in the
 * format: 'NET_STA_LOC_CHAN'.  The passed srcname must have enough
 * room for the resulting string.
 *
 * Returns a pointer to the resulting string or NULL on error.
 ***************************************************************************/
char *
msr_srcname (MSRecord *msr, char *srcname)
{
  if ( msr == NULL )
    return NULL;
  
  /* Build the source name string */
  sprintf (srcname, "%s_%s_%s_%s",
	   msr->network, msr->station,
	   msr->location, msr->channel);
  
  return srcname;
} /* End of msr_srcname() */


/***************************************************************************
 * msr_print:
 *
 * Prints header values in an MSRecord struct, if 'details' is greater
 * than 0 then detailed information about each blockette is printed.
 * If 'details' is greater than 1 very detailed information is
 * printed.  If no FSDH (msr->fsdh) is present only a single line with
 * basic information is printed.
 ***************************************************************************/
void
msr_print (MSRecord *msr, flag details)
{
  double nomsamprate;
  char srcname[50];
  char time[25];
  char b;
  int idx;
  
  if ( ! msr )
    return;
  
  /* Generate a source name string */
  srcname[0] = '\0';
  msr_srcname (msr, srcname);
  
  /* Generate a start time string */
  ms_hptime2seedtimestr (msr->starttime, time);
  
  /* Report information in the fixed header */
  if ( details > 0 && msr->fsdh )
    {
      nomsamprate = msr_nomsamprate (msr);
      
      printf ("%s, %06d, %c\n", srcname, msr->sequence_number, msr->dataquality);
      printf ("             start time: %s\n", time);
      printf ("      number of samples: %d\n", msr->fsdh->numsamples);
      printf ("     sample rate factor: %d  (%.10g samples per second)\n",
	      msr->fsdh->samprate_fact, nomsamprate);
      printf (" sample rate multiplier: %d\n", msr->fsdh->samprate_mult);
      
      if ( details > 1 )
	{
	  /* Activity flags */
	  b = msr->fsdh->act_flags;
	  printf ("         activity flags: [%u%u%u%u%u%u%u%u] 8 bits\n",
		  bit(b,0x01), bit(b,0x02), bit(b,0x04), bit(b,0x08),
		  bit(b,0x10), bit(b,0x20), bit(b,0x40), bit(b,0x80));
	  if ( b & 0x01 ) printf ("                         [Bit 0] Calibration signals present\n");
	  if ( b & 0x02 ) printf ("                         [Bit 1] Time correction applied\n");
	  if ( b & 0x04 ) printf ("                         [Bit 2] Beginning of an event, station trigger\n");
	  if ( b & 0x08 ) printf ("                         [Bit 3] End of an event, station detrigger\n");
	  if ( b & 0x10 ) printf ("                         [Bit 4] A positive leap second happened in this record\n");
	  if ( b & 0x20 ) printf ("                         [Bit 5] A negative leap second happened in this record\n");
	  if ( b & 0x40 ) printf ("                         [Bit 6] Event in progress\n");
	  if ( b & 0x80 ) printf ("                         [Bit 7] Undefined bit set\n");

	  /* I/O and clock flags */
	  b = msr->fsdh->io_flags;
	  printf ("    I/O and clock flags: [%u%u%u%u%u%u%u%u] 8 bits\n",
		  bit(b,0x01), bit(b,0x02), bit(b,0x04), bit(b,0x08),
		  bit(b,0x10), bit(b,0x20), bit(b,0x40), bit(b,0x80));
	  if ( b & 0x01 ) printf ("                         [Bit 0] Station volume parity error possibly present\n");
	  if ( b & 0x02 ) printf ("                         [Bit 1] Long record read (possibly no problem)\n");
	  if ( b & 0x04 ) printf ("                         [Bit 2] Short record read (record padded)\n");
	  if ( b & 0x08 ) printf ("                         [Bit 3] Start of time series\n");
	  if ( b & 0x10 ) printf ("                         [Bit 4] End of time series\n");
	  if ( b & 0x20 ) printf ("                         [Bit 5] Clock locked\n");
	  if ( b & 0x40 ) printf ("                         [Bit 6] Undefined bit set\n");
	  if ( b & 0x80 ) printf ("                         [Bit 7] Undefined bit set\n");

	  /* Data quality flags */
	  b = msr->fsdh->dq_flags;
	  printf ("     data quality flags: [%u%u%u%u%u%u%u%u] 8 bits\n",
		  bit(b,0x01), bit(b,0x02), bit(b,0x04), bit(b,0x08),
		  bit(b,0x10), bit(b,0x20), bit(b,0x40), bit(b,0x80));
	  if ( b & 0x01 ) printf ("                         [Bit 0] Amplifier saturation detected\n");
	  if ( b & 0x02 ) printf ("                         [Bit 1] Digitizer clipping detected\n");
	  if ( b & 0x04 ) printf ("                         [Bit 2] Spikes detected\n");
	  if ( b & 0x08 ) printf ("                         [Bit 3] Glitches detected\n");
	  if ( b & 0x10 ) printf ("                         [Bit 4] Missing/padded data present\n");
	  if ( b & 0x20 ) printf ("                         [Bit 5] Telemetry synchronization error\n");
	  if ( b & 0x40 ) printf ("                         [Bit 6] A digital filter may be charging\n");
	  if ( b & 0x80 ) printf ("                         [Bit 7] Time tag is questionable\n");
	}

      printf ("   number of blockettes: %d\n", msr->fsdh->numblockettes);
      printf ("        time correction: %ld\n", (long int) msr->fsdh->time_correct);
      printf ("            data offset: %d\n", msr->fsdh->data_offset);
      printf (" first blockette offset: %d\n", msr->fsdh->blockette_offset);
    }
  else
    {
      printf ("%s, %06d, %c, %d, %d samples, %-.10g Hz, %s\n",
	      srcname, msr->sequence_number, msr->dataquality,
	      msr->reclen, msr->samplecnt, msr->samprate, time);
    }

  /* Report information in the blockette chain */
  if ( details > 0 && msr->blkts )
    {
      BlktLink *cur_blkt = msr->blkts;
      
      while ( cur_blkt )
	{
	  if ( cur_blkt->blkt_type == 100 )
	    {
	      struct blkt_100_s *blkt_100 = (struct blkt_100_s *) cur_blkt->blktdata;
	      
	      printf ("          BLOCKETTE %u: (%s)\n", cur_blkt->blkt_type,
		      get_blktdesc(cur_blkt->blkt_type));
	      printf ("              next blockette: %u\n", cur_blkt->next_blkt);
	      printf ("          actual sample rate: %.10g\n", blkt_100->samprate);
	      
	      if ( details > 1 )
		{
		  b = blkt_100->flags;
		  printf ("             undefined flags: [%u%u%u%u%u%u%u%u] 8 bits\n",
			  bit(b,0x01), bit(b,0x02), bit(b,0x04), bit(b,0x08),
			  bit(b,0x10), bit(b,0x20), bit(b,0x40), bit(b,0x80));
		  
		  printf ("          reserved bytes (3): %u,%u,%u\n",
			  blkt_100->reserved[0], blkt_100->reserved[1], blkt_100->reserved[2]);
		}
	    }

	  else if ( cur_blkt->blkt_type == 200 )
	    {
	      struct blkt_200_s *blkt_200 = (struct blkt_200_s *) cur_blkt->blktdata;
	      
	      printf ("          BLOCKETTE %u: (%s)\n", cur_blkt->blkt_type,
		      get_blktdesc(cur_blkt->blkt_type));
	      printf ("              next blockette: %u\n", cur_blkt->next_blkt);
	      printf ("            signal amplitude: %g\n", blkt_200->amplitude);
	      printf ("               signal period: %g\n", blkt_200->period);
	      printf ("         background estimate: %g\n", blkt_200->background_estimate);
	      
	      if ( details > 1 )
		{
		  b = blkt_200->flags;
		  printf ("       event detection flags: [%u%u%u%u%u%u%u%u] 8 bits\n",
			  bit(b,0x01), bit(b,0x02), bit(b,0x04), bit(b,0x08),
			  bit(b,0x10), bit(b,0x20), bit(b,0x40), bit(b,0x80));
		  if ( b & 0x01 ) printf ("                         [Bit 0] 1: Dilatation wave\n");
		  else            printf ("                         [Bit 0] 0: Compression wave\n");
		  if ( b & 0x02 ) printf ("                         [Bit 1] 1: Units after deconvolution\n");
		  else            printf ("                         [Bit 1] 0: Units are digital counts\n");
		  if ( b & 0x04 ) printf ("                         [Bit 2] Bit 0 is undetermined\n");
		  printf ("               reserved byte: %u\n", blkt_200->reserved);
		}
	      
	      ms_btime2seedtimestr (&blkt_200->time, time);
	      printf ("           signal onset time: %s\n", time);
	      printf ("               detector name: %.24s\n", blkt_200->detector);
	    }

	  else if ( cur_blkt->blkt_type == 201 )
	    {
	      struct blkt_201_s *blkt_201 = (struct blkt_201_s *) cur_blkt->blktdata;
	      
	      printf ("          BLOCKETTE %u: (%s)\n", cur_blkt->blkt_type,
		      get_blktdesc(cur_blkt->blkt_type));
	      printf ("              next blockette: %u\n", cur_blkt->next_blkt);
	      printf ("            signal amplitude: %g\n", blkt_201->amplitude);
	      printf ("               signal period: %g\n", blkt_201->period);
	      printf ("         background estimate: %g\n", blkt_201->background_estimate);
	      
	      b = blkt_201->flags;
	      printf ("       event detection flags: [%u%u%u%u%u%u%u%u] 8 bits\n",
		      bit(b,0x01), bit(b,0x02), bit(b,0x04), bit(b,0x08),
		      bit(b,0x10), bit(b,0x20), bit(b,0x40), bit(b,0x80));
	      if ( b & 0x01 ) printf ("                         [Bit 0] 1: Dilation wave\n");
	      else            printf ("                         [Bit 0] 0: Compression wave\n");

	      if ( details > 1 )
		printf ("               reserved byte: %u\n", blkt_201->reserved);	      
	      ms_btime2seedtimestr (&blkt_201->time, time);
	      printf ("           signal onset time: %s\n", time);
	      printf ("                  SNR values: ");
	      for (idx=0; idx < 6; idx++) printf ("%u  ", blkt_201->snr_values[idx]);
	      printf ("\n");
	      printf ("              loopback value: %u\n", blkt_201->loopback);
	      printf ("              pick algorithm: %u\n", blkt_201->pick_algorithm);
	      printf ("               detector name: %.24s\n", blkt_201->detector);
	    }

	  else if ( cur_blkt->blkt_type == 300 )
	    {
	      struct blkt_300_s *blkt_300 = (struct blkt_300_s *) cur_blkt->blktdata;
	      
	      printf ("          BLOCKETTE %u: (%s)\n", cur_blkt->blkt_type,
		      get_blktdesc(cur_blkt->blkt_type));
	      printf ("              next blockette: %u\n", cur_blkt->next_blkt);
	      ms_btime2seedtimestr (&blkt_300->time, time);
	      printf ("      calibration start time: %s\n", time);
	      printf ("      number of calibrations: %u\n", blkt_300->numcalibrations);
	      
	      b = blkt_300->flags;
	      printf ("           calibration flags: [%u%u%u%u%u%u%u%u] 8 bits\n",
		      bit(b,0x01), bit(b,0x02), bit(b,0x04), bit(b,0x08),
		      bit(b,0x10), bit(b,0x20), bit(b,0x40), bit(b,0x80));
	      if ( b & 0x01 ) printf ("                         [Bit 0] First pulse is positive\n");
	      if ( b & 0x02 ) printf ("                         [Bit 1] Calibration's alternate sign\n");
	      if ( b & 0x04 ) printf ("                         [Bit 2] Calibration was automatic\n");
	      if ( b & 0x08 ) printf ("                         [Bit 3] Calibration continued from previous record(s)\n");
	      
	      printf ("               step duration: %u\n", blkt_300->step_duration);
	      printf ("           interval duration: %u\n", blkt_300->interval_duration);
	      printf ("            signal amplitude: %g\n", blkt_300->amplitude);
	      printf ("        input signal channel: %.3s", blkt_300->input_channel);
	      if ( details > 1 )
		printf ("               reserved byte: %u\n", blkt_300->reserved);
	      printf ("         reference amplitude: %u\n", blkt_300->reference_amplitude);
	      printf ("                    coupling: %.12s\n", blkt_300->coupling);
	      printf ("                     rolloff: %.12s\n", blkt_300->rolloff);
	    }
	  
	  else if ( cur_blkt->blkt_type == 310 )
	    {
	      struct blkt_310_s *blkt_310 = (struct blkt_310_s *) cur_blkt->blktdata;
	      
	      printf ("          BLOCKETTE %u: (%s)\n", cur_blkt->blkt_type,
		      get_blktdesc(cur_blkt->blkt_type));
	      printf ("              next blockette: %u\n", cur_blkt->next_blkt);
	      ms_btime2seedtimestr (&blkt_310->time, time);
	      printf ("      calibration start time: %s\n", time);
	      if ( details > 1 )
		printf ("               reserved byte: %u\n", blkt_310->reserved1);
	      
	      b = blkt_310->flags;
	      printf ("           calibration flags: [%u%u%u%u%u%u%u%u] 8 bits\n",
		      bit(b,0x01), bit(b,0x02), bit(b,0x04), bit(b,0x08),
		      bit(b,0x10), bit(b,0x20), bit(b,0x40), bit(b,0x80));
	      if ( b & 0x04 ) printf ("                         [Bit 2] Calibration was automatic\n");
	      if ( b & 0x08 ) printf ("                         [Bit 3] Calibration continued from previous record(s)\n");
	      if ( b & 0x10 ) printf ("                         [Bit 4] Peak-to-peak amplitude\n");
	      if ( b & 0x20 ) printf ("                         [Bit 5] Zero-to-peak amplitude\n");
	      if ( b & 0x40 ) printf ("                         [Bit 6] RMS amplitude\n");
	      
	      printf ("        calibration duration: %u\n", blkt_310->duration);
	      printf ("               signal period: %g\n", blkt_310->period);
	      printf ("            signal amplitude: %g\n", blkt_310->amplitude);
	      printf ("        input signal channel: %.3s", blkt_310->input_channel);
	      if ( details > 1 )
		printf ("               reserved byte: %u\n", blkt_310->reserved2);	      
	      printf ("         reference amplitude: %u\n", blkt_310->reference_amplitude);
	      printf ("                    coupling: %.12s\n", blkt_310->coupling);
	      printf ("                     rolloff: %.12s\n", blkt_310->rolloff);
	    }

	  else if ( cur_blkt->blkt_type == 320 )
	    {
	      struct blkt_320_s *blkt_320 = (struct blkt_320_s *) cur_blkt->blktdata;
	      
	      printf ("          BLOCKETTE %u: (%s)\n", cur_blkt->blkt_type,
		      get_blktdesc(cur_blkt->blkt_type));
	      printf ("              next blockette: %u\n", cur_blkt->next_blkt);
	      ms_btime2seedtimestr (&blkt_320->time, time);
	      printf ("      calibration start time: %s\n", time);
	      if ( details > 1 )
		printf ("               reserved byte: %u\n", blkt_320->reserved1);
	      
	      b = blkt_320->flags;
	      printf ("           calibration flags: [%u%u%u%u%u%u%u%u] 8 bits\n",
		      bit(b,0x01), bit(b,0x02), bit(b,0x04), bit(b,0x08),
		      bit(b,0x10), bit(b,0x20), bit(b,0x40), bit(b,0x80));
	      if ( b & 0x04 ) printf ("                         [Bit 2] Calibration was automatic\n");
	      if ( b & 0x08 ) printf ("                         [Bit 3] Calibration continued from previous record(s)\n");
	      if ( b & 0x10 ) printf ("                         [Bit 4] Random amplitudes\n");
	      
	      printf ("        calibration duration: %u\n", blkt_320->duration);
	      printf ("      peak-to-peak amplitude: %g\n", blkt_320->ptp_amplitude);
	      printf ("        input signal channel: %.3s", blkt_320->input_channel);
	      if ( details > 1 )
		printf ("               reserved byte: %u\n", blkt_320->reserved2);
	      printf ("         reference amplitude: %u\n", blkt_320->reference_amplitude);
	      printf ("                    coupling: %.12s\n", blkt_320->coupling);
	      printf ("                     rolloff: %.12s\n", blkt_320->rolloff);
	      printf ("                  noise type: %.8s\n", blkt_320->noise_type);
	    }
	  
	  else if ( cur_blkt->blkt_type == 390 )
	    {
	      struct blkt_390_s *blkt_390 = (struct blkt_390_s *) cur_blkt->blktdata;
	      
	      printf ("          BLOCKETTE %u: (%s)\n", cur_blkt->blkt_type,
		      get_blktdesc(cur_blkt->blkt_type));
	      printf ("              next blockette: %u\n", cur_blkt->next_blkt);
	      ms_btime2seedtimestr (&blkt_390->time, time);
	      printf ("      calibration start time: %s\n", time);
	      if ( details > 1 )
		printf ("               reserved byte: %u\n", blkt_390->reserved1);
	      
	      b = blkt_390->flags;
	      printf ("           calibration flags: [%u%u%u%u%u%u%u%u] 8 bits\n",
		      bit(b,0x01), bit(b,0x02), bit(b,0x04), bit(b,0x08),
		      bit(b,0x10), bit(b,0x20), bit(b,0x40), bit(b,0x80));
	      if ( b & 0x04 ) printf ("                         [Bit 2] Calibration was automatic\n");
	      if ( b & 0x08 ) printf ("                         [Bit 3] Calibration continued from previous record(s)\n");
	      
	      printf ("        calibration duration: %u\n", blkt_390->duration);
	      printf ("            signal amplitude: %g\n", blkt_390->amplitude);
	      printf ("        input signal channel: %.3s", blkt_390->input_channel);
	      if ( details > 1 )
		printf ("               reserved byte: %u\n", blkt_390->reserved2);
	    }

	  else if ( cur_blkt->blkt_type == 395 )
	    {
	      struct blkt_395_s *blkt_395 = (struct blkt_395_s *) cur_blkt->blktdata;
	      
	      printf ("          BLOCKETTE %u: (%s)\n", cur_blkt->blkt_type,
		      get_blktdesc(cur_blkt->blkt_type));
	      printf ("              next blockette: %u\n", cur_blkt->next_blkt);
	      ms_btime2seedtimestr (&blkt_395->time, time);
	      printf ("        calibration end time: %s\n", time);
	      if ( details > 1 )
		printf ("          reserved bytes (2): %u,%u\n",
			blkt_395->reserved[0], blkt_395->reserved[1]);
	    }

	  else if ( cur_blkt->blkt_type == 400 )
	    {
	      struct blkt_400_s *blkt_400 = (struct blkt_400_s *) cur_blkt->blktdata;
	      
	      printf ("          BLOCKETTE %u: (%s)\n", cur_blkt->blkt_type,
		      get_blktdesc(cur_blkt->blkt_type));
	      printf ("              next blockette: %u\n", cur_blkt->next_blkt);
	      printf ("      beam azimuth (degrees): %g\n", blkt_400->azimuth);
	      printf ("  beam slowness (sec/degree): %g\n", blkt_400->slowness);
	      printf ("               configuration: %u\n", blkt_400->configuration);
	      if ( details > 1 )
		printf ("          reserved bytes (2): %u,%u\n",
			blkt_400->reserved[0], blkt_400->reserved[1]);
	    }

	  else if ( cur_blkt->blkt_type == 405 )
	    {
	      struct blkt_405_s *blkt_405 = (struct blkt_405_s *) cur_blkt->blktdata;
	      
	      printf ("          BLOCKETTE %u: (%s, incomplete)\n", cur_blkt->blkt_type,
		      get_blktdesc(cur_blkt->blkt_type));
	      printf ("              next blockette: %u\n", cur_blkt->next_blkt);
	      printf ("           first delay value: %u\n", blkt_405->delay_values[0]);
	    }

	  else if ( cur_blkt->blkt_type == 500 )
	    {
	      struct blkt_500_s *blkt_500 = (struct blkt_500_s *) cur_blkt->blktdata;
	      
	      printf ("          BLOCKETTE %u: (%s)\n", cur_blkt->blkt_type,
		      get_blktdesc(cur_blkt->blkt_type));
	      printf ("              next blockette: %u\n", cur_blkt->next_blkt);
	      printf ("              VCO correction: %g%%\n", blkt_500->vco_correction);
	      ms_btime2seedtimestr (&blkt_500->time, time);
	      printf ("           time of exception: %s\n", time);
	      printf ("                        usec: %d\n", blkt_500->usec);
	      printf ("           reception quality: %u%%\n", blkt_500->reception_qual);
	      printf ("             exception count: %u\n", blkt_500->exception_count);
	      printf ("              exception type: %.16s\n", blkt_500->exception_type);
	      printf ("                 clock model: %.32s\n", blkt_500->clock_model);
	      printf ("                clock status: %.128s\n", blkt_500->clock_status);
	    }
	  
	  else if ( cur_blkt->blkt_type == 1000 )
	    {
	      struct blkt_1000_s *blkt_1000 = (struct blkt_1000_s *) cur_blkt->blktdata;
	      int recsize;
	      char order[40];
	      
	      /* Calculate record size in bytes as 2^(blkt_1000->rec_len) */
	      recsize = (unsigned int) 1 << blkt_1000->reclen;
	      
	      /* Big or little endian? */
	      if (blkt_1000->byteorder == 0)
		strncpy (order, "Little endian", sizeof(order)-1);
	      else if (blkt_1000->byteorder == 1)
		strncpy (order, "Big endian", sizeof(order)-1);
	      else
		strncpy (order, "Unknown value", sizeof(order)-1);
	      
	      printf ("         BLOCKETTE %u: (%s)\n", cur_blkt->blkt_type,
		      get_blktdesc(cur_blkt->blkt_type));
	      printf ("              next blockette: %u\n", cur_blkt->next_blkt);
	      printf ("                    encoding: %s (val:%u)\n",
		      (char *) get_encoding (blkt_1000->encoding), blkt_1000->encoding);
	      printf ("                  byte order: %s (val:%u)\n",
		      order, blkt_1000->byteorder);
	      printf ("               record length: %d (val:%u)\n",
		      recsize, blkt_1000->reclen);
	      
	      if ( details > 1 )
		printf ("               reserved byte: %u\n", blkt_1000->reserved);
	    }
	  
	  else if ( cur_blkt->blkt_type == 1001 )
	    {
	      struct blkt_1001_s *blkt_1001 = (struct blkt_1001_s *) cur_blkt->blktdata;
	      
	      printf ("         BLOCKETTE %u: (%s)\n", cur_blkt->blkt_type,
		      get_blktdesc(cur_blkt->blkt_type));
	      printf ("              next blockette: %u\n", cur_blkt->next_blkt);
	      printf ("              timing quality: %u%%\n", blkt_1001->timing_qual);
	      printf ("                micro second: %d\n", blkt_1001->usec);
	      
	      if ( details > 1 )
		printf ("               reserved byte: %u\n", blkt_1001->reserved);
	      
	      printf ("                 frame count: %u\n", blkt_1001->framecnt);
	    }

	  else if ( cur_blkt->blkt_type == 2000 )
	    {
	      struct blkt_2000_s *blkt_2000 = (struct blkt_2000_s *) cur_blkt->blktdata;
	      char order[40];
	      
	      /* Big or little endian? */
	      if (blkt_2000->byteorder == 0)
		strncpy (order, "Little endian", sizeof(order)-1);
	      else if (blkt_2000->byteorder == 1)
		strncpy (order, "Big endian", sizeof(order)-1);
	      else
		strncpy (order, "Unknown value", sizeof(order)-1);
	      
	      printf ("         BLOCKETTE %u: (%s)\n", cur_blkt->blkt_type,
		      get_blktdesc(cur_blkt->blkt_type));
	      printf ("              next blockette: %u\n", cur_blkt->next_blkt);
	      printf ("            blockette length: %u\n", blkt_2000->length);
	      printf ("                 data offset: %u\n", blkt_2000->data_offset);
	      printf ("               record number: %u\n", blkt_2000->recnum);
	      printf ("                  byte order: %s (val:%u)\n",
		      order, blkt_2000->byteorder);
	      b = blkt_2000->flags;
	      printf ("                  data flags: [%u%u%u%u%u%u%u%u] 8 bits\n",
		      bit(b,0x01), bit(b,0x02), bit(b,0x04), bit(b,0x08),
		      bit(b,0x10), bit(b,0x20), bit(b,0x40), bit(b,0x80));

	      if ( details > 1 )
		{
		  if ( b & 0x01 ) printf ("                         [Bit 0] 1: Stream oriented\n");
		  else            printf ("                         [Bit 0] 0: Record oriented\n");
		  if ( b & 0x02 ) printf ("                         [Bit 1] 1: Blockette 2000s may NOT be packaged\n");
		  else            printf ("                         [Bit 1] 0: Blockette 2000s may be packaged\n");
		  if ( ! (b & 0x04) && ! (b & 0x08) )
		                  printf ("                      [Bits 2-3] 00: Complete blockette\n");
		  else if ( ! (b & 0x04) && (b & 0x08) )
		                  printf ("                      [Bits 2-3] 01: First blockette in span\n");
		  else if ( (b & 0x04) && (b & 0x08) )
		                  printf ("                      [Bits 2-3] 11: Continuation blockette in span\n");
		  else if ( (b & 0x04) && ! (b & 0x08) )
		                  printf ("                      [Bits 2-3] 10: Final blockette in span\n");
		  if ( ! (b & 0x10) && ! (b & 0x20) )
		                  printf ("                      [Bits 4-5] 00: Not file oriented\n");
		  else if ( ! (b & 0x10) && (b & 0x20) )
		                  printf ("                      [Bits 4-5] 01: First blockette of file\n");
		  else if ( (b & 0x10) && ! (b & 0x20) )
		                  printf ("                      [Bits 4-5] 10: Continuation of file\n");
		  else if ( (b & 0x10) && (b & 0x20) )
		                  printf ("                      [Bits 4-5] 11: Last blockette of file\n");
		}
	      
	      printf ("           number of headers: %u\n", blkt_2000->numheaders);
	      
	      /* Crude display of the opaque data headers */
	      if ( details > 1 )
		printf ("                     headers: %.*s\n",
			(blkt_2000->data_offset - 15), blkt_2000->payload);
	    }
	  
	  else
	    {
	      printf ("         BLOCKETTE %u: (%s, not parsed)\n", cur_blkt->blkt_type,
		      get_blktdesc(cur_blkt->blkt_type));
	      printf ("              next blockette: %u\n", cur_blkt->next_blkt);
	    }
	  
	  cur_blkt = cur_blkt->next;
	}
    }
} /* End of msr_print() */


/***************************************************************************
 * msr_host_latency:
 *
 * Calculate the latency based on the host time in UTC accounting for
 * the time covered using the number of samples and sample rate; in
 * other words, the difference between the host time and the time of
 * the last sample in the specified Mini-SEED record.
 *
 * Double precision is returned, but the true precision is dependent
 * on the accuracy of the host system clock among other things.
 *
 * Returns seconds of latency or 0.0 on error (indistinguishable from
 * 0.0 latency).
 ***************************************************************************/
double
msr_host_latency (MSRecord *msr)
{
  double span = 0.0;            /* Time covered by the samples */
  double epoch;                 /* Current epoch time */
  double latency = 0.0;
  time_t tv;

  if ( msr == NULL )
    return 0.0;
  
  /* Calculate the time covered by the samples */
  if ( msr->samprate > 0.0 && msr->samplecnt > 0 )
    span = (1.0 / msr->samprate) * (msr->samplecnt - 1);
  
  /* Grab UTC time according to the system clock */
  epoch = (double) time(&tv);
  
  /* Now calculate the latency */
  latency = epoch - ((double) msr->starttime / HPTMODULUS) - span;
  
  return latency;
} /* End of msr_host_latency() */
