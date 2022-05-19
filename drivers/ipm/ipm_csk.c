/*
 * Copyright (c) 2018, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT listenai_csk_mailbox

#include <errno.h>
#include <device.h>
#include <string.h>
#include <drivers/ipm.h>
#include "hal/Driver_MBX.h"
#include "hal/reg/cmn_mailbox_reg_venus.h"
#ifdef CONFIG_SOC_LS_ADSP_CSK6
#include "hal/venus_cp.h"
#endif

#ifdef CONFIG_SOC_CSK6
#include "hal/venus_ap.h"
#endif

#define CSK_MBX_CH(ch)            (0x1UL << ch)

#define CSK_IPM_MAX_ID_VAL  15
/* Four data registers are used by default */
#define CSK_MBOX_REGS_MAX_NUM   4
#define CSK_MBOX_SEND_COMPLETE_REG   15
#define CSK_MBOX_SEND_NOTIFY_OFFSET   16
#define CSK_MBOX_SEND_MAX_RETRY   5

static K_SEM_DEFINE(data_sem, 1, 1);
struct csk_mailbox_config {
	MAILBOX_RegDef *mbx_base;
};

struct csk_mailbox_data {
	ipm_callback_t callback;
    uint32_t rx_buf[CSK_MBOX_REGS_MAX_NUM];
	void *user_data;
};


static void csk_mailbox_isr(const struct device *dev)
{
	struct csk_mailbox_data *data = dev->data;
	const struct csk_mailbox_config *config = dev->config;
	uint32_t status = 0;
    uint32_t received_channel = 0; 
#ifdef CONFIG_SOC_CSK6
	status = config->mbx_base->REG_CP_MAILBOX_IRQ.all;
#elif CONFIG_SOC_LS_ADSP_CSK6
	status = config->mbx_base->REG_AP_MAILBOX_IRQ.all;
#endif
	/* send completion notification */
	if(status & CSK_MBX_CH(CSK_MBOX_SEND_COMPLETE_REG)) {  
		k_sem_give(&data_sem);
#ifdef CONFIG_SOC_CSK6
		config->mbx_base->REG_CP_MAILBOX_IRQ.all = CSK_MBX_CH(CSK_MBOX_SEND_COMPLETE_REG);  
#elif CONFIG_SOC_LS_ADSP_CSK6
		config->mbx_base->REG_AP_MAILBOX_IRQ.all = CSK_MBX_CH(CSK_MBOX_SEND_COMPLETE_REG);
#endif
		status &= ~CSK_MBX_CH(CSK_MBOX_SEND_COMPLETE_REG);
	}
	if(status) {  
    	while (status) {
			if (status & 0x01) {
				break;
			}
			received_channel += 1;
			status >>= 1;
    	}
		if(received_channel < CSK_IPM_MAX_ID_VAL) {
			memset(data->rx_buf, 0, sizeof(data->rx_buf));
#ifdef CONFIG_SOC_CSK6				
			data->rx_buf[0] = config->mbx_base->REG_0X030.all;
			data->rx_buf[1] = config->mbx_base->REG_0X034.all;
			data->rx_buf[2] = config->mbx_base->REG_0X038.all;
			data->rx_buf[3] = config->mbx_base->REG_0X03C.all;
#elif CONFIG_SOC_LS_ADSP_CSK6
			data->rx_buf[0] = config->mbx_base->REG_0X010.all;
			data->rx_buf[1] = config->mbx_base->REG_0X014.all;
			data->rx_buf[2] = config->mbx_base->REG_0X018.all;
			data->rx_buf[3] = config->mbx_base->REG_0X01C.all;
#endif

#ifdef CONFIG_SOC_CSK6
			config->mbx_base->REG_CP_MAILBOX_IRQ.all = 0x1U << received_channel;  
			/* Use channel 16 as send completion notification */
			config->mbx_base->REG_AP_MAILBOX_CTRL.all |= 0x1U << (CSK_IPM_MAX_ID_VAL + CSK_MBOX_SEND_NOTIFY_OFFSET);  
#elif CONFIG_SOC_LS_ADSP_CSK6
			config->mbx_base->REG_AP_MAILBOX_IRQ.all = 0x1U << received_channel;  
			config->mbx_base->REG_CP_MAILBOX_CTRL.all |= 0x1U << (CSK_IPM_MAX_ID_VAL + CSK_MBOX_SEND_NOTIFY_OFFSET);  
#endif 
			if (data->callback) {
				data->callback(dev, data->user_data,
							(uint32_t)received_channel,
							data->rx_buf);
			}
		}
	}
}

