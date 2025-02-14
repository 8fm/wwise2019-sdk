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

 function: basic codebook pack/unpack/code/decode operations

 ********************************************************************/

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ogg.h"
#include "ivorbiscodec.h"
#include "codebook.h"
#include "os.h"

#ifdef __GNU__
//May be faster than below
#include <byteswap.h>
#endif

static inline void oggpack_look_64bits(oggpack_buffer *b, AkUInt64& io_bits, int &io_available)
{
#ifdef LITTLE_ENDIAN
#ifdef UNALIGNED_DATA_TYPES
	//Take advantage of the fact that the bytes are already in the right order.	
	AkUInt64 ret = *(AkUInt64*)(b->headptr);	
#else	
	//Take advantage of the fact that the bytes are already in the right order.		
	AkUInt64 ret = (AkUInt64)b->headptr[0] | ((AkUInt64)b->headptr[1] << 32);
#endif
#else			
	AkUInt32 ret1 = b->headptr[4] | (b->headptr[5] << 8) |
		 ( b->headptr[6] << 16) | (b->headptr[7] << 24);
	AkUInt32 ret2 = b->headptr[0] | (b->headptr[1] << 8) |
		(b->headptr[2] << 16) | (b->headptr[3] << 24);	
	AkUInt64 ret = ((AkUInt64)ret1 << 32) | ret2;
#endif
	ret >>= b->headbit;		//Those bits are already used.	
	io_bits |= ret;
	io_available = 64 - b->headbit;
}


struct LeafByte
{
	static inline ogg_uint32_t Extract(const ogg_uint16_t* pLeaf, AkUInt8 test)
	{
		return test & 0x7F;
	}
};


struct LeafWord
{
	static inline ogg_uint32_t Extract(const ogg_uint16_t* pLeaf, AkUInt8 test)
	{
		return *(pLeaf + (test & 0x7F));
	}
};

// Walk the Huffman tree to find the encoded symbol.
// Tree nodes are always byte-sized and represent the offset to the next target node, from that parent node.
// Leaf nodes can be 8 or 16 bits.
// The "base" of the tree is a look-up table.  Can have either symbols or offsets to nodes.
template <class LEAF_POLICY>
inline ogg_uint32_t FindSymbol(codebook *book, AkUInt64& lok, int &out_read)
{
	const ogg_uint16_t*pWalk = (ogg_uint16_t*)book->dec_table;

	//First check the LUT, always 16 bit
	const int lutEntry = lok & book->lut_Mask;
	AkUInt16 lutVal = pWalk[lutEntry];
	out_read = book->lut_lengths[lutEntry];
	lok >>= out_read;

	if (lutVal >= 0x8000)
	{
		//Found a direct leaf.  Return the symbol
		return lutVal & 0x7fff;
	}

	//Jump to the sub-tree
	AKSIMD_PREFETCHMEMORY(lutVal * 2, pWalk);
	pWalk += lutVal;	
	AkUInt8 test = 0;
	do
	{
		//Jump to the selected node		
		pWalk += test;				

		//Test the next bit in the sequence
		int bit = lok & 1;	
		lok >>= 1;

		//Select which way we go.
		test = ((AkUInt8*)pWalk)[bit];
		out_read++;
	} while (test < 0x80);

	return LEAF_POLICY::Extract(pWalk, test);	
}

struct MonoPolicy
{
	static inline void ApplyOffset(ogg_int32_t*& pLeft, ogg_int32_t*& pRight, int offset)
	{
		pLeft += offset;
		pRight = pLeft;
	}	

	static inline ogg_int32_t* LastChannel(ogg_int32_t* pLeft, ogg_int32_t* pRight)
	{
		return pLeft;
	}

	static inline void Advance(ogg_int32_t*& pLeft, ogg_int32_t*& pRight)
	{
		pLeft++;
		pRight = pLeft;
	}
};

struct StereoPolicy
{
	static inline void ApplyOffset(ogg_int32_t*& pLeft, ogg_int32_t*& pRight, int offset)
	{
		pLeft += offset;
		pRight += offset;
	}

	static inline ogg_int32_t* LastChannel(ogg_int32_t* pLeft, ogg_int32_t* pRight)
	{
		return pRight;
	}

	static inline void Advance(ogg_int32_t*& pLeft, ogg_int32_t*& pRight)
	{
		pLeft++;		
	}
};


template<class CHANNELS, int i>
class ExpandRecursive : public CHANNELS
{
public:
	static inline void Expand(ogg_int32_t*& pLeft, ogg_int32_t*& pRight, const int entry, const int add, const int mask, const int q_del, const int q_bits)
	{		
		*pLeft += add + (entry & mask) * q_del;		
		CHANNELS::Advance(pLeft, pRight);
		ExpandRecursive<CHANNELS, i - 1>::Expand(pRight, pLeft, entry >> q_bits, add, mask, q_del, q_bits);	//Yes, left and right are reversed to alternate between writing to each channel.
	}
};

template<class CHANNELS>
class ExpandRecursive<CHANNELS, 1> : public CHANNELS
{
public:
	static inline void Expand(ogg_int32_t*& pLeft, ogg_int32_t*& pRight, const int entry, const int add, const int mask, const int q_del, const int q_bits)
	{		
		*pLeft += add + (entry & mask) * q_del;	
		CHANNELS::Advance(pLeft, pRight);	
	}
};

struct ExpandEntry1Stereo : public StereoPolicy
{
	static inline void Expand(ogg_int32_t*& pLeft, ogg_int32_t*& pRight, int entry, const int add, const int mask, const int q_del, const int q_bits)
	{
		*pLeft += add + entry * q_del;
		pLeft++;

		//Swap left & right to alternate writing to each channel
		ogg_int32_t* swap = pRight;
		pRight = pLeft;
		pLeft = swap;
	}	
};

// Decode one residue partition.
template <class EXPANSION, class LEAF>
void vorbis_book_decodev_add_t(
	codebook*		s,
	ogg_int32_t**	a,
	const ogg_int32_t offset,
	oggpack_buffer*	b,
	const int		n)
{
	//Cache everything.
	const ogg_int32_t add = s->pResidueParams->add;
	const int mask = s->pResidueParams->mask;
	const int q_bits = s->pResidueParams->q_bits;
	const int q_del = s->pResidueParams->q_delShift;
	const int maxLen = s->dec_maxlength;	
	
	ogg_int32_t* pLeft = a[0];
	ogg_int32_t* pRight = a[1];
	EXPANSION::ApplyOffset(pLeft, pRight, offset);
	const ogg_int32_t* pEnd = pRight + n;

	AkUInt64 bitsEncoded = 0;
	int bitsAvailable = 0;
	int bits = 0;	
 
	while(EXPANSION::LastChannel(pLeft, pRight) != pEnd)
	{
		// Not enough bits.  Read more, by blocks of 64 bits.
		oggpack_look_64bits(b, bitsEncoded, bitsAvailable);
		const int bitsStart = bitsAvailable;

		//Extract all we can from these 64 bits.
		while(bitsAvailable >= maxLen && EXPANSION::LastChannel(pLeft, pRight) != pEnd)
		{
			//Walk the Huffman tree
			ogg_uint32_t entry = FindSymbol<LEAF>(s, bitsEncoded, bits);

			//The symbol found is directly an array of integers bit-packed.
			//Split the component array, scale them to their real value and finally, add them to their proper frequency bin (pointed by pLeft and pRight)
			EXPANSION::Expand(pLeft, pRight, entry, add, mask, q_del, q_bits);
			bitsAvailable -= bits;
		}

		oggpack_adv(b, bitsStart - bitsAvailable);
	}
}

