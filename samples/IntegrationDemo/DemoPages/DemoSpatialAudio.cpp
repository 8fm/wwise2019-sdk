/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided 
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

  Copyright (c) 2020 Audiokinetic Inc.
*******************************************************************************/

// DemoSpatialAudio.cpp
/// \file
/// Defines all methods declared in DemoSpatialAudio.h

#include "stdafx.h"

#include <math.h>
#include <float.h>
#include "Menu.h"
#include "MovableChip.h"
#include "DemoSpatialAudio.h"
#include <AK/SoundEngine/Common/AkSoundEngine.h>    // Sound engine
#include <AK/SpatialAudio/Common/AkSpatialAudio.h>

//If you get a compiling error here, it means the file wasn't generated with the banks.  Did you generate the soundbanks before compiling?
#include "../WwiseProject/GeneratedSoundBanks/Wwise_IDs.h"		

// Our game object ID.  Completely arbitrary.
// IMPORTANT: As stated in AK::SpatialAudio::AddRoom and AddPortal, room and portal IDs exist in the same ID-space than game objects.
const AkGameObjectID EMITTER = 100;
const AkGameObjectID LISTENER = 103;
const AkRoomID ROOM = 200;
const AkPortalID PORTAL0 = 300;
const AkPortalID PORTAL1 = 301;

#define REPEAT_TIME 20

/////////////////////////////////////////////////////////////////////
// DemoSpatialAudio Public Methods
/////////////////////////////////////////////////////////////////////

DemoSpatialAudio::DemoSpatialAudio(Menu& in_ParentMenu, DemoSpatialAudio_Scenario in_scenario)
	: Page( in_ParentMenu, (in_scenario == Scenario_Portals) ? "Portals" : "Portals and Geometry")
	, m_pEmitterChip( NULL )
	, m_pListenerChip(NULL)
	, m_pRoom(NULL)
	, m_pPortal0(NULL)
	, m_pPortal1(NULL)
	, m_aLines(NULL)
	, m_numLines(0)
	, m_fGameObjectX( 0 )
	, m_fGameObjectZ( 0 )
	, m_dryDiffraction( 0 )
	, m_wetDiffraction( 0 )
	, m_fWidth( 0.0f )
	, m_fHeight(0.0f)
	, m_room0_width(0.0f)
	, m_room0_height(0.0f)
	, m_roomCornerx(0.0f)
	, m_roomCornery(0.0f)
	, m_portalsOpen(0x3)	// both portals open
	, m_uRepeat(REPEAT_TIME)
	, m_uLastTick(0)
	, m_bMoved(true)
	, m_eScenario(in_scenario)
	, m_pGameSideObs(NULL)
	, m_pGeometryInRooms(NULL)
{
	switch (in_scenario)
	{
	case Scenario_Portals:
		m_szHelp =
			"In this demo we have 2 rooms, ROOM (box in the upper left) and Outside, 2 portals (PORTAL0 in the upper right, PORTAL1 in the lower left), and\n"
			"* 'Emitter/[E]' - the position of an emitter.\n"
			"* 'Listener/[L]' - the position of the listener (namely the main character or camera in a game), which also emits a footstep sound when moving.\n"
			"The sound propagation path(s) are displayed, with the resulting diffraction amounts in the lower left corner.\n"
			"Spatial Audio is set up so that diffraction controls both project-wide obstruction and the Diffraction built-in parameter, although only the former is used in the Wwise project.\n"
			"In addition to portal-driven propagation, a naive game-side obstruction algorithm is used for same-room obstruction (emitter-listener and portals-listener).\n"
			"Finally, this demo shows how to use a Room to play multi-channel ambient sounds / room tones that contract and become point-sources at portals.\n"
			"Refer to Rooms and Portals in the SDK documentation for more details.\n"
			"* <<UG_RIGHT_STICK>> + <<DIRECTIONAL_TYPE>> - Move the listener game object.\n"
			"* <<UG_BUTTON5>> + <<UG_RIGHT_STICK>> + <<DIRECTIONAL_TYPE>> - Move the emitter game object.\n"
			"* <<UG_BUTTON1>> - Open/close portals in sequence.";
		break;
	case Scenario_Portals_And_Geometry:
		m_szHelp =
			"In this demo we have 2 rooms, ROOM (box in the upper left) and Outside, 2 portals (PORTAL0 in the upper right, PORTAL1 in the lower left),\n"
			"geometry for the wall inside and outside of ROOM and an obstacle, and\n"
			"* 'Emitter/[E]' - the position of an emitter.\n"
			"* 'Listener/[L]' - the position of the listener (namely the main character or camera in a game), which also emits a footstep sound when moving.\n"
			"The sound propagation path(s) are displayed, with the resulting diffraction amounts in the lower left corner.\n"
			"Spatial Audio is set up so that diffraction controls both project-wide obstruction and the Diffraction built-in parameter, although only the former is used in the Wwise project.\n"
			"Spatial Audio handles diffraction / obstruction using geometry and portals; the game does not have to compute obstruction.\n"
			"Finally, this demo shows how to use a Room to play multi-channel ambient sounds / room tones that contract and become point-sources at portals.\n"
			"Refer to Rooms and Portals in the SDK documentation for more details.\n"
			"* <<UG_RIGHT_STICK>> + <<DIRECTIONAL_TYPE>> - Move the listener game object.\n"
			"* <<UG_BUTTON5>> + <<UG_RIGHT_STICK>> + <<DIRECTIONAL_TYPE>> - Move the emitter game object.\n"
			"* <<UG_BUTTON1>> - Open/close portals in sequence.";
		break;
	}
}

void DemoSpatialAudio::RegisterEmittersAndListeners()
{
	// Register the emitter game object
	AK::SoundEngine::RegisterGameObj(EMITTER, "Emitter E");
	
	// Let's register a different listener than the default one (registered in IntegrationDemo.cpp) just for this demo, because we are going to move it around.
	AK::SoundEngine::RegisterGameObj(LISTENER, "Listener L");
	
	// Associate our emitters with our listener. This will set up direct path from EMITTER -> LISTENER
	// The listener object will also be emitting so we need to set LISTENER -> LISTENER too.
	static const int kNumLstnrsForEm = 1;
	static const AkGameObjectID aLstnrsForEmitter[kNumLstnrsForEm] = { LISTENER };
	AK::SoundEngine::SetListeners(EMITTER, aLstnrsForEmitter, kNumLstnrsForEm);
	AK::SoundEngine::SetListeners(LISTENER, aLstnrsForEmitter, kNumLstnrsForEm);

	// And register our listener as the one and only spatial audio listener.
	AK::SpatialAudio::RegisterListener(LISTENER);
}

