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

/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis 'TREMOR' CODEC SOURCE CODE.   *
 *                                                                  *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis 'TREMOR' SOURCE CODE IS (C) COPYRIGHT 1994-2003    *
 * BY THE Xiph.Org FOUNDATION http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: floor backend 1 implementation

 ********************************************************************/

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ogg.h"
#include "ivorbiscodec.h"
#include "floor1.h"
#include "codebook.h"
#include "misc.h"
#include "os.h"
#include "../../../../AkAudiolib/Common/AkRuntimeEnvironmentMgr.h"

#define floor1_rangedB 140 /* floor 1 fixed at -140dB to 0dB range */
#define VIF_POSIT 63

/***********************************************/

static void mergesort(char *index,ogg_uint16_t *vals,ogg_uint16_t n){
  ogg_uint16_t i,j;
  char *temp,*A = index,*B = (char*) alloca(n*sizeof(*B));

  for(i=1;i<n;i<<=1){
    for(j=0;j+i<n;){
      int k1=j;
      int mid=j+i;
      int k2=mid;
      int end=(j+i*2<n?j+i*2:n);
      while(k1<mid && k2<end){
	if(vals[(int)A[k1]]<vals[(int)A[k2]])
	  B[j++]=A[k1++];
	else
	  B[j++]=A[k2++];
      }
      while(k1<mid) B[j++]=A[k1++];
      while(k2<end) B[j++]=A[k2++];
    }
    for(;j<n;j++)B[j]=A[j];
    temp=A;A=B;B=temp;
  }
 
  if(B==index)
	  for(j=0;j<n;j++)B[j]=A[j];
}

int floor1_info_unpack(vorbis_info_floor1*	info,
						codec_setup_info*	ci,
						oggpack_buffer*		opb,
						CAkVorbisAllocator&	VorbisAllocator)
{
	int j,k,count = 0,maxclass = -1,rangebits;

	/* read partitions */
	info->partitions		= (int) oggpack_read(opb,5); /* only 0 to 31 legal */
	int AllocSize			= info->partitions*sizeof(*info->partitionclass);
	info->partitionclass	= (char *)VorbisAllocator.Alloc(AllocSize);

	for(j = 0 ; j < info->partitions ; ++j)
	{
		info->partitionclass[j] = oggpack_read(opb,4); /* only 0 to 15 legal */
		if(maxclass<info->partitionclass[j])
		{
			maxclass=info->partitionclass[j];
		}
	}

	/* read partition classes */
	AllocSize	= (maxclass+1)*sizeof(*info->Class);
	info->Class	= (floor1class *)VorbisAllocator.Alloc(AllocSize);

	for(j = 0 ; j < maxclass + 1 ; ++j)
	{
		info->Class[j].class_dim	= oggpack_read(opb,3)+1; /* 1 to 8 */
		info->Class[j].class_subs	= oggpack_read(opb,2); /* 0,1,2,3 bits */
		if(oggpack_eop(opb)<0)
		{
			goto err_out;
		}
		if(info->Class[j].class_subs)
		{
			info->Class[j].class_book = oggpack_read(opb,8);
			if(info->Class[j].class_book>=ci->books)
			{
				goto err_out;
			}
		}
		else
		{
			info->Class[j].class_book=0;
		}
		if(info->Class[j].class_book>=ci->books)
		{
			goto err_out;
		}
		for(k=0;k<(1<<info->Class[j].class_subs);k++)
		{
			info->Class[j].class_subbook[k] = oggpack_read(opb,8)-1;
			if(info->Class[j].class_subbook[k]>=ci->books && info->Class[j].class_subbook[k]!=0xff)
			{
				goto err_out;
			}
		}
	}

	/* read the post list */
	info->mult	= oggpack_read(opb,2)+1;     /* only 1,2,3,4 legal now */ 
	rangebits	= oggpack_read(opb,4);

	for(j = 0,k = 0;j < info->partitions ; ++j)
	{
		count+=info->Class[(int)info->partitionclass[j]].class_dim; 
	}

	info->postlist		= (ogg_uint16_t *)VorbisAllocator.Alloc((count+2)*sizeof(*info->postlist));
	info->forward_index	= (char *)VorbisAllocator.Alloc((count+2)*sizeof(*info->forward_index));
	info->loneighbor	= (char *)VorbisAllocator.Alloc(count*sizeof(*info->loneighbor));
	info->hineighbor	= (char *)VorbisAllocator.Alloc(count*sizeof(*info->hineighbor));

	count=0;
	for(j=0,k=0;j<info->partitions;j++)
	{
		count += info->Class[(int)info->partitionclass[j]].class_dim; 
		for( ; k < count ; ++k)
		{
			int t = info->postlist[k+2] = oggpack_read_var(opb,rangebits);
			if(t >= (1<<rangebits))
			{
				goto err_out;
			}
		}
	}

	if(oggpack_eop(opb))
	{
		goto err_out;
	}

	info->postlist[0]	= 0;
	info->postlist[1]	= 1<<rangebits;
	info->posts			= count + 2;

	/* also store a sorted position index */
	for(j=0;j<info->posts;j++)info->forward_index[j]=j;
	mergesort(info->forward_index,info->postlist,info->posts);

	/* discover our neighbors for decode where we don't use fit flags
	(that would push the neighbors outward) */
	for(j=0;j<info->posts-2;j++)
	{
		int lo=0;
		int hi=1;
		int lx=0;
		int hx=info->postlist[1];
		int currentx=info->postlist[j+2];
		for(k=0;k<j+2;k++)
		{
			int x=info->postlist[k];
			if(x>lx && x<currentx)
			{
				lo = k;
				lx = x;
			}
			if(x<hx && x>currentx)
			{
				hi = k;
				hx = x;
			}
		}
		info->loneighbor[j] = lo;
		info->hineighbor[j] = hi;
	}

	return 0;

err_out:

	return -1;
}

