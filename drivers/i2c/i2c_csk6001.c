/*
 * Copyright (c) 2021 listenai Intelligent Technology (anhui) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT listenai_csk_i2c

#include <drivers/i2c.h>
#include <drivers/pinmux.h>
#include "hal/Driver_I2C.h"
#include "hal/i2c.h"
#include <soc_dt.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(i2c_csk);

#define DEV_CFG(dev) ((const struct i2c_csk_config *const)(dev)->config)
#define DEV_DATA(dev) ((struct i2c_csk_data *const)(dev)->data)
#define DEV_BASE(dev) ((I2C_RegDef *)(DEV_CFG(dev))->base)
#define DEV_HANDLER(dev) ((void *)(DEV_DATA(dev)->handle))

#define MAX_MSG (40)
#define Q_ALIGN (4)

#define I2C_SLAVE_ADDR (0x35) /* csk6001 default i2c slave address 0x35 */



struct i2c_csk_config {
	I2C_RegDef *base;
	void (*irq_config_func)(const struct device *dev);
	/* I2C alternate configuration */
	const struct csk6_pinctrl_cfg *pinctrl_list;
	uint8_t pinctrl_list_size;
};

struct i2c_csk_data {
	void *handle; /* driver handle */
	struct k_msgq *msgq; /* driver message queues*/
	uint32_t status; /* driver event status */
	bool mode; /* transfer mode: true: master, false: slave */
};

static void *i2c_csk_get_ctx(const struct device *dev)
{
	I2C_RegDef *base = DEV_BASE(dev);
	if (base == VENUS_I2C0) {
		return I2C0();
	} else if (base == VENUS_I2C1) {
		return I2C1();
	} else {
		LOG_ERR("i2c reg base[%p] err, default: I2C0", base);
		return NULL;
	}
}

static void i2c_csk_event_cb(uint32_t event, void *user_arg)
{
	struct i2c_csk_data *data = (struct i2c_csk_data *)user_arg;
	int ret = 0;
	data->status |= event;

	ret = k_msgq_put(data->msgq, &data->status, K_NO_WAIT);
	if (ret != 0) {
		LOG_ERR("ret:%d", ret);
	}

	data->status = 0;
	return;
}

static int i2c_csk_configure(const struct device *dev, uint32_t dev_config)
{
	struct i2c_csk_data *data = DEV_DATA(dev);
	void *I2C_Handler = DEV_HANDLER(dev);
	uint32_t arg = 0;

	if (!(I2C_MODE_MASTER & dev_config)) { /* slave mode */

		/* slave address 10bit*/
		if (I2C_ADDR_10_BITS & dev_config) {
			arg |= CSK_I2C_ADDRESS_10BIT;
		}

		/* use default slave address, user also can define slave adderss use dev_config*/
		arg |= I2C_SLAVE_ADDR;

		I2C_Control(I2C_Handler, CSK_I2C_OWN_ADDRESS, arg);

		/* interrupt mode */
		I2C_Control(I2C_Handler, CSK_I2C_TRANSMIT_MODE, 0); /* 0: interrupt 1: DMA */

		/* slave mode */
		LOG_DBG("slave mode");
		data->mode = false;

	} else { /* master mode */

		/* master mode support both 7bit and 10bit slave address*/

		/* bus speed */
		switch (I2C_SPEED_GET(dev_config)) {
		case I2C_SPEED_STANDARD:
			arg = CSK_I2C_BUS_SPEED_STANDARD;
			break;
		case I2C_SPEED_FAST:
			arg = CSK_I2C_BUS_SPEED_FAST;
			break;
		case I2C_SPEED_FAST_PLUS:
			arg = CSK_I2C_BUS_SPEED_FAST_PLUS;
			break;
		case I2C_SPEED_HIGH:
			arg = CSK_I2C_BUS_SPEED_HIGH;
			break;
		case I2C_SPEED_ULTRA:
			return -ENOTSUP; /* 5 MHz not support */
		default:
			return -EINVAL;
		}
		I2C_Control(I2C_Handler, CSK_I2C_BUS_SPEED, arg);

		/* interrupt mode */
		I2C_Control(I2C_Handler, CSK_I2C_TRANSMIT_MODE, 0); /* 0: interrupt 1: DMA */

		/* Clear bus */
		I2C_Control(I2C_Handler, CSK_I2C_BUS_CLEAR, 0);

		/* master mode */
		data->mode = true;
	}

	return 0;
}

