#ifndef _CV_CROP_H_
#define _CV_CROP_H_

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef struct _CVSize
{
	union
	{
		uint32_t width;
		int32_t x;
	};
	union
	{
		uint32_t height;
		int32_t y;
	};	
	
}CVSize;

#define MAX_OUTPUT_IMAGE_WIDTH	(400)
#define MAX_OUTPUT_IMAGE_HEIGTH (1000)
#define MAX_OUTPUT_IMAGE_SIZE	(MAX_OUTPUT_IMAGE_WIDTH*MAX_OUTPUT_IMAGE_HEIGTH)
#define W_START_POINT	((MAX_OUTPUT_IMAGE_WIDTH-180)>>1)

#define ORI_STITCH_ROW // 横向拼接

#ifdef ORI_STITCH_ROW
#define ORI_MAX_OUTPUT_IMAGE_WIDTH (128 * 5)
#define ORI_MAX_OUTPUT_IMAGE_HEIGHT (180)
#else
#define ORI_MAX_OUTPUT_IMAGE_WIDTH (128)
#define ORI_MAX_OUTPUT_IMAGE_HEIGHT (180 * 5)
#endif

enum ISTITCH_ISTATUS_ {
	ISTITCH_ISTATUS_BEGIN_GLOBAL_,	//扫描开始
	ISTITCH_ISTATUS_BEGIN_BATCH_,	//扫描过程中某次拼接开始
	ISTITCH_ISTATUS_CONTINUE_,
};

enum CV_CROP_ERR_ {
	CV_CROP_NO_ERR = 0,
	CV_CROP_ERR_INVALID_PARAM = 3001,
	CV_CROP_ERR_OVER_BOUND,	//超出上边界或者下边界
	CV_CROP_ERR_OUTBUF_NOT_ENOUGH,
};

/**
 * 输入图像(裁剪后)转置
 * para: [in]src---裁剪后的图像
 * 		 [out]dst---转置后的图像
 *		 [in]width---src图像对应的宽
 *		 [in]height---src图像对应的高
 * NOTE：支持src地址与dst地址相同
 **/
void in_img_transfer(uint8_t *src, uint8_t *dst, uint32_t width, uint32_t height);

/**
 * 输出图像转置
 * para：[in]src---输入图像数据(拼接后的图像)
 *		 [out]dst---转置后的图像
 *		 [in]width---src图像对应的宽
 *		 [in]height---src图像对应的高
 * NOTE：支持src地址与dst地址相同
 **/
void out_img_transfer(uint8_t *src, uint8_t *dst, uint32_t width, uint32_t height);

/**
 * 拼接图像(竖向拼接)
 * para：[in]in_img---裁剪转置后的图像数据
 *		 [out]ou_img---拼接后的图像(竖向)
 *		 [in]in_size---in_img对应的图像大小
 *		 [out]ou_size---ou_img对应的图像大小
 *		 [inout]last_stitch---上一次拼接后的位置信息
 *		 [in]shift---当前图像基于上一张图像的偏移信息
 *		 [in]in_status---拼接状态，参考枚举ISTITCH_ISTATUS_
 **/
int stitch_img_T(uint8_t *in_img, uint8_t *ou_img, CVSize *in_size, CVSize *ou_size, CVSize *last_stitch, int *shift, int in_status);

/**
 * @brief 横向拼接原图
 * @param in_img 			原图
 * @param ou_img 			拼接后图片
 * @param in_size			单张图片大小
 * @param ou_size			拼接后输出大小
 * @param in_status			拼接状态
 * @return int
 */
int stitch_img_ORI(uint8_t *in_img, uint8_t *ou_img, CVSize *in_size, CVSize *ou_size, int in_status);

#ifdef __cplusplus
}
#endif
#endif