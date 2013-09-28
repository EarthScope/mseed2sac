/***************************************************************************
 * fdzipstream.c
 *
 * Create ZIP archives in streaming fashion, writing to a file
 * descriptor.  The output stream (file descriptor) does not need to
 * be seekable and can be a pipe or a network socket.  The entire
 * archive contents does not need to be in memory at once.
 *
 * zlib is required for deflate compression: http://www.zlib.net/
 *
 * What this will do for you:
 *
 * - Create a ZIP archive in a streaming fashion, writing to an output
 *   stream (file descriptor, pipe, network socket) without seeking.
 * - Compress the archive entries (using zlib).
 * - Add ZIP64 structures as needed to support large (>4GB) archives.
 * - Simple creation of ZIP archives even if not streaming.
 *
 * What this will NOT do for you:
 *
 * - Open/close files or sockets.
 * - Support advanced ZIP archive features (e.g. file attributes).
 * - Allow archiving of individual files/entries larger than 4GB, the total
 *    of all files can be larger than 4GB but not individual entries.
 * - Allow every possible compression method.
 * 
 * ZIP archive file/entry modifiation times are stored in UTC.
 *
 * Usage pattern
 *
 * Creating a ZIP archive when entire files/entries are in memory:
 *  zs_init ()
 *    for each entry:
 *      zs_writeentry ()
 *  zs_finish ()
 *  zs_free ()
 *
 * Creating a ZIP archive when files/entries are chunked:
 *  zs_init ()
 *    for each entry:
 *      zs_entrybegin ()
 *        for each chunk of entry:
 *          zs_entrydata()
 *      zs_entryend()
 *  zs_finish ()
 *  zs_free ()
 *
 * Written by CTrabant
 *
 * Modified 2013.9.28
 ***************************************************************************/
/* Allow this code to be skipped by declaring NOFDZIP */
#ifndef NOFDZIP

#define FDZIPVERSION 1.0

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <zlib.h>

#include "fdzipstream.h"

#define BIT_SET(a,b) ((a) |= (1<<(b)))

static int64_t zs_writedata ( ZIPstream *zstream,
			      unsigned char *writeentry,
			      int64_t writesize );
static int64_t zs_deflatedata ( ZIPstream *zstream, ZIPentry *zentry, int flush,
				unsigned char *input, int64_t intputsize,
				unsigned char *output, int64_t outputsize );
static uint32_t zs_datetime_unixtodos ( time_t t );
static void zs_htolx ( void *data, int size );


/***************************************************************************
 * zs_init:
 *
 * Initialize and return an ZIPstream struct. If a pointer to an
 * existing ZIPstream is supplied it will be re-initizlied, otherwise
 * memory will be allocated.
 *
 * Returns a pointer to a ZIPstream struct on success or NULL on error.
 ***************************************************************************/
ZIPstream *
zs_init ( int fd, ZIPstream *zs )
{
  ZIPentry *zentry, *tofree;
  
  if ( ! zs )
    {
      zs = (ZIPstream *) malloc (sizeof(ZIPstream));
    }
  else
    {
      zentry = zs->FirstEntry;
      while ( zentry )
	{
	  tofree = zentry;
	  zentry = zentry->next;
	  free (tofree);
	}
    }
  
  if ( zs == NULL )
    {
      fprintf (stderr, "zs_init: Cannot allocate memory\n");
      return NULL;
    }
  
  memset (zs, 0, sizeof (ZIPstream));
  
  zs->fd = fd;
  
  return zs;
}  /* End of zs_init() */


/***************************************************************************
 * zs_free:
 *
 * Free all memory associated with a ZIPstream including all ZIPentry
 * structures.
 ***************************************************************************/
void
zs_free ( ZIPstream *zs )
{
  ZIPentry *zentry, *tofree;
  
  if ( ! zs )
    return;
  
  zentry = zs->FirstEntry;
  while ( zentry )
    {
      tofree = zentry;
      zentry = zentry->next;
      free (tofree);
    }
  
  free (zs);
  
}  /* End of zs_free() */


/* Helper functions to write little-endian integer values to a
 * specified offset in the ZIPstream buffer and increment offset. */
static void packuint16 (ZIPstream *ZS, int *O, uint16_t V)
{
  memcpy (ZS->buffer+*O, &V, 2);
  zs_htolx(ZS->buffer+*O, 2);
  *O += 2;
}
static void packuint32 (ZIPstream *ZS, int *O, uint32_t V)
{
  memcpy (ZS->buffer+*O, &V, 4);
  zs_htolx(ZS->buffer+*O, 4);
  *O += 4;
}
static void packuint64 (ZIPstream *ZS, int *O, uint64_t V)
{
  memcpy (ZS->buffer+*O, &V, 8);
  zs_htolx(ZS->buffer+*O, 8);
  *O += 8;
}


