/*
 * Copyright (C) 2017-2023 Adrien Gesta-Fline
 *
 * This file is part of libAAF.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */



/**
 * @file LibCFB/LibCFB.c
 * @brief Compound File Binary Library
 * @author Adrien Gesta-Fline
 * @version 0.1
 * @date 04 october 2017
 *
 * @ingroup LibCFB
 * @addtogroup LibCFB
 * @{
 * @brief LibCFB is a library that allows to parse any Compound File Binary File Format
 * (a.k.a *Structured Storage File Format*).
 *
 * The specifications of the CFB can be found at https://www.amwa.tv/projects/MS-03.shtml
 *
 * @note When using LibAAF, you should not have to talk to LibCFB directly.\n
 * All the following steps are handled by LibAAF.
 *
 * Interaction with LibCFB is done through the CFB_Data structure, which is allocated
 * with cfb_alloc().
 *
 * The first thing you will need to do in order to parse any CFB file is to allocate
 * CFB_Data with cfb_alloc(). Then, you can "load" a file by calling cfb_load_file().
 * It will open the file for reading, check if it is a regular CFB file, then retrieve
 * all the CFB components (Header, DiFAT, FAT, MiniFAT) needed to later parse the file's
 * directories (nodes) and streams.
 *
 * **Example:**
 * @code
 * CFB_Data *cfbd = cfb_alloc();
 * cfb_load_file( cfbd, "/path/to/file" );
 * @endcode
 *
 * Once the file is loaded, you can access the nodes with the functions
 * cfb_getNodeByPath() and cfb_getChildNode(), and access a node's stream with the
 * functions cfb_getStream() or cfb_foreachSectorInStream(). The former is prefered
 * for small-size streams, since it allocates the entire stream to memory, while the
 * later loops through each sector that composes a stream, better for the big ones.
 *
 * **Example:**
 * @code
 * // Get the root "directory" (node, SID 0) "directly"
 * cfbNode *root = cfbd->nodes[0];
 *
 *
 * // Get a "file" (stream node) called "properties" inside the "Header" directory
 * cfbNode *properties = cfb_getNodeByPath( cfbd, "/Header/properties", 0 );
 *
 *
 * // Get the "properties" node stream
 * unsigned char *stream    = NULL;
 * uint64_t       stream_sz = 0;
 *
 * cfb_getStream( cfbd, properties, &stream, &stream_sz );
 *
 *
 * // Loop through each sector that composes the "properties" stream
 * cfbSectorID_t sectorID = 0;
 *
 * cfb_foreachSectorInStream( cfbd, properties, &stream, &stream_sz, &sectorID )
 * {
 * 	// do stuff..
 * }
 * 	@endcode
 *
 * Finaly, once you are done working with the file, you can close the file and free
 * the CFB_Data and its content by simply calling cfb_release().
 *
 * **Example:**
 * @code
 * cfb_release( &cfbd );
 * @endcode
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>	// ceil()
#include <wchar.h>
#include <inttypes.h>

#include <libaaf/LibCFB.h>
#include <libaaf/debug.h>

#include <libaaf/utils.h>


#define debug( ... ) \
	_dbg( cfbd->dbg, cfbd, DEBUG_SRC_ID_LIB_CFB, VERB_DEBUG, __VA_ARGS__ )

#define warning( ... ) \
	_dbg( cfbd->dbg, cfbd, DEBUG_SRC_ID_LIB_CFB, VERB_WARNING, __VA_ARGS__ )

#define error( ... ) \
	_dbg( cfbd->dbg, cfbd, DEBUG_SRC_ID_LIB_CFB, VERB_ERROR, __VA_ARGS__ )



static int cfb_getFileSize( CFB_Data *cfbd );

static int cfb_openFile( CFB_Data *cfbd );

static uint64_t cfb_readFile( CFB_Data *cfbd, unsigned char *buf, uint64_t offset, uint64_t len );

static void cfb_closeFile( CFB_Data *cfbd );

static int cfb_is_valid( CFB_Data *cfbd );

static int cfb_retrieveFileHeader( CFB_Data *cfbd );

static int cfb_retrieveDiFAT( CFB_Data *cfbd );

static int cfb_retrieveFAT( CFB_Data * cfbd );

static int cfb_retrieveMiniFAT( CFB_Data * cfbd );

static int cfb_retrieveNodes( CFB_Data *cfbd );

static cfbSID_t getNodeCount( CFB_Data *cfbd );

static cfbSID_t cfb_getIDByNode( CFB_Data *cfbd, cfbNode *node );






const wchar_t * cfb_CLSIDToText( const cfbCLSID_t *clsid )
{
	static wchar_t str[96];

	if ( clsid == NULL ) {
		str[0] = 'n';
		str[1] = '/';
		str[2] = 'a';
		str[3] = '\0';
	}
	else {
		swprintf( str, sizeof(str), L"{ 0x%08x 0x%04x 0x%04x { 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x } }",
			clsid->Data1,
			clsid->Data2,
			clsid->Data3,
			clsid->Data4[0],
			clsid->Data4[1],
			clsid->Data4[2],
			clsid->Data4[3],
			clsid->Data4[4],
			clsid->Data4[5],
			clsid->Data4[6],
			clsid->Data4[7] );
	}

	return str;
}



/**
 * Allocates a new CFB_Data structure.
 *
 * @return Pointer to the newly allocated CFB_Data structure.
 */

