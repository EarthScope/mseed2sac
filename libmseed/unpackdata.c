/************************************************************************
 *  Routines for unpacking INT_16, INT_32, FLOAT_32, FLOAT_64,
 *  STEIM1 and STEIM2, data records.
 *
 *  Original framework by:
 *
 *	Douglas Neuhauser						
 *	Seismographic Station						
 *	University of California, Berkeley				
 *	doug@seismo.berkeley.edu					
 *									
 *  Modified by Chad Trabant,
 *  (previously) ORFEUS/EC-Project MEREDIAN
 *  (currently) IRIS Data Management Center
 *
 *  modified: 2006.173
 ************************************************************************/

/*
 * Copyright (c) 1996 The Regents of the University of California.
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for educational, research and non-profit purposes,
 * without fee, and without a written agreement is hereby granted,
 * provided that the above copyright notice, this paragraph and the
 * following three paragraphs appear in all copies.
 * 
 * Permission to incorporate this software into commercial products may
 * be obtained from the Office of Technology Licensing, 2150 Shattuck
 * Avenue, Suite 510, Berkeley, CA  94704.
 * 
 * IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY
 * FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES,
 * INCLUDING LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND
 * ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA HAS BEEN
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE
 * PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE UNIVERSITY OF
 * CALIFORNIA HAS NO OBLIGATIONS TO PROVIDE MAINTENANCE, SUPPORT,
 * UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 */

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include "libmseed.h"
#include "unpackdata.h"

#define X0  pf->w[0].fw
#define XN  pf->w[1].fw


/************************************************************************
 *  msr_unpack_int_16:							*
 *	Unpack int_16 miniSEED data and place in supplied buffer.	*
 *  Return: # of samples returned.                                      *
 ************************************************************************/
int msr_unpack_int_16 
 (int16_t      *ibuf,		/* ptr to input data.			*/
  int		num_samples,	/* number of data samples in all frames.*/
  int		req_samples,	/* number of data desired by caller.	*/
  int32_t      *databuff,	/* ptr to unpacked data array.		*/
  int		swapflag)       /* if data should be swapped.	        */
{
  int		nd = 0;		/* # of data points in packet.		*/
  uint16_t	stmp;
  
  if (num_samples < 0) return 0;
  if (req_samples < 0) return 0;
  
  for (nd=0; nd<req_samples && nd<num_samples; nd++) {
    stmp = ibuf[nd];
    if ( swapflag ) gswap2a (&stmp);
    databuff[nd] = stmp;
  }
  
  return nd;
}  /* End of msr_unpack_int_16() */


/************************************************************************
 *  msr_unpack_int_32:							*
 *	Unpack int_32 miniSEED data and place in supplied buffer.	*
 *  Return: # of samples returned.                                      *
 ************************************************************************/
int msr_unpack_int_32
 (int32_t      *ibuf,		/* ptr to input data.			*/
  int		num_samples,	/* number of data samples in all frames.*/
  int		req_samples,	/* number of data desired by caller.	*/
  int32_t      *databuff,	/* ptr to unpacked data array.		*/
  int		swapflag)	/* if data should be swapped.	        */
{
  int		nd = 0;		/* # of data points in packet.		*/
  int32_t    	itmp;
  
  if (num_samples < 0) return 0;
  if (req_samples < 0) return 0;
  
  for (nd=0; nd<req_samples && nd<num_samples; nd++) {
    itmp = ibuf[nd];
    if ( swapflag) gswap4a (&itmp);
    databuff[nd] = itmp;
  }
  
  return nd;
}  /* End of msr_unpack_int_32() */


/************************************************************************
 *  msr_unpack_float_32:	       				 	*
 *	Unpack float_32 miniSEED data and place in supplied buffer.	*
 *  Return: # of samples returned.                                      *
 ************************************************************************/
int msr_unpack_float_32
 (float	       *fbuf,		/* ptr to input data.			*/
  int		num_samples,	/* number of data samples in all frames.*/
  int		req_samples,	/* number of data desired by caller.	*/
  float	       *databuff,	/* ptr to unpacked data array.		*/
  int		swapflag)	/* if data should be swapped.	        */
{
  int		nd = 0;		/* # of data points in packet.		*/
  float    	ftmp;
  
  if (num_samples < 0) return 0;
  if (req_samples < 0) return 0;
  
  for (nd=0; nd<req_samples && nd<num_samples; nd++) {
    ftmp = fbuf[nd];
    if ( swapflag ) gswap4a (&ftmp);
    databuff[nd] = ftmp;
  }
  
  return nd;
}  /* End of msr_unpack_float_32() */


/************************************************************************
 *  msr_unpack_float_64:	       					*
 *	Unpack float_64 miniSEED data and place in supplied buffer.	*
 *  Return: # of samples returned.                                       *
 ************************************************************************/
