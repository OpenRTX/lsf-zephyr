#ifndef _LSOCR_ERR_H_
#define _LSOCR_ERR_H_

typedef enum lsocr_errcode
{
	LSOCR_ERR_OK = 0,
	LSOCR_ERR_INVALID_HANDLE = 1,	//不可用实例
	LSOCR_ERR_INVALID_PARAM = 2,	//不可用参数
	LSOCR_ERR_INVALID_SIZE = 3,		//不正确尺寸
	LSOCR_ERR_INVALID_RES = 4,		//不可用资源
	LSOCR_ERR_MEM_OVERFLOW = 5,		//内存溢出
	LSOCR_ERR_NOT_ALIGN = 6,        //内存未对齐
	LSOCR_ERR_INPUT_SIZE_OVERFLOW = 7,
	LSOCR_ERR_INPUT_DATA = 8,
	LSOCR_ERR_INPUT_FORMAT = 9,
}lsocr_errcode_t;

#endif