static inline int ilog(unsigned int v){
  int ret=0;
  while(v){
    ret++;
    v>>=1;
  }
  return(ret);
}

static inline int render_point(int x0,int x1,int y0,int y1,int x)
{
  y0&=0x7fff; /* mask off flag */
  y1&=0x7fff;
    
  {
    int dy=y1-y0;
    int adx=x1-x0;
    int ady=abs(dy);
    int err=ady*(x-x0);
    
	int off = (err / adx);
    if(dy<0)return(y0-off);	
    return(y0+off);
  }
}

static int quant_look[4]={256,128,86,64};

ogg_int32_t *floor1_inverse1(	vorbis_dsp_state*	vd,
								vorbis_info_floor1*	info,
								ogg_int32_t*		fit_value)
{
	codec_setup_info* ci = vd->csi;
  
	int i,j,k;
	codebook *books=ci->book_param;   
	int quant_q=quant_look[info->mult-1];

	/* unpack wrapped/predicted values from stream */
	if (oggpack_read(GET_OGGPACK_BUFFER(), 1) != 1)
		return NULL;

	int fit_bits = ilog(quant_q-1);
	fit_value[0]=oggpack_read_var( GET_OGGPACK_BUFFER(), fit_bits );
	fit_value[1]=oggpack_read_var( GET_OGGPACK_BUFFER(), fit_bits );

	/* partition by partition */
	/* partition by partition */
	for(i=0,j=2;i<info->partitions;i++)
	{
		int classv=info->partitionclass[i];
		int cdim=info->Class[classv].class_dim;
		int csubbits=info->Class[classv].class_subs;
		int csub=1<<csubbits;
		int cval=0;

		/* decode the partition's first stage cascade value */
		if(csubbits)
		{
			cval=ak_vorbis_book_decode(books+info->Class[classv].class_book, GET_OGGPACK_BUFFER() );
		}

		for(k=0;k<cdim;k++)
		{
			int book = info->Class[classv].class_subbook[cval&(csub - 1)];
			cval >>= csubbits;
			if (book != 0xff) 
			{
				fit_value[j + k] = ak_vorbis_book_decode(books + book, GET_OGGPACK_BUFFER());
			}
			else
			{
				fit_value[j + k] = 0;
			}
		}
		j+=cdim;
	}

	/* unwrap positive values and reconstitute via linear interpolation */
	for(i=2;i<info->posts;i++)
	{
		int predicted=render_point(info->postlist[(int)info->loneighbor[i-2]],
				info->postlist[(int)info->hineighbor[i-2]],
				fit_value[(int)info->loneighbor[i-2]],
				fit_value[(int)info->hineighbor[i-2]],
				info->postlist[i]);

		int hiroom=quant_q-predicted;
		int loroom=predicted;
		int room=(hiroom<loroom?hiroom:loroom)<<1;
		int val=fit_value[i];

		if(val)
		{
			if(val>=room)
			{
				if (hiroom > loroom)
				{
					val = val - loroom;
				}
				else 
				{
					val = -1 - (val - hiroom);
				}
			}
			else
			{
				if (val & 1) 
				{
					val = -((val + 1) >> 1);
				}
				else 
				{
					val >>= 1;
				}
			}

			fit_value[i]=val+predicted;
			fit_value[(int)info->loneighbor[i-2]]&=0x7fff;
			fit_value[(int)info->hineighbor[i-2]]&=0x7fff;
		}
		else
		{
			fit_value[i]=predicted|0x8000;
		}
	
	}

	return(fit_value);
}

