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

#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include "AkMarkerStructs.h"
#include <AK/Tools/Common/AkArray.h>
#include <string.h>

class CAkMarkersQueue
{
public:
	CAkMarkersQueue()
		: m_pHead(nullptr)
		, m_Length(0)
		, m_Reserve(0)
	{
	}

	AKRESULT Init()
	{
		m_Length = 0;
		m_Reserve = 5;
		m_pHead = (AkMarkerNotification*)AkAlloc(AkMemID_Processing, sizeof(AkMarkerNotification) * m_Reserve);
		return m_pHead == nullptr ? AK_InsufficientMemory : AK_Success;
	};

	void Term()
	{
		if (m_pHead)
		{
			AkFree(AkMemID_Processing, m_pHead);
			m_pHead = nullptr;
		}
		m_Length = 0;
		m_Reserve = 0;
	}

	AkForceInline AkMarkerNotificationRange Front() const
	{
		return {
			m_Length == 0 ? nullptr : m_pHead,
			m_Length
		};
	}

	AKRESULT Enqueue(AkAudioMarker * pMarkers, AkUInt32 uNumMarkers, AkUInt32 uBufferStartPosition, CAkPBI * pCtx)
	{
		AKASSERT(m_pHead != nullptr);
		AkUInt32 spaceLeft = m_Reserve - m_Length;
		if (spaceLeft < uNumMarkers)
		{
			// Realloc enough space in one shot
			AkUInt32 newReserve = m_Reserve + uNumMarkers + 5;
			AkMarkerNotification * pNew = (AkMarkerNotification*)AkRealloc(AkMemID_Processing, m_pHead, sizeof(AkMarkerNotification) * newReserve);
			if (!pNew)
			{
				return AK_InsufficientMemory;
			}
			m_pHead = pNew;
			m_Reserve = newReserve;
		}
		for (AkUInt32 i = 0; i < uNumMarkers; i++)
		{
			AKASSERT(pMarkers[i].dwPosition >= uBufferStartPosition);
			AkMarkerNotification * pNotif = m_pHead + m_Length;
			pNotif->marker = pMarkers[i];
			pNotif->uOffsetInBuffer = pMarkers[i].dwPosition - uBufferStartPosition;
			pNotif->pCtx = pCtx;
			m_Length++;
		}
		return AK_Success;
	}

	void Dequeue(AkUInt32 uNumMarkers)
	{
		AKASSERT(uNumMarkers <= m_Length);
		AkUInt32 remainder = m_Length - uNumMarkers;
		if (remainder > 0)
		{
			// Copy markers that will survive the flush to the beginning of the array
			memmove(m_pHead, m_pHead + uNumMarkers, remainder * sizeof(AkMarkerNotification));
		}
		m_Length = remainder;
	}
private:
	AkMarkerNotification * m_pHead;
	AkUInt32 m_Length;
	AkUInt32 m_Reserve;
};