/***************************************************************************
 * zs_writeentry:
 *
 * Write ZIP archive entry contained in a memory buffer using the
 * specified compression method.
 *
 * The method argument specifies the compression method to be used for
 * this entry.  Possible values:
 *   Z_STORE - no compression
 *   Z_DEFLATE - deflate compression
 *
 * The entry modified time (modtime) is stored in UTC.
 *
 * If specified, writestatus will be set to the output of write() when
 * a write error occurs, otherwise it will be set to 0.
 *
 * Return pointer to ZIPentry on success and NULL on error.
 ***************************************************************************/
ZIPentry *
zs_writeentry ( ZIPstream *zstream, unsigned char *entry, int64_t entrysize,
		char *name, time_t modtime, int method, ssize_t *writestatus )
{
  ZIPentry *zentry;
  unsigned char *writeentry = NULL;
  int64_t writesize = 0;
  int64_t lwritestatus;
  int packed;
  uint32_t u32;
  
  if ( writestatus )
    *writestatus = 0;
  
  if ( ! zstream || ! name )
    return NULL;
  
  if ( entrysize > 0xFFFFFFFF )
    {
      fprintf (stderr, "zs_writeentry(%s): Individual entries cannot exceed %lld bytes\n",
	       (name) ? name : "", (long long) 0xFFFFFFFF);
      return NULL;
    }
  
  /* Allocate and initialize new entry */
  if ( (zentry = (ZIPentry *) calloc (1, sizeof(ZIPentry))) == NULL )
    {
      fprintf (stderr, "zs_writeentry: Cannot allocate memory\n");
      return NULL;
    }
  
  zentry->GeneralFlag = 0;
  u32 = zs_datetime_unixtodos (modtime);
  zentry->DOSDate = (uint16_t) (u32 >> 16);
  zentry->DOSTime = (uint16_t) (u32 & 0xFFFF);
  zentry->CRC32 = crc32 (0L, Z_NULL, 0);
  zentry->CompressedSize = 0;
  zentry->UncompressedSize = 0;
  zentry->LocalHeaderOffset = zstream->WriteOffset;
  strncpy (zentry->Name, name, ZENTRY_NAME_LENGTH - 1);
  zentry->NameLength = strlen (zentry->Name);
  
  /* Add new entry to stream list */
  if ( ! zstream->FirstEntry )
    {
      zstream->FirstEntry = zentry;
      zstream->LastEntry = zentry;
    }
  else
    {
      zstream->LastEntry->next = zentry;
      zstream->LastEntry = zentry;
    }
  zstream->EntryCount++;
  
  /* Calculate, or continue calculation of, CRC32 of original data */
  zentry->CRC32 = crc32 (zentry->CRC32, entry, entrysize);
  
  /* Process entry data depending on method */
  if ( method == ZS_STORE )
    {
      zentry->CompressionMethod = ZS_STORE;
      
      writeentry = entry;
      writesize = entrysize;
      zentry->UncompressedSize += entrysize;
      zentry->CompressedSize += entrysize;
    }
  else if ( method == ZS_DEFLATE )
    {
      zentry->CompressionMethod = ZS_DEFLATE;
      
      /* Allocate deflate zlib stream state & initialize */
      zentry->zlstream.zalloc = Z_NULL;
      zentry->zlstream.zfree = Z_NULL;
      zentry->zlstream.opaque = Z_NULL;
      zentry->zlstream.total_in = 0;
      zentry->zlstream.total_out = 0;
      zentry->zlstream.data_type = Z_BINARY;
      
      if ( deflateInit2 (&zentry->zlstream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
			 -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY) != Z_OK )
	{
	  fprintf (stderr, "zs_writeentry: Error with deflateInit2()\n");
	  return NULL;
	}
      
      /* Determine maximum size of compressed data and allocate buffer */
      writesize = deflateBound (&zentry->zlstream, entrysize);
      
      if ( (writeentry = (unsigned char *) malloc (writesize)) == NULL )
	{
	  fprintf (stderr, "zs_writeentry: Error allocating deflation buffer\n");
	  return NULL;
	}
      
      /* Compress entry data */
      writesize = zs_deflatedata (zstream, zentry, 1, entry, entrysize, writeentry, writesize);
      
      if ( writesize < 0 )
	{
	  fprintf (stderr, "zs_writeentry: Error deflating data\n");
	  return NULL;
	}
      
      zentry->CompressedSize = writesize;
      zentry->UncompressedSize = entrysize;
      
      deflateEnd (&zentry->zlstream);
    }
  else
    {
      fprintf (stderr, "Unrecognized compression method: %d\n", method);
      return NULL;
    }
  
  /* Write the Local File Header */
  packed = 0;
  packuint32 (zstream, &packed, LOCALHEADERSIG);              /* Data Description signature */
  packuint16 (zstream, &packed, 20);                          /* Version needed to extract (2.0) */
  packuint16 (zstream, &packed, zentry->GeneralFlag);         /* General purpose bit flag */
  packuint16 (zstream, &packed, zentry->CompressionMethod);   /* Compression method */
  packuint16 (zstream, &packed, zentry->DOSTime);             /* DOS file modification time */
  packuint16 (zstream, &packed, zentry->DOSDate);             /* DOS file modification date */
  packuint32 (zstream, &packed, zentry->CRC32);               /* CRC-32 value of entry */
  packuint32 (zstream, &packed, zentry->CompressedSize);      /* Compressed entry size */
  packuint32 (zstream, &packed, zentry->UncompressedSize);    /* Uncompressed entry size */
  packuint16 (zstream, &packed, zentry->NameLength);          /* File/entry name length */
  packuint16 (zstream, &packed, 0);                           /* Extra field length */
  /* File/entry name */
  memcpy (zstream->buffer+packed, zentry->Name, zentry->NameLength); packed += zentry->NameLength;
  
  lwritestatus = zs_writedata (zstream, zstream->buffer, packed);
  if ( lwritestatus != packed )
    {
      fprintf (stderr, "Error writing ZIP local header: %s\n", strerror(errno));
      
      if ( writestatus )
	*writestatus = lwritestatus;
      
      return NULL;
    }
  
  /* Write entry data */
  lwritestatus = zs_writedata (zstream, writeentry, writesize);
  if ( lwritestatus != writesize )
    {
      fprintf (stderr, "Error writing ZIP entry data (%d): %s\n",
	       zstream->fd, strerror(errno));
      
      if ( writestatus )
	*writestatus = (ssize_t)lwritestatus;
      
      return NULL;
    }
  
  /* Free memory if allocated in this function */
  if ( writeentry != entry )
    free (writeentry);
  
  return zentry;
}  /* End of zs_writeentry() */


