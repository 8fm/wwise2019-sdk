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

#pragma once

#include <AK/Tools/Common/AkVectors.h>
#include <AK/SpatialAudio/Common/AkSpatialAudio.h>
#include "AkSpatialAudioPrivateTypes.h"

class AkAcousticNode
{
public:
	typedef AkArray< AkAcousticNode*, AkAcousticNode*, ArrayPoolSpatialAudio > tLinkArray;
	typedef tLinkArray::Iterator tLinkIter;

public:

	AkAcousticNode(): m_bDirty(true)
	{
	}
	~AkAcousticNode()
	{
		ClearLinks();
		m_Links.Term();
	};

	AKRESULT Link(AkAcousticNode* in_pPortal, bool in_bLinkBack = true)
	{
		for (tLinkArray::Iterator it = m_Links.Begin(); it != m_Links.End(); ++it)
		{
			if ((*it) == in_pPortal)
			{
				m_bDirty = true;
				return AK_Success;
			}
		}

		if (m_Links.AddLast(in_pPortal) != NULL &&
			(!in_bLinkBack || in_pPortal->Link(this, false) == AK_Success)
			)
		{
			m_bDirty = true;
			return AK_Success;
		}
		else
		{
			Unlink(in_pPortal);
			return AK_InsufficientMemory;
		}
	}


	AKRESULT Unlink(AkAcousticNode* in_pToNode, bool in_bUnlinkBack = true)
	{
		for (tLinkArray::Iterator it = m_Links.Begin(); it != m_Links.End(); ++it)
		{
			if ((*it) == in_pToNode)
			{
				if (in_bUnlinkBack)
					in_pToNode->Unlink(this, false);

				m_Links.Erase(it);
				m_bDirty = true;
				return AK_Success;
			}
		}

		return AK_IDNotFound;
	}

	void ClearLinks()
	{
		for (tLinkArray::Iterator it = m_Links.Begin(); it != m_Links.End(); ++it)
			(*it)->Unlink(this, false);

		m_Links.RemoveAll();

		m_bDirty = true;
	}

	
	const Ak3DVector& GetFront() const { return m_Front; }
	const Ak3DVector& GetUp() const { return m_Up; }

	const AK::SpatialAudio::OsString& GetName() const { return m_strName; }

	void SetDirty(bool in_bDirty) { m_bDirty = in_bDirty; }

protected:
	
	Ak3DVector						m_Front;
	Ak3DVector						m_Up;

	AK::SpatialAudio::OsString		m_strName;
	tLinkArray						m_Links;

	bool							m_bDirty;
};
