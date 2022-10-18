#if defined(CSK6001)
#include <xtensa/config/core-isa.h>
#include <xtensa/tie/xt_core.h>
#include <xtensa/tie/xt_misc.h>
#include <xtensa/tie/xt_hifi3.h>
#include <xtensa/tie/xt_hifi4.h>
#else
#include "common.h"
#endif 

#include "hifi/NatureDSP_CV_math.h"

void cv_cvtcolor_yuv422vyuy_bgr888(const unsigned char* src, unsigned char* dst, unsigned int width, unsigned int height)
{
	int i, j, k;
	ae_int16x4 x0, x1, y0, v0, u0, xh0;
	ae_int32x2 z00, z01, z10, z11, z20, z21;
	ae_int24x2 z0, z1;
	ae_int16x4* px;
	ae_int24x2* pz;
	ae_int16x4 x256 = AE_MOVDA16(256);
	ae_int16x4 x128 = AE_MOVDA16(128);
	ae_int16x4 x359 = AE_MOVDA16(359);
	ae_int16x4 x183 = AE_MOVDA16(-183);
	ae_int16x4 x444 = AE_MOVDA16(444);
	ae_int16x4 x88 = AE_MOVDA16(-88);
	ae_int16x4 xff = AE_MOVDA16(0xff);
	ae_int32x2 zmask0 = AE_MOVDA32(0xff);
	ae_int32x2 zmask1 = AE_MOVDA32(0xff00);
	ae_int32x2 zmask2 = AE_MOVDA32(0xff0000);

	unsigned int z;
	const unsigned char* yuv_plane = src;
	unsigned char* rgb_plane = dst;

	px = (ae_int16x4*)yuv_plane;
	pz = (ae_int24x2*)rgb_plane;

	NASSERT(((uintptr_t)src & 0x7) == 0);
	NASSERT(((uintptr_t)dst & 0x7) == 0);
	NASSERT((width & 0x3) == 0);

	AE_L16X4_IP(xh0, px, 8);
	ae_valign zo_align = AE_ZALIGN64();

	for (int j = 0; j < ((height*width) >> 2); j++)
	{
		x0 = xh0;

		x1 = AE_AND16(x0, xff);
		y0 = AE_SLAA16S(x0, -8);
		y0 = AE_AND16(y0, xff);
		//y0 = AE_MOVINT16X4_FROMINT64((AE_SRLI64(AE_MOVINT64_FROMINT16X4(x0), 8)));

		x0 = AE_SEL16_7632(x1, x1);
		x1 = AE_SEL16_5410(x1, x1);
		v0 = AE_SEL16_7531(x0, x1);
		u0 = AE_SEL16_6420(x0, x1);
		v0 = AE_SUB16(v0, x128);
		u0 = AE_SUB16(u0, x128);

		//R
		AE_MUL16X4(z00, z01, y0, x256);
		//z10 = z00; z11 = z01;
		AE_MULA16X4(z00, z01, v0, x359);
		//G
		AE_MUL16X4(z10, z11, y0, x256);
		//z20 = z10; z21 = z11;
		AE_MULA16X4(z10, z11, v0, x183);
		AE_MULA16X4(z10, z11, u0, x88);
		//B
		AE_MUL16X4(z20, z21, y0, x256);
		AE_MULA16X4(z20, z21, u0, x444);
		AE_L16X4_IP(xh0, px, 8);

		//L->H:BGR0
		z00 = AE_AND32(AE_SLAA32S(z00, 8), zmask2);//H ((>>8)<<16)
		z10 = AE_AND32(z10, zmask1); //((>>8)<<8)
		z20 = AE_AND32(AE_SLAA32S(z20, -8), zmask0);//L
		z00 = AE_OR32(z00, AE_OR32(z10, z20));
		z0 = AE_MOVINT24X2_FROMF32X2(z00);
		AE_SA24X2_IP(z0, zo_align, pz);

		z01 = AE_AND32(AE_SLAA32S(z01, 8), zmask2);
		z11 = AE_AND32(z11, zmask1);
		z21 = AE_AND32(AE_SLAA32S(z21, -8), zmask0);
		z01 = AE_OR32(z01, AE_OR32(z11, z21));
		z1 = AE_MOVINT24X2_FROMF32X2(z01);
		AE_SA24X2_IP(z1, zo_align, pz);
	}
	AE_SA64POS_FP(zo_align, pz);
}