/***************************************************************************
 * zs_entrybegin:
 *
 * Begin a streaming entry by writing a Local File Header to the
 * output stream.  The modtime argument sets the modification time
 * stamp for the entry.
 *
 * The method argument specifies the compression method to be used
 * for this entry.  This argument can be:
 *   Z_STORE   - no compression
 *   Z_DEFLATE - deflate compression
 *
 * The entry modified time (modtime) is stored in UTC.
 *
 * If specified, writestatus will be set to the output of write() when
 * a write error occurs, otherwise it will be set to 0.
 *
 * Return pointer to ZIPentry on success and NULL on error.
 ***************************************************************************/
ZIPentry *
zs_entrybegin ( ZIPstream *zstream, char *name, time_t modtime, int method,
		ssize_t *writestatus )
{
  ZIPentry *zentry;
  int64_t lwritestatus;
  int packed;
  uint32_t u32;
  
  if ( writestatus )
    *writestatus = 0;
  
  if ( ! zstream | ! name )
    return NULL;
  
  /* Allocate and initialize new entry */
  if ( (zentry = (ZIPentry *) calloc (1, sizeof(ZIPentry))) == NULL )
    {
      fprintf (stderr, "zs_entrybegin: Cannot allocate memory\n");
      return NULL;
    }
  
  zentry->GeneralFlag = 0;
  u32 = zs_datetime_unixtodos (modtime);
  zentry->DOSDate = (uint16_t) (u32 >> 16);
  zentry->DOSTime = (uint16_t) (u32 & 0xFFFF);
  zentry->CRC32 = crc32 (0L, Z_NULL, 0);
  zentry->CompressedSize = 0;
  zentry->UncompressedSize = 0;
  zentry->LocalHeaderOffset = zstream->WriteOffset;
  strncpy (zentry->Name, name, ZENTRY_NAME_LENGTH - 1);
  zentry->NameLength = strlen (zentry->Name);
  
  /* Add new entry to stream list */
  if ( ! zstream->FirstEntry )
    {
      zstream->FirstEntry = zentry;
      zstream->LastEntry = zentry;
    }
  else
    {
      zstream->LastEntry->next = zentry;
      zstream->LastEntry = zentry;
    }
  zstream->EntryCount++;
  
  /* Set bit to denote streaming */
  BIT_SET (zentry->GeneralFlag, 3);
  
  /* Process entry data depending on method */
  if ( method == ZS_STORE )
    {
      zentry->CompressionMethod = ZS_STORE;
    }
  else if ( method == ZS_DEFLATE )
    {
      zentry->CompressionMethod = ZS_DEFLATE;
      
      /* Allocate deflate zlib stream state & initialize */
      zentry->zlstream.zalloc = Z_NULL;
      zentry->zlstream.zfree = Z_NULL;
      zentry->zlstream.opaque = Z_NULL;
      zentry->zlstream.total_in = 0;
      zentry->zlstream.total_out = 0;
      zentry->zlstream.data_type = Z_BINARY;
      
      if ( deflateInit2(&zentry->zlstream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
			-MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY) != Z_OK )
	{
	  fprintf (stderr, "zs_beginentry: Error with deflateInit2()\n");
	  return NULL;
	}
    }
  else
    {
      fprintf (stderr, "Unrecognized compression method: %d\n", method);
      return NULL;
    }
  
  /* Write the Local File Header, with zero'd CRC and sizes */
  packed = 0;
  packuint32 (zstream, &packed, LOCALHEADERSIG);              /* Data Description signature */
  packuint16 (zstream, &packed, 20);                          /* Version needed to extract (2.0) */
  packuint16 (zstream, &packed, zentry->GeneralFlag);         /* General purpose bit flag */
  packuint16 (zstream, &packed, zentry->CompressionMethod);   /* Compression method */
  packuint16 (zstream, &packed, zentry->DOSTime);             /* DOS file modification time */
  packuint16 (zstream, &packed, zentry->DOSDate);             /* DOS file modification date */
  packuint32 (zstream, &packed, 0);                           /* CRC-32 value of entry */
  packuint32 (zstream, &packed, 0);                           /* Compressed entry size */
  packuint32 (zstream, &packed, 0);                           /* Uncompressed entry size */
  packuint16 (zstream, &packed, zentry->NameLength);          /* File/entry name length */
  packuint16 (zstream, &packed, 0);                           /* Extra field length */
  /* File/entry name */
  memcpy (zstream->buffer+packed, zentry->Name, zentry->NameLength); packed += zentry->NameLength;
  
  lwritestatus = zs_writedata (zstream, zstream->buffer, packed);
  if ( lwritestatus != packed )
    {
      fprintf (stderr, "Error writing ZIP local header: %s\n", strerror(errno));
      
      if ( writestatus )
	*writestatus = (ssize_t)lwritestatus;
      
      return NULL;
    }
  
  return zentry;
}  /* End of zs_entrybegin() */


