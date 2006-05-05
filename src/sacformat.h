#ifndef SAC_FORMAT_H
#define SAC_FORMAT_H

#ifdef __cplusplus
extern "C" {
#endif

#define REGCONV 100

#define SACHEADERLEN 632  /* SAC header length in bytes (only version 6?) */
#define NUMFLOATHDR 70    /* Number of float header variables, 4 bytes each */
#define NUMINTHDR 40      /* Number of integer header variables, 4 bytes each */
#define NUMSTRHDR 23      /* Number of string header variables, 22x8 bytes + 1x16 bytes */

/* Undefined values for float, integer and string header variables */
#define FUNDEF -12345.0
#define IUNDEF -12345
#define SUNDEF "-1234.0  "

/* SAC header structure as it exists in binary SAC files */
struct SACHeader
{
	float	delta;			/* RF time increment, sec    */
	float	depmin;			/*    minimum amplitude      */
	float	depmax;			/*    maximum amplitude      */
	float	scale;			/*    amplitude scale factor */
	float	odelta;			/*    observed time inc      */
	float	b;			/* RD initial value, time    */
	float	e;			/* RD final value, time      */
	float	o;			/*    event start, sec < nz. */
	float	a;			/*    1st arrival time       */
	float	fmt;			/*    internal use           */
	float	t0;			/*    user-defined time pick */
	float	t1;			/*    user-defined time pick */
	float	t2;			/*    user-defined time pick */
	float	t3;			/*    user-defined time pick */
	float	t4;			/*    user-defined time pick */
	float	t5;			/*    user-defined time pick */
	float	t6;			/*    user-defined time pick */
	float	t7;			/*    user-defined time pick */
	float	t8;			/*    user-defined time pick */
	float	t9;			/*    user-defined time pick */
	float	f;			/*    event end, sec > nz    */
	float	resp0;			/*    instrument respnse parm*/
	float	resp1;			/*    instrument respnse parm*/
	float	resp2;			/*    instrument respnse parm*/
	float	resp3;			/*    instrument respnse parm*/
	float	resp4;			/*    instrument respnse parm*/
	float	resp5;			/*    instrument respnse parm*/
	float	resp6;			/*    instrument respnse parm*/
	float	resp7;			/*    instrument respnse parm*/
	float	resp8;			/*    instrument respnse parm*/
	float	resp9;			/*    instrument respnse parm*/
	float	stla;			/*  T station latititude     */
	float	stlo;			/*  T station longitude      */
	float	stel;			/*  T station elevation, m   */
	float	stdp;			/*  T station depth, m      */
	float	evla;			/*    event latitude         */
	float	evlo;			/*    event longitude        */
	float	evel;			/*    event elevation        */
	float	evdp;			/*    event depth            */
	float	mag;		/*    reserved for future use*/
	float	user0;			/*    available to user      */
	float	user1;			/*    available to user      */
	float	user2;			/*    available to user      */
	float	user3;			/*    available to user      */
	float	user4;			/*    available to user      */
	float	user5;			/*    available to user      */
	float	user6;			/*    available to user      */
	float	user7;			/*    available to user      */
	float	user8;			/*    available to user      */
	float	user9;			/*    available to user      */
	float	dist;			/*    stn-event distance, km */
	float	az;			/*    event-stn azimuth      */
	float	baz;			/*    stn-event azimuth      */
	float	gcarc;			/*    stn-event dist, degrees*/
	float	sb;			/*    internal use           */
	float	sdelta;			/*    internal use           */
	float	depmen;			/*    mean value, amplitude  */
	float	cmpaz;			/*  T component azimuth     */
	float	cmpinc;			/*  T component inclination */
	float	xminimum;		/*    reserved for future use*/
	float	xmaximum;		/*    reserved for future use*/
	float	yminimum;		/*    reserved for future use*/
	float	ymaximum;		/*    reserved for future use*/
	float	unused6;		/*    reserved for future use*/
	float	unused7;		/*    reserved for future use*/
	float	unused8;		/*    reserved for future use*/
	float	unused9;		/*    reserved for future use*/
	float	unused10;		/*    reserved for future use*/
	float	unused11;		/*    reserved for future use*/
	float	unused12;		/*    reserved for future use*/
	int32_t nzyear;			/*  F zero time of file, yr  */
	int32_t nzjday;			/*  F zero time of file, day */
	int32_t nzhour;			/*  F zero time of file, hr  */
	int32_t nzmin;			/*  F zero time of file, min */
	int32_t nzsec;			/*  F zero time of file, sec */
	int32_t nzmsec;			/*  F zero time of file, millisec*/
	int32_t nvhdr;			/*    internal use (version) */
	int32_t norid;			/*    origin ID              */
	int32_t nevid;			/*    event ID               */
	int32_t npts;			/* RF number of samples      */
	int32_t nsnpts;			/*    internal use           */
	int32_t nwfid;			/*    waveform ID            */
	int32_t nxsize;			/*    reserved for future use*/
	int32_t nysize;			/*    reserved for future use*/
	int32_t unused15;		/*    reserved for future use*/
	int32_t iftype;			/* RA type of file          */
	int32_t idep;			/*    type of amplitude      */
	int32_t iztype;			/*    zero time equivalence  */
	int32_t unused16;		/*    reserved for future use*/
	int32_t iinst;			/*    recording instrument   */
	int32_t istreg;			/*    stn geographic region  */
	int32_t ievreg;			/*    event geographic region*/
	int32_t ievtyp;			/*    event type             */
	int32_t iqual;			/*    quality of data        */
	int32_t isynth;			/*    synthetic data flag    */
	int32_t imagtyp;		/*    reserved for future use*/
	int32_t imagsrc;		/*    reserved for future use*/
	int32_t unused19;		/*    reserved for future use*/
	int32_t unused20;		/*    reserved for future use*/
	int32_t unused21;		/*    reserved for future use*/
	int32_t unused22;		/*    reserved for future use*/
	int32_t unused23;		/*    reserved for future use*/
	int32_t unused24;		/*    reserved for future use*/
	int32_t unused25;		/*    reserved for future use*/
	int32_t unused26;		/*    reserved for future use*/
	int32_t leven;			/* RA data-evenly-spaced flag*/
	int32_t lpspol;			/*    station polarity flag  */
	int32_t lovrok;			/*    overwrite permission   */
	int32_t lcalda;			/*    calc distance, azimuth */
	int32_t unused27;		/*    reserved for future use*/
	char	kstnm[8];		/*  F station name           */
	char	kevnm[16];		/*    event name             */
	char	khole[8];		/*    man-made event name    */
	char	ko[8];			/*    event origin time id   */
	char	ka[8];			/*    1st arrival time ident */
	char	kt0[8];			/*    time pick 0 ident      */
	char	kt1[8];			/*    time pick 1 ident      */
	char	kt2[8];			/*    time pick 2 ident      */
	char	kt3[8];			/*    time pick 3 ident      */
	char	kt4[8];			/*    time pick 4 ident      */
	char	kt5[8];			/*    time pick 5 ident      */
	char	kt6[8];			/*    time pick 6 ident      */
	char	kt7[8];			/*    time pick 7 ident      */
	char	kt8[8];			/*    time pick 8 ident      */
	char	kt9[8];			/*    time pick 9 ident      */
	char	kf[8];			/*    end of event ident     */
	char	kuser0[8];		/*    available to user      */
	char	kuser1[8];		/*    available to user      */
	char	kuser2[8];		/*    available to user      */
	char	kcmpnm[8];		/*  F component name         */
	char	knetwk[8];		/*    network name           */
	char	kdatrd[8];		/*    date data read         */
	char	kinst[8];		/*    instrument name        */
};

/* A SAC header null value initializer 
 * Usage: struct SACHeader sh = NullSACHeader; */
#define NullSACHeader {                                                          \
        -12345., -12345., -12345., -12345., -12345.,                             \
        -12345., -12345., -12345., -12345., -12345.,                             \
        -12345., -12345., -12345., -12345., -12345.,                             \
        -12345., -12345., -12345., -12345., -12345.,                             \
        -12345., -12345., -12345., -12345., -12345.,                             \
        -12345., -12345., -12345., -12345., -12345.,                             \
        -12345., -12345., -12345., -12345., -12345.,                             \
        -12345., -12345., -12345., -12345., -12345.,                             \
        -12345., -12345., -12345., -12345., -12345.,                             \
        -12345., -12345., -12345., -12345., -12345.,                             \
        -12345., -12345., -12345., -12345., -12345.,                             \
        -12345., -12345., -12345., -12345., -12345.,                             \
        -12345., -12345., -12345., -12345., -12345.,                             \
        -12345., -12345., -12345., -12345., -12345.,                             \
        -12345, -12345, -12345, -12345, -12345,                                  \
        -12345, -12345, -12345, -12345, -12345,                                  \
        -12345, -12345, -12345, -12345, -12345,                                  \
        -12345, -12345, -12345, -12345, -12345,                                  \
        -12345, -12345, -12345, -12345, -12345,                                  \
        -12345, -12345, -12345, -12345, -12345,                                  \
        -12345, -12345, -12345, -12345, -12345,                                  \
        -12345, -12345, -12345, -12345, -12345,                                  \
        { '-','1','2','3','4','5',' ',' ' },                                     \
        { '-','1','2','3','4','5',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ' },     \
        { '-','1','2','3','4','5',' ',' ' },{ '-','1','2','3','4','5',' ',' ' }, \
        { '-','1','2','3','4','5',' ',' ' },{ '-','1','2','3','4','5',' ',' ' }, \
        { '-','1','2','3','4','5',' ',' ' },{ '-','1','2','3','4','5',' ',' ' }, \
        { '-','1','2','3','4','5',' ',' ' },{ '-','1','2','3','4','5',' ',' ' }, \
        { '-','1','2','3','4','5',' ',' ' },{ '-','1','2','3','4','5',' ',' ' }, \
        { '-','1','2','3','4','5',' ',' ' },{ '-','1','2','3','4','5',' ',' ' }, \
        { '-','1','2','3','4','5',' ',' ' },{ '-','1','2','3','4','5',' ',' ' }, \
        { '-','1','2','3','4','5',' ',' ' },{ '-','1','2','3','4','5',' ',' ' }, \
        { '-','1','2','3','4','5',' ',' ' },{ '-','1','2','3','4','5',' ',' ' }, \
        { '-','1','2','3','4','5',' ',' ' },{ '-','1','2','3','4','5',' ',' ' }, \
        { '-','1','2','3','4','5',' ',' ' }                                      \
};


/* definitions of constants for SAC enumerated data values */
#define IREAL   0			/* undocumented              */
#define ITIME   1			/* file: time series data    */
#define IRLIM   2			/* file: real&imag spectrum  */
#define IAMPH   3			/* file: ampl&phas spectrum  */
#define IXY     4			/* file: gen'l x vs y data   */
#define IUNKN   5			/* x data: unknown type      */
					/* zero time: unknown        */
					/* event type: unknown       */
#define IDISP   6			/* x data: displacement (nm) */
#define IVEL    7			/* x data: velocity (nm/sec) */
#define IACC    8			/* x data: accel (cm/sec/sec)*/
#define IB      9			/* zero time: start of file  */
#define IDAY   10			/* zero time: 0000 of GMT day*/
#define IO     11			/* zero time: event origin   */
#define IA     12			/* zero time: 1st arrival    */
#define IT0    13			/* zero time: user timepick 0*/
#define IT1    14			/* zero time: user timepick 1*/
#define IT2    15			/* zero time: user timepick 2*/
#define IT3    16			/* zero time: user timepick 3*/
#define IT4    17			/* zero time: user timepick 4*/
#define IT5    18			/* zero time: user timepick 5*/
#define IT6    19			/* zero time: user timepick 6*/
#define IT7    20			/* zero time: user timepick 7*/
#define IT8    21			/* zero time: user timepick 8*/
#define IT9    22			/* zero time: user timepick 9*/
#define IRADNV 23			/* undocumented              */
#define ITANNV 24			/* undocumented              */
#define IRADEV 25			/* undocumented              */
#define ITANEV 26			/* undocumented              */
#define INORTH 27			/* undocumented              */
#define IEAST  28			/* undocumented              */
#define IHORZA 29			/* undocumented              */
#define IDOWN  30			/* undocumented              */
#define IUP    31			/* undocumented              */
#define ILLLBB 32			/* undocumented              */
#define IWWSN1 33			/* undocumented              */
#define IWWSN2 34			/* undocumented              */
#define IHGLP  35			/* undocumented              */
#define ISRO   36			/* undocumented              */

/* Source types */
#define INUCL  37			/* event type: nuclear shot  */
#define IPREN  38			/* event type: nuke pre-shot */
#define IPOSTN 39			/* event type: nuke post-shot*/
#define IQUAKE 40			/* event type: earthquake    */
#define IPREQ  41			/* event type: foreshock     */
#define IPOSTQ 42			/* event type: aftershock    */
#define ICHEM  43			/* event type: chemical expl */
#define IOTHER 44			/* event type: other source  */
#define IQB    72			/* Quarry Blast or mine expl. confirmed by quarry */
#define IQB1   73  /* Quarry or mine blast with designed shot information-ripple fired */
#define IQB2   74  /* Quarry or mine blast with observed shot information-ripple fired */
#define IQBX   75  /* Quarry or mine blast - single shot */
#define IQMT   76  /* Quarry or mining-induced events: tremors and rockbursts */
#define IEQ    77  /* Earthquake */
#define IEQ1   78  /* Earthquakes in a swarm or aftershock sequence */
#define IEQ2   79  /* Felt earthquake */
#define IME    80  /* Marine explosion */
#define IEX    81  /* Other explosion */
#define INU    82  /* Nuclear explosion */
#define INC    83  /* Nuclear cavity collapse */
#define IO_    84  /* Other source of known origin */
#define IL     85  /* Local event of unknown origin */
#define IR     86  /* Regional event of unknown origin */
#define IT     87  /* Teleseismic event of unknown origin */
#define IU     88  /* Undetermined or conflicting information  */
#define IEQ3   89  /* Damaging earthquake */
#define IEQ0   90  /* Probable earthquake */
#define IEX0   91  /* Probable explosion */
#define IQC    92  /* Mine collapse */
#define IQB0   93  /* Probable Mine Blast */
#define IGEY   94  /* Geyser */
#define ILIT   95  /* Light */
#define IMET   96  /* Meteoric Event */
#define IODOR  97  /* Odors */
#define IOS   103 			/* Other source: Known origin*/

					/* data quality: other problm*/
#define IGOOD  45			/* data quality: good        */
#define IGLCH  46			/* data quality: has glitches*/
#define IDROP  47			/* data quality: has dropouts*/
#define ILOWSN 48			/* data quality: low s/n     */

#define IRLDTA 49			/* data is real data         */
#define IVOLTS 50			/* file: velocity (volts)    */

/* Magnitude type and source */
#define IMB    52                      /* Bodywave Magnitude */ 
#define IMS    53                      /* Surface Magnitude */
#define IML    54                      /* Local Magnitude  */ 
#define IMW    55                      /* Moment Magnitude */
#define IMD    56                      /* Duration Magnitude */
#define IMX    57                      /* User Defined Magnitude */
#define INEIC  58                      /* INEIC */
#define IPDEQ  59                      /* IPDE */
#define IPDEW  60                      /* IPDE */
#define IPDE   61                      /* IPDE */

#define IISC   62                      /* IISC */
#define IREB   63                      /* IREB */
#define IUSGS  64                      /* IUSGS */
#define IBRK   65                      /* IBRK */
#define ICALTECH 66                    /* ICALTECH */
#define ILLNL  67                      /* ILLNL */
#define IEVLOC 68                      /* IEVLOC */
#define IJSOP  69                      /* IJSOP */
#define IUSER  70                      /* IUSER */
#define IUNKNOWN 71                    /* IUNKNOWN */


#ifdef __cplusplus
}
#endif

#endif /* SAC_FORMAT_H */
