/*
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>

#include <string.h>

#include "venus_cp.h"
#include "luna/luna.h"
#include "hal/ClockManager.h"

#include "dma_venus.h"

#include "cv.h"
#include "img.h"

#define BMP_128_180_HEAD_LEN (14 + 40 + 1024)
#define BMP_128_180_DATA_LEN (128*180)
#define BMP_128_180_LEN (BMP_128_180_HEAD_LEN + BMP_128_180_DATA_LEN)

#define BMP_IMG_WIDTH (128)
static __attribute__((aligned(4))) uint8_t temp_buf[280 * BMP_IMG_WIDTH] = {0};

static void
csk_dma_irq_handler(const struct device *dev)
{
    dma_irq_handler();
}

void main(void)
{
    uint8_t *src = NULL;
    uint8_t *dest = NULL;
    int src_len = 0;
    int dest_len = 0;    

    printk("[Remote][%s %d] Enter...\n",__FUNCTION__,__LINE__);

    /* dma initialize */
	IRQ_CONNECT(3, 1, csk_dma_irq_handler, NULL, 0);                                                                                                          
	irq_enable(3);
    dma_initialize();

    /* get test input bmp information */
    int image_cnt = sizeof(img_resource) / (BMP_128_180_LEN);
    printk("image cnt: %d\n", image_cnt);

    /* xy crop prepare */
    ai_xy_crop_new();
    ai_xy_crop_prepare();

    /* cutline ocr prepare */
    ai_cutline_ocr_prepare();

    printk("[0]: ai prepare done\n");

    for(int i = 0; i < image_cnt; i++){
        /* 1. find x,y offset 
         * 2. output cropped frame */
        src = (uint8_t *)img_resource  + i * BMP_128_180_LEN + BMP_128_180_HEAD_LEN;
        src_len = BMP_128_180_DATA_LEN;
        dest = (uint8_t *)temp_buf;
        ai_xy_crop_copy(src, src_len, dest, &dest_len);

        /* 1. collect cuted frame 
         * 2. if stop scanning then use cutline algothrim
         * 3. use ocr algothrim to get result */
        src = dest;
        src_len = dest_len;
        dest = NULL;
        dest_len = 0;
        ai_cutline_ocr_copy(src, src_len);
    }

    dest = (uint8_t *)temp_buf;
    dest_len = 0;
    ai_cutline_ocr_deal(&dest, &dest_len);

    ai_cutline_ocr_free();

E2D:
	while(1){
		k_msleep(100);
	}
}