CFB_Data * cfb_alloc( struct dbg *dbg )
{
	CFB_Data * cfbd = calloc( sizeof(CFB_Data), sizeof(unsigned char) );

	if ( cfbd == NULL ) {
		error( "%s.", strerror(errno) );
		return NULL;
	}

	cfbd->dbg = dbg;

	return cfbd;
}



/**
 * fclose() the file pointer then frees all the components of
 * the CFB_Data structure, including the structure itself.
 *
 * @param cfbd Pointer to Pointer to the CFB_Data structure.
 */

void cfb_release( CFB_Data **cfbd )
{
	if ( cfbd == NULL || *cfbd == NULL )
		return;

	cfb_closeFile( *cfbd );

	if ( (*cfbd)->DiFAT != NULL ) {
		free( (*cfbd)->DiFAT );
		(*cfbd)->DiFAT = NULL;
	}

	if ( (*cfbd)->fat != NULL ) {
		free( (*cfbd)->fat );
		(*cfbd)->fat = NULL;
	}

	if ( (*cfbd)->miniFat != NULL ) {
		free( (*cfbd)->miniFat );
		(*cfbd)->miniFat = NULL;
	}

	if ( (*cfbd)->nodes != NULL ) {
		free( (*cfbd)->nodes );
		(*cfbd)->nodes = NULL;
	}

	if ( (*cfbd)->hdr != NULL ) {
		free( (*cfbd)->hdr );
		(*cfbd)->hdr = NULL;
	}

	free( *cfbd );
	*cfbd = NULL;
}



/**
 * Loads a Compound File Binary File, retrieves its Header, FAT,
 * MiniFAT and Nodes. then sets the CFB_Data structure so it is
 * ready for parsing the file. The user should call cfb_release()
 * once he's done using the file.
 *
 * @param  cfbd     Pointer to the CFB_Data structure.
 * @param  file     Pointer to a NULL terminated string holding the file path.
 *
 * @return          0 on success\n
 *                  1 on error
 */

int cfb_load_file( CFB_Data **cfbd_p, const char * file )
{
	CFB_Data *cfbd = *cfbd_p;
	snprintf( cfbd->file, sizeof(((CFB_Data){0}).file), "%s", file );


	if ( cfb_openFile( cfbd ) < 0 ) {
		cfb_release( cfbd_p );
		return -1;
	}

	if ( cfb_getFileSize( cfbd ) < 0 ) {
		cfb_release( cfbd_p );
		return -1;
	}

	if ( cfb_is_valid( cfbd ) == 0 ) {
		cfb_release( cfbd_p );
		return -1;
	}

	if ( cfb_retrieveFileHeader( cfbd ) < 0 ) {
		error( "Could not retrieve CFB header." );
		cfb_release( cfbd_p );
		return -1;
	}

	if ( cfb_retrieveDiFAT( cfbd ) < 0 ) {
		error( "Could not retrieve CFB DiFAT." );
		cfb_release( cfbd_p );
		return -1;
	}

	if ( cfb_retrieveFAT( cfbd ) < 0 ) {
		error( "Could not retrieve CFB FAT." );
		cfb_release( cfbd_p );
		return -1;
	}

	if ( cfb_retrieveMiniFAT( cfbd ) < 0 ) {
		error( "Could not retrieve CFB MiniFAT." );
		cfb_release( cfbd_p );
		return -1;
	}

	if ( cfb_retrieveNodes( cfbd ) < 0 ) {
		error( "Could not retrieve CFB Nodes." );
		cfb_release( cfbd_p );
		return -1;
	}

	return 0;
}



int cfb_new_file( CFB_Data *cfbd, const char *file, int sectSize )
{
	(void)file;

	if ( sectSize != 512 &&
	     sectSize != 4096 )
	{
		error( "Only standard sector sizes (512 and 4096 bytes) are supported." );
		return -1;
	}

	cfbHeader *hdr = malloc( sizeof(cfbHeader) );

	if ( cfbd->hdr == NULL ) {
		error( "%s.", strerror( errno ) );
		return -1;
	}

	hdr->_abSig = 0xe11ab1a1e011cfd0;

	/*
	 * _uMinorVersion is set to 33 reference implementation.
	 * _uMinorVersion is set to 0x3e in all AAF files
	 */
	hdr->_uMinorVersion = 0x3e;

	hdr->_uDllVersion   = ( sectSize == 512 ) ? 3 : 4;
	hdr->_uByteOrder    = 0xfffe;
	hdr->_uSectorShift  = ( sectSize == 512 ) ? 9 : 12;
	hdr->_uMiniSectorShift = 6;
	hdr->_usReserved  = 0;
	hdr->_ulReserved1 = 0;

	hdr->_csectDir = 0;
	hdr->_csectFat = 0;
	hdr->_sectDirStart = 0;
	hdr->_signature = 0;

	hdr->_ulMiniSectorCutoff = 4096;

	hdr->_sectMiniFatStart = 0;
	hdr->_csectMiniFat = 0;
	hdr->_sectDifStart = 0;
	hdr->_csectDif = 0;

	int i = 0;

	for ( i = 0; i < 109; i++ )
		hdr->_sectFat[i] = CFB_FREE_SECT;

	return 0;
}



/**
 * Ensures the file is a valid Compound File Binary File.
 *
 * @param  cfbd Pointer to the CFB_Data structure.
 *
 * @return      1 if the file is valid,\n
 *              0 otherwise.
 */