int msr_unpack_float_64
 (double       *fbuf,		/* ptr to input data.			*/
  int		num_samples,	/* number of data samples in all frames.*/
  int		req_samples,	/* number of data desired by caller.	*/
  double       *databuff,	/* ptr to unpacked data array.		*/
  int		swapflag)	/* if data should be swapped.	        */
{
  int		nd = 0;		/* # of data points in packet.		*/
  double  	dtmp;
  
  if (num_samples < 0) return 0;
  if (req_samples < 0) return 0;
  
  for (nd=0; nd<req_samples && nd<num_samples; nd++) {
    dtmp = fbuf[nd];
    if ( swapflag ) gswap8a (&dtmp);
    databuff[nd] = dtmp;
  }
  
  return nd;
}  /* End of msr_unpack_float_64() */


/************************************************************************
 *  msr_unpack_steim1:							*
 *	Unpack STEIM1 data frames and place in supplied buffer.		*
 *	Data is divided into frames.                                    *
 *                                                                      *
 *  Return: # of samples returned or negative error code.               *
 ************************************************************************/
int msr_unpack_steim1
 (FRAME	       *pf,		/* ptr to Steim1 data frames.		*/
  int		nbytes,		/* number of bytes in all data frames.	*/
  int		num_samples,	/* number of data samples in all frames.*/
  int		req_samples,	/* number of data desired by caller.	*/
  int32_t      *databuff,	/* ptr to unpacked data array.		*/
  int32_t      *diffbuff,	/* ptr to unpacked diff array.		*/
  int32_t      *px0,		/* return X0, first sample in frame.	*/
  int32_t      *pxn,		/* return XN, last sample in frame.	*/
  int		swapflag,	/* if data should be swapped.	        */
  int           verbose)
{
  int32_t      *diff = diffbuff;
  int32_t      *data = databuff;
  int32_t      *prev;
  int	        num_data_frames = nbytes / sizeof(FRAME);
  int		nd = 0;		/* # of data points in packet.		*/
  int		fn;		/* current frame number.		*/
  int		wn;		/* current work number in the frame.	*/
  int		compflag;      	/* current compression flag.		*/
  int		nr, i;
  int32_t	last_data;
  int32_t	itmp;
  int16_t	stmp;
  uint32_t	ctrl;
  
  if (num_samples < 0) return 0;
  if (num_samples == 0) return 0;
  if (req_samples < 0) return 0;
  
  /* Extract forward and reverse integration constants in first frame */
  *px0 = X0;
  *pxn = XN;
  
  if ( swapflag )
    {
      gswap4 (px0);
      gswap4 (pxn);
    }
  
  if ( verbose > 2 )
    fprintf (stderr, "forward/reverse integration constants:\nX0: %d  XN: %d\n",
	     *px0, *pxn);
  
  /* Decode compressed data in each frame */
  for (fn = 0; fn < num_data_frames; fn++)
    {
      
      ctrl = pf->ctrl;
      if ( swapflag ) gswap4 (&ctrl);

      for (wn = 0; wn < VALS_PER_FRAME; wn++)
	{
	  if (nd >= num_samples) break;
	  
	  compflag = (ctrl >> ((VALS_PER_FRAME-wn-1)*2)) & 0x3;
	  
	  switch (compflag)
	    {
	      
	    case STEIM1_SPECIAL_MASK:
	      /* Headers info -- skip it */
	      break;
	      
	    case STEIM1_BYTE_MASK:
	      /* Next 4 bytes are 4 1-byte differences */
	      for (i=0; i < 4 && nd < num_samples; i++, nd++)
		*diff++ = pf->w[wn].byte[i];
	      break;
	      
	    case STEIM1_HALFWORD_MASK:
	      /* Next 4 bytes are 2 2-byte differences */
	      for (i=0; i < 2 && nd < num_samples; i++, nd++)
		{
		  if (swapflag)
		    {
		      stmp = pf->w[wn].hw[i];
		      if ( swapflag ) gswap2 (&stmp);
		      *diff++ = stmp;
		    }
		  else *diff++ = pf->w[wn].hw[i];
		}
	      break;
	      
	    case STEIM1_FULLWORD_MASK:
	      /* Next 4 bytes are 1 4-byte difference */
	      if (swapflag)
		{
		  itmp = pf->w[wn].fw;
		  if ( swapflag ) gswap4 (&itmp);
		  *diff++ = itmp;
		}
	      else *diff++ = pf->w[wn].fw;
	      nd++;
	      break;
	      
	    default:
	      /* Should NEVER get here */
	      fprintf (stderr, "msr_unpack_steim1(): invalid compression flag = %d\n", compflag);
	      return MS_STBADCOMPFLAG;
	    }
	}
      ++pf;
    }
  
  /* Test if the number of samples implied by the data frames is the
   * same number indicated in the header.
   */
  if ( nd != num_samples )
    {
      fprintf (stderr, "msr_unpack_steim1(): number of samples indicated in header (%d) does not equal data (%d)\n",
	       num_samples, nd);
    }
  
  /*	For now, assume sample count in header to be correct.		*/
  /*	One way of "trimming" data from a block is simply to reduce	*/
  /*	the sample count.  It is not clear from the documentation	*/
  /*	whether this is a valid or not, but it appears to be done	*/
  /*	by other program, so we should not complain about its effect.	*/
  
  nr = req_samples;
  
  /* Compute first value based on last_value from previous buffer.	*/
  /* The two should correspond in all cases EXCEPT for the first	*/
  /* record for each component (because we don't have a valid xn from	*/
  /* a previous record).  Although the Steim compression algorithm	*/
  /* defines x(-1) as 0 for the first record, this only works for the	*/
  /* first record created since coldstart of the datalogger, NOT the	*/
  /* first record of an arbitrary starting record for an event.	*/
  
  /* In all cases, assume x0 is correct, since we don't have x(-1).	*/
  data = databuff;
  diff = diffbuff;
  last_data = *px0;
  if (nr > 0)
    *data = *px0;
  
  /* Compute all but first values based on previous value               */
  prev = data - 1;
  while (--nr > 0 && --nd > 0)
    last_data = *++data = *++diff + *++prev;
  
  /* If a short count was requested compute the last sample in order    */
  /* to perform the integrity check comparison                          */
  while (--nd > 0)
    last_data = *++diff + last_data;
  
  /* Verify that the last value is identical to xn = rev. int. constant */
  if (last_data != *pxn)
    {
      fprintf (stderr, "Data integrity check for Steim-1 failed, last_data=%d, xn=%d\n",
	       last_data, *pxn);
    }
  
  return ((req_samples < num_samples) ? req_samples : num_samples);
}  /* End of msr_unpack_steim1() */