/***************************************************************************
 * zs_entrydata:
 *
 * Write a chunk of entry data, of size entrysize, to the output
 * stream according to the parameters already set for the stream and
 * entry.
 *
 * If the call contains the final data for this entry, the flush
 * argument should be set to true (1) to flush internal buffers.  If
 * more data is expected for this stream, the flush argument should be
 * false (0).
 *
 * If specified, writestatus will be set to the output of write() when
 * a write error occurs, otherwise it will be set to 0.
 *
 * Return pointer to ZIPentry on success and NULL on error.
 ***************************************************************************/
ZIPentry *
zs_entrydata ( ZIPstream *zstream, ZIPentry *zentry, unsigned char *entry,
	       int64_t entrysize, int final, ssize_t *writestatus )
{
  unsigned char *writeentry = NULL;
  int64_t inputcount = 0;
  int64_t writesize = 0;
  int64_t lwritestatus;
  int deflatechunk;
  int flush;
  
  if ( writestatus )
    *writestatus = 0;
  
  if ( ! zstream | ! zentry )
    return NULL;
  
  /* Calculate, or continue calculation of, CRC32 */
  zentry->CRC32 = crc32 (zentry->CRC32, (unsigned char *)entry, entrysize);
  
  /* Process entry data depending on method */
  if ( zentry->CompressionMethod == ZS_STORE )
    {
      writeentry = entry;
      writesize = entrysize;
      
      /* Write entry data */
      lwritestatus = zs_writedata (zstream, writeentry, writesize);
      if ( lwritestatus != writesize )
	{
	  fprintf (stderr, "Error writing ZIP entry data (%d): %s\n",
		   zstream->fd, strerror(errno));
	  
	  if ( writestatus )
	    *writestatus = (ssize_t)lwritestatus;
	  
	  return NULL;
	}
      
      zentry->UncompressedSize += entrysize;
      zentry->CompressedSize += entrysize;
    }
  else if ( zentry->CompressionMethod == ZS_DEFLATE )
    {
      while ( inputcount < entrysize )
	{
	  /* Determine input chunk size, maximum is 80% of buffer size */
	  deflatechunk = ((entrysize - inputcount) > (int)(ZS_BUFFER_SIZE * 0.8)) ?
	    (int)(ZS_BUFFER_SIZE * 0.8) : (entrysize - inputcount);
	  
	  writeentry = entry + inputcount;
	  
	  flush = ( final && (inputcount + deflatechunk) >= entrysize ) ? 1 : 0;
	  
	  /* Compress entry chunk */
	  writesize = zs_deflatedata (zstream, zentry, flush, writeentry, deflatechunk,
				      zstream->buffer, ZS_BUFFER_SIZE);
	  
	  if ( writesize < 0 )
	    {
	      fprintf (stderr, "zs_entrydata: Error deflating data\n");
	      return NULL;
	    }
	  
	  /* Write entry data */
	  lwritestatus = zs_writedata (zstream, zstream->buffer, writesize);
	  if ( lwritestatus != writesize )
	    {
	      fprintf (stderr, "Error writing ZIP entry data (%d): %s\n",
		       zstream->fd, strerror(errno));
	      
	      if ( writestatus )
		*writestatus = (ssize_t)lwritestatus;
	      
	      return NULL;
	    }
	  
	  zentry->CompressedSize += writesize;
	  zentry->UncompressedSize += deflatechunk;
	  inputcount += deflatechunk;
	}
    }
  
  return zentry;
}  /* End of zs_entrydata() */


