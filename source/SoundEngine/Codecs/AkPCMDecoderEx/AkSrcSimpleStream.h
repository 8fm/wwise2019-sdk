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

#include <AKBase/IAkSimpleStream.h>
#include "AkSrcFilePCM.h"

class CAkSrcSimpleStream
	: public IAkSimpleStream
{
public:
	CAkSrcSimpleStream() : m_pSrcFile(NULL), m_bStitchConsumed(false) {}

	void Init(CAkSrcFilePCM * in_pSrcFile){ m_pSrcFile = in_pSrcFile; }

	virtual AKRESULT Read(
		AkUInt32        in_uReqSize,        ///< Requested read size
		void *&         out_pBuffer,        ///< Buffer address
		AkUInt32 &      out_uSize           ///< The size that was actually read
		);

	virtual AkUInt64 GetPosition();

	virtual AKRESULT SetPosition(
		AkInt64         in_iPosition      ///< Position, from beginning
		);

	virtual AKRESULT GetFileSize(
		AkUInt64 & out_uSize
		);

	void SetBuffer(AkUInt8 * in_pBuffer);

private:
	CAkSrcFilePCM * m_pSrcFile;
	bool m_bStitchConsumed;
};
