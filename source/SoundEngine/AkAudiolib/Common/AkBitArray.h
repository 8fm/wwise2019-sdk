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
// AkBitArray.h
//
//////////////////////////////////////////////////////////////////////

#ifndef _AK_BIT_ARRAY_H_
#define _AK_BIT_ARRAY_H_

#include <AK/Tools/Common/AkAssert.h>
#include <memory.h>

template <class T, int SHIFT=0 >
class CAkBitArray 
{
public:
	typedef T FieldType;
	enum { WIDTH = sizeof(T)*8 };

	CAkBitArray()
		: m_iBitArray(0)
	{
	}

	CAkBitArray(AkUInt64 initialVal)
		: m_iBitArray((T)(initialVal >> SHIFT))
	{
	}

	inline void EnsureValidIndex(
#ifdef AK_ENABLE_ASSERTS
		T in_BitIndex
#else
		T //in_BitIndex // Preventing unused warning.
#endif
		) const
	{
#ifdef AK_ENABLE_ASSERTS
		// When SHIFT is set to the default value (0), most compilers warns of the risky useless compare >= between an unsigned int and 0.
		// Force casting to a signed integer should prevent the Insignificant warning.
		AKASSERT(((int)in_BitIndex) >= SHIFT);
#endif
	}

	template<typename RHS_T, int RHS_SHIFT >
	CAkBitArray(CAkBitArray<RHS_T, RHS_SHIFT> initialVal)
		: m_iBitArray((T)initialVal.m_iBitArray << (RHS_SHIFT - SHIFT))
	{
		//AKSTATICASSERT((RHS_SHIFT) >= (SHIFT), "RHS will not fit into LHS.");
	}

	void SetBit( T in_BitIndex )
	{
		EnsureValidIndex(in_BitIndex);
		in_BitIndex -= SHIFT;
		m_iBitArray |= (1ULL << in_BitIndex);
	}

	void UnsetBit( T in_BitIndex )
	{
		EnsureValidIndex(in_BitIndex);
		in_BitIndex -= SHIFT;
		m_iBitArray &= (~(1ULL << in_BitIndex));
	}

	void Set( T in_BitIndex, bool in_value )
	{
		if( in_value )
			SetBit( in_BitIndex );
		else
			UnsetBit( in_BitIndex );
	}

	void Mask(AkUInt64 in_BitMask, bool in_setOrClear)
	{
		in_BitMask = in_BitMask >> SHIFT;
		if (in_setOrClear)
			m_iBitArray = m_iBitArray | ((T)in_BitMask);
		else
			m_iBitArray = m_iBitArray & ~((T)in_BitMask);
	}

	bool IsSet( T in_BitIndex ) const
	{
		EnsureValidIndex(in_BitIndex);
		in_BitIndex -= SHIFT;
		return ( m_iBitArray & (1ULL << in_BitIndex) )?true:false;
	}

	bool IsEmpty() const 
	{
		return (m_iBitArray == 0);
	}

	void ClearBits()
	{
		m_iBitArray = 0;
	}

	CAkBitArray<T, SHIFT>& operator=(CAkBitArray< T, SHIFT> in_rhs)
	{
		m_iBitArray = in_rhs.m_iBitArray;
		return *this;
	}

	template<typename RHS_T, int RHS_SHIFT >
	CAkBitArray<T, SHIFT>& operator=(CAkBitArray<RHS_T, RHS_SHIFT> in_rhs)
	{
		//AKSTATICASSERT(RHS_SHIFT >= SHIFT, "RHS will not fit into LHS.");
		m_iBitArray = ((T)in_rhs.m_iBitArray << (RHS_SHIFT - SHIFT));
		return *this;
	}

	CAkBitArray<T, SHIFT> operator&(const CAkBitArray<T, SHIFT>& in_rhs) const
	{
		CAkBitArray<T, SHIFT> res;
		res.m_iBitArray = (this->m_iBitArray & in_rhs.m_iBitArray);
		return res;
	}

	void operator&=(const CAkBitArray<T, SHIFT>& in_rhs)
	{
		m_iBitArray = (this->m_iBitArray & in_rhs.m_iBitArray);
	}

	template<typename RHS_T, int RHS_SHIFT >
	CAkBitArray<T, SHIFT> operator&(const CAkBitArray<RHS_T, RHS_SHIFT>& in_rhs) const
	{
		//AKSTATICASSERT(RHS_SHIFT >= SHIFT, "RHS will not fit into LHS.");
		CAkBitArray<T, SHIFT> res;
		res.m_iBitArray = (this->m_iBitArray & (in_rhs.m_iBitArray >> (RHS_SHIFT - SHIFT)));
		return res;
	}

	template<typename RHS_T, int RHS_SHIFT >
	void operator&=(const CAkBitArray<RHS_T, RHS_SHIFT>& in_rhs)
	{
		//AKSTATICASSERT(RHS_SHIFT >= SHIFT, "RHS will not fit into LHS.");
		m_iBitArray = (this->m_iBitArray & ((T)in_rhs.m_iBitArray >> (RHS_SHIFT - SHIFT)));
	}

