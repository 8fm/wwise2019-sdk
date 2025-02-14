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

#ifndef _AK_MATERIAL_FILTER_MANAGER_H_
#define _AK_MATERIAL_FILTER_MANAGER_H_

#include "AkIndexable.h"
#include "AkVirtualAcousticsProps.h"
#include <AK/SoundEngine/Common/AkVirtualAcoustics.h>

enum AkVirtualAcousticsType
{
	AkVirtualAcousticsType_Texture
};
class CAkVirtualAcoustics
	: public CAkIndexable
{
public:

	static CAkVirtualAcoustics* Create(AkUniqueID in_ulID, AkVirtualAcousticsType in_eType);
	virtual ~CAkVirtualAcoustics();

	virtual void SetAkProp(AkVirtualAcousticsPropID in_eProp, AkReal32 in_fValue, AkReal32 in_fMin, AkReal32 in_fMax) = 0;
	virtual void SetAkProp(AkVirtualAcousticsPropID in_eProp, AkInt32 in_iValue, AkInt32 in_iMin, AkInt32 in_iMax) = 0;

	virtual AkUInt32 AddRef();
	virtual AkUInt32 Release();

protected:
	AKRESULT Init();
	CAkVirtualAcoustics(AkUniqueID in_ulID);

	void AddToIndex();	// Adds the Attenuation in the General index
	void RemoveFromIndex();	// Removes the Attenuation from the General index
};

class CAkAcousticTexture : public CAkVirtualAcoustics
{
public:
	CAkAcousticTexture(AkUniqueID in_ulID);

	inline const AkAcousticTexture * Get() const { return &m_texture; }
	inline void Set(const AkAcousticTexture & in_texture) {
		m_texture = in_texture;
	}

protected:
	virtual void SetAkProp(AkVirtualAcousticsPropID in_eProp, AkReal32 in_fValue, AkReal32 in_fMin, AkReal32 in_fMax);
	virtual void SetAkProp(AkVirtualAcousticsPropID in_eProp, AkInt32 in_iValue, AkInt32 in_iMin, AkInt32 in_iMax);

	AkAcousticTexture m_texture;
};

class CAkVirtualAcousticsMgr
{
public:

	//Public Methods
	static AKRESULT AddAcousticTexture(AkAcousticTextureID in_AcousticTextureID, const AkAcousticTexture& in_pNode);
	static const AkAcousticTexture* GetAcousticTexture(AkAcousticTextureID in_AcousticTextureID);
};

#endif //_AK_MATERIAL_FILTER_MANAGER_H_