bool DemoSpatialAudio::Init()
{
	RegisterEmittersAndListeners();

	// Load the sound bank
	AkBankID bankID; // Not used
	if ( AK::SoundEngine::LoadBank( "Bus3d_Demo.bnk", bankID ) != AK_Success )
	{
		SetLoadFileErrorMessage( "Bus3d_Demo.bnk" );
		return false;
	}

	m_uLastTick = m_pParentMenu->GetTickCount();

	return Page::Init();
}

void DemoSpatialAudio::Release()
{
	AK::SoundEngine::StopAll();
	AK::SpatialAudio::RemovePortal(PORTAL0);
	AK::SpatialAudio::RemovePortal(PORTAL1);
	AK::SpatialAudio::RemoveRoom(ROOM);
	AK::SpatialAudio::RemoveRoom(AkRoomID::OutdoorsGameObjID);
	AK::SoundEngine::UnregisterGameObj(EMITTER);
	AK::SoundEngine::UnregisterGameObj(LISTENER);
	AK::SoundEngine::UnloadBank( "Bus3d_Demo.bnk", NULL );

	for (int i = 0; i < m_numLines; i++)
	{
		if (m_aLines[i])
			delete m_aLines[i];
	}
	delete[] m_aLines;

	delete m_pGameSideObs;
	delete m_pGeometryInRooms;

	Page::Release();
}

#define POSITION_RANGE (200.0f)

float DemoSpatialAudio::PixelsToAKPos_X(float in_X)
{
	return ((in_X / m_fWidth) - 0.5f) * POSITION_RANGE;
}

float DemoSpatialAudio::PixelsToAKPos_Y(float in_y)
{
	return -((in_y / m_fHeight) - 0.5f) * POSITION_RANGE;
}

float DemoSpatialAudio::PixelsToAKLen_X(float in_X)
{
	return (in_X / m_fWidth) * POSITION_RANGE;
}

float DemoSpatialAudio::PixelsToAKLen_Y(float in_y)
{
	return (in_y / m_fHeight) * POSITION_RANGE;
}

int DemoSpatialAudio::AKPosToPixels_X(float in_X)
{
	return (int)(((in_X / POSITION_RANGE) + 0.5f) * m_fWidth);
}

int DemoSpatialAudio::AKPosToPixels_Y(float in_y)
{
	return (int)(((-in_y / POSITION_RANGE) + 0.5f) * m_fHeight);
}

// Naive containment test.
// Returns the room index.
AkRoomID DemoSpatialAudio::IsInRoom(float in_x, float in_y)
{
	if (in_x <= m_roomCornerx && in_y <= m_roomCornery)
		return ROOM;
	return AkRoomID::OutdoorsGameObjID;
}

void DemoSpatialAudio::UpdateGameObjPos(MovableChip* in_pChip, AkGameObjectID in_GameObjectId)
{
	if (in_pChip)
	{
		float x, y;
		in_pChip->GetPos(x, y);

		// Converting X-Y UI into X-Z world plan.
		AkVector position;
		m_fGameObjectX = position.X = PixelsToAKPos_X(x);
		position.Y = 0;
		m_fGameObjectZ = position.Z = PixelsToAKPos_Y(y);
		AkVector orientationFront;
		orientationFront.Z = 1;
		orientationFront.Y = orientationFront.X = 0;
		AkVector orientationTop;
		orientationTop.X = orientationTop.Z = 0;
		orientationTop.Y = 1;

		AkSoundPosition soundPos;
		soundPos.Set(position, orientationFront, orientationTop);
		AK::SoundEngine::SetPosition(in_GameObjectId, soundPos);

		// For SpatialAudio
		AkRoomID roomID = IsInRoom(x, y);
		
		// Set emitter aux send values if we are updating the emitter.
		AK::SpatialAudio::SetGameObjectInRoom(in_GameObjectId, roomID);
	}
	
}