static int i2c_csk_transfer(const struct device *dev, struct i2c_msg *msgs, uint8_t num_msgs,
			    uint16_t addr)
{
	struct i2c_csk_data *data = DEV_DATA(dev);
	void *I2C_Handler = DEV_HANDLER(dev);
	uint32_t ret = 0;

	if (!num_msgs) {
		return 0;
	}

	/* attenton: one i2c transfer(trnsmit or receive) can transfer max 256 bytes(address len + data len), 
	which means that if address length is 1 byte, then max data length is 255 bytes*/
	if (data->mode) {
		for (int i = 0; i < num_msgs; i++) {
			/* master and slave mode both supporting 10-bit address and 7-bit address */

			/* I2c write or read */
			if ((msgs->flags & I2C_MSG_RW_MASK) == I2C_MSG_WRITE) {
				/* start transfer */
				if ((msgs->flags & I2C_MSG_STOP) == I2C_MSG_STOP) {
					/* write, then stop */
					ret = I2C_MasterTransmit(I2C_Handler, addr, msgs->buf,
								 msgs->len, 0);
				} else {
					/* write, then wait next operation(write/read), not stop */
					ret = I2C_MasterTransmit(I2C_Handler, addr, msgs->buf,
								 msgs->len, 1);
				}
			} else {
				/* start transfer */
				if ((msgs->flags & I2C_MSG_STOP) == I2C_MSG_STOP) {
					/* read, then stop */
					ret = I2C_MasterReceive(I2C_Handler, addr, msgs->buf,
								msgs->len, 0);
				} else {
					/* read, then wait next operation, not stop */
					ret = I2C_MasterReceive(I2C_Handler, addr, msgs->buf,
								msgs->len, 1);
				}
			}

			/* transfer error, abort transfer and return */
			if (ret != CSK_DRIVER_OK) {
				LOG_ERR("I2C Master transfer error: %d\n", ret);
				I2C_Control(I2C_Handler, CSK_I2C_ABORT_TRANSFER, 0);

				ret = -EIO;
				break;
			}

			/* wait until transfer done */
			uint32_t status = 0;
			do {
				status = 0;
				k_msgq_get(data->msgq, &status, K_FOREVER);
			} while (!(status & CSK_I2C_EVENT_TRANSFER_DONE));

			/* Move to the next message */
			msgs++;
		}
	} else {
		/* slave mode only support basic i2c write and read, which means that i2c read only read data, 
		not opearte after i2c write */

		for (int i = 0; i < num_msgs; i++) {
			/* master and slave mode both support 10-bit address and 7-bit address */

			/* I2c write or read */
			if ((msgs->flags & I2C_MSG_RW_MASK) == I2C_MSG_WRITE) {
				/* wait until host controller start receive*/
				uint32_t status = 0;
				do {
					status = 0;
					k_msgq_get(data->msgq, &status, K_FOREVER);
				} while (!(status & CSK_I2C_EVENT_SLAVE_TRANSMIT));

				/* start slave transmit, one by one */
				int index = 0;
				do {
					/* master receive byte count must less or equal than 256 bytes, because driver max transfer limit; in case
					index is bigger than msgs->len, user msgs->len must bigger or equal than master receive byte counter */
					I2C_SlaveTransmit(I2C_Handler, &msgs->buf[index++], 1);

					/* wait until TRANSFER_INCOMPLETE, or TRANSFER_COMPLETE */
					do {
						status = 0;
						k_msgq_get(data->msgq, &status, K_FOREVER);
					} while (!(status & (CSK_I2C_EVENT_TRANSFER_INCOMPLETE |
							     CSK_I2C_EVENT_TRANSFER_DONE)));

				} while (!(status & CSK_I2C_EVENT_TRANSFER_DONE));

			} else {
				/* Detect trigger slave receive interrupt */
				uint32_t status = 0;
				do {
					status = 0;
					k_msgq_get(data->msgq, &status, K_FOREVER);
				} while (!(status & CSK_I2C_EVENT_SLAVE_RECEIVE));

				/* wait until master transfer done */
				do {
					status = 0;
					k_msgq_get(data->msgq, &status, K_FOREVER);
				} while (!(status & (CSK_I2C_EVENT_TRANSFER_DONE)));

				/* get slave received bufer byte cnt */
				int cnt = I2C_GetDataCount(I2C_Handler);
				LOG_DBG("slave recevie data cnt:%d\n", cnt);

				/* fetch received data from driver, user msgs->len must bigger or equal than master send byte count;
					the size of data received from master maybe not equal to user msgs->len */
				int len = (msgs->len <= cnt) ? msgs->len : cnt;
				I2C_SlaveReceive(I2C_Handler, msgs->buf, len);
			}

			/* Move to the next message */
			msgs++;
		}
	}

	return ret;
}

