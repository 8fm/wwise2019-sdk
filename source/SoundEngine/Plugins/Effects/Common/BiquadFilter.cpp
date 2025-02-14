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

// Direct form biquad filter y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]

public:

#if defined AKSIMD_V4F32_SUPPORTED		
	struct OutOfPlaceOutputPolicy
	{
		AkForceInline void WriteSIMD(AkReal32* pIn, const AkUInt32 in_offset, AKSIMD_V4F32 &v) { AKSIMD_STORE_V4F32(pOut+in_offset, v); }
		AkForceInline void AdvanceSIMD( AkReal32* AK_RESTRICT &pIn ) { pIn += 4; pOut += 4; }
		AkForceInline void FinalAdvance(AkReal32* AK_RESTRICT &pIn, const AkUInt32 in_offset) { pIn += in_offset; pOut += in_offset; }
		AkReal32* pOut;
	};

	struct InPlaceOutputPolicy
	{
		AkForceInline void WriteSIMD(AkReal32* pIn, const AkUInt32 in_offset, AKSIMD_V4F32 &v) { AKSIMD_STORE_V4F32(pIn+in_offset, v); }
		AkForceInline void AdvanceSIMD( AkReal32* AK_RESTRICT &pIn ) { pIn += 4; }
		AkForceInline void FinalAdvance(AkReal32* AK_RESTRICT &pIn, const AkUInt32 in_offset) { pIn += in_offset; }
	};

	AkForceInline void ProcessBuffer(AkReal32 * io_pBuffer, AkUInt32 in_uNumframes, AkUInt32 in_iChannel = 0)
	{
		InPlaceOutputPolicy irrelevent;
		ProcessBufferAll<InPlaceOutputPolicy>(io_pBuffer, in_uNumframes, in_iChannel, irrelevent);
	}

	AkForceInline void ProcessBuffer(AkReal32 * in_pBuffer, AkUInt32 in_uNumframes, AkUInt32 in_iChannel, AkReal32 * out_pBuffer)
	{
		OutOfPlaceOutputPolicy outputPtr = { out_pBuffer };
		ProcessBufferAll<OutOfPlaceOutputPolicy>(in_pBuffer, in_uNumframes, in_iChannel, outputPtr);
	}

	//From mdct_SIMD...  Should move to AkSIMD.
#if defined AK_CPU_ARM_NEON
	AkForceInline void SIMDRotate(AKSIMD_V4F32 &A, AKSIMD_V4F32 &B, AKSIMD_V4F32 &C, AKSIMD_V4F32 &D)
	{
		float32x4x2_t vfTemp0 = vtrnq_f32(A, B);
		float32x4x2_t vfTemp1 = vtrnq_f32(C, D);

		A = AKSIMD_SHUFFLE_V4F32(vfTemp0.val[0], vfTemp1.val[0], AKSIMD_SHUFFLE(1, 0, 1, 0));
		B = AKSIMD_SHUFFLE_V4F32(vfTemp0.val[1], vfTemp1.val[1], AKSIMD_SHUFFLE(1, 0, 1, 0));
		C = AKSIMD_SHUFFLE_V4F32(vfTemp0.val[0], vfTemp1.val[0], AKSIMD_SHUFFLE(3, 2, 3, 2));
		D = AKSIMD_SHUFFLE_V4F32(vfTemp0.val[1], vfTemp1.val[1], AKSIMD_SHUFFLE(3, 2, 3, 2));
	}
#else //#if defined( AK_CPU_X86 ) || defined( AK_CPU_X86_64 )
	AkForceInline void SIMDRotate(AKSIMD_V4F32 &A, AKSIMD_V4F32 &B, AKSIMD_V4F32 &C, AKSIMD_V4F32 &D)
	{
		AKSIMD_V4F32 tmp4, tmp5, tmp6, tmp7;		
		tmp4 = AKSIMD_MOVELH_V4F32(A, B);
		tmp5 = AKSIMD_MOVEHL_V4F32(B, A);
		tmp6 = AKSIMD_MOVELH_V4F32(C, D);
		tmp7 = AKSIMD_MOVEHL_V4F32(D, C);

		A = AKSIMD_SHUFFLE_V4F32(tmp4, tmp6, AKSIMD_SHUFFLE(2, 0, 2, 0));
		B = AKSIMD_SHUFFLE_V4F32(tmp4, tmp6, AKSIMD_SHUFFLE(3, 1, 3, 1));
		C = AKSIMD_SHUFFLE_V4F32(tmp5, tmp7, AKSIMD_SHUFFLE(2, 0, 2, 0));
		D = AKSIMD_SHUFFLE_V4F32(tmp5, tmp7, AKSIMD_SHUFFLE(3, 1, 3, 1));
	}
