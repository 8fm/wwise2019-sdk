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
// AkPositionKeeper.h
//
//////////////////////////////////////////////////////////////////////


#ifndef _AKPOSITIONKEEPER_H_
#define _AKPOSITIONKEEPER_H_

#include <AK/Tools/Common/AkKeyArray.h>
#include <AK/Tools/Common/AkObject.h>
#include <AK/SoundEngine/Common/AkSpeakerConfig.h>
#include "Ak3DParams.h"
#include "AudiolibDefs.h"
#include "AkConnectedListeners.h"

// Adds Extend to AkArray to copy / propagate the last entry until the end (if at least one entry),
// as is desired with this data which can be set via different APIs (with different lengths).
class AkPerLstnrObjData : public AkArray<AkGameRayParams, const AkGameRayParams&>
{
public:
	bool Extend(AkUInt32 in_uiSize)
	{
		AkUInt32 oldNumItems = Length();
		if (AkArray<AkGameRayParams, const AkGameRayParams&>::Resize(in_uiSize))
		{
			// Extend.
			if (oldNumItems > 0)
			{
				AkUInt32 newNumItems = Length();
				if (newNumItems > oldNumItems)
				{
					for (size_t i = oldNumItems; i < newNumItems; i++)
					{
						*(m_pItems + i) = (m_pItems[oldNumItems - 1]);
					}
				}
			}
			return true;
		}
		return false;
	}
};
typedef CAkKeyArray<AkGameObjectID, AkPerLstnrObjData, ArrayPoolDefault, AkGrowByPolicy_DEFAULT, AkTransferMovePolicy< MapStruct< AkGameObjectID, AkPerLstnrObjData > > > AkPerLstnrObjDataMap;

static AkForceInline void _SetSoundPosToListener( const AkListenerPosition & in_listPos, AkSoundPosition & io_sndPos )
{
	// Take the listener orientation, as is
	AkReal32 fScale = AK_LOWER_MIN_DISTANCE;

	// Position the sound AK_LOWER_MIN_DISTANCE Distance unit in front of the Listener
	AkVector position;
	position.X = in_listPos.Position().X + (in_listPos.OrientationFront().X * fScale);
	position.Y = in_listPos.Position().Y + (in_listPos.OrientationFront().Y * fScale);
	position.Z = in_listPos.Position().Z + (in_listPos.OrientationFront().Z * fScale);
	io_sndPos.SetPosition(position);

	// Orientation should be the opposite of the listener one
	AkVector orientationFront;
	orientationFront.X = -(in_listPos.OrientationFront().X);
	orientationFront.Y = -(in_listPos.OrientationFront().Y);
	orientationFront.Z = -(in_listPos.OrientationFront().Z);
	io_sndPos.SetOrientation(orientationFront, in_listPos.OrientationTop());
}

class AkSoundPositionRef
{
	friend class AkPositionStore;
public:

	AkForceInline AkSoundPositionRef()
		: m_aPosition( NULL )
		, m_uNumPosition( 0 )
		, m_eMultiPositionType( AK::SoundEngine::MultiPositionType_SingleSource )
	{}

	// GetFirstPositionFixme is temporary only
	// Everyone using this function should change its implementation, it is currently only there for backward compatibility and
	// for development time only. Using GetFirstPositionFixme() will result in the old behavior, which will suppose there may be only one
	// position per object.
	AkForceInline const AkSoundPosition& GetFirstPositionFixme() const
	{
		return (GetPositions()) ? GetPositions()->position : GetDefaultPosition().position;
	}

	AkForceInline const AkChannelEmitter * GetPositions() const
	{
		return m_aPosition;
	}

	static const AkChannelEmitter & GetDefaultPosition()
	{
		static AkChannelEmitter defaultPosition;
		static const AkVector pos = { AK_DEFAULT_SOUND_POSITION_X, AK_DEFAULT_SOUND_POSITION_Y, AK_DEFAULT_SOUND_POSITION_Z };
		static const AkVector orient = { AK_DEFAULT_SOUND_ORIENTATION_X, AK_DEFAULT_SOUND_ORIENTATION_Y, AK_DEFAULT_SOUND_ORIENTATION_Z };
		static const AkVector orientTop = { AK_DEFAULT_TOP_X, AK_DEFAULT_TOP_Y, AK_DEFAULT_TOP_Z };
		defaultPosition.position.Set(pos, orient, orientTop);
		defaultPosition.uInputChannels = AK_SPEAKER_SETUP_ALL_SPEAKERS;

		return defaultPosition;
	}

	AkForceInline AkUInt32 GetNumPosition() const
	{
		return m_uNumPosition;
	}

	AkForceInline AK::SoundEngine::MultiPositionType GetMultiPositionType() const
	{
		return (AK::SoundEngine::MultiPositionType)m_eMultiPositionType;
	}

protected:

	AkChannelEmitter *		m_aPosition;
	AkPerLstnrObjDataMap	m_mapDataPerListener;
	AkUInt16				m_uNumPosition;

	AkUInt8					m_eMultiPositionType :3; // AK::SoundEngine::MultiPositionType, on 8 bits to save some memory.

private:
	AkSoundPositionRef& operator = (const AkSoundPositionRef&) {return *this;}
	AkSoundPositionRef(const AkSoundPositionRef&){}

};


class AkPositionStore : public AkSoundPositionRef
{
public:
	~AkPositionStore()
	{
		FreeCurrentItem(&m_aPosition, m_uNumPosition);

		for (AkPerLstnrObjDataMap::Iterator iter = m_mapDataPerListener.Begin(); iter != m_mapDataPerListener.End(); ++iter)
		{
			iter.pItem->item.Term();
		}
		
		m_mapDataPerListener.Term();
	}

	bool PositionsEqual(const AkChannelEmitter* in_Positions, AkUInt32 in_NumPos)
	{
		bool bEqual(m_uNumPosition == in_NumPos);

		for (AkUInt32 i = 0; i < in_NumPos && bEqual; i++)
		{
			bEqual = m_aPosition[i].uInputChannels == in_Positions[i].uInputChannels;
			if (bEqual)
				bEqual = AkMath::CompareAkTransform(m_aPosition[i].position, in_Positions[i].position);
		}
		return bEqual;
	}

	AKRESULT SetPosition(const AkChannelEmitter* in_Positions, AkUInt32 in_NumPos)
	{
		AKRESULT eResult = Allocate(&m_aPosition, in_NumPos, m_uNumPosition);
		if (eResult != AK_Success)
			return eResult;

		AKASSERT(m_uNumPosition == in_NumPos);

		for (AkUInt32 i = 0; i < m_uNumPosition; i++)
		{
			m_aPosition[i].position = in_Positions[i].position;
			m_aPosition[i].uInputChannels = in_Positions[i].uInputChannels;
		}

		return eResult;
	}

	void SetSpreadAndFocusValues(AkGameObjectID in_uListenerID, AkReal32* in_spread, AkReal32* in_focus, AkUInt32 in_NumPos)
	{
		AkPerLstnrObjData* pData = m_mapDataPerListener.Set(in_uListenerID);

		if (pData)
		{
			// Grow only.
			AkUInt32 newSize = AkMax(in_NumPos, pData->Length());
			if (pData->Extend(newSize))
			{
				// Set obstruction+occlusion on all items (up to in_uNumOcclusionObstruction).
				for (AkUInt32 i = 0; i < in_NumPos; i++)
				{
					(*pData)[i].spread = in_spread[i];
					(*pData)[i].focus = in_focus[i];
				}

				// Write last value in remaining entries if applicable,
				if (in_NumPos > 0)
				{
					for (AkUInt32 i = in_NumPos; i < pData->Length(); i++)
					{
						(*pData)[i].spread = in_spread[in_NumPos - 1];
						(*pData)[i].focus = in_focus[in_NumPos - 1];
					}
				}
			}
		}
	}

	/// REVIEW No return code: it just fails because of memory, error monitoring will be misleading
	AKRESULT SetObstructionOcclusion(AkGameObjectID in_uListenerID, AkObstructionOcclusionValues* in_fObstructionOcclusionValues, AkUInt32 in_uNumOcclusionObstruction)
	{
		AKRESULT res = AK_Success;

		AkPerLstnrObjData* pData = m_mapDataPerListener.Set(in_uListenerID);

		if (pData)
		{
			// Grow only.
			AkUInt32 newSize = AkMax(in_uNumOcclusionObstruction, pData->Length());
			if (pData->Extend(newSize))
			{
				// Set obstruction+occlusion on all items (up to in_uNumOcclusionObstruction).
				for (AkUInt32 i = 0; i < in_uNumOcclusionObstruction; i++)
				{
					(*pData)[i].obstruction = in_fObstructionOcclusionValues[i].obstruction;
					(*pData)[i].occlusion = in_fObstructionOcclusionValues[i].occlusion;
				}

				// Write last value in remaining entries if applicable,
				if (in_uNumOcclusionObstruction > 0)
				{
					for (AkUInt32 i = in_uNumOcclusionObstruction; i < pData->Length(); i++)
					{
						(*pData)[i].obstruction = in_fObstructionOcclusionValues[in_uNumOcclusionObstruction - 1].obstruction;
						(*pData)[i].occlusion = in_fObstructionOcclusionValues[in_uNumOcclusionObstruction - 1].occlusion;
					}
				}
			}
		}

		return res;
	}
	
	AkPerLstnrObjDataMap& GetPerListenerData()
	{
		return m_mapDataPerListener;
	}
	