/***************************************************************************
 * zs_entryend:
 *
 * End a streaming entry by writing a Data Description record to
 * output stream.
 *
 * If specified, writestatus will be set to the output of write() when
 * a write error occurs, otherwise it will be set to 0.
 *
 * Return pointer to ZIPentry on success and NULL on error.
 ***************************************************************************/
ZIPentry *
zs_entryend ( ZIPstream *zstream, ZIPentry *zentry, ssize_t *writestatus)
{
  int64_t lwritestatus;
  int packed;
  
  if ( writestatus )
    *writestatus = 0;
  
  if ( ! zstream || ! zentry )
    return NULL;
  
  if ( zentry->CompressionMethod == ZS_DEFLATE )
    {
      deflateEnd (&zentry->zlstream);
    }
  
  /* Write Data Description */
  packed = 0;
  packuint32 (zstream, &packed, DATADESCRIPTIONSIG);       /* Data Description signature */
  packuint32 (zstream, &packed, zentry->CRC32);            /* CRC-32 value of entry */
  packuint32 (zstream, &packed, zentry->CompressedSize);   /* Compressed entry size */
  packuint32 (zstream, &packed, zentry->UncompressedSize); /* Uncompressed entry size */
  
  lwritestatus = zs_writedata (zstream, zstream->buffer, packed);
  if ( lwritestatus != packed )
    {
      fprintf (stderr, "Error writing streaming ZIP data description: %s\n", strerror(errno));
      
      if ( writestatus )
	*writestatus = (ssize_t) lwritestatus;
      
      return NULL;
    }
  
  return zentry;
}  /* End of zs_entryend() */


/***************************************************************************
 * zs_finish:
 *
 * Write end of ZIP archive structures (Central Directory, etc.).
 *
 * ZIP64 structures will be added to the Central Directory when the
 * total length of the archive exceeds 0xFFFFFFFF bytes.
 *
 * If specified, writestatus will be set to the output of write() when
 * a write error occurs, otherwise it will be set to 0.
 *
 * Returns 0 on success and non-zero on error.
 ***************************************************************************/
