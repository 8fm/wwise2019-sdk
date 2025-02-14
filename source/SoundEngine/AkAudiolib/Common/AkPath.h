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
// AkPath.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _PATH_H_
#define _PATH_H_

#include <AK/Tools/Common/AkArray.h>
#include <AK/Tools/Common/AkKeyArray.h>
#include "AkSharedEnum.h"
#include "AkConnectedListeners.h"
#include "AkSettings.h"

class	CAkBehavioralCtx;
struct	AkPathState;

struct AkPathVertex
{
	AkVector		Vertex;				// some given location
	AkTimeMs		Duration;			// time to get to next one
};

// entries in a list of paths are made of this
struct AkPathListItem
{
	AkPathVertex*	pVertices;		// array of vertices and time pairs
	AkInt32			iNumVertices;	// how many there are
	AkReal32		fRangeX;
	AkReal32		fRangeY;
	AkReal32		fRangeZ;
};

// entries in a list of paths are made of this
struct AkPathListItemOffset
{
	AkUInt32		ulVerticesOffset;	// num of vertices of offset to the beginning of this path
	AkInt32			iNumVertices;		// how many there are
};

class AkSharedPathState : public AkArray<bool, bool>
{
public:
	static AkSharedPathState* Create(AkUInt32 in_uCount)
	{
		AkSharedPathState *pNew = AkNew(AkMemID_Object, AkSharedPathState());
		if (pNew && in_uCount > 0)
		{
			if (!pNew->Resize(in_uCount))
			{
				pNew->Term();
				AkDelete(AkMemID_Object, pNew);
				return NULL;
			}

			pNew->ClearPlayedFlags();
		}

		return pNew;
	}

	inline void AddRef() {m_cRef++;}
	inline void Release() 
	{
		if (m_cRef == 1)
		{
			Term();
			AkDelete(AkMemID_Object, this);
		}
		else
			m_cRef--;
	}

	void ClearPlayedFlags()
	{
		for(AkUInt32 ulCounter = 0 ; ulCounter < Length() ; ++ulCounter)
			(*this)[ulCounter] = false;
	}

	bool AllPlayed()
	{
		bool played = true;
		for(AkUInt32 ulCounter = 0 ; ulCounter < Length() ; ++ulCounter)
			played = played && (*this)[ulCounter];
		return played;
	}
private:
	AkSharedPathState() : m_cRef(0)
	{
	}	

	AkUInt16 m_cRef;
};

class AkSafePathStatePtr
{
public:
	AkSafePathStatePtr():ptr(NULL){}
	AkSafePathStatePtr(AkSharedPathState* in_pOther)
	{
		*this = in_pOther;	//use operator=
	}

	~AkSafePathStatePtr()
	{
		if(ptr)
			ptr->Release();
	}

	inline AkSharedPathState& operator*(){return *ptr;}
	inline AkSharedPathState* operator->(){return ptr;}
	
	inline bool IsNull() {return ptr == NULL;}
	inline AkSafePathStatePtr& operator=(AkSafePathStatePtr& _b)
	{
		*this = _b.ptr;	//use operator=
		return *this;
	}

	inline AkSafePathStatePtr& operator=(AkSharedPathState * _b)
	{
		if (ptr)
			ptr->Release();
		ptr = _b;

		if (ptr)
			ptr->AddRef();

		return *this;
	}

	
private:
	AkSharedPathState *ptr;
};


class CAkPath
{
friend class CAkPathManager;

public:
	CAkPath();
	~CAkPath();

	void Term();

	// tell whether it's running or not
	bool IsRunning();

	// tell whether it's continuous or not
	bool IsContinuous(){ return (m_PathMode & AkPathContinuous) != 0; }

	// tell whether it's idle or not
	bool IsIdle();

	// how much is free ?
	//AkInt32 NeedRefill();

	// update the sound position
	void UpdatePosition(
		AkUInt32 in_uCurrentBufferTick
		);

	AKRESULT SetPathsList(
		AkPathListItem*	in_pPathList,
		AkUInt32		in_ulListSize,
		AkPathMode		in_PathMode,
		bool			in_bIsLooping,
		AkPathState*	in_pState
		);

