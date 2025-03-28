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

#ifndef _AKDIALOGUEEVENT_H
#define _AKDIALOGUEEVENT_H

#include "AkIndexable.h"

#include "AkAudioLibIndex.h"
#include "AkDecisionTree.h"
#include "AkPropBundle.h"
#include "AkModifiers.h"

class CAkDialogueEvent
	: public CAkIndexable
{
public:
    virtual ~CAkDialogueEvent();

	static CAkDialogueEvent* Create( AkUniqueID in_ulID = 0 );

	virtual AkUInt32 AddRef();
	virtual AkUInt32 Release();

	AKRESULT SetInitialValues( AkUInt8* in_pData, AkUInt32 in_ulDataSize );

	void SetAkProp( AkPropID in_eProp, AkReal32 in_fValue, AkReal32 in_fMin, AkReal32 in_fMax );
	void SetAkProp( AkPropID in_eProp, AkInt32 in_iValue, AkInt32 in_iMin, AkInt32 in_iMax );
	AKRESULT SetDecisionTree( void* in_pData, AkUInt32 in_uSize, AkUInt32 in_uDepth );
	AKRESULT SetArguments( AkUniqueID*, AkUInt8*, AkUInt32 ) { return AK_Success; }

#ifdef AK_SUPPORT_WCHAR
	AKRESULT ResolveArgumentValueNames( const wchar_t** in_aNames, AkArgumentValueID * out_pPath, AkUInt32 in_cPath );
#endif //AK_SUPPORT_WCHAR

	AKRESULT ResolveArgumentValueNames( const char** in_aNames, AkArgumentValueID * out_pPath, AkUInt32 in_cPath );

	AkForceInline AkUniqueID ResolvePath( AkArgumentValueID * in_pPath, AkUInt32 in_cPath, AkPlayingID in_idSequence, AkCandidateCallbackFunc in_candidateCallbackFunc = NULL, void* in_pCookie = NULL )
	{
#ifndef AK_OPTIMIZED
		// Use index lock during resolution in case proxy is also pushing this.
		AkAutoLock<CAkLock> IndexLock( g_pIndex->m_idxDialogueEvents.GetLock() ); 
#endif
		return m_decisionTree.ResolvePath( ID(), in_pPath, in_cPath, in_idSequence, in_candidateCallbackFunc, in_pCookie );
	}

	AkPropValue * FindCustomProp(AkUInt32 in_uPropID);

private:
    CAkDialogueEvent( AkUniqueID in_ulID );

	AKRESULT Init();

	AkDecisionTree	m_decisionTree;
	AkPropBundle<AkPropValue, AkUInt8, AkMemID_Event> m_props;
	AkPropBundle< RANGED_MODIFIERS<AkPropValue>, AkUInt8, AkMemID_Event > m_ranges;
};

#endif // _AKDIALOGUEEVENT_H
