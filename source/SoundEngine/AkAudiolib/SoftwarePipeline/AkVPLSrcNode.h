/***********************************************************************
  The content of this file includes source code for the sound engine
  portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
  Two Source Code" as defined in the Source Code Addendum attached
  with this file.  Any use of the Level Two Source Code shall be
  subject to the terms and conditions outlined in the Source Code
  Addendum and the End User License Agreement for Wwise(R).

  Version:  Build: 
  Copyright (c) 2006-2020 Audiokinetic Inc.
 ***********************************************************************/

//////////////////////////////////////////////////////////////////////
//
// AkVPLSrcNode.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _AK_VPL_SRC_NODE_H_
#define _AK_VPL_SRC_NODE_H_

#include "AkPBI.h"
#include "AkVPLNode.h"
#include "AkFileParserBase.h"

enum AkSrcProcessOrder
{
	SrcProcessOrder_SwVoice,
	SrcProcessOrder_HwVoice,
	SrcProcessOrder_HwVoiceLowLatency,
};

class IAkRTPCSubscriberPlugin;

class CAkVPLSrcNode : public CAkVPLNode
{
public:
	static CAkVPLSrcNode * Create( AkSrcType in_eSrcType, AkUInt32 in_uPluginID, CAkPBI * in_pCtx );

	virtual AKRESULT	Init() { return AK_Success; }
	virtual void		Term( AkCtxDestroyReason in_eReason );

	void				Start();
	void				Stop();
	void				Pause();
	void				Resume( AkReal32 in_fOutputRate );

	virtual AKRESULT	TimeSkip( AkUInt32 & io_uFrames );
	virtual AKRESULT	Seek();
	virtual AkReal32	GetPitch() const { return m_pCtx->GetEffectiveParams().Pitch(); }

	void				Connect( CAkVPLNode * in_pInput );
	void				Disconnect( );
	CAkPBI *			GetContext() { return m_pCtx; }

	// CAkVPLSrcNode interface
	virtual AKRESULT	StartStream( AkUInt8 * in_pBuffer, AkUInt32 ulBufferSize ) = 0;
	virtual void		StopStream() = 0;
	virtual void		GetBuffer( AkVPLState & io_state ) = 0;
	virtual void		ReleaseBuffer() {}
	virtual AKRESULT	Seek( AkUInt32 in_SourceOffset );

	virtual void        VirtualOn( AkVirtualQueueBehavior eBehavior ) {}	// Sources have no input.
	virtual AKRESULT    VirtualOff( AkVirtualQueueBehavior eBehavior, bool in_bUseSourceOffset ) { return AK_Success; }

	virtual AkReal32	GetDuration() const = 0; // Returns total duration in ms, considering looping. 0 if looping infinite.
	virtual AKRESULT	StopLooping() = 0;

	virtual bool IsUsingThisSlot(const CAkUsageSlot* in_pUsageSlot) { return false; }
	virtual bool IsUsingThisSlot(const AkUInt8* in_pData) { return false; }

	// Query source to determine if it should start with a fade in, or if first buffer should be played at constant volume.
	AkForceInline bool	StartWithFadeIn() { return m_pCtx->NeedsFadeIn(); }

	// IO/buffering status
	virtual void		NotifySourceStarvation();
	inline void			SetIOReady() { m_bIOReady = true; }
	inline bool			IsIOReady() { return m_bIOReady; }

	inline bool			IsPreBuffering() { return m_bWaitForBuffering; }

	virtual bool SupportResampling() const
	{
		return false;
	}

	virtual AKRESULT SwitchToNextSrc(CAkVPLSrcNode * pNextSrc)
	{
		return AK_NotImplemented;
	}

	virtual bool SupportMediaRelocation() const
	{
		return false;
	}

	virtual AKRESULT RelocateMedia( AkUInt8* in_pNewMedia,  AkUInt8* in_pOldMedia )
	{
		return AK_NotImplemented;
	}

	void RelocateAnalysisData( AkUInt8* in_pNewMedia,  AkUInt8* in_pOldMedia )
	{
		AKASSERT( MustRelocateAnalysisDataOnMediaRelocation() );
		m_pAnalysisData = (AkFileParser::AnalysisData*)((AkUIntPtr)m_pAnalysisData + (AkUIntPtr)in_pNewMedia - (AkUIntPtr)in_pOldMedia);
	}

	// Enter/Resets pre-buffering status: puts the source in pre-buffering mode except if the sound does not require
	// pre-buffering (e.g. interactive music).
	inline void EnterPreBufferingState()
	{
		m_bWaitForBuffering = m_pCtx->RequiresPreBuffering();
	}
	inline void LeavePreBufferingState()
	{
		m_bWaitForBuffering = false;
	}
	inline void SetPrepareNextSource()
	{
		m_bPrepareNextSource = true;
	}
	inline bool CanPrepareNextSource()
	{
		return m_bPrepareNextSource;
	}

	virtual bool MustRelocatePitchInputBufferOnMediaRelocation()
	{
		// This function was created for only one purpose:
		// Allow Media relocation to work on PCM buffers which are, at the moment to write this line, the only src that are passing actual media
		// pointers to the Voice Pipeline This pointer may be stored in the pitch node inside the m_InputBuffer.
		return false;
	}

	virtual bool MustRelocateAnalysisDataOnMediaRelocation()
	{
		return m_pAnalysisData != NULL;
	}

	AKRESULT			FetchStreamedData( AkUInt8 * in_pBuffer, AkUInt32 ulBufferSize );

	virtual AKRESULT	ChangeSourcePosition() = 0;

	// Returns estimate of relative loudness at current position, compared to the loudest point of the sound, in dBs (always negative).
	// in_uBufferedFrames is the number of samples that are currently buffered in the pitch node (used for interpolation).
	virtual AkReal32 GetAnalyzedEnvelope( AkUInt32 in_uBufferedFrames ) { return 0; }

	// Returns linear make-up gain (all non-logical volumes; language make-up, loudness normalization, and so on).
	inline AkReal32 GetNormalization() const
	{		
		AkReal32 fNormGain = 1.0f;
		if ( m_pAnalysisData )
		{
			fNormGain *= m_pAnalysisData->fDownmixNormalizationGain;
			if ( m_pCtx->NormalizeLoudness() )
				fNormGain *= m_pAnalysisData->fLoudnessNormalizationGain;
		}
		return fNormGain;
	}

	virtual AkInt32 SrcProcessOrder() { return SrcProcessOrder_SwVoice; }

	// Restart the plugin after a media change
	virtual void RestartSourcePlugin() {};

#ifndef AK_OPTIMIZED
	virtual void RecapPluginParamDelta() = 0;
#endif

	virtual void PrepareNextBuffer() {};

protected:
    CAkVPLSrcNode( CAkPBI * in_pCtx );

protected:

	AkFileParser::AnalysisData * m_pAnalysisData;

	CAkPBI *			m_pCtx;		// Pointer to the associated sound context.
	bool				m_bIOReady : 1;	// True=I/O is ready.

private:
	AkUInt8 m_bWaitForBuffering    :1; // True when need to wait for stream object to contain enough data.
	AkUInt8 m_bPrepareNextSource   :1; // True when next source in the VPL can be prepared
};

#endif //_AK_VPL_SRC_NODE_H_
