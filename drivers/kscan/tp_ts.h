
/*****************************************************************************
*  Copyright Statement:
*  --------------------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of MediaTek Inc. (C) 2005
*
*  BY OPENING THIS FILE, BUYER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
*  THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
*  RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO BUYER ON
*  AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
*  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
*  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
*  NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
*  SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
*  SUPPLIED WITH THE MEDIATEK SOFTWARE, AND BUYER AGREES TO LOOK ONLY TO SUCH
*  THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. MEDIATEK SHALL ALSO
*  NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE RELEASES MADE TO BUYER'S
*  SPECIFICATION OR TO CONFORM TO A PARTICULAR STANDARD OR OPEN FORUM.
*
*  BUYER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND CUMULATIVE
*  LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
*  AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
*  OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY BUYER TO
*  MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
*
*  THE TRANSACTION CONTEMPLATED HEREUNDER SHALL BE CONSTRUED IN ACCORDANCE
*  WITH THE LAWS OF THE STATE OF CALIFORNIA, USA, EXCLUDING ITS CONFLICT OF
*  LAWS PRINCIPLES.  ANY DISPUTES, CONTROVERSIES OR CLAIMS ARISING THEREOF AND
*  RELATED THERETO SHALL BE SETTLED BY ARBITRATION IN SAN FRANCISCO, CA, UNDER
*  THE RULES OF THE INTERNATIONAL CHAMBER OF COMMERCE (ICC).
*
*****************************************************************************/

/*****************************************************************************
 *
 * Filename:
 * ---------
 *   Capacitive_TP_Cypress_CY8CTMA340.h
 *
 *
 * Description:
 * ------------
 *   Capacitive Touch Panel I2C read/write Driver header file
 *
 * Author:
 * -------
 * -------
 *
 *============================================================================
 *             HISTORY
 * Below this line, this part is controlled by PVCS VM. DO NOT MODIFY!!
 *------------------------------------------------------------------------------
 * removed!
 * removed!
 * removed!
 *
 * removed!
 * removed!
 * removed!
 *
 * removed!
 * removed!
 * removed!
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by PVCS VM. DO NOT MODIFY!!
 *============================================================================
 *****************************************************************************/
#ifndef _BL_TS_H_ 
#define _BL_TS_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <zephyr.h>
#include <device.h>
#include "bl_chip_common.h"
#include "fw_update.h"

#include "kscan_bl6xxx.h"


extern void ctp_delay_us(unsigned int time);
extern void ctp_delay_ms(unsigned short time);

#define   MDELAY(n)	    ctp_delay_ms(n)	
#define   UDELAY(n)	    ctp_delay_us(n)

#ifdef   BL_POWER_CONTROL_SUPPORT
#define	CTP_LDO_PIN		GPIO10
#define CTP_SET_LDO_OUTPUT	    gpio_set_direction(CTP_LDO_PIN,GPIO_OUT)
#define CTP_SET_LDO_HIGH		GPIO_WriteIO(CTP_LDO_PIN,1)
#define CTP_SET_LDO_LOW		    GPIO_WriteIO(CTP_LDO_PIN,0)
#endif

#ifdef RESET_PIN_WAKEUP
#define	CTP_RESET_PIN		GPIO19
#define CTP_SET_RESET_PIN_OUTPUT	gpio_set_direction(CTP_RESET_PIN,GPIO_OUT);
#define CTP_SET_RESET_PIN_HIGH		gpio_output_set(CTP_RESET_PIN, 1)
#define CTP_SET_RESET_PIN_LOW		gpio_output_set(CTP_RESET_PIN, 0)
#endif

#ifdef   GPIO_EINT
#define  CTP_EINT_PIN			GPIO16
#define CTP_SET_I2C_EINT_OUTPUT		gpio_set_direction(CTP_EINT_PIN,GPIO_OUT)
#define CTP_SET_I2C_EINT_INPUT		gpio_set_direction(CTP_EINT_PIN,GPIO_IN)
#define CTP_SET_I2C_EINT_HIGH		gpio_output_set(CTP_EINT_PIN,1)
#define CTP_SET_I2C_EINT_LOW		gpio_output_set(CTP_EINT_PIN,0)	
#endif

#ifdef   CTP_USE_SW_I2C
#define  CTP_I2C_DELAY			50
#define CTP_I2C_DATA_PIN		GPIO11 
#define CTP_I2C_CLK_PIN			GPIO10 