bool DemoSpatialAudio::Update()
{
	UniversalInput::Iterator it;
	for ( it = m_pParentMenu->Input()->Begin(); it != m_pParentMenu->Input()->End(); it++ )
	{
		// Skip this input device if it's not connected
		if ( ! it->IsConnected() )
			continue;

		if ((*it).IsButtonDown(UG_BUTTON5))
		{
			m_pEmitterChip->Update(*it);
			m_bMoved = true;
		}
		else
		{
			float xprev, yprev;
			m_pListenerChip->GetPos(xprev, yprev);
			m_pListenerChip->Update(*it);
			float x, y; 
			m_pListenerChip->GetPos(x, y);
			if ((m_uLastTick + REPEAT_TIME) <= (AkUInt32)m_pParentMenu->GetTickCount()
				&& (((x - xprev)*(x - xprev) + (y - yprev)*(y - yprev)) > 0.1f))
			{
				AK::SoundEngine::PostEvent("Play_Footstep", LISTENER);
				m_uLastTick = m_pParentMenu->GetTickCount();
				
			}

			m_bMoved = true;
		}

		if (m_uRepeat == 0)
		{
			if ((*it).IsButtonDown(UG_BUTTON1))
			{
				// toggle portals open state.
				m_portalsOpen = (m_portalsOpen + 1) & 0x3;
				SetPortals();
				m_bMoved = true;
				m_uRepeat = REPEAT_TIME;
			}
		}
		else
		{
			m_uRepeat--;
		}
	}

	if ( m_bMoved )
	{
		// Update room dimensions for proper updating of emitters' room and "ray-cast" obstruction.
		float roomx, roomy, roomw, roomh;
		m_pRoom->GetRect(roomx, roomy, roomw, roomh);
		m_roomCornerx = roomx + roomw;
		m_roomCornery = roomy + roomh;

		// Update emitters, set their room.
		UpdateGameObjPos(m_pEmitterChip, EMITTER);
		UpdateGameObjPos(m_pListenerChip, LISTENER);

		if (m_pGameSideObs)
			m_pGameSideObs->Update(this);

		//
		// Query propagation for UI display.
		//
		
		for (int i = 0; i < m_numLines; i++)
		{
			if (m_aLines[i])
				delete m_aLines[i];
		}
		delete[] m_aLines;
		m_aLines = NULL;
		m_numLines = 0;

		AkVector emitterPos, listenerPos;
		AkDiffractionPathInfo paths[8];
		AkUInt32 uNumPaths = 8;	// 8 arbitrary
		AK::SpatialAudio::QueryDiffractionPaths(
			EMITTER,	///< The ID of the game object that the client wishes to query.
			listenerPos,	///< Returns the position of the listener game object that is associated with the game object \c in_gameObjectID.
			emitterPos,		///< Returns the position of the emitter game object \c in_gameObjectID.
			paths,			///< Pointer to an array of \c AkPropagationPathInfo's which will be filled after returning.
			uNumPaths		///< The number of slots in \c out_aPaths, after returning the number of valid elements written.
			);

		// Count lines to draw.
		for (AkUInt32 path = 0; path < uNumPaths; path++)
		{
			int nodeCount = paths[path].nodeCount;
			int numSegments = nodeCount + 1;
			m_numLines += numSegments;
		}

		m_dryDiffraction = 0.f;
		m_wetDiffraction = 0.f;

		if (m_numLines > 0)
		{
			float dryDiffraction = FLT_MAX;
			float wetDiffraction = FLT_MAX;
			bool wetDiffractionSet = false;

			m_aLines = new Line*[m_numLines];

			int line = 0;
			for (AkUInt32 path = 0; path < uNumPaths; path++)
			{
				// Create line segments between each node. 
				// Meanwhile, find out whether one of these nodes is a portal and store its ID.
				if (paths[path].nodeCount > 0)
				{
					// Listener to node 0.
					m_aLines[line] = new Line(*this);
					m_aLines[line++]->SetPoints(AKPosToPixels_X(listenerPos.X), AKPosToPixels_Y(listenerPos.Z), AKPosToPixels_X(paths[path].nodes[0].X), AKPosToPixels_Y(paths[path].nodes[0].Z));

					AkPortalID portalID = paths[path].portals[0];

					AkUInt32 node = 1;
					while (node < paths[path].nodeCount)
					{
						m_aLines[line] = new Line(*this);
						m_aLines[line++]->SetPoints(AKPosToPixels_X(paths[path].nodes[node - 1].X), AKPosToPixels_Y(paths[path].nodes[node - 1].Z), AKPosToPixels_X(paths[path].nodes[node].X), AKPosToPixels_Y(paths[path].nodes[node].Z));
						if (!portalID.IsValid())
							portalID = paths[path].portals[node];
						++node;
					}
					// Last node to emitter.
					m_aLines[line] = new Line(*this);
					m_aLines[line++]->SetPoints(AKPosToPixels_X(paths[path].nodes[node - 1].X), AKPosToPixels_Y(paths[path].nodes[node - 1].Z), AKPosToPixels_X(emitterPos.X), AKPosToPixels_Y(emitterPos.Z));

					// Query portal's wet diffraction.
					AkReal32 portalWetDiffraction;
					if (portalID.IsValid() 
						&& AK::SpatialAudio::QueryWetDiffraction(portalID, portalWetDiffraction) == AK_Success
						&& portalWetDiffraction < wetDiffraction)
					{
						// Pick wetDiffraction as the smallest wet diffraction value across parallel paths.
						wetDiffraction = portalWetDiffraction;
						wetDiffractionSet = true;
					}
				}
				else
				{
					// No node: Listener to emitter.
					m_aLines[line] = new Line(*this);
					m_aLines[line++]->SetPoints(AKPosToPixels_X(listenerPos.X), AKPosToPixels_Y(listenerPos.Z), AKPosToPixels_X(emitterPos.X), AKPosToPixels_Y(emitterPos.Z));
				}

				// Display the minimum diffraction amount of the paths. Since we only have Room (i.e. game object) scoped RTPCs in Wwise at the moment, this is the actual value that will be pushed
				// to our built-in game parameter "Diffraction".
				dryDiffraction = AkMin(dryDiffraction, paths[path].diffraction);
			}
			m_dryDiffraction = dryDiffraction * 100.f;
			if (wetDiffractionSet)
				m_wetDiffraction = wetDiffraction * 100.f;
			else
				m_wetDiffraction = 0.f;
		}
		else
		{
			m_dryDiffraction = 0.f;
			m_wetDiffraction = 0.f;
		}
	}

	// Reset m_bMoved.
	m_bMoved = false;

	return Page::Update();
}

void DemoSpatialAudio::Draw()
{
	Page::Draw();

	if (m_pEmitterChip)
		m_pEmitterChip->Draw();
	if (m_pListenerChip)
		m_pListenerChip->Draw();

	if (m_pRoom)
		m_pRoom->Draw(DrawStyle_Selected);
	if (m_pPortal0)
		m_pPortal0->Draw(DrawStyle_Selected);
	if (m_pPortal1)
		m_pPortal1->Draw(DrawStyle_Selected);

	for (int i = 0; i < m_numLines; i++)
	{
		if (m_aLines[i])
			m_aLines[i]->Draw();
	}

	if (m_pGeometryInRooms)
		m_pGeometryInRooms->Draw();

	if (m_pGameSideObs)
		m_pGameSideObs->Draw();

	char strBuf[128];
	if (m_numLines)
		snprintf(strBuf, 128, "X: %.2f\nZ: %.2f\nDiffraction dry: %.2f\nDiffraction wet: %.2f", m_fGameObjectX, m_fGameObjectZ, m_dryDiffraction, m_wetDiffraction);
	else
		snprintf(strBuf, 128, "X: %.2f\nZ: %.2f\nTransmission", m_fGameObjectX, m_fGameObjectZ);

	static int s_nOffset = 5 * GetLineHeight( DrawStyle_Text );

	DrawTextOnScreen( strBuf, 5, m_pParentMenu->GetHeight() - s_nOffset, DrawStyle_Text );
	
	// Display instructions at the bottom of the page
	int iInstructionsY = m_pParentMenu->GetHeight() - 3 * GetLineHeight( DrawStyle_Text );
	DrawTextOnScreen( "(Press <<UG_BUTTON2>> To Go Back...)", (int)(m_pParentMenu->GetWidth() * 0.4), iInstructionsY, DrawStyle_Text );
}