/*
This function replaces the original floor_from_dB lookup table.  
Initially, the table contains the values -140db to 0db indexed in 256 increments, with the output scaled to 0-65536 (initially for fixed-point computation)
So, mathematically the original table's function was pow(10, ((X+1)*140/256)-140) * 65536.
The function floor_from_dB3 implements this computation (including the 140db rescaling) and does the pow() using the int<->float conversion. 
This conversion is refined a bit.  The 65536 is not necessary anymore given float computations.

inline float floor_from_dB3(float x)
{
	float p = -23.16266269286690 + x* 0.0908339713445762;
	int w = p;
	float z = p - w + 1;
	//union { uint32_t i; float f; } v = { (1 << 23) * (p + 121.2740838f + 27.7280233f / (4.84252568f - z) - 1.49012907f * z) };
	union { unsigned int i; float f; } v = { 8388608.0f * (x* 0.0908339713445762 + 98.1114211071331f + 27.7280233f / (4.84252568f - z) - 1.49012907f * z) };

	return v.f;
}

void render_line3(int x0, int x1, int y0, int y1, ogg_int32_t *&d)
{
	int len = x1 - x0;
	const float dy = (y1) - (y0);
	const float adx = x1 - x0;
	const float slope = dy / adx;
	float y = y0;

	ogg_int32_t* pEnd = d + len;

	float fx = 0;
	float fy = y0;

	while (d < pEnd)
	{
		float fi = floor_from_dB3(fy);
		*((float*)d) = fi;
		fx = fx + 1.0f;
		fy = slope*fx + y0;
		d++;
	}
}
*/

struct floor_Scalar
{
	//This function is a factorization of the render_line3 floor_from_dB3 code above.  Served as a base for the SIMD versions
	static AkForceInline void render_line(int x0, int x1, int y0, int y1, ogg_int32_t *&out, ogg_int32_t *in_pEnd)
	{
		int len = x1 - x0;
		const float dy = y1 - y0;
		const float adx = x1 - x0;
		const float slope = dy / adx;		

		ogg_int32_t* pEnd = AkMin(out + len, in_pEnd);

		float fx = 0;		

		const float b = -23.16266269286690 + y0*0.0908339713445762;
		const float a = slope*0.0908339713445762;
		const float c = slope*-373463.931103;
		const float d = y0*-373463.931103 + 1.1000539433507213e9;
		const float e = -3.90516593047587e-10*slope;
		const float f = -3.90516593047587e-10*y0 + 1.161016522989446e-7;

		while (out < pEnd)
		{			
			float w = (float)(int)(fx*a + b);
			float y = c*fx + d + 1.0 / (4.299235046832561e-9* w + e*fx + f) + 1.250010863763456e7*w;

			unsigned int i = (unsigned int)(y);
			float fi = (*(float*)&i);
			*((float*)out) = fi;

			fx = fx + 1.0f;			
			out++;
		}
	}
};

void test_render_line_Scalar(int x0, int x1, int y0, int y1, ogg_int32_t *&out, ogg_int32_t *in_pEnd)
{
	return floor_Scalar::render_line(x0, x1, y0, y1, out, in_pEnd);
};

