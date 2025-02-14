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

#ifndef _AK_SRC_FILECAF_H_
#define _AK_SRC_FILECAF_H_

#include "AkSrcFilePCM.h"
#include "AkSrcSimpleStream.h"
#include "AkFileParserBase.h"
#include "AkAudioLib.h"
#include <AKBase/AkCAFReader.h>
#include "AkPCMDecoderHelpers.h"

namespace AK
{
	inline void ConvertOnTheFlyCAF(AkInt16 * pDstBuffer, void * pSrcBuffer, AkUInt32 uValidFrames, AkUInt32 uMaxFrames, AkUInt32 uBlockAlign, const AkChannelConfig& in_bufferConfig, const AkCAFReader & in_reader)
	{
		const CAFAmbixAdaptorMatrix & matrix = in_reader.GetMatrix();
		if (in_bufferConfig.eConfigType == AK_ChannelConfigType_Ambisonic
			&& matrix.pMatrix)
		{
			AkInt16 * pTmpBuf = (AkInt16 *)malloc(uValidFrames*matrix.C*sizeof(AkInt16));
			ConvertOnTheFly(pTmpBuf, pSrcBuffer, uValidFrames, uMaxFrames, uBlockAlign, matrix.C, false, in_reader.NeedSwap());

			in_reader.ApplyMatrix(pTmpBuf, pDstBuffer, uValidFrames);

			free(pTmpBuf);
		}
		else
		{
			ConvertOnTheFly(pDstBuffer, pSrcBuffer, uValidFrames, uMaxFrames, uBlockAlign, in_bufferConfig.uNumChannels, false, in_reader.NeedSwap());
		}
	}
}

class CAkSrcFileCAF : public CAkSrcFilePCM
	, public AK::WWISESOUNDENGINE_DLL::IAnalysisObserver
{
public:

	//Constructor and destructor
    CAkSrcFileCAF( CAkPBI * in_pCtx );
	virtual ~CAkSrcFileCAF();

	// Data access.
	virtual void GetBuffer( AkVPLState & io_state );

	// IAnalysisObserver
	virtual void OnAnalysisChanged( AK::WWISESOUNDENGINE_DLL::AnalysisInfo * in_pAnalysisInfo );

protected:

    virtual AKRESULT ParseHeader(
		AkUInt8 * in_pBuffer	// Buffer to parse
		);

	// Returns block align. This function is virtual, in order for derived sources to manage their own 
	// block align independently of that of the pipeline/PBI (read 24 bits).
	// Here, we return the _original_ file's block align.
	virtual AkUInt16 GetBlockAlign() const;
	virtual AKRESULT StartStream(AkUInt8 * in_pBuffer, AkUInt32 ulBufferSize);

	void ProcessFade(AkVPLState & buffer, AkUInt32 in_uBufferStartSampleIdx );
	void ProcessLoopEnd(AkVPLState& io_rAudioBuffer, AkUInt32 in_uBufferStartSampleIdx);

	bool AllocCrossfadeBuffer();

	enum AkPCMExState
	{
		State_Init,
		State_FillXFadeBuffer,
		State_Play
	};

	AkCAFReader m_reader;
	CAkSrcSimpleStream m_stream;

	AkInt16 *		m_pOutputBuffer;	// Used for 24 bit sounds only.

	AkPCMExState	m_pcmExState;

	AkUInt32		m_uWAVFileSize; // Excluding junk tagged at the end, such as ID3 headers
	AkUInt16		m_uBlockAlign;

	AkSourceChannelOrdering m_ordering;

	AkUInt32					m_uEndFadeInSample;
	AkUInt32					m_uStartFadeOutSample;
	AkCurveInterpolation		m_eFadeInCurve;
	AkCurveInterpolation		m_eFadeOutCurve;

	AkPCMCrossfadeBuffer		m_CrossfadeBuffer;
	AkUniqueID					m_uiSourceID;
};

#endif // _AK_SRC_FILECAF_H_