static int cfb_is_valid( CFB_Data *cfbd )
{
	uint64_t signature = 0;

	if ( cfbd->file_sz < sizeof(struct StructuredStorageHeader) ) {
		error( "Not a valid Compound File : File size is lower than header size." );
		return 0;
	}

	if ( cfb_readFile( cfbd, (unsigned char*)&signature, 0, sizeof(uint64_t) ) != sizeof(uint64_t) ) {
		return 0;
	}

	if ( signature != 0xe11ab1a1e011cfd0 ) {
		error( "Not a valid Compound File : Wrong signature." );
		return 0;
	}

	return 1;
}



/**
 * Retrieves the Compound File Binary File size.
 *
 * @param  cfbd Pointer to the CFB_Data structure.
 * @return      0.
 */

static int cfb_getFileSize( CFB_Data * cfbd )
{
	if ( fseek( cfbd->fp, 0L, SEEK_END ) < 0 ) {
		error( "fseek() failed : %s.", strerror(errno) );
		return -1;
	}

	cfbd->file_sz = ftell( cfbd->fp );

	if ( (long)cfbd->file_sz < 0 ) {
		error( "ftell() failed : %s.", strerror(errno) );
		return -1;
	}

	if ( cfbd->file_sz == 0 ) {
		error( "File is empty (0 byte)." );
		return -1;
	}

	return 0;
}



/**
 * Opens a given Compound File Binary File for reading.
 *
 * @param  cfbd Pointer to the CFB_Data structure.
 * @return      O.
 */

static int cfb_openFile( CFB_Data *cfbd )
{
	cfbd->fp = fopen( cfbd->file, "rb" );

	if ( cfbd->fp == NULL ) {
		error( "%s.", strerror(errno) );
		return -1;
	}

	return 0;
}



/**
 * Reads a bytes block from the file. This function is
 * called by cfb_getSector() and cfb_getMiniSector()
 * that will do the sector index to file offset conversion.
 *
 * @param cfbd   Pointer to the CFB_Data structure.
 * @param buf    Pointer to the buffer that will hold the len bytes read.
 * @param offset Position in the file the read should start.
 * @param len    Number of bytes to read from the offset position.
 */

static uint64_t cfb_readFile( CFB_Data *cfbd, unsigned char *buf, uint64_t offset, uint64_t len )
{
	FILE *fp = cfbd->fp;


	if ( len + offset > cfbd->file_sz ) {
		error( "Requested data goes %"PRIu64" bytes beyond the EOF : offset %"PRIu64" | length %"PRIu64"", (len + offset) - cfbd->file_sz, offset, len );
		return 0;
	}


	int rc = fseek( fp, offset, SEEK_SET );

	if ( rc < 0 ) {
		error( "%s.", strerror(errno) );
		return 0;
	}


	uint64_t byteRead = fread( buf, sizeof(unsigned char), len, fp );

	if ( byteRead < len ) {
		warning( "Could only retrieve %"PRIu64" bytes out of %"PRIu64" requested.", byteRead, len );
	}


	return byteRead;
}



/**
 * Closes the file pointer hold by the CFB_Data.fp.
 *
 * @param cfbd Pointer to the CFB_Data structure.
 */

static void cfb_closeFile( CFB_Data *cfbd )
{
	if ( cfbd == NULL || cfbd->fp == NULL )
		return;

	if ( fclose( cfbd->fp ) != 0 ) {
		error( "%s.", strerror(errno) );
	}

	cfbd->fp = NULL;
}



/**
 * Retrieves a sector content from the FAT.
 *
 * @param id   Index of the sector to retrieve in the FAT.
 * @param cfbd Pointer to the CFB_Data structure.
 * @return     Pointer to the sector's data bytes.
 */

unsigned char * cfb_getSector( CFB_Data *cfbd, cfbSectorID_t id )
{
	/*
	 * foreachSectorInChain() calls cfb_getSector() before
	 * testing the ID, so we have to ensure the ID is valid
	 * before we try to actualy get the sector.
	 */

	if ( id >= CFB_MAX_REG_SID )
		return NULL;


	if ( cfbd->fat_sz > 0 && id >= cfbd->fat_sz ) {
		error( "Asking for an out of range FAT sector @ index %u (max FAT index is %u)", id, cfbd->fat_sz );
		return NULL;
	}



	uint64_t sectorSize = (1 << cfbd->hdr->_uSectorShift);
	uint64_t fileOffset = (id + 1) << cfbd->hdr->_uSectorShift;



	unsigned char *buf = calloc( sectorSize, sizeof(unsigned char) );

	if ( buf == NULL ) {
		error( "%s.", strerror(errno) );
		return NULL;
	}



	if ( cfb_readFile( cfbd, buf, fileOffset, sectorSize ) == 0 ) {
		free( buf );
		return NULL;
	}


	return buf;
}



/**
 * Retrieves a mini-sector content from the MiniFAT.
 *
 * @param id   Index of the mini-sector to retrieve in the MiniFAT.
 * @param cfbd Pointer to the CFB_Data structure.
 * @return     Pointer to the mini-sector's data bytes.
 */