bool DemoSpatialAudio::OnPointerEvent(PointerEventType in_eType, int in_x, int in_y)
{
	if ( in_eType == PointerEventType_Moved )
	{
		bool bMoveListener = true;
		for (UniversalInput::Iterator it = m_pParentMenu->Input()->Begin(); it != m_pParentMenu->Input()->End(); it++)
		{
			if ((*it).IsButtonDown(UG_BUTTON5))
			{
				bMoveListener = false;
				break;
			}
		}

		if (bMoveListener)
			m_pListenerChip->SetPos((float)in_x, (float)in_y);
		else
			m_pEmitterChip->SetPos((float)in_x, (float)in_y);

		m_bMoved = true;	// Force update of game objects and GUI of sound propagation.
	}

	return Page::OnPointerEvent( in_eType, in_x, in_y );
}

// Called at init but also every time we open or close (all) doors.
void DemoSpatialAudio::InitControls_Portals()
{
	m_pRoom = new Box(*this);
	m_pPortal0 = new Box(*this);
	m_pPortal1 = new Box(*this);

	// Register rooms to Spatial Audio.
	AkRoomParams paramsRoom;
	// Let's orient our rooms towards the top of the screen. 
	paramsRoom.Front.X = 0.f;
	paramsRoom.Front.Y = 0.f;
	paramsRoom.Front.Z = 1.f;
	paramsRoom.Up.X = paramsRoom.Up.Z = 0.f;
	paramsRoom.Up.Y = 1.f;
	paramsRoom.WallOcclusion = 0.9f;	// Let's have a bit of sound transmitted through walls when all portals are closed.
	paramsRoom.RoomGameObj_KeepRegistered = true;	// We intend to use the room's game object to post events (see documentation of AkRoomParams::RoomGameObj_KeepRegistered).
	paramsRoom.strName = "Room Object";
	paramsRoom.RoomGameObj_AuxSendLevelToSelf = 0.25f;	// Since we will be playing an ambience ("Play_Ambience_Quad", below), on this room's game object, we here route some of it to the room's auxiliary bus to add some of its reverb.
	paramsRoom.ReverbAuxBus = AK::SoundEngine::GetIDFromString("Room");
	
	AK::SpatialAudio::SetRoom(ROOM, paramsRoom);
	
	// Also register "outside". Outside is already created automatically, but we want to give it a name, and set its aux send (reverb)
	paramsRoom.WallOcclusion = 0.f;
	paramsRoom.RoomGameObj_KeepRegistered = false; 
	paramsRoom.strName = "Outside Object";
	paramsRoom.ReverbAuxBus = AK::SoundEngine::GetIDFromString("Outside");
	AK::SpatialAudio::SetRoom(AkRoomID::OutdoorsGameObjID, paramsRoom);

	SetPortals();

	// Play a 4.0 ambient sound (room tone) on the ROOM game object. Leveraging the processing performed by Spatial Audio, the room tone, which is set to
	// 3D Spatialization in Wwise, will 
	// - rotate according to the listener orientation when the listener is in the room (this is not actually demonstrated since the listener never rotates);
	// - expand (using spread) as the listener penetrates the room;
	// - contract to portals as the listener walks away from the room.
	AK::SoundEngine::PostEvent("Play_Ambience_Quad", ROOM.AsGameObjectID());
}

void DemoSpatialAudio::SetPortals()
{
	float room0_width = m_room0_width = m_fWidth / 2.f;
	float room0_height = m_room0_height = m_fHeight / 2.f;
	
	m_pRoom->SetPosition(0, 0);
	m_pRoom->SetDimensions((int)room0_width, (int)room0_height);

	const float PORTAL_MARGIN = 15.f;
	const float PORTAL0_WIDTH = 90.f;	// width in screen coordinates (left-right). This is actually used for the portal's depth.
	const float PORTAL0_HEIGHT = 60.f;
	float portal0TopLeftX = room0_width - PORTAL0_WIDTH / 2.f;
	float portal0TopLeftZ = PORTAL_MARGIN;

	m_pPortal0->SetPosition((int)portal0TopLeftX, (int)portal0TopLeftZ);
	m_pPortal0->SetDimensions((int)PORTAL0_WIDTH, (int)PORTAL0_HEIGHT);

	const float PORTAL1_HEIGHT = 90.f;
	
	float portal1TopLeftX = PORTAL_MARGIN;
	float portal1TopLeftZ = room0_height - PORTAL1_HEIGHT / 2.f;
	float portal1Width = room0_width - 2.f * PORTAL_MARGIN;
	m_pPortal1->SetPosition((int)portal1TopLeftX, (int)portal1TopLeftZ);
	m_pPortal1->SetDimensions((int)portal1Width, (int)PORTAL1_HEIGHT);
	
	//
	// Register portals to SpatialAudio.
	//

	AkPortalParams paramsPortal;
	
	//
	// Portal 0 (ROOM->Outside, like an horizontal tube, on the top right)
	//
	paramsPortal.Transform.SetPosition( 
		PixelsToAKPos_X(portal0TopLeftX + PORTAL0_WIDTH / 2.f),
		0,
		PixelsToAKPos_Y(portal0TopLeftZ + PORTAL0_HEIGHT / 2.f));
	// Points to the right.
	// Up vector: This is a 2D game with Y pointing towards the player, and so should the local Y.
	paramsPortal.Transform.SetOrientation(1.f, 0.f, 0.f, 0.f, 1.f, 0.f);
	// Portal extent. Defines the dimensions of the portal relative to its center; all components must be positive numbers. The local X and Y dimensions (side and top) are used in diffraction calculations, 
	// whereas the Z dimension (front) defines a depth value which is used to implement smooth transitions between rooms. It is recommended that users experiment with different portal depths to find a value 
	// that results in appropriately smooth transitions between rooms.
	// Important: divide height and width by 2, because Extent expresses dimensions relative to the center (like a radius).
	paramsPortal.Extent.X = PixelsToAKLen_Y(PORTAL0_HEIGHT / 2.f);	// Door width. Note: PORTAL0_HEIGHT is screen coordinates (up-down).
	paramsPortal.Extent.Y = PixelsToAKLen_Y(PORTAL0_HEIGHT / 2.f);	// This app is 2D. Just pick a non-zero value so that this edge is never chosen.
	paramsPortal.Extent.Z = PixelsToAKLen_X(PORTAL0_WIDTH / 2.f);	// Portal depth (transition region). Note: PORTAL0_WIDTH is screen coordinates (left-right).
	// Whether or not the portal is active/enabled. For example, this parameter may be used to simulate open/closed doors.
	paramsPortal.bEnabled = (m_portalsOpen & 0x1) > 0;	// Open if bit 0 of our portal open state m_portalsOpen is set.
	// Name used to identify portal (optional).
	paramsPortal.strName = "Portal ROOM->Outside, horizontal";
	// ID of the room that the portal connects to, in the direction of the Front vector.
	paramsPortal.FrontRoom = AkRoomID::OutdoorsGameObjID;
	// ID of room that that portal connects, in the direction opposite to the Front vector. 
	paramsPortal.BackRoom = ROOM;

	AK::SpatialAudio::SetPortal(PORTAL0, paramsPortal);
	
	
	//
	// Portal 1 (Outside->ROOM, like a wide vertical tube, bottom-left).
	//
	paramsPortal.Transform.SetPosition(
		PixelsToAKPos_X(portal1TopLeftX + portal1Width / 2.f),
		0,
		PixelsToAKPos_Y(portal1TopLeftZ + PORTAL1_HEIGHT / 2.f));
	// Points up towards ROOM.
	// Up vector: This is a 2D game with Y pointing towards the player, and so should the local Y.
	paramsPortal.Transform.SetOrientation(0.f, 0.f, 1.f, 0.f, 1.f, 0.f);
	// Portal extent. Defines the dimensions of the portal relative to its center; all components must be positive numbers. The local X and Y dimensions (side and top) are used in diffraction calculations, 
	// whereas the Z dimension (front) defines a depth value which is used to implement smooth transitions between rooms. It is recommended that users experiment with different portal depths to find a value 
	// that results in appropriately smooth transitions between rooms.
	// Important: divide width and height by 2, because Extent expresses dimensions relative to the center (like a radius).
	paramsPortal.Extent.X = PixelsToAKLen_X(portal1Width / 2.f);	// Door width. 
	paramsPortal.Extent.Y = PixelsToAKLen_X(PORTAL0_HEIGHT / 2.f);	// This app is 2D. Just pick a non-zero value so that this edge is never chosen.
	paramsPortal.Extent.Z = PixelsToAKLen_Y(PORTAL1_HEIGHT / 2.f);	// Portal depth (transition region). 
	/// Whether or not the portal is active/enabled. For example, this parameter may be used to simulate open/closed doors.
	paramsPortal.bEnabled = (m_portalsOpen & 0x2) > 0;	// Open if bit 1 of our portal open state m_portalsOpen is set.
	/// Name used to identify portal (optional).
	paramsPortal.strName = "Outside->ROOM, vertical";
	// ID of the room that the portal connects to, in the direction of the Front vector.
	paramsPortal.FrontRoom = ROOM;
	// ID of room that that portal connects, in the direction opposite to the Front vector. 
	paramsPortal.BackRoom = AkRoomID::OutdoorsGameObjID;

	AK::SpatialAudio::SetPortal(PORTAL1, paramsPortal);	
}

