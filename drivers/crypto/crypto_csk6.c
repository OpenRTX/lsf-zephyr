/*
 * Copyright (c) 2022 listenai Intelligent Technology (anhui) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define DT_DRV_COMPAT listenai_csk_crypto

#include <init.h>
#include <kernel.h>
#include <device.h>
#include <sys/__assert.h>
#include <crypto/cipher.h>

#include "Driver_CRYPTO.h"
#include "crypto.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(crypto_csk6, LOG_LEVEL_DBG);


#define DEV_DATA(dev) ((struct crypto_csk6_data *const)(dev)->data)
#define DEV_CFG(dev) ((const struct crypto_csk6_config *const)(dev)->config)
#define DEV_HANDLER(dev) ((void *)(DEV_DATA(dev)->handle))

#define AES_BLOCK_SIZE	(16)

#define CSK6_CRYP_SUPPORT (CAP_RAW_KEY | CAP_SEPARATE_IO_BUFS | CAP_SYNC_OPS | \
		      CAP_NO_IV_PREFIX)

struct crypto_csk6_config {

};

struct crypto_csk6_data {
    /* driver handle */
    void *handle; 

	/* crypto driver callback event*/
	volatile uint32_t event;

	struct k_sem device_sem;
	struct k_sem session_sem;
};

static void crypto_csk6_isr(const void *arg)
{
	struct device *dev = (struct device *)arg;
	struct crypto_csk6_data *data = DEV_DATA(dev);
	CRYPTO_RESOURCES *crypto_resources = data->handle;

	if(crypto_resources->irq_handler != NULL)
	{
		crypto_resources->irq_handler();
	}

}

static void CRYPTO_EventCallback(uint32_t event, void* workspace)
{
	const struct device *dev = (const struct device *)workspace;
	struct crypto_csk6_data *data = DEV_DATA(dev);

	data->event |= event;
}

static int csk6_do_decrypt(struct cipher_ctx *ctx, uint8_t *in_buf, int in_len,
		      uint8_t *out_buf)
{
	const struct device *dev = ctx->device;
	struct crypto_csk6_data *data = DEV_DATA(dev);

	switch(ctx->ops.cipher_mode)
	{
		case CRYPTO_CIPHER_MODE_ECB:
			CRYPTO_ECB_Decrypt(DEV_HANDLER(dev), (uint32_t *)in_buf, in_len, (uint32_t *)out_buf);
			break;

		case CRYPTO_CIPHER_MODE_CBC:
			CRYPTO_CBC_Decrypt(DEV_HANDLER(dev), (uint32_t *)in_buf, in_len, (uint32_t *)out_buf);
			break;

		default:
			LOG_ERR("Err decrypt mode %d\n", ctx->ops.cipher_mode);
			break;
	}

	while(!(data->event & CSK_CRYPTO_EVENT_DONE));

	return 0;
}

static int csk6_do_encrypt(struct cipher_ctx *ctx, uint8_t *in_buf, int in_len,
		      uint8_t *out_buf)
{
	const struct device *dev = ctx->device;
	struct crypto_csk6_data *data = DEV_DATA(dev);

	switch(ctx->ops.cipher_mode)
	{
		case CRYPTO_CIPHER_MODE_ECB:
			CRYPTO_ECB_Encrypt(DEV_HANDLER(dev), (uint32_t *)in_buf, in_len, (uint32_t *)out_buf);
			break;

		case CRYPTO_CIPHER_MODE_CBC:
			CRYPTO_CBC_Encrypt(DEV_HANDLER(dev), (uint32_t *)in_buf, in_len, (uint32_t *)out_buf);
			break;

		default:
			LOG_ERR("Err encrypt mode %d\n", ctx->ops.cipher_mode);
			break;
	}

	while(!(data->event & CSK_CRYPTO_EVENT_DONE));

	return 0;
}

static int crypto_csk6_ecb_encrypt(struct cipher_ctx *ctx,
				    struct cipher_pkt *pkt)
{
	int ret;

	ret = csk6_do_encrypt(ctx, pkt->in_buf, pkt->in_len, pkt->out_buf);
	if(0 == ret)
	{
		pkt->out_len = pkt->in_len;
	}

	return ret;
}

static int crypto_csk6_cbc_encrypt(struct cipher_ctx *ctx,
				    struct cipher_pkt *pkt, uint8_t *iv)
{
	int ret;
	const struct device *dev = ctx->device;

	CRYPTO_Control(DEV_HANDLER(dev), CSK_CRYPTO_SET_AES_IV, (uint32_t)iv);

