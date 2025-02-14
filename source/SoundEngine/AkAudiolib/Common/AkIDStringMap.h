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
// AkIDStringMap.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _ID_STRING_MAP_H_
#define _ID_STRING_MAP_H_

#include <AK/Tools/Common/AkHashList.h>
#include "AkMonitor.h"

class AkIDStringHash
{
public:

	typedef AkHashList< AkGameObjectID, char, AkMonitor::MonitorPoolDefault > AkStringHash;

	AkIDStringHash() {}

	void Term();

	AkStringHash::Item * Preallocate(AkGameObjectID in_ID, const char* in_pszString);
	void FreePreallocatedString( AkStringHash::Item * in_pItem );

	AKRESULT Set( AkStringHash::Item * in_pItem );

	AKRESULT Set(AkGameObjectID in_ID, const char* in_pszString);

	void Unset(AkGameObjectID in_ID);

	char * GetStr(AkGameObjectID in_ID);

//Private, but set as public to allowing to iterate in the list.
	AkStringHash m_list;
};




#endif //_ID_STRING_MAP_H_