#if defined AK_CPU_ARM_NEON
AkForceInline AKSIMD_V4I32 DEINTERLEAVE_V4I32_EVEN(const AKSIMD_V4I32& xyzw, const AKSIMD_V4I32& abcd)
{
	//return akshuffle_xzac( xyzw, abcd );
	int32x2x2_t xz_yw = vtrn_s32( vget_low_s32( xyzw ), vget_high_s32( xyzw ) );
	int32x2x2_t ac_bd = vtrn_s32( vget_low_s32( abcd ), vget_high_s32( abcd ) );
	AKSIMD_V4I32 xzac = vcombine_s32( xz_yw.val[0], ac_bd.val[0] );
	return xzac;
}

AkForceInline AKSIMD_V4I32 DEINTERLEAVE_V4I32_ODD( const AKSIMD_V4I32& xyzw, const AKSIMD_V4I32& abcd )
{
	//return akshuffle_ywbd( xyzw, abcd );
	int32x2x2_t xz_yw = vtrn_s32( vget_low_s32( xyzw ), vget_high_s32( xyzw ) );
	int32x2x2_t ac_bd = vtrn_s32( vget_low_s32( abcd ), vget_high_s32( abcd ) );
	AKSIMD_V4I32 ywbd = vcombine_s32( xz_yw.val[1], ac_bd.val[1] );
	return ywbd;
}

//On ARM, the shift can be of positive or negative value.  The opcode for right shift is the LEFT shift with a negative value.  
//The code doesn't take advantage of this.  But the whole expend operation is exactly that: shifting left or right when negative.
#define INIT_SHIFT_VAR(__var, __val) AKSIMD_V4I32 __var = vdupq_n_s32((int32_t)(-__val))	
#define SHIFT_BY_VAR(__a, __b) vqshlq_s32(__a, __b)

#elif defined AK_CPU_X86 || defined AK_CPU_X86_64
#define DEINTERLEAVE_V4I32_EVEN(__a__, __b__) _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(__a__), _mm_castsi128_ps(__b__), _MM_SHUFFLE(2, 0, 2, 0)))
#define DEINTERLEAVE_V4I32_ODD(__a__, __b__) _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(__a__), _mm_castsi128_ps(__b__), _MM_SHUFFLE(3, 1, 3, 1)))

#define INIT_SHIFT_VAR(__var, __val) const int __var = __val
#define SHIFT_BY_VAR(__a, __b) AKSIMD_SHIFTRIGHTARITH_V4I32(__a, __b)
#endif

#ifdef AKSIMD_V4F32_SUPPORTED
template <class LEAF>
void vorbis_book_decodev_add_SIMD_8_2ch(
	codebook*		s,
	ogg_int32_t**	a,
	const ogg_int32_t offset,
	oggpack_buffer*	b,
	const int		n)
{
	AKASSERT(s->pResidueParams->q_delShift * ((1 << s->pResidueParams->q_bits) - 1) < 65535);	//If this pops, the _mm_mullo_epi16 call will overflow and the result will be wrong. (3 is the highest possible value for the multiplicand)	
	AKASSERT(n % 4 == 0);
	const AKSIMD_V4I32 add = s->pResidueParams->add_simd;
	const AKSIMD_V4I32 mask = s->pResidueParams->mask_simd;
	const AKSIMD_V4I32 q_del = s->pResidueParams->q_del_simd;
	
	AKASSERT(s->pResidueParams->q_delShift == (AKSIMD_GETELEMENT_V4I32(q_del, 3) << (s->pResidueParams->q_bits * 3)));	//Make sure we didn't loose anything
		
	const int maxLen = s->dec_maxlength;

	const int end = offset + n;

	AkUInt64 bitsEncoded = 0;
	int bitsAvailable = 0;
	int bits = 0;
	int i = offset;
	const int halfshift = s->pResidueParams->q_bits * 4;
	AKSIMD_V4I32 *p0 = (AKSIMD_V4I32*)(a[0]+i);
	AKSIMD_V4I32 *p1 = (AKSIMD_V4I32*)(a[1]+i);

	while (i < end)
	{
		// Not enough bits.  Read more.		
		oggpack_look_64bits(b, bitsEncoded, bitsAvailable);
		const int bitsStart = bitsAvailable;
		while (bitsAvailable >= maxLen && i < end)
		{
			//Stereo version...
			//One entry becomes 8 items (4 samples in 2 channels)
			ogg_uint32_t entry = FindSymbol<LEAF>(s, bitsEncoded, bits);
			
			AKSIMD_V4I32 x1 = AKSIMD_AND_V4I32(AKSIMD_SET_V4I32(entry), mask);
			x1 = AKSIMD_MULLO16_V4I32(x1, q_del);	//WARNING!  NOT SURE IF RESULT IS ALWAYS 16 BITS!  MAY NEED MORE.  SEE ASSERT AT ENTRY
			x1 = AKSIMD_ADD_V4I32(x1, add);	//Todo, try MADD			

			entry >>= halfshift;
						
			AKSIMD_V4I32 x2 = AKSIMD_AND_V4I32(AKSIMD_SET_V4I32(entry), mask);
			x2 = AKSIMD_MULLO16_V4I32(x2, q_del);
			x2 = AKSIMD_ADD_V4I32(x2, add);	//Todo, try MADD			

			AKSIMD_V4I32 ch0 = DEINTERLEAVE_V4I32_EVEN(x1,x2);		//Deinterleave for each channel
			ch0 = AKSIMD_ADD_V4I32(ch0, AKSIMD_LOAD_V4I32(p0));
			AKSIMD_STORE_V4I32(p0, ch0);
			p0++;

			AKSIMD_V4I32 ch1 = DEINTERLEAVE_V4I32_ODD(x1,x2);		//Deinterleave for each channel
			ch1 = AKSIMD_ADD_V4I32(ch1, AKSIMD_LOAD_V4I32(p1));
			AKSIMD_STORE_V4I32(p1, ch1);
			p1++;

			i += 4;
			
			bitsAvailable -= bits;
		}

		oggpack_adv(b, bitsStart - bitsAvailable);
	}
}

template <class LEAF>
void vorbis_book_decodev_add_SIMD_4_2ch(
	codebook*		s,
	ogg_int32_t**	a,
	const ogg_int32_t offset,
	oggpack_buffer*	b,
	const int		n)
{
	AKASSERT(n % 4 == 0);
	const AKSIMD_V4I32 add = s->pResidueParams->add_simd;
	const AKSIMD_V4I32 mask = s->pResidueParams->mask_simd;
	const AKSIMD_V4I32 q_del = s->pResidueParams->q_del_simd;	
	INIT_SHIFT_VAR(shift,  s->pResidueParams->shift_simd);
	
	const int maxLen = s->dec_maxlength * 2;	//We need 2 entries per loop.

	const int end = offset + n;

	AkUInt64 bitsEncoded = 0;
	int bitsAvailable = 0;
	int bits = 0;
	int i = offset;	
	AKSIMD_V4I32 *p0 = (AKSIMD_V4I32*)(a[0] + i);
	AKSIMD_V4I32 *p1 = (AKSIMD_V4I32*)(a[1] + i);

	while (i < end)
	{
		// Not enough bits.  Read more.		
		oggpack_look_64bits(b, bitsEncoded, bitsAvailable);
		const int bitsStart = bitsAvailable;
		while (bitsAvailable >= maxLen && i < end)
		{
			//Stereo version...
			//One entry becomes 4 items (2 samples in 2 channels)
			ogg_uint32_t entry = FindSymbol<LEAF>(s, bitsEncoded, bits);
			bitsAvailable -= bits;

			AKSIMD_V4I32 x1 = AKSIMD_AND_V4I32(AKSIMD_SET_V4I32(entry), mask);
			x1 = AKSIMD_MULLO16_V4I32(x1, q_del);	//WARNING!  NOT SURE IF RESULT IS ALWAYS 16 BITS!  MAY NEED MORE.  SEE ASSERT AT ENTRY
			x1 = SHIFT_BY_VAR(x1, shift);
			x1 = AKSIMD_ADD_V4I32(x1, add);		

			entry = FindSymbol<LEAF>(s, bitsEncoded, bits);
			bitsAvailable -= bits;

			AKSIMD_V4I32 x2 = AKSIMD_AND_V4I32(AKSIMD_SET_V4I32(entry), mask);
			x2 = AKSIMD_MULLO16_V4I32(x2, q_del);
			x2 = SHIFT_BY_VAR(x2, shift);
			x2 = AKSIMD_ADD_V4I32(x2, add);			

			AKSIMD_V4I32 ch0 = DEINTERLEAVE_V4I32_EVEN(x1,x2);		//Deinterleave for each channel
			ch0 = AKSIMD_ADD_V4I32(ch0, AKSIMD_LOAD_V4I32(p0));
			AKSIMD_STORE_V4I32(p0, ch0);
			p0++;

			AKSIMD_V4I32 ch1 = DEINTERLEAVE_V4I32_ODD(x1,x2);		//Deinterleave for each channel
			ch1 = AKSIMD_ADD_V4I32(ch1, AKSIMD_LOAD_V4I32(p1));
			AKSIMD_STORE_V4I32(p1, ch1);
			p1++;

			i += 4;			
		}

		oggpack_adv(b, bitsStart - bitsAvailable);
	}
}

