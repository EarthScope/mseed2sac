/***************************************************************************
 * traceutils.c:
 *
 * Generic routines to handle Traces.
 *
 * Written by Chad Trabant, IRIS Data Management Center
 *
 * modified: 2005.336
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libmseed.h"


/***************************************************************************
 * mst_init:
 *
 * Initialize and return a Trace struct, allocating memory if needed.
 * If the specified Trace includes data samples they will be freed.
 *
 * Returns a pointer to a Trace struct on success or NULL on error.
 ***************************************************************************/
Trace *
mst_init ( Trace *mst )
{
  /* Free datasamples if present */
  if ( mst )
    {
      if ( mst->datasamples )
	free (mst->datasamples);

      if ( mst->private )
	free (mst->private);
    }
  else
    {
      mst = (Trace *) malloc (sizeof(Trace));
    }
  
  if ( mst == NULL )
    {
      fprintf (stderr, "mst_init(): error allocating memory\n");
      return NULL;
    }
  
  memset (mst, 0, sizeof (Trace));
 
  return mst;
} /* End of mst_init() */


/***************************************************************************
 * mst_free:
 *
 * Free all memory associated with a Trace struct and set the pointer
 * to 0.
 ***************************************************************************/
void
mst_free ( Trace **ppmst )
{
  if ( ppmst && *ppmst )
    {
      /* Free datasamples if present */
      if ( (*ppmst)->datasamples )
        free ((*ppmst)->datasamples);

      /* Free private memory if present */
      if ( (*ppmst)->private )
        free ((*ppmst)->private);
      
      free (*ppmst);
      
      *ppmst = 0;
    }
} /* End of mst_free() */


/***************************************************************************
 * mst_initgroup:
 *
 * Initialize and return a TraceGroup struct, allocating memory if
 * needed.  If the supplied TraceGroup is not NULL any associated
 * memory it will be freed.
 *
 * Returns a pointer to a TraceGroup struct on success or NULL on error.
 ***************************************************************************/
TraceGroup *
mst_initgroup ( TraceGroup *mstg )
{
  Trace *mst = 0;
  Trace *next = 0;
  
  if ( mstg )
    {
      mst = mstg->traces;
      
      while ( mst )
	{
	  next = mst->next;
	  mst_free (&mst);
	  mst = next;
	}
    }
  else
    {
      mstg = (TraceGroup *) malloc (sizeof(TraceGroup));
    }
  
  if ( mstg == NULL )
    {
      fprintf (stderr, "mst_initgroup(): Error allocating memory\n");
      return NULL;
    }
  
  memset (mstg, 0, sizeof (TraceGroup));
  
  return mstg;
} /* End of mst_initgroup() */


/***************************************************************************
 * mst_freegroup:
 *
 * Free all memory associated with a TraceGroup struct and set the
 * pointer to 0.
 ***************************************************************************/
void
mst_freegroup ( TraceGroup **ppmstg )
{
  Trace *mst = 0;
  Trace *next = 0;
  
  if ( *ppmstg )
    {
      mst = (*ppmstg)->traces;
      
      while ( mst )
	{
	  next = mst->next;
	  mst_free (&mst);
	  mst = next;
	}
      
      free (*ppmstg);
      
      *ppmstg = 0;
    }
} /* End of mst_freegroup() */


/***************************************************************************
 * mst_findmatch:
 *
 * Traverse the Trace chain starting at 'mst' until a Trace is found
 * that matches the given name identifiers.
 *
 * Return a pointer a matching Trace otherwise 0 if no match found.
 ***************************************************************************/
Trace *
mst_findmatch ( Trace *startmst,
		char *network, char *station, char *location, char *channel )
{
  if ( ! startmst )
    return 0;
  
  while ( startmst )
    {
      /* Check if this trace matches the record */
      if ( ! strcmp (network, startmst->network) &&
	   ! strcmp (station, startmst->station) &&
	   ! strcmp (location, startmst->location) &&
	   ! strcmp (channel, startmst->channel) )
	break;
      
      startmst = startmst->next;
    }
  
  return startmst;
} /* End of mst_findmatch() */


/***************************************************************************
 * mst_findadjacent:
 *
 * Find a Trace in a TraceGroup matching the given name identifiers,
 * samplerate and is adjacent with a time span.
 *
 * The time tolerance and sample rate tolerance are used to determine
 * if traces abut.  If timetol is -1.0 the default tolerance of 1/2
 * the sample period will be used.  If samprratetol is -1.0 the
 * default tolerance check of abs(1-sr1/sr2) < 0.0001 is used (defined
 * in libmseed.h).  If timetol or sampratetol is -2.0 the respective
 * tolerance check will not be performed.
 *
 * The 'whence' flag will be set, when a matching Trace is found, to
 * indicate where the indicated time span is adjacent to the Trace
 * using the following values:
 * 1: time span fits at the end of the Trace
 * 2: time span fits at the beginning of the Trace
 *
 * Return a pointer a matching Trace and set the 'whence' flag
 * otherwise 0 if no match found.
 ***************************************************************************/
