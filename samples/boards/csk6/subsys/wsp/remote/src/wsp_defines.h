#ifndef __WSP_DEFINES_H__
#define __WSP_DEFINES_H__

#ifdef __GNUC__
#define LS_WSP_API __attribute__((visibility ("default")))
#elif defined(_MSC_VER) && defined(_DLL)
#define LS_CSP_API __declspec(dllexport)
#else
#define LS_CSP_API
#endif

#define  _In     const
#define  _Out
#define  _Inout

typedef	int					ivBool;		

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
