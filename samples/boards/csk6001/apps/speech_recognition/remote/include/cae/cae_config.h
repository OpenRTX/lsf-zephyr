/**
* @file    cae_config.h
* @brief   I2c interface to access encryption chip  Configuration 
            CAE algorithm 4 or 5 mic radius dimension  Configuration
 *			
 * @version 1.0
 * @date    2015/12/07
 * 
 * @see        
 * 
 * History:
 * index    version        date            author        notes
 * 0        1.0            2015/12/07      kunzhang      Create this file
 */
 #ifndef __CAE_CONFIG_H__
#define __CAE_CONFIG_H__

#define TEST_DMA						(1)
#define TEST_AEC						(1)
#define TEST_MAB						(1)
#define TEST_DRE						(1)
#define TEST_NNAES					(1)
#define TEST_FIX	        				(0)
#define TEST_AGC                    	(1)
#define TEST_DOA					    (1)


#define TEST_BYPASS  (0)
#define TEST_AEC_OFF  (0)
#define TEST_DER_OFF  (0)
#define TEST_NNAES_OFF  (0)
#define TEST_MAB_OFF  (0)
#define TEST_AGC_OFF  (0)

#define DMA_PRINTF_MODULE_TIME		(0)
#define DMA_PRINT_FUNC_TIME				(0)
#define DMA_PRINT_WAIT_TIME				(0)
#define AEC_PRINT_FUNC_TIME				(0)
#define NNAES_ADDRE_ON (1)
#define DER_PRINT_FUNC_TIME				(0)


#define TEST_MVA_WIDTH_BYTE_256				(1)

#define CHANNEL_NUM 2
#define REF_NUM 1
#define REF_NUM2 2
#define BEAM_NUM 3
#define WAKEBEAM_NUM 2
#define WAKEBEAM_RECORDNUM 60
#define GRID_NUM   3
#define NUM_AWPE_REF_CH 1
#define AEC_LEVEL 5
#define LOG_PCM  0
#define MATH_TABLE	(1)
#define F_DELAY   15616
#define MVA_SPEED_UP	(1)

#if MATH_TABLE
#define LOG10_ON        (1)
#define POW_ON          (1)
#define EXP_ON          (1)
#define SQRT_ON        (1)
#define COS_ON			(0)
#define SIN_ON			(0)
#else    
#define LOG10_ON        (0)
#define POW_ON          (0)
#define EXP_ON          (0)
#define SQRT_ON        (0)
#define COS_ON			(0)
#define SIN_ON			(0)
#endif

#define CAE_1 (2)
#define DER_1 (3)

#endif