int
zs_finish ( ZIPstream *zstream, ssize_t *writestatus )
{
  ZIPentry *zentry;
  int64_t lwritestatus;
  int packed;
  
  uint64_t cdsize;
  uint64_t zip64endrecord;
  int zip64 = 0;
  
  if ( writestatus )
    *writestatus = 0;
  
  if ( ! zstream )
    return -1;
  
  /* Store offset of Central Directory */
  zstream->CentralDirectoryOffset = zstream->WriteOffset;
  
  zentry = zstream->FirstEntry;
  while ( zentry )
    {
      zip64 = ( zentry->LocalHeaderOffset > 0xFFFFFFFF ) ? 1 : 0;

      /* Write Central Directory Header, packing into write buffer and swapped to little-endian order */
      packed = 0;
      packuint32 (zstream, &packed, CENTRALHEADERSIG);    /* Central File Header signature */
      packuint16 (zstream, &packed, 0);                   /* Version made by */
      packuint16 (zstream, &packed, 20);                  /* Version needed to extract, (2.0) */
      packuint16 (zstream, &packed, zentry->GeneralFlag); /* General purpose bit flag */
      packuint16 (zstream, &packed, zentry->CompressionMethod); /* Compression method */
      packuint16 (zstream, &packed, zentry->DOSTime);     /* DOS file modification time */
      packuint16 (zstream, &packed, zentry->DOSDate);     /* DOS file modification date */
      packuint32 (zstream, &packed, zentry->CRC32);       /* CRC-32 value of entry */
      packuint32 (zstream, &packed, zentry->CompressedSize); /* Compressed entry size */
      packuint32 (zstream, &packed, zentry->UncompressedSize); /* Uncompressed entry size */
      packuint16 (zstream, &packed, zentry->NameLength);  /* File/entry name length */
      packuint16 (zstream, &packed, ( zip64 ) ? 12 : 0 ); /* Extra field length, switch for ZIP64 */
      packuint16 (zstream, &packed, 0);                   /* File/entry comment length */
      packuint16 (zstream, &packed, 0);                   /* Disk number start */
      packuint16 (zstream, &packed, 0);                   /* Internal file attributes */
      packuint32 (zstream, &packed, 0);                   /* External file attributes */
      packuint32 (zstream, &packed, ( zip64 ) ?
		  0xFFFFFFFF : zentry->LocalHeaderOffset); /* Relative offset of Local Header */
      
      /* File/entry name */
      memcpy (zstream->buffer+packed, zentry->Name, zentry->NameLength); packed += zentry->NameLength;
      
      if ( zip64 )  /* ZIP64 Extra Field */
	{
	  packuint16 (zstream, &packed, 1);      /* Extra field ID, 1 = ZIP64 */
	  packuint16 (zstream, &packed, 8);      /* Extra field data length */
	  packuint64 (zstream, &packed, zentry->LocalHeaderOffset); /* Offset to Local Header */
	}
      
      lwritestatus = zs_writedata (zstream, zstream->buffer, packed);
      if ( lwritestatus != packed )
	{
	  fprintf (stderr, "Error writing ZIP central directory header: %s\n", strerror(errno));
	  
	  if ( writestatus )
	    *writestatus = (ssize_t) lwritestatus;
	  
	  return -1;
	}
      
      zentry = zentry->next;
    }
  
  /* Calculate size of Central Directory */
  cdsize = zstream->WriteOffset - zstream->CentralDirectoryOffset;
  
  /* Add ZIP64 structures if offset to Central Directory is beyond limit */
  if ( zstream->CentralDirectoryOffset > 0xFFFFFFFF )
    {
      /* Note offset of ZIP64 End of Central Directory Record */
      zip64endrecord = zstream->WriteOffset;
      
      /* Write ZIP64 End of Central Directory Record, packing into write buffer and swapped to little-endian order */
      packed = 0;
      packuint32 (zstream, &packed, ZIP64ENDRECORDSIG); /* ZIP64 End of Central Dir record */
      packuint64 (zstream, &packed, 44);                /* Size of this record after this field */
      packuint16 (zstream, &packed, 30);                /* Version made by */
      packuint16 (zstream, &packed, 45);                /* Version needed to extract */
      packuint32 (zstream, &packed, 0);                 /* Number of this disk */
      packuint32 (zstream, &packed, 0);                 /* Disk with start of the CD */
      packuint64 (zstream, &packed, zstream->EntryCount); /* Number of CD entries on this disk */
      packuint64 (zstream, &packed, zstream->EntryCount); /* Total number of CD entries */
      packuint64 (zstream, &packed, cdsize);            /* Size of Central Directory */
      packuint64 (zstream, &packed, zstream->CentralDirectoryOffset); /* Offset to Central Directory */
      
      lwritestatus = zs_writedata (zstream, zstream->buffer, packed);
      if ( lwritestatus != packed )
	{
	  fprintf (stderr, "Error writing ZIP64 end of central directory record: %s\n", strerror(errno));
	  
	  if ( writestatus )
	    *writestatus = (ssize_t) lwritestatus;
	  
	  return -1;
	}
      
      /* Write ZIP64 End of Central Directory Locator, packing into write buffer and swapped to little-endian order */
      packed = 0;
      packuint32 (zstream, &packed, ZIP64ENDLOCATORSIG); /* ZIP64 End of Central Dir Locator */
      packuint32 (zstream, &packed, 0);                  /* Number of disk w/ ZIP64 End of CD */
      packuint64 (zstream, &packed, zip64endrecord);     /* Offset to ZIP64 End of CD */
      packuint32 (zstream, &packed, 1);                  /* Total number of disks */
      
      lwritestatus = zs_writedata (zstream, zstream->buffer, packed);
      if ( lwritestatus != packed )
	{
	  fprintf (stderr, "Error writing ZIP64 end of central directory locator: %s\n", strerror(errno));
	  
	  if ( writestatus )
	    *writestatus = (ssize_t) lwritestatus;
	  
	  return -1;
	}
    }
  
  /* Write End of Central Directory Record, packing into write buffer and swapped to little-endian order */
  packed = 0;
  packuint32 (zstream, &packed, ENDHEADERSIG);     /* End of Central Dir signature */
  packuint16 (zstream, &packed, 0);                /* Number of this disk */
  packuint16 (zstream, &packed, 0);                /* Number of disk with CD */
  packuint16 (zstream, &packed, zstream->EntryCount); /* Number of entries in CD this disk */
  packuint16 (zstream, &packed, zstream->EntryCount); /* Number of entries in CD */
  packuint32 (zstream, &packed, cdsize);           /* Size of Central Directory */
  packuint32 (zstream, &packed, (zstream->CentralDirectoryOffset > 0xFFFFFFFF) ?
	      0xFFFFFFFF : zstream->CentralDirectoryOffset); /* Offset to start of CD */
  packuint16 (zstream, &packed, 0);                /* ZIP file comment length */
  
  lwritestatus = zs_writedata (zstream, zstream->buffer, packed);
  if ( lwritestatus != packed )
    {
      fprintf (stderr, "Error writing end of central directory record: %s\n", strerror(errno));
      
      if ( writestatus )
	*writestatus = (ssize_t) lwritestatus;
      
      return -1;
    }
  
  return 0;
}  /* End of zs_finish() */


