#include <init.h>
#include <arch/cpu.h>

#include "soc.h"
#include "autoconf.h"
#include "hal/venus_ap.h"
#include "hal/reg/cmn_sysctrl_reg_venus.h"
#include "hal/gpio.h"
#include "hal/IOMuxManager.h"

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

/**
 * @brief  Make sure the two boot pins are pull up. Avoid the mcu reboot to rom_boot.
 */
static void set_boot_pins_to_high(void)
{
	int boot0_pin = 0;
	int boot1_pin = 1;
	int boot_port = CSK_IOMUX_PAD_B;

	/* make sure PB0 and PB1 are input PULL_UP */
	GPIO_SetDir(GPIOB(), 1UL << boot0_pin, CSK_GPIO_DIR_INPUT);
	GPIO_SetDir(GPIOB(), 1UL << boot1_pin, CSK_GPIO_DIR_INPUT);
	IOMuxManager_ModeConfigure(boot_port, boot0_pin, HAL_IOMUX_PULLUP_MODE);
	IOMuxManager_ModeConfigure(boot_port, boot1_pin, HAL_IOMUX_PULLUP_MODE);
}


/* Overrides the weak ARM implementation:
   Set general purpose retention register and reboot */
void sys_arch_reboot(int type)
{
	ARG_UNUSED(type);

	set_boot_pins_to_high();

	/* Fix a issue in 32M flash */
#if (CONFIG_FLASH_WRITE_BLOCK_SIZE == 4)
	extern int csk_flash_clear_add4(void);
	csk_flash_clear_add4();
#endif

	NVIC_SystemReset();
}


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