template <class LEAF>
void vorbis_book_decodev_add_SIMD_2(
	codebook*		s,
	ogg_int32_t**	a,
	const ogg_int32_t offset,
	oggpack_buffer*	b,
	const int		n)
{
	AKASSERT(n % 4 == 0);
	const AKSIMD_V4I32 add = s->pResidueParams->add_simd;
	const AKSIMD_V4I32 mask = s->pResidueParams->mask_simd;
	const AKSIMD_V4I32 q_del = s->pResidueParams->q_del_simd;
	INIT_SHIFT_VAR(shift,  s->pResidueParams->shift_simd);

	const int maxLen = s->dec_maxlength * 2;	//We need 2 entries per loop.

	const int end = offset + n;

	AkUInt64 bitsEncoded = 0;
	int bitsAvailable = 0;
	int bits = 0;
	int i = offset;
	const int mergeShift = s->pResidueParams->q_bits * 2;
	AKSIMD_V4I32 *p0 = (AKSIMD_V4I32*)(a[0] + i);	

	while (i < end)
	{
		// Not enough bits.  Read more.		
		oggpack_look_64bits(b, bitsEncoded, bitsAvailable);
		const int bitsStart = bitsAvailable;
		while (bitsAvailable >= maxLen && i < end)
		{
			//One entry becomes 2 items, we need 2 to make one SIMD write
			ogg_uint32_t entry1 = FindSymbol<LEAF>(s, bitsEncoded, bits);
			bitsAvailable -= bits;

			ogg_uint32_t entry2 = FindSymbol<LEAF>(s, bitsEncoded, bits);
			bitsAvailable -= bits;
			
			ogg_uint32_t entry = (entry2 << mergeShift) | entry1;

			AKSIMD_V4I32 x1 = AKSIMD_AND_V4I32(AKSIMD_SET_V4I32(entry), mask);
			x1 = AKSIMD_MULLO16_V4I32(x1, q_del);	//WARNING!  NOT SURE IF RESULT IS ALWAYS 16 BITS!  MAY NEED MORE.  SEE ASSERT AT ENTRY
			x1 = SHIFT_BY_VAR(x1, shift);
			x1 = AKSIMD_ADD_V4I32(x1, add);			
			
			x1 = AKSIMD_ADD_V4I32(x1, AKSIMD_LOAD_V4I32(p0));
			AKSIMD_STORE_V4I32(p0, x1);
			p0++;

			i += 4;
		}

		oggpack_adv(b, bitsStart - bitsAvailable);
	}
}


template <class LEAF>
void vorbis_book_decodev_add_SIMD_4(
	codebook*		s,
	ogg_int32_t**	a,
	const ogg_int32_t offset,
	oggpack_buffer*	b,
	const int		n)
{
	AKASSERT(n % 4 == 0);
	const AKSIMD_V4I32 add = s->pResidueParams->add_simd;
	const AKSIMD_V4I32 mask = s->pResidueParams->mask_simd;
	const AKSIMD_V4I32 q_del = s->pResidueParams->q_del_simd;
	INIT_SHIFT_VAR(shift,  s->pResidueParams->shift_simd);

	const int maxLen = s->dec_maxlength;	//We need 2 entries per loop.

	const int end = offset + n;

	AkUInt64 bitsEncoded = 0;
	int bitsAvailable = 0;
	int bits = 0;
	int i = offset;
	AKSIMD_V4I32 *p0 = (AKSIMD_V4I32*)(a[0] + i);	

	while (i < end)
	{
		// Not enough bits.  Read more.		
		oggpack_look_64bits(b, bitsEncoded, bitsAvailable);
		const int bitsStart = bitsAvailable;
		while (bitsAvailable >= maxLen && i < end)
		{			
			//One entry becomes 4 items 
			ogg_uint32_t entry = FindSymbol<LEAF>(s, bitsEncoded, bits);
			bitsAvailable -= bits;

			AKSIMD_V4I32 x1 = AKSIMD_AND_V4I32(AKSIMD_SET_V4I32(entry), mask);
			x1 = AKSIMD_MULLO16_V4I32(x1, q_del);	//WARNING!  NOT SURE IF RESULT IS ALWAYS 16 BITS!  MAY NEED MORE.  SEE ASSERT AT ENTRY
			x1 = SHIFT_BY_VAR(x1, shift);
			x1 = AKSIMD_ADD_V4I32(x1, add);
					
			x1 = AKSIMD_ADD_V4I32(x1, AKSIMD_LOAD_V4I32(p0));
			AKSIMD_STORE_V4I32(p0, x1);
			p0++;

			i += 4;
		}

		oggpack_adv(b, bitsStart - bitsAvailable);
	}
}

#endif
/**** pack/unpack helpers ******************************************/
static int _ilog(unsigned int v){
  int ret=0;
  while(v){
    ret++;
    v>>=1;
  }
  return(ret);
}

static ogg_uint32_t decpack(ogg_int32_t entry, ogg_int32_t used_entry, ogg_int32_t quantvals,
	codebook *b, oggpack_buffer *opb, CodebookBuildParam *in_pBuild){
  ogg_uint32_t ret=0;
  int j;
  
  switch(in_pBuild->dec_type){

  case 0:
    return (ogg_uint32_t)entry;

  case 1:
	  if(in_pBuild->maptype == 1){
      /* vals are already read into temporary column vector here */
      for(j=0;j<b->dim;j++){
	ogg_uint32_t off=entry%quantvals;
	entry/=quantvals;
	ret |= ((ogg_uint16_t *)(in_pBuild->q_val))[off] << (b->pResidueParams->q_bits*j);
      }
    }else{
      for(j=0;j<b->dim;j++)
		  ret |= oggpack_read_var(opb, b->pResidueParams->q_bits) << (b->pResidueParams->q_bits*j);
    }
    return ret;
    
  case 2:
    for(j=0;j<b->dim;j++){
      ogg_uint32_t off=entry%quantvals;
      entry/=quantvals;
	  ret |= off << (in_pBuild->q_pack*j);
    }
    return ret;

  case 3:
    return (ogg_uint32_t)used_entry;

  }
  return 0; /* silence compiler */
}

/* 32 bit float (not IEEE; nonnormalized mantissa +
   biased exponent) : neeeeeee eeemmmmm mmmmmmmm mmmmmmmm 
   Why not IEEE?  It's just not that important here. */

