#ifndef __LS_TRANSLATION_TYPE_H__
#define __LS_TRANSLATION_TYPE_H__

#include<stdint.h>

#define mem_addr_t mem_addr_t_

typedef void* lstrans_inst;

typedef struct {
	char*    addr;
	uint32_t size;
} mem_addr_t;


typedef struct {
	char*	  str;
	uint32_t size;
} data_t;

typedef struct {
	mem_addr_t dict;
	mem_addr_t embedding;
	mem_addr_t encoder;
	mem_addr_t decoder;
} res_addr;
typedef struct _lstrans_mem
{
	mem_addr_t share;           /*indicate share memory address and size*/
	mem_addr_t psram;			/*indicate psram address and size*/
	res_addr   res;				/*indicate resource address and size in flash*/
								/*res: address and length of each resource*/
}lstrans_mem_t;

typedef enum lstrans_errcode
{
	LSTRANS_OK = 0,
	LSTRANS_INVALID_HANDLE,
	LSTRANS_INVALID_PARAM,
	LSTRANS_INVALID_RES,
	LSTRANS_MEM_OVERFLOW,
	LSTRANS_NOT_ALIGN,
	LSTRANS_INPUT_OVERFLOW,
	LSTRANS_OUTPUT_OVERFLOW,
	LSTRANS_INPUT_FORMAT,
}lstrans_errcode_e;

typedef enum
{
	CallBackAsynResult = 0,  /*Asynchronous translation result*/
	CallBackCount
}lstans_cb_type_e;


typedef int32_t (*PLSTransCallBack)(const char* pParam, void *pUserParam);

#endif