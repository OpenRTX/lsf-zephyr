#ifndef __IVPLATFORM_H__
#define __IVPLATFORM_H__
/*----------------------------------------------+
 |												
 |	ivPlatform.h - Platform Config				
 |												
 |		Copyright (c) 1999-2013, iFLYTEK Ltd.	
 |		All rights reserved.					
 |												
 +----------------------------------------------*/


/*
 *	TODO: 在这里包含目标平台程序需要的公共头文件
 */

#include <stddef.h>
#ifdef _WIN32
#include <stdio.h>
#include <crtdbg.h>
#endif

#if 0
/*
 *	TODO: 根据目标平台特性修改下面的配置选项
 */

#if defined __GNUC__ /*检测gcc类编译器*/
	#pragma message ("GCC compiler")
	#define IV_GNUC_COMPILER        1
		#if defined __i386__ /*检测编译器使用x86指令集*/
			#pragma message ("GCC x86")
			#define IV_GNUC_X86             1
		#elif defined __x86_64__ /*检测编译器使用x86_64指令集*/
			#pragma message ("GCC x86_64")
			#define IV_GNUC_X86_64             1
			#define IV_IS_X64
		#elif defined __arm__ /*检测编译器使用arm指令集，包含arm指令集的Android*/
			#pragma message ("GCC arm")
			#define IV_GNUC_ARM             1
			#if defined __ARM_NEON__
				#pragma message ("GCC arm neon")
				#define IV_GNUC_ARM_NEON        1 /*设定使用arm neon指令优化*/
				#if defined __ANDROID__
					#define __ANDROID_NEON__        1
				#endif
			#endif
		#elif defined __aarch64__ /*检测编译器使用arm指令集，包含arm指令集的Android*/
			#pragma message ("GCC arm 64")
			#define IV_GNUC_ARM            1
			#define IV_IS_X64
			#if defined __ARM_NEON__
				#pragma message ("GCC arm 64 neon")
				#define IV_GNUC_ARM_NEON        1 /*设定使用arm neon指令优化*/
				#if defined __ANDROID__
					#define __ANDROID_NEON__        1
				#endif
			#endif
		#endif
#elif defined _MSC_VER /*检测微软编译器*/
	#pragma message ("Microsoft Visual C++")
	#if defined _WIN32_WCE
		#pragma message ("WinCE")
		#if defined _M_ARM
			#pragma message ("WinCE ARM")
			#define IV_WINCE_ARM	
			#if _M_ARM==7
				#pragma message ("WinCE ARMv7")
				#define IV_WINCE_ARM_NEON
			#elif _M_ARM==6
				#pragma message ("WinCE ARMv6")
				#define IV_WINCE_ARM_V6
			#elif _M_ARM==5
				#pragma message ("WinCE ARMv5")
			#elif _M_ARM==4
				#pragma message ("WinCE ARMv4")
			#endif
		#elif defined _M_MRX000
			#pragma message ("WinCE MIPS")
		#elif defined _M_X86
			#pragma message ("WinCE x86")
		#endif
	#else
		#pragma message ("Windows Desktop")
		#if defined _M_X64
			#define IV_IS_X64
			#pragma message ("Windows Desktop x64")
		#elif defined WIN32
			#define _M_X86
			#pragma message ("Windows Desktop x86")
		#endif
	#endif
#else
	#pragma message ("Other need add more")
	#define IV_OTHER_COMPILER
#endif
#endif

#if defined __ANDROID__ /*检测Android平台，注意这是Android平台宏，不是CPU类型，可能包含arm、mips、x86等多种*/
#define IV_ANDROID             1
#endif

//#define IV_IS_X64

#define IV_UNIT_BITS			8			/* 内存基本单元位数 */
#define IV_BIG_ENDIAN			0			/* 是否是 Big-Endian 字节序.唤醒暂不支持Big-Endian */

#if defined  IV_IS_X64                   		/* 64位机标志 */
   #define IV_PTR_GRID				8			/* 最大指针对齐值 */
#else
   #define IV_PTR_GRID				4			/* 最大指针对齐值 */
