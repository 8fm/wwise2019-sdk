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

#include "stdafx.h"
#ifndef AK_OPTIMIZED
#ifndef PROXYCENTRAL_CONNECTED


#include "ObjectProxyLocal.h"
#include "AkCritical.h"
#include "AkIndexable.h"


ObjectProxyLocal::ObjectProxyLocal()
	: m_pIndexable( NULL )
	, m_refCount( 1 )
{
}

ObjectProxyLocal::~ObjectProxyLocal()
{
	AKASSERT( m_refCount == 0 );
}

void ObjectProxyLocal::AddRef()
{
	++m_refCount;
}

bool ObjectProxyLocal::Release()
{
	AKASSERT( m_refCount > 0 );

	bool bRet = false;
	
	if( --m_refCount == 0 )
	{
		if (m_pIndexable != NULL)
		{
			CAkFunctionCritical SpaceSetAsCritical;
			m_pIndexable->Release();
		}
		delete this;

		bRet = true;
	}

	return bRet;
}

AkUniqueID ObjectProxyLocal::GetID() const
{
	// m_pIndexable may be NULL if:
	//  - SetIndexable() was never called (should not happen)
	//  - Ran out of memory when creating the m_pIndexable in the subclass
	if ( m_pIndexable != NULL )
		return m_pIndexable->ID();

	return AK_INVALID_UNIQUE_ID;
}

bool ObjectProxyLocal::DoesNeedEndianSwap() const
{
    return false;
}

void ObjectProxyLocal::SetIndexable( CAkIndexable* in_pIndexable )
{
	// Note: m_pIndexable may be NULL if we ran out of memory when creating
	// the CAkIndexable in the subclass...
	m_pIndexable = in_pIndexable;
}

#endif
#endif // #ifndef AK_OPTIMIZED
