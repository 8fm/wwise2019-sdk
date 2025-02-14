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

#ifndef _AK_SRC_FILEPCMEX_H_
#define _AK_SRC_FILEPCMEX_H_

#include "AkSrcFilePCM.h"
#include "AkFileParserBase.h"
#include "AkAudioLib.h"
#include "AkPCMDecoderHelpers.h"

namespace AK
{
	inline void ConvertOnTheFlyWAV(AkInt16 * pDstBuffer, void * pSrcBuffer, AkUInt32 uValidFrames, AkUInt32 uMaxFrames, AkUInt32 uBlockAlign, const AkChannelConfig& in_bufferConfig, AkSourceChannelOrdering in_eChannelOrder, bool in_bFloat, bool in_bSwap)
	{
		ConvertOnTheFly(pDstBuffer, pSrcBuffer, uValidFrames, uMaxFrames, uBlockAlign, in_bufferConfig.uNumChannels, in_bFloat, in_bSwap);

		if (AkFileParser::IsFuMaCompatible(in_bufferConfig)
			&& in_eChannelOrder  == SourceChannelOrdering_FuMa)
		{
			// Assuming B-format with FuMa ordering.
			// Convert to ACN+SN3D
			AkFileParser::ConvertFuMaToWwiseInterleaved(pDstBuffer, in_bufferConfig, uValidFrames);
		}
		else if (in_bufferConfig.eConfigType == AK_ChannelConfigType_Standard
			&& in_eChannelOrder == SourceChannelOrdering_Film)
		{
			AkFileParser::ConvertFilmToStdInterleaved(pDstBuffer, in_bufferConfig, uValidFrames);
		}
	}
}

class CAkSrcFilePCMEx : public CAkSrcFilePCM
						,public AK::WWISESOUNDENGINE_DLL::IAnalysisObserver
{
public:

	//Constructor and destructor
    CAkSrcFilePCMEx( CAkPBI * in_pCtx );
	virtual ~CAkSrcFilePCMEx();

	// Data access.
	virtual void	 GetBuffer( AkVPLState & io_state );

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

	void ProcessFade(AkVPLState & buffer, AkUInt32 in_uBufferStartSampleIdx );
	void ProcessLoopStart(AkUInt8 * in_pBuffer, AkUInt32 in_uBufferSize);
	void ProcessLoopEnd( AkVPLState& io_rAudioBuffer, AkUInt32 in_uBufferStartSampleIdx  );

	bool AllocCrossfadeBuffer();

	AKRESULT ParseHeader_Init();
	AKRESULT ParseHeader_ReadChunkHeader( AkUInt8 * in_pBuffer );
	AKRESULT ParseHeader_ReadChunkData( AkUInt8 * in_pBuffer );
	AKRESULT ParseHeader_ReadChunkPadding( AkUInt8 * in_pBuffer );
	AKRESULT ParseHeader_CheckForEOF( AkUInt8 * in_pBuffer );
	AKRESULT ParseHeader_GotoDataChunkForPlayback();
	AKRESULT ParseHeader_FillXFadeBuffer(AkUInt8 * in_pBuffer, AkUInt32 in_uBufferSize);
	AKRESULT ParseHeader_SetLoopPoints(AkUInt8 * in_pBuffer);

	enum AkPCMExState
	{
		State_Init,
		State_ReadChunkHeader,
		State_ReadChunkData,
		State_ReadChunkPadding,
		State_FillXFadeBuffer,
		State_Play
	};

	AkPCMExState	m_pcmExState;
	AkChunkHeader	m_chunkHeader;

	AkInt16 *		m_pOutputBuffer;	// Used for 24 bit sounds only.

	AkUInt32		m_uWAVFileSize; // Excluding junk tagged at the end, such as ID3 headers
	AkUInt16		m_uBlockAlign;

	AkDataTypeID	m_dataTypeID;
	AkSourceChannelOrdering m_ordering;

	AkUInt32					m_uEndFadeInSample;
	AkUInt32					m_uStartFadeOutSample;
	AkCurveInterpolation		m_eFadeInCurve;
	AkCurveInterpolation		m_eFadeOutCurve;

	AkPCMCrossfadeBuffer		m_CrossfadeBuffer;
private:
	bool						m_bProcessTrimAndFade;
	AkUniqueID					m_uiSourceID;
};

#endif // _AK_SRC_FILEPCMEX_H_
