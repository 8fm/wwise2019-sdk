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

#ifndef AK_OPTIMIZED

#include "AkPrivateTypes.h"
#include "CommandData.h" // for all derived classes

class IObjectProxy
{
public:
	virtual void AddRef() = 0;
	virtual bool Release() = 0;
	
	virtual AkUniqueID GetID() const = 0;

    virtual bool DoesNeedEndianSwap() const = 0;

	virtual bool IsLocalProxy() const = 0;

	enum MethodIDs
	{
		LastMethodID = 1
	};
};

namespace ObjectProxyCommandData
{
	struct CommandData : public ProxyCommandData::CommandData
	{
		inline CommandData() {}
		inline CommandData( AkUInt16 in_methodID ) : ProxyCommandData::CommandData( ProxyCommandData::TypeObjectProxy, in_methodID ) {}

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUInt64 m_proxyInstancePtr;
		AkUniqueID m_objectID;

		DECLARE_BASECLASS( ProxyCommandData::CommandData );
	};

	template< AkUInt32 METHOD_ID >
	struct CommandData0 : public CommandData
	{
		inline CommandData0() : ObjectProxyCommandData::CommandData( METHOD_ID ) {}

		inline CommandData0( IObjectProxy * in_pProxy ) : ObjectProxyCommandData::CommandData( METHOD_ID )
		{
			m_proxyInstancePtr = (AkUInt64) in_pProxy;
			m_objectID = in_pProxy->GetID();
		}
	};

	template< AkUInt32 METHOD_ID, class PARAM1 >
	struct CommandData1 : public CommandData
	{
		inline CommandData1() : ObjectProxyCommandData::CommandData( METHOD_ID ) {}

		inline CommandData1( IObjectProxy * in_pProxy, const PARAM1 & in_param1 ) 
			: ObjectProxyCommandData::CommandData( METHOD_ID )
			, m_param1( in_param1 )
		{
			m_proxyInstancePtr = (AkUInt64) in_pProxy;
			m_objectID = in_pProxy->GetID();
		}

		inline bool Serialize( CommandDataSerializer& in_rSerializer ) const
		{
			return ObjectProxyCommandData::CommandData::Serialize( in_rSerializer )
				&& in_rSerializer.Put( m_param1 );
		}

		inline bool Deserialize( CommandDataSerializer& in_rSerializer )
		{
			return ObjectProxyCommandData::CommandData::Deserialize( in_rSerializer )
				&& in_rSerializer.Get( m_param1 );
		}

		PARAM1 m_param1;
	};

	template< AkUInt32 METHOD_ID, class PARAM1, class PARAM2 >
	struct CommandData2 : public CommandData
	{
		inline CommandData2() : ObjectProxyCommandData::CommandData( METHOD_ID ) {}

		inline CommandData2( IObjectProxy * in_pProxy, const PARAM1 & in_param1, const PARAM2 & in_param2 ) 
			: ObjectProxyCommandData::CommandData( METHOD_ID )
			, m_param1( in_param1 )
			, m_param2( in_param2 )
		{
			m_proxyInstancePtr = (AkUInt64) in_pProxy;
			m_objectID = in_pProxy->GetID();
		}

		inline bool Serialize( CommandDataSerializer& in_rSerializer ) const
		{
			return ObjectProxyCommandData::CommandData::Serialize( in_rSerializer )
				&& in_rSerializer.Put( m_param1 )
				&& in_rSerializer.Put( m_param2 );
		}

		inline bool Deserialize( CommandDataSerializer& in_rSerializer )
		{
			return ObjectProxyCommandData::CommandData::Deserialize( in_rSerializer )
				&& in_rSerializer.Get( m_param1 )
				&& in_rSerializer.Get( m_param2 );
		}

		PARAM1 m_param1;
		PARAM2 m_param2;
	};

	template< AkUInt32 METHOD_ID, class PARAM1, class PARAM2, class PARAM3 >
	struct CommandData3 : public CommandData
	{
		inline CommandData3() : ObjectProxyCommandData::CommandData( METHOD_ID ) {}

		inline CommandData3( IObjectProxy * in_pProxy, const PARAM1 & in_param1, const PARAM2 & in_param2, const PARAM3 & in_param3 ) 
			: ObjectProxyCommandData::CommandData( METHOD_ID )
			, m_param1( in_param1 )
			, m_param2( in_param2 )
			, m_param3( in_param3 )
		{
			m_proxyInstancePtr = (AkUInt64) in_pProxy;
			m_objectID = in_pProxy->GetID();
		}

		inline bool Serialize( CommandDataSerializer& in_rSerializer ) const
		{
			return ObjectProxyCommandData::CommandData::Serialize( in_rSerializer )
				&& in_rSerializer.Put( m_param1 )
				&& in_rSerializer.Put( m_param2 )
				&& in_rSerializer.Put( m_param3 );
		}

		inline bool Deserialize( CommandDataSerializer& in_rSerializer )
		{
			return ObjectProxyCommandData::CommandData::Deserialize( in_rSerializer )
				&& in_rSerializer.Get( m_param1 )
				&& in_rSerializer.Get( m_param2 )
				&& in_rSerializer.Get( m_param3 );
		}

		PARAM1 m_param1;
		PARAM2 m_param2;
		PARAM3 m_param3;
	};

	template< AkUInt32 METHOD_ID, class PARAM1, class PARAM2, class PARAM3, class PARAM4 >
	struct CommandData4 : public CommandData
	{
		inline CommandData4() : ObjectProxyCommandData::CommandData( METHOD_ID ) {}