static ogg_int32_t _float32_unpack(long val,int *point){
  ogg_int32_t mant=val&0x1fffff;
  int    sign=val&0x80000000;
  
  *point=((val&0x7fe00000L)>>21)-788;

  if(mant){
    while(!(mant&0x40000000)){
      mant<<=1;
      *point-=1;
    }
    if(sign)mant= -mant;
  }else{
    *point=-9999;
  }
  return mant;
}

/* choose the smallest supported node size that fits our decode table.
   Legal bytewidths are 1/1 1/2 2/2 2/4 4/4 */
static int _determine_node_bytes(ogg_int32_t used, int leafwidth){

  /* special case small books to size 4 to avoid multiple special
     cases in repack */
  if(used<2)
    return 4;

  if(leafwidth==3)leafwidth=4;
  if(_ilog(3*used-6)+1 <= leafwidth*4) 
    return leafwidth/2?leafwidth/2:1;
  return leafwidth;
}

/* convenience/clarity; leaves are specified as multiple of node word
   size (1 or 2) */
static int _determine_leaf_words(int nodeb, int leafwidth){
  if(leafwidth>nodeb)return 2;
  return 1;
}

/* given a list of word lengths, number of used entries, and byte
   width of a leaf, generate the decode table.

   The decode table is a Look-Up table for the first levels of the Huffman tree, with each entry either pointing to a sub-tree to walk trough, or a final symbol.
   Tree nodes are always byte-sized and represent the offset to the next target node, from that parent node.
   Leaf nodes can be 8 or 16 bits.
   LUT entries are 16 bits to allow far jumps for big tables.
   The LUT size is determined by the number of levels in the Huffman tree that are completely defined (i.e. all bit combinations are used).  
   Then we add 1 extra bit, which will cause duplication of final symbols, but statistically it will also ensure the tree traversal falls more often in the LUT.
   If the distance between the parent node and its 2 child nodes is greater than 256, this function fails.  
   The caller _make_decode_table will select a greater LUT size and try again.  This will split the sub-trees in smaller ones.
   */
static int _make_words(char *l, long n, ogg_uint32_t *r, ogg_int32_t quantvals,
	codebook *b, oggpack_buffer *opb, CodebookBuildParam * in_pBuild, int lut_Size, int &out_lutSymbols, char *pExtraBits, int &out_LeafSize)
{
	ogg_int32_t i, j, count = 0;
	ogg_int32_t top = 0;

	out_lutSymbols = 0;
	ogg_uint32_t maxLeaf = 0;
	
	if (n < 2)
	{
		AKASSERT(!"NOT TESTED");
		r[0] = 0x80000000;
		return 0;
	}	

	top = 1 << lut_Size;
	memset(r, 0, top*sizeof(*r));
	memset(pExtraBits, 0, top);
	
	ogg_uint32_t marker[33] = { 0 };	

	for (i = 0;i < n;i++)
	{
		ogg_int32_t length = l[i];
		if (length)
		{
			ogg_uint32_t entry = marker[length];
			ogg_uint32_t* pWalk = r;
			if (count && !entry)
				return -1; /* overpopulated tree! */			

			/* chase the tree as far as it's already populated, fill in past */
			int lutBits = AkMin(length, lut_Size);
			ogg_uint32_t lutEntry = entry;
			if (length > lut_Size)	//Keep only the MSBs, as they come first.
				lutEntry >>= (length - lut_Size);
			
			//Reverse the bits as they are encoded MSB first.
			ogg_uint32_t reverse = 0;
			for (int bit = 0; bit < lutBits; bit++)
				reverse |= ((lutEntry >> bit) & 1) << (lutBits - bit - 1);

			lutEntry = reverse;
			if (r[lutEntry] == 0)	//LUT Not initialized
			{
				if (length <= lut_Size)
				{
					out_lutSymbols++;
					//Complete symbol
					ogg_uint32_t value = decpack(i, count++, quantvals, b, opb, in_pBuild);
					maxLeaf = value > maxLeaf ? value : maxLeaf;
					value |= 0x80000000;
					r[lutEntry] = value;	
					pExtraBits[lutEntry] = length;

					if (length < lut_Size)
					{
						//Shorter bit pattern then the lut.  Duplicates needed.  						
						int diffBits = lut_Size - length;
						for (int highBits = 0; highBits < (1 << diffBits); highBits++)
						{
							int idx = lutEntry | (highBits << length);
							r[idx] = value;													
							pExtraBits[idx] = length;
						}
					}					
				}
				else
				{
					//Allocate node.				
					r[lutEntry] = top;
					pExtraBits[lutEntry] = lut_Size;
				}
			}
			
			pWalk += r[lutEntry];			
			
			if (length > lut_Size)
			{
				//Build remainder of the tree.
				for (j = lut_Size;j < length - 1;j++)
				{
					int bit = (entry >> (length - j - 1)) & 1;
					if (pWalk - r >= top)
					{
						top += 2;
						pWalk[0] = top;
						pWalk[1] = 0;
					}
					else if (!pWalk[bit])
					{
						pWalk[bit] = top;
					}

					pWalk = r + pWalk[bit];
				}
				{
					ogg_uint32_t value = decpack(i, count++, quantvals, b, opb, in_pBuild);
					maxLeaf = value > maxLeaf ? value : maxLeaf;
					value |= 0x80000000;
					if (pWalk - r >= top)
					{
						top += 2;
						pWalk[0] = value;
						pWalk[1] = 0;
					}
					else
					{
						int bit = (entry >> (length - j - 1)) & 1;
						pWalk[bit] = value;
					}
				}
			}

			/* Look to see if the next shorter marker points to the node
			above. if so, update it and repeat.  */
			for (j = length;j > 0;j--)
			{
				if (marker[j] & 1)
				{
					marker[j] = marker[j - 1] << 1;
					break;
				}
				marker[j]++;
			}


			{
				/* prune the tree; the implicit invariant says all the longer
			markers were dangling from our just-taken node.  Dangle them
			from our *new* node. */
				ogg_uint32_t entryLast = marker[length];
				for (j = length + 1;j < 33;j++)
				{
					if ((marker[j] >> 1) == entry)
					{
						entry = marker[j];
						entryLast <<= 1;
						marker[j] = entryLast;
					}
					else
						break;
				}
			}
		}
	}

	if (maxLeaf <= 0x7F)
		out_LeafSize = 1;
	else if (maxLeaf <= 0x7FFF)
		out_LeafSize = 2;
	else
	{
		AKASSERT(!"Vorbis with 4 bytes tables never tested!");
		out_LeafSize = 4;

	}
	  
  return top;
}

static int GetNodeSize(ogg_uint32_t* node, int leafSize)
{	
	return  ((node[0] >> 31) == 0) + ((node[1] >> 31) == 0);
}

enum BuildTreeError { Ok, JumpTooFar, NotEnoughMemory };

