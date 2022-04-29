/*
 * luna.h
 *
 *  Created on: Aug 18, 2017
 *      Author: dwwang
 */

#ifndef __LUNA_LUNA_H__
#define __LUNA_LUNA_H__

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

#include "luna_error.h"
#include "luna_bits.h"
#include "luna_math_types.h"

#define LUNA_VER_MAJOR 3
#define LUNA_VER_MINOR 0
#define LUNA_VER_PATCH 0
#define LUNA_VER_BUILD 0
#define LUNA_VERSION   ( (LUNA_VER_MAJOR << 24) + (LUNA_VER_MINOR << 16) +  (LUNA_VER_PATCH << 8)  +  (LUNA_VER_BUILD << 0) )

#define LUNA_SHARE_MEM_AHB_BASE			(0x48000000)
#define LUNA_SHARE_MEM_AHB_BASE_MASK	(0x480FFFFF)
#define LUNA_SHARE_MEM_BASE				(0x5FE00000)
#define LUNA_PSRAM_MEM_BASE             (0x60000000)

#define LUNA_WIDTH_BYTE_MG		(128)
#define LUNA_WIDTH_BYTE_32		(32/8)  //32bits width
#define LUNA_WIDTH_BYTE_64		(64/8)  //64bits width
#define LUNA_WIDTH_BYTE_IO_256	(256/8)  //256bits IO read width
#define LUNA_WIDTH_BYTE_IO_512	(512/8)  //512bits IO read width
#define LUNA_WIDTH_BYTE_MG_128	(128/8)  //128bits single MG write width
#define LUNA_WIDTH_BYTE_MG_256	(256/8)  //128bits single MG write width
#define LUNA_WIDTH_BYTE_MG_512	(512/8)  //128bits single MG write width

#define __luna_attr__ 			//__attribute__ ((aligned(32))) 	__attribute__ ((section (".SHARE_SECTION")))
#define __luna_attr_align4__ 	//__attribute__ ((aligned(4))) 	__attribute__ ((section (".SHARE_SECTION")))

#define _FAST_FUNC_RO           __attribute__ ((section (".sharedmem.text"))) 	//code in shared memory
#define _FAST_DATA_VI           __attribute__ ((section (".sharedmem.data"))) 	//initialized data in shared memory
#define _FAST_DATA_ZI           __attribute__ ((section (".sharedmem.bss"))) 	//zero initialized data in shared memory

//#define USE_SHAREMEM_CMD 1
#if USE_SHAREMEM_CMD
#define __luna_cmd_attr__		const _FAST_FUNC_RO
#define __luna_param_attr__
#else
#define __luna_cmd_attr__		const
#define __luna_param_attr__
#endif
//#define LUNA_LOOP_LIST
#define MAX_NODE_NUM	30

/*******************  luna function  ********************/

unsigned int reg_read(unsigned int addr);
void reg_write(unsigned int addr, unsigned int data);
void luna_print_regs();

void luna_init(void);
void luna_int_handler();
int32_t luna_execute(const uint32_t *api, void* param);
int32_t luna_execute_looplist(const uint32_t *api, void* param, uint32_t node_num);
int32_t luna_execute_cmd(const uint32_t *api, void* param, uint32_t param_size);
void luna_looplist_init(uint32_t *para_buf, uint32_t size);
void luna_looplist_start();
uint32_t luna_get_node_list_num();

void start_counter();
uint32_t get_counter();

#define LUNA_LOG printf
#define LUNA_PRINTF printf

#endif /* __LUNA_LUNA_H__ */
