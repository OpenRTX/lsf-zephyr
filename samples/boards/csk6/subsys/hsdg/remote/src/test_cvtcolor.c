#include "hifi/NatureDSP_CV_math.h"
#include "hifi_sim/NatureDSP_CV_math.h"


#include <luna.h>

#define ALIGNN(n,a)	((n+(a-1))&~(a-1))
#define MALLOCA(p, s, a)		(char*)(ALIGNN((uintptr_t)(p), a)); (p) = (char*)ALIGNN((uintptr_t)(p), a) + (s);
// #define printk printf;


void HIFI_SIM(cv_cvtcolor_yuv422vyuy_bgr888_dma)(const unsigned char* src, unsigned char* dst, unsigned int width, unsigned int height, unsigned int split_num,
	unsigned char* sharebuf, unsigned int sharesize)
{
	int i, j, k;
	int n = split_num; //8
	int block_size = width * height / n;
	unsigned char *p_block_s0 = sharebuf;
	unsigned char *p_block_s1 = p_block_s0 + block_size * 2;
	unsigned char *p_block_d0 = p_block_s1 + block_size * 2;
	unsigned char *p_block_d1 = p_block_d0 + block_size * 3;
	NASSERT(p_block_d1 - sharebuf <= sharesize);
	NASSERT((width * height) % n == 0);
	NASSERT((height / n) % 2 == 0);

	for (k = 0; k < n; k++)
	{
		unsigned char* p_block_s = (unsigned char*)src + k*block_size * 2;
		unsigned char* p_block_d = dst + k*block_size * 3;
		memcpy(p_block_s0, p_block_s, block_size * 2);
		cv_cvtcolor_yuv422vyuy_bgr888(p_block_s0, p_block_d0, width, height / n);
		memcpy(p_block_d, p_block_d0, block_size * 3);
	}
}

void cv_cvtcolor_yuv422vyuy_bgr888_dma(const unsigned char* src, unsigned char* dst, unsigned int width, unsigned int height, unsigned int split_num,
	unsigned char* sharebuf, unsigned int sharesize)
{
	int i, j, k;
	int n = split_num; //8
	int block_size = width * height / n;
	int block_size1 = block_size / 2;
	unsigned char *p_block_s0 = sharebuf;
	unsigned char *p_block_s1 = p_block_s0 + block_size * 2;
	unsigned char *p_block_d0 = p_block_s1 + block_size * 2;
	unsigned char *p_block_d1 = p_block_d0 + block_size * 3;
	NASSERT(p_block_d1 - sharebuf <= sharesize);
	NASSERT((width * height) % n == 0);
	NASSERT((height/ n) % 2 == 0);

	unsigned char* p_block_s = (unsigned char*)src + (n - 1)*block_size * 2;
	unsigned char* p_block_d = dst + (n - 1)*block_size * 3;
	unsigned char* p_block_ss, *p_block_dd;

	for (k = 0; k < n; k++)
	{
		if (k == 0)
		{
			p_block_ss = p_block_s0;
			p_block_dd = p_block_d0;
			//start_counter();
			dma_cpy_async(0, p_block_s0, p_block_s, block_size * 2);
			dma_wait_complete(0);
			p_block_s -= block_size * 2;
			//printk("counter1 = %d\n", get_counter());

			//start_counter();
			cv_cvtcolor_yuv422vyuy_bgr888(p_block_ss, p_block_dd, width, height / n / 2);
			//printk("counter1 = %d\n", get_counter());

			dma_cpy_async(0, p_block_s1, p_block_s, block_size * 2);
			cv_cvtcolor_yuv422vyuy_bgr888(p_block_ss + block_size / 2 * 2, p_block_dd + block_size / 2 * 3, width, height / n / 2);
			dma_wait_complete(0);
			p_block_s -= block_size * 2;
		}
		else if (k == n - 1)
		{
			p_block_ss = p_block_s1;
			p_block_dd = p_block_d1;

			dma_cpy_async(0, p_block_d, p_block_d0, block_size * 3);
			cv_cvtcolor_yuv422vyuy_bgr888(p_block_ss, p_block_dd, width, height / n / 2);
			dma_wait_complete(0);
			p_block_d -= block_size * 3;

			cv_cvtcolor_yuv422vyuy_bgr888(p_block_ss + block_size / 2 * 2, p_block_dd + block_size / 2 * 3, width, height / n / 2);

			//start_counter();
			dma_cpy_async(0, p_block_d, p_block_d1, block_size * 3);
			dma_wait_complete(0);
			p_block_d -= block_size * 3;
			//printk("counter1 = %d\n", get_counter());
		}
		else if (k % 2 == 0)
		{
			p_block_ss = p_block_s0;
			p_block_dd = p_block_d0;

			dma_cpy_async(0, p_block_s1, p_block_s, block_size * 2);
			cv_cvtcolor_yuv422vyuy_bgr888(p_block_ss, p_block_dd, width, height / n / 2);
			dma_wait_complete(0);

			dma_cpy_async(0, p_block_d, p_block_d1, block_size * 3);
			cv_cvtcolor_yuv422vyuy_bgr888(p_block_ss + block_size / 2 * 2, p_block_dd + block_size / 2 * 3, width, height / n / 2);
			dma_wait_complete(0);

			p_block_s -= block_size * 2;
			p_block_d -= block_size * 3;
		}
		else
		{
			p_block_ss = p_block_s1;
			p_block_dd = p_block_d1;

			dma_cpy_async(0, p_block_s0, p_block_s, block_size * 2);
			cv_cvtcolor_yuv422vyuy_bgr888(p_block_ss, p_block_dd, width, height / n / 2);
			dma_wait_complete(0);

			dma_cpy_async(0, p_block_d, p_block_d0, block_size * 3);
			cv_cvtcolor_yuv422vyuy_bgr888(p_block_ss + block_size / 2 * 2, p_block_dd + block_size / 2 * 3, width, height / n / 2);
			dma_wait_complete(0);

			p_block_s -= block_size * 2;
			p_block_d -= block_size * 3;
		}
	}
}


