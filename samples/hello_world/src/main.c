/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>

#ifdef CONFIG_SOC_LS_ADSP_CSK6

#include "venus_cp.h"
#include "cmn_iomux_reg_venus.h"
#include "Driver_Common.h"
#include "string.h"
#include "Driver_I2S.h"
#include "IOMuxManager.h"

#define I2S2_MCLK 15 // GPIOA, 15, 15
#define I2S2_BCK 16 // GPIOA, 16, 15
#define I2S2_LRCK 17 // GPIOA, 17, 15
#define I2S2_DIN 18 // GPIOA, 18, 15
#define I2S2_DOUT 9 // GPIOA, 9, 15
#define I2S2_FUNC 15 // function 15
#define I2S2_GROUP CSK_IOMUX_PAD_A
#define I2S1_MCLK 6 // GPIOA, 6, 14
#define I2S1_BCK 16 // GPIOA, 16, 14
#define I2S1_LRCK 17 // GPIOA, 17, 14
#define I2S1_DIN 18 // GPIOA, 18, 14
#define I2S1_DOUT 15 // GPIOA, 15, 14
#define I2S1_FUNC 14 // function 14
#define I2S1_GROUP CSK_IOMUX_PAD_A
#define I2S0_MCLK 0 // GPIOB, 0, 15
#define I2S0_MCLK_GROUP CSK_IOMUX_PAD_B
#define I2S0_MCLK_FUNC 14 // function 14 (same as I2S1 MCLK)
#define I2S0_BCK 8 // GPIOB, 8, 15
#define I2S0_LRCK 7 // GPIOB, 7, 15
#define I2S0_DIN 6 // GPIOB, 6, 15
#define I2S0_FUNC 15 // function 15 (except MCLK)
#define I2S0_GROUP CSK_IOMUX_PAD_B
#define I2C0_SCL 4 // GPIOA, 4, 8
#define I2C0_SDA 5 // GPIOA, 5, 8
#define I2C0_FUNC 8 // function 8
#define I2C0_GROUP CSK_IOMUX_PAD_A
#define I2C1_SCL 7 // GPIOA, 7, 9
#define I2C1_SDA 6 // GPIOA, 6, 9
#define I2C1_FUNC 9 // function 9
#define I2C1_GROUP CSK_IOMUX_PAD_A
#define DMIC01_CLK 16 // GPIOA, 16, 17
#define DMIC01_DAT 17 // GPIOA, 17, 17
#define DMIC23_CLK 2 // GPIOA, 2, 17
#define DMIC23_DAT 7 // GPIOA, 7, 17
#define DMIC_FUNC 17 // function 17
#define DMIC_GROUP CSK_IOMUX_PAD_A
static bool g_playdone = 0;

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
}

#define CHANNEL_BITS        16
#define SAMPLE_RATE         16000
#define DATA_SIZE           128
uint8_t test_datas[DATA_SIZE] __attribute__((aligned(32))) = {
	0X31, 0X2e, 0X20, 0Xe5, 0Xad, 0X98, 0Xe5, 0X9c, 0Xa8, 0Xe6, 0X95, 0Xb0, 0Xe6, 0X8d, 0Xae, 0Xe9,
	0X95, 0Xbf, 0Xe5, 0Xba, 0Xa6, 0Xe9, 0X99, 0X90, 0Xe5, 0X88, 0Xb6, 0Xef, 0Xbc, 0X8c, 0X32, 0X35,
	0X35, 0X2a, 0X34, 0Xe5, 0Xad, 0X97, 0Xe8, 0X8a, 0X82, 0X0d, 0X0a, 0X32, 0X2e, 0X20, 0Xe6, 0X95,
	0Xb0, 0Xe6, 0X8d, 0Xae, 0Xe9, 0X95, 0Xbf, 0Xe5, 0Xba, 0Xa6, 0Xe4, 0Xb8, 0X8d, 0Xe8, 0X83, 0Xbd,
	0Xe4, 0Xb8, 0Xba, 0Xe5, 0Xa5, 0X87, 0Xe6, 0X95, 0Xb0, 0Xe3, 0X80, 0X82, 0X0d, 0X0a, 0X33, 0X2e,
	0X20, 0Xe6, 0Xa8, 0Xa1, 0Xe6, 0X8b, 0X9f, 0Xe5, 0X8f, 0X91, 0Xe9, 0X80, 0X81, 0Xe5, 0X87, 0Xba,
	0Xe6, 0X9d, 0Xa5, 0Xe7, 0X9a, 0X84, 0Xe6, 0X95, 0Xb0, 0Xe6, 0X8d, 0Xae, 0Xef, 0Xbc, 0X8c, 0Xe7,
	0X94, 0Xa8, 0X6b, 0X69, 0X6e, 0X67, 0X73, 0X74, 0Xe5, 0X92, 0X8c, 0X64, 0X73, 0X6c, 0X6f, 0X67,
};

void *g_i2s_out = NULL;
bool i2s_oneshot_play()
{
	int32_t ret;
	const uint32_t *data;
	uint32_t num;

	data = (const uint32_t *)test_datas;
	num = DATA_SIZE / 2;

	ret = I2S_Control(g_i2s_out, CSK_I2S_TXCH_STEREO_SRC_STEREO, 0);
	printk("I2S_Control., ret:%d\n", ret);
	k_msleep(500);
	ret = I2S_Send(g_i2s_out, data, num, CH_BMP_STEREO, I2S_TX_FLAG_START_NOW);
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

		uint32_t n;
		n = sizeof(test_datas) / 4;

		/* swap */
		for (i = 0; i < n;) {
			uint8_t t = test_datas[i];
			test_datas[i] = test_datas[i + 1];
			test_datas[i + 1] = t;

			t = test_datas[i + 2];
			test_datas[i + 2] = test_datas[i + 3];
			test_datas[i + 3] = t;
			i += 4;
		}

		g_i2s_out = init_i2s_wm8960_out(true, CH_BMP_LEFT, 0, 1, CHANNEL_BITS, I2S_PROTO_PCMMODE_A, SAMPLE_RATE, cb_i2s_oneshot_play_event);

		// g_i2s_out = init_i2s(cb_i2s_oneshot_play_event, 2);
		if (g_i2s_out == NULL) {
			printk("g_i2s_out is null.\n");
			return;
		}
		k_msleep(100);
		while(1) {
			printk("I2S PLAY\n");
			g_playdone = 0;
			if (!i2s_oneshot_play()) {
				printk("i2s_oneshot_play return error.\n");
				return;
			}
			while (!g_playdone);
			k_msleep(500);
		}
		i2s_oneshot_play();

#else
	prepare_cp_run();
#endif
}