#endif
	
	//Processes 4 channels at a time. Must be pre-aligned. 
	template<class OUTPUT_POLICY>
	AkForceInline void ProcessBuffer4(AkReal32 *& AK_RESTRICT io_pBuffer, const AkUInt32 in_uNumframes, const AkUInt32 in_uChannelStride, AKSIMD_V4F32 &vfFFwd1, AKSIMD_V4F32 &vfFFwd2, AKSIMD_V4F32 &fFFbk1, AKSIMD_V4F32 &fFFbk2, OUTPUT_POLICY& in_Output)
	{				
		const AkReal32 * pfEnd = io_pBuffer + in_uNumframes;		
		
		AkReal32 *pCoefs = m_Memories.GetQuadCoefPtr();
		const AKSIMD_V4F32 B0 = AKSIMD_SET_V4F32(pCoefs[0]); //fB0
		const AKSIMD_V4F32 B1 = AKSIMD_SET_V4F32(pCoefs[1]); //fB1
		const AKSIMD_V4F32 B2 = AKSIMD_SET_V4F32(pCoefs[2]); //fB2
		const AKSIMD_V4F32 A1 = AKSIMD_SET_V4F32(pCoefs[3]); //fA1
		const AKSIMD_V4F32 A2 = AKSIMD_SET_V4F32(pCoefs[4]); //fA2
				
		while(io_pBuffer < pfEnd)
		{
			//Load one block from each channel and transpose			
			AKSIMD_V4F32 A = AKSIMD_LOAD_V4F32(io_pBuffer);
			AKSIMD_V4F32 B = AKSIMD_LOAD_V4F32(io_pBuffer + in_uChannelStride);
			AKSIMD_V4F32 C = AKSIMD_LOAD_V4F32(io_pBuffer + 2 * in_uChannelStride);
			AKSIMD_V4F32 D = AKSIMD_LOAD_V4F32(io_pBuffer + 3 * in_uChannelStride);
			SIMDRotate(A, B, C, D);
			
			AKSIMD_V4F32 Y0 = AKSIMD_MUL_V4F32(A, B0);
			Y0 = AKSIMD_MADD_V4F32(vfFFwd1, B1, Y0);
			Y0 = AKSIMD_MADD_V4F32(vfFFwd2, B2, Y0);
			Y0 = AKSIMD_MADD_V4F32(fFFbk1, A1, Y0);
			Y0 = AKSIMD_MADD_V4F32(fFFbk2, A2, Y0);			

			AKSIMD_V4F32 Y1 = AKSIMD_MUL_V4F32(B, B0);
			Y1 = AKSIMD_MADD_V4F32(A, B1, Y1);
			Y1 = AKSIMD_MADD_V4F32(vfFFwd1, B2, Y1);
			Y1 = AKSIMD_MADD_V4F32(Y0, A1, Y1);
			Y1 = AKSIMD_MADD_V4F32(fFFbk1, A2, Y1);

			AKSIMD_V4F32 Y2 = AKSIMD_MUL_V4F32(C, B0);
			Y2 = AKSIMD_MADD_V4F32(B, B1, Y2);
			Y2 = AKSIMD_MADD_V4F32(A, B2, Y2);
			Y2 = AKSIMD_MADD_V4F32(Y1, A1, Y2);
			Y2 = AKSIMD_MADD_V4F32(Y0, A2, Y2);

			AKSIMD_V4F32 Y3 = AKSIMD_MUL_V4F32(D, B0);
			Y3 = AKSIMD_MADD_V4F32(C, B1, Y3);
			Y3 = AKSIMD_MADD_V4F32(B, B2, Y3);
			Y3 = AKSIMD_MADD_V4F32(Y2, A1, Y3);
			Y3 = AKSIMD_MADD_V4F32(Y1, A2, Y3);

			vfFFwd1 = D;
			vfFFwd2 = C;
			fFFbk1 = Y3;
			fFFbk2 = Y2;

			SIMDRotate(Y0, Y1, Y2, Y3);

			in_Output.WriteSIMD(io_pBuffer, 0,  Y0);
			in_Output.WriteSIMD(io_pBuffer, in_uChannelStride, Y1);
			in_Output.WriteSIMD(io_pBuffer, 2 * in_uChannelStride, Y2);
			in_Output.WriteSIMD(io_pBuffer, 3 * in_uChannelStride, Y3);
			in_Output.AdvanceSIMD(io_pBuffer);
		}
		in_Output.FinalAdvance(io_pBuffer, 4 * in_uChannelStride - in_uNumframes);	//The pointer was normally advanced through the first channel.  Make it hop on the last.
	}

	template<class OUTPUT_POLICY>
	AkForceInline void ProcessBuffer2(AkReal32 *& AK_RESTRICT io_pBuffer, const AkUInt32 in_uNumframes, const AkUInt32 in_uChannelStride, AKSIMD_V4F32 &vfFFwd1, AKSIMD_V4F32 &vfFFwd2, AKSIMD_V4F32 &vfFFbk1, AKSIMD_V4F32 &vfFFbk2, OUTPUT_POLICY& in_Output)
	{
		/*
		y0a = 		   	x0*B0 			+ xm1*B1 			+ xm2*B2 	+ ym1*A1 			+ ym2*A2
		y1a = xp1*B0 + 	x0*(B1+B0*A1) 	+ xm1*(B2+B1*A1) 	+ xm2*B2*A1 + ym1*(A2+A1*A1)	+ ym2*A2*A1
		*/		
		const AkReal32 * pfEnd = io_pBuffer + in_uNumframes;	

		const AKSIMD_V4F32 *pCoefs = (AKSIMD_V4F32 *)m_Memories.GetStereoCoefPtr();
		const AKSIMD_V4F32 vFirst  = pCoefs[0];
		const AKSIMD_V4F32 vSecond = pCoefs[1];
		const AKSIMD_V4F32 vThird  = pCoefs[2];
		const AKSIMD_V4F32 vFourth = pCoefs[3];
		const AKSIMD_V4F32 vFifth  = pCoefs[4];
		const AKSIMD_V4F32 vSixth  = pCoefs[5];

		while(io_pBuffer < pfEnd)
		{							
			AKSIMD_V4F32 L = AKSIMD_LOAD_V4F32(io_pBuffer);
			AKSIMD_V4F32 R = AKSIMD_LOAD_V4F32(io_pBuffer + in_uChannelStride);
			AKSIMD_V4F32 X0X1 = AKSIMD_MOVELH_V4F32(L, R);	//L0L1R0R1
			AKSIMD_V4F32 X2X3 = AKSIMD_MOVEHL_V4F32(R, L);

			AKSIMD_V4F32 X0 = AKSIMD_DUP_EVEN(X0X1);				
			AKSIMD_V4F32 Y0Y1 = AKSIMD_MUL_V4F32(X0X1, vFirst);
			Y0Y1 = AKSIMD_MADD_V4F32(X0, vSecond, Y0Y1);
			Y0Y1 = AKSIMD_MADD_V4F32(vfFFwd1, vThird, Y0Y1);
			Y0Y1 = AKSIMD_MADD_V4F32(vfFFwd2, vFourth, Y0Y1);
			Y0Y1 = AKSIMD_MADD_V4F32(vfFFbk1, vFifth, Y0Y1);
			Y0Y1 = AKSIMD_MADD_V4F32(vfFFbk2, vSixth, Y0Y1);
			
			vfFFwd1 = AKSIMD_DUP_ODD(X0X1);
			vfFFbk2 = AKSIMD_DUP_EVEN(Y0Y1);
			vfFFbk1 = AKSIMD_DUP_ODD(Y0Y1);

			AKSIMD_V4F32 X2 = AKSIMD_DUP_EVEN(X2X3);
			AKSIMD_V4F32 Y2Y3 = AKSIMD_MUL_V4F32(X2X3, vFirst);		
			Y2Y3 = AKSIMD_MADD_V4F32(X2, vSecond, Y2Y3);
			Y2Y3 = AKSIMD_MADD_V4F32(vfFFwd1, vThird, Y2Y3);
			Y2Y3 = AKSIMD_MADD_V4F32(X0, vFourth, Y2Y3);
			Y2Y3 = AKSIMD_MADD_V4F32(vfFFbk1, vFifth, Y2Y3);
			Y2Y3 = AKSIMD_MADD_V4F32(vfFFbk2, vSixth, Y2Y3);
			
			vfFFwd2 = X2;
			vfFFwd1 = AKSIMD_DUP_ODD(X2X3);
			vfFFbk2 = AKSIMD_DUP_EVEN(Y2Y3);
			vfFFbk1 = AKSIMD_DUP_ODD(Y2Y3);

			L = AKSIMD_MOVELH_V4F32(Y0Y1, Y2Y3);
			R = AKSIMD_MOVEHL_V4F32(Y2Y3, Y0Y1);
			
			in_Output.WriteSIMD(io_pBuffer, 0,  L);
			in_Output.WriteSIMD(io_pBuffer, in_uChannelStride, R);
			in_Output.AdvanceSIMD(io_pBuffer);
		}
		in_Output.FinalAdvance(io_pBuffer, 2*in_uChannelStride- in_uNumframes);	//The pointer was normally advanced through the first channel.  Make it hop on the last.
	}

	template<class OUTPUT_POLICY>
	AkForceInline void ProcessBuffer1(AkReal32 *& AK_RESTRICT io_pBuffer, const AkUInt32 in_uNumframes, const AkUInt32 in_uChannelStride, AKSIMD_V4F32 &vfFFwd1, AKSIMD_V4F32 &vfFFwd2, AKSIMD_V4F32 &fFFbk1, AKSIMD_V4F32 &fFFbk2, OUTPUT_POLICY& in_Output)
	{				
		const AkReal32 * pfEnd = io_pBuffer + in_uNumframes;
				
		/*
		The following code is the implementation of the biquad filter in direct form:

		Y = x0*B0 + xm2*B2 + xm1*B1 + ym2*A2 + ym1*A1  (the "m" in var names stands for "minus".  "p" for plus.  So xm1 means the previous x, and yp1 means the next y.

		Since the output of Y+1 depends on Y+0, it follows that Y+1 depends on the same inputs as Y+0.  Given that, we can recursively replace Y+0 in the Y+1 formula:
		yp0 = xp0*B0 + xm2*B2 + xm1*B1 + ym2*A2 + ym1*A1
		yp1 = xp1*B0 + xm1*B2 + xp0*B1 + ym1*A2 + (xp0*B0 + xm2*B2 + xm1*B1 + ym2*A2 + ym1*A1)*A1	(Note the shifts in other inputs: xp0 became xp1, etc)

		Repeat the process for yp2 and yp3, regroup terms and simplify and you get:

				1		2				3						4										5											6						7							8
		yp0 = xp0*B0 																					+ xm1*B1 								  + xm2*B2 				  + ym1*A1 						+ ym2*A2
		yp1 = xp1*B0 										   + xp0(B1+B0A1) 							+ xm1(B2+B1A1) 							  + xm2*B2A1 			  + ym1(A1A1+A2) 				+ ym2*A2A1
		yp2 = xp2*B0 				+ xp1(B1+B0A1) 			   + xp0(B2+B1A1+B0A1A1+B0A2) 				+ xm1(B2A1+B1A1A1+B1A2) 				  + xm2(B2A1A1+B2A2) 	  + ym1(A1A1A1+A2A1+A1A2) 		+ ym2(A2A1A1+A2A2)
		yp3 = xp3*B0 + xp2(B1+A1B0) + xp1(B2+A1B1+A1A1B0+A2B0) + xp0(A1B2+A1A1B1+A1A1A1B0+2A1A2B0+A2B1) + xm1(A1A1B2+A1A1A1B1+A1A2B1+A2B2+A1A2B1) + xm2(A1A1A1B2+2A1A2B2) + ym1(A1A1A1A1+3A1A1A2+A2A2) 	+ ym2(2A1A2A2+A1A1A1A2)

		The constants are computed from the ordinary coefficients in SetCoefs
		*/
		const AKSIMD_V4F32 *pCoefs = (AKSIMD_V4F32*)m_Memories.GetMonoCoefPtr();
		const AKSIMD_V4F32 vFirst  = pCoefs[0];
		const AKSIMD_V4F32 vSecond = pCoefs[1];
		const AKSIMD_V4F32 vThird  = pCoefs[2];
		const AKSIMD_V4F32 vFourth = pCoefs[3];
		const AKSIMD_V4F32 vXPrev1 = pCoefs[4];
		const AKSIMD_V4F32 vXPrev2 = pCoefs[5];
		const AKSIMD_V4F32 vYPrev1 = pCoefs[6];
		const AKSIMD_V4F32 vYPrev2 = pCoefs[7];		

		while(io_pBuffer < pfEnd)
		{
			AKSIMD_V4F32 vC = AKSIMD_LOAD_V4F32(io_pBuffer);		//Ready in 2 cycles if in cache

			AKSIMD_V4F32 term1 = AKSIMD_MUL_V4F32(vC, vFirst);
			AKSIMD_V4F32 term5 = AKSIMD_MADD_V4F32(vfFFwd1, vXPrev1, term1);
			AKSIMD_V4F32 term6 = AKSIMD_MADD_V4F32(vfFFwd2, vXPrev2, term5);
			AKSIMD_V4F32 term7 = AKSIMD_MADD_V4F32(fFFbk1, vYPrev1, term6);
			AKSIMD_V4F32 term8 = AKSIMD_MADD_V4F32(fFFbk2, vYPrev2, term7);
			vfFFwd1 = AKSIMD_SPLAT_V4F32(vC, 3);
			vfFFwd2 = AKSIMD_SPLAT_V4F32(vC, 2);
			AKSIMD_V4F32 term2 = AKSIMD_MADD_V4F32(vfFFwd2, vSecond, term8);
			AKSIMD_V4F32 term3 = AKSIMD_MADD_V4F32(AKSIMD_SPLAT_V4F32(vC, 1), vThird, term2);
			AKSIMD_V4F32 vY = AKSIMD_MADD_V4F32(AKSIMD_SPLAT_V4F32(vC, 0), vFourth, term3);

			fFFbk1 = AKSIMD_SPLAT_V4F32(vY, 3);
			fFFbk2 = AKSIMD_SPLAT_V4F32(vY, 2);
			in_Output.WriteSIMD(io_pBuffer, 0, vY);
			in_Output.AdvanceSIMD(io_pBuffer);
		}
		in_Output.FinalAdvance(io_pBuffer, in_uChannelStride - in_uNumframes);	//The pointer was normally advanced through the first channel.  Make it hop on the last.
	}

	template<class OUTPUT_POLICY>
	AkForceInline void ProcessBufferAll(AkReal32 * io_pBuffer, AkUInt32 in_uNumframes, const AkUInt32 in_uChannelStride, OUTPUT_POLICY& in_Output)
	{		
		AKASSERT((((AkUIntPtr)io_pBuffer) & (AK_SIMD_ALIGNMENT - 1)) == 0);

		// Deal with unfriendly number of samples: the SIMD code works well with trailing zeros.
		// This class assumes that the buffer size is rounded up to the next SIMD friendly number).		
		AkUInt32 uNumUnaligned = (in_uNumframes & 0x3);
		if (uNumUnaligned)
		{
			AkUInt32 uNumTrail = 4 - uNumUnaligned;
			for (AkUInt32 ch = 0; ch < m_Memories.NumChannels(); ++ch)
			{
				AkReal32 * pPad = io_pBuffer + in_uNumframes + (ch*in_uChannelStride);
				for (AkUInt32 i = 0; i < uNumTrail; ++i)
				{
					*(pPad++) = 0.f;
				}
			}

			in_uNumframes += uNumTrail;
		}

		AkReal32 * pMems = m_Memories.GetRawMemories();
		AkUInt32 uRemainingChannels = m_Memories.NumChannels();

		AKSIMD_V4F32 vfFFwd1;
		AKSIMD_V4F32 vfFFwd2;
		AKSIMD_V4F32 vfFFbk1;
		AKSIMD_V4F32 vfFFbk2;

		// If we are to perform non-monoProcessing, then do so for as many channels as we should have available
		if (!m_Memories.MonoProcess())
		{
			while (uRemainingChannels >= 4)
			{
				vfFFwd1 = AKSIMD_LOAD_V4F32(pMems);
				vfFFwd2 = AKSIMD_LOAD_V4F32(pMems + 4);
				vfFFbk1 = AKSIMD_LOAD_V4F32(pMems + 8);
				vfFFbk2 = AKSIMD_LOAD_V4F32(pMems + 12);
				ProcessBuffer4<OUTPUT_POLICY>(io_pBuffer, in_uNumframes, in_uChannelStride, vfFFwd1, vfFFwd2, vfFFbk1, vfFFbk2, in_Output);

				AKSIMD_STORE_V4F32(pMems, vfFFwd1);
				AKSIMD_STORE_V4F32(pMems + 4, vfFFwd2);
				AKSIMD_STORE_V4F32(pMems + 8, vfFFbk1);
				AKSIMD_STORE_V4F32(pMems + 12, vfFFbk2);

				pMems += 16;				
				uRemainingChannels -= 4;
			}

			if (uRemainingChannels >= 2)
			{
				vfFFwd1 = AKSIMD_LOAD_V4F32(pMems);
				vfFFwd1 = AKSIMD_UNPACKLO_V4F32(vfFFwd1, vfFFwd1);
				vfFFwd2 = AKSIMD_LOAD_V4F32(pMems + 4);
				vfFFwd2 = AKSIMD_UNPACKLO_V4F32(vfFFwd2, vfFFwd2);
				vfFFbk1 = AKSIMD_LOAD_V4F32(pMems + 8);
				vfFFbk1 = AKSIMD_UNPACKLO_V4F32(vfFFbk1, vfFFbk1);
				vfFFbk2 = AKSIMD_LOAD_V4F32(pMems + 12);
				vfFFbk2 = AKSIMD_UNPACKLO_V4F32(vfFFbk2, vfFFbk2);
				ProcessBuffer2<OUTPUT_POLICY>(io_pBuffer, in_uNumframes, in_uChannelStride, vfFFwd1, vfFFwd2, vfFFbk1, vfFFbk2, in_Output);

				AKSIMD_STORE1_V4F32(pMems, vfFFwd1);
				AKSIMD_STORE1_V4F32((pMems + 4), vfFFwd2);
				AKSIMD_STORE1_V4F32((pMems + 8), vfFFbk1);
				AKSIMD_STORE1_V4F32((pMems + 12), vfFFbk2);
				AKSIMD_STORE1_V4F32((pMems + 1), AKSIMD_MOVEHL_V4F32(vfFFwd1, vfFFwd1));
				AKSIMD_STORE1_V4F32((pMems + 5), AKSIMD_MOVEHL_V4F32(vfFFwd2, vfFFwd2));
				AKSIMD_STORE1_V4F32((pMems + 9), AKSIMD_MOVEHL_V4F32(vfFFbk1, vfFFbk1));
				AKSIMD_STORE1_V4F32((pMems + 13), AKSIMD_MOVEHL_V4F32(vfFFbk2, vfFFbk2));

				pMems += 2;				
				uRemainingChannels -= 2;
			}
		}

		// Process the remaining monochannel, or if we only do monoProcessing then process all of the channels 
		while (uRemainingChannels >= 1)
		{
			AkUInt32 activeChannel = m_Memories.NumChannels() - uRemainingChannels;
			AkReal32 fFFwd1, fFFwd2, fFFbk1, fFFbk2;
			m_Memories.GetMemories(activeChannel, fFFwd1, fFFwd2, fFFbk1, fFFbk2);
			vfFFwd1 = AKSIMD_SET_V4F32(fFFwd1);
			vfFFwd2 = AKSIMD_SET_V4F32(fFFwd2);
			vfFFbk1 = AKSIMD_SET_V4F32(fFFbk1);
			vfFFbk2 = AKSIMD_SET_V4F32(fFFbk2);

			ProcessBuffer1<OUTPUT_POLICY>(io_pBuffer, in_uNumframes, in_uChannelStride, vfFFwd1, vfFFwd2, vfFFbk1, vfFFbk2, in_Output); 

			m_Memories.SetMemories(activeChannel,
				AKSIMD_GETELEMENT_V4F32(vfFFwd1, 0),
				AKSIMD_GETELEMENT_V4F32(vfFFwd2, 0),
				AKSIMD_GETELEMENT_V4F32(vfFFbk1, 0),
				AKSIMD_GETELEMENT_V4F32(vfFFbk2, 0));			
			uRemainingChannels -= 1;
		}				
		
		// Do a de-denorm pass
		AkUInt32 uMemSize = m_Memories.m_uSize;
		pMems = m_Memories.GetRawMemories();
		for(AkUInt32 c = 0; c < uMemSize; ++c)
		{
			RemoveDenormal(*pMems);
			pMems++;
		}
	}

	AkForceInline void ProcessBufferMono(AkReal32 * io_pBuffer, const AkUInt32 in_uNumframes, const AkUInt32 in_iChannel = 0)
	{
		InPlaceOutputPolicy irrelevent;
		ProcessBufferMono_<InPlaceOutputPolicy>(io_pBuffer, in_uNumframes, in_iChannel, irrelevent);
	}

	AkForceInline void ProcessBufferMono(AkReal32 * io_pBuffer, const AkUInt32 in_uNumframes, const AkUInt32 in_iChannel, AkReal32 *out_pBuffer)
	{
		OutOfPlaceOutputPolicy outputPtr = { out_pBuffer };
		ProcessBufferMono_<OutOfPlaceOutputPolicy>(io_pBuffer, in_uNumframes, in_iChannel, outputPtr);
	}

	template<class OUTPUT_POLICY>
	AkForceInline void ProcessBufferMono_(AkReal32 * io_pBuffer, AkUInt32 in_uNumframes, const AkUInt32 in_iChannel, OUTPUT_POLICY& in_Output)
	{
		// filters must have coefs set up for mono-processing if this processing is utilized
		AKASSERT(m_Memories.NumChannels() == 1 || m_Memories.MonoProcess());

		// Deal with unfriendly number of samples: the SIMD code works well with trailing zeros.
		// This class assumes that the buffer size is rounded up to the next SIMD friendly number).
		AkUInt32 uNumUnaligned = (in_uNumframes & 0x3);
		if (uNumUnaligned)
		{
			AkUInt32 uNumTrail = 4 - uNumUnaligned;
			AkReal32 * pPad = io_pBuffer + in_uNumframes;
			in_uNumframes += uNumTrail;
			do
			{
				*(pPad++) = 0.f;
			} while (--uNumTrail);
		}

		AkReal32 fFFwd1, fFFwd2, fFFbk1, fFFbk2;
		m_Memories.GetMemories(in_iChannel, fFFwd1, fFFwd2, fFFbk1, fFFbk2);
		AKSIMD_V4F32 vfFFwd1 = AKSIMD_SET_V4F32(fFFwd1);
		AKSIMD_V4F32 vfFFwd2 = AKSIMD_SET_V4F32(fFFwd2);
		AKSIMD_V4F32 vfFFbk1 = AKSIMD_SET_V4F32(fFFbk1);
		AKSIMD_V4F32 vfFFbk2 = AKSIMD_SET_V4F32(fFFbk2);

		ProcessBuffer1<OUTPUT_POLICY>(io_pBuffer, in_uNumframes, in_uNumframes, vfFFwd1, vfFFwd2, vfFFbk1, vfFFbk2, in_Output);

		fFFwd1 = AKSIMD_GETELEMENT_V4F32(vfFFwd1, 0);
		fFFwd2 = AKSIMD_GETELEMENT_V4F32(vfFFwd2, 0);
		fFFbk1 = AKSIMD_GETELEMENT_V4F32(vfFFbk1, 0);
		fFFbk2 = AKSIMD_GETELEMENT_V4F32(vfFFbk2, 0);
		m_Memories.SetMemories(in_iChannel, fFFwd1, fFFwd2, fFFbk1, fFFbk2);
	}

#endif