unsigned char * cfb_getMiniSector( CFB_Data *cfbd, cfbSectorID_t id )
{
	/*
	 * foreachMiniSectorInChain() calls cfb_getMiniSector() before
	 * testing the ID, so we have to ensure the ID is valid before
	 * we try to actualy get the sector.
	 */

	if ( id >= CFB_MAX_REG_SID )
		return NULL;


	if ( cfbd->fat_sz > 0 && id >= cfbd->miniFat_sz ) {
		error( "Asking for an out of range MiniFAT sector @ index %u (0x%x) (Maximum MiniFAT index is %u)", id, id, cfbd->miniFat_sz );
		return NULL;
	}

	int MiniSectorSize = 1 << cfbd->hdr->_uMiniSectorShift;
	int SectorSize     = 1 << cfbd->hdr->_uSectorShift;



	unsigned char * buf = calloc( MiniSectorSize, sizeof(unsigned char) );

	if ( buf == NULL ) {
		error( "%s.", strerror(errno) );
		return NULL;
	}



	/* Retrieve the MiniSector file offset */
	cfbSectorID_t fatId  = cfbd->nodes[0]._sectStart;
	uint64_t      offset = 0;
	uint32_t      i      = 0;

	/* Fat Divisor: allow to guess the number of mini-stream sectors per standard sector */
	unsigned int fatDiv = SectorSize / MiniSectorSize;

	/* move forward in the FAT's mini-stream chain to retrieve the sector we want. */
	for ( i = 0; i < id/fatDiv; i++ ) {   fatId = cfbd->fat[fatId];   }

	offset  = ((fatId + 1) << cfbd->hdr->_uSectorShift);
	offset += ((id % fatDiv ) << cfbd->hdr->_uMiniSectorShift);



	if ( cfb_readFile( cfbd, buf, offset, MiniSectorSize ) == 0 ) {
		free( buf );
		return NULL;
	}


	return buf;
}



/**
 * Retrieves a stream from a stream Node.
 *
 * @param cfbd      Pointer to the CFB_Data structure.
 * @param node      Pointer to the node to retrieve the stream from.
 * @param stream    Pointer to the pointer where the stream data will be saved.
 * @param stream_sz Pointer to an uint64_t where the stream size will be saved.
 * @return          The retrieved stream size.
 */

uint64_t cfb_getStream( CFB_Data *cfbd, cfbNode *node, unsigned char **stream, uint64_t *stream_sz )
{

//	if ( node == NULL || node->_mse == STGTY_ROOT )
//		return;

//	Should not happen.. or could it ?
//	if ( node->_ulSizeLow < 1 )
//		return;

	uint64_t stream_len = cfb_getNodeStreamLen( cfbd, node );  //node->_ulSizeLow;

	if ( stream_len == 0 ) {
		return 0;
	}

	*stream = calloc( stream_len, sizeof(unsigned char) );

	if ( *stream == NULL ) {
		error( "%s.", strerror( errno ) );
		return 0;
	}


	unsigned char *buf    = NULL;
	cfbSectorID_t  id     = node->_sectStart;
	uint64_t       offset = 0;
	uint64_t       cpy_sz = 0;

	if ( stream_len < cfbd->hdr->_ulMiniSectorCutoff ) { /* mini-stream */

		cfb_foreachMiniSectorInChain( cfbd, buf, id ) {

			if ( !buf ) {
				free( *stream );
				*stream = NULL;
				return 0;
			}

			cpy_sz = ( (stream_len - offset) < (uint64_t)(1<<cfbd->hdr->_uMiniSectorShift) ) ?
			           (stream_len - offset) : (uint64_t)(1<<cfbd->hdr->_uMiniSectorShift);

			memcpy( *stream+offset, buf, cpy_sz );

			free( buf );

			offset += (1<<cfbd->hdr->_uMiniSectorShift);
		}
	}
	else {

		cfb_foreachSectorInChain( cfbd, buf, id ) {

			cpy_sz = ( (stream_len - offset) < (uint64_t)(1<<cfbd->hdr->_uSectorShift) ) ?
			           (stream_len - offset) : (uint64_t)(1<<cfbd->hdr->_uSectorShift);

			memcpy( *stream+offset, buf, cpy_sz );

			free( buf );

			offset += (1<<cfbd->hdr->_uSectorShift);
		}
	}

	if ( stream_sz != NULL )
		*stream_sz = stream_len;

	return stream_len;
}



/**
 * Loops through all the sectors that compose a stream
 * and retrieve their content.
 *
 * This function should be called through the macro cfb_foreachSectorInStream().
 *
 * @param  cfbd      Pointer to the CFB_Data structure.
 * @param  node      Pointer to the Node that hold the stream.
 * @param  buf       Pointer to pointer to the buffer that will receive the sector's
 *                   content.
 * @param  bytesRead Pointer to a size_t that will receive the number of bytes retreived.
 *                   This should be equal to the sectorSize, except for the last sector
 *                   in which the stream might end before the end of the sector.
 * @param  sectID    Pointer to the Node index.
 * @return           1 if we have retrieved a sector with some data, \n
 *                   0 if we have reached the #CFB_END_OF_CHAIN.
 */

