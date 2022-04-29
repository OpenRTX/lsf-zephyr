#ifndef __IVW_DEFINES_H__
#define __IVW_DEFINES_H__

#define SIZEOF_CHAR					(1)
#define SIZEOF_SHORT				(2)
#define SIZEOF_INT					(4)

//#define USE_RES_CHECK

#ifdef MLP_RESGROUP
#define MAX_CHANNEL_COUNT (MLP_RESGROUP)
#elif !defined (__GROUP_N_CHANNEL__)
#define MAX_CHANNEL_COUNT (2)
#else
#if(__GROUP_N_CHANNEL__ == 1)
#define MAX_CHANNEL_COUNT (__GROUP_N_CHANNEL__)
#elif (__GROUP_N_CHANNEL__ == 3)
#define MAX_CHANNEL_COUNT (3)
#else
#define MAX_CHANNEL_COUNT (2)
#endif
#endif

#ifdef __GNUC__
#define WIVW_API __attribute__((visibility ("default")))
#elif defined(_MSC_VER) && defined(_DLL)
#define WIVW_API __declspec(dllexport)
#else
#define WIVW_API
#endif

#ifdef SUPPORT_SHUFFLE_ARM_8FRAME
#define IVW_ONCE_FRAME_SIZE         (8)
#elif MLP_GROUP70
#define IVW_ONCE_FRAME_SIZE         (1)
#else
#define IVW_ONCE_FRAME_SIZE         (16)
#endif
#define IVW_MAX_STR_BUF_LEN			(256)
#define IVW_FIXED_WRITE_BUF_LEN_N_FRAMES        (160 * IVW_ONCE_FRAME_SIZE)
#define IVW_FIXED_WRITE_BUF_LEN_1_FRAMES		(320)
#define IVW_FIXED_WRITE_BUF_LEN_2_FRAMES		(320 * 2)
#define IVW_FIXED_WRITE_BUF_LEN_8_FRAMES		(320 * 8)	//fixed write buffer length in bytes. same as NSAMPLE_FRAME_SHIFT
#define IVW_FIXED_WRITE_BUF_LEN_16_FRAMES		(320 * 16)	//fixed write buffer length in bytes. same as NSAMPLE_FRAME_SHIFT
#define IVW_FIXED_WRITE_BUF_LEN_32_FRAMES		(320 * 32)	//fixed write buffer length in bytes. same as NSAMPLE_FRAME_SHIFT

#define IVW_FIXED_WRITE_BUF_LEM_NCHANNEL        (IVW_FIXED_WRITE_BUF_LEN_N_FRAMES * MAX_CHANNEL_COUNT * sizeof(short))


#define  _In
#define  _Out
#define  _Inout
#define  _In_opt
#define  _Out_opt
#define  _Inout_opt

typedef	int					ivBool;		

//typedef signed char			int8_t;
//typedef signed short		int16_t;
//typedef signed int			int32_t;
//typedef unsigned int		size_t;

typedef unsigned long long	ivULongLong;
typedef signed long long	ivLongLong;

typedef	signed char			ivInt8;		/* 8-bit */
typedef	unsigned char		ivUInt8;	/* 8-bit */
typedef	char				ivChar;		/* 8-bit */
typedef	unsigned char		ivUChar;	/* 8-bit */


typedef	signed short		ivInt16;	/* 16-bit */
typedef	unsigned short		ivUInt16;	/* 16-bit */
typedef	signed short		ivShort;	/* 16-bit */
typedef	unsigned short		ivUShort;	/* 16-bit */

typedef	signed int			ivInt32;	/* 32-bit */
typedef	unsigned int		ivUInt32;	/* 32-bit */
typedef	signed int			ivInt;	/* 32-bit */
typedef	unsigned int		ivUInt;	/* 32-bit */
typedef void				ivVoid;

typedef float               ivFloat;  /* 32-bit */

#ifdef IV_TYPE_INT64
typedef	signed long long	ivInt64;	/* 64-bit */
typedef	unsigned long long 	ivUInt64;	/* 64-bit */
#endif

#endif