static BuildTreeError pack_sub_tree(unsigned char *out, int &top, ogg_uint32_t* tree, int rootIdx, int leafSize, int &io_remaining, CAkVorbisAllocator& VorbisAllocator)
{
	int level = 1;
	GrowableAlloc& toProcess = VorbisAllocator.growingSpace[0];
	if (!toProcess.Grow(sizeof(short)))
		return NotEnoughMemory;
	
	toProcess[0] = rootIdx;
	ogg_uint32_t* pNode = tree + rootIdx;
	int childNodes = GetNodeSize(pNode, 1);
	GrowableAlloc& parents = VorbisAllocator.growingSpace[1];
	if(!parents.Grow(1))
		return NotEnoughMemory;

	GrowableAlloc& nextNodes = VorbisAllocator.growingSpace[2];
	GrowableAlloc& nextParents = VorbisAllocator.growingSpace[3];
	
	BuildTreeError err = Ok;

	int levelCount = 1;
	int nextCount = 0;	
	while (levelCount && err == Ok)
	{
		if(!nextNodes.Grow(childNodes*sizeof(short)))
			err = NotEnoughMemory;
		if(!nextParents.Grow(childNodes*sizeof(short) * 2))
			err = NotEnoughMemory;
		nextCount = 0;			
		childNodes = 0;
		int end = top + levelCount*2;
		for (int j = 0; j < levelCount && err == Ok; j++)
		{
			AKASSERT(toProcess[j] != 0);
			pNode = tree + toProcess[j];

			if (level > 1)
			{
				int offset = (top - (parents[j] & ~1)) / 2;
				if (offset > 0x7f)
					err = JumpTooFar;

				out[parents[j]] = offset;
			}

			if (pNode[0] & 0x80000000UL)	//If left is a leaf
			{
				if (pNode[1] & 0x80000000UL)	// If right is also a leaf
				{					
					if (leafSize == 1)
					{
						//Need 2 bytes
						out[top] = (pNode[0] & 0x7f) | 0x80;
						out[top + 1] = (pNode[1] & 0x7f) | 0x80;											
					}
					else
					{
						//Need 6 bytes
						//First 2 bytes are markers.  The symbol will be at the end of this level					
						out[top] = (end - top)/2 | 0x80;	//1 word of offset to the symbol
						out[top + 1] = (end + 2 - top)/2 | 0x80; //2 words of offset to the symbol
						ogg_uint16_t *pSym = (ogg_uint16_t *)(out + end);
						pSym[0] = (ogg_uint16_t)pNode[0];
						pSym[1] = (ogg_uint16_t)pNode[1];						
						end += 4;
					}			
					io_remaining -= 2;
				}
				else
				{
					//Left is a leaf and right is a node.								
					nextParents[nextCount] = top + 1;
					if (leafSize == 1)
					{
						//Need 2 bytes
						out[top] = (pNode[0] & 0x7f) | 0x80;																
					}
					else
					{
						//Need 4 bytes
						out[top] = (end - top)/2 | 0x80;
						ogg_uint16_t *pSym = (ogg_uint16_t *)(out + end);
						pSym[0] = (ogg_uint16_t)pNode[0];																
						end += 2;
					}																						
					
					//Prepare next level.  Add the next node to visit
					nextNodes[nextCount] = pNode[1];
					pNode[1] = 0;
					pNode = tree + nextNodes[nextCount];
					childNodes += GetNodeSize(pNode, leafSize);	//Size of the next node							
					nextCount++;

					io_remaining--;
				}
			}
			else
			{
				if (pNode[1] & 0x80000000UL)
				{
					//Left is a node, right is a leaf.  										
					nextParents[nextCount] = top;
					if (leafSize == 1)
					{
						//Need 2 bytes						
						out[top+1] = (pNode[1] & 0x7f) | 0x80;												
					}
					else
					{
						//Need 4 bytes						
						out[top+1] = (end - top)/2 | 0x80;
						ogg_uint16_t *pSym = (ogg_uint16_t *)(out + end);
						pSym[0] = (ogg_uint16_t)pNode[1];						
						end += 2;
					}

					//Prepare next level.  Add the next node to visit
					nextNodes[nextCount] = pNode[0];	
					pNode[0] = 0;
					pNode = tree + nextNodes[nextCount];
					childNodes += GetNodeSize(pNode, leafSize);	//Size of the next node														
					nextCount++;

					io_remaining--;
				}				
				else
				{
					//Both are nodes.					
					nextNodes[nextCount] = pNode[0];
					nextNodes[nextCount + 1] = pNode[1];		

					nextParents[nextCount] = top;
					nextParents[nextCount+1] = top+1;

					pNode[0] = 0;
					pNode[1] = 0;

					ogg_uint32_t* pTemp = tree + nextNodes[nextCount];
					childNodes += GetNodeSize(pTemp, leafSize);	//Size of the next node

					pTemp = tree + nextNodes[nextCount + 1];
					childNodes += GetNodeSize(pTemp, leafSize);	//Size of the next node
					
					nextCount += 2;
				}
			}		
			top += 2;
		}		

		if (leafSize == 2)
			top = end;

		toProcess.Swap(nextNodes);
		parents.Swap(nextParents);

		levelCount = nextCount;
		level++;
	}

	return err;
}


#if defined AK_WIN && defined DEBUG_HUFFMAN_TREE
#define DBG_WIDTH 100
void print_tree(ogg_uint32_t* tree, int rootIdx)
{
	int level = 1;
	int *toProcess = (int*)malloc(sizeof(int));		//All nodes to process, 	
	*toProcess = rootIdx;	//Start with the root node
	ogg_uint32_t* pNode = tree + rootIdx;
	char line[DBG_WIDTH+1];
	int cur = 0;
	
	int levelCount = 1;
	int nextCount = 4;
	while (levelCount)
	{
		int *nextNodes = (int*)malloc(nextCount*sizeof(int)*2);
		nextCount = 0;				
		int cur = 0;
		int offset = 0;
		int div = DBG_WIDTH / (levelCount*2 + 1);
		for (int q = 0; q < div; q++, cur++)
			line[cur] = ' ';
		for (int j = 0; j < levelCount; j++)
		{
			AKASSERT(toProcess[j] != 0);
			pNode = tree + toProcess[j];
			if (pNode[0] & 0x80000000UL)	//If left is a leaf	
			{
				sprintf(line + cur, "%X", pNode[0] & 0x7FFFFFFF);
				cur = strlen(line);
			}
			else
			{
				line[cur] = '+';
				line[cur+1] = '-';
				nextNodes[nextCount] = pNode[0];
				nextCount++;
				cur += 2;
			}			

			for (int q = 5; q < div; q++, cur++)
				line[cur] = '-';
				
			if (pNode[1] & 0x80000000UL)	//If left is a leaf		
			{
				sprintf(line + cur, "%X", pNode[1] & 0x7FFFFFFF);
				cur = strlen(line);
			}
			else
			{
				line[cur] = '-';
				line[cur + 1] = '+';
				nextNodes[nextCount] = pNode[1];
				nextCount++;
				cur += 2;
			}
			
			if (j < levelCount-1)
				for (int q = 0; q < div; q++, cur++)
					line[cur] = ' ';
		}		
		line[cur] = 0;
		OutputDebugStringA(line);
		OutputDebugStringA("\n");
		free(toProcess);
		toProcess = nextNodes;
		levelCount = nextCount;
		level++;
	}
	free(toProcess);
}

#define DBG_WIDTH 100
void print_tree_pack(unsigned char* tree, int rootIdx)
{
	int level = 1;
	int *toProcess = (int*)malloc(sizeof(int));		//All nodes to process, 	
	*toProcess = rootIdx*2;	//Start with the root node
	unsigned char* pNode = tree + rootIdx*2;
	char line[DBG_WIDTH + 1];
	int cur = 0;

	int levelCount = 1;
	int nextCount = 4;
	while (levelCount)
	{
		int *nextNodes = (int*)malloc(nextCount*sizeof(int) * 2);
		nextCount = 0;
		int cur = 0;
		int offset = 0;
		int div = DBG_WIDTH / (levelCount * 2 + 1);
		for (int q = 0; q < div; q++, cur++)
			line[cur] = ' ';
		for (int j = 0; j < levelCount; j++)
		{
			AKASSERT(toProcess[j] != 0);
			pNode = tree + toProcess[j];
			if (pNode[0] & 0x80UL)	//If left is a leaf	
			{
				sprintf(line + cur, "%X", pNode[0] & 0x7F);
				cur = strlen(line);
			}
			else
			{
				line[cur] = '+';
				line[cur + 1] = '-';
				nextNodes[nextCount] = pNode + pNode[0]*2 - tree;
				nextCount++;
				cur += 2;
			}

			for (int q = 5; q < div; q++, cur++)
				line[cur] = '-';

			if (pNode[1] & 0x80UL)	//If left is a leaf		
			{
				sprintf(line + cur, "%X", pNode[1] & 0x7F);
				cur = strlen(line);
			}
			else
			{
				line[cur] = '-';
				line[cur + 1] = '+';
				nextNodes[nextCount] = pNode + pNode[1]*2 - tree;
				nextCount++;
				cur += 2;
			}

			if (j < levelCount - 1)
				for (int q = 0; q < div; q++, cur++)
					line[cur] = ' ';
		}
		line[cur] = 0;
		OutputDebugStringA(line);
		OutputDebugStringA("\n");
		free(toProcess);
		toProcess = nextNodes;
		levelCount = nextCount;
		level++;
	}
	free(toProcess);
}
#endif