	AkForceInline AKRESULT SetPositionWithDefaultOrientation( const AkVector& in_Position )
	{
		// allocate a single entry
		AKRESULT eResult = Allocate(&m_aPosition, 1, m_uNumPosition);
		if( eResult != AK_Success )
			return eResult;

		static const AkVector orient = { AK_DEFAULT_SOUND_ORIENTATION_X, AK_DEFAULT_SOUND_ORIENTATION_Y, AK_DEFAULT_SOUND_ORIENTATION_Z };
		static const AkVector orientTop = { AK_DEFAULT_TOP_X, AK_DEFAULT_TOP_Y, AK_DEFAULT_TOP_Z };
		m_aPosition->position.Set(in_Position, orient, orientTop);

		return AK_Success;
	}

	AkForceInline AKRESULT SetPositionToListener( const AkListenerPosition& in_listenerPosition )
	{
		AKRESULT eResult = Allocate(&m_aPosition, 1, m_uNumPosition);
		if( eResult != AK_Success )
			return eResult;

		_SetSoundPosToListener( in_listenerPosition, m_aPosition[0].position );

		return AK_Success;
	}

	AkForceInline void  SetMultiPositionType( AK::SoundEngine::MultiPositionType in_eMultiPositionType )
	{
		m_eMultiPositionType = in_eMultiPositionType;
	}
	
	void GetGameRayParams(AkGameObjectID in_uListenerID, AkUInt32 in_uPosIdx, AkGameRayParams& out_params)
	{
		// Per listener data
		AkPerLstnrObjData* pPerListenerData = m_mapDataPerListener.Exists(in_uListenerID);
		if (pPerListenerData && pPerListenerData->Length() > 0)
		{
			AkUInt32 idx = AkMin(in_uPosIdx, pPerListenerData->Length()-1);
			out_params = (*pPerListenerData)[idx];
		}
		else
		{
			out_params.obstruction = 0.f;
			out_params.occlusion = 0.f;
			out_params.spread = 0.f;
			out_params.focus = 0.f;
		}
	}

	void GetGameRayParams(AkGameObjectID in_uListenerID, AkUInt32 in_uNumRays, AkGameRayParams * out_arRays)
	{
		// Per listener data
		AkPerLstnrObjData* pPerListenerData = m_mapDataPerListener.Exists(in_uListenerID);
		if (pPerListenerData && pPerListenerData->Length() > 0)
		{
			// since from two different API function number of obstruction/occlusion and position migth differ
			AkUInt32 nbItem = AkMin(in_uNumRays, pPerListenerData->Length());
			for (AkUInt32 i = 0; i < nbItem; i++)
			{
				out_arRays[i] = (*pPerListenerData)[i];
			}
			// Copy the last entry in remaining slots if appl.
			for (AkUInt32 i = nbItem; i < in_uNumRays; i++)
			{
				out_arRays[i] = (*pPerListenerData)[nbItem-1];
			}
		}
		else
		{
			for (AkUInt32 i = 0; i < in_uNumRays; i++)
			{
				out_arRays[i].obstruction = 0.f;
				out_arRays[i].occlusion = 0.f;
				out_arRays[i].spread = 0.f;
				out_arRays[i].focus = 0.f;
			}
		}
	}

	void ClearDataForListenersNotInSet(const AkListenerSet& in_currentListenerSet)
	{
		for (AkPerLstnrObjDataMap::Iterator it = m_mapDataPerListener.Begin(); it != m_mapDataPerListener.End();)
		{
			if (!in_currentListenerSet.Exists((*it).key))
			{
				(*it).item.Term();
				it = m_mapDataPerListener.Erase(it);
			}
			else
			{
				++it;
			}
		}
	}

private:

	AkForceInline AKRESULT Allocate(AkChannelEmitter** in_Ptr, AkUInt32 in_NumItems, AkUInt16& CurrentNumItems)
	{
		size_t size = in_NumItems * sizeof(AkChannelEmitter);
		AkChannelEmitter* temp;

		// reallocate only if the new in_NumItems is different than the current one
		if(in_Ptr && (in_NumItems != CurrentNumItems))
		{
			FreeCurrentItem(in_Ptr, CurrentNumItems);
			
			if( in_NumItems == 0 )
				return AK_Success;

			// We have to re-allocate, the actual size doesn't fit.
			temp = (AkChannelEmitter*)AkAlloc(AkMemID_Processing, size);
			if (!temp)
				return AK_InsufficientMemory;

			memset(temp, 0, size);

			*in_Ptr = temp;

			CurrentNumItems = (AkUInt16)in_NumItems;
		}
		return AK_Success;
	}

	void FreeCurrentItem(AkChannelEmitter** in_Ptr, AkUInt16& CurrentNumItems)
	{
		if (*in_Ptr != NULL) {
			CurrentNumItems = 0;
			AkFree(AkMemID_Processing, *in_Ptr);
			*in_Ptr = NULL;
		}
	}
};

#endif //_AKPOSITIONKEEPER_H_
