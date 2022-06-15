#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <lisa_mem.h>
#include "utils/cv_crop.h"

void in_img_transfer(uint8_t *src, uint8_t *dst, uint32_t width, uint32_t height)
{
	static uint8_t* buffer_tmp = NULL;
	static uint8_t buffer_tmp_size = 0;
	uint32_t i, j, m, n;

	const uint32_t buf_size = width * height * sizeof(uint8_t);
	
	if (buf_size > 0) {
		if (buffer_tmp == NULL) {
			buffer_tmp = (uint8_t*)lisa_mem_alloc(buf_size);
			buffer_tmp_size = buf_size;
		}
		else if (buf_size > buffer_tmp_size)
		{
			buffer_tmp = (uint8_t*)lisa_mem_realloc(buffer_tmp, buf_size);
			buffer_tmp_size = buf_size;
		}
		if (buffer_tmp == NULL) return;
	} else {
		buffer_tmp_size = 0;
		if (buffer_tmp != NULL) {
			lisa_mem_free(buffer_tmp);
			buffer_tmp = NULL;
		}
		return;
	}

	memcpy(buffer_tmp, src, buf_size);
	m = 0;
	for (i = 0; i < height; i++)
	{
		n = i;
		for (j = 0; j < width; j++)
		{
			dst[n] = buffer_tmp[m++];
			n += height;
		}
	}
}

#if 0
static uint8_t g_out_tmp_buf[MAX_OUTPUT_IMAGE_SIZE];
void out_img_transfer(uint8_t *src, uint8_t *dst, uint32_t width, uint32_t height)
{
	int i, j;
	//memset(g_out_tmp_buf, 0, MAX_OUTPUT_IMAGE_SIZE);
	memcpy(g_out_tmp_buf, src, height*width*sizeof(uint8_t));
	for (i = 0; i < height; i++)
	{
		for (j = 0; j < width; j++)
		{
			dst[j*height + i] = g_out_tmp_buf[i*width + j];
		}
	}
}
#endif

int stitch_img_T(uint8_t *in_img, uint8_t *ou_img, CVSize *in_size, CVSize *ou_size, CVSize *last_stitch, int *shift, int in_status)
{
	int i;
	int ret = CV_CROP_NO_ERR;
	const int height = in_size->height;
	const int width  = in_size->width;
	if (NULL == in_img || NULL == ou_img || NULL == ou_size || NULL == last_stitch || NULL == shift)
	{
		return CV_CROP_ERR_INVALID_PARAM;
	}	
	
	if (ISTITCH_ISTATUS_BEGIN_GLOBAL_ == in_status)
	{
		last_stitch->y = 0;
		last_stitch->x = 0 + W_START_POINT;
	}
	else if (ISTITCH_ISTATUS_BEGIN_BATCH_ == in_status)
	{
		last_stitch->y = 0;
	}
	
	if (last_stitch->x < 0)	//y_shift < 0 over bound
	{
		ret = CV_CROP_ERR_OVER_BOUND;
		last_stitch->x = 0;
	}	
	if ((last_stitch->x + width) > MAX_OUTPUT_IMAGE_WIDTH)	//y_shift > 0 over bound
	{
		ret = CV_CROP_ERR_OVER_BOUND;
		last_stitch->x = MAX_OUTPUT_IMAGE_WIDTH - width;
	}

	if ( (last_stitch->y + height) > MAX_OUTPUT_IMAGE_HEIGTH)
	{
		return CV_CROP_ERR_OUTBUF_NOT_ENOUGH;
	}
	
#if 0	
	for (i = 0; i < height; i++)
	{
		int32_t I_offset = i * width;
		int32_t O_offset = (i + last_stitch->y) * MAX_OUTPUT_IMAGE_WIDTH + last_stitch->x;
		memcpy(&ou_img[O_offset], &in_img[I_offset], width);
	}
#else
	int32_t I_offset = 0;
	int32_t O_offset = last_stitch->y * MAX_OUTPUT_IMAGE_WIDTH + last_stitch->x;
	for (i = 0; i < height; i++)
	{
		memcpy(&ou_img[O_offset], &in_img[I_offset], width);
		I_offset += width;
		O_offset += MAX_OUTPUT_IMAGE_WIDTH;
	}
#endif
	
	ou_size->height = last_stitch->y + height;
	ou_size->width  = MAX_OUTPUT_IMAGE_WIDTH;
	last_stitch->y += height;
	last_stitch->x += (int32_t)shift[1]; //w
	
	return ret;
}

#if 0
int stitch_img_ORI(uint8_t *in_img, uint8_t *ou_img, CVSize *in_size, CVSize *ou_size, int in_status)
{
	if (NULL == in_img || NULL == ou_img || NULL == in_size || NULL == ou_size) {
		return CV_CROP_ERR_INVALID_PARAM;
	}

	int i;
	int ret = CV_CROP_NO_ERR;

	// 待拼接的图片宽高
	int width  = in_size->width;
	int height = in_size->height;

	if (ISTITCH_ISTATUS_BEGIN_GLOBAL_ == in_status || ISTITCH_ISTATUS_BEGIN_BATCH_ == in_status) {
		ou_size->width = 0;
		ou_size->height = 0;
	}
	// 已经拼接的图片宽高
	int stitch_width = ou_size->width;
	//int stitch_height = ou_size->height;

#ifdef ORI_STITCH_ROW
	if(stitch_width + width > ORI_MAX_OUTPUT_IMAGE_WIDTH) {
		// 超出边界
		return CV_CROP_ERR_OVER_BOUND;
	}
	if(height > ORI_MAX_OUTPUT_IMAGE_HEIGHT) {
		// 超出边界
		return CV_CROP_ERR_OVER_BOUND;
	}
#else
	if(stitch_height + height > ORI_MAX_OUTPUT_IMAGE_HEIGHT) {
		// 超出边界
		return CV_CROP_ERR_OVER_BOUND;
	}
	if(width > ORI_MAX_OUTPUT_IMAGE_WIDTH) {
		// 超出边界
		return CV_CROP_ERR_OVER_BOUND;
	}
#endif

	int32_t i_start = 0;
	int32_t o_start = 0;
	for (i = 0; i < height; i++)
	{
		i_start = i * width;
#ifdef ORI_STITCH_ROW
		o_start = (i * ORI_MAX_OUTPUT_IMAGE_WIDTH) + stitch_width;
#else
		o_start = (stitch_height * ORI_MAX_OUTPUT_IMAGE_WIDTH) + (i * width);
#endif
		memcpy(ou_img + o_start, in_img + i_start, width);
	}
#ifdef ORI_STITCH_ROW
	ou_size->width  = stitch_width + width;
	ou_size->height = ORI_MAX_OUTPUT_IMAGE_HEIGHT;
#else
	ou_size->width  = ORI_MAX_OUTPUT_IMAGE_WIDTH;
	ou_size->height = stitch_height + height;
#endif
	return ret;
}

#endif
