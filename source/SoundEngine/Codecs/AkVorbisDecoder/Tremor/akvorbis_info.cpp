/***********************************************************************
  The content of this file includes source code for the sound engine
  portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
  Two Source Code" as defined in the Source Code Addendum attached
  with this file.  Any use of the Level Two Source Code shall be
  subject to the terms and conditions outlined in the Source Code
  Addendum and the End User License Agreement for Wwise(R).

  Version:  Build: 
  Copyright (c) 2006-2019 Audiokinetic Inc.
***********************************************************************/

/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis 'TREMOR' CODEC SOURCE CODE.   *
 *                                                                  *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis 'TREMOR' SOURCE CODE IS (C) COPYRIGHT 1994-2003    *
 * BY THE Xiph.Org FOUNDATION http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: maintain the info structure, info <-> header packets

 ********************************************************************/

/* general handling of the header and the vorbis_info structure (and
   substructures) */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "ogg.h"
#include "ivorbiscodec.h"
#include "codec_internal.h"
#include "packed_codebooks.h"
#include "codebook.h"
#include "os.h"
#include "AkVorbisCodebookMgr.h"

/* helpers */
/* used by synthesis, which has a full, alloced vi */
int vorbis_info_init(codec_setup_info *csi, long in_blocksize0, long in_blocksize1 )
{
	memset(csi,0,sizeof(*csi));

	csi->blocksizes[0] = 1 << in_blocksize0;
	csi->blocksizes[1] = 1 << in_blocksize1;

	if(csi->blocksizes[0]<64)
	{
	  goto err_out; 
	}
	if(csi->blocksizes[1]<csi->blocksizes[0])
	{
	  goto err_out;
	}
	if(csi->blocksizes[1]>8192)
	{
	  goto err_out;
	}

	return(0);

err_out:
	return(OV_EBADHEADER);
}


/* all of the real encoding details are here.  The modes, books,
   everything */
int vorbis_unpack_books(Codebook* in_pCodebook,int channels,oggpack_buffer *opb)
{
	int i;
//--------------------------------------------------------------------------------
// codebooks
//--------------------------------------------------------------------------------
	codec_setup_info *ci = &in_pCodebook->csi;
	ci->books = oggpack_read(opb,8)+1;
	int AllocSize = sizeof(*ci->book_param)*ci->books;

	//Alloc the book structures temporarily.  
	//We'll compute the amount of memory the decoding tables take and figure out the total amount of memory to use.
	ci->book_param = (codebook *)in_pCodebook->allocator.AllocDecodingTable(AllocSize);
	if(!ci->book_param)
		return(OV_EBADHEADER);

	memset(ci->book_param, 0, AllocSize);
	
#ifdef AK_POINTER_64
	const int c_oldBookStructSize = 72;
#else
	const int c_oldBookStructSize = 64;
#endif
	in_pCodebook->allocator.AddBookSize(ci->books*c_oldBookStructSize);
	int err = 0;
	for(i = 0 ; i < ci->books ; i++)
	{
		unsigned int idx = oggpack_read(opb,10);
		oggpack_buffer opbBook;

		ogg_buffer bufBook;
		bufBook.data = (unsigned char *) g_packedCodebooks[ idx ];
		bufBook.size = MAX_PACKED_CODEBOOK_SIZE;
		oggpack_readinit( &opbBook, &bufBook );		
		err = vorbis_book_unpack(&opbBook, ci->book_param + i, in_pCodebook->allocator, channels == 2 ? 2 : 1);
		if(err != 0)
			return err;
	}

	//Init the memory pool that contain all vorbis structures, *excluding* the books.
	if (!in_pCodebook->allocator.Reserve())
		return(OV_NOMEM);

//--------------------------------------------------------------------------------
// floor backend settings
//--------------------------------------------------------------------------------
	ci->floors = oggpack_read(opb,6)+1;

	AllocSize = sizeof(vorbis_info_floor1) * ci->floors;

	ci->floor_param = (vorbis_info_floor1*)in_pCodebook->allocator.Calloc(AllocSize);

	for(i = 0 ; i < ci->floors ; i++)
	{
		if(floor1_info_unpack(&ci->floor_param[i], ci, opb, in_pCodebook->allocator) != 0)
		{
			goto err_out;
		}
	}
//--------------------------------------------------------------------------------
// residue backend settings
//--------------------------------------------------------------------------------
	ci->residues = oggpack_read(opb,6)+1;
	AllocSize = sizeof(*ci->residue_param)*ci->residues;
	ci->residue_param = (vorbis_info_residue*)in_pCodebook->allocator.Alloc(AllocSize);

	for(i = 0 ; i < ci->residues ; ++i)
	{
		if(res_unpack(ci->residue_param + i, ci, opb, in_pCodebook->allocator))
		{
			goto err_out;
		}
	}
//--------------------------------------------------------------------------------
// map backend settings
//--------------------------------------------------------------------------------
	ci->maps = oggpack_read(opb,6)+1;
	AllocSize = sizeof(*ci->map_param)*ci->maps;
	ci->map_param = (vorbis_info_mapping*)in_pCodebook->allocator.Alloc(AllocSize);

	for(i = 0 ; i < ci->maps ; ++i)
	{
		if(mapping_info_unpack(ci->map_param + i, ci, channels, opb, in_pCodebook->allocator))
		{
			goto err_out;
		}
	}
//--------------------------------------------------------------------------------
// mode settings
//--------------------------------------------------------------------------------
	ci->modes=oggpack_read(opb,6)+1;
	AllocSize = ci->modes*sizeof(*ci->mode_param);
	ci->mode_param = (vorbis_info_mode *)in_pCodebook->allocator.Alloc(AllocSize);

	for(i = 0 ; i < ci->modes ; ++i)
	{
		ci->mode_param[i].blockflag = oggpack_read(opb,1);
		ci->mode_param[i].mapping = oggpack_read(opb,8);
		if(ci->mode_param[i].mapping>=ci->maps)
		{
			goto err_out;
		}
	}

	return(0);

err_out:
	return(OV_EBADHEADER);
}