Trace *
mst_findadjacent ( TraceGroup *mstg, flag *whence,
		   char *network, char *station, char *location, char *channel,
		   double samprate, double sampratetol,
		   hptime_t starttime, hptime_t endtime, double timetol )
{
  Trace *mst = 0;
  double pregap, postgap;
  
  if ( ! mstg )
    return 0;
  
  *whence = 0;
  
  mst = mstg->traces;
  
  while ( (mst = mst_findmatch (mst, network, station, location, channel)) )
    {
      /* Perform samprate tolerance check if requested */
      if ( sampratetol != -2.0 )
	{ 
	  /* Perform default samprate tolerance check if requested */
	  if ( sampratetol == -1.0 )
	    {
	      if ( ! MS_ISRATETOLERABLE (samprate, mst->samprate) )
		{
		  mst = mst->next;
		  continue;
		}
	    }
	  /* Otherwise check against the specified sample rate tolerance */
	  else if ( ms_dabs (samprate - mst->samprate) > sampratetol )
	    {
	      mst = mst->next;
	      continue;
	    }
	}
      
      /* post/pregap are negative when the record overlaps the trace
       * segment and positive when there is a time gap.
       */
      postgap = ((double)(starttime - mst->endtime)/HPTMODULUS) - (1.0 / samprate);
      
      pregap = ((double)(mst->starttime - endtime)/HPTMODULUS) - (1.0 / samprate);
      
      /* If not checking the time tolerance decide if beginning or end is a better fit */
      if ( timetol == -2.0 )
	{
	  if ( ms_dabs(postgap) < ms_dabs(pregap) )
	    *whence = 1;
	  else
	    *whence = 2;
	  
	  break;
	}
      else
	{
	  /* Calculate default time tolerance (1/2 sample period) if needed */
	  if ( timetol == -1.0 )
	    timetol = 0.5 / samprate;
	  
	  if ( ms_dabs(postgap) <= timetol ) /* Span fits right at the end of the trace */
	    {
	      *whence = 1;
	      break;
	    }
	  else if ( ms_dabs(pregap) <= timetol ) /* Span fits right at the beginning of the trace */
	    {
	      *whence = 2;
	      break;
	    }
	}
      
      mst = mst->next;
    }
  
  return mst;
} /* End of mst_findadjacent() */


/***************************************************************************
 * mst_addmsr:
 *
 * Add MSrecord time coverage to a Trace.  The start or end time will
 * be updated and samples will be copied if they exist.  No checking
 * is done to verify that the record matches the trace in any way.
 *
 * If whence is 1 the coverage will be added at the end of the trace,
 * whereas if whence is 2 the coverage will be added at the beginning
 * of the trace.
 *
 * Return 0 on success and -1 on error.
 ***************************************************************************/
int
mst_addmsr ( Trace *mst, MSrecord *msr, flag whence )
{
  int samplesize = 0;
  
  if ( ! mst || ! msr )
    return -1;
  
  /* Reallocate data sample buffer if samples are present */
  if ( msr->datasamples && msr->numsamples >= 0 )
    {
      /* Check that the entire record was decompressed */
      if ( msr->samplecnt != msr->numsamples )
	{
	  fprintf (stderr, "mst_addmsr(): Sample counts do not match, record not fully decompressed?\n");
	  fprintf (stderr, "  The sample buffer will likely contain a discontinuity.\n");
	}

      if ( (samplesize = get_samplesize(msr->sampletype)) == 0 )
	{
	  fprintf (stderr, "mst_addmsr(): Unrecognized sample type: '%c'\n",
		   msr->sampletype);
	  return -1;
	}
    
      if ( msr->sampletype != mst->sampletype )
	{
	  fprintf (stderr, "mst_addmsr(): Mismatched sample type, '%c' and '%c'\n",
		   msr->sampletype, mst->sampletype);
	  return -1;
	}
      
      mst->datasamples = realloc (mst->datasamples,
				  mst->numsamples * samplesize +
				  msr->numsamples * samplesize );
      
      if ( mst->datasamples == NULL )
	{
	  fprintf (stderr, "mst_addmsr(): Error allocating memory\n");
	  return -1;
	}
    }
  
  /* Add samples at end of trace */
  if ( whence == 1 )
    {
      if ( msr->datasamples && msr->numsamples >= 0 )
	{
	  memcpy ((char *)mst->datasamples + (mst->numsamples * samplesize),
		  msr->datasamples,
		  msr->numsamples * samplesize);
	  
	  mst->numsamples += msr->numsamples;
	}
      
      mst->endtime = msr_endtime (msr);
      
      if ( mst->endtime == HPTERROR )
	{
	  fprintf (stderr, "mst_addmsr(): Error calculating record end time\n");
	  return -1;
	}
    }
  
  /* Add samples at the beginning of trace */
  else if ( whence == 2 )
    {
      if ( msr->datasamples && msr->numsamples >= 0 )
	{
	  /* Move any samples to end of buffer */
	  if ( mst->numsamples > 0 )
	    {
	      memmove ((char *)mst->datasamples + (msr->numsamples * samplesize),
		       mst->datasamples,
		       msr->numsamples * samplesize);
	    }

	  memcpy (mst->datasamples,
		  msr->datasamples,
		  msr->numsamples * samplesize);
	  
	  mst->numsamples += msr->numsamples;
	}
      
      mst->starttime = msr->starttime;
    }
  
  /* Update Trace sample count */
  mst->samplecnt += msr->samplecnt;
  
  return 0;
} /* End of mst_addmsr() */