#endif

#define IV_PTR_PREFIX						/* 指针修饰关键字(典型取值有 near | far, 可以为空) */
#define IV_CONST				const		/* 常量关键字(可以为空) */
#define IV_EXTERN				extern		/* 外部关键字 */
#define IV_STATIC				static		/* 静态函数关键字(可以为空) */
#define IV_INLINE				//__inline	/* 内联关键字(典型取值有 inline, 可以为空) */
#define IV_CALL_STANDARD		//__stdcall	/* 普通函数修饰关键字(典型取值有 stdcall | fastcall | pascal, 可以为空) */

#define IV_CALL_REENTRANT					/* 递归函数修饰关键字(典型取值有 stdcall | reentrant, 可以为空) */
#define IV_CALL_VAR_ARG						/* 变参函数修饰关键字(典型取值有 cdecl, 可以为空) */

#define IV_TYPE_INT8			char		/* 8位数据类型 */

#define IV_TYPE_INT16			short		/* 16位数据类型 */
#define IV_TYPE_INT24			int			/* 24位数据类型 */
#define IV_TYPE_INT32			int			/* 32位数据类型 */

#define IV_SIZEOF(exp)			(sizeof(exp))

#if defined IV_IS_X64
#define IV_TYPE_INT48           long long/*__int64*/         /* 48位数据类型 */
#define IV_TYPE_INT64           long long/*__int64*/         /* 64位数据类型 */
#define IV_TYPE_ADDRESS		    unsigned long long		/* 地址数据类型 */
#define IV_TYPE_SIZE			unsigned long long		/* 大小数据类型 */
#define IV_ANSI_MEMORY          0               /* 是否使用 ANSI 内存操作库 */
#define IV_ANSI_STRING          0               /* 是否使用 ANSI 字符串操作库 */
#else
#define IV_TYPE_INT48			long long	/* 48位数据类型 */
#define IV_TYPE_INT64			long long	/* 64位数据类型 */
#define IV_TYPE_ADDRESS		    unsigned int		/* 地址数据类型 */
#define IV_TYPE_SIZE			unsigned int		/* 大小数据类型 */

#define IV_ANSI_MEMORY			0			/* 是否使用 ANSI 内存操作库 */
#define	IV_ANSI_STRING			0			/* 是否使用 ANSI 字符串操作库 */
#endif

#define IV_VOLATILE				volatile

#ifdef WIN32
#define IV_ASSERT(exp)			_ASSERT(exp)/* 断言操作(可以为空) */
#else
#define IV_ASSERT(exp)			/* 断言操作(可以为空) */
#endif



#if defined __GNUC__
#define IV_GNUC_COMPILER        1
#endif

#define IV_ARM_COMPILER			0

#if defined _MSC_VER
#define IV_WIN32_COMPILER       1
#endif


#define IV_YIELD                                /* 空闲操作(在协作式调度系统中应定义为任务切换调用, 可以为空) */
#define IV_TTS_ARM_CODECACHE    0               /* 手工代码Cache */

#if defined(DEBUG) || defined(_DEBUG)
	#define IV_DEBUG			1			/* 是否支持调试 */
#else
	#define IV_DEBUG			0			/* 是否支持调试 */
#endif

/* 调试方式下则输出日志 */
#ifndef IV_LOG
    #define IV_LOG              IV_DEBUG        /* 是否输出日志 */
#endif

/* 根据平台编译选项决定是否以 Unicode 方式构建 */
#if defined(UNICODE) || defined(_UNICODE)
	#define IV_UNICODE			0			/* 是否以 Unicode 方式构建 */
#else
	#define IV_UNICODE			0			/* 是否以 Unicode 方式构建 */
#endif

#ifdef _MSC_VER
//编译信息
#define STRING2(x) #x
#define STRING(x) STRING2(x)
#define To_msg(msg)	__FILE__ "(" STRING(__LINE__) ") : "##msg
#define CompileMsg(msg) 	//__pragma(message(To_msg(msg)))
#else
#define CompileMsg(msg)
#endif

#endif /* __IVPLATFORM_H__ */
