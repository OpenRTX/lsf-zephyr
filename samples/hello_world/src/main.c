/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>
#include <device.h>
#include <drivers/spi.h>

#ifdef CONFIG_SOC_LS_ADSP_CSK6

#include "venus_cp.h"
#include "cmn_iomux_reg_venus.h"
#include "Driver_Common.h"
#include "string.h"
#include "Driver_I2S.h"
#include "IOMuxManager.h"
#include "i2s_venus.h"

static bool g_playdone = 0;
void *g_i2s_out = NULL;

void cb_i2s_oneshot_play_event(uint32_t event_info, uint32_t usr_param)
{
	void *i2s_dev = (void *)usr_param;
	uint8_t event, ch_no;

	event = event_info & 0xFF;
	ch_no = (event_info >> 8) & 0xFF;

	if (event & CSK_I2S_EVENT_TRANSMIT_COMPLETE) {
		g_playdone = 1;
		int32_t tx_cnt = I2S_GetTxCount(i2s_dev, 0x3);
		printk("%s: I2S (ch_no = %d, tx = %d) Play DONE!!\n", __func__, ch_no, tx_cnt);
	}

	if (event & CSK_I2S_EVENT_TX_FIFO_UNDERRUN) {
		printk("%s: I2S TX FIFO Underflow!!\r\n", __func__);
	}

	if (event & CSK_I2S_EVENT_TX_FIFO_EMPTY) {
		printk("%s: I2S TX FIFO Empty!!\r\n", __func__);
	}

	I2S_DEV *i2s = (I2S_DEV *)g_i2s_out;
	i2s->reg->REG_I2S_CFG0.bit.ENABLE = 0;
}

#define CHANNEL_BITS        16
#define SAMPLE_RATE         16000
#define DATA_SIZE           8192
uint8_t test_datas[DATA_SIZE] __attribute__((aligned(32))) = {0};


bool i2s_oneshot_play()
{
	int32_t ret;
	const uint32_t *data;
	uint32_t num;

	data = (const uint32_t *)test_datas;
	num = 16;

	ret = I2S_Control(g_i2s_out, CSK_I2S_TXCH_STEREO_SRC_STEREO, 0);
	printk("I2S_Control., ret:%d\n", ret);
	ret = I2S_Send(g_i2s_out, data, num, CH_BMP_STEREO, I2S_TX_FLAG_START_NOW);
	k_msleep(500);
	printk("I2S_Send., ret:%d\n", ret);
	return (ret == CSK_DRIVER_OK);
}

#else
#define CP_BOOT_ADDRESS 0x60000000
#define SYSCTRL_RESET_CP (0xCAFE000B)
#define SYSCTRL_RELEASE_CP (0xCAFE100B)
#define SYSCTRL_RESET_NPU (0xCAFE000C)
#define CP_PSRAM_BASE (0x68400000)
#define AP_PSRAM_BASE (0x30000000)

#define CP_FW_ADDR (0x18000000 + 0x00200000)
#define CP_FW_LEN 500000

#include "venus_ap.h"
#include "cmn_sysctrl_reg_venus.h"
#include "PSRAMManager.h"
#include <cache.h>

void prepare_cp_run()
{
	PSRAM_Initialize();

	SYSCFG_RegDef *cmn_sys_ctrl = (SYSCFG_RegDef *)CMN_SYSCTRL_BASE;

	// set boot address
	cmn_sys_ctrl->REG_CP_CTRL2.all = CP_BOOT_ADDRESS;
	cmn_sys_ctrl->REG_CP_CTRL1.bit.CP_STAT_VECTOR_SEL = 0x1;

	// stall HIFI4
	cmn_sys_ctrl->REG_CP_CTRL1.bit.CP_HIFI4_RUNSTALL = 0x1;

	// release CP & NPU
	cmn_sys_ctrl->REG_SW_RESET_CP.all = SYSCTRL_RELEASE_CP;
	if (cmn_sys_ctrl->REG_SW_RESET_NPU.all) { // = 1 NPU reset
		cmn_sys_ctrl->REG_SW_RESET_NPU.all = SYSCTRL_RESET_NPU;
	}

	__DMB();
   	memcpy((void *)AP_PSRAM_BASE, (void *)CP_FW_ADDR, CP_FW_LEN);

    sys_cache_data_range((void *)AP_PSRAM_BASE, CP_FW_LEN, K_CACHE_WB);
	__DSB();

	// run cp
	cmn_sys_ctrl->REG_SW_RESET_CP.all = SYSCTRL_RESET_CP;
	cmn_sys_ctrl->REG_SW_RESET_CP.all = SYSCTRL_RELEASE_CP;
	cmn_sys_ctrl->REG_CP_CTRL1.bit.CP_HIFI4_RUNSTALL = 0x0;

	printk("Set CP boot address:%x.\n", CP_BOOT_ADDRESS);
	printk("Boot CP...\n");
}

#endif

void main(void)
{
	printk("Hello World! %s\n", CONFIG_BOARD);
#ifdef CONFIG_SOC_LS_ADSP_CSK6

		uint32_t i;

		/* source */
		for (i = 0; i < sizeof(test_datas); i++) {
			test_datas[i] = i;
		}

		const struct device *dev = device_get_binding("SPI_FAKE");

		if (!device_is_ready(dev)) {
			printk("spi device is not ready.\n");
			return;
		}
		
		struct spi_buf_set buf_set = {0};
		struct spi_buf spi_buf = {0};

		spi_buf.buf = test_datas;
		spi_buf.len = 130;

		buf_set.buffers = &spi_buf;
		buf_set.count = 1;

		spi_write(dev, NULL, &buf_set);
		spi_write(dev, NULL, &buf_set);

#else
	prepare_cp_run();
#endif
}