		inline CommandData4( IObjectProxy * in_pProxy, const PARAM1 & in_param1, const PARAM2 & in_param2, const PARAM3 & in_param3, const PARAM4 & in_param4 ) 
			: ObjectProxyCommandData::CommandData( METHOD_ID )
			, m_param1( in_param1 )
			, m_param2( in_param2 )
			, m_param3( in_param3 )
			, m_param4( in_param4 )
		{
			m_proxyInstancePtr = (AkUInt64) in_pProxy;
			m_objectID = in_pProxy->GetID();
		}

		inline bool Serialize( CommandDataSerializer& in_rSerializer ) const
		{
			return ObjectProxyCommandData::CommandData::Serialize( in_rSerializer )
				&& in_rSerializer.Put( m_param1 )
				&& in_rSerializer.Put( m_param2 )
				&& in_rSerializer.Put( m_param3 )
				&& in_rSerializer.Put( m_param4 );
		}

		inline bool Deserialize( CommandDataSerializer& in_rSerializer )
		{
			return ObjectProxyCommandData::CommandData::Deserialize( in_rSerializer )
				&& in_rSerializer.Get( m_param1 )
				&& in_rSerializer.Get( m_param2 )
				&& in_rSerializer.Get( m_param3 )
				&& in_rSerializer.Get( m_param4 );
		}

		PARAM1 m_param1;
		PARAM2 m_param2;
		PARAM3 m_param3;
		PARAM4 m_param4;
	};

	template< AkUInt32 METHOD_ID, class PARAM1, class PARAM2, class PARAM3, class PARAM4, class PARAM5 >
	struct CommandData5 : public CommandData
	{
		inline CommandData5() : ObjectProxyCommandData::CommandData( METHOD_ID ) {}

		inline CommandData5( IObjectProxy * in_pProxy, const PARAM1 & in_param1, const PARAM2 & in_param2, const PARAM3 & in_param3, const PARAM4 & in_param4, const PARAM5 & in_param5 ) 
			: ObjectProxyCommandData::CommandData( METHOD_ID )
			, m_param1( in_param1 )
			, m_param2( in_param2 )
			, m_param3( in_param3 )
			, m_param4( in_param4 )
			, m_param5( in_param5 )
		{
			m_proxyInstancePtr = (AkUInt64) in_pProxy;
			m_objectID = in_pProxy->GetID();
		}

		inline bool Serialize( CommandDataSerializer& in_rSerializer ) const
		{
			return ObjectProxyCommandData::CommandData::Serialize( in_rSerializer )
				&& in_rSerializer.Put( m_param1 )
				&& in_rSerializer.Put( m_param2 )
				&& in_rSerializer.Put( m_param3 )
				&& in_rSerializer.Put( m_param4 )
				&& in_rSerializer.Put( m_param5 );
		}

		inline bool Deserialize( CommandDataSerializer& in_rSerializer )
		{
			return ObjectProxyCommandData::CommandData::Deserialize( in_rSerializer )
				&& in_rSerializer.Get( m_param1 )
				&& in_rSerializer.Get( m_param2 )
				&& in_rSerializer.Get( m_param3 )
				&& in_rSerializer.Get( m_param4 )
				&& in_rSerializer.Get( m_param5 );
		}

		PARAM1 m_param1;
		PARAM2 m_param2;
		PARAM3 m_param3;
		PARAM4 m_param4;
		PARAM5 m_param5;
	};

	template< AkUInt32 METHOD_ID, class PARAM1, class PARAM2, class PARAM3, class PARAM4, class PARAM5, class PARAM6 >
	struct CommandData6 : public CommandData
	{
		inline CommandData6() : ObjectProxyCommandData::CommandData( METHOD_ID ) {}

		inline CommandData6( IObjectProxy * in_pProxy, const PARAM1 & in_param1, const PARAM2 & in_param2, const PARAM3 & in_param3, const PARAM4 & in_param4, const PARAM5 & in_param5, const PARAM6 & in_param6 ) 
			: ObjectProxyCommandData::CommandData( METHOD_ID )
			, m_param1( in_param1 )
			, m_param2( in_param2 )
			, m_param3( in_param3 )
			, m_param4( in_param4 )
			, m_param5( in_param5 )
			, m_param6( in_param6 )
		{
			m_proxyInstancePtr = (AkUInt64) in_pProxy;
			m_objectID = in_pProxy->GetID();
		}

		inline bool Serialize( CommandDataSerializer& in_rSerializer ) const
		{
			return ObjectProxyCommandData::CommandData::Serialize( in_rSerializer )
				&& in_rSerializer.Put( m_param1 )
				&& in_rSerializer.Put( m_param2 )
				&& in_rSerializer.Put( m_param3 )
				&& in_rSerializer.Put( m_param4 )
				&& in_rSerializer.Put( m_param5 )
				&& in_rSerializer.Put( m_param6 );
		}

		inline bool Deserialize( CommandDataSerializer& in_rSerializer )
		{
			return ObjectProxyCommandData::CommandData::Deserialize( in_rSerializer )
				&& in_rSerializer.Get( m_param1 )
				&& in_rSerializer.Get( m_param2 )
				&& in_rSerializer.Get( m_param3 )
				&& in_rSerializer.Get( m_param4 )
				&& in_rSerializer.Get( m_param5 )
				&& in_rSerializer.Get( m_param6 );
		}

		PARAM1 m_param1;
		PARAM2 m_param2;
		PARAM3 m_param3;
		PARAM4 m_param4;
		PARAM5 m_param5;
		PARAM6 m_param6;
	};
}

#endif // #ifndef AK_OPTIMIZED