#define CTP_WRITE		CTP_SLAVE_ADDR
#define CTP_READ		(CTP_SLAVE_ADDR+1)
#define CTP_ACK_COUNTER	10

#define CTP_SET_I2C_CLK_OUTPUT		gpio_set_direction(CTP_I2C_CLK_PIN,GPIO_OUT)
#define CTP_SET_I2C_CLK_HIGH		gpio_output_set(CTP_I2C_CLK_PIN,1)
#define CTP_SET_I2C_CLK_LOW			gpio_output_set(CTP_I2C_CLK_PIN,0)

#define CTP_SET_I2C_DATA_OUTPUT		gpio_set_direction(CTP_I2C_DATA_PIN,GPIO_OUT)
#define CTP_SET_I2C_DATA_INPUT		gpio_set_direction(CTP_I2C_DATA_PIN,GPIO_IN)
#define CTP_SET_I2C_DATA_HIGH		gpio_output_set(CTP_I2C_DATA_PIN,1)
#define CTP_SET_I2C_DATA_LOW		gpio_output_set(CTP_I2C_DATA_PIN,0)
#define CTP_GET_I2C_DATA_BIT		gpio_input_get(CTP_I2C_DATA_PIN)

#define CTP_I2C_START_BIT \
	{ \
		volatile unsigned int j; \
		CTP_SET_I2C_CLK_OUTPUT; \
		CTP_SET_I2C_DATA_OUTPUT; \
		CTP_SET_I2C_CLK_HIGH; \
		CTP_SET_I2C_DATA_HIGH; \
		for(j=0;j<CTP_I2C_DELAY;j++);\
		CTP_SET_I2C_DATA_LOW; \
		for(j=0;j<CTP_I2C_DELAY;j++);\
		CTP_SET_I2C_CLK_LOW; \
	}

	#define CTP_I2C_STOP_BIT \
	{ \
		volatile unsigned int j; \
		CTP_SET_I2C_CLK_OUTPUT; \
		CTP_SET_I2C_DATA_OUTPUT; \
		CTP_SET_I2C_CLK_LOW; \
		CTP_SET_I2C_DATA_LOW; \
		for(j=0;j<CTP_I2C_DELAY;j++);\
		CTP_SET_I2C_CLK_HIGH; \
		for(j=0;j<CTP_I2C_DELAY;j++);\
		CTP_SET_I2C_DATA_HIGH; \
		for(j=0;j<CTP_I2C_DELAY;j++);\
	}
#endif

struct ctp_bl6133_info {
    int irq_pin;
    int rst_pin;
    int i2c_addr;
};

// Lisiqiang 20191212 +++ // �Ż� TP ��λAssert ���� 
#if defined(__EIOT_MMI_SUPPORT__)
typedef struct _tagExtDataInfo {	
    uint8_t ucType ; 	
    struct ctp_bl6133_info* pInfo ; 
} EXT_DATA_INFO, *PEXT_DATA_INFO ; 
#endif // __EIOT_MMI_SUPPORT__// Lisiqiang 20191212 --- int bl6133_init(const struct ft6x36_info *info);
// Lisiqiang 20191212 --- 

extern int CTP_FLASH_I2C_WRITE(unsigned char i2c_addr, unsigned char *value, unsigned short len);
extern int CTP_FLASH_I2C_READ(unsigned char i2c_addr, unsigned char *value, unsigned short len);
// extern int ctp_bl_ts_init(const struct ctp_bl6133_info *info);

extern uint8_t bl6xxx_chip_get_id(uint8_t *buf, bl6xxx_dev_data_t *data);

#if(UPDATE_MODE==I2C_UPDATE_MODE)
void bl_enter_update_with_i2c(void);
void bl_exit_update_with_i2c(void);
#endif
#if(UPDATE_MODE==INT_UPDATE_MODE)
extern void bl_enter_update_with_int(void);
extern void bl_exit_update_with_int(void);
#endif
#ifdef  RESET_PIN_WAKEUP
void bl_ts_reset_wakeup(void);
#endif

#if(UPDATE_MODE==I2C_UPDATE_MODE)
#define   SET_WAKEUP_HIGH    bl_exit_update_with_i2c()	 
#define   SET_WAKEUP_LOW	 bl_enter_update_with_i2c()	 
#endif

#if(UPDATE_MODE==INT_UPDATE_MODE)
#define   SET_WAKEUP_HIGH    bl_exit_update_with_int()	 
#define   SET_WAKEUP_LOW	 bl_enter_update_with_int()	 
#endif
#endif