/***************************************************************************
 * mst_addspan:
 *
 * Add a time span to a Trace.  The start or end time will be updated
 * and samples will be copied if they are provided.  No checking is done to
 * verify that the record matches the trace in any way.
 *
 * If whence is 1 the coverage will be added at the end of the trace,
 * whereas if whence is 2 the coverage will be added at the beginning
 * of the trace.
 *
 * Return 0 on success and -1 on error.
 ***************************************************************************/
int
mst_addspan ( Trace *mst, hptime_t starttime, hptime_t endtime,
	      void *datasamples, int numsamples, char sampletype,
	      flag whence )
{
  int samplesize = 0;
  
  if ( ! mst )
    return -1;
  
  if ( datasamples && numsamples > 0 )
    {
      if ( (samplesize = get_samplesize(sampletype)) == 0 )
	{
	  fprintf (stderr, "mst_addspan(): Unrecognized sample type: '%c'\n",
		   sampletype);
	  return -1;
	}
      
      if ( sampletype != mst->sampletype )
	{
	  fprintf (stderr, "mst_addspan(): Mismatched sample type, '%c' and '%c'\n",
		   sampletype, mst->sampletype);
	  return -1;
	}
      
      mst->datasamples = realloc (mst->datasamples,
				  mst->numsamples * samplesize +
				  numsamples * samplesize);
      
      if ( mst->datasamples == NULL )
	{
	  fprintf (stderr, "mst_addspan(): Error allocating memory\n");
	  return -1;
	}
    }
  
  /* Add samples at end of trace */
  if ( whence == 1 )
    {
      if ( datasamples && numsamples > 0 )
	{
	  memcpy ((char *)mst->datasamples + (mst->numsamples * samplesize),
		  datasamples,
		  numsamples * samplesize);
	  
	  mst->numsamples += numsamples;
	}
      
      mst->endtime = endtime;      
    }
  
  /* Add samples at the beginning of trace */
  else if ( whence == 2 )
    {
      if ( datasamples && numsamples > 0 )
	{
	  /* Move any samples to end of buffer */
	  if ( mst->numsamples > 0 )
	    {
	      memmove ((char *)mst->datasamples + (numsamples * samplesize),
		       mst->datasamples,
		       numsamples * samplesize);
	    }
	  
	  memcpy (mst->datasamples,
		  datasamples,
		  numsamples * samplesize);
	  
	  mst->numsamples += numsamples;
	}
      
      mst->starttime = starttime;
    }
  
  /* Update Trace sample count */
  if ( numsamples > 0 )
    mst->samplecnt += numsamples;
  
  return 0;
} /* End of mst_addspan() */


/***************************************************************************
 * mst_addmsrtogroup:
 *
 * Add data samples from a MSrecord to a Trace in a TraceGroup by
 * searching the group for the approriate Trace and either adding data
 * to it or creating a new Trace if no match found.
 *
 * Matching traces are found using the mst_findadjacent() routine.
 *
 * Return a pointer to the Trace updated or 0 on error.
 ***************************************************************************/
Trace *
mst_addmsrtogroup ( TraceGroup *mstg, MSrecord *msr,
		    double timetol, double sampratetol )
{
  Trace *mst = 0;
  hptime_t endtime;
  flag whence;
  
  if ( ! mstg || ! msr )
    return 0;
  
  endtime = msr_endtime (msr);
  
  if ( endtime == HPTERROR )
    {
      fprintf (stderr, "mst_addmsrtogroup(): Error calculating record end time\n");
      return 0;
    }
  
  /* Find matching, time adjacent Trace */
  mst = mst_findadjacent (mstg, &whence,
			  msr->network, msr->station, msr->location, msr->channel,
			  msr->samprate, sampratetol,
			  msr->starttime, endtime, timetol);
  
  /* If a match was found update it otherwise create a new Trace and
     add to end of Trace chain */
  if ( mst )
    {
      /* Records with no time coverage do not contribute to a trace */
      if ( msr->samplecnt == 0 || msr->samprate <= 0.0 )
	return mst;
      
      if ( mst_addmsr (mst, msr, whence) )
	{
	  return 0;
	}
    }
  else
    {
      mst = mst_init (NULL);
      
      strncpy (mst->network, msr->network, sizeof(mst->network));
      strncpy (mst->station, msr->station, sizeof(mst->station));
      strncpy (mst->location, msr->location, sizeof(mst->location));
      strncpy (mst->channel, msr->channel, sizeof(mst->channel));
      
      mst->starttime = msr->starttime;
      mst->samprate = msr->samprate;
      mst->sampletype = msr->sampletype;
      
      if ( mst_addmsr (mst, msr, 1) )
	{
	  mst_free (&mst);
	  return 0;
	}
      
      /* Link new Trace into the end of the chain */
      if ( ! mstg->traces )
	{
	  mstg->traces = mst;
	}
      else
	{
	  Trace *lasttrace = mstg->traces;
	  
	  while ( lasttrace->next )
	    lasttrace = lasttrace->next;
	  
	  lasttrace->next = mst;
	}
      
      mstg->numtraces++;
    }
  
  return mst;
}  /* End of mst_addmsrtogroup() */


