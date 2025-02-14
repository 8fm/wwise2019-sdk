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

 function: subsumed libogg includes

 ********************************************************************/
#ifndef _OGG_H
#define _OGG_H

#include "os_types.h"
#include <AK/Tools/Common/AkAssert.h>
#include <AK/AkPlatforms.h>

//MJEAN 
//Defines which optimisations to enable on CPUs.
#ifndef LITTLE_ENDIAN
	#if defined AK_CPU_X86 || defined AK_CPU_X86_64 || defined AK_CPU_ARM || defined AK_CPU_ARM_64
	#define LITTLE_ENDIAN
	#endif
#endif //LITTLE_ENDIAN

#if defined AK_CPU_X86 || defined AK_CPU_X86_64
#define UNALIGNED_DATA_TYPES
#endif

#include <stdio.h>

#ifdef _DEBUG
#define PRINT	printf
#else
#define PRINT
#endif

class GrowableAlloc
{
public:
	GrowableAlloc()
		: m_pData(NULL)
		, m_size(0)
	{}

	~GrowableAlloc()
	{
		if (m_pData)
			AkFree(AkMemID_Processing, m_pData);
	}

	inline void Free()
	{
		if (m_pData)
		{
			AkFree(AkMemID_Processing, m_pData);
			m_pData = NULL;
			m_size = 0;
		}
	}

	inline bool Grow(int in_size)
	{
		if (in_size > m_size)
		{
			in_size = AkMax(in_size, 256);//Do not uselessly allocate less than 256 saving hundreds of re-allocations.

			short* pNewAllocation = (short*)AkAlloc(AkMemID_Processing, in_size);
			if (!pNewAllocation)
				return false;// Failed to grow

			//Success
			if (m_pData)
				AkFree(AkMemID_Processing, m_pData);

			m_size = in_size;
			m_pData = pNewAllocation;
		}
		return true;
	}

	short operator [](int i) const    { return m_pData[i]; }
	short & operator [](int i) { return m_pData[i]; }

	inline void Swap(GrowableAlloc& in_other)
	{
		short* temp_pData = m_pData;
		int temp_size = m_size;
		m_pData = in_other.m_pData;
		m_size = in_other.m_size;
		in_other.m_pData = temp_pData;
		in_other.m_size = temp_size;
	}

private:
	short* m_pData;
	int m_size;
};

#define NUM_VORBIS_GROWING_ALLOCATOR 4
//================================================================================
// allocation using some memory block
//================================================================================
class CAkVorbisAllocator
{
public:
	CAkVorbisAllocator()
	{
		pStartAddress   = NULL;
		pCurrentAddress	= NULL;
		CurrentSize		= 0;
		MaxSize			= 0;
		BooksSize		= 0;
	}

	inline void Init(unsigned int Size)
	{
		MaxSize = Size;
	}

	inline bool Reserve()
	{
		MaxSize -= BooksSize;	//In this implementation, the books are in another allocation.
		pStartAddress = (AkUInt8*)AkAlloc(AkMemID_Processing, MaxSize);
		pCurrentAddress = pStartAddress;		
		return pCurrentAddress != NULL;
	}

	inline AkUInt8* AllocDecodingTable(unsigned int Size)
	{
		return (AkUInt8*)AkAlloc(AkMemID_Processing, Size);
	}

	inline void FreeDecodingTable(void* in_pTable)
	{
		AkFree(AkMemID_Processing, in_pTable);
	}	

	inline void Term()
	{
		if ( pStartAddress )
		{
			AkFree( AkMemID_Processing, pStartAddress );
			pCurrentAddress	= 0;
			CurrentSize		= 0;
			MaxSize			= 0;
			pStartAddress	= NULL;
		}
	}

	inline AkUInt8* GetAddress()
	{
		return pStartAddress;
	}

	inline AkUInt32 GetCurrentSize()
	{
		return CurrentSize;
	}

