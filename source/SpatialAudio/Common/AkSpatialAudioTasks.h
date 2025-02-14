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

#include <AK/Tools/Common/AkKeyArray.h>
#include "AkSpatialAudioCompts.h"

class AkSoundGeometry;
class AkScene;
class AkAcousticPortal;

enum AkSAObjectTaskID
{
	TaskID_Listener,
	TaskID_Emitter,
	TaskID_EmitterListener,
};

struct AkSAObjectTaskData
{
private:

	AkSAObjectTaskID eTaskID;
	union
	{
		CAkSpatialAudioEmitter* pEmitter;
		CAkSpatialAudioListener* pListener;
	};

	friend void AkSAObjectTaskFcn(void* in_pData, AkUInt32 in_uIdxBegin, AkUInt32 in_uIdxEnd, AkTaskContext in_ctx, void* in_pUserData);

public:

	static AkSAObjectTaskData ListenerTask(CAkSpatialAudioListener* in_listener)
	{
		AkSAObjectTaskData data;
		data.eTaskID = TaskID_Listener;
		data.pListener = in_listener;
		return data;
	}

	static AkSAObjectTaskData EmitterTask(CAkSpatialAudioEmitter* in_emitter)
	{
		AkSAObjectTaskData data;
		data.eTaskID = TaskID_Emitter;
		data.pEmitter = in_emitter;
		return data;
	}

	static AkSAObjectTaskData EmitterListenerTask(CAkSpatialAudioEmitter* in_emitter)
	{
		AkSAObjectTaskData data;
		data.eTaskID = TaskID_EmitterListener;
		data.pEmitter = in_emitter;
		return data;
	}

	static void ProcessResults(AkSAObjectTaskData* in_pData, AkUInt32 in_uIdxBegin, AkUInt32 in_uIdxEnd, void* in_pUserData) {};
};

struct AkSAPortalTaskData
{
	AkSAPortalTaskData() {}
private:

	AkPortalPair pair;

	CAkDiffractionPathSegments paths;

public:

	bool ResolvePointers(AkSoundGeometry* pGeometry, AkAcousticPortal*& out_pPortal0, AkAcousticPortal*& out_pPortal1, AkAcousticRoom*& out_pRoom);

	static AkForceInline AkPortalPair & Get(AkSAPortalTaskData & in_item)
	{
		return in_item.pair;
	}

	static AkSAPortalTaskData PortalTask(AkPortalID in_portal0, AkPortalID in_portal1)
	{
		AkSAPortalTaskData data;
		data.pair = AkPortalPair(in_portal0, in_portal1);
		return data;
	}

	static void ProcessResults(AkSAPortalTaskData* in_pData, AkUInt32 in_uIdxBegin, AkUInt32 in_uIdxEnd, void* in_pUserData);

	friend void AkSAPortalTaskFcn(void* in_pData, AkUInt32 in_uIdxBegin, AkUInt32 in_uIdxEnd, AkTaskContext in_ctx, void* in_pUserData);
};

struct AkSAPortalRayCastingTaskData
{
	AkSAPortalRayCastingTaskData() {}

	AkAcousticPortal* pPortal;
	
	enum RoomFlags
	{
		None = 0,
		Front = 1U << 0,
		Back = 1U << 1,

		Both = (Front | Back)
	};

	AkUInt32 roomFlags;

	static AkForceInline AkAcousticPortal* & Get(AkSAPortalRayCastingTaskData & in_item)
	{
		return in_item.pPortal;
	}

	static void ProcessResults(AkSAPortalRayCastingTaskData* in_pData, AkUInt32 in_uIdxBegin, AkUInt32 in_uIdxEnd, void* in_pUserData);
	
	friend void AkSAPortalRaycCastingTaskFnc(void* in_pData, AkUInt32 in_uIdxBegin, AkUInt32 in_uIdxEnd, AkTaskContext in_ctx, void* in_pUserData);
};

/// Task function: process the [in_uIdxBegin,in_uIdxEnd[ range of items in the in_pData array.
//typedef void(*AkParallelForFunc)(void* in_pData, AkUInt32 in_uIdxBegin, AkUInt32 in_uIdxEnd, AkTaskContext in_ctx, void* in_pUserData);