/***************************************************************************
 * mst_addtracetogroup:
 *
 * Add a Trace to a TraceGroup at the end of the Trace chain.
 *
 * Return a pointer to the Trace added or 0 on error.
 ***************************************************************************/
Trace *
mst_addtracetogroup ( TraceGroup *mstg, Trace *mst )
{
  Trace *lasttrace;

  if ( ! mstg || ! mst )
    return 0;
  
  if ( ! mstg->traces )
    {
      mstg->traces = mst;
    }
  else
    {
      lasttrace = mstg->traces;
      
      while ( lasttrace->next )
	lasttrace = lasttrace->next;
      
      lasttrace->next = mst;
    }
  
  mst->next = 0;
  
  mstg->numtraces++;
  
  return mst;
} /* End of mst_addtracetogroup() */


/***************************************************************************
 * mst_heal:
 *
 * Check if traces in TraceGroup can be healed, if contiguous segments
 * belong together they will be merged.  This routine is only useful
 * if the trace group was assembled from segments out of time order
 * (e.g. a file of Mini-SEED records not in time order) but forming
 * contiguous time coverage.
 *
 * The time tolerance and sample rate tolerance are used to determine
 * if the traces are indeed the same.  If timetol is -1.0 the default
 * tolerance of 1/2 the sample period will be used.  If samprratetol
 * is -1.0 the default tolerance check of abs(1-sr1/sr2) < 0.0001 is
 * used (defined in libmseed.h).
 *
 * Return number of trace mergings on success otherwise -1.
 ***************************************************************************/
int
mst_heal ( TraceGroup *mstg, double timetol, double sampratetol )
{
  int mergings = 0;
  Trace *curtrace = 0;
  Trace *nexttrace = 0;
  Trace *searchtrace = 0;
  Trace *prevtrace = 0;
  int8_t merged = 0;
  double postgap;
  double pregap;
  
  if ( ! mstg )
    return -1;
  
  curtrace = mstg->traces;
  
  while ( curtrace )
    {
      nexttrace = mstg->traces;
      prevtrace = mstg->traces;
      
      while ( nexttrace )
	{
	  searchtrace = nexttrace;
	  nexttrace = searchtrace->next;
	  
	  /* Do not process the same Trace we are trying to match */
	  if ( searchtrace == curtrace )
	    {
	      prevtrace = searchtrace;
	      continue;
	    }
	  
	  /* Check if this trace matches the curtrace */
	  if ( strcmp (searchtrace->network, curtrace->network) ||
	       strcmp (searchtrace->station, curtrace->station) ||
	       strcmp (searchtrace->location, curtrace->location) ||
	       strcmp (searchtrace->channel, curtrace->channel) )
	    {
	      prevtrace = searchtrace;
	      continue;
	    }
      	  
	  /* Perform default samprate tolerance check if requested */
	  if ( sampratetol == -1.0 )
	    {
	      if ( ! MS_ISRATETOLERABLE (searchtrace->samprate, curtrace->samprate) )
		{
		  prevtrace = searchtrace;
		  continue;
		}
	    }
	  /* Otherwise check against the specified sample rates tolerance */
	  else if ( ms_dabs (searchtrace->samprate - curtrace->samprate) > sampratetol )
	    {
	      prevtrace = searchtrace;
	      continue;
	    }
	  
	  merged = 0;
	  
	  /* post/pregap are negative when searchtrace overlaps curtrace
	   * segment and positive when there is a time gap.
	   */
	  postgap = ((double)(searchtrace->starttime - curtrace->endtime)/HPTMODULUS)
	    - (1.0 / curtrace->samprate);
	  
	  pregap = ((double)(curtrace->starttime - searchtrace->endtime)/HPTMODULUS)
	    - (1.0 / curtrace->samprate);
	  
	  /* Calculate default time tolerance (1/2 sample period) if needed */
	  if ( timetol == -1.0 )
	    timetol = 0.5 / searchtrace->samprate;
	  
	  /* Fits right at the end of curtrace */
	  if ( ms_dabs(postgap) <= timetol )
	    {
	      /* Merge searchtrace with curtrace */
	      mst_addspan (curtrace, searchtrace->starttime, searchtrace->endtime,
			   searchtrace->datasamples, searchtrace->numsamples,
			   searchtrace->sampletype, 1);
	      
	      /* If no data is present, make sure sample count is updated */
	      if ( searchtrace->numsamples <= 0 )
		curtrace->samplecnt += searchtrace->samplecnt;
	      
	      merged = 1;
	    }
	  
	  /* Fits right at the beginning of curtrace */
	  else if ( ms_dabs(pregap) <= timetol )
	    {
	      /* Merge searchtrace with curtrace */
	      mst_addspan (curtrace, searchtrace->starttime, searchtrace->endtime,
			   searchtrace->datasamples, searchtrace->numsamples,
			   searchtrace->sampletype, 2);
	      
	      /* If no data is present, make sure sample count is updated */
	      if ( searchtrace->numsamples <= 0 )
		curtrace->samplecnt += searchtrace->samplecnt;
	      
	      merged = 1;
	    }
	  
	  if ( merged )
	    {
	      /* Re-link trace chain and free searchtrace */
	      if ( searchtrace == mstg->traces )
		mstg->traces = nexttrace;
	      else
		prevtrace->next = nexttrace;
	      
	      mst_free (&searchtrace);
	      
	      mstg->numtraces--;
	      mergings++;
	      break;
	    }
	  
	  prevtrace = searchtrace;
	}
      
      curtrace = curtrace->next;
    }

  return mergings;
}  /* End of mst_heal() */