static const float fFLOOR_fromdB_LOOKUP[256] = {
	1.0649863e-07F, 1.1341951e-07F, 1.2079015e-07F, 1.2863978e-07F,
	1.3699951e-07F, 1.4590251e-07F, 1.5538408e-07F, 1.6548181e-07F,
	1.7623575e-07F, 1.8768855e-07F, 1.9988561e-07F, 2.128753e-07F,
	2.2670913e-07F, 2.4144197e-07F, 2.5713223e-07F, 2.7384213e-07F,
	2.9163793e-07F, 3.1059021e-07F, 3.3077411e-07F, 3.5226968e-07F,
	3.7516214e-07F, 3.9954229e-07F, 4.2550680e-07F, 4.5315863e-07F,
	4.8260743e-07F, 5.1396998e-07F, 5.4737065e-07F, 5.8294187e-07F,
	6.2082472e-07F, 6.6116941e-07F, 7.0413592e-07F, 7.4989464e-07F,
	7.9862701e-07F, 8.5052630e-07F, 9.0579828e-07F, 9.6466216e-07F,
	1.0273513e-06F, 1.0941144e-06F, 1.1652161e-06F, 1.2409384e-06F,
	1.3215816e-06F, 1.4074654e-06F, 1.4989305e-06F, 1.5963394e-06F,
	1.7000785e-06F, 1.8105592e-06F, 1.9282195e-06F, 2.0535261e-06F,
	2.1869758e-06F, 2.3290978e-06F, 2.4804557e-06F, 2.6416497e-06F,
	2.8133190e-06F, 2.9961443e-06F, 3.1908506e-06F, 3.3982101e-06F,
	3.6190449e-06F, 3.8542308e-06F, 4.1047004e-06F, 4.3714470e-06F,
	4.6555282e-06F, 4.9580707e-06F, 5.2802740e-06F, 5.6234160e-06F,
	5.9888572e-06F, 6.3780469e-06F, 6.7925283e-06F, 7.2339451e-06F,
	7.7040476e-06F, 8.2047000e-06F, 8.7378876e-06F, 9.3057248e-06F,
	9.9104632e-06F, 1.0554501e-05F, 1.1240392e-05F, 1.1970856e-05F,
	1.2748789e-05F, 1.3577278e-05F, 1.4459606e-05F, 1.5399272e-05F,
	1.6400004e-05F, 1.7465768e-05F, 1.8600792e-05F, 1.9809576e-05F,
	2.1096914e-05F, 2.2467911e-05F, 2.3928002e-05F, 2.5482978e-05F,
	2.7139006e-05F, 2.8902651e-05F, 3.0780908e-05F, 3.2781225e-05F,
	3.4911534e-05F, 3.7180282e-05F, 3.9596466e-05F, 4.2169667e-05F,
	4.4910090e-05F, 4.7828601e-05F, 5.0936773e-05F, 5.4246931e-05F,
	5.7772202e-05F, 6.1526565e-05F, 6.5524908e-05F, 6.9783085e-05F,
	7.4317983e-05F, 7.9147585e-05F, 8.4291040e-05F, 8.9768747e-05F,
	9.5602426e-05F, 0.00010181521F, 0.00010843174F, 0.00011547824F,
	0.00012298267F, 0.00013097477F, 0.00013948625F, 0.00014855085F,
	0.00015820453F, 0.00016848555F, 0.00017943469F, 0.00019109536F,
	0.00020351382F, 0.00021673929F, 0.00023082423F, 0.00024582449F,
	0.00026179955F, 0.00027881276F, 0.00029693158F, 0.00031622787F,
	0.00033677814F, 0.00035866388F, 0.00038197188F, 0.00040679456F,
	0.00043323036F, 0.00046138411F, 0.00049136745F, 0.00052329927F,
	0.00055730621F, 0.00059352311F, 0.00063209358F, 0.00067317058F,
	0.00071691700F, 0.00076350630F, 0.00081312324F, 0.00086596457F,
	0.00092223983F, 0.00098217216F, 0.0010459992F, 0.0011139742F,
	0.0011863665F, 0.0012634633F, 0.0013455702F, 0.0014330129F,
	0.0015261382F, 0.0016253153F, 0.0017309374F, 0.0018434235F,
	0.0019632195F, 0.0020908006F, 0.0022266726F, 0.0023713743F,
	0.0025254795F, 0.0026895994F, 0.0028643847F, 0.0030505286F,
	0.0032487691F, 0.0034598925F, 0.0036847358F, 0.0039241906F,
	0.0041792066F, 0.0044507950F, 0.0047400328F, 0.0050480668F,
	0.0053761186F, 0.0057254891F, 0.0060975636F, 0.0064938176F,
	0.0069158225F, 0.0073652516F, 0.0078438871F, 0.0083536271F,
	0.0088964928F, 0.009474637F, 0.010090352F, 0.010746080F,
	0.011444421F, 0.012188144F, 0.012980198F, 0.013823725F,
	0.014722068F, 0.015678791F, 0.016697687F, 0.017782797F,
	0.018938423F, 0.020169149F, 0.021479854F, 0.022875735F,
	0.024362330F, 0.025945531F, 0.027631618F, 0.029427276F,
	0.031339626F, 0.033376252F, 0.035545228F, 0.037855157F,
	0.040315199F, 0.042935108F, 0.045725273F, 0.048696758F,
	0.051861348F, 0.055231591F, 0.058820850F, 0.062643361F,
	0.066714279F, 0.071049749F, 0.075666962F, 0.080584227F,
	0.085821044F, 0.091398179F, 0.097337747F, 0.10366330F,
	0.11039993F, 0.11757434F, 0.12521498F, 0.13335215F,
	0.14201813F, 0.15124727F, 0.16107617F, 0.17154380F,
	0.18269168F, 0.19456402F, 0.20720788F, 0.22067342F,
	0.23501402F, 0.25028656F, 0.26655159F, 0.28387361F,
	0.30232132F, 0.32196786F, 0.34289114F, 0.36517414F,
	0.38890521F, 0.41417847F, 0.44109412F, 0.46975890F,
	0.50028648F, 0.53279791F, 0.56742212F, 0.60429640F,
	0.64356699F, 0.68538959F, 0.72993007F, 0.77736504F,
	0.82788260F, 0.88168307F, 0.9389798F, 1.F,
};