void DemoSpatialAudio::InitControls()
{
	float fMargin = (float)m_pEmitterChip->GetRightBottomMargin();
	float x = (m_pParentMenu->GetWidth() - fMargin) * 1.f / 2.0f;
	float y = (m_pParentMenu->GetHeight() - fMargin) * 1.f / 2.0f;

	m_pEmitterChip = new MovableChip(*this);
	m_pEmitterChip->SetLabel( "<E>" );
	m_pEmitterChip->UseJoystick(UG_STICKRIGHT);
	m_pEmitterChip->SetPos(x - 100, y - 100);
	m_pEmitterChip->SetNonLinear();
	m_pEmitterChip->SetMaxSpeed(3.f);

	m_pListenerChip = new MovableChip(*this);
	m_pListenerChip->SetLabel("<L>");
	m_pListenerChip->UseJoystick(UG_STICKRIGHT);
	m_pListenerChip->SetNonLinear();
	m_pListenerChip->SetPos(x + 30, y + 30);
	m_pListenerChip->SetMaxSpeed(3.f);
	m_pListenerChip->Update(*(m_pParentMenu->Input()->Begin()));

	m_fWidth = (float)m_pParentMenu->GetWidth() - fMargin;
	m_fHeight = (float)m_pParentMenu->GetHeight() - fMargin;


	// In the Portals (only) demo, the "game" takes care of computing and setting obstruction between the emitter and listener when they are in the same room, and between portals and the listener.
	// This is implemented in GameSideObstructionComponent. No geometry in passed to Wwise Spatial Audio in that demo, and m_pGeometryInRooms is thus NULL.
	// In the Portals + Geometry demo, propagation of sound via diffraction effects is entirely handled by Wwise Spatial Audio, either via Portals or geometry, and thus m_pGameSideObs is not needed and left NULL.
	switch (m_eScenario)
	{
	case Scenario_Portals:
		InitControls_Portals();
		m_pGameSideObs = new GameSideObstructionComponent;
		AK::SoundEngine::PostEvent("Play_Room_Emitter", EMITTER);
		break;
	case Scenario_Portals_And_Geometry:
		InitControls_Portals();
		m_pGeometryInRooms = new GeometryInRooms(this);
		AK::SoundEngine::PostEvent("Play_Room_Emitter", EMITTER);
		break;
	}


	// Update movable chips the first time.
	UniversalInput::Iterator it;
	for (it = m_pParentMenu->Input()->Begin(); it != m_pParentMenu->Input()->End(); it++)
	{
		// Skip this input device if it's not connected
		if (!it->IsConnected())
			continue;

		m_pEmitterChip->Update(*it);
		m_pListenerChip->Update(*it);
		UpdateGameObjPos(m_pEmitterChip, EMITTER);
		UpdateGameObjPos(m_pListenerChip, LISTENER);
	}
}


//
// GameSideObstructionComponent:
// Implements game-side obstruction when emitters and listeners are in the same room.
//
GameSideObstructionComponent::GameSideObstructionComponent()
	:m_aRaycastEtoL(NULL)
{
}

GameSideObstructionComponent::~GameSideObstructionComponent()
{
	delete m_aRaycastEtoL;
}

void GameSideObstructionComponent::Draw()
{
	if (m_aRaycastEtoL)
		m_aRaycastEtoL->Draw(DrawStyle_Selected);
}

