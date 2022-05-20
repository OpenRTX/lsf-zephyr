#include <init.h>
#include <arch/cpu.h>

#include "soc.h"
#include "autoconf.h"
#include "hal/venus_ap.h"
#include "hal/reg/cmn_sysctrl_reg_venus.h"

#define SYSTEM_CLK ((SYSCFG_RegDef *)CMN_SYSCTRL_BASE)

volatile static const char __attribute__((section(".vectors")))
venus_system_tag[] = { 0x48, 0x72, 0x01, 0x00, 0xA4, 0x52, 0x08, 0x00, 0x00, 0x00, 0x00, 0x18, 0x01,
		       0x00, 0x00, 0x00, 0x56, 0x45, 0x4E, 0x55, 0x53, 0x5F, 0x52, 0x54, 0x4F, 0x53,
		       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x05, 0xD7, 0x2C };

#ifdef CONFIG_HAS_CSK6_GPT
static void GPT_Init_Handler(void)
{
	IP_SYSCTRL->REG_MISC_CLK_CFG1.bit.DIV_GPT_CLKS = 0x2;
	IP_SYSCTRL->REG_MISC_CLK_CFG1.bit.DIV_GPT_CLKS_LD = 0x1;
	IP_SYSCTRL->REG_MISC_CLK_CFG1.bit.DIV_GPT_CLKT0 = 0x2;
	IP_SYSCTRL->REG_MISC_CLK_CFG1.bit.DIV_GPT_CLKT0_LD = 0x1;
	IP_SYSCTRL->REG_MISC_CLK_CFG1.bit.ENA_GPT_CLKS = 0x1;
	IP_SYSCTRL->REG_MISC_CLK_CFG1.bit.ENA_GPT_CLKT0 = 0x1;
}
#endif

static int csk6_init(const struct device *arg)
{
	extern void BootClock_Init();

#if CONFIG_USB_DC_CSK6
	SYSTEM_CLK->REG_MISC_CLK_CFG2.bit.ENA_USBC_CLK = 0x01;
#endif

	BootClock_Init();
	outw(ABB_BASE, inw(ABB_BASE)); //write 0 to bit16
	SCB_EnableICache();
	SCB_EnableDCache();
	/*TODO: will rewrite later*/
#ifdef CONFIG_HAS_CSK6_GPT
	GPT_Init_Handler();
#endif

#if CONFIG_CSK6_PSRAM
	extern int32_t PSRAM_Initialize(void);
	PSRAM_Initialize();
#endif
	return 0;
}

SYS_INIT(csk6_init, PRE_KERNEL_1, 0);