int cfb__foreachSectorInStream( CFB_Data *cfbd, cfbNode *node, unsigned char **buf, size_t *bytesRead, cfbSectorID_t *sectID )
{
	if ( node == NULL )
		return 0;

	if ( *sectID >= CFB_MAX_REG_SID )
		return 0;

	/* is this possible ? */
//	if ( node->_ulSizeLow < 1 )
//		return 0;

	/* free the previously allocated buf, if any */
	if ( *buf != NULL ) {
		free( *buf );
		*buf = NULL;
	}


	/* if *nodeID == 0, then it is the first function call */
	*sectID = ( *sectID == 0 ) ? node->_sectStart : *sectID;


	size_t stream_sz = cfb_getNodeStreamLen( cfbd, node );

	if ( stream_sz < cfbd->hdr->_ulMiniSectorCutoff ) {
		/* Mini-Stream */
		*buf       = cfb_getMiniSector( cfbd, *sectID );
		*bytesRead = (1<<cfbd->hdr->_uMiniSectorShift);
		*sectID    = cfbd->miniFat[*sectID];
	}
	else {
		/* Stream */
		*buf       = cfb_getSector( cfbd, *sectID );
		*bytesRead = (1<<cfbd->hdr->_uSectorShift);
		*sectID    = cfbd->fat[*sectID];
	}


	// trim data length to match the EXACT stream size

	// if ( *sectID >= CFB_MAX_REG_SECT )
	//      *bytesRead = ( stream_sz % *bytesRead );
	//      *bytesRead = ( stream_sz % *bytesRead );


	return 1;
}



/**
 * Retrieves the Header of the Compound File Binary.
 * The Header begins at offset 0 and is 512 bytes long,
 * regardless of the file's sector size.
 *
 * @param  cfbd Pointer to the CFB_Data structure.
 * @return      0 on success\n
 *              1 on failure
 */

static int cfb_retrieveFileHeader( CFB_Data *cfbd )
{
	cfbd->hdr = calloc( sizeof(cfbHeader), sizeof(unsigned char) );

	if ( cfbd->hdr == NULL ) {
		error( "%s.", strerror(errno) );
		return -1;
	}

	if ( cfb_readFile( cfbd, (unsigned char*)cfbd->hdr, 0, sizeof(cfbHeader) ) == 0 ) {
		free( cfbd->hdr );
		cfbd->hdr = NULL;
		return -1;
	}

	return 0;
}



static int cfb_retrieveDiFAT( CFB_Data *cfbd )
{
	cfbSectorID_t * DiFAT = NULL;

	/*
	 * Check DiFAT properties in header.
	 * (Exemple AMWA aaf files.)
	 */

	cfbSectorID_t csectDif = 0;

	if ( cfbd->hdr->_csectFat > 109 ) {
		csectDif = ceil( (float)((cfbd->hdr->_csectFat - 109) * 4) / (1<<cfbd->hdr->_uSectorShift) );
	}

	if ( csectDif != cfbd->hdr->_csectDif ) {
		warning( "cfbd->hdr->_csectDif value seems wrong (%u)", cfbd->hdr->_csectDif );
		// warning( "cfbd->hdr->_csectDif value seems wrong (%u). Correcting from cfbd->hdr->_csectFat.", cfbd->hdr->_csectDif );
		// cfbd->hdr->_csectDif = csectDif;
	}

	if ( csectDif == 0 && cfbd->hdr->_sectDifStart != CFB_END_OF_CHAIN ) {
		warning( "cfbd->hdr->_sectDifStart is 0x%08x (%u) but should be CFB_END_OF_CHAIN. Correcting.", cfbd->hdr->_sectDifStart, cfbd->hdr->_sectDifStart );
		cfbd->hdr->_sectDifStart = CFB_END_OF_CHAIN;
	}


	/*
	 * DiFAT size is the number of FAT sector entries in the DiFAT chain.
	 */

	uint32_t DiFAT_sz = ( cfbd->hdr->_csectDif )
		* (((1<<cfbd->hdr->_uSectorShift) / sizeof(cfbSectorID_t)) - 1)
		+ 109;


	DiFAT = calloc( DiFAT_sz, sizeof(cfbSectorID_t) );

	if ( DiFAT == NULL ) {
		error( "%s.", strerror( errno ) );
		return -1;
	}


	/*
	 * Retrieves the 109 first DiFAT entries, from the last bytes of
	 * the cfbHeader structure.
	 */

	memcpy( DiFAT, cfbd->hdr->_sectFat, 109 * sizeof(cfbSectorID_t) );


	unsigned char  *buf    = NULL;
	cfbSectorID_t   id     = 0; //cfbd->hdr->_sectDifStart;
	uint64_t        offset = 109 * sizeof(cfbSectorID_t);

	uint64_t cnt = 0;

	cfb_foreachSectorInDiFATChain( cfbd, buf, id ) {

		if ( buf == NULL ) {
			error( "Error retrieving sector %u (0x%08x) out of the DiFAT chain.", id, id );
			return -1;
		}

		memcpy( (unsigned char*)DiFAT+offset, buf, (1<<cfbd->hdr->_uSectorShift)-4 );

		offset += (1 << cfbd->hdr->_uSectorShift) - 4;
		cnt++;

		/*
		 * If we count more DiFAT sector when parsing than
		 * there should be, it means the sector list does
		 * not end by a proper CFB_END_OF_CHAIN.
		 */

		if ( cnt >= cfbd->hdr->_csectDif )
			break;
	}

	free( buf );

	/*
	 * Standard says DIFAT should end with a CFB_END_OF_CHAIN index,
	 * however it has been observed that some files end with CFB_FREE_SECT.
	 * So we consider it ok.
	 */
	if ( id != CFB_END_OF_CHAIN /*&& id != CFB_FREE_SECT*/ )
		warning( "Incorrect end of DiFAT Chain 0x%08x (%d)", id, id );

	cfbd->DiFAT    = DiFAT;
	cfbd->DiFAT_sz = DiFAT_sz;

	return 0;
}