struct floor_Tremor
{	
	//Original Tremor render_line is faster by a lot on most ARM processors.  The new algo complexity isn't compensated by the SIMD instructions.
	static AkForceInline void render_line(int x0, int x1, int y0, int y1, ogg_int32_t *&d, ogg_int32_t *in_pEnd)
	{
		AKSIMD_PREFETCHMEMORY(y0 * 4, fFLOOR_fromdB_LOOKUP);
		const int dy = y1 - y0;
		const int adx = x1 - x0;
		const int base = dy / adx;
		const int ady = abs(dy) - abs(base*adx);
		const int sy = (dy < 0 ? base - 1 : base + 1);
		int x = x0;
		int y = y0;
		int err = 0;

		in_pEnd = AkMin(d + adx, in_pEnd);
		*((float*)d) = fFLOOR_fromdB_LOOKUP[y];
		d++;		
		x++;
		while (d < in_pEnd)
		{			
			err = err + ady;
			if (err < adx)
			{
				y += base;
			}
			else
			{
				err -= adx;
				y += sy;
			}

			*((float*)d) = fFLOOR_fromdB_LOOKUP[y];
			d++;
			x++;
		}
	}
};

void test_render_line_Tremor(int x0, int x1, int y0, int y1, ogg_int32_t *&out, ogg_int32_t *in_pEnd)
{
	floor_Tremor::render_line(x0, x1, y0, y1, out, in_pEnd);	
};

// Implemention of render_line for general V4F32 usage; specialization for ARM_NEON to follow
#if !defined(AK_CPU_ARM_NEON)

#ifdef AKSIMD_AVX_SUPPORTED
extern int floor1_inverse2_AVX(struct vorbis_dsp_state*	vd,
	vorbis_info_floor1*	info,
	ogg_int32_t*		fit_value,
	ogg_int32_t*		out,
	int in_end);