	void SetSoundUniqueID( AkUniqueID in_soundUniqueID );
	void SetPlayingID( AkPlayingID in_playingID );
	void SetIsLooping(bool in_bIsLooping);
	void UpdateStartPosition();

	static AkForceInline AkUInt32 Convert( AkTimeMs in_ValueInTime )
	{
		// Approximation is nearly as accurate and way faster.
		AkUInt32 l_uResult = ( in_ValueInTime + AK_MS_PER_BUFFER_TICK - 1 ) / AK_MS_PER_BUFFER_TICK;
		if( !l_uResult )
		{
			l_uResult = 1;
		}
		return l_uResult;
	}

	void InitRotationMatricesForNoFollowMode(const AkListenerSet& in_Listeners);
	AkReal32* InitRotationMatrixForListener(AkGameObjectID in_uListenerId) ;
	AkReal32* GetNoFollowRotationMatrix(AkGameObjectID in_uListenerId)
	{
		AkReal32* pMatrix = (AkReal32*)m_NoFollowOrientationRotationMatrices.Exists(in_uListenerId);
		if (!pMatrix)
			pMatrix = InitRotationMatrixForListener(in_uListenerId);
		
		return pMatrix;
	}

private:
	// our path's possible states
	enum State
	{
		Idle	= 0,								// not being played
		Running	= 1,								// being played
		Paused	= 2									// in pause
	};

	// start moving
	AKRESULT Start(
		AkUInt32 in_CurrentBufferTick
		);

	// pause
	void Pause(
		AkUInt32 in_CurrentBufferTick
		);

	// resume
	void Resume(
		AkUInt32 in_CurrentBufferTick
		);

	// stop moving
	void Stop();

	// go to next vertex in list
	void NextVertex();

	// go to next in list
	AKRESULT GetNextPathList();

	// get a random one
	bool PickRandomList();

	// get next one in list
	bool PickSequenceList();

	inline void RandomizePosition(AkVector& in_rPosition);

	// our path's current state
	State						m_eState;				// our path's current state

	AkPathListItem*				m_pPathsList;			// the play list
	AkSafePathStatePtr			m_pbPlayed;				// the corresponding played flags
	AkPathListItem*				m_pCurrentList;			// the one that is being played
	AkUInt16					m_ulCurrentListIndex;	// current one played in sequence mode
	AkUInt16					m_ulListSize;			// the size of the play list
	AkUInt16					m_uCurrentVertex;		// Vertex index in current list
	AkPathMode					m_PathMode;				// random/sequence & step/continuous
	bool						m_bWasStarted;			// Set to true once this path has been started once
	bool						m_bIsLooping;			// 

	typedef AkArray<CAkBehavioralCtx*, CAkBehavioralCtx*, ArrayPoolDefault>	AkPBIList;
	AkPBIList					m_PBIsList;				// the PBIs we manage
	AkUInt16					m_uPotentialUsers;		// those that could be using it
	AkUInt16					m_uNumUsers;			// those that are using it

	AkUInt32					m_StartTime;
	AkUInt32					m_EndTime;
	AkUInt32					m_Duration;
	AkReal32					m_fa;
	AkReal32					m_fb;
	AkUInt32					m_TimePaused;

	AkVector					m_StartPosition;		// starting point
	AkVector					m_Direction;			// m_NextPosition - m_StartPosition
	AkVector					m_LastPositionNotified;

	// NOTE: For now m_ulSoundUniqueID and m_playingID will be used only for MONITOR_PATH_EVENT notifications. If
	// those notifications are ever #ifdef'd, we should #ifdef this member (and places where
	// it's used) too.
	AkUniqueID					m_ulSoundUniqueID;		// Unique ID of the sound using this path
	AkPlayingID					m_playingID;

	CAkKeyArray<AkGameObjectID, AkReal32[9]> m_NoFollowOrientationRotationMatrices; //Stores the initial rotation matrices for each listener for this sound, if set in No-Follow mode.		
};

#endif