/**
 * Retrieves the FAT (File Allocation Table). Requires
 * the DiFAT to be retrieved first.
 *
 * @param  cfbd Pointer to the CFB_Data structure.
 * @return      0.
 */

static int cfb_retrieveFAT( CFB_Data * cfbd )
{

	cfbSectorID_t *FAT    = NULL;
	uint64_t       FAT_sz = (((cfbd->hdr->_csectFat) * (1<<cfbd->hdr->_uSectorShift)))
	                      / sizeof(cfbSectorID_t);


	FAT = calloc( FAT_sz, sizeof(cfbSectorID_t) );

	if ( FAT == NULL ) {
		error( "%s.", strerror( errno ) );
		return -1;
	}

	cfbd->fat    = FAT;
	cfbd->fat_sz = FAT_sz;



	unsigned char *buf    = NULL;
	cfbSectorID_t  id     = 0;
	uint64_t       offset = 0;

	cfb_foreachFATSectorIDInDiFAT( cfbd, id ) {

		if ( cfbd->DiFAT[id] == CFB_FREE_SECT )
			continue;

		/* observed in fairlight's AAFs.. */
		if ( cfbd->DiFAT[id] == 0x00000000 && id > 0 ) {
			warning( "Got a NULL FAT index in the DiFAT @ %u, should be CFB_FREE_SECT.", id );
			continue;
		}

		buf = cfb_getSector( cfbd, cfbd->DiFAT[id] );

		if ( buf == NULL ) {
			error( "Error retrieving FAT sector %u (0x%08x).", id, id );
			return -1;
		}

		memcpy( ((unsigned char*)FAT)+offset, buf, (1<<cfbd->hdr->_uSectorShift) );

		free ( buf );

		offset += (1<<cfbd->hdr->_uSectorShift);
	}


	return 0;
}



/**
 * Retrieves the MiniFAT (Mini File Allocation Table).
 *
 * @param  cfbd Pointer to the CFB_Data structure.
 * @return      0.
 */

static int cfb_retrieveMiniFAT( CFB_Data * cfbd )
{

	uint64_t miniFat_sz = cfbd->hdr->_csectMiniFat * (1<<cfbd->hdr->_uSectorShift)
	                    / sizeof(cfbSectorID_t);

	cfbSectorID_t *miniFat = calloc( miniFat_sz, sizeof(cfbSectorID_t) );

	if ( miniFat == NULL ) {
		error( "%s.", strerror(errno) );
		return -1;
	}


	unsigned char *buf    = NULL;
	cfbSectorID_t  id     = cfbd->hdr->_sectMiniFatStart;
	uint64_t       offset = 0;

	cfb_foreachSectorInChain( cfbd, buf, id ) {
		if ( buf == NULL ) {
			error( "Error retrieving MiniFAT sector %u (0x%08x).", id, id );
			return -1;
		}

		memcpy( (unsigned char*)miniFat+offset, buf, (1<<cfbd->hdr->_uSectorShift) );

		free( buf );

		offset += (1<<cfbd->hdr->_uSectorShift);
	}


	cfbd->miniFat    = miniFat;
	cfbd->miniFat_sz = miniFat_sz;


	return 0;
}



/**
 * Retrieves the nodes (directories and files) of the
 * Compound File Tree, as an array of cfbNodes.
 *
 * Each node correspond to a cfbNode struct of 128 bytes
 * length. The nodes are stored in a dedicated FAT chain,
 * starting at the FAT sector[cfbHeader._sectDirStart].
 *
 * Once retrieved, the nodes are accessible through the
 * CFB_Data.nodes pointer. Each Node is then accessible by
 * its ID (a.k.a SID) :
 *
 * ```
	cfbNode *node = CFB_Data.nodes[ID];
 * ```
 *
 * @param cfbd Pointer to the CFB_Data structure.
 * @return     0.
 */