/************************************************************************
 *  msr_unpack_steim2:							*
 *	Unpack STEIM2 data frames and place in supplied buffer.		*
 *	Data is divided into frames.                                    *
 *                                                                      *
 *  Return: # of samples returned or negative error code.               *
 ************************************************************************/
int msr_unpack_steim2 
 (FRAME	       *pf,		/* ptr to Steim2 data frames.		*/
  int		nbytes,		/* number of bytes in all data frames.	*/
  int		num_samples,	/* number of data samples in all frames.*/
  int		req_samples,	/* number of data desired by caller.	*/
  int32_t      *databuff,	/* ptr to unpacked data array.		*/
  int32_t      *diffbuff,	/* ptr to unpacked diff array.		*/
  int32_t      *px0,		/* return X0, first sample in frame.	*/
  int32_t      *pxn,		/* return XN, last sample in frame.	*/
  int		swapflag,	/* if data should be swapped.	        */
  int           verbose)
{
  int32_t      *diff = diffbuff;
  int32_t      *data = databuff;
  int32_t      *prev;
  int		num_data_frames = nbytes / sizeof(FRAME);
  int		nd = 0;		/* # of data points in packet.		*/
  int		fn;		/* current frame number.		*/
  int		wn;		/* current work number in the frame.	*/
  int		compflag;     	/* current compression flag.		*/
  int		nr, i;
  int		n, bits, m1, m2;
  int32_t	last_data;
  int32_t    	val;
  int8_t	dnib;
  uint32_t	ctrl;
  
  if (num_samples < 0) return 0;
  if (num_samples == 0) return 0;
  if (req_samples < 0) return 0;
  
  /* Extract forward and reverse integration constants in first frame.*/
  *px0 = X0;
  *pxn = XN;
  
  if ( swapflag )
    {
      gswap4 (px0);
      gswap4 (pxn);
    }
  
  if ( verbose > 2 )
    fprintf (stderr, "forward/reverse integration constants:\nX0: %d  XN: %d\n",
	     *px0, *pxn);
  
  /* Decode compressed data in each frame */
  for (fn = 0; fn < num_data_frames; fn++)
    {
      
      ctrl = pf->ctrl;
      if ( swapflag ) gswap4 (&ctrl);
      
      for (wn = 0; wn < VALS_PER_FRAME; wn++)
	{
	  if (nd >= num_samples) break;
	  
	  compflag = (ctrl >> ((VALS_PER_FRAME-wn-1)*2)) & 0x3;
	  
	  switch (compflag)
	    {
	    case STEIM2_SPECIAL_MASK:
	      /* Headers info -- skip it */
	      break;
	      
	    case STEIM2_BYTE_MASK:
	      /* Next 4 bytes are 4 1-byte differences */
	      for (i=0; i < 4 && nd < num_samples; i++, nd++)
		*diff++ = pf->w[wn].byte[i];
	      break;
	      
	    case STEIM2_123_MASK:
	      val = pf->w[wn].fw;
	      if ( swapflag ) gswap4 (&val);
	      dnib =  val >> 30 & 0x3;
	      switch (dnib)
		{
		case 1:	/* 1 30-bit difference */
		  bits = 30; n = 1; m1 = 0x3fffffff; m2 = 0x20000000; break;
		case 2:	/* 2 15-bit differences */
		  bits = 15; n = 2; m1 = 0x00007fff; m2 = 0x00004000; break;
		case 3:	/* 3 10-bit differences */
		  bits = 10; n = 3; m1 = 0x000003ff; m2 = 0x00000200; break;
		default:	/*  should NEVER get here  */
		  fprintf(stderr, "msr_unpack_steim2(): invalid compflag, dnib, fn, wn = %d, %d, %d, %d\n", 
			  compflag, dnib, fn, wn);
		  return MS_STBADCOMPFLAG;
		}
	      /*  Uncompress the differences */
	      for (i=(n-1)*bits; i >= 0 && nd < num_samples; i-=bits, nd++)
		{
		  *diff = (val >> i) & m1;
		  *diff = (*diff & m2) ? *diff | ~m1 : *diff;
		  diff++;
		}
	      break;
	      
	    case STEIM2_567_MASK:
	      val = pf->w[wn].fw;
	      if ( swapflag ) gswap4 (&val);
	      dnib =  val >> 30 & 0x3;
	      switch (dnib)
		{
		case 0:	/*  5 6-bit differences  */
		  bits = 6; n = 5; m1 = 0x0000003f; m2 = 0x00000020; break;
		case 1:	/*  6 5-bit differences  */
		  bits = 5; n = 6; m1 = 0x0000001f; m2 = 0x00000010; break;
		case 2:	/*  7 4-bit differences  */
		  bits = 4; n = 7; m1 = 0x0000000f; m2 = 0x00000008; break;
		default:
		  fprintf (stderr, "msr_unpack_steim2(): invalid compflag, dnib, fn, wn = %d, %d, %d, %d\n", 
			   compflag, dnib, fn, wn);
		  return MS_STBADCOMPFLAG;
		}
	      /* Uncompress the differences */
	      for (i=(n-1)*bits; i >= 0 && nd < num_samples; i-=bits, nd++)
		{
		  *diff = (val >> i) & m1;
		  *diff = (*diff & m2) ? *diff | ~m1 : *diff;
		  diff++;
		}
	      break;
	      
	    default:
	      /* Should NEVER get here */
	      fprintf (stderr, "msr_unpack_steim2(): invalid compflag, fn, wn = %d, %d, %d - nsamp: %d\n",
		       compflag, fn, wn, nd);
	      return MS_STBADCOMPFLAG;
	    }
	}
      ++pf;
    }
    
  /* Test if the number of samples implied by the data frames is the
   * same number indicated in the header.
   */
  if ( nd != num_samples )
    {
      fprintf (stderr, "msr_unpack_steim2(): number of samples indicated in header (%d) does not equal data (%d)\n",
	       num_samples, nd);
    }

  /*	For now, assume sample count in header to be correct.		*/
  /*	One way of "trimming" data from a block is simply to reduce	*/
  /*	the sample count.  It is not clear from the documentation	*/
  /*	whether this is a valid or not, but it appears to be done	*/
  /*	by other program, so we should not complain about its effect.	*/

  nr = req_samples;

  /* Compute first value based on last_value from previous buffer.	*/
  /* The two should correspond in all cases EXCEPT for the first	*/
  /* record for each component (because we don't have a valid xn from	*/
  /* a previous record).  Although the Steim compression algorithm	*/
  /* defines x(-1) as 0 for the first record, this only works for the	*/
  /* first record created since coldstart of the datalogger, NOT the	*/
  /* first record of an arbitrary starting record for an event.	*/

  /* In all cases, assume x0 is correct, since we don't have x(-1).	*/
  data = databuff;
  diff = diffbuff;
  last_data = *px0;
  if (nr > 0)
    *data = *px0;

  /* Compute all but first values based on previous value               */
  prev = data - 1;
  while (--nr > 0 && --nd > 0)
    last_data = *++data = *++diff + *++prev;

  /* If a short count was requested compute the last sample in order    */
  /* to perform the integrity check comparison                          */
  while (--nd > 0)
    last_data = *++diff + last_data;
  
  /* Verify that the last value is identical to xn = rev. int. constant */
  if (last_data != *pxn)
    {
      fprintf (stderr, "Data integrity check for Steim-2 failed, last_data=%d, xn=%d\n",
	       last_data, *pxn);
    }
  
  return ((req_samples < num_samples) ? req_samples : num_samples);
}  /* End of msr_unpack_steim2() */
