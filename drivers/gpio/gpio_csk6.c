
#define DT_DRV_COMPAT listenai_csk_gpio

#include <errno.h>
#include <device.h>
#include <drivers/gpio.h>
#include <soc.h>
#include <sys/util.h>

#include "gpio_utils.h"

#include "hal/gpio.h"
#include "hal/Driver_GPIO.h"
#include <hal/IOMuxManager.h>

typedef void *gpio_driver_ctx_t;

struct csk_gpio_config {
    struct gpio_driver_config common;
    int driver_num;
    void *arg;
};

struct csk_gpio_data {
	/* gpio_driver_data needs to be first */
	struct gpio_driver_data common;
	/* port ISR callback routine address */
	sys_slist_t callbacks;
};

static const gpio_driver_ctx_t csk_gpio_get_driver_ctx(int port_num)
{
    if (port_num == 0) {
        return GPIOA();
    } else if (port_num == 1) {
        return GPIOB();
    } else {
        return NULL;
    }
}

static void csk_gpio_isr(const struct device *dev)
{
    const struct csk_gpio_config *config = dev->config;
    gpio_driver_ctx_t driver_context = csk_gpio_get_driver_ctx(config->driver_num);
	struct csk_gpio_data *data = dev->data;
    GPIO_RegDef *gpio_reg = ((GPIO_RESOURCES *)driver_context)->reg;
    uint32_t int_status;

    int_status = gpio_reg->REG_INTRSTATUS.all;
    // clear gpio interrupt status
    gpio_reg->REG_INTRSTATUS.all = int_status;

	gpio_fire_callbacks(&data->callbacks, dev, int_status);
}

static int csk_gpio_a_pinmux_set(gpio_pin_t pin)
{
    uint32_t pin_func = CSK_IOMUX_FUNC_DEFAULT;

    if((pin <=2) || ((pin >= 10) && (pin <= 14)))
    {
        pin_func = CSK_IOMUX_FUNC_ALTER1;
    }

    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, pin, pin_func);

    return 0;
}

static int csk_gpio_pinmux_set(const struct device *dev, gpio_pin_t pin)
{
    const struct csk_gpio_config *config = dev->config;

    if (config->driver_num == 0) {
        csk_gpio_a_pinmux_set(pin);
    } else if (config->driver_num == 1) {
        IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, pin, CSK_IOMUX_FUNC_DEFAULT);
    } else {
        return -EINVAL;
    }

    return 0;
}

static int csk_gpio_pull_set(const struct device *dev, gpio_pin_t pin, gpio_flags_t flags)
{
    const struct csk_gpio_config *config = dev->config;
    uint8_t pad;

    if (0 == config->driver_num) {
        pad = CSK_IOMUX_PAD_A;
    } else if (1 == config->driver_num) {
        pad = CSK_IOMUX_PAD_B;
    } else {
        return -EINVAL;
    }
    

    if(GPIO_PULL_UP == flags){
        IOMuxManager_ModeConfigure(pad, pin, HAL_IOMUX_PULLUP_MODE);
    }else if(GPIO_PULL_DOWN == flags){
        IOMuxManager_ModeConfigure(pad, pin, HAL_IOMUX_PULLDOWN_MODE);
    }

    return 0;
}

static int csk_gpio_configure(const struct device *dev, gpio_pin_t pin, gpio_flags_t flags)
{
    const struct csk_gpio_config *config = dev->config;
    gpio_driver_ctx_t driver_context = csk_gpio_get_driver_ctx(config->driver_num);

    csk_gpio_pinmux_set(dev, pin);

    if (flags & GPIO_OUTPUT) {
        GPIO_SetDir(driver_context, 1UL << pin, CSK_GPIO_DIR_OUTPUT);
    } else if (flags & GPIO_INPUT) {
        if (flags & GPIO_INT_DEBOUNCE) {
            GPIO_Control(driver_context, CSK_GPIO_DEBOUNCE_ENABLE, 1UL << pin);
        }
        GPIO_SetDir(driver_context, 1UL << pin, CSK_GPIO_DIR_INPUT);
    }

    if (flags & GPIO_PULL_UP) {
        csk_gpio_pull_set(dev, pin, GPIO_PULL_UP);
    } else if (flags & GPIO_PULL_DOWN) {
        csk_gpio_pull_set(dev, pin, GPIO_PULL_DOWN);
    }

    if (flags & GPIO_OUTPUT_INIT_HIGH) {
        GPIO_PinWrite(driver_context, 1UL << pin, 1);
    } else if (flags & GPIO_OUTPUT_INIT_LOW) {
        GPIO_PinWrite(driver_context, 1UL << pin, 0);
    }
    return 0;
}

static int csk_gpio_port_get_raw(const struct device *dev, uint32_t *value)
{
    const struct csk_gpio_config *config = dev->config;
    gpio_driver_ctx_t driver_context = csk_gpio_get_driver_ctx(config->driver_num);
    GPIO_RegDef *gpio_reg = ((GPIO_RESOURCES *)driver_context)->reg;
    *value = gpio_reg->REG_DATAIN.all;
    return 0;
}

static int csk_gpio_port_set_masked_raw(const struct device *dev, gpio_port_pins_t mask, gpio_port_value_t value)
{
    const struct csk_gpio_config *config = dev->config;
    gpio_driver_ctx_t driver_context = csk_gpio_get_driver_ctx(config->driver_num);
    GPIO_RegDef *gpio_reg = ((GPIO_RESOURCES *)driver_context)->reg;
    gpio_reg->REG_DATAOUT.all = (gpio_reg->REG_DATAOUT.all & ~mask) | (mask & value);
    return 0;
}

