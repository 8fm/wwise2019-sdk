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
// AkIDStringMap.cpp
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <string.h>
#include <stddef.h>

#include "AkIDStringMap.h"
#include "AkMath.h"

// bernstein hash
// IMPORTANT: must be kept in sync with version in SoundBankIDToStringChunk.cpp
inline AkHashType AkHash(const char * in_string)
{
	AkHashType hash = 5381;
    int c;

    while ((c = (*in_string++)))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Implementation of AkIDStringHash
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

void AkIDStringHash::Term()
{
	m_list.Term();
}

AkIDStringHash::AkStringHash::Item * AkIDStringHash::Preallocate(AkGameObjectID in_ID, const char* in_pszString)
{
	// Allocate string 'in-place' in the hash item structure to save memory blocks.

	AkUInt32 sizeofString = (AkUInt32) ( sizeof(char)*(strlen(in_pszString) + 1) );
	AkUInt32 sizeofItem = offsetof( AkStringHash::Item, Assoc.item ) + sizeofString;
	sizeofItem = AkMax(sizeofItem, sizeof(AkStringHash::Item)); // In case the string is smaller than the padding
	AkStringHash::Item * pItem = (AkStringHash::Item *) AkAlloc(AkMemID_Profiler, sizeofItem);
	if ( !pItem ) 
		return NULL;

	pItem->Assoc.key = in_ID;
	memcpy( &( pItem->Assoc.item ), in_pszString, sizeofString ); // Use memcpy, since we already called strlen

	return pItem;
}

void AkIDStringHash::FreePreallocatedString( AkStringHash::Item * in_pItem )
{
	AkFree(AkMemID_Profiler, in_pItem);
}

AKRESULT AkIDStringHash::Set(AkGameObjectID in_ID, const char* in_pszString)
{
	// Do not try to unset previous, as external calls to Set/Unset should be balanced. (performance issue)

	if( in_pszString != NULL )
	{
		AkStringHash::Item * pItem = Preallocate( in_ID, in_pszString );
		if ( pItem == NULL )
			return AK_Fail;
        
		m_list.Set( pItem );
	}

	return AK_Success;
}

AKRESULT AkIDStringHash::Set( AkStringHash::Item * in_pItem )
{
	m_list.Set( in_pItem );

	return AK_Success;
}


void AkIDStringHash::Unset(AkGameObjectID in_ID)
{
	m_list.Unset( in_ID );
}

char * AkIDStringHash::GetStr(AkGameObjectID in_ID)
{
	return m_list.Exists( in_ID );
}