#endif

struct floor_V4F32
{
	static AkForceInline void render_line(int x0, int x1, int y0, int y1, ogg_int32_t *&out, ogg_int32_t *in_pEnd)
	{
		int len = x1 - x0;
		const float dy = y1 - y0;
		const float adx = x1 - x0;
		const float slope = dy / adx;
		const float fy0 = y0;

		ogg_int32_t* pEnd = AkMin(out + len, in_pEnd);

		const AKSIMD_DECLARE_V4F32(four, 4.f, 4.f, 4.f, 4.f);
		const AKSIMD_DECLARE_V4F32(ramp, 0.f, 1.f, 2.f, 3.f);

		AKSIMD_V4F32 fx = ramp;

		const AKSIMD_V4F32 a = AKSIMD_SET_V4F32(slope*0.0908339713445762);
		const AKSIMD_V4F32 b = AKSIMD_SET_V4F32(-23.16266269286690 + fy0*0.0908339713445762);
		const AKSIMD_V4F32 c = AKSIMD_SET_V4F32(slope*-373463.931103);
		const AKSIMD_V4F32 d = AKSIMD_SET_V4F32(1.1000539433507213e9 + fy0*-373463.9311030);
		const AKSIMD_V4F32 e = AKSIMD_SET_V4F32(slope *-3.90516593047587e-10);
		const AKSIMD_V4F32 f = AKSIMD_SET_V4F32(1.161016522989446e-7 - 3.90516593047587e-10*fy0);
		const AKSIMD_V4F32 g = AKSIMD_SET_V4F32( 4.299235046832561e-9);
		const AKSIMD_V4F32 h = AKSIMD_SET_V4F32( 1.250010863763456e7);

		while (out < pEnd)
		{
			//float w = (float)(int)(fx*a + b);
			AKSIMD_V4F32 w = AKSIMD_MADD_V4F32(fx, a, b);
			w = AKSIMD_CEIL_V4F32(w);

			//float y = c*fx + d + 1.0 / (g* w + e*fx + f) + h*w;
			AKSIMD_V4F32 tmp1 = AKSIMD_MADD_V4F32(fx, e, f);
			AKSIMD_V4F32 denum = AKSIMD_MADD_V4F32(w, g, tmp1);	// g* w + (e*fx + f)
			AKSIMD_V4F32 tmp2 = AKSIMD_MADD_V4F32(fx, c, d);
			AKSIMD_V4F32 y = AKSIMD_MADD_V4F32(w, h, tmp2); // h*w + (c*fx + d)
			y = AKSIMD_ADD_V4F32(y, AKSIMD_RECIP_V4F32(denum));

			AKSIMD_STOREU_V4I32((AKSIMD_V4I32*)out, AKSIMD_ROUND_V4F32_TO_V4I32(y));

			fx = AKSIMD_ADD_V4F32(fx, four);
			out += 4;;
		}
		out = pEnd;
	}
};