void GameSideObstructionComponent::Update(DemoSpatialAudio * in_pDemo)
{
	// Perform naive ray-casting for same-room obstruction.
	// We use game-side ray-casting to compute the obstruction between the listener and each portal.
	// We also use it to compute obstruction between the emitter and listener *only when they are in the same room*. When they aren't, obstruction flows from more diffraction/transmission modeling of SpatialAudio's rooms and portals.
	const float k_obstructed = 0.7f;

	float listx, listy;
	in_pDemo->m_pListenerChip->GetPos(listx, listy);
	float emitx, emity;
	in_pDemo->m_pEmitterChip->GetPos(emitx, emity);

	// Compute coordinates relative to the room's corner.
	// Quadrants:
	// 2 | 1
	// -----
	// 3 | 4
	float roomCornerx = in_pDemo->m_roomCornerx;
	float roomCornery = in_pDemo->m_roomCornery;
	float relListx = (listx - roomCornerx);
	float relListy = (listy - roomCornery);

	// Compute obstruction between portals and the listener. Portal 0 is obstructed when the listener is in quadrant 3, and portal 1 is obstructed when the listener is in quadrant 1.
	float obsPortal0 = (relListx < 0 && relListy > 0) ? k_obstructed : 0.f;
	float obsPortal1 = (relListx > 0 && relListy < 0) ? k_obstructed : 0.f;
	
	// Pass to SpatialAudio.
	AK::SpatialAudio::SetPortalObstructionAndOcclusion(PORTAL0, obsPortal0, 0.f);
	AK::SpatialAudio::SetPortalObstructionAndOcclusion(PORTAL1, obsPortal1, 0.f);

	// Emitter: if it is in the same room as the listener, compute a ray-cast based obstruction value.
	// This is done by the game, with various levels of complexity, and is set directly to the sound engine via AK::SoundEngine::SetObjectObstructionAndOcclusion.
	// If the emitter is in a different room, Spatial Audio will pick the largest values between obstruction/occlusion provided by the user, and those driven by
	// diffraction/transmission. Since we want the latter to apply, let's pass 0.
	float obsEmitter = 0.f;
	if (in_pDemo->IsInRoom(listx, listy) == in_pDemo->IsInRoom(emitx, emity))
	{
		float relEmitx = (emitx - roomCornerx);
		float relEmity = (emity - roomCornery);
		// Obstruction is non-zero only if emitter and listener are not in the same quadrant
		if (((relEmity >= 0) ^ (relListy >= 0)) && relEmitx * relListx < 0)
		{
			float crossprod = relListx * relEmity - relEmitx * relListy;
			obsEmitter = (relEmitx * crossprod > 0.f) ? k_obstructed : 0.f;
		}
	}
	AK::SoundEngine::SetObjectObstructionAndOcclusion(EMITTER, LISTENER, obsEmitter, 0.0f);

	// Ray-cast line between emitter and listener. Show it when they are in the same room and obstruction is 0.
	delete m_aRaycastEtoL;
	m_aRaycastEtoL = NULL;
	if (obsEmitter == 0.f && (in_pDemo->IsInRoom(listx, listy) == in_pDemo->IsInRoom(emitx, emity)))
	{
		Line * pLine = new Line(*in_pDemo);
		pLine->SetPoints((int)emitx, (int)emity, (int)listx, (int)listy);
		m_aRaycastEtoL = pLine;
	}
}