	ret = csk6_do_encrypt(ctx, pkt->in_buf, pkt->in_len, pkt->out_buf);
	if(0 == ret)
	{
		pkt->out_len = pkt->in_len;
	}
	
	return ret;
}

static int crypto_csk6_cbc_decrypt(struct cipher_ctx *ctx,
				    struct cipher_pkt *pkt, uint8_t *iv)
{
	int ret;
	const struct device *dev = ctx->device;

	CRYPTO_Control(DEV_HANDLER(dev), CSK_CRYPTO_SET_AES_IV, (uint32_t)iv);

	ret = csk6_do_decrypt(ctx, pkt->in_buf, pkt->in_len, pkt->out_buf);
	if(0 == ret)
	{
		pkt->out_len = pkt->in_len;
	}
	
	return ret;
}

static int crypto_csk6_ecb_decrypt(struct cipher_ctx *ctx,
				    struct cipher_pkt *pkt)
{
	int ret;

	ret = csk6_do_decrypt(ctx, pkt->in_buf, pkt->in_len, pkt->out_buf);
	if(0 == ret)
	{
		pkt->out_len = pkt->in_len;
	}

	return ret;
}


static int crypto_csk6_session_setup(const struct device *dev,
				      struct cipher_ctx *ctx,
				      enum cipher_algo algo,
				      enum cipher_mode mode,
				      enum cipher_op op_type)
{
	if(ctx->keylen != 16u)
	{
		LOG_ERR("Unsupported keylen \n");
		return -EINVAL;
	}

    if (op_type == CRYPTO_CIPHER_OP_ENCRYPT)
    {
		switch (mode) 
        {
            case CRYPTO_CIPHER_MODE_ECB:
                ctx->ops.block_crypt_hndlr = crypto_csk6_ecb_encrypt;
                break;
            case CRYPTO_CIPHER_MODE_CBC:
                ctx->ops.cbc_crypt_hndlr = crypto_csk6_cbc_encrypt;
                break;
            default:
                return -EINVAL;
		}
	} else{
		switch (mode) 
        {
            case CRYPTO_CIPHER_MODE_ECB:
                ctx->ops.block_crypt_hndlr = crypto_csk6_ecb_decrypt;
                break;            
            case CRYPTO_CIPHER_MODE_CBC:
                ctx->ops.cbc_crypt_hndlr = crypto_csk6_cbc_decrypt;
                break;
            default:
                return -EINVAL;
		}

	}

	CRYPTO_PowerControl(DEV_HANDLER(dev), CSK_POWER_FULL);
    CRYPTO_Control(DEV_HANDLER(dev), CSK_CRYPTO_AES_KEY_MODE_USER, (uint32_t)ctx->key.bit_stream);

    return 0;
}

static int crypto_csk6_session_free(const struct device *dev,
				     struct cipher_ctx *ctx)
{
	CRYPTO_PowerControl(DEV_HANDLER(dev), CSK_POWER_OFF);
    return 0;
}

static int crypto_csk6_query_caps(const struct device *dev)
{
	return CSK6_CRYP_SUPPORT;
}

static int crtpto_csk6_async_callback_set(const struct device *dev,crypto_completion_cb cb)
{
	return -EINVAL;
}

static int crypto_csk6_init(const struct device *dev)
{
    struct crypto_csk6_data *data = DEV_DATA(dev);

    data->handle = CRYPTO0();

	k_sem_init(&data->device_sem, 1, 1);
	k_sem_init(&data->session_sem, 1, 1);

    CRYPTO_Initialize(DEV_HANDLER(dev), CRYPTO_EventCallback, (void*)dev);

    IRQ_CONNECT(DT_INST_IRQN(0), DT_INST_IRQ(0, priority),
		    crypto_csk6_isr, DEVICE_DT_INST_GET(0), 0);

	irq_enable(DT_INST_IRQN(0));

	return 0;
}

static struct crypto_driver_api crypto_enc_funcs = {
	.begin_session = crypto_csk6_session_setup,
	.free_session = crypto_csk6_session_free,
	.crypto_async_callback_set = crtpto_csk6_async_callback_set,
	.query_hw_caps = crypto_csk6_query_caps,
};

static struct crypto_csk6_data crypto_csk6_dev_data = {

};

static struct crypto_csk6_config crypto_csk6_dev_config = {

};

DEVICE_DT_INST_DEFINE(0, crypto_csk6_init, NULL,
		    &crypto_csk6_dev_data,
		    &crypto_csk6_dev_config, POST_KERNEL,
		    CONFIG_CRYPTO_INIT_PRIORITY, (void *)&crypto_enc_funcs);
