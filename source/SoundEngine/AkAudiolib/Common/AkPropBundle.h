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

#include <AK/Tools/Common/AkBankReadHelpers.h>
#include <AK/Tools/Common/AkObject.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>

#include "AkParameters.h"

// Bundle of properties specified by AkPropID. 
// m_pProps points to a dynamic structure consisting of:
//
// 1 x AkUInt8 = number of properties (cProp)
// cProp x AkUInt8 = cProp times AkPropID of property
// padding to 32 bit
// cProp x T_VALUE = cProp times value of property
//

template< class T_VALUE, typename T_INDEX = AkUInt8, AkMemID T_MEMID = AkMemID_Object >
class AkPropBundle
{
public:
	struct Iterator
	{
		T_INDEX* pID;
		T_VALUE* pValue;

		/// ++ operator
		Iterator& operator++()
		{
			AKASSERT( pID && pValue );
			++pID;
			++pValue;
			return *this;
		}

		/// -- operator
        Iterator& operator--()
		{
			AKASSERT( pID && pValue );
			--pID;
			--pValue;
			return *this;
		}

		/// * operator
		T_VALUE& operator*()
		{
			AKASSERT( pValue );
			return *pValue;
		}

		/// == operator
		bool operator ==( const Iterator& in_rOp ) const
		{
			return ( pID == in_rOp.pID );
		}

		/// != operator
		bool operator !=( const Iterator& in_rOp ) const
		{
			return ( pID != in_rOp.pID );
		}
	};

	/// Returns the iterator to the first item of the array, will be End() if the array is empty.
	Iterator Begin() const
	{
		Iterator returnedIt;
		if ( m_pProps )
		{
			T_INDEX cProps = m_pProps[ 0 ];
			returnedIt.pID = m_pProps + 1;
			returnedIt.pValue = (T_VALUE *) ( (AkUInt8*)m_pProps + FirstPropByteOffset( cProps ) );
		}
		else
		{
			returnedIt.pID = NULL;
			returnedIt.pValue = NULL;
		}
		return returnedIt;
	}

	/// Returns the iterator to the end of the array
	Iterator End() const
	{
		Iterator returnedIt;
		returnedIt.pValue = NULL; // Since iterator comparison checks only pID
		if ( m_pProps )
		{
			T_INDEX cProps = m_pProps[ 0 ];
			returnedIt.pID = m_pProps + cProps + 1;
		}
		else
		{
			returnedIt.pID = NULL;
		}
		return returnedIt;
	}

	AkPropBundle() : m_pProps( NULL ) {}

	~AkPropBundle()
	{
		RemoveAll();
	}

	AKRESULT SetInitialParams( AkUInt8*& io_rpData, AkUInt32& io_rulDataSize ) 
	{
		AKASSERT( !m_pProps );

		T_INDEX cProps = READBANKDATA( T_INDEX, io_rpData, io_rulDataSize );
		if ( cProps )
		{
			AkUInt32 uFirstPropByteOffset = FirstPropByteOffset( cProps );
			AkUInt32 uAllocSize = uFirstPropByteOffset + sizeof( T_VALUE ) * cProps;
			T_INDEX * pProps = (T_INDEX *) AkAlloc(T_MEMID, uAllocSize );
			if ( !pProps )
				return AK_InsufficientMemory;

			pProps[0] = cProps;

			// Copy prop ids
			memcpy( pProps + 1, io_rpData, sizeof( T_INDEX ) * cProps );
			SKIPBANKBYTES( sizeof( T_INDEX ) * cProps, io_rpData, io_rulDataSize );

			// Copy prop values
			memcpy( (AkUInt8*)pProps + uFirstPropByteOffset, io_rpData, sizeof( T_VALUE ) * cProps );
			SKIPBANKBYTES( sizeof( T_VALUE ) * cProps, io_rpData, io_rulDataSize );

			m_pProps = pProps;
		}

		return AK_Success;
	}

	// Add a property value, when it is known that there is currently no value for this property in the bundle.
	T_VALUE * AddAkProp( const T_INDEX in_ePropID )
	{
		AKASSERT( !FindProp( in_ePropID ) );

		AkUInt32 cProps = m_pProps ? m_pProps[ 0 ] : 0;

		AkUInt32 uAllocSize = FirstPropByteOffset( cProps + 1 ) + (cProps + 1) * sizeof( T_VALUE );
		T_INDEX * pProps = (T_INDEX *)AkAlloc(T_MEMID, uAllocSize);
		if ( !pProps )
			return NULL;

		if ( m_pProps )
		{
			// Copy prop ids
			memcpy( pProps + 1, m_pProps + 1, sizeof( T_INDEX ) * cProps );

			// Copy prop values
			memcpy( (AkUInt8*)pProps + FirstPropByteOffset( cProps + 1 ), (AkUInt8*)m_pProps + FirstPropByteOffset( cProps ), sizeof( T_VALUE ) * cProps );

			AkFree(T_MEMID, m_pProps);
		}

		pProps[ cProps + 1 ] = in_ePropID;

		pProps[ 0 ] = (T_INDEX) (cProps + 1);
		m_pProps = pProps;

		T_VALUE * pProp = (T_VALUE*) ( (AkUInt8*)pProps + FirstPropByteOffset( cProps + 1 ) ) + cProps;
		return pProp;
	}

	// Set a property value, adding the property to bundle if necessary.
	AKRESULT SetAkProp( T_INDEX in_ePropID, T_VALUE in_Value )
	{
		T_VALUE * pProp = FindProp( in_ePropID );
		if ( !pProp )
			pProp = AddAkProp( in_ePropID );

		if ( pProp )
		{
			*pProp = in_Value;
			return AK_Success;
		}

		return AK_Fail;
	}

	AkForceInline T_VALUE GetAkProp( T_INDEX in_ePropID, const T_VALUE in_DefaultValueOverride) const
	{
		T_VALUE * pProp = FindProp( in_ePropID );

		return pProp ? *pProp : in_DefaultValueOverride;
	}	

	T_VALUE * FindProp( T_INDEX in_ePropID ) const
	{
		if ( m_pProps )
		{
			AkUInt32 iProp = 0;
			AkUInt32 cProp = m_pProps[ 0 ];
			AKASSERT( cProp > 0 );

			do
			{
				if ( m_pProps[ iProp + 1 ] == in_ePropID )
					return (T_VALUE*) ( (AkUInt8*)m_pProps + FirstPropByteOffset( cProp ) ) + iProp;
			}
			while ( ++iProp < cProp );
		}

		return NULL;
	}

	void RemoveAll()
	{
		if (m_pProps)
			AkFree(T_MEMID, m_pProps);

		m_pProps = NULL;
	}

	bool IsEmpty() const { return m_pProps == NULL; }

private:
	// Return byte offset of first property value
	static AkForceInline AkUInt32 FirstPropByteOffset( AkUInt32 in_cProp )
	{
		// Enforce 4-byte alignment for T_VALUE
		return ( ( ( in_cProp + 1 ) * sizeof(T_INDEX) ) + 3 ) & ~0x3;
	}

	T_INDEX * m_pProps;
};
