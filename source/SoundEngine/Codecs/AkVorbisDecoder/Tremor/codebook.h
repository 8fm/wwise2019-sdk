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
 * THE OggVorbis 'TREMOR' SOURCE CODE IS (C) COPYRIGHT 1994-2002    *
 * BY THE Xiph.Org FOUNDATION http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: basic shared codebook operations

 ********************************************************************/

#ifndef _V_CODEBOOK_H_
#define _V_CODEBOOK_H_

#include "ogg.h"
#include <AK/SoundEngine/Common/AkSimd.h>



typedef void(*decode_ptr)(
	codebook* book,
	ogg_int32_t**	a,
	const ogg_int32_t offset,
	oggpack_buffer*	b,
	const int		n);

// Extension to the codebook structure for codebooks that encore spectrum residue.
struct ResidueBook
{
#ifdef AKSIMD_V4F32_SUPPORTED
	AKSIMD_V4I32 add_simd;
	AKSIMD_V4I32 mask_simd;
	AKSIMD_V4I32 q_del_simd;		
	int shift_simd;
#endif

	//vorbis_book_decodev_add_t book-specific constants.  
	int add;
	int mask;
	ogg_int32_t q_delShift;
	ogg_int32_t q_del;
	
	decode_ptr Decode;		//Using a function pointer for decoding function.  It is actually faster than a tree of if/else (unpredictable conditions make the branch prediction unit fail).

	unsigned char q_bits;
};

// Temporary structure to help build codebook trees and decoding tables.
struct CodebookBuildParam
{
	ogg_int32_t q_min;
	int         q_minp;
	unsigned short entries;         /* codebook entries */
	unsigned short used_entries;    /* populated codebook entries */
	int         q_delp;
	int         q_pack;
	void  *q_val;
	//NOTE.  In AK Vorbis, maptype & dec_type are synonymous.  
	int maptype;
	int dec_type;        /* 0 = entry number
								   1 = packed vector of values
								   2 = packed vector of column offsets, maptype 1
								   3 = scalar offset into value array,  maptype 2 (UNUSED IN ENCODER, STRIPPED FROM AK DECODER)  */
	int		dec_nodeb;			//Size of nodes (as per ORIGINAL vorbis code).  Only necessary while building tables.
};

//Codebook structure, keeping all necessary data to traverse the decode tree.
typedef struct codebook{ 
  void *			dec_table;			//Huffman tree, array-packed.  Look-up table at the beginning.
  unsigned char *	lut_lengths;		//Length of terminal symbols within the look-up table.
  ResidueBook *		pResidueParams;		//Optional extension to this codebook.  Parameters for residue decoding.
  
  unsigned short	lut_Mask;			//Pre-computed mask for look-up table indexing.  

  unsigned char		dim;             /* codebook dimensions (elements per vector) */
  unsigned char		dec_leafw;			//Size of leaves 
  unsigned char		dec_maxlength;
} codebook;

extern int vorbis_book_unpack(oggpack_buffer *b, codebook *c, CAkVorbisAllocator& VorbisAllocator, int channels);
extern ogg_int32_t ak_vorbis_book_decode(codebook *book, oggpack_buffer *b); // signature change to avoid symbol clash with standard vorbis

extern void vorbis_book_decodevv_add(codebook *book, ogg_int32_t **a,
				     ogg_int32_t off, oggpack_buffer *b,int n);
#endif