inline ogg_int32_t ShiftRight(const ogg_int32_t x, const int bits)
{
	//Shifting negative is like rotating by 32-bits
	//This implementation works only if x is smaller than 16 bits.  If not, we need to flush the LSBs as well.
	const int n = (32 + bits) & 31;
	return (x >> n) | (x << (32 - n));	//Rotate left
}

BuildTreeError BuildTree(codebook* s,
	char*				lengthlist,
	ogg_int32_t			quantvals,
	oggpack_buffer*		opb,
	CodebookBuildParam * in_pBuild,
	CAkVorbisAllocator&	VorbisAllocator,
	int lut_Size)
{
	int i;
	ogg_uint32_t *work;
	int lutSymbols = 0;

	int lutItems = 1 << lut_Size;
	s->lut_Mask = lutItems - 1;
	int workSize = lutItems + (in_pBuild->used_entries * 2 - 2);
	work = (ogg_uint32_t*)alloca(workSize*sizeof(*work));
	char * pExtraBits = (char*)alloca(lutItems);

	int leafSize;
	int slots = _make_words(lengthlist, in_pBuild->entries, work, quantvals, s, opb, in_pBuild, lut_Size, lutSymbols, pExtraBits, leafSize) - lutItems;
	int remainingSymbols = in_pBuild->used_entries - lutSymbols;
	int nodes = slots - remainingSymbols;

	//LUT takes 2 bytes per entry (because of large offsets in large trees) plus one byte for the bit length of the symbol.	
	int AllocSize = lutItems * 3 + remainingSymbols*leafSize + nodes;
	s->dec_leafw = leafSize;
	if(s->dec_leafw == 2)
		AllocSize += remainingSymbols;		//Unfortunately, big leaves cost more.
	
	s->dec_table = VorbisAllocator.AllocDecodingTable(AllocSize);
	if(s->dec_table == NULL)
		return NotEnoughMemory;

	ogg_uint16_t* lut = (ogg_uint16_t*)s->dec_table;
	int top = lutItems * 2;
	s->lut_lengths = (unsigned char*)(lut + lutItems);
	top += lutItems;

	//Pack the lut, always 16 bits.	...  Maybe have another variation for small trees?
	BuildTreeError err = Ok;
	for (i = 0; i < lutItems && err == Ok; i++)
	{
		s->lut_lengths[i] = pExtraBits[i];
		if (work[i] & 0x80000000UL)
		{
			lut[i] = (ogg_uint16_t)work[i] | 0x8000;
		}
		else
		{
			lut[i] = top / 2;
			err = pack_sub_tree((unsigned char*)s->dec_table, top, work, work[i], leafSize, remainingSymbols, VorbisAllocator);
		}
	}

	if(err != Ok)
	{
		if (s->dec_table != NULL)
		{
			VorbisAllocator.FreeDecodingTable(s->dec_table);
			s->dec_table = NULL;
		}
	}
	else
	{
		AKASSERT(remainingSymbols == 0);
		AKASSERT(top <= AllocSize);
		AKASSERT(s->dec_leafw <= 2);	//4 byte leaves not supported yet.
	}
	return err;
}

static ogg_uint32_t ComputeOldSize(CodebookBuildParam * in_pBuild, codebook* s)
{
	return (in_pBuild->used_entries * (s->dec_leafw + 1) - 2) * in_pBuild->dec_nodeb;
}

static decode_ptr DecodeFunctions_Scalar[2][2][4] =
{
	//Leaf size = 1
	{
		//Mono
		{
			vorbis_book_decodev_add_t<ExpandRecursive<MonoPolicy, 1>, LeafByte>,
			vorbis_book_decodev_add_t<ExpandRecursive<MonoPolicy, 2>, LeafByte>,
			vorbis_book_decodev_add_t<ExpandRecursive<MonoPolicy, 4>, LeafByte>,
			vorbis_book_decodev_add_t<ExpandRecursive<MonoPolicy, 8>, LeafByte>
		},
		//Stereo
		{
			vorbis_book_decodev_add_t<ExpandEntry1Stereo, LeafByte>,
			vorbis_book_decodev_add_t<ExpandRecursive<StereoPolicy, 2>, LeafByte>,
			vorbis_book_decodev_add_t <ExpandRecursive<StereoPolicy, 4>, LeafByte>,
			vorbis_book_decodev_add_t<ExpandRecursive<StereoPolicy, 8>, LeafByte>
		}
	},
	//Leaf size = 2
	{
		//Mono
		{
			vorbis_book_decodev_add_t<ExpandRecursive<MonoPolicy, 1>, LeafWord>,
			vorbis_book_decodev_add_t<ExpandRecursive<MonoPolicy, 2>, LeafWord>,
			vorbis_book_decodev_add_t<ExpandRecursive<MonoPolicy, 4>, LeafWord>,
			vorbis_book_decodev_add_t<ExpandRecursive<MonoPolicy, 8>, LeafWord>
		},
		//Stereo
		{
			vorbis_book_decodev_add_t<ExpandEntry1Stereo, LeafWord>,
			vorbis_book_decodev_add_t<ExpandRecursive<StereoPolicy, 2>, LeafWord>,
			vorbis_book_decodev_add_t <ExpandRecursive<StereoPolicy, 4>, LeafWord>,
			vorbis_book_decodev_add_t<ExpandRecursive<StereoPolicy, 8>, LeafWord>
		}
	}
};

#if defined AKSIMD_V4F32_SUPPORTED
static decode_ptr DecodeFunctions_SIMD[2][2][4] =
{
	//Leaf size = 1
	{
		//Mono
		{
			vorbis_book_decodev_add_t<ExpandRecursive<MonoPolicy, 1>, LeafByte>,
			vorbis_book_decodev_add_SIMD_2<LeafByte>,
			vorbis_book_decodev_add_SIMD_4<LeafByte>
		},
		//Stereo
		{
			vorbis_book_decodev_add_t<ExpandEntry1Stereo, LeafByte>,
			vorbis_book_decodev_add_t<ExpandRecursive<StereoPolicy, 2>, LeafByte>,	//Still faster than the SIMD version.
			vorbis_book_decodev_add_SIMD_4_2ch<LeafByte>,
			vorbis_book_decodev_add_SIMD_8_2ch<LeafByte>
		}
	},
	//Leaf size = 2
	{
		//Mono
		{
			vorbis_book_decodev_add_t<ExpandRecursive<MonoPolicy, 1>, LeafWord>,
			vorbis_book_decodev_add_SIMD_2<LeafWord>,
			vorbis_book_decodev_add_SIMD_4<LeafWord>
		},
		//Stereo
		{
			vorbis_book_decodev_add_t<ExpandEntry1Stereo, LeafWord>,
			vorbis_book_decodev_add_t<ExpandRecursive<StereoPolicy, 2>, LeafWord>,	//Still faster than the SIMD version.
			vorbis_book_decodev_add_SIMD_4_2ch<LeafWord>,
			vorbis_book_decodev_add_SIMD_8_2ch<LeafWord>
		}
	}
};
#endif