//
// GeometryInRooms:
// Defines and registers geometry to Spatial Audio.
//
GeometryInRooms::GeometryInRooms(DemoSpatialAudio * in_pDemo)
{
	for (int i = 0; i < k_numGeometryPlanes; i++)
		m_geometry[i] = NULL;

	// Let's construct 3 disjoint meshes: one for the inside wall, assigned to room ROOM, one for the outside wall, assigned to room AkRoomID::OutdoorsGameObjID, and another one for an obstacle, outside.

	// In general, for diffraction, it is important to join meshes to avoid sound leaking through cracks.
	// Here in our 2D geometry, we do not assign triangles to the top and bottom planes (through screen), and utilize
	// AkGeometryParams::EnableDiffractionOnBoundaryEdges = false so that edges on top and bottom will not allow sound to pass through. 

	float room0_width_px = in_pDemo->m_room0_width;
	float room0_height_px = in_pDemo->m_room0_height;
	if (room0_height_px < 0)
		room0_height_px = 0;

	int line = 0;
	int centerx = in_pDemo->AKPosToPixels_X(0);
	int centery = in_pDemo->AKPosToPixels_Y(0);
	float horizontal_wall_left = -in_pDemo->PixelsToAKLen_X(room0_width_px);
	float room0_height = in_pDemo->PixelsToAKLen_Y(room0_height_px);


	{
		// Outer wall (assigned to Outside).
		//

		//       
		//		  |
		//        |
		// -------|

		// Draw Lines.
		m_geometry[line] = new Line(*in_pDemo);
		m_geometry[line++]->SetPoints(centerx, centery, centerx, centery - (int)room0_height_px);
		m_geometry[line] = new Line(*in_pDemo);
		m_geometry[line++]->SetPoints(centerx, centery, centerx - (int)room0_width_px, centery);

		// Register to SpatialAudio
		const int k_numVertices = 6;

		AkGeometryParams geom;
		geom.NumVertices = k_numVertices;
		// In the coordinate system we have chosen, x is right-left, z is top-bottom and y is through screen. (0,0,0) is the center of the window.
		/*
		AkVertex vertices[] = {
		{ 0, -30.f, 0 },
		{ 0, 30.f, 0 },
		{ 0, 30.f, room0_height },
		{ 0, -30.f, room0_height },
		{ horizontal_wall_left, -30.f, 0 },
		{ horizontal_wall_left, 30.f, 0 }
		};
		*/
		// Init one by one for old compilers.
		AkVertex vertices[k_numVertices];
		AkPlacementNew(vertices + 0) AkVertex(0, -30.f, 0);
		AkPlacementNew(vertices + 1) AkVertex(0, 30.f, 0);
		AkPlacementNew(vertices + 2) AkVertex(0, 30.f, room0_height);
		AkPlacementNew(vertices + 3) AkVertex(0, -30.f, room0_height);
		AkPlacementNew(vertices + 4) AkVertex(horizontal_wall_left, -30.f, 0);
		AkPlacementNew(vertices + 5) AkVertex(horizontal_wall_left, 30.f, 0);
		geom.Vertices = vertices;

		// Assign surfaces. Direct path diffraction is not affected by the acoustic properties of surfaces, because in this model, the edges don't absorb any energy.
		// In this demo, the only purpose of using acoustic surfaces with textures defined in Wwise is for showing colors in the 3D Game Object Viewer.
		geom.NumSurfaces = 1;
		AkAcousticSurface outerSurface;
		outerSurface.strName = "Outside";
		outerSurface.textureID = AK::SoundEngine::GetIDFromString("Brick");
		geom.Surfaces = &outerSurface;

		const int k_numTriOutside = 4;
		geom.NumTriangles = k_numTriOutside;
		/*
		AkTriangle tri[] = {
		{ 0, 1, 2, 0 },
		{ 0, 2, 3, 0 },
		{ 0, 1, 4, 0 },
		{ 1, 4, 5, 0 }
		};
		*/
		// Init one by one for old compilers.
		AkTriangle tri[k_numTriOutside];
		AkPlacementNew(tri + 0) AkTriangle(0, 1, 2, 0);
		AkPlacementNew(tri + 1) AkTriangle(0, 2, 3, 0);
		AkPlacementNew(tri + 2) AkTriangle(0, 1, 4, 0);
		AkPlacementNew(tri + 3) AkTriangle(1, 4, 5, 0);
		geom.Triangles = tri;

		geom.EnableDiffraction = true;
		// We did not add triangles on the top and bottom (through screen) of our "wall", and set EnableDiffractionOnBoundaryEdges to false,
		// so that sound does not diffract via the top and bottom edges.
		geom.EnableDiffractionOnBoundaryEdges = false;

		// Associate with room "Outside". Doing so, the edges used by geometric diffraction are only searched when emitters or the listener are in this room, and the search space is limited to this room, 
		// which is much quicker than searching across the edges of all geometry. Coupling between edge diffraction of separate rooms is handled by portal diffraction, which is less CPU intensive.
		geom.RoomID = AkRoomID::OutdoorsGameObjID;

		AK::SpatialAudio::SetGeometry(0, geom);
	}

	{
		// Inner wall (assigned to ROOM)
		// Note: the geometry assigned to ROOM is offset from the outside wall constructed above, by k_offsetInnerWall pixels, to represent the wall of a room and the thickness of the "building" as it would
		// typically occur in a video game, or real life. However this is not necessary; the walls may be coincident (k_offsetInnerWall = 0) or even reversed (k_offsetInnerWall < 0). An emitter in room ROOM
		// will always only see these walls 
		//

		// 45|------|32
		//	 |		|
		// 76|------|01

		// Draw Lines.
		const int k_offsetInnerWall = 5;
		int topleftx_px = 0;
		int toplefty_px = 0;
		float bottomrightx_px = room0_width_px - k_offsetInnerWall;
		float bottomrighty_px = room0_height_px - k_offsetInnerWall;

		m_geometry[line] = new Line(*in_pDemo);
		m_geometry[line++]->SetPoints(topleftx_px, toplefty_px, (int)bottomrightx_px, toplefty_px);
		m_geometry[line] = new Line(*in_pDemo);
		m_geometry[line++]->SetPoints((int)bottomrightx_px, toplefty_px, (int)bottomrightx_px, (int)bottomrighty_px);
		m_geometry[line] = new Line(*in_pDemo);
		m_geometry[line++]->SetPoints((int)bottomrightx_px, (int)bottomrighty_px, topleftx_px, (int)bottomrighty_px);
		m_geometry[line] = new Line(*in_pDemo);
		m_geometry[line++]->SetPoints(topleftx_px, (int)bottomrighty_px, topleftx_px, toplefty_px);


		// Register to SpatialAudio

		float topleftx = in_pDemo->PixelsToAKPos_X((float)topleftx_px);
		float toplefty = in_pDemo->PixelsToAKPos_Y((float)toplefty_px);
		float bottomrightx = in_pDemo->PixelsToAKPos_X(bottomrightx_px);
		float bottomrighty = in_pDemo->PixelsToAKPos_Y(bottomrighty_px);

		AkGeometryParams geom;

		const int k_numVertices = 8;
		geom.NumVertices = k_numVertices;
		// In the coordinate system we have chosen, x is right-left, z is top-bottom and y is through screen. (0,0,0) is the center of the window.
		/*
		AkVertex vertices[] = {
			{bottomrightx, -30.f, bottomrighty};
			{bottomrightx, 30.f, bottomrighty};
			{bottomrightx, 30.f, toplefty};
			{bottomrightx, -30.f, toplefty};
			{topleftx, -30.f, toplefty};
			{topleftx, 30.f, toplefty};
			{topleftx, 30.f, bottomrighty};
			{topleftx, -30.f, bottomrighty};
		};
		*/
		// Init one by one for old compilers.
		AkVertex vertices[k_numVertices];
		AkPlacementNew(vertices + 0) AkVertex(bottomrightx, -30.f, bottomrighty);
		AkPlacementNew(vertices + 1) AkVertex(bottomrightx, 30.f, bottomrighty);
		AkPlacementNew(vertices + 2) AkVertex(bottomrightx, 30.f, toplefty);
		AkPlacementNew(vertices + 3) AkVertex(bottomrightx, -30.f, toplefty);
		AkPlacementNew(vertices + 4) AkVertex(topleftx, -30.f, toplefty);
		AkPlacementNew(vertices + 5) AkVertex(topleftx, 30.f, toplefty);
		AkPlacementNew(vertices + 6) AkVertex(topleftx, 30.f, bottomrighty);
		AkPlacementNew(vertices + 7) AkVertex(topleftx, -30.f, bottomrighty);
		geom.Vertices = vertices;

		// Assign surfaces. Direct path diffraction is not affected by the acoustic properties of surfaces, because in this model, the edges don't absorb any energy.
		// In this demo, the only purpose of using acoustic surfaces with textures defined in Wwise is for showing colors in the 3D Game Object Viewer.
		geom.NumSurfaces = 1;
		AkAcousticSurface innerSurface;
		innerSurface.strName = "Inside";
		innerSurface.textureID = AK::SoundEngine::GetIDFromString("Drywall");
		geom.Surfaces = &innerSurface;

		const int k_numTriRoom = 8;
		geom.NumTriangles = k_numTriRoom;
		/*
		AkTriangle tri[] = {
		{ 0, 1, 2, 0 },
		{ 0, 2, 3, 0 },
		{ 2, 3, 4, 0 },
		{ 2, 4, 5, 0 },
		{ 4, 5, 6, 0 },
		{ 4, 6, 7, 0 },
		{ 0, 1, 6, 0 },
		{ 0, 6, 7, 0 }
		};
		*/
		// Init one by one for old compilers.
		AkTriangle tri[k_numTriRoom];
		AkPlacementNew(tri + 0) AkTriangle(0, 1, 2, 0);
		AkPlacementNew(tri + 1) AkTriangle(0, 2, 3, 0);
		AkPlacementNew(tri + 2) AkTriangle(2, 3, 4, 0);
		AkPlacementNew(tri + 3) AkTriangle(2, 4, 5, 0);
		AkPlacementNew(tri + 4) AkTriangle(4, 5, 6, 0);
		AkPlacementNew(tri + 5) AkTriangle(4, 6, 7, 0);
		AkPlacementNew(tri + 6) AkTriangle(0, 1, 6, 0);
		AkPlacementNew(tri + 7) AkTriangle(0, 6, 7, 0);
		geom.Triangles = tri;

		// Associate with room "Outside". Doing so, the edges used by geometric diffraction are only searched when emitters or the listener are in this room, and the search space is limited to this room, 
		// which is much quicker than searching across the edges of all geometry. Coupling between edge diffraction of separate rooms is handled by portal diffraction, which is less CPU intensive.
		geom.RoomID = ROOM;

		AK::SpatialAudio::SetGeometry(1, geom);
	}


	{
		// Obstacle, outside.
		//

		// Draw Lines.
		const float k_thickness = 5;
		float topleftx_px = room0_width_px / 3.f;
		float toplefty_px = room0_height_px * 1.333f;
		float bottomrightx_px = room0_width_px * 1.333f;
		float bottomrighty_px = toplefty_px + k_thickness;
		m_geometry[line] = new Line(*in_pDemo);
		m_geometry[line++]->SetPoints((int)topleftx_px, (int)toplefty_px, (int)bottomrightx_px, (int)toplefty_px);
		m_geometry[line] = new Line(*in_pDemo);
		m_geometry[line++]->SetPoints((int)bottomrightx_px, (int)toplefty_px, (int)bottomrightx_px, (int)bottomrighty_px);
		m_geometry[line] = new Line(*in_pDemo);
		m_geometry[line++]->SetPoints((int)bottomrightx_px, (int)bottomrighty_px, (int)topleftx_px, (int)bottomrighty_px);
		m_geometry[line] = new Line(*in_pDemo);
		m_geometry[line++]->SetPoints((int)topleftx_px, (int)bottomrighty_px, (int)topleftx_px, (int)toplefty_px);

		float topleftx = in_pDemo->PixelsToAKPos_X(topleftx_px);
		float toplefty = in_pDemo->PixelsToAKPos_Y(toplefty_px);
		float bottomrightx = in_pDemo->PixelsToAKPos_X(bottomrightx_px);
		float bottomrighty = in_pDemo->PixelsToAKPos_Y(bottomrighty_px);

		const int k_numVertices = 8;

		AkGeometryParams geom;
		geom.NumVertices = k_numVertices;
		// In the coordinate system we have chosen, x is right-left, z is top-bottom and y is through screen. (0,0,0) is the center of the window.

		AkVertex vertices[k_numVertices];
		AkPlacementNew(vertices + 0) AkVertex(topleftx, 30.f, toplefty);
		AkPlacementNew(vertices + 1) AkVertex(topleftx, -30.f, toplefty);
		AkPlacementNew(vertices + 2) AkVertex(bottomrightx, -30.f, toplefty);
		AkPlacementNew(vertices + 3) AkVertex(bottomrightx, 30.f, toplefty);
		AkPlacementNew(vertices + 4) AkVertex(bottomrightx, 30.f, bottomrighty);
		AkPlacementNew(vertices + 5) AkVertex(bottomrightx, -30.f, bottomrighty);
		AkPlacementNew(vertices + 6) AkVertex(topleftx, -30.f, bottomrighty);
		AkPlacementNew(vertices + 7) AkVertex(topleftx, 30.f, bottomrighty);
		geom.Vertices = vertices;

		// Assign surfaces. Direct path diffraction is not affected by the acoustic properties of surfaces, because in this model, the edges don't absorb any energy.
		// In this demo, the only purpose of using acoustic surfaces with textures defined in Wwise is for showing colors in the 3D Game Object Viewer.
		geom.NumSurfaces = 1;
		AkAcousticSurface outerSurface;
		outerSurface.strName = "Outside";
		outerSurface.textureID = AK::SoundEngine::GetIDFromString("Brick");
		geom.Surfaces = &outerSurface;

		const int k_numTriangles = 8;
		geom.NumTriangles = k_numTriangles;

		AkTriangle tri[k_numTriangles];
		AkPlacementNew(tri + 0) AkTriangle(0, 1, 2, 0);
		AkPlacementNew(tri + 1) AkTriangle(0, 2, 3, 0);
		AkPlacementNew(tri + 2) AkTriangle(2, 3, 4, 0);
		AkPlacementNew(tri + 3) AkTriangle(2, 4, 5, 0);
		AkPlacementNew(tri + 4) AkTriangle(4, 5, 6, 0);
		AkPlacementNew(tri + 5) AkTriangle(4, 6, 7, 0);
		AkPlacementNew(tri + 6) AkTriangle(6, 7, 0, 0);
		AkPlacementNew(tri + 7) AkTriangle(6, 0, 1, 0);
		geom.Triangles = tri;

		geom.EnableDiffraction = true;
		// We did not add triangles on the top and bottom (through screen) of our "wall", and set EnableDiffractionOnBoundaryEdges to false,
		// so that sound does not diffract via the top and bottom edges.
		geom.EnableDiffractionOnBoundaryEdges = false;

		// Associate with room "Outside". Doing so, the edges used by geometric diffraction are only searched when emitters or the listener are in this room, and the search space is limited to this room, 
		// which is much quicker than searching across the edges of all geometry. Coupling between edge diffraction of separate rooms is handled by portal diffraction, which is less CPU intensive.
		geom.RoomID = AkRoomID::OutdoorsGameObjID;

		AK::SpatialAudio::SetGeometry(2, geom);
	}

	AKASSERT(line == k_numGeometryPlanes);
}

GeometryInRooms::~GeometryInRooms()
{
	for (int i = 0; i < k_numGeometryPlanes; i++)
		delete m_geometry[i];
	AK::SpatialAudio::RemoveGeometry(0);
	AK::SpatialAudio::RemoveGeometry(1);
	AK::SpatialAudio::RemoveGeometry(2);
}

void GeometryInRooms::Draw()
{
	for (int i = 0; i < k_numGeometryPlanes; i++)
	{
		if (m_geometry[i])
			m_geometry[i]->Draw();
	}
}