static void csk_mailbox_config_func(const struct device *dev)
{
	IRQ_CONNECT(DT_INST_IRQN(0),
		    DT_INST_IRQ(0, priority),
		    csk_mailbox_isr, DEVICE_DT_INST_GET(0), 0);
	irq_enable(DT_INST_IRQN(0));
}

static int csk_mailbox_ipm_send(const struct device *dev, int wait, uint32_t id,
			   const void *data, int size)
{
	uint8_t flags;
	int ret;
	int sem_status;
    if (id > CSK_IPM_MAX_ID_VAL) {
		return -EINVAL;
	}

	if (size > CSK_MBOX_REGS_MAX_NUM  * sizeof(uint32_t)) {
		return -EMSGSIZE;
	}

	if (wait == 1){
		k_sem_take(&data_sem, K_FOREVER);
	}
	else{
		sem_status = k_sem_take(&data_sem, K_NO_WAIT);
		if(sem_status == -EBUSY) return sem_status;
	}
	/* xtensa not support irp_lock now */
#ifdef CONFIG_SOC_CSK6
	flags = irq_lock();
#endif

	ret = MBX_Send(NULL, data, id);
	
#ifdef CONFIG_SOC_CSK6
	irq_unlock(flags);
#endif
	if (ret != CSK_DRIVER_OK) return -EIO;
	return 0;
}

static int csk_mailbox_ipm_max_data_size_get(const struct device *dev)
{
	ARG_UNUSED(dev);
    return CSK_MBOX_REGS_MAX_NUM *sizeof(uint32_t);
}

static uint32_t csk_mailbox_ipm_max_id_val_get(const struct device *dev)
{
    ARG_UNUSED(dev);
	return CSK_IPM_MAX_ID_VAL;
}

static void csk_mailbox_ipm_register_callback(const struct device *dev,
					 ipm_callback_t cb,
					 void *user_data)
{
	struct csk_mailbox_data *driver_data = dev->data;
	driver_data->callback = cb;
	driver_data->user_data = user_data;
}

static int csk_mailbox_ipm_set_enabled(const struct device *dev, int enable)
{
	ARG_UNUSED(dev);
    ARG_UNUSED(enable);
	return 0;
}

static int csk_mailbox_init(const struct device *dev)
{
	MBX_Initialize(NULL, NULL);
	csk_mailbox_config_func(dev);
	return 0;
}

static const struct ipm_driver_api csk_mailbox_driver_api = {
	.send = csk_mailbox_ipm_send,
	.register_callback = csk_mailbox_ipm_register_callback,
	.max_data_size_get = csk_mailbox_ipm_max_data_size_get,
	.max_id_val_get = csk_mailbox_ipm_max_id_val_get,
	.set_enabled = csk_mailbox_ipm_set_enabled
};


static const struct csk_mailbox_config csk_mailbox_0_config = {
	.mbx_base = (MAILBOX_RegDef *)DT_INST_REG_ADDR(0),
};

static struct csk_mailbox_data csk_mailbox_0_data;

DEVICE_DT_INST_DEFINE(0,
		    &csk_mailbox_init,
		    NULL,
		    &csk_mailbox_0_data, &csk_mailbox_0_config,
		    PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
		    &csk_mailbox_driver_api);