/***************************************************************************
 * mst_groupsort:
 *
 * Sort a TraceGroup first on source name, then sample rate, then
 * start time and finally on descending endtime (longest trace first).
 * The "bubble sort" algorithm herein is not terribly efficient.
 *
 * Return 0 on success and -1 on error.
 ***************************************************************************/
int
mst_groupsort ( TraceGroup *mstg )
{
  Trace *mst, *pmst;
  char src1[30], src2[30];
  int swap;
  int swapped;
  int strcmpval;
  
  if ( ! mstg )
    return -1;
  
  if ( ! mstg->traces )
    return 0;

  /* Loop over the Trace chain until no more entries are swapped, "bubble" sort */
  do
    {
      swapped = 0;
      
      mst = mstg->traces;
      pmst = mst;
      
      while ( mst->next ) {
	swap = 0;
	
	mst_srcname (mst, src1);
	mst_srcname (mst->next, src2);
	
	strcmpval = strcmp (src1, src2);
	
	/* If the source names do not match make sure the "greater" string is 2nd,
	 * otherwise, if source names do match, make sure the highest sample rate is 2nd
	 * otherwise, if sample rates match, make sure the later start time is 2nd
	 * otherwise, if start times match, make sure the earlier end time is 2nd
	 */
	if ( strcmpval > 0 )
	  {
	    swap = 1;
	  }
	else if ( strcmpval == 0 )
	  {
	    if ( mst->samprate > mst->next->samprate )
	      {
		swap = 1;
	      }
	    else if ( mst->samprate == mst->next->samprate )
	      {
		if ( mst->starttime > mst->next->starttime )
		  {
		    swap = 1;
		  }
		else if (  mst->starttime == mst->next->starttime )
		  {
		    if ( mst->endtime < mst->next->endtime )
		      {
			swap = 1;
		      }
		  }
	      }
	  }
		
	/* If a swap condition was found swap the entries */
	if ( swap )
	  {
	    swapped++;
	    
	    if ( mst == mstg->traces && pmst == mstg->traces )
	      {
		mstg->traces = mst->next;
	      }
	    else
	      {
		pmst->next = mst->next;
	      }
	    
	    pmst = mst->next;
	    mst->next = mst->next->next;
	    pmst->next = mst;
	  }
	else
	  {
	    pmst = mst;
	    mst = mst->next;
	  }
	
	if ( ! mst )
	  break;
      }
    } while ( swapped );
  
  return 0;
} /* End of mst_groupsort() */


/***************************************************************************
 * mst_srcname:
 *
 * Generate a source name string for a specified Trace in the
 * format: 'NET_STA_LOC_CHAN'.  The passed srcname must have enough
 * room for the resulting string.
 *
 * Returns a pointer to the resulting string or NULL on error.
 ***************************************************************************/
char *
mst_srcname (Trace *mst, char *srcname)
{
  if ( ! mst )
    return NULL;
  
  /* Build the source name string */
  sprintf (srcname, "%s_%s_%s_%s",
	   mst->network, mst->station,
	   mst->location, mst->channel);
  
  return srcname;
} /* End of mst_srcname() */


/***************************************************************************
 * mst_printtracelist:
 *
 * Print trace list summary information for the specified TraceGroup.
 *
 * By default only print the srcname, starttime and endtime for each
 * trace.  If details is greater than 0 include the sample rate,
 * number of samples and a total trace count.  If gaps is greater than
 * 0 and the previous trace matches (srcname & samprate) include the
 * gap between the endtime of the last trace and the starttime of the
 * current trace.
 *
 * The timeformat flag can either be:
 * 0 : SEED time format (year, day-of-year, hour, min, sec)
 * 1 : ISO time format (year, month, day, hour, min, sec)
 * 2 : Epoch time, seconds since the epoch
 ***************************************************************************/