static int csk_gpio_port_set_bits_raw(const struct device *dev, gpio_port_pins_t pins)
{
    const struct csk_gpio_config *config = dev->config;
    gpio_driver_ctx_t driver_context = csk_gpio_get_driver_ctx(config->driver_num);
    GPIO_RegDef *gpio_reg = ((GPIO_RESOURCES *)driver_context)->reg;
    gpio_reg->REG_DATAOUT.all |= pins;
    return 0;
}

static int csk_gpio_port_clear_bits_raw(const struct device *dev, gpio_port_pins_t pins)
{
    const struct csk_gpio_config *config = dev->config;
    gpio_driver_ctx_t driver_context = csk_gpio_get_driver_ctx(config->driver_num);
    GPIO_RegDef *gpio_reg = ((GPIO_RESOURCES *)driver_context)->reg;
    gpio_reg->REG_DATAOUT.all &= ~pins;
    return 0;
}

static int csk_gpio_port_toggle_bits(const struct device *dev, gpio_port_pins_t pins)
{
    const struct csk_gpio_config *config = dev->config;
    gpio_driver_ctx_t driver_context = csk_gpio_get_driver_ctx(config->driver_num);
    GPIO_RegDef *gpio_reg = ((GPIO_RESOURCES *)driver_context)->reg;
    gpio_reg->REG_DATAOUT.all ^= pins;
    return 0;
}

static int csk_gpio_pin_interrupt_configure(const struct device *dev,
                        gpio_pin_t pin,
                        enum gpio_int_mode mode,
                        enum gpio_int_trig trig)
{
    uint32_t key;
    const struct csk_gpio_config *config = dev->config;
    gpio_driver_ctx_t driver_context = csk_gpio_get_driver_ctx(config->driver_num);
    GPIO_RegDef *gpio_reg = ((GPIO_RESOURCES *)driver_context)->reg;
    key = irq_lock();

    if (gpio_reg->REG_CHANNELDIR.all & (1UL << pin)) {
        //gpio interrupt could not works in output mode
        irq_unlock(key);
        return -ENOTSUP;
    }

    uint32_t isr_conf_mask = CSK_GPIO_INTR_ENABLE;
    if (mode == GPIO_INT_MODE_EDGE) {
        if (trig == GPIO_INT_TRIG_LOW) {
            isr_conf_mask |= CSK_GPIO_SET_INTR_NEGATIVE_EDGE;
        } else if (trig == GPIO_INT_TRIG_HIGH) {
            isr_conf_mask |= CSK_GPIO_SET_INTR_POSITIVE_EDGE;
        } else {
            isr_conf_mask |= CSK_GPIO_SET_INTR_DUAL_EDGE;
        }
    } else if (mode == GPIO_INT_MODE_LEVEL) {
        if (trig == GPIO_INT_TRIG_LOW) {
            isr_conf_mask |= CSK_GPIO_SET_INTR_LOW_LEVEL;
        } else if (trig == GPIO_INT_TRIG_HIGH) {
            isr_conf_mask |= CSK_GPIO_SET_INTR_HIGH_LEVEL;
        }
    }
    GPIO_Control(driver_context, isr_conf_mask, 1UL << pin);
    irq_unlock(key);
    return 0;
}

static int csk_gpio_manage_callback(const struct device *dev,
				    struct gpio_callback *cb, bool set)
{
	struct csk_gpio_data *data = dev->data;

	return gpio_manage_callback(&data->callbacks, cb, set);
}

static const struct gpio_driver_api csk_gpio_driver_api = {
        .pin_configure = csk_gpio_configure,
        .port_get_raw = csk_gpio_port_get_raw,
        .port_set_masked_raw = csk_gpio_port_set_masked_raw,
        .port_set_bits_raw = csk_gpio_port_set_bits_raw,
        .port_clear_bits_raw = csk_gpio_port_clear_bits_raw,
        .port_toggle_bits = csk_gpio_port_toggle_bits,
        .pin_interrupt_configure = csk_gpio_pin_interrupt_configure,
        .manage_callback = csk_gpio_manage_callback
};


//TODO: Add obtaion configuration from devicetree MACRO
#define GPIO_CSK_INIT(n) \
    static int csk_gpio_##n##_init(const struct device *dev);	\
    static struct csk_gpio_data csk_gpio_##n##_data;		\
    static const struct csk_gpio_config csk_gpio_##n##_config = {   \
        .common = {						                            \
			.port_pin_mask = GPIO_PORT_PIN_MASK_FROM_DT_INST(n),    \
		},						                                    \
		.driver_num = n,                                            \
	};                                                              \
	DEVICE_DT_INST_DEFINE(n,					\
			    csk_gpio_##n##_init,			\
			    NULL,					\
			    &csk_gpio_##n##_data,                   \
                &csk_gpio_##n##_config,     \
			    POST_KERNEL,				\
			    CONFIG_KERNEL_INIT_PRIORITY_DEVICE,	\
			    &csk_gpio_driver_api);			\
                \
    static int csk_gpio_##n##_init(const struct device *dev)	\
	{		\
        GPIO_Initialize(csk_gpio_get_driver_ctx(n), NULL, NULL); \
		IRQ_CONNECT(DT_INST_IRQ_BY_IDX(n, 0, irq),		\
			    DT_INST_IRQ_BY_IDX(n, 0, priority),		\
			    csk_gpio_isr,				\
			    DEVICE_DT_INST_GET(n), 0);			\
									\
		irq_enable(DT_INST_IRQ_BY_IDX(n, 0, irq));		\
		return 0;						\
	}

DT_INST_FOREACH_STATUS_OKAY(GPIO_CSK_INIT)