void AkSAObjectTaskFcn(void* in_pData, AkUInt32 in_uIdxBegin, AkUInt32 in_uIdxEnd, AkTaskContext in_ctx, void* in_pUserData);
void AkSAPortalTaskFcn(void* in_pData, AkUInt32 in_uIdxBegin, AkUInt32 in_uIdxEnd, AkTaskContext in_ctx, void* in_pUserData);
void AkSAPortalRaycCastingTaskFnc(void* in_pData, AkUInt32 in_uIdxBegin, AkUInt32 in_uIdxEnd, AkTaskContext in_ctx, void* in_pUserData);

template<AkParallelForFunc fcn, typename tTaskData, typename tTaskGlobalContext, typename tTaskArray>
struct AkSATaskQueueBase
{
	~AkSATaskQueueBase()
	{
		m_taskArray.Term();
	}

	void Enqueue(const tTaskData& in_task) 
	{ 
		m_taskArray.AddLast(in_task); 
	};

	void Run(AkTaskSchedulerDesc& in_scheduler, tTaskGlobalContext* in_globalData, const char* in_debugName)
	{
		if (!m_taskArray.IsEmpty())
		{
			if (in_scheduler.fcnParallelFor != NULL)
			{
				in_scheduler.fcnParallelFor((void*)&(*m_taskArray.Begin()), 0, m_taskArray.Length(), 1, fcn, in_globalData, in_debugName);
			}
			else
			{
				AkTaskContext ctx;
				ctx.uIdxThread = 0;
				fcn((void*)&(*m_taskArray.Begin()), 0, m_taskArray.Length(), ctx, in_globalData);
			}
		
			tTaskData::ProcessResults(&*m_taskArray.Begin(), 0, m_taskArray.Length(), in_globalData);

			m_taskArray.RemoveAll();
		}
	}

	tTaskArray m_taskArray;
};

template<AkParallelForFunc fcn, typename tTaskData, typename tTaskGlobalContext>
struct AkSATaskQueue : AkSATaskQueueBase<fcn, tTaskData, tTaskGlobalContext, AkArray<tTaskData, const tTaskData&, ArrayPoolSpatialAudio, AkGrowByPolicy_Legacy_SpatialAudio<32>> >
{
};

template<AkParallelForFunc fcn, typename tTaskKey, typename tTaskData, typename tTaskGlobalContext>
struct AkSAKeyedTaskQueue : AkSATaskQueueBase<fcn, tTaskData, tTaskGlobalContext, AkSortedKeyArray<tTaskKey, tTaskData, ArrayPoolSpatialAudio, tTaskData, AkGrowByPolicy_Legacy_SpatialAudio<32>> >
{
	typedef AkSATaskQueueBase<fcn, tTaskData, tTaskGlobalContext, AkSortedKeyArray<tTaskKey, tTaskData, ArrayPoolSpatialAudio, tTaskData, AkGrowByPolicy_Legacy_SpatialAudio<32>> > tBase;

	void Set(tTaskData& in_task)
	{
		tTaskData* data = tBase::m_taskArray.Set(tTaskData::Get(in_task));
		if (data)
		{
			*data = in_task;
		}
	};
	
	tTaskData* Set(const tTaskKey& in_key)
	{
		return tBase::m_taskArray.Set(in_key);
	};
};

typedef AkSATaskQueue<AkSAObjectTaskFcn, AkSAObjectTaskData, AkSoundGeometry> AkSAObjectTaskQueue;
struct AkSAPortalToPortalTaskQueue : public AkSAKeyedTaskQueue<AkSAPortalTaskFcn, AkPortalPair, AkSAPortalTaskData, AkSoundGeometry> {};
struct AkSAPortalRayCastingTaskQueue : public AkSAKeyedTaskQueue<AkSAPortalRaycCastingTaskFnc, AkAcousticPortal*, AkSAPortalRayCastingTaskData, AkSoundGeometry> {};