void
mst_printtracelist ( TraceGroup *mstg, flag timeformat,
		     flag details, flag gaps )
{
  Trace *mst = 0;
  char srcname[50];
  char prevsrcname[50];
  char stime[30];
  char etime[30];
  char gapstr[20];
  double gap;
  double prevsamprate;
  hptime_t prevendtime;
  int tracecnt = 0;
  
  if ( ! mstg )
    {
      return;
    }
  
  mst = mstg->traces;
  
  /* Print out the appropriate header */
  if ( details > 0 && gaps > 0 )
    printf ("   Source              Start sample             End sample        Gap  Hz   Samples\n");
  else if ( details <= 0 && gaps > 0 )
    printf ("   Source              Start sample             End sample        Gap\n");
  else if ( details > 0 && gaps <= 0 )
    printf ("   Source              Start sample             End sample        Hz   Samples\n");
  else
    printf ("   Source              Start sample             End sample\n");
  
  prevsrcname[0] = '\0';
  prevsamprate = -1.0;
  prevendtime = 0;
  
  while ( mst )
    {
      mst_srcname (mst, srcname);
      
      /* Create formatted time strings */
      if ( timeformat == 2 )
	{
	  snprintf (stime, sizeof(stime), "%.6f", (double) MS_HPTIME2EPOCH(mst->starttime) );
	  snprintf (etime, sizeof(etime), "%.6f", (double) MS_HPTIME2EPOCH(mst->endtime) );
	}
      else if ( timeformat == 1 )
	{
	  if ( ms_hptime2isotimestr (mst->starttime, stime) == NULL )
	    fprintf (stderr, "Error converting trace start time for %s\n", srcname);
	  
	  if ( ms_hptime2isotimestr (mst->endtime, etime) == NULL )
	    fprintf (stderr, "Error converting trace end time for %s\n", srcname);
	}
      else
	{
	  if ( ms_hptime2seedtimestr (mst->starttime, stime) == NULL )
	    fprintf (stderr, "Error converting trace start time for %s\n", srcname);
	  
	  if ( ms_hptime2seedtimestr (mst->endtime, etime) == NULL )
	    fprintf (stderr, "Error converting trace end time for %s\n", srcname);
	}
      
      /* Print trace info at varying levels */
      if ( gaps > 0 )
	{
	  gap = 0.0;
	  
	  if ( ! strcmp (prevsrcname, srcname) && prevsamprate != -1.0 &&
	       MS_ISRATETOLERABLE (prevsamprate, mst->samprate) )
	    gap = (double) (mst->starttime - prevendtime) / HPTMODULUS;
	  
	  /* Check that any overlap is not larger than the trace coverage */
	  if ( gap < 0.0 )
	    if ( (gap * -1.0) > (((double)(mst->endtime - mst->starttime)/HPTMODULUS) + (1.0 / mst->samprate)) )
	      gap = -(((double)(mst->endtime - mst->starttime)/HPTMODULUS) + (1.0 / mst->samprate));
	  
	  /* Fix up gap display */
	  if ( gap >= 86400.0 || gap <= -86400.0 )
	    snprintf (gapstr, sizeof(gapstr), "%-3.1fd", (gap / 86400));
	  else if ( gap >= 3600.0 || gap <= -3600.0 )
	    snprintf (gapstr, sizeof(gapstr), "%-3.1fh", (gap / 3600));
	  else
	    snprintf (gapstr, sizeof(gapstr), "%-4.4g", gap);
	  
	  if ( details <= 0 )
	    printf ("%-15s %-24s %-24s %-4s\n",
		    srcname, stime, etime, gapstr);
	  else
	    printf ("%-15s %-24s %-24s %-s %-4.4g %-d\n",
		    srcname, stime, etime, gapstr, mst->samprate, mst->samplecnt);
	}
      else if ( details > 0 && gaps <= 0 )
	printf ("%-15s %-24s %-24s %-4.4g %-d\n",
		srcname, stime, etime, mst->samprate, mst->samplecnt);
      else
	printf ("%-15s %-24s %-24s\n", srcname, stime, etime);
      
      if ( gaps > 0 )
	{
	  strcpy (prevsrcname, srcname);
	  prevsamprate = mst->samprate;
	  prevendtime = mst->endtime;
	}
      
      tracecnt++;
      mst = mst->next;
    }

  if ( tracecnt != mstg->numtraces )
    fprintf (stderr, "mst_printtracelist(): number of traces in trace group is inconsistent\n");
  
  if ( details > 0 )
    printf ("Total: %d trace(s)\n", tracecnt);
  
}  /* End of mst_printtracelist() */


/***************************************************************************
 * mst_printgaplist:
 *
 * Print gap/overlap list summary information for the specified
 * TraceGroup.  Overlaps are printed as negative gaps.  The trace
 * summary information in the TraceGroup is logically inverted so gaps
 * for like channels are identified.
 *
 * If mingap and maxgap are not NULL their values will be enforced and
 * only gaps/overlaps matching their implied criteria will be printed.
 *
 * The timeformat flag can either be:
 * 0 : SEED time format (year, day-of-year, hour, min, sec)
 * 1 : ISO time format (year, month, day, hour, min, sec)
 * 2 : Epoch time, seconds since the epoch
 ***************************************************************************/