static int cfb_retrieveNodes( CFB_Data *cfbd )
{
	cfbd->nodes_cnt = getNodeCount( cfbd );

	cfbNode *node  = calloc( cfbd->nodes_cnt, sizeof(cfbNode) );

	if ( node == NULL ) {
		error( "%s.", strerror( errno ) );
		return -1;
	}

	unsigned char *buf = NULL;
	cfbSectorID_t  id  = cfbd->hdr->_sectDirStart;
	cfbSID_t       i   = 0;


	if ( cfbd->hdr->_uSectorShift == 9 ) { /* 512 bytes sectors */

		cfb_foreachSectorInChain( cfbd, buf, id ) {

			if ( buf == NULL ) {
				error( "Error retrieving Directory sector %u (0x%08x).", id, id );
				return -1;
			}

			memcpy( &node[i++], buf,     CFB_NODE_SIZE );
			memcpy( &node[i++], buf+128, CFB_NODE_SIZE );
			memcpy( &node[i++], buf+256, CFB_NODE_SIZE );
			memcpy( &node[i++], buf+384, CFB_NODE_SIZE );

			free( buf );
		}
	}
	else if ( cfbd->hdr->_uSectorShift == 12 ) { /* 4096 bytes sectors */

		cfb_foreachSectorInChain( cfbd, buf, id ) {

			if ( buf == NULL ) {
				error( "Error retrieving Directory sector %u (0x%08x).", id, id );
				return -1;
			}

			memcpy( &node[i++], buf,      CFB_NODE_SIZE );
			memcpy( &node[i++], buf+128,  CFB_NODE_SIZE );
			memcpy( &node[i++], buf+256,  CFB_NODE_SIZE );
			memcpy( &node[i++], buf+384,  CFB_NODE_SIZE );
			memcpy( &node[i++], buf+512,  CFB_NODE_SIZE );
			memcpy( &node[i++], buf+640,  CFB_NODE_SIZE );
			memcpy( &node[i++], buf+768,  CFB_NODE_SIZE );
			memcpy( &node[i++], buf+896,  CFB_NODE_SIZE );
			memcpy( &node[i++], buf+1024, CFB_NODE_SIZE );
			memcpy( &node[i++], buf+1152, CFB_NODE_SIZE );
			memcpy( &node[i++], buf+1280, CFB_NODE_SIZE );
			memcpy( &node[i++], buf+1408, CFB_NODE_SIZE );
			memcpy( &node[i++], buf+1536, CFB_NODE_SIZE );
			memcpy( &node[i++], buf+1664, CFB_NODE_SIZE );
			memcpy( &node[i++], buf+1792, CFB_NODE_SIZE );
			memcpy( &node[i++], buf+1920, CFB_NODE_SIZE );
			memcpy( &node[i++], buf+2048, CFB_NODE_SIZE );
			memcpy( &node[i++], buf+2176, CFB_NODE_SIZE );
			memcpy( &node[i++], buf+2304, CFB_NODE_SIZE );
			memcpy( &node[i++], buf+2432, CFB_NODE_SIZE );
			memcpy( &node[i++], buf+2560, CFB_NODE_SIZE );
			memcpy( &node[i++], buf+2688, CFB_NODE_SIZE );
			memcpy( &node[i++], buf+2816, CFB_NODE_SIZE );
			memcpy( &node[i++], buf+2944, CFB_NODE_SIZE );
			memcpy( &node[i++], buf+3072, CFB_NODE_SIZE );
			memcpy( &node[i++], buf+3200, CFB_NODE_SIZE );
			memcpy( &node[i++], buf+3328, CFB_NODE_SIZE );
			memcpy( &node[i++], buf+3456, CFB_NODE_SIZE );
			memcpy( &node[i++], buf+3584, CFB_NODE_SIZE );
			memcpy( &node[i++], buf+3712, CFB_NODE_SIZE );
			memcpy( &node[i++], buf+3840, CFB_NODE_SIZE );
			memcpy( &node[i++], buf+3968, CFB_NODE_SIZE );

			free( buf );
		}
	}
	else {

		/* handle non-standard sector size, that is different than 512B or 4kB */
		/* TODO has not been tested yet, should not even exist anyway */

		warning( "Parsing non-standard sector size !!! (%u bytes)", (1<<cfbd->hdr->_uSectorShift) )
		uint32_t nodesPerSect = (1 << cfbd->hdr->_uMiniSectorShift) / sizeof(cfbNode);

		cfb_foreachSectorInChain( cfbd, buf, id ) {
			if ( buf == NULL ) {
				error( "Error retrieving Directory sector %u (0x%08x).", id, id );
				return -1;
			}

			for ( i = 0; i < nodesPerSect; i++ ) {
				memcpy( &node[i], buf+(i * CFB_NODE_SIZE), CFB_NODE_SIZE );
			}

			free( buf );
		}
	}


	cfbd->nodes = node;

	return 0;
}



/**
 * Converts 16-bits MS/CFB wchar_t to system wchar_t.
 *
 * @param buf      Pointer to wchar_t output buffer. If NULL, then function will allocate a new buffer.
 * @param w16buf   Pointer to a 16-bits MS/CFB "wchar_t" array.
 * @param w16blen  Size of the w16buf array in bytes, including the NULL. If it is set to
 *                  CFB_W16TOWCHAR_STRLEN, then function will parse w16buf up to NULL to retrieve w16buf byte size
 *
 * @return          Pointer to buf,\n
 *                  NULL on failure.
 */

wchar_t * cfb_w16towchar( wchar_t *buf, uint16_t *w16buf, size_t w16blen )
{
	if ( w16buf == NULL )
		return NULL;

	if ( w16blen == CFB_W16TOWCHAR_STRLEN ) {
		w16blen = 0;
		while ( w16buf[w16blen>>1] != 0x0000 ) {
			w16blen += sizeof(uint16_t);
		}
		w16blen += sizeof(uint16_t); /* NULL termination */
	}

	if ( buf == NULL ) {
		buf = malloc( w16blen * sizeof(wchar_t) );
		if ( buf == NULL )
			return NULL;
	}

	for ( size_t i = 0; i < w16blen>>1; i++ ) {
		buf[i] = ((uint16_t*)w16buf)[i];
	}

	return buf;
}



/**
 * Retrieves a Node in the Compound File Tree by path.
 *
 * @param cfbd      Pointer to the CFB_Data structure.
 * @param path      Pointer to a NULL terminated char array, holding the Node path.
 * @param id        Next node index. This is used internaly by the function and should
 *                  be set to 0 when called by the user.
 *
 * @return          Pointer to the retrieved Node,\n
 *                  NULL on failure.
 */

