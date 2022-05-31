#ifndef __LSOCR_DEFINE_H__
#define __LSOCR_DEFINE_H__
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>


typedef char *str_t;
typedef void *ptr_t;
typedef const void *cptr_t;

#define  LSAssert(expr) assert(expr)
#define  MAX(u,v) ((u)>(v)?(u):(v))
#define  MIN(u,v) ((u)<(v)?(u):(v))
#define  GridSize(n)	((size_t)(((size_t)(n)+(32-1))&(~32 + 1)))


#endif