void
mst_printgaplist (TraceGroup *mstg, flag timeformat,
		  double *mingap, double *maxgap)
{
  Trace *mst, *pmst;
  char src1[30], src2[30];
  char time1[30], time2[30];
  char gapstr[30];
  double gap, nsamples;
  flag printflag;
  int gapcnt = 0;

  if ( ! mstg )
    return;
  
  if ( ! mstg->traces )
    return;
  
  mst = mstg->traces;
  pmst = mst;
  
  printf ("   Source              Last Sample              Next Sample       Gap   Samples\n");
  
  while ( mst->next )
    {
      mst_srcname (mst, src1);
      mst_srcname (mst->next, src2);
      
      if ( ! strcmp (src1, src2) )
	{
	  /* Skip Traces with 0 sample rate, usually from SOH records */
	  if ( mst->samprate == 0.0 )
	    {
	      pmst = mst;
	      mst = mst->next;
	      continue;
	    }
	  
	  /* Check that sample rates match using default tolerance */
	  if ( ! MS_ISRATETOLERABLE (mst->samprate, mst->next->samprate) )
	    {
	      fprintf (stderr, "%s Sample rate changed! %.10g -> %.10g\n",
		       src1, mst->samprate, mst->next->samprate );
	    }
	  
	  gap = (double) (mst->next->starttime - mst->endtime) / HPTMODULUS;
	  
	  /* Check that any overlap is not larger than the trace coverage */
	  if ( gap < 0.0 )
	    if ( (gap * -1.0) > (((double)(mst->next->endtime - mst->next->starttime)/HPTMODULUS) + (1.0 / mst->next->samprate)) )
	      gap = -(((double)(mst->next->endtime - mst->next->starttime)/HPTMODULUS) + (1.0 / mst->next->samprate));
	  
	  printflag = 1;

	  /* Check gap/overlap criteria */
	  if ( mingap )
	    if ( gap < *mingap )
	      printflag = 0;

	  if ( maxgap )
	    if ( gap > *maxgap )
	      printflag = 0;
	  
	  if ( printflag )
	    {
	      nsamples = ms_dabs(gap) * mst->samprate;
	      
	      if ( gap > 0.0 )
		nsamples -= 1.0;
	      else
		nsamples += 1.0;
	      
	      /* Fix up gap display */
	      if ( gap >= 86400.0 || gap <= -86400.0 )
		snprintf (gapstr, sizeof(gapstr), "%-3.1fd", (gap / 86400));
	      else if ( gap >= 3600.0 || gap <= -3600.0 )
		snprintf (gapstr, sizeof(gapstr), "%-3.1fh", (gap / 3600));
	      else
		snprintf (gapstr, sizeof(gapstr), "%-4.4g", gap);
	      
	      /* Create formatted time strings */
	      if ( timeformat == 2 )
		{
		  snprintf (time1, sizeof(time1), "%.6f", (double) MS_HPTIME2EPOCH(mst->endtime) );
		  snprintf (time2, sizeof(time2), "%.6f", (double) MS_HPTIME2EPOCH(mst->next->starttime) );
		}
	      else if ( timeformat == 1 )
		{
		  if ( ms_hptime2isotimestr (mst->endtime, time1) == NULL )
		    fprintf (stderr, "Error converting trace end time for %s\n", src1);
		  
		  if ( ms_hptime2isotimestr (mst->next->starttime, time2) == NULL )
		    fprintf (stderr, "Error converting next trace start time for %s\n", src1);
		}
	      else
		{
		  if ( ms_hptime2seedtimestr (mst->endtime, time1) == NULL )
		    fprintf (stderr, "Error converting trace end time for %s\n", src1);
		  
		  if ( ms_hptime2seedtimestr (mst->next->starttime, time2) == NULL )
		    fprintf (stderr, "Error converting next trace start time for %s\n", src1);
		}
	      
	      printf ("%-15s %-24s %-24s %-4s  %-.8g\n",
		      src1, time1, time2, gapstr, nsamples);
	      
	      gapcnt++;
	    }
	}
      
      pmst = mst;
      mst = mst->next;
    }
  
  printf ("Total: %d gap(s)\n", gapcnt);
  
}  /* End of mst_printgaplist() */


/***************************************************************************
 * mst_pack:
 *
 * Pack Trace data into Mini-SEED records using the specified record
 * length, encoding format and byte order.  The datasamples array and
 * numsamples field will be adjusted (reduced) based on how many
 * samples were packed.
 *
 * As each record is filled and finished they are passed to
 * record_handler along with their length in bytes.  It is the
 * responsibility of record_handler to process the record, the memory
 * will be re-used when record_handler returns.
 *
 * If the flush flag is > 0 all of the data will be packed into data
 * records even though the last one will probably not be filled.
 *
 * If the mstemplate argument is not NULL it will be used as the
 * template for the packed Mini-SEED records.  Otherwise a new
 * MSrecord will be initialized and populated from values in the
 * Trace.  The reclen, encoding and byteorder arguments take
 * precedence over those in the template.  The start time, sample
 * rate, datasamples, numsamples and sampletype values from the
 * template will be preserved.
 *
 * Returns the number of records created on success and -1 on error.
 ***************************************************************************/