	inline AkUInt32 GetMaxSize()
	{
		return MaxSize;
	}

	inline void* Alloc(unsigned int Size)
	{
		if(Size != 0)
		{
			unsigned int AlignedSize = ((Size + 3) & ~3);
			unsigned int NextSize = CurrentSize + AlignedSize;
			if(NextSize <= MaxSize)
			{
				AkUInt8* pAddress = pCurrentAddress;
				pCurrentAddress += AlignedSize;
				CurrentSize += AlignedSize;
				return pAddress;
			}
			else
			{
				AKASSERT( !"No more UVM memory");
			}
		}
		return NULL;
	}

	inline void* Calloc(unsigned int Size)
	{
		void* pAddress = Alloc(Size);
		if(pAddress != NULL)
		{
			AKPLATFORM::AkMemSet(pAddress,0,Size);
		}
		return pAddress;
	}

	inline void AddBookSize(unsigned int Size)
	{
		BooksSize += Size;
	}

	GrowableAlloc growingSpace[NUM_VORBIS_GROWING_ALLOCATOR];

private:
	AkUInt8*		pStartAddress;
	AkUInt8*		pCurrentAddress;
	unsigned int	CurrentSize;
	unsigned int	MaxSize;
	unsigned int	BooksSize;
} AK_ALIGN_DMA;

typedef struct ogg_buffer_state
{
	struct ogg_buffer    *unused_buffers;
	struct ogg_reference *unused_references;
	int                   outstanding;
	int                   shutdown;
} ogg_buffer_state;

typedef struct ogg_buffer
{
	unsigned char*	data;
	int				size;
} ogg_buffer;

typedef struct oggpack_buffer 
{
#ifdef UNALIGNED_DATA_TYPES
  unsigned char *headptr;
#else
  AkUInt32 *headptr;
#endif
  char            headbit;
  int			 headend;    
} oggpack_buffer;


typedef struct oggbyte_buffer
{
	ogg_reference*	baseref;

	ogg_reference*	ref;
	unsigned char*	ptr;
	int				pos;
	int				end;
} oggbyte_buffer;

typedef struct ogg_sync_state 
{
	/* decode memory management pool */
	ogg_buffer_state *bufferpool;

	/* stream buffers */
	ogg_reference    *fifo_head;
	ogg_reference    *fifo_tail;
	int				 fifo_fill;

	/* stream sync management */
	int               unsynced;
	int               headerbytes;
	int               bodybytes;
} ogg_sync_state;

typedef struct ogg_stream_state
{
	ogg_reference *header_head;
	ogg_reference *header_tail;
	ogg_reference *body_head;
	ogg_reference *body_tail;

	int            e_o_s;	/* set when we have buffered the last
							packet in the logical bitstream */
	int            b_o_s;	/* set after we've written the initial page
							of a logical bitstream */
	int			   serialno;
	int			   pageno;
	ogg_int64_t    packetno;	/* sequence number for decode; the framing
								knows where there's a hole in the data,
								but we need coupling so that the codec
								(which is in a seperate abstraction
								layer) also knows about the gap */
	ogg_int64_t    granulepos;

	int            lacing_fill;
	ogg_uint32_t   body_fill;

	/* decode-side state data */
	int            holeflag;
	int            spanflag;
	int            clearflag;
	int            laceptr;
	ogg_uint32_t   body_fill_next;

} ogg_stream_state;

struct ogg_packet
{
	ogg_buffer		buffer;
	unsigned char	e_o_s;
};

/* Ogg BITSTREAM PRIMITIVES: bitstream ************************/

extern ogg_uint32_t bitwise_mask[];

inline int oggpack_eop(const oggpack_buffer *b)
{
	return b->headend < 0;	
}

#ifdef UNALIGNED_DATA_TYPES
static AkForceInline void oggpack_readinit( oggpack_buffer *b,ogg_buffer *r )
{
  b->headbit = 0;
  b->headptr = r->data;
  b->headend = r->size;    
}