static int _make_decode_table(codebook*			s,
	char*				lengthlist,
	ogg_int32_t			quantvals,
	oggpack_buffer*		opb,
	CodebookBuildParam *in_pBuild,
	CAkVorbisAllocator&	VorbisAllocator,
	int channels)
{	
		
	if(in_pBuild->dec_nodeb == 4)
	{
		AKASSERT(!"NOT TESTED!  NOT WORKING");
		//int AllocSize = (s->used_entries * 2 + 1)*sizeof(*work);
		//s->dec_table = VorbisAllocator.Alloc(AllocSize);
		/* +1 (rather than -2) is to accommodate 0 and 1 sized books,
		which are specialcased to nodeb==4 */
		//_make_words(lengthlist,s->entries,(ogg_uint32_t*)s->dec_table,quantvals,s,opb,maptype, lutSymbols);		
	}

	//AUDIOKINETIC
	//Compute the expected size of this codebook.  
	//This is done because we want to avoid forcing users to regenerate their vorbis files because of the LUT optimization
	ogg_uint32_t uOldSize = ComputeOldSize(in_pBuild, s);
	VorbisAllocator.AddBookSize(uOldSize);

	//Find the min bit length
	int lut_Min = 0xFF;
	for(int i = 0; i < in_pBuild->used_entries; i++)
	{
		if (lengthlist[i] > 0 && lut_Min > lengthlist[i])
			lut_Min = lengthlist[i];
	}

	//Put 1 extra bits than the minimum.  Why?  Because it brings the average hit in the LUT over 50% in my tests.
	int lut_Size = lut_Min + 1;	
	BuildTreeError bTreeResult = Ok;
	do
	{
		lut_Size++;
		bTreeResult = BuildTree(s, lengthlist, quantvals, opb, in_pBuild, VorbisAllocator, lut_Size);
		//In case of failure, the tree could not be built because the distance between nodes was too far.  
		//Enlarging the LUT will split the subtrees in more pieces, reducing the node distance.
	} while(bTreeResult == JumpTooFar);

	if(bTreeResult == NotEnoughMemory)
		return OV_NOMEM;

	if(in_pBuild->dec_type == 1 && s->pResidueParams->q_bits > 0)
	{			
		//This is a code book for residue, compute constants.
		int shiftM = -8 - in_pBuild->q_delp;
		s->pResidueParams->add = ShiftRight(in_pBuild->q_min, -8 - in_pBuild->q_minp);
		s->pResidueParams->mask = (1 << s->pResidueParams->q_bits) - 1;
		s->pResidueParams->q_delShift = ShiftRight(s->pResidueParams->q_del, shiftM);

		int dimIdx = 0;
		for(; s->dim != (1 << dimIdx); dimIdx++);	//Bit count.

		AKASSERT(s->dec_leafw <= 2);
		AKASSERT(channels <= 2);
		AKASSERT(dimIdx <= 8);

#if defined AKSIMD_V4F32_SUPPORTED
		AKASSERT(ShiftRight(s->pResidueParams->q_delShift, -shiftM) == s->pResidueParams->q_del);	//Make sure we didn't loose anything.
			
		s->pResidueParams->Decode = DecodeFunctions_SIMD[s->dec_leafw - 1][channels - 1][dimIdx];

		if (s->dim == 8 || s->dim == 4 || s->dim == 2)
		{
			s->pResidueParams->add_simd = AKSIMD_SET_V4I32(s->pResidueParams->add);
			AKSIMD_GETELEMENT_V4I32(s->pResidueParams->mask_simd, 0) = s->pResidueParams->mask;
			AKSIMD_GETELEMENT_V4I32(s->pResidueParams->mask_simd, 1) = s->pResidueParams->mask << (s->pResidueParams->q_bits * 1);
			AKSIMD_GETELEMENT_V4I32(s->pResidueParams->mask_simd, 2) = s->pResidueParams->mask << (s->pResidueParams->q_bits * 2);
			AKSIMD_GETELEMENT_V4I32(s->pResidueParams->mask_simd, 3) = s->pResidueParams->mask << (s->pResidueParams->q_bits * 3);
		}

		if (s->dim == 8)
		{
			//Find lowest bit of q_del to check how much we can shift it for the common multiplication operation.
			//Normally, in scalar version of EXPAND, we'd need to shift and mask the Entry, then multiply by q_delShift.  But since the SIMD shift can't be of different 
			//count, we'll use the multiply as a divisor for the upper parts of Entry.  
			AKSIMD_GETELEMENT_V4I32(s->pResidueParams->q_del_simd,0) = s->pResidueParams->q_delShift;
			AKSIMD_GETELEMENT_V4I32(s->pResidueParams->q_del_simd,1) = s->pResidueParams->q_delShift >> (s->pResidueParams->q_bits * 1);
			AKSIMD_GETELEMENT_V4I32(s->pResidueParams->q_del_simd,2) = s->pResidueParams->q_delShift >> (s->pResidueParams->q_bits * 2);
			AKSIMD_GETELEMENT_V4I32(s->pResidueParams->q_del_simd,3) = s->pResidueParams->q_delShift >> (s->pResidueParams->q_bits * 3);
																	  
			AKASSERT(s->pResidueParams->q_delShift * ((1 << s->pResidueParams->q_bits) - 1) < 65536);	//If this pops, the _mm_mullo_epi16 call will overflow and the result will be wrong. 
			AKASSERT(s->pResidueParams->q_delShift == (AKSIMD_GETELEMENT_V4I32(s->pResidueParams->q_del_simd, 3) << (s->pResidueParams->q_bits * 3)));	//Make sure we didn't loose anything
		}

		if (s->dim == 4 || s->dim == 2)
		{
			//Find lowest bit of q_del to check how much we can shift it for the common multiplication operation.
			//Normally, in scalar version of EXPAND, we'd need to shift and mask the Entry, then multiply by q_delShift.  But since the SIMD shift can't be of different 
			//count, we'll use the multiply as a divisor for the upper parts of Entry. 
			//However, contrary to the 8-dimension version, the bit width can be larger and we could loose precision.  So we need an extra shift, which will be common.

			int lsb = -in_pBuild->q_delp;	//Lowest bit.

			int commonShift = lsb - (s->pResidueParams->q_bits * 3);

			ogg_uint32_t tqdel = s->pResidueParams->q_delShift;
			if (shiftM - commonShift > 0)
			{
				s->pResidueParams->shift_simd = shiftM - commonShift;
				tqdel = s->pResidueParams->q_del >> commonShift;
			}
			else
				s->pResidueParams->shift_simd = 0;

			AKSIMD_GETELEMENT_V4I32(s->pResidueParams->q_del_simd, 0) = tqdel;
			AKSIMD_GETELEMENT_V4I32(s->pResidueParams->q_del_simd, 1) = tqdel >> (s->pResidueParams->q_bits * 1);
			AKSIMD_GETELEMENT_V4I32(s->pResidueParams->q_del_simd, 2) = tqdel >> (s->pResidueParams->q_bits * 2);
			AKSIMD_GETELEMENT_V4I32(s->pResidueParams->q_del_simd, 3) = tqdel >> (s->pResidueParams->q_bits * 3);

			if (tqdel * ((1 << s->pResidueParams->q_bits) - 1) >= 65536)
			{
				//If this pops, the _mm_mullo_epi16 call will overflow and the result will be wrong. 
				// Use the scalar decoding method		
				s->pResidueParams->Decode = DecodeFunctions_Scalar[s->dec_leafw - 1][channels - 1][dimIdx];
			}
			else			
				AKASSERT(tqdel == (AKSIMD_GETELEMENT_V4I32(s->pResidueParams->q_del_simd, 3) << (s->pResidueParams->q_bits * 3)));	//Make sure we didn't loose anything
		}
#else
		s->pResidueParams->Decode = DecodeFunctions_Scalar[s->dec_leafw - 1][channels - 1][dimIdx];
#endif		
	}	

	return 0;
}