int
mst_pack ( Trace *mst, void (*record_handler) (char *, int),
	   int reclen, flag encoding, flag byteorder,
	   int *packedsamples, flag flush, flag verbose,
	   MSrecord *mstemplate )
{
  MSrecord *msr;
  char srcname[50];
  int packedrecords;
  int samplesize;
  int bufsize;
  
  hptime_t preservestarttime = 0;
  double preservesamprate = 0.0;
  void *preservedatasamples = 0;
  int32_t preservenumsamples = 0;
  char preservesampletype = 0;
  
  if ( mstemplate )
    {
      msr = mstemplate;
      
      preservestarttime = msr->starttime;
      preservesamprate = msr->samprate;
      preservedatasamples = msr->datasamples;
      preservenumsamples = msr->numsamples;
      preservesampletype = msr->sampletype;
    }
  else
    {
      msr = msr_init (NULL);
      
      if ( msr == NULL )
	{
	  fprintf (stderr, "mst_pack(): Error initializing msr\n");
	  return -1;
	}
      
      msr->drec_indicator = 'D';
      strcpy (msr->network, mst->network);
      strcpy (msr->station, mst->station);
      strcpy (msr->location, mst->location);
      strcpy (msr->channel, mst->channel);
    }
  
  /* Setup MSrecord template for packing */
  msr->reclen = reclen;
  msr->encoding = encoding;
  msr->byteorder = byteorder;
  
  msr->starttime = mst->starttime;
  msr->samprate = mst->samprate;
  msr->datasamples = mst->datasamples;
  msr->numsamples = mst->numsamples;
  msr->sampletype = mst->sampletype;
  
  /* Sample count sanity check */
  if ( mst->samplecnt != mst->numsamples )
    {
      fprintf (stderr, "mst_pack(): Sample counts do not match, abort\n");
      return -1;
    }
  
  /* Pack data */
  packedrecords = msr_pack (msr, record_handler, packedsamples, flush, verbose);
  
  if ( verbose > 1 )
    {
      fprintf (stderr, "Packed %d records for %s trace\n", packedrecords, mst_srcname (mst, srcname));
    }
  
  /* Adjust Trace start time, data array and sample count */
  if ( *packedsamples > 0 )
    {
      /* The new start time was calculated my msr_pack */
      mst->starttime = msr->starttime;
      
      samplesize = get_samplesize (mst->sampletype);
      bufsize = (mst->numsamples - *packedsamples) * samplesize;
      
      if ( bufsize )
	{
	  memmove (mst->datasamples,
		   (char *) mst->datasamples + (*packedsamples * samplesize),
		   bufsize);
	  
	  mst->datasamples = realloc (mst->datasamples, bufsize);
	  
	  if ( mst->datasamples == NULL )
	    {
	      fprintf (stderr, "mst_pack(): Error re-allocing datasamples buffer\n");
	      return -1;
	    }
	}
      else
	{
	  if ( mst->datasamples )
	    free (mst->datasamples);
	  mst->datasamples = 0;
	}
      
      mst->samplecnt -= *packedsamples;
      mst->numsamples -= *packedsamples;
    }
    
  /* Reinstate preserved values if a template was used */
  if ( mstemplate )
    {
      msr->starttime = preservestarttime;
      msr->samprate = preservesamprate;
      msr->datasamples = preservedatasamples;
      msr->numsamples = preservenumsamples;
      msr->sampletype = preservesampletype;
    }
  
  return packedrecords;
}  /* End of mst_pack() */


/***************************************************************************
 * mst_packgroup:
 *
 * Pack TraceGroup data into Mini-SEED records by calling mst_pack()
 * for each Trace in the group.
 *
 * Returns the number of records created on success and -1 on error.
 ***************************************************************************/
int
mst_packgroup ( TraceGroup *mstg, void (*record_handler) (char *, int),
		int reclen, flag encoding, flag byteorder,
		int *packedsamples, flag flush, flag verbose,
		MSrecord *mstemplate )
{
  Trace *mst;
  int packedrecords = 0;
  int tracesamples = 0;
  char srcname[30];

  if ( ! mstg )
    {
      return -1;
    }
  
  *packedsamples = 0;
  mst = mstg->traces;
  
  while ( mst )
    {
      if ( mst->numsamples <= 0 )
	{
	  if ( verbose > 1 )
	    {
	      mst_srcname (mst, srcname);
	      fprintf (stderr, "No data samples for %s, skipping\n", srcname);
	    }
	}
      else
	{
	  packedrecords += mst_pack (mst, record_handler, reclen, encoding,
				     byteorder, &tracesamples, flush, verbose,
				     mstemplate);
	  
	  if ( packedrecords == -1 )
	    break;
	  
	  *packedsamples += tracesamples;
	}
      
      mst = mst->next;
    }
  
  return packedrecords;
}  /* End of mst_packgroup() */
