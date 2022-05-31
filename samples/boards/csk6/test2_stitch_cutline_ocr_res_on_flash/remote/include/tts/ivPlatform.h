/*----------------------------------------------------------------+
 |                                                                |
 |    ivPlatform.h - Platform Config                              |
 |                                                                |
 |        Platform: Win32                                         |
 |                                                                |
 |        Copyright (c) 1999-2013, ANHUI USTC iFLYTEK CO.,LTD.    |
 |        All rights reserved.                                    |
 |                                                                |
 +----------------------------------------------------------------*/

/*
 *    TODO: ���������Ŀ��ƽ̨������Ҫ�Ĺ���ͷ�ļ�
 */
#ifdef WIN32
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <crtdbg.h>
#include <windows.h>
#endif
/*
 *    TODO: ����Ŀ��ƽ̨�����޸����������ѡ��
 */

#define IV_UNIT_BITS            8               /* �ڴ������Ԫλ�� */
#define IV_BIG_ENDIAN           0               /* �Ƿ��� Big-Endian �ֽ��� */
#define IV_PTR_GRID             4               /* ���ָ�����ֵ */

#define IV_PTR_PREFIX                           /* ָ�����ιؼ���(����ȡֵ�� near | far, ����Ϊ��) */
#define IV_CONST                const           /* �����ؼ���(����Ϊ��) */
#define IV_EXTERN               extern          /* �ⲿ�ؼ��� */
#define IV_STATIC               static          /* ��̬�����ؼ���(����Ϊ��) */

#ifdef WIN32
#define IV_INLINE               __inline        /* �����ؼ���(����ȡֵ�� inline, ����Ϊ��) */
#define IV_CALL_STANDARD        __stdcall       /* ��ͨ�������ιؼ���(����ȡֵ�� stdcall | fastcall | pascal, ����Ϊ��) */
#define IV_CALL_REENTRANT       __stdcall       /* �ݹ麯�����ιؼ���(����ȡֵ�� stdcall | reentrant, ����Ϊ��) */
#define IV_CALL_VAR_ARG         __cdecl         /* ��κ������ιؼ���(����ȡֵ�� cdecl, ����Ϊ��) */
#else
#define IV_INLINE							/* �����ؼ���(����ȡֵ�� inline, ����Ϊ��) */
#define IV_CALL_STANDARD					/* ��ͨ�������ιؼ���(����ȡֵ�� stdcall | fastcall | pascal, ����Ϊ��) */
#define IV_CALL_REENTRANT					/* �ݹ麯�����ιؼ���(����ȡֵ�� stdcall | reentrant, ����Ϊ��) */
#define IV_CALL_VAR_ARG						/* ��κ������ιؼ���(����ȡֵ�� cdecl, ����Ϊ��) */
#endif

#define IV_TYPE_INT8            char            /* 8λ�������� */
#define IV_TYPE_INT16           short           /* 16λ�������� */
#define IV_TYPE_INT24           int             /* 24λ�������� */
#define IV_TYPE_INT32           int             /* 32λ�������� */

#if  defined(WIN32) && !defined(x64)  /* 48/64 λ���������ǿ�ѡ��, ��Ǳ�Ҫ��Ҫ����, ��ĳЩ 32 λƽ̨��, ʹ��ģ�ⷽʽ�ṩ�� 48/64 λ������������Ч�ʺܵ� */
#define IV_TYPE_INT48           __int64         /* 48λ�������� */
#define IV_TYPE_INT64           __int64         /* 64λ�������� */

#define IV_TYPE_ADDRESS         size_t          /* ��ַ�������� */
#define IV_TYPE_SIZE            size_t          /* ��С�������� */

#define IV_ANSI_MEMORY          1               /* �Ƿ�ʹ�� ANSI �ڴ������ */
#define IV_ANSI_STRING          1               /* �Ƿ�ʹ�� ANSI �ַ��������� */

#define IV_ASSERT(exp)          _ASSERT(exp)    /* ���Բ���(����Ϊ��) */

#elif defined(WINCE) 

#define IV_TYPE_INT48           __int64         /* 48λ�������� */
#define IV_TYPE_INT64           __int64         /* 64λ�������� */

#define IV_TYPE_ADDRESS         unsigned int          /* ��ַ�������� */
#define IV_TYPE_SIZE            unsigned int          /* ��С�������� */

#define IV_ANSI_MEMORY          1               /* �Ƿ�ʹ�� ANSI �ڴ������ */
#define IV_ANSI_STRING          1               /* �Ƿ�ʹ�� ANSI �ַ��������� */

#define IV_ASSERT(exp)          /*_ASSERT(exp)*/    /* ���Բ���(����Ϊ��) */

#else
#define IV_TYPE_INT48			long long	/* 48λ�������� */
#define IV_TYPE_INT64			long long	/* 64λ�������� */

#define IV_TYPE_ADDRESS			unsigned long long		/* ��ַ�������� */
#define IV_TYPE_SIZE			unsigned long long		/* ��С�������� */

#define IV_ANSI_MEMORY			0			/* �Ƿ�ʹ�� ANSI �ڴ������ */
#define	IV_ANSI_STRING			0			/* �Ƿ�ʹ�� ANSI �ַ��������� */

#define IV_ASSERT(exp)			/* ���Բ���(����Ϊ��) */
#endif

#define IV_YIELD                                /* ���в���(��Э��ʽ����ϵͳ��Ӧ����Ϊ�����л�����, ����Ϊ��) */
#define IV_TTS_ARM_CODECACHE    0               /* �ֹ�����Cache */

/* ����ƽ̨����ѡ������Ƿ�֧�ֵ��� */
#if defined(DEBUG) || defined(_DEBUG)
#define IV_DEBUG                1               /* �Ƿ�֧�ֵ��� */
#else
#define IV_DEBUG                0               /* �Ƿ�֧�ֵ��� */
#endif

/* ���Է�ʽ���������־ */
#ifndef IV_LOG
    #define IV_LOG              0        /* �Ƿ������־ */
#endif

/* �ں�֧��Unicode����ҳ����Ҫ����ΪUnicode�� */
#if defined(UNICODE) || defined(_UNICODE)
    #define IV_UNICODE          1               /* �Ƿ��� Unicode ��ʽ���� */
#else
    #define IV_UNICODE          0               /* �Ƿ��� Unicode ��ʽ���� */
#endif

