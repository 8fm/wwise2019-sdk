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

#ifndef _AK_VORBIS_CODEBOOK_MGR_H_
#define _AK_VORBIS_CODEBOOK_MGR_H_

#include "AkSrcVorbis.h"
#include <AK/Tools/Common/AkHashList.h>
#include <AK/Tools/Common/AkLock.h>
#include "AkPrivateTypes.h"
#include "packed_codebooks.h"

struct AkVorbisSourceState;

struct Codebook
{
	AkUInt32 key;
	Codebook * pNextItem;
	CAkVorbisAllocator allocator;
	AkInt32 cRef;
	codec_setup_info csi;

	Codebook(): cRef(0)
	{
		csi.book_param = NULL;
		csi.books = 0;
	}

	void Term();
};

class AkVorbisCodebookMgr
{
public:
	AkVorbisCodebookMgr();
	~AkVorbisCodebookMgr();

	enum CodebookDecodeState
	{
		Codebook_NeedsDecode,
		Codebook_NeedsWait,
		Codebook_Decoded
	};

	codec_setup_info * Decodebook(
		AkVorbisSourceState & in_VorbisState, 
		class CAkPBI * in_pPBI,		// For error monitoring
		ogg_packet *op
		);

	void ReleaseCodebook( AkVorbisSourceState & in_VorbisState );

private:
	

	typedef AkHashListBare<AkUInt32, Codebook> CodebookList;
	CodebookList m_codebooks;
	CAkLock m_codebookLock;
};

extern AkVorbisCodebookMgr g_VorbisCodebookMgr;

#endif // _AK_VORBIS_CODEBOOK_MGR_H_