#else
//Keeping this implementation for future activation, when most ARM variants are fast enough.
struct floor_V4F32
{		
	static AkForceInline void render_line(int x0, int x1, int y0, int y1, ogg_int32_t *&out, ogg_int32_t *in_pEnd)
	{
		int len = x1 - x0;
		const float dy = y1 - y0;
		const float adx = len;
		const float slope = dy / adx;
		const float fy0 = y0;

		ogg_int32_t* pEnd = AkMin(out + len, in_pEnd);
		
		const AKSIMD_DECLARE_V4F32(four, 4.f, 4.f, 4.f, 4.f);
		const AKSIMD_DECLARE_V4F32(ramp, 0.f, 1.f, 2.f, 3.f);

		AKSIMD_V4F32 fx = ramp;

		const AKSIMD_V4F32 a = AKSIMD_SET_V4F32(slope*0.0908339713445762);
		const AKSIMD_V4F32 b = AKSIMD_SET_V4F32(-23.16266269286690 + fy0 * 0.0908339713445762);
		const AKSIMD_V4F32 c = AKSIMD_SET_V4F32(slope*-373463.931103);
		const AKSIMD_V4F32 d = AKSIMD_SET_V4F32(1.1000539433507213e9 + fy0 * -373463.9311030);
		const AKSIMD_V4F32 e = AKSIMD_SET_V4F32(slope *-3.90516593047587e-10);
		const AKSIMD_V4F32 f = AKSIMD_SET_V4F32(1.161016522989446e-7 - 3.90516593047587e-10*fy0);
		const AKSIMD_V4F32 g = AKSIMD_SET_V4F32(4.299235046832561e-9);
		const AKSIMD_V4F32 h = AKSIMD_SET_V4F32(1.250010863763456e7);

		//Deal with alignment
		int unaligned = ((AkUIntPtr)out & 15) >> 4;
		if (unaligned)
		{
			//float w = (float)(int)(fx*a + b);
			AKSIMD_V4F32 w = AKSIMD_MADD_V4F32(fx, a, b);
			w = AKSIMD_CONVERT_V4I32_TO_V4F32(AKSIMD_TRUNCATE_V4F32_TO_V4I32(w));	//CEIL, using round-toward-zero conversions . In this application, all numbers are negative.

			//float y = c*fx + d + 1.0 / (g* w + e*fx + f) + h*w;
			AKSIMD_V4F32 tmp1 = AKSIMD_MADD_V4F32(fx, e, f);
			AKSIMD_V4F32 denum = AKSIMD_MADD_V4F32(w, g, tmp1);	// g* w + (e*fx + f)
			AKSIMD_V4F32 tmp2 = AKSIMD_MADD_V4F32(fx, c, d);
			AKSIMD_V4F32 y = AKSIMD_MADD_V4F32(w, h, tmp2); // h*w + (c*fx + d)
			y = AKSIMD_ADD_V4F32(y, AKSIMD_RECIP_V4F32(denum));

			AKSIMD_STOREU_V4I32((AKSIMD_V4I32*)out, AKSIMD_ROUND_V4F32_TO_V4I32(y));
			fx = AKSIMD_ADD_V4F32(fx, AKSIMD_SET_V4F32((float)unaligned));
			out += unaligned;
		}

#pragma unroll (4)
		while (out < pEnd)
		{
			//float w = (float)(int)(fx*a + b);
			AKSIMD_V4F32 w = AKSIMD_MADD_V4F32(fx, a, b);
			w = AKSIMD_CONVERT_V4I32_TO_V4F32(AKSIMD_TRUNCATE_V4F32_TO_V4I32(w));	//CEIL, using round-toward-zero conversions . In this application, all numbers are negative.

			//float y = c*fx + d + 1.0 / (g* w + e*fx + f) + h*w;

			AKSIMD_V4F32 denum = AKSIMD_MADD_V4F32(fx, e, f);
			denum = AKSIMD_MADD_V4F32(w, g, denum);	// g* w + (e*fx + f)
			AKSIMD_V4F32 y = AKSIMD_MADD_V4F32(fx, c, d);
			y = AKSIMD_MADD_V4F32(w, h, y); // h*w + (c*fx + d)
			y = AKSIMD_ADD_V4F32(y, AKSIMD_RECIP_V4F32(denum));

			AKSIMD_STORE_V4I32((AKSIMD_V4I32*)out, AKSIMD_ROUND_V4F32_TO_V4I32(y));

			fx = AKSIMD_ADD_V4F32(fx, four);
			out += 4;;
		}
		out = pEnd;
	}
};
#endif

void test_render_line_V4F32(int x0, int x1, int y0, int y1, ogg_int32_t *&out, ogg_int32_t *in_pEnd)
{
	return floor_V4F32::render_line(x0, x1, y0, y1, out, in_pEnd);
};

floor1_inverse2_fn floor1_inverse2;

void InitFloorFunc()
{	
#if defined AKSIMD_AVX_SUPPORTED
	if (AK::AkRuntimeEnvironmentMgr::Instance()->GetSIMDSupport(AK::AK_SIMD_AVX))
		floor1_inverse2 = floor1_inverse2_AVX;
	else
#endif
#ifdef AK_CPU_ARM_NEON
		floor1_inverse2 = floor1_inverse2_template<floor_Tremor>;
#else
		floor1_inverse2 = floor1_inverse2_template<floor_V4F32>;
#endif
}