/***************************************************************************
 * zs_writedata:
 *
 * Write data to output descriptor in blocks of ZS_WRITE_SIZE bytes.
 *
 * The ZIPstream.WriteOffset value will be incremented accordingly.
 *
 * Return number of bytes written on success and return value of
 * write() on error.
 ***************************************************************************/
static int64_t
zs_writedata ( ZIPstream *zstream, unsigned char *writeentry,
	       int64_t writesize )
{
  ssize_t lwritestatus;
  int64_t written;
  size_t writelen;
  
  if ( ! zstream || ! writeentry )
    return 0;
  
  /* Write blocks of ZS_WRITE_SIZE until done */
  written = 0;
  while ( written < writesize )
    {
      writelen = ( (writesize - written) > ZS_WRITE_SIZE ) ? 
	ZS_WRITE_SIZE : (writesize - written);
      
      lwritestatus = write (zstream->fd, writeentry+written, writelen);
      
      if ( lwritestatus != writelen )
	{
	  return lwritestatus;
 	}
      
      zstream->WriteOffset += writelen;      
      written += writelen;
    }
  
  return written;
}  /* End of zs_writedata() */


/***************************************************************************
 * zs_deflatedata:
 *
 * Compress (deflate) input data and write to output buffer.  The zlib
 * stream at zentry.zlstream must already be initilized with
 * deflateInit() or deflateInit2().
 *
 * If the call contains the final data for this stream, the flush
 * argument should be set to true (1) causing the zlib stream to be
 * flushed with the Z_FINISH flag.  If more data is expected for this
 * stream, the flush argument should be false (0).
 *
 * If the input size is greater than ZS_WRITE_SIZE, feed the deflate()
 * call with chunks of input ZS_WRITE_SIZE until complete.
 *
 * Return number of bytes written to output buffer on success and -1
 * on error.
 ***************************************************************************/