/*

test_cvt_yuv begin...
//CP:200M
//320x240
cv_cvtcolor_yuv422vyuy_bgr888_sim(psram) = 916745
cv_cvtcolor_yuv422vyuy_bgr888(psram) = 820280
cv_cvtcolor_yuv422vyuy_bgr888_sim(share) = 883423
cv_cvtcolor_yuv422vyuy_bgr888(share) = 326564
//640x480
cv_cvtcolor_yuv422vyuy_bgr888_dma_sim = 3726385
cv_cvtcolor_yuv422vyuy_bgr888_dma = 1471809
z1 = 0x6012f680, z2 = 0x60210680, size = 230400
test_cvt_yuv end...

*/
#define PSRAM_INST_SIZE (1*1024*1024)
#define SHARE_MEME_SIZE (640*1024)
static char g_psram_inst_buffer[PSRAM_INST_SIZE];
void test_cvt_yuv()
{
	printk("\ntest_cvt_yuv\n");
	
	clock_t last_t;
	char* p_psram = g_psram_inst_buffer;
	char* p_share = (char *)LUNA_SHARE_MEM_BASE;
	const unsigned int srcw = 640;
	const unsigned int srch = 480;
	const unsigned int dstw = 640;
	const unsigned int dsth = 480;
	unsigned int crc32;
	extern struct k_heap _system_heap;
// rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, 3 * srcw*srch * 1);
	uint8_t* x = (uint8_t *)sys_heap_aligned_alloc(&_system_heap , 8,3 * srcw*srch * 1);
	ASSERT( x!=0);
	uint8_t* z1 = (uint8_t *)sys_heap_aligned_alloc(&_system_heap ,8,3 * srcw*srch * 1);
	ASSERT( z1!=0);
	uint8_t* z2 = (uint8_t *)sys_heap_aligned_alloc(&_system_heap ,8,3 * srcw*srch * 1);
		
	ASSERT( z2!=0);
	// for (int i = 0; i < 3 * srcw*srch; i++)
	// {
	// 	x[i] = rand() % 255;
	// }
	// ((unsigned char*)x)[0] = 129;

	printk("%s begin...\n", __FUNCTION__);

	//
	last_t = clock();
	// cv_cvtcolor_yuv422vyuy_bgr888_sim(x, z1, srcw / 2, srch / 2); 
	printk("cv_cvtcolor_yuv422vyuy_bgr888_sim(psram) = %d\n", clock() - last_t);

	last_t = clock();
	cv_cvtcolor_yuv422vyuy_bgr888(x, z2, srcw / 2, srch / 2);
	printk("cv_cvtcolor_yuv422vyuy_bgr888(psram) = %d\n", clock() - last_t);

	ASSERT(test_cmp(z1, z2, 3 * dstw / 2 * dsth / 2, 0x11) == 0);

	//
	last_t = clock();
	// cv_cvtcolor_yuv422vyuy_bgr888_sim((unsigned char*)p_share, (unsigned char*)p_share, srcw / 2, srch / 2);
	printk("cv_cvtcolor_yuv422vyuy_bgr888_sim(share) = %d\n", clock() - last_t);

	last_t = clock();
	cv_cvtcolor_yuv422vyuy_bgr888((unsigned char*)p_share, (unsigned char*)p_share, srcw / 2, srch / 2); 
	printk("cv_cvtcolor_yuv422vyuy_bgr888(share) = %d\n", clock() - last_t);

	//dma
	cv_cvtcolor_yuv422vyuy_bgr888(x, z1, srcw, srch);

	last_t = clock();
	// cv_cvtcolor_yuv422vyuy_bgr888_dma_sim(x, z2, srcw, srch, 16, (unsigned char*)p_share, 640 * 1024);
	printk("cv_cvtcolor_yuv422vyuy_bgr888_dma_sim = %d\n", clock() - last_t);
	// NASSERT(test_cmp(z1, z2, 3 * dstw * dsth, 0x11) == 0);

	//memcpy(z2, x, srcw*srch * 2);
	dma_cpy_async(0, z2, x, srcw*srch * 2);
	dma_wait_complete(0);

	last_t = clock();
	cv_cvtcolor_yuv422vyuy_bgr888_dma(z2, z2, srcw, srch, 16, (unsigned char*)p_share, 640 * 1024); 
	printk("cv_cvtcolor_yuv422vyuy_bgr888_dma = %d\n", clock() - last_t);

	// NASSERT(test_cmp(z1, z2, 3 * dstw * dsth, 0x11) == 0);
	printk("z1 = 0x%x, z2 = 0x%x, size = %d\n", z1, z2, 3 * dstw / 2 * dsth / 2);

#if 1
	// dma_cpy_async(0, z2, cvtcolor_yuv422vyuy_resource, srcw*srch * 2);
	// dma_wait_complete(0);
	last_t = clock();
	cv_cvtcolor_yuv422vyuy_bgr888_dma(z2, z2, srcw, srch, 16, (unsigned char*)p_share, 640 * 1024);
	printk("cv_cvtcolor_yuv422vyuy_bgr888_dma = %d\n", clock() - last_t);
	// crc32 = iv_crc32_calc(z2, srcw*srch * 3, 0);
	printk("crc32 = 0x%x\n", crc32);
	NASSERT(crc32 = 0x42d81e47);
#endif 

	printk("%s end...\n", __FUNCTION__);
}