inline void oggpack_adv(oggpack_buffer *b,int bits)
{  
  bits+=b->headbit;
  b->headbit=bits&7;
  b->headend-=(bits>>3);
  b->headptr+=(bits>>3);
}

//Functions for CPUs that support unaligned pointers to some data types.
inline ogg_int32_t oggpack_look(oggpack_buffer *b, int bits)
{		
	ogg_uint32_t m = bitwise_mask[bits];
	ogg_int32_t ret = 0;	
#ifdef LITTLE_ENDIAN
	ret = (ogg_int32_t)(*(AkInt64*)b->headptr >> b->headbit);
#else
	bits += b->headbit;
	/* make this a switch jump-table */
	ret = b->headptr[0] >> b->headbit;
	if (bits>8)
	{
		ret |= b->headptr[1] << (8 - b->headbit);
		if (bits>16)
		{
			ret |= b->headptr[2] << (16 - b->headbit);
			if (bits>24)
			{
				ret |= b->headptr[3] << (24 - b->headbit);
				if (bits>32 && b->headbit)
					ret |= b->headptr[4] << (32 - b->headbit);
			}
		}
	}

#endif

	ret &= m;
	return ret;
}


/* bits <= 32 */
template <int BITS>
inline ogg_int32_t oggpack_read_t(oggpack_buffer *b)
{
	ogg_int32_t ret = oggpack_look(b, BITS);
	oggpack_adv(b, BITS);
	return(ret);
}

template<>
inline ogg_int32_t oggpack_read_t<1>(oggpack_buffer *b)
{
	ogg_int32_t ret = (b->headptr[0] >> b->headbit) & 1;
	oggpack_adv(b, 1);
	return ret;
}

#ifdef LITTLE_ENDIAN

#define READ_BYTE_TEMPLATE(_BITS) \
template <>\
inline ogg_int32_t oggpack_read_t<_BITS>(oggpack_buffer *b)\
{\
	ogg_int32_t ret = ((*(AkUInt16*)b->headptr >> b->headbit) & ((1<<_BITS)-1));\
	oggpack_adv(b, _BITS);\
	return ret;\
}

READ_BYTE_TEMPLATE(2)
READ_BYTE_TEMPLATE(3)
READ_BYTE_TEMPLATE(4)
READ_BYTE_TEMPLATE(5)
READ_BYTE_TEMPLATE(6)
READ_BYTE_TEMPLATE(7)

template <>
inline ogg_int32_t oggpack_read_t<8>(oggpack_buffer *b)
{
	ogg_int32_t ret = (char)((*(AkInt16*)b->headptr >> b->headbit));	//Use simple cast, it is free on most platforms.
	oggpack_adv(b, 8);
	return ret;
}

template <>
inline ogg_int32_t oggpack_read_t<16>(oggpack_buffer *b)
{
	ogg_int32_t ret = (AkInt16)((*(AkUInt32*)b->headptr >> b->headbit));	//Use simple cast, it is free on most platforms.
	oggpack_adv(b, 16);
	return ret;
}
#endif

#else
//Functions for CPUs that don't support unaligned pointers to some data types.
static AkForceInline void oggpack_readinit(oggpack_buffer *b, ogg_buffer *r)
{		
	//Align the pointer and make it look like we already consumed the extra.
	b->headptr = reinterpret_cast<AkUInt32*>((reinterpret_cast<AkUIntPtr>(r->data) & (AkUIntPtr)(~3)));
	b->headbit = reinterpret_cast<AkUIntPtr>(r->data) & (AkUIntPtr)(3);
	b->headend = r->size + b->headbit;
	b->headbit *= 8;
}

inline void oggpack_adv(oggpack_buffer *b, int bits)
{
	bits += b->headbit;
	b->headbit = bits & 31;
	b->headend -= (bits >> 5);
	b->headptr += (bits >> 5);
}