static int i2c_csk_init(const struct device *dev)
{
	const struct i2c_csk_config *config = DEV_CFG(dev);
	struct i2c_csk_data *data = DEV_DATA(dev);

	/* Get device handle */
	data->handle = i2c_csk_get_ctx(dev);
	if (data->handle == NULL) {
		return -EIO;
	}
	void *I2C_Handler = data->handle;

	/* Initialize i2c */
	I2C_Initialize(I2C_Handler, i2c_csk_event_cb, data);
	I2C_PowerControl(I2C_Handler, CSK_POWER_FULL);

	PINCTRL_CONFIG_PINS(config->pinctrl_list, config->pinctrl_list_size);

	/* IRQ_CONNECT and irq_enable */
	config->irq_config_func(dev);

	return 0;
}

static void i2c_csk_isr(const struct device *dev)
{
	extern void i2c_irq_handler(I2C_RESOURCES * i2c);

	void *I2C_Handler = DEV_HANDLER(dev);
	i2c_irq_handler(I2C_Handler);
}

static const struct i2c_driver_api i2c_csk_driver_api = {
	.configure = i2c_csk_configure,
	.transfer = i2c_csk_transfer,
	.slave_register = NULL,
	.slave_unregister = NULL,
	.recover_bus = NULL,
};

#define I2C_DEVICE_INIT_CSK(n)                                                                     	\
	static void i2c_csk_config_func_##n(const struct device *dev);                             		\
																									\
	static const struct csk6_pinctrl_cfg                                       						\
		i2c_alts_##n[DT_INST_NUM_PINCTRLS_BY_IDX(n, 0)] =          									\
			CSK6_DT_ALT_ITEMS_LIST(n);                        										\
                                                                                                   	\
	static const struct i2c_csk_config i2c_csk_config_##n = {                                  		\
		.base = (I2C_RegDef *)DT_INST_REG_ADDR(n),                                         \
		.irq_config_func = i2c_csk_config_func_##n,                                        \
		.pinctrl_list = i2c_alts_##n, 													   \
		.pinctrl_list_size = ARRAY_SIZE(i2c_alts_##n),									   \
	};                	                                                                         \
                                                                                                   \
	K_MSGQ_DEFINE(msgq_##n, sizeof(uint32_t), MAX_MSG, Q_ALIGN);                               \
	static struct i2c_csk_data i2c_csk_data_##n = { .msgq = &msgq_##n };                       \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(n, &i2c_csk_init, NULL, &i2c_csk_data_##n, &i2c_csk_config_##n,      \
			      POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,                     \
			      &i2c_csk_driver_api);                                                \
                                                                                                   \
	static void i2c_csk_config_func_##n(const struct device *dev)                              \
	{                                                                                          \
		IRQ_CONNECT(DT_INST_IRQN(n), DT_INST_IRQ(n, priority), i2c_csk_isr,                \
			    DEVICE_DT_INST_GET(n), 0);                                             \
                                                                                                   \
		irq_enable(DT_INST_IRQN(n));                                                       \
	}

DT_INST_FOREACH_STATUS_OKAY(I2C_DEVICE_INIT_CSK)