static int64_t
zs_deflatedata ( ZIPstream *zstream, ZIPentry *zentry, int flush,
		 unsigned char *input, int64_t inputsize,
		 unsigned char *output, int64_t outputsize )
{
  int64_t inputcount = 0;
  int64_t outputcount = 0;
  int deflatelength;
  int compresslength;
  int rv = Z_OK;
  int flushflag;
  
  if ( ! zstream || ! zentry || ! input || ! output)
    return -1;
  
  while ( inputcount < inputsize )
    {
      /* Determine input deflate length, maximum ZS_WRITE_SIZE */
      deflatelength = ((inputsize - inputcount) > ZS_WRITE_SIZE) ?
	ZS_WRITE_SIZE : (inputsize - inputcount);
      
      /* Input data */
      zentry->zlstream.next_in = input + inputcount;
      zentry->zlstream.avail_in = deflatelength;
      
      flushflag = ( flush && (inputcount + deflatelength) >= inputsize ) ? Z_FINISH : Z_NO_FLUSH;
      
      do
	{
	  /* Output buffer, allow up to ZS_WRITE_SIZE per call */
	  zentry->zlstream.next_out = output + outputcount;
	  compresslength = ((outputsize - outputcount) > ZS_WRITE_SIZE) ?
	    ZS_WRITE_SIZE : (outputsize - outputcount);
	  zentry->zlstream.avail_out = compresslength;
	  
	  if ( compresslength <= 0 )
	    {
	      fprintf (stderr, "zs_deflatedata: Output buffer not large enough for %s\n",
		       zentry->Name);
	      return -1;
	    }
	  
	  rv = deflate (&zentry->zlstream, flushflag);
	  
	  if ( rv == Z_STREAM_ERROR )
	    {
	      fprintf (stderr, "zs_deflatedata: Error in deflate() for entry: %s\n",
		       zentry->Name);
	      return -1;
	    }
	  
	  outputcount += compresslength - zentry->zlstream.avail_out;
	  
	} while ( zentry->zlstream.avail_out == 0 );
      
      inputcount += deflatelength;
    }
  
  if ( (flush && rv != Z_STREAM_END) ||
       (! flush && rv != Z_OK) )
    {
      fprintf (stderr, "zs_deflatedata: Error with deflate(): %d\n", rv);
      return -1;
    }
  
  return outputcount;
}  /* End of zs_deflatedata() */


/* DOS time start date is January 1, 1980 */
#define DOSTIME_STARTDATE  0x00210000L

/***************************************************************************
 * zs_datetime_unixtodos:
 *
 * Convert Unix time_t to 4 byte DOS date and time.
 *
 * Routine adapted from sources:
 *  Copyright (C) 2006 Michael Liebscher <johnnycanuck@users.sourceforge.net>
 *
 * Return converted 4-byte quantity on success and 0 on error.
 ***************************************************************************/
static uint32_t
zs_datetime_unixtodos ( time_t t )
{
  struct tm s;         
  
  if ( gmtime_r (&t, &s) == NULL ) 
    return 0;
  
  s.tm_year += 1900;
  s.tm_mon += 1;
  
  return ( ((s.tm_year) < 1980) ? DOSTIME_STARTDATE :
	   (((uint32_t)(s.tm_year) - 1980) << 25) |
	   ((uint32_t)(s.tm_mon) << 21) |
	   ((uint32_t)(s.tm_mday) << 16) |
	   ((uint32_t)(s.tm_hour) << 11) |
	   ((uint32_t)(s.tm_min) << 5) |
	   ((uint32_t)(s.tm_sec) >> 1) );
}


/***************************************************************************
 * Byte swapping routine:
 *
 * Functions for generalized, in-place byte swapping from host order
 * to little-endian.  A run-time test of byte order is conducted on
 * the first usage and a static variable is used to store the result
 * for later use.
 *
 * The byte-swapping requires memory-aligned quantities.
 *
 ***************************************************************************/
static void
zs_htolx ( void *data, int size )
{
  static int le = -1;
  int16_t host = 1;
  
  uint16_t *data2;
  uint32_t *data4;
  uint32_t h0, h1;
  
  /* Determine byte order, test for little-endianness */
  if ( le < 0 )
    {
      le = (*((int8_t *)(&host)));
    }
  
  /* Swap bytes if not little-endian, requires memory-aligned quantities */
  if ( le == 0 )
    {
      switch ( size )
	{
	case 2:
	  data2 = (uint16_t *) data;
	  *data2=(((*data2>>8)&0xff) | ((*data2&0xff)<<8));
	  break;
	case 4:
	  data4 = (uint32_t *) data;
	  *data4=(((*data4>>24)&0xff) | ((*data4&0xff)<<24) |
		  ((*data4>>8)&0xff00) | ((*data4&0xff00)<<8));
	  break;
	case 8:
	  data4 = (uint32_t *) data;
	  
	  h0 = data4[0];
	  h0 = (((h0>>24)&0xff) | ((h0&0xff)<<24) |
		((h0>>8)&0xff00) | ((h0&0xff00)<<8));
	  
	  h1 = data4[1];
	  h1 = (((h1>>24)&0xff) | ((h1&0xff)<<24) |
		((h1>>8)&0xff00) | ((h1&0xff00)<<8));
	  
	  data4[0] = h1;
	  data4[1] = h0;
	  break;
	}
    }
}

#endif /* NOFDZIP */
