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

#ifndef _AKSRCBANKXMA_H
#define _AKSRCBANKXMA_H

#include "AkSrcBase.h"
#include "AkSrcXMABase.h"

class CAkSrcBankXMA 
	: public CAkSrcXMABase<CAkSrcBaseEx>
	, public IAkSHAPEAware
{
public:

	//Constructor and destructor
	CAkSrcBankXMA( CAkPBI * in_pCtx );
	virtual ~CAkSrcBankXMA();
	
	// Data access.
	virtual AKRESULT StartStream( AkUInt8 * pvBuffer, AkUInt32 ulBufferSize );
	virtual void	 StopStream();
	virtual void	 GetBuffer( AkVPLState & io_state );
    virtual void     VirtualOn( AkVirtualQueueBehavior eBehavior );
    virtual AKRESULT VirtualOff( AkVirtualQueueBehavior eBehavior, bool in_bUseSourceOffset );
	virtual AKRESULT ChangeSourcePosition();

	// IO/buffering status notifications. 
	// In-memory XMA may starve because of the XMA decoder. 
	virtual void	 NotifySourceStarvation();

protected:

	// IAkSHAPEAware interface.
	virtual void UpdateInput(AkUInt32 in_uStreamIdx) {}
	virtual bool OnFlowgraphPrepare( AkUInt32 in_uShapeFrameIdx, AkUInt32 in_uStreamIndex ) { return CAkSrcXMABase<CAkSrcBaseEx>::OnFlowgraphPrepare(in_uShapeFrameIdx,in_uStreamIndex); }
	virtual bool NeedsUpdate() { return false; }
	virtual void HwVoiceFinished() { CAkSrcXMABase<CAkSrcBaseEx>::HwVoiceFinished(); };
	virtual void SetFatalError() { CAkSrcXMABase<CAkSrcBaseEx>::SetFatalError(); };
	virtual AkUInt32 GetNumStreams() const { return CAkSrcXMABase<CAkSrcBaseEx>::GetNumStreams(); }
	virtual AkACPVoice* GetAcpVoice(AkUInt32 in_uIdx) { return CAkSrcXMABase<CAkSrcBaseEx>::GetAcpVoice(in_uIdx); }

private:

	// Initialize XMA context if it does not exist, and places the hardware position to the desired 
	// source offset (PBI) if specified, otherwise it places it to the current PCM offset.
	AKRESULT        InitXMA( bool in_bUseSourceOffset );
	
	AKRESULT		SeekToSourceOffset( AkXMA2WaveFormat & in_xmaFmt, DWORD * in_pSeekTable, AkUInt8 * in_pData, AkUInt32 in_uNbStreams, DWORD * out_dwBitOffset, DWORD & out_dwSubframesSkip );
	AKRESULT		FindDecoderPosition( AkUInt32& io_uSampleOffset, XMA2WAVEFORMATEX & in_xmaFmt, DWORD * in_pSeekTable, AkUInt8* pData, AkUInt32 in_uNbStreams, DWORD * out_dwBitOffset, DWORD & out_dwSubframesSkip );
};

#endif
