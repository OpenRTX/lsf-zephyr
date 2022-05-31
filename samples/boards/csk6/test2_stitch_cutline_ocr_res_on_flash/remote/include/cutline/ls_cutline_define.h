
#ifndef _LS_CUTLINE_DEFINE_H_
#define _LS_CUTLINE_DEFINE_H_

/*
 *	数据类型
 */
typedef signed char    i8_t;
typedef unsigned char  u8_t;
typedef signed short   i16_t;
typedef unsigned short u16_t;
typedef signed int	   i32_t;
typedef unsigned int   u32_t;
typedef float		   f32_t;
typedef signed long long   i64_t;
typedef unsigned long long u64_t;

/* 内存基本单元 */
typedef	u8_t		byte_t;

/* 字符 */
//typedef i8_t * str_t;

/* 边界值定义 */
#define LS_SBYTE_MAX	(+127)
#define LS_MAX_INT8	    (+127)
#define LS_MAX_INT16	(+32767)
#define LS_MAX_UINT16	(+65535)
#define LS_INT_MAX		(+8388607L)
#define LS_MAX_INT32	(+2147483647L)
#define LS_MAX_UINT32   (+4294967295L)
#define LS_MIN_UINT32   (+0L)

#define LS_SBYTE_MIN	(-LS_SBYTE_MAX - 1)
#define LS_MIN_INT8	    (-LS_MAX_INT8 - 1)
#define LS_MIN_INT16	(-LS_MAX_INT16 - 1)
#define LS_INT_MIN		(-LS_INT_MAX - 1)
#define LS_MIN_INT32	(-LS_MAX_INT32 - 1)

#define LS_BYTE_MAX		(0xffU)
#define LS_USHORT_MAX	(0xffffU)
#define LS_UINT_MAX		(0xffffffUL)
#define LS_ULONG_MAX	(0xffffffffUL)

#endif /* _LS_CUTLINE_DEFINE_H_ */
