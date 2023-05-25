#include <init.h>
#include <arch/cpu.h>

#include "soc.h"
#include "autoconf.h"
#include "venus_ap.h"
#include "reg/cmn_sysctrl_reg_venus.h"
#include "gpio.h"
#include "IOMuxManager.h"

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


/**
 * @brief  External operation for csk6, Make sure the csk6 reboot 
 *         all other peripheral except arm core successfully
 */
static void csk6_reboot_external(void)
{
	IP_SYSCTRL->REG_AP_CTRL1.bit.AP_ENA_RESETREQ = 1;
}

	

/* Overrides the weak ARM implementation:
   Set general purpose retention register and reboot */
void sys_arch_reboot(int type)
{
	ARG_UNUSED(type);

	set_boot_pins_to_high();

	/* Fix a issue in 32M flash */
	extern int csk_flash_clear_add4(void);
	csk_flash_clear_add4();

	csk6_reboot_external();
	NVIC_SystemReset();
}


static int csk6_init(const struct device *arg)
{
	/*TODO: will rewrite later*/
#ifdef CONFIG_HAS_CSK6_GPT
	GPT_Init_Handler();
#endif

#ifdef CONFIG_CSK6_ICACHE
	SCB_EnableICache();
#else
	SCB_DisableICache();
#endif
#ifdef CONFIG_CSK6_DCACHE
	SCB_EnableDCache();
#else
	SCB_DisableDCache();
#endif

	return 0;
}


#if defined(CONFIG_CPU_HAS_FPU)
static inline void z_arm_floating_point_init(void)
{
	/*
	 * Upon reset, the Co-Processor Access Control Register is, normally,
	 * 0x00000000. However, it might be left un-cleared by firmware running
	 * before Zephyr boot.
	 */
	SCB->CPACR &= (~(CPACR_CP10_Msk | CPACR_CP11_Msk));

#if defined(CONFIG_FPU)
	/*
	 * Enable CP10 and CP11 Co-Processors to enable access to floating
	 * point registers.
	 */
#if defined(CONFIG_USERSPACE)
	/* Full access */
	SCB->CPACR |= CPACR_CP10_FULL_ACCESS | CPACR_CP11_FULL_ACCESS;
#else
	/* Privileged access only */
	SCB->CPACR |= CPACR_CP10_PRIV_ACCESS | CPACR_CP11_PRIV_ACCESS;
#endif /* CONFIG_USERSPACE */
	__ISB();
	/*
	 * Upon reset, the FPU Context Control Register is 0xC0000000
	 * (both Automatic and Lazy state preservation is enabled).
	 */
#if defined(CONFIG_MULTITHREADING) && !defined(CONFIG_FPU_SHARING)
	/* Unshared FP registers (multithreading) mode. We disable the
	 * automatic stacking of FP registers (automatic setting of
	 * FPCA bit in the CONTROL register), upon exception entries,
	 * as the FP registers are to be used by a single context (and
	 * the use of FP registers in ISRs is not supported). This
	 * configuration improves interrupt latency and decreases the
	 * stack memory requirement for the (single) thread that makes
	 * use of the FP co-processor.
	 */
	FPU->FPCCR &= (~(FPU_FPCCR_ASPEN_Msk | FPU_FPCCR_LSPEN_Msk));
#else
	/*
	 * FP register sharing (multithreading) mode or single-threading mode.
	 *
	 * Enable both automatic and lazy state preservation of the FP context.
	 * The FPCA bit of the CONTROL register will be automatically set, if
	 * the thread uses the floating point registers. Because of lazy state
	 * preservation the volatile FP registers will not be stacked upon
	 * exception entry, however, the required area in the stack frame will
	 * be reserved for them. This configuration improves interrupt latency.
	 * The registers will eventually be stacked when the thread is swapped
	 * out during context-switch or if an ISR attempts to execute floating
	 * point instructions.
	 */
	FPU->FPCCR = FPU_FPCCR_ASPEN_Msk | FPU_FPCCR_LSPEN_Msk;
#endif /* CONFIG_FPU_SHARING */

	/* Make the side-effects of modifying the FPCCR be realized
	 * immediately.
	 */
	__DSB();
	__ISB();

	/* Initialize the Floating Point Status and Control Register. */
	__set_FPSCR(0);

	/*
	 * Note:
	 * The use of the FP register bank is enabled, however the FP context
	 * will be activated (FPCA bit on the CONTROL register) in the presence
	 * of floating point instructions.
	 */

#endif /* CONFIG_FPU */

	/*
	 * Upon reset, the CONTROL.FPCA bit is, normally, cleared. However,
	 * it might be left un-cleared by firmware running before Zephyr boot.
	 * We must clear this bit to prevent errors in exception unstacking.
	 *
	 * Note:
	 * In Sharing FP Registers mode CONTROL.FPCA is cleared before switching
	 * to main, so it may be skipped here (saving few boot cycles).
	 *
	 * If CONFIG_INIT_ARCH_HW_AT_BOOT is set, CONTROL is cleared at reset.
	 */
#if (!defined(CONFIG_FPU) || !defined(CONFIG_FPU_SHARING)) && \
	(!defined(CONFIG_INIT_ARCH_HW_AT_BOOT))

	__set_CONTROL(__get_CONTROL() & (~(CONTROL_FPCA_Msk)));
#endif
}
#endif /* CONFIG_CPU_HAS_FPU */

void z_arm_platform_init(void)
{
	extern void BootClock_Init();
	
#if defined(CONFIG_CPU_HAS_FPU)
	z_arm_floating_point_init();
#endif

#if CONFIG_USB_DC_CSK6
	SYSTEM_CLK->REG_MISC_CLK_CFG2.bit.ENA_USBC_CLK = 0x01;
#endif

	BootClock_Init();
	outw(ABB_BASE, inw(ABB_BASE)); //write 0 to bit16

	/*TODO: will rewrite later*/
	
#if CONFIG_CSK6_PSRAM
	extern int32_t PSRAM_Initialize(void);
	PSRAM_Initialize();
#endif
}

SYS_INIT(csk6_init, PRE_KERNEL_1, 0);
