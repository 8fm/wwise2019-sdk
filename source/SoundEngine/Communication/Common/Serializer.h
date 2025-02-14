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

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/Tools/Common/AkBankReadHelpers.h>
#include <AK/IBytes.h>
#include <math.h>

namespace AK
{
	struct SwapPolicy
	{
		SwapPolicy(bool in_bSwapEndian)
			: m_bSwapEndian(in_bSwapEndian)
		{}

		AkInt16 Swap(const AkInt16& in_rValue) const;
		AkUInt16 Swap(const AkUInt16& in_rValue) const;
		AkInt32 Swap(const AkInt32& in_rValue) const;
		AkUInt32 Swap(const AkUInt32& in_rValue) const;
		AkInt64 Swap(const AkInt64& in_rValue) const;
		AkUInt64 Swap(const AkUInt64& in_rValue) const;
		AkReal32 Swap(const AkReal32& in_rValue) const;
		bool IsSwapping() const {
			return m_bSwapEndian;
		}

		const bool m_bSwapEndian;
	};

	struct NoSwapPolicy
	{
		NoSwapPolicy(bool in_bSwapEndian)
{
			AKASSERT(!in_bSwapEndian);
		}

		inline AkInt16 Swap(const AkInt16 in_rValue) const { return in_rValue; }
		inline AkUInt16 Swap(const AkUInt16 in_rValue) const { return in_rValue; }
		inline AkInt32 Swap(const AkInt32 in_rValue) const { return in_rValue; }
		inline AkUInt32 Swap(const AkUInt32 in_rValue) const { return in_rValue; }
		inline AkInt64 Swap(const AkInt64 in_rValue) const { return in_rValue; }
		inline AkUInt64 Swap(const AkUInt64 in_rValue) const { return in_rValue; }
		inline AkReal32 Swap(const AkReal32 in_rValue) const { return in_rValue; }
		bool IsSwapping() const {
			return false;
		}
	};

	template<class TSwapPolicy = SwapPolicy>
	class Deserializer
		: public TSwapPolicy
	{
	public:
		Deserializer(bool in_bSwapEndian)
			: TSwapPolicy(in_bSwapEndian)
			, m_pReadBytes(NULL)
			, m_readPos(0)
		{
		}

		inline void Deserializing(const AkUInt8* in_pData)
		{
			m_pReadBytes = in_pData;
			m_readPos = 0;
		}
	inline const AkUInt8* GetReadBytes() const { return m_pReadBytes + m_readPos; }
	inline AkInt32 GetReadSize() const { return m_readPos; }
	inline bool HasData() const { return m_pReadBytes != NULL; }

	inline void Reset()
	{
		m_pReadBytes = NULL;
		m_readPos = 0;
	}

	inline void Skip(AkInt32 in_cBytes) { m_readPos += in_cBytes; }
		template <typename T>
		inline bool GetSimple(T& out_rValue)
		{
			out_rValue = ReadUnaligned<T>(m_pReadBytes + m_readPos);
			m_readPos += sizeof(T);

			return true;
		}

		template <typename T>
		inline bool GetSwap(T& out_rValue)
		{
			out_rValue = this->Swap(AK::ReadUnaligned<T>(m_pReadBytes + m_readPos));
			m_readPos += sizeof(T);

			return true;
		}		

		inline bool Get(AkInt8& out_rValue) { return GetSimple(out_rValue); }
		inline bool Get(AkUInt8& out_rValue) { return GetSimple(out_rValue); }
		inline bool Get(AkInt16& out_rValue) { return GetSwap(out_rValue); }
		inline bool Get(AkUInt16& out_rValue) { return GetSwap(out_rValue); }
		inline bool Get(AkInt32& out_rValue) { return GetSwap(out_rValue); }
		inline bool Get(AkUInt32& out_rValue) { return GetSwap(out_rValue); }
		inline bool Get(AkInt64& out_rValue) { return GetSwap(out_rValue); }
		inline bool Get(AkUInt64& out_rValue) { return GetSwap(out_rValue); }
		inline bool Get(AkReal32& out_rValue) { return GetSwap(out_rValue); }

		inline bool Get(bool& out_rValue)
		{
			AkUInt8 bValue;
			if (!GetSimple(bValue))
				return false;

			out_rValue = bValue != 0;
			return true;
		}

		bool GetFloat24(AkReal32& out_rValue)
		{
			AKASSERT(!this->IsSwapping());	//Not implemented.
			AkUInt8* p = (AkUInt8*)(&out_rValue);
			p[0] = 0;	//The low byte was dropped. Just assume 0.
			p[1] = m_pReadBytes[m_readPos];
			p[2] = m_pReadBytes[m_readPos + 1];
			p[3] = m_pReadBytes[m_readPos + 2];			
			m_readPos += 3;
			return true;
		}

		AkNoInline bool GetPacked(AkUInt32& out_value) { return _GetPackedUInt<AkUInt32>(out_value); }	

		bool Get(void*& out_rpData, AkInt32& out_rSize)
		{
			bool bRet = false;

			if (Get(out_rSize))
			{
				if (out_rSize)
				{
					out_rpData = (void*)(m_pReadBytes + m_readPos);

					m_readPos += out_rSize;
				}
				else
				{
					out_rpData = NULL;
				}

				bRet = true;
			}

			return bRet;
		}

		bool Get(char*& out_rpszData, AkInt32& out_rSize) { return Get((void*&)out_rpszData, out_rSize); }

		template <typename T>
		bool _GetPackedUInt(T& out_uUnpacked)
		{
			out_uUnpacked = 0;

			AkUInt32 idx = 0;
			bool bDone = false;

			do
			{
				AkUInt8 byTemp = m_pReadBytes[m_readPos + idx];

				out_uUnpacked <<= 7;
				out_uUnpacked |= (AkUInt32)(byTemp & 0x7f);

				++idx;

				if ((byTemp & 0x80) == 0x0)
					bDone = true;
			} while (idx < (2 * sizeof(T)) && !bDone);

			AKASSERT(bDone);
			m_readPos += idx;

			return bDone;
		}

		class AutoSetDataPeeking
		{
		public:
			inline AutoSetDataPeeking(Deserializer& in_rSerializer)
				: m_rSerializer(in_rSerializer)
				, m_readPosBeforePeeking(in_rSerializer.m_readPos)
			{
			}

			inline ~AutoSetDataPeeking()
			{
				m_rSerializer.m_readPos = m_readPosBeforePeeking;
			}

			Deserializer & m_rSerializer;
			AkUInt32 m_readPosBeforePeeking;
		};

	private:
		friend class AutoSetDataPeeking;

		const AkUInt8* m_pReadBytes;
		AkUInt32 m_readPos;
	};


	template<class TWriter, class TSwapPolicy>
	class Serializer
		: public TWriter
		, public TSwapPolicy
	{
	public:
		Serializer(bool in_bSwapEndian)
			: TWriter()
			, TSwapPolicy(in_bSwapEndian)
		{
		}

		~Serializer()
		{
			this->Clear();
		}

		// Basic, known size types.
		AkNoInline bool Put(AkInt8 in_value) { return this->Write(in_value); }
		AkNoInline bool Put(AkUInt8 in_value) { return this->Write(in_value); }
		AkNoInline bool Put(AkInt16 in_value) { return this->Write(this->Swap(in_value)); }
		AkNoInline bool Put(AkInt32 in_value) { return this->Write(this->Swap(in_value)); }
		AkNoInline bool Put(AkUInt16 in_value) { return this->Write(this->Swap(in_value)); }
		AkNoInline bool Put(AkUInt32 in_value) { return this->Write(this->Swap(in_value)); }
		AkNoInline bool Put(AkInt64 in_value) { return this->Write(this->Swap(in_value)); }
		AkNoInline bool Put(AkUInt64 in_value) { return this->Write(this->Swap(in_value)); }
		AkNoInline bool Put(AkReal32 in_value) { return this->Write(this->Swap(in_value)); }

		bool Put(bool in_value) { return Put((AkUInt8)in_value); }

		//Specialization for floats.  In general we don't need the full precision so drop the last byte.
		inline bool PutFloat24(const AkReal32 val)
		{
#if defined(AK_ENDIANNESS_LITTLE)
			AkUInt8 *p = ((AkUInt8*)&val);
			this->Write(p[1]);
			this->Write(p[2]);
			this->Write(p[3]);			
#else
#error Big Endian not supported yet
#endif
			return true;
		}

		// Packed into variable length byte arrays
		AkNoInline bool PutPacked(AkUInt32 in_value) { return _PutPackedUInt<AkUInt32>(in_value); }
		
		// Variable length types
		bool Put(const void* in_pvData, AkInt32 in_size)
		{
			AkInt32 lDummy = 0;

			return Put(in_size)
				&& this->WriteBytes(in_pvData, in_size, lDummy);
		}

		inline bool Put(const char* in_pszData)
		{
			return Put((void*)in_pszData, in_pszData != NULL ? (AkInt32)(::strlen(in_pszData) + 1) : 0);
		}

// Pack AkUIntX value into variable length byte array.
		template <typename T>
		inline bool _PutPackedUInt(T in_uUnpacked)
		{
			AkUInt8 abyPacked[2 * sizeof(T)];
			AkUInt8 byTemp;
			AkInt32 idx = 2 * sizeof(T) - 1;
			AkUInt8 uFlag = 0x0;

			do
			{
				byTemp = (AkUInt8)(in_uUnpacked & 0x7f);
				abyPacked[idx] = byTemp | uFlag;

				in_uUnpacked >>= 7;
				uFlag = 0x80;
				--idx;
			} while (in_uUnpacked != 0 && idx >= 0);

			++idx;

			AKASSERT(in_uUnpacked == 0);

			AkInt32 iDummy;
			AkInt32 iSize = (2 * sizeof(T)) - idx;

			return WriteBytes(&abyPacked[idx], iSize, iDummy);
		}
	};
}
