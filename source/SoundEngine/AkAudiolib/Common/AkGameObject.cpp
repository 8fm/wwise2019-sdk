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

//////////////////////////////////////////////////////////////////////
//
// AkGameObject.cpp
//
//////////////////////////////////////////////////////////////////////

#include <stdafx.h>
#include "AkGameObject.h"
#include "AkDeltaMonitor.h"


void AkGameObjComponentPtr::Delete()
{
	AkDelete(AkMemID_GameObject, ptr);
	ptr = NULL;
}


AKSOUNDENGINE_API CAkGameObject::~CAkGameObject()
{
	AkDeltaMonitor::LogGameObjectDestroyed(m_GameObjID, HasComponent(GameObjComponentIdx_Listener));
	
	AkRTPCKey rtpcKey((CAkRegisteredObj*)this);
	CAkScopedRtpcObj::Term(rtpcKey);

	for (AkGameObjComponentArray::Iterator it = m_components.Begin(); it != m_components.End(); ++it)
		(*it).Delete();

	m_components.Term();
}
