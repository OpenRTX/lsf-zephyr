#ifndef __BASIC_DEFINES_H__
#define __BASIC_DEFINES_H__


#ifdef  IVW_DUAL_CHANNEL
#define IVW_CHN_NUM   2
#else
#define IVW_CHN_NUM   1
#endif

#define  MD5_LEN				     (32)
#define  MAX_STATE_COUNT_ARC		 (2)
#define  W_LOG_ZERO					(-0x3FFFFFFF)

#define ivMax(a,b)  (((a)>(b))?(a):(b))
#define ivMin(a,b)  (((a)<(b))?(a):(b))

#ifdef _WIN32
#include <assert.h>
//#define IV_ASSERT(exp)			_ASSERT(exp)
#define IV_ASSERT(exp)			assert(exp)
// #define snprintf _snprintf
#else
#define IV_ASSERT(exp)			
#define HMODULE	void*
#endif


#define STRING2(x) #x
#define STRING(x) STRING2(x)
#define MLP_ALIGN_SIZE_32			(32)

#ifdef _MSC_VER
// For The Compile Infomation
#define To_msg(msg)	__FILE__ "(" STRING(__LINE__) ") : "##msg
#define CompileMsg(msg) 	__pragma(message(To_msg("#######"##msg##"######")))
#else
#define CompileMsg(msg)
#endif

#define RDeclareApi(func)		extern Proc_##func  func##_;
#define DeclareApi(func)		Proc_##func  func##_;

#define GetProcApi(func)\
	{\
	func##_ = (Proc_##func)GetProcAddress(hand, #func);\
	if(func##_ == NULL)\
		{\
		sglog_error_assert_return(0, ("GetProcAddress | err, %s = NULL", #func), -1);\
		}\
	}
#define GetProcApiVar(func)\
	{\
	func##_ = (Proc_##func)( func);\
	}

#ifdef _MSC_VER
typedef unsigned int bool;
#	define true		1
#	define false	0
#else
#	include	<stdbool.h>
#endif

#endif