/* most of the time, entries%dimensions == 0, but we need to be
   well defined.  We define that the possible vales at each
   scalar is values == entries/dim.  If entries%dim != 0, we'll
   have 'too few' values (values*dim<entries), which means that
   we'll have 'left over' entries; left over entries use zeroed
   values (and are wasted).  So don't generate codebooks like
   that */
/* there might be a straightforward one-line way to do the below
   that's portable and totally safe against roundoff, but I haven't
   thought of it.  Therefore, we opt on the side of caution */
static ogg_int32_t _book_maptype1_quantvals(codebook *b, CodebookBuildParam* in_pBuild){
  /* get us a starting hint, we'll polish it below */
	int bits = _ilog(in_pBuild->entries);
	int vals = in_pBuild->entries >> ((bits - 1)*(b->dim - 1) / b->dim);

  while(1){
    long acc=1;
    long acc1=1;
    int i;
    for(i=0;i<b->dim;i++){
      acc*=vals;
      acc1*=vals+1;
    }
	if(acc <= in_pBuild->entries && acc1>in_pBuild->entries){
      return(vals);
    }else{
		if(acc>in_pBuild->entries){
        vals--;
      }else{
        vals++;
      }
    }
  }
}

int vorbis_book_unpack(oggpack_buffer *opb,codebook *s, CAkVorbisAllocator& VorbisAllocator, int channels)
{
	int		quantvals=0;
	ogg_int32_t	i,j;	
	int		ak_lenbits;
	memset(s,0,sizeof(*s));

	CodebookBuildParam bookParams;

	/* first the basic parameters */
	s->dim = oggpack_read(opb,4);
	bookParams.entries = oggpack_read(opb, 14);
	bookParams.used_entries = 0;

	char * lengthlist = (char *)alloca(sizeof(*lengthlist)*bookParams.entries);

	/* codeword ordering.... length ordered or unordered? */
	if ( oggpack_read(opb,1) == 0 )
	{
		/* unordered */

		ak_lenbits = oggpack_read(opb,3);

		/* allocated but unused entries? */
		if(oggpack_read(opb,1))
		{
			/* yes, unused entries */
			for(i=0;i<bookParams.entries;i++)
			{
				if(oggpack_read(opb,1))
				{
					ogg_int32_t num=oggpack_read_var(opb,ak_lenbits);
					lengthlist[i]=num+1;
					bookParams.used_entries++;
					if(num+1>s->dec_maxlength)
					{
						s->dec_maxlength=num+1;
					}
				}
				else
				{
					lengthlist[i]=0;
				}
			}
		}
		else
		{
			/* all entries used; no tagging */
			bookParams.used_entries = bookParams.entries;
			for(i = 0; i<bookParams.entries; i++)
			{
				ogg_int32_t num=oggpack_read_var(opb,ak_lenbits);
				lengthlist[i]=num+1;
				if(num+1>s->dec_maxlength)
				{
					s->dec_maxlength=num+1;
				}
			}
		}
	}
	else // 1
	{
		/* ordered */
		ogg_int32_t length=oggpack_read(opb,5)+1;

		bookParams.used_entries = bookParams.entries;

		for(i = 0; i<bookParams.entries;)
		{
			ogg_int32_t num = oggpack_read_var(opb, _ilog(bookParams.entries - i));
			for(j = 0; j<num && i<bookParams.entries; j++, i++)
			{
				lengthlist[i]=length;
			}
			s->dec_maxlength=length;
			length++;
		}
	}

	/* Do we have a mapping to unpack? */
	int err = 0;
	bookParams.maptype = oggpack_read(opb, 1);
	if(bookParams.maptype == 0)
	{
		/* no mapping; decode type 0 */

		/* how many bytes for the indexing? */
		/* this is the correct boundary here; we lose one bit to
		node/leaf mark */
		bookParams.dec_nodeb = _determine_node_bytes(bookParams.used_entries, _ilog(bookParams.entries) / 8 + 1);
		s->dec_leafw = _determine_leaf_words(bookParams.dec_nodeb, _ilog(bookParams.entries) / 8 + 1);
		bookParams.dec_type		= 0;

		err = _make_decode_table(s, lengthlist, quantvals, opb, &bookParams, VorbisAllocator, channels);		 
	}
	else // maptype == 1
	{
		s->pResidueParams = (ResidueBook*)AkMalign(AkMemID_Processing, sizeof(ResidueBook), AK_SIMD_ALIGNMENT);
		if(s->pResidueParams == NULL)
			return OV_NOMEM;

		s->pResidueParams->Decode = NULL;

		bookParams.q_min = _float32_unpack(oggpack_read(opb, 32), &bookParams.q_minp);
		s->pResidueParams->q_del = _float32_unpack(oggpack_read(opb, 32), &bookParams.q_delp);
		s->pResidueParams->q_bits = oggpack_read(opb, 4) + 1;
		/*s->q_seq	= */oggpack_read(opb,1);

		s->pResidueParams->q_del >>= s->pResidueParams->q_bits;
		bookParams.q_delp += s->pResidueParams->q_bits;

		/* mapping type 1; implicit values by lattice  position */
		quantvals=_book_maptype1_quantvals(s, &bookParams);

		/* dec_type choices here are 1,2; 3 doesn't make sense */
		{			
			/* packed values */
			long total1 = (s->pResidueParams->q_bits*s->dim + 8) / 8; /* remember flag bit */
			/* vector of column offsets; remember flag bit */
			long total2 = (_ilog(quantvals - 1)*s->dim + 8) / 8 + (s->pResidueParams->q_bits + 7) / 8;

			AKASSERT( total1<=4 && total1<=total2 );
//			if(total1<=4 && total1<=total2)
			{
				/* use dec_type 1: vector of packed values */

				/* need quantized values before  */
				bookParams.q_val = alloca(sizeof(ogg_uint16_t)*quantvals);

				for(i=0;i<quantvals;i++)
					((ogg_uint16_t *)bookParams.q_val)[i] = oggpack_read_var(opb, s->pResidueParams->q_bits);

				bookParams.dec_type = 1;
				bookParams.dec_nodeb = _determine_node_bytes(bookParams.used_entries, (int)total1);
				s->dec_leafw = _determine_leaf_words(bookParams.dec_nodeb, (int)total1);

				err = _make_decode_table(s,lengthlist,quantvals,opb,&bookParams,VorbisAllocator, channels);
				bookParams.q_val = 0; /* about to go out of scope; _make_decode_table
							was using it */

				AKASSERT(err != 0 || s->pResidueParams->Decode);
			}
		}
	}
	return err;
}

ogg_int32_t ak_vorbis_book_decode(codebook *book, oggpack_buffer *b)
{
	AkUInt64 bitsEncoded = oggpack_look(b, 24);
	int bits = 0;
	ogg_int32_t ret = 0;

	/* chase the tree with the bits we got */	
	if (book->dec_leafw == 1)
	{		
		ret = FindSymbol<LeafByte>(book, bitsEncoded, bits);
	}
	else if (book->dec_leafw == 2)
	{
		ret = FindSymbol<LeafWord>(book, bitsEncoded, bits);
	}
	else
	{
		AKASSERT(!"NOT IMPLEMENTED YET");
	}

	oggpack_adv(b, bits);
	return ret;
}