	CAkBitArray<T, SHIFT> operator|(const CAkBitArray<T, SHIFT>& in_rhs) const
	{
		CAkBitArray<T, SHIFT> res;
		res.m_iBitArray = (this->m_iBitArray | in_rhs.m_iBitArray);
		return res;
	}

	template<typename RHS_T, int RHS_SHIFT >
	void operator|=(const CAkBitArray<RHS_T, RHS_SHIFT>& in_rhs)
	{
		//AKSTATICASSERT(RHS_SHIFT >= SHIFT, "RHS will not fit into LHS.");
		m_iBitArray = (this->m_iBitArray | ((T)in_rhs.m_iBitArray << (RHS_SHIFT - SHIFT)));
	}

	template<typename RHS_T, int RHS_SHIFT >
	CAkBitArray<T, SHIFT> operator|(const CAkBitArray<RHS_T, RHS_SHIFT>& in_rhs) const
	{
		//AKSTATICASSERT(RHS_SHIFT >= SHIFT, "RHS will not fit into LHS.");
		CAkBitArray<T, SHIFT> res;
		res.m_iBitArray = (this->m_iBitArray | ((T)in_rhs.m_iBitArray << (RHS_SHIFT - SHIFT)));
		return res;
	}

	void operator|=(const CAkBitArray<T, SHIFT>& in_rhs)
	{
		m_iBitArray = (this->m_iBitArray | in_rhs.m_iBitArray);
	}
	
	CAkBitArray<T, SHIFT> operator~() const
	{
		CAkBitArray<T, SHIFT> res;
		res.m_iBitArray = ~(m_iBitArray);
		return res;
	}

	bool operator!=(const CAkBitArray<T, SHIFT>& in_lhs) const
	{
		return (m_iBitArray != in_lhs.m_iBitArray);
	}

	bool operator==(const CAkBitArray<T, SHIFT>& in_lhs) const
	{
		return (m_iBitArray == in_lhs.m_iBitArray);
	}

	T m_iBitArray;
};

#define BITS_PER_ELEMENT 32
#define ROUND_TO_ELEMENT(__BITS) ((__BITS - 1 + BITS_PER_ELEMENT) / BITS_PER_ELEMENT)
template <AkUInt32 FIXEDSIZE>
class CAkLargeBitArray
{
public:	

	CAkLargeBitArray()
	{
		memset(&m_iBitArray, 0, BaseElements() * sizeof(AkUInt32));
	}	

	inline void EnsureValidIndex(
#ifdef AK_ENABLE_ASSERTS
		AkUInt32 in_BitIndex
#else
		AkUInt32 //in_BitIndex // Preventing unused warning.
#endif
	) const
	{
#ifdef AK_ENABLE_ASSERTS
		// When SHIFT is set to the default value (0), most compilers warns of the risky useless compare >= between an unsigned int and 0.
		// Force casting to a signed integer should prevent the Insignificant warning.
		AKASSERT(((int)in_BitIndex) >= 0 && in_BitIndex < FIXEDSIZE);
#endif
	}	

	inline void SetBit(AkUInt32 in_BitIndex)
	{
		EnsureValidIndex(in_BitIndex);		
		AkUInt32 idx = in_BitIndex / BITS_PER_ELEMENT;
		m_iBitArray[idx] |= (1ULL << (in_BitIndex - idx*BITS_PER_ELEMENT));
	}

	inline void UnsetBit(AkUInt32 in_BitIndex)
	{
		EnsureValidIndex(in_BitIndex);		
		AkUInt32 idx = in_BitIndex / BITS_PER_ELEMENT;
		m_iBitArray[idx] &= ~(1ULL << (in_BitIndex - idx * BITS_PER_ELEMENT));
	}

	inline void Set(AkUInt32 in_BitIndex, bool in_value)
	{
		if (in_value)
			SetBit(in_BitIndex);
		else
			UnsetBit(in_BitIndex);
	}

	inline bool IsSet(AkUInt32 in_BitIndex) const
	{
		EnsureValidIndex(in_BitIndex);	
		AkUInt32 idx = in_BitIndex / BITS_PER_ELEMENT;
		return (m_iBitArray[idx] & (1ULL << (in_BitIndex - idx * BITS_PER_ELEMENT))) != 0;
	}

	inline bool IsEmpty() const
	{
		AkUInt32 uEmpty = 0;
		for (AkUInt32 i = 0; i < BaseElements(); i++)
			uEmpty |= m_iBitArray[i];
		return uEmpty != 0;
	}

	inline void ClearBits()
	{
		for (AkUInt32 i = 0; i < BaseElements(); i++)
			m_iBitArray[i] = 0;		
	}

	inline void SetAll()
	{
		for (AkUInt32 i = 0; i < BaseElements(); i++)
			m_iBitArray[i] = 0xFFFFFFFF;
		m_iBitArray[BaseElements() - 1] &= (1 << (FIXEDSIZE - (BaseElements() - 1) * 32))-1;
	}
	
	static inline AkUInt32 BaseElements() { return ROUND_TO_ELEMENT(FIXEDSIZE); }

	AkUInt32 m_iBitArray[ROUND_TO_ELEMENT(FIXEDSIZE)];
};


#endif //_AK_BIT_ARRAY_H_