inline ogg_int32_t oggpack_look(oggpack_buffer *b, int bits)
{
	ogg_uint32_t m = bitwise_mask[bits];
	AkUInt64 ret = 0;
#ifdef LITTLE_ENDIAN
	ret = ((AkUInt64)b->headptr[0] | ((AkUInt64)b->headptr[1] << 32));
	ret >>= b->headbit;
#else
		Unsupported, code it!
#endif
	ret &= m;
	return (ogg_int32_t)ret;
}

/* bits <= 32 */
template <int BITS>
inline ogg_int32_t oggpack_read_t(oggpack_buffer *b)
{
	ogg_int32_t ret = oggpack_look(b, BITS);
	oggpack_adv(b, BITS);
	return(ret);
}

template<>
inline ogg_int32_t oggpack_read_t<1>(oggpack_buffer *b)
{
	ogg_int32_t ret = (b->headptr[0] >> b->headbit) & 1;
	oggpack_adv(b, 1);
	return ret;
}
#endif


#define oggpack_read(BUF, BITS) oggpack_read_t<BITS>(BUF)

inline ogg_int32_t oggpack_read_var(oggpack_buffer *b, int bits)
{
	ogg_int32_t ret = oggpack_look(b, bits);
	oggpack_adv(b, bits);
	return(ret);
}

/* Ogg BITSTREAM PRIMITIVES: decoding **************************/

extern ogg_sync_state *ogg_sync_create(void);
extern int      ogg_sync_destroy(ogg_sync_state *oy);
extern int      ogg_sync_reset(ogg_sync_state *oy);

extern unsigned char *ogg_sync_bufferin(ogg_sync_state *oy, long size);
extern int      ogg_sync_wrote(ogg_sync_state *oy, long bytes);
//extern long     ogg_sync_pageseek(ogg_sync_state *oy,ogg_page *og);
//extern int      ogg_sync_pageout(ogg_sync_state *oy, ogg_page *og);
//extern int      ogg_stream_pagein(ogg_stream_state *os, ogg_page *og);
extern int      ogg_stream_packetout(ogg_stream_state *os,ogg_packet *op);
extern int      ogg_stream_packetpeek(ogg_stream_state *os,ogg_packet *op);

/* Ogg BITSTREAM PRIMITIVES: general ***************************/

extern ogg_stream_state *ogg_stream_create(int serialno);
extern int      ogg_stream_destroy(ogg_stream_state *os);
extern int      ogg_stream_reset(ogg_stream_state *os);
extern int      ogg_stream_reset_serialno(ogg_stream_state *os,int serialno);
extern int      ogg_stream_eos(ogg_stream_state *os);
/*
extern int      ogg_page_checksum_set(ogg_page *og);

extern int      ogg_page_version(ogg_page *og);
extern int      ogg_page_continued(ogg_page *og);
extern int      ogg_page_bos(ogg_page *og);
extern int      ogg_page_eos(ogg_page *og);
extern ogg_int64_t  ogg_page_granulepos(ogg_page *og);
extern ogg_uint32_t ogg_page_serialno(ogg_page *og);
extern ogg_uint32_t ogg_page_pageno(ogg_page *og);
extern int      ogg_page_packets(ogg_page *og);
extern int      ogg_page_getbuffer(ogg_page *og, unsigned char **buffer);
*/
extern int      ogg_packet_release(ogg_packet *op);
/*
extern int      ogg_page_release(ogg_page *og);

extern void     ogg_page_dup(ogg_page *d, ogg_page *s);
*/
/* Ogg BITSTREAM PRIMITIVES: return codes ***************************/

#define  OGG_SUCCESS   0

#define  OGG_HOLE     -10
#define  OGG_SPAN     -11
#define  OGG_EVERSION -12
#define  OGG_ESERIAL  -13
#define  OGG_EINVAL   -14
#define  OGG_EEOS     -15

#endif  /* _OGG_H */