cfbNode * cfb_getNodeByPath( CFB_Data *cfbd, const wchar_t *path, cfbSID_t id )
{

	/*
	 * begining of the first function call.
	 */

	if ( id == 0 ) {

		if ( path[0] == '/' && path[1] == 0x0000 ) {
			return &cfbd->nodes[0];
		}

		/*
		 * work either with or without "/Root Entry"
		 */

		if ( wcsncmp( path, L"/Root Entry", 11 ) != 0 ) {
			id = cfbd->nodes[0]._sidChild;
		}
	}


	uint32_t l = 0;

	/*
	 * retrieves the first node's name from path
	 */

	for( l = 0; l < wcslen(path); l++ )
		if ( l > 0 && path[l] == '/' )
			break;

	/*
	 * removes any leading '/'
	 */

	if ( path[0] == '/' ) {
		path++;
		l--;
	}


	wchar_t ab[CFB_NODE_NAME_SZ];

	while ( 1 ) {

		if ( id >= cfbd->nodes_cnt ) {
			error( "Out of range Node index %d, max %u.", id, cfbd->nodes_cnt );
			return NULL;
		}

		// laaf_util_dump_hex( cfbd->nodes[id]->_ab, cfbd->nodes[id]->_cb );

		cfb_w16towchar( ab, cfbd->nodes[id]._ab, cfbd->nodes[id]._cb );

		int32_t rc = 0;

		if ( wcslen(ab) == l )
			rc = wcsncmp( path, ab, l );
		else
			rc = l - wcslen(ab);

		/*
		 * Some node in the path was found.
		 */

		if ( rc == 0 ) {
			/*
			 * get full path length minus any terminating '/'
			 */

			uint32_t pathLen = wcslen(path);

			if ( path[pathLen-1] == '/' )
				pathLen--;

			/*
			 * If pathLen equals node name length, then
			 * we got the target node. Else, move forward
			 * to next node in the path.
			 */

			if ( pathLen == l )
				return &cfbd->nodes[id];
			else
				return cfb_getNodeByPath( cfbd, path+l, cfbd->nodes[id]._sidChild );
		}
		else if ( rc > 0 )	id = cfbd->nodes[id]._sidRightSib;
		else if ( rc < 0 )	id = cfbd->nodes[id]._sidLeftSib;


		if ( (int32_t)id < 0 )
			return NULL;
	}
}



/**
 * Retrieves a child node of a parent startNode.
 *
 * @param cfbd      Pointer to the CFB_Data structure.
 * @param name      Pointer to a NULL terminated string, holding the name of the
 *                  searched node.
 * @param startNode Pointer to the parent Node of the searched node.
 *
 * @return          Pointer to the retrieved node,\n
 *                  NULL if not found.
 */

cfbNode * cfb_getChildNode( CFB_Data *cfbd, const wchar_t *name, cfbNode *startNode )
{

	int32_t   rc = 0;

	/** @TODO : cfb_getIDByNode should be quiker (macro ?) */
	cfbSID_t id = cfb_getIDByNode( cfbd, &cfbd->nodes[startNode->_sidChild] );

	uint32_t nameUTF16Len = ((wcslen( name )+1) << 1);

	wchar_t nodename[CFB_NODE_NAME_SZ];

	while ( 1 ) {

		if ( id >= cfbd->nodes_cnt ) {
			error( "Out of range Node index %u, max %u.", id, cfbd->nodes_cnt );
			return NULL;
		}

		cfb_w16towchar( nodename, cfbd->nodes[id]._ab, cfbd->nodes[id]._cb );

		if ( cfbd->nodes[id]._cb == nameUTF16Len )
			rc = wcscmp( name, nodename );
		else
			rc = nameUTF16Len - cfbd->nodes[id]._cb;

		/*
		 * Node found
		 */

		if ( rc == 0 )
			break;

		/*
		 * Not found, go right
		 */

		else if ( rc > 0 )
			id = cfbd->nodes[id]._sidRightSib;

		/*
		 * Not found, go left
		 */

		else if ( rc < 0 )
			id = cfbd->nodes[id]._sidLeftSib;



		/*
		 * Not found
		 */

		if ( id >= CFB_MAX_REG_SID )
			break;
	}

	if ( rc == 0 ) {
		return &cfbd->nodes[id];
	}

	return NULL;
}



static cfbSID_t cfb_getIDByNode( CFB_Data *cfbd, cfbNode *node )
{
	cfbSID_t id = 0;

	for ( ; id < cfbd->nodes_cnt; id++ ) {
		if ( node == &cfbd->nodes[id] ) {
			return id;
		}
	}

	return -1;
}



/**
 * Loops through each FAT sector in the Directory chain
 * to retrieve the total number of Directories (Nodes)
 *
 * @param cfbd Pointer to the CFB_Data structure.
 * @return     The retrieved Nodes count.
 */

static cfbSID_t getNodeCount( CFB_Data *cfbd )
{
	uint64_t      cnt = ( 1<<cfbd->hdr->_uSectorShift );
	cfbSectorID_t id  = cfbd->hdr->_sectDirStart;

	while ( id < CFB_MAX_REG_SID ) {

		if ( id >= cfbd->fat_sz ) {
			error( "index (%u) > FAT size (%u).", id, cfbd->fat_sz );
			break;
		}

		id = cfbd->fat[id];

		cnt += ( 1<<cfbd->hdr->_uSectorShift );
	}

	return cnt / sizeof(cfbNode);
}


/**
 * @}
 */
