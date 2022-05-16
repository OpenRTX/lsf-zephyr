/*
 * Copyright (c) 2021 listenai Intelligent Technology (anhui) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "kernel.h"
#include <stdint.h>
#define DT_DRV_COMPAT listenai_csk_usbd

#include <drivers/usb/usb_dc.h>
#include <usb/usb_device.h>

#include "hal/usb_venus.h"
#include "hal/cache.h"

#include <sys/byteorder.h>

#define LOG_LEVEL LOG_LEVEL_ERR
#include <logging/log.h>
LOG_MODULE_REGISTER(usb_dc_csk);


#define NUM_OF_EP_MAX		DT_INST_PROP(0, num_bidir_endpoints)

struct cb_msg {
	uint8_t ep;
	uint8_t type;
	uint32_t cb;
};

K_MSGQ_DEFINE(usb_dc_msgq, sizeof(struct cb_msg), 10, 4);

/*
 * USB endpoint dma mode private structure.
 */
struct usb_ep_dma_prv {
	uint32_t addr; /* Endpoint dma transfer address */
	uint8_t channel_num; /* Endpoint dma channel number */
	usb_dc_ep_callback cb; /* Endpoint dma transfer callback function */
	uint32_t length; /* Endpoint dma transfer total length */
	uint8_t enable; /* Endpoint dma enable/disable */
};

/*
 * USB endpoint private structure.
 */
struct usb_ep_ctrl_prv {
	uint8_t ep_ena;
	uint8_t fifo_num;
	uint32_t fifo_size;
	uint16_t mps; /* Max ep pkt size */
	usb_dc_ep_callback cb; /* Endpoint callback function */
	uint32_t data_len; /*lenth for each transfer packet */
	struct usb_ep_dma_prv dma_info;
};

/*
 * USB controller private structure.
 */
struct usb_venus_ctrl_prv {
	usb_dc_status_callback status_cb;
	struct usb_ep_ctrl_prv in_ep_ctrl[USB_VENUS_IN_EP_NUM]; /* USB IN endpoint information */
	struct usb_ep_ctrl_prv out_ep_ctrl[USB_VENUS_OUT_EP_NUM]; /* USB OUT endpoint information */
	uint32_t txfifo_alloc_addr; /* USB TxFifo address allocated */
	uint32_t rxfifo_alloc_addr; /* USB RxFifo address allocated */
	uint8_t attached;
	uint8_t address; /* USB device address */
	uint8_t channel_active; /* USB dma channel active flag */
	uint8_t status;
};

struct ep_mem_block {
	void *data;
};



struct usb_device_data {
	uint8_t address;
	uint32_t bd_active;
	bool attached;

	K_KERNEL_STACK_MEMBER(thread_stack, CONFIG_USB_DC_CSK6_THREAD_STACK_SIZE);
	struct k_thread thread;
};

static struct usb_device_data dev_data;

#define USB_DC_CB_TYPE_MGMT		0
#define USB_DC_CB_TYPE_EP		1

static struct usb_venus_ctrl_prv usb_venus_ctrl = { 0 };
static void usb_venus_isr_handler(const void *unused);
int usb_dc_ep_enable_dma(const uint8_t ep);
int usb_dc_ep_disable_dma(const uint8_t ep);

static void usb_csk6001_thread_main(void *arg1, void *unused1, void *unused2);

static int usb_venus_init(void)
{
	/* Initial usb_venus_ctrl */
	usb_venus_ctrl.status = USB_VENUS_STS_SETUP;

	/* Endpoint0 is fifo size is fixed to 64bytes */
	usb_venus_ctrl.in_ep_ctrl[0].mps = USB_MAX_CTRL_MPS;
	usb_venus_ctrl.in_ep_ctrl[0].fifo_size = USB_MAX_CTRL_MPS;
	usb_venus_ctrl.out_ep_ctrl[0].mps = USB_MAX_CTRL_MPS;
	usb_venus_ctrl.out_ep_ctrl[0].fifo_size = USB_MAX_CTRL_MPS;
	usb_venus_ctrl.txfifo_alloc_addr = 0x400 >> 3;

	/* USB_VENUS_IN_EP_NUM == USB_VENUS_OUT_EP_NUM */
	for (uint32_t i = 0; i < USB_VENUS_IN_EP_NUM; i++) {
		usb_venus_ctrl.in_ep_ctrl[i].dma_info.channel_num =
			USB_VENUS_DMA_CHANNEL_NOT_ASSIGNED;
		usb_venus_ctrl.out_ep_ctrl[i].dma_info.channel_num =
			USB_VENUS_DMA_CHANNEL_NOT_ASSIGNED;
	}

	/* Set device speed to Full Speed */
	CSK_USBC->POWER &= ~USB_VENUS_POWER_HSENABLE;

	/* Enable soft connect */
	CSK_USBC->POWER |= USB_VENUS_POWER_SOFTCONN;


	k_thread_create(&dev_data.thread, dev_data.thread_stack,
		CONFIG_USB_DC_CSK6_THREAD_STACK_SIZE,
		usb_csk6001_thread_main, NULL, NULL, NULL,
		K_PRIO_COOP(CONFIG_USB_DC_CSK6_THREAD_COOP_PRIO), 0, K_NO_WAIT);


	/* Register USB ISR */
	IRQ_CONNECT(DT_INST_IRQN(0), DT_INST_IRQ(0, priority), usb_venus_isr_handler, 0, 0);

	/* Enable usb module interrupt */
	CSK_USBC->INTRUSBE = 0x00;
	CSK_USBC->INTRUSBE |= USB_VENUS_INTRUSBE_RESET;

	/* Enable global interrupt */
	irq_enable(DT_INST_IRQN(0));

	return 0;
}

static int usb_venus_reset(void)
{
	/* USB controller reset */
	CSK_USBC->SOFT_RST |= (USB_VENUS_SOFT_RST_NRST | USB_VENUS_SOFT_RST_NRSTX);
	return 0;
}

static uint8_t usb_venus_ep_is_valid(uint8_t ep)
{
	uint8_t ep_idx = USB_EP_GET_IDX(ep);

	/* Check if ep enabled */
	if ((USB_EP_DIR_IS_OUT(ep)) && ep_idx < USB_VENUS_OUT_EP_NUM) {
		return 1;
	} else if ((USB_EP_DIR_IS_IN(ep)) && ep_idx < USB_VENUS_IN_EP_NUM) {
		return 1;
	}

	return 0;
}

static int usb_venus_ep_set(uint8_t ep, uint32_t ep_mps, enum usb_dc_ep_transfer_type ep_type)
{
	uint8_t ep_idx = USB_EP_GET_IDX(ep);

	LOG_DBG("%s ep %x, mps %d, type %d", __func__, ep, ep_mps, ep_type);

	if (USB_EP_DIR_IS_OUT(ep)) {
		if (usb_venus_ctrl.out_ep_ctrl[USB_EP_GET_IDX(ep)].ep_ena == 1) {
			return 0;
		}
	} else {
		if (usb_venus_ctrl.in_ep_ctrl[USB_EP_GET_IDX(ep)].ep_ena == 1) {
			return 0;
		}
	}

	if (ep_mps > (USB_VENUS_MAXP_MASK)) {
		return -EINVAL;
	}
	/* Set max packet size */
	if (USB_EP_DIR_IS_OUT(ep)) {
		EDPxReg_SEL(ep_idx);
		CSK_USBC->RXMAXP = ep_mps;
	} else {
		EDPxReg_SEL(ep_idx);
		CSK_USBC->TXMAXP = ep_mps;
	}

	LOG_DBG("Endpoint setting %x, mps 0x%x", ep, ep_mps);

	//EP0 setting
	if (!ep_idx) {
		/* max packet size is fixed to 64Bytes for EP0 */
		/* No need to set EP0 type */
	} else {
		switch (ep_type) {
		case USB_DC_EP_CONTROL:
			/* No need to set EP0, EP1-EP15 can't be set to control transfer */
			break;
		case USB_DC_EP_BULK:
			LOG_DBG("EP%x setting USB_DC_EP_BULK", ep);
			/* Clear ISO type bit */
			EDPxReg_SEL(ep_idx);
			CSK_USBC->TXCSRH &= (~USB_VENUS_TXCSRH_ISO_MASK);
			CSK_USBC->RXCSRH &= (~USB_VENUS_RXCSRH_ISO_MASK);
			if (USB_EP_DIR_IS_OUT(ep)) {
				/* force the endpoint data toggle */
				CSK_USBC->RXCSRL &= (~USB_VENUS_RXCSRL_CLRDATATOG_MASK);
				CSK_USBC->RXCSRL |= USB_VENUS_RXCSRL_CLRDATATOG;
				/* set endpoint fifo size and fifo address */
				CSK_USBC->RXFIFOSZ &=
					~(USB_VENUS_FIFOSZ_DPB_MASK | USB_VENUS_FIFOSZ_SZ_MASK);
				CSK_USBC->RXFIFOSZ |= GET_FIFOSZ_CFG(ep_mps);
				usb_venus_ctrl.out_ep_ctrl[ep_idx].fifo_size = ep_mps;
				usb_venus_ctrl.out_ep_ctrl[ep_idx].mps = ep_mps;
				usb_venus_ctrl.rxfifo_alloc_addr +=
					ep_mps >> 3; //8bytes per address
				CSK_USBC->RXFIFOADD = usb_venus_ctrl.rxfifo_alloc_addr;
				LOG_INF("EP%x RX fifo adress is 0x%x, size is 0x%x", ep,
					usb_venus_ctrl.rxfifo_alloc_addr,
					usb_venus_ctrl.out_ep_ctrl[ep_idx].fifo_size);
				if (usb_venus_ctrl.out_ep_ctrl[ep_idx].dma_info.enable) {
					//dma multiple packet mode
					CSK_USBC->RXCSRH &= ~(USB_VENUS_RXCSRH_AUTOCLEAR |
							      USB_VENUS_RXCSRH_DMAREQENAB |
							      USB_VENUS_RXCSRH_DMAREQMODE_1);
					CSK_USBC->RXCSRH |= (USB_VENUS_RXCSRH_AUTOCLEAR |
							     USB_VENUS_RXCSRH_DMAREQENAB |
							     USB_VENUS_RXCSRH_DMAREQMODE_1);
				} else {
					//dma single packet mode
					/* The DMAReqEnab bit (D13) of the appropriate RxCSR register set to 0. */
					CSK_USBC->RXCSRH &= ~USB_VENUS_RXCSRH_DMAREQENAB_MASK;
				}
			} else {
				/* force the endpoint data toggle */
				CSK_USBC->TXCSRH &= (~USB_VENUS_TXCSRH_FRCDATATOG_MASK);
				CSK_USBC->TXCSRH |= USB_VENUS_TXCSRH_FRCDATATOG;
				/* set endpoint fifo size and fifo address */
				CSK_USBC->TXFIFOSZ &=
					~(USB_VENUS_FIFOSZ_DPB_MASK | USB_VENUS_FIFOSZ_SZ_MASK);
				CSK_USBC->TXFIFOSZ |= GET_FIFOSZ_CFG(ep_mps);
				if ((CSK_USBC->TXCSRL & USB_VENUS_TXCSRL_FIFONOTEMPTY) ==
				    USB_VENUS_TXCSRL_FIFONOTEMPTY) {
					//The CPU write 1 to this bit to flush the latest packet from endpoint TX FIFO
					CSK_USBC->TXCSRL |= USB_VENUS_TXCSRL_FLUSHFIFO;
				}
				usb_venus_ctrl.in_ep_ctrl[ep_idx].fifo_size = ep_mps;
				usb_venus_ctrl.in_ep_ctrl[ep_idx].mps = ep_mps;
				usb_venus_ctrl.txfifo_alloc_addr +=
					ep_mps >> 3; //8bytes per address
				CSK_USBC->TXFIFOADD = usb_venus_ctrl.txfifo_alloc_addr;
				//				LOG_INF("EP%x TX fifo adress is 0x%x", ep, usb_venus_ctrl.rxfifo_alloc_addr);
				if (usb_venus_ctrl.in_ep_ctrl[ep_idx].dma_info.enable) {
					//dma multiple packet mode
					CSK_USBC->TXCSRH &= ~(USB_VENUS_TXCSRH_AUTOSET |
							      USB_VENUS_TXCSRH_DMAREQENAB |
							      USB_VENUS_TXCSRH_DMAREQMODE_1);
					CSK_USBC->TXCSRH |= (USB_VENUS_TXCSRH_AUTOSET |
							     USB_VENUS_TXCSRH_DMAREQENAB |
							     USB_VENUS_TXCSRH_DMAREQMODE_1);
				} else {
					//dma single packet mode
					/* The DMAReqEnab bit (D13) of the appropriate RxCSR register set to 0. */
					CSK_USBC->TXCSRH &= ~USB_VENUS_TXCSRH_DMAREQENAB_MASK;
				}
			}

#ifdef DOUBLE_PACKET_ENABLE
			CSK_USBC->TXFIFOSZ |= USB_VENUS_FIFOSZ_DPB;
#endif
			break;
		case USB_DC_EP_INTERRUPT:
			LOG_DBG("EP%x setting USB_DC_EP_INTERRUPT", ep);
			/* Clear ISO type bit */
			EDPxReg_SEL(ep_idx);
			CSK_USBC->TXCSRH &= (~USB_VENUS_TXCSRH_ISO_MASK);
			CSK_USBC->RXCSRH &= (~USB_VENUS_RXCSRH_ISO_MASK);
			if (USB_EP_DIR_IS_OUT(ep)) {
				CSK_USBC->RXFIFOSZ &=
					~(USB_VENUS_FIFOSZ_DPB_MASK | USB_VENUS_FIFOSZ_SZ_MASK);
				CSK_USBC->RXFIFOSZ |= GET_FIFOSZ_CFG(ep_mps);
				usb_venus_ctrl.out_ep_ctrl[ep_idx].fifo_size = ep_mps;
				usb_venus_ctrl.out_ep_ctrl[ep_idx].mps = ep_mps;
				usb_venus_ctrl.rxfifo_alloc_addr +=
					ep_mps >> 3; //8bytes per address
				CSK_USBC->RXFIFOADD = usb_venus_ctrl.rxfifo_alloc_addr;

				if (usb_venus_ctrl.out_ep_ctrl[ep_idx].dma_info.enable) {
					//dma multiple packet mode
					CSK_USBC->RXCSRH &= ~(USB_VENUS_RXCSRH_AUTOCLEAR |
							      USB_VENUS_RXCSRH_DMAREQENAB |
							      USB_VENUS_RXCSRH_DMAREQMODE_1);
					CSK_USBC->RXCSRH |= (USB_VENUS_RXCSRH_AUTOCLEAR |
							     USB_VENUS_RXCSRH_DMAREQENAB |
							     USB_VENUS_RXCSRH_DMAREQMODE_1);
				} else {
					//dma single packet mode
					/* The DMAReqEnab bit (D13) of the appropriate RxCSR register set to 0. */
					CSK_USBC->RXCSRH &= ~USB_VENUS_RXCSRH_DMAREQENAB_MASK;
				}
			} else {
				CSK_USBC->TXFIFOSZ &=
					~(USB_VENUS_FIFOSZ_DPB_MASK | USB_VENUS_FIFOSZ_SZ_MASK);
				CSK_USBC->TXFIFOSZ |= GET_FIFOSZ_CFG(ep_mps);
				usb_venus_ctrl.in_ep_ctrl[ep_idx].fifo_size = ep_mps;
				usb_venus_ctrl.in_ep_ctrl[ep_idx].mps = ep_mps;
				usb_venus_ctrl.txfifo_alloc_addr +=
					ep_mps >> 3; //8bytes per address
				CSK_USBC->TXFIFOADD = usb_venus_ctrl.txfifo_alloc_addr;

				if (usb_venus_ctrl.in_ep_ctrl[ep_idx].dma_info.enable) {
					//dma multiple packet mode
					CSK_USBC->TXCSRH &= ~(USB_VENUS_TXCSRH_AUTOSET |
							      USB_VENUS_TXCSRH_DMAREQENAB |
							      USB_VENUS_TXCSRH_DMAREQMODE_1);
					CSK_USBC->TXCSRH |= (USB_VENUS_TXCSRH_AUTOSET |
							     USB_VENUS_TXCSRH_DMAREQENAB |
							     USB_VENUS_TXCSRH_DMAREQMODE_1);
				} else {
					//dma single packet mode
					/* The DMAReqEnab bit (D13) of the appropriate RxCSR register set to 0. */
					CSK_USBC->TXCSRH &= ~USB_VENUS_TXCSRH_DMAREQENAB_MASK;
				}
			}

#ifdef DOUBLE_PACKET_ENABLE
			CSK_USBC->TXFIFOSZ |= USB_VENUS_FIFOSZ_DPB;
#endif
			break;
		case USB_DC_EP_ISOCHRONOUS:
			LOG_DBG("EP%x setting USB_DC_EP_ISOCHRONOUS", ep);
			EDPxReg_SEL(ep_idx);
			if (USB_EP_DIR_IS_OUT(ep)) {
				/* Set ISO type bit */
				CSK_USBC->RXCSRH |= (USB_VENUS_RXCSRH_ISO);
				CSK_USBC->RXFIFOSZ &=
					~(USB_VENUS_FIFOSZ_DPB_MASK | USB_VENUS_FIFOSZ_SZ_MASK);
				CSK_USBC->RXFIFOSZ |= GET_FIFOSZ_CFG(ep_mps);
				usb_venus_ctrl.out_ep_ctrl[ep_idx].fifo_size = ep_mps;
				usb_venus_ctrl.out_ep_ctrl[ep_idx].mps = ep_mps;
				usb_venus_ctrl.rxfifo_alloc_addr +=
					ep_mps >> 3; //8bytes per address
				CSK_USBC->RXFIFOADD = usb_venus_ctrl.rxfifo_alloc_addr;
				LOG_INF("EP%x RX fifo adress is 0x%x, dma status is %d", ep,
					usb_venus_ctrl.rxfifo_alloc_addr,
					usb_venus_ctrl.out_ep_ctrl[ep_idx].dma_info.enable);
				if (usb_venus_ctrl.out_ep_ctrl[ep_idx].dma_info.enable) {
					//dma multiple packet mode
					CSK_USBC->RXCSRH &= ~(USB_VENUS_RXCSRH_AUTOCLEAR |
							      USB_VENUS_RXCSRH_DMAREQENAB |
							      USB_VENUS_RXCSRH_DMAREQMODE_1);
					CSK_USBC->RXCSRH |= (USB_VENUS_RXCSRH_AUTOCLEAR |
							     USB_VENUS_RXCSRH_DMAREQENAB |
							     USB_VENUS_RXCSRH_DMAREQMODE_1);
				} else {
					//dma single packet mode
					/* The DMAReqEnab bit (D13) of the appropriate RxCSR register set to 0. */
					CSK_USBC->RXCSRH &= ~USB_VENUS_RXCSRH_DMAREQENAB_MASK;
				}
			} else {
				/* Set ISO type bit */
				CSK_USBC->TXCSRH |= (USB_VENUS_TXCSRH_ISO);
				CSK_USBC->TXFIFOSZ &=
					~(USB_VENUS_FIFOSZ_DPB_MASK | USB_VENUS_FIFOSZ_SZ_MASK);
				CSK_USBC->TXFIFOSZ |= GET_FIFOSZ_CFG(ep_mps);
				usb_venus_ctrl.in_ep_ctrl[ep_idx].fifo_size = ep_mps;
				usb_venus_ctrl.in_ep_ctrl[ep_idx].mps = ep_mps;
				usb_venus_ctrl.txfifo_alloc_addr +=
					ep_mps >> 3; //8bytes per address
				CSK_USBC->TXFIFOADD = usb_venus_ctrl.txfifo_alloc_addr;
				LOG_INF("EP%x TX fifo adress is 0x%x", ep,
					usb_venus_ctrl.rxfifo_alloc_addr);
				if (usb_venus_ctrl.in_ep_ctrl[ep_idx].dma_info.enable) {
					//dma multiple packet mode
					CSK_USBC->TXCSRH &= ~(USB_VENUS_TXCSRH_AUTOSET |
							      USB_VENUS_TXCSRH_DMAREQENAB |
							      USB_VENUS_TXCSRH_DMAREQMODE_1);
					CSK_USBC->TXCSRH |= (USB_VENUS_TXCSRH_AUTOSET |
							     USB_VENUS_TXCSRH_DMAREQENAB |
							     USB_VENUS_TXCSRH_DMAREQMODE_1);
				} else {
					//dma single packet mode
					/* The DMAReqEnab bit (D13) of the appropriate RxCSR register set to 0. */
					CSK_USBC->TXCSRH &= ~USB_VENUS_TXCSRH_DMAREQENAB_MASK;
				}
			}

#ifdef DOUBLE_PACKET_ENABLE
			CSK_USBC->TXFIFOSZ |= USB_VENUS_FIFOSZ_DPB;
#endif

			break;
		default:
			return -EINVAL;
		}
	}

	return 0;
}

static uint8_t usb_venus_ep_is_enabled(uint8_t ep)
{
	uint8_t ep_idx = USB_EP_GET_IDX(ep);

	/* Check if ep enabled */
	if ((USB_EP_DIR_IS_OUT(ep)) && usb_venus_ctrl.out_ep_ctrl[ep_idx].ep_ena) {
		return 1;
	} else if ((USB_EP_DIR_IS_IN(ep)) && usb_venus_ctrl.in_ep_ctrl[ep_idx].ep_ena) {
		return 1;
	}

	return 0;
}

static int usb_venus_tx_fifo_notempty(int epindex)
{
	if (!epindex) {
		EDPxReg_SEL(epindex);
		return ((CSK_USBC->TXCSRL & USB_VENUS_TXCSRL_FIFONOTEMPTY_MASK) >>
			USB_VENUS_TXCSRL_FIFONOTEMPTY_POS);
	} else {
		return 0;
	}
}

/**
  \fn          int32_t usb_dma_set_channel_active_flag (uint8_t ch)
  \brief       Protected set of channel active flag
  \param[in]   ch        Channel number (0..7 or 4)
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
static int32_t usb_dma_set_channel_active_flag(uint8_t ch)
{
	if (usb_venus_ctrl.channel_active & (1U << ch)) {
		return -1;
	}

	usb_venus_ctrl.channel_active |= (1U << ch);
	return 0;
}

/**
  \fn          void usb_dma_clear_channel_active_flag (uint8_t ch)
  \brief       Protected clear of channel active flag
  \param[in]   ch        Channel number (0..7 or 4)
*/
__inline static void usb_dma_clear_channel_active_flag(uint8_t ch)
{
	usb_venus_ctrl.channel_active &= ~(1U << ch);
}

//dynamically allocate available free DMA channel
//return 0: function succeeded
//return -1: function failed
static int32_t usb_dma_get_free_channel(uint8_t *pch)
{
	uint8_t i, found = 0;

	if (pch == NULL)
		return -1;

	for (i = 0; i < USB_VENUS_DMA_NUMBER_OF_CHANNELS; i++) {
		if ((usb_venus_ctrl.channel_active & (1U << i)) == 0) {
			*pch = i;
			found = 1;
			break;
		}
	}
	return (found ? 0 : -1);
}

static int usb_venus_dma_multiple_packet_setting(uint8_t ep, uint32_t address, uint32_t length)
{
	uint32_t ep_idx, dir;
	ep_idx = USB_EP_GET_IDX(ep);
	dir = USB_EP_GET_DIR(ep) >> 7;

	if (ep_idx == 0) {
		return -EINVAL;
	}

	//check whether dma channel is assigned before
	if (dir == USB_VENUS_DMA_DIR_RX_ENDPOINT) {
		if (usb_venus_ctrl.out_ep_ctrl[ep_idx].dma_info.channel_num !=
		    USB_VENUS_DMA_CHANNEL_NOT_ASSIGNED)
			return 0;
	} else {
		if (usb_venus_ctrl.in_ep_ctrl[ep_idx].dma_info.channel_num !=
		    USB_VENUS_DMA_CHANNEL_NOT_ASSIGNED)
			return 0;
	}

	uint8_t dma_channel = 0;
	int ret;
	ret = usb_dma_get_free_channel(&dma_channel);
	if (ret != 0) {
		LOG_DBG("DMA channel resource is not available");
	}

	if (dir == USB_VENUS_DMA_DIR_RX_ENDPOINT) {
		usb_venus_ctrl.out_ep_ctrl[ep_idx].dma_info.addr = address;
		usb_venus_ctrl.out_ep_ctrl[ep_idx].dma_info.channel_num = dma_channel;
		usb_venus_ctrl.out_ep_ctrl[ep_idx].dma_info.length = length;
	} else {
		usb_venus_ctrl.in_ep_ctrl[ep_idx].dma_info.addr = address;
		usb_venus_ctrl.in_ep_ctrl[ep_idx].dma_info.channel_num = dma_channel;
		usb_venus_ctrl.in_ep_ctrl[ep_idx].dma_info.length = length;
	}

	usb_dma_set_channel_active_flag(dma_channel);

	EDPxReg_SEL(ep_idx);
	/* The DMAReqEnab bit (D13) of the appropriate RxCSR register set to 1. */
	if (dir == USB_VENUS_DMA_DIR_RX_ENDPOINT) {
		CSK_USBC->RXCSRH &= ~(USB_VENUS_RXCSRH_AUTOCLEAR | USB_VENUS_RXCSRH_DMAREQENAB |
				      USB_VENUS_RXCSRH_DMAREQMODE_1);
		CSK_USBC->RXCSRH |= (USB_VENUS_RXCSRH_AUTOCLEAR | USB_VENUS_RXCSRH_DMAREQENAB |
				     USB_VENUS_RXCSRH_DMAREQMODE_1);
	} else {
		CSK_USBC->TXCSRH &= ~(USB_VENUS_TXCSRH_AUTOSET | USB_VENUS_TXCSRH_DMAREQENAB |
				      USB_VENUS_TXCSRH_DMAREQMODE_1);
		CSK_USBC->TXCSRH |= (USB_VENUS_TXCSRH_AUTOSET | USB_VENUS_TXCSRH_DMAREQENAB |
				     USB_VENUS_TXCSRH_DMAREQMODE_1);
	}

	cache_dma_fast_inv_stage1(address, address + length);

	//dma channel index begin from 0
	CSK_USBC->USB_DMA[dma_channel].ADDR = address;
	CSK_USBC->USB_DMA[dma_channel].COUNT = length;
	CSK_USBC->USB_DMA[dma_channel].CNTL =
		USB_VENUS_DMA_CNTL_DMA_ENAB | (dir << USB_VENUS_DMA_CNTL_DMA_DIR_POS) |
		USB_VENUS_DMA_CNTL_DMAMODE_1 | USB_VENUS_DMA_CNTL_DMAIE |
		USB_VENUS_DMA_CNTL_DMA_BRSTM_3 | (ep_idx << USB_VENUS_DMA_CNTL_DMAEP_POS);

	return 0;
}

static int usb_venus_tx(uint8_t ep, const uint8_t *const data, uint32_t data_len)
{
	enum usb_venus_in_ep_idx ep_idx = USB_EP_GET_IDX(ep);
	uint32_t ep_mps = usb_venus_ctrl.in_ep_ctrl[ep_idx].mps;
	uint32_t i;

	LOG_DBG("%s ep %x", __func__, ep);

	/* Wait for FIFO space available */
	while (usb_venus_tx_fifo_notempty(ep_idx))
		;

	/* For now tx-fifo sizes are not configured (cf usb_venus_set_fifo). Here
	 * we force available fifo size to be a multiple of ep mps in order to
	 * prevent splitting data incorrectly.
	 */

	/*
	 * Write data to FIFO, make sure that we are protected against
	 * other USB register accesses.  After starting
	 * to access a FIFO, the application must complete the transaction
	 * before accessing the register."
	 */
	if ((ep_idx == 0) || (usb_venus_ctrl.out_ep_ctrl[ep_idx].dma_info.enable == 0)) {
		if (data_len > ep_mps) {
			data_len = ep_mps;
		}
	}

	//after loading data packet into the FIFO
	if (ep_idx == 0) {
		if (usb_venus_ctrl.status == USB_VENUS_STS_OUT){
			usb_venus_ctrl.status = USB_VENUS_STS_SETUP;
			EDPxReg_SEL(ep_idx);
			CSK_USBC->CSR0L |= USB_VENUS_CSR0L_DATAEND;
			return data_len;
		}

		EDPxReg_SEL(ep_idx);
		CSK_USBC->CSR0L |= USB_VENUS_CSR0L_SERVICEDRXPKTRDY;

		uint8_t *pFifo_addr = (uint8_t *)(&CSK_USBC->FIFOX[ep_idx]);
		for (i = 0U; i < data_len; i++) {
			*pFifo_addr = data[i];
		}

		CSK_USBC->CSR0L |= USB_VENUS_CSR0L_TXPKTRDY;
		if (data_len < ep_mps) {
			//last packet, endpoint0 send dataend
			CSK_USBC->CSR0L |= USB_VENUS_CSR0L_DATAEND;
			if (usb_venus_ctrl.status != USB_VENUS_STS_STATUS){
			usb_venus_ctrl.status = USB_VENUS_STS_SETUP;
			}
			LOG_DBG("USB IN EP%d write %u bytes", ep_idx, data_len);
			return data_len;
		} else {
			//update state machine
			usb_venus_ctrl.status = USB_VENUS_STS_IN;
		}
	} else {
		if (usb_venus_ctrl.out_ep_ctrl[ep_idx].dma_info.enable == 0) {
			//dma mode disable
			uint8_t *pFifo_addr = (uint8_t *)(&CSK_USBC->FIFOX[ep_idx]);
			for (i = 0U; i < data_len; i++) {
				*pFifo_addr = data[i];
			}
			EDPxReg_SEL(ep_idx);
			CSK_USBC->TXCSRL |= USB_VENUS_TXCSRL_TXPKTRDY;
			if(data_len == 0){
				usb_venus_ctrl.status = USB_VENUS_STS_SETUP;
			}

		} else {
			LOG_DBG("USB dam multiple packet setting");
			usb_venus_dma_multiple_packet_setting(ep, (uint32_t)data, data_len);

		}
	}

	LOG_DBG("USB IN EP%d write %u bytes", ep_idx, data_len);
	return data_len;
}

int usb_dc_attach(void)
{
	int ret;

	if (usb_venus_ctrl.attached) {
		return 0;
	}

	ret = usb_venus_init();
	if (ret) {
		return ret;
	}

	usb_venus_ctrl.attached = 1U;

	return 0;
}

int usb_dc_detach(void)
{
	if (!usb_venus_ctrl.attached) {
		return 0;
	}

	//disable usb interrupt(usb_venus_isr_handler)
	irq_disable(DT_INST_IRQN(0));

	/* Enable soft disconnect */
	CSK_USBC->POWER &= ~USB_VENUS_POWER_SOFTCONN;
	usb_venus_ctrl.attached = 0U;

	return 0;
}

int usb_dc_reset(void)
{
	int ret;

	ret = usb_venus_reset();

	/* Clear private data */
	(void)memset(&usb_venus_ctrl, 0, sizeof(usb_venus_ctrl));

	return ret;
}

int usb_dc_set_address(const uint8_t addr)
{
	if (addr > (USB_VENUS_FADDR_ADDR_MASK)) {
		return -EINVAL;
	}

	usb_venus_ctrl.status = USB_VENUS_STS_STATUS;
	usb_venus_ctrl.address = addr;

	return 0;
}

int usb_dc_ep_check_cap(const struct usb_dc_ep_cfg_data *const cfg)
{
	uint8_t ep_idx = USB_EP_GET_IDX(cfg->ep_addr);

	LOG_DBG("ep %x, mps %d, type %d", cfg->ep_addr, cfg->ep_mps, cfg->ep_type);

	if ((cfg->ep_type == USB_DC_EP_CONTROL) && ep_idx) {
		LOG_ERR("invalid endpoint configuration");
		return -1;
	}

	if (cfg->ep_mps > VENUS_USB_MAX_PACKET_SIZE) {
		LOG_WRN("unsupported packet size");
		return -1;
	}

	if ((USB_EP_DIR_IS_OUT(cfg->ep_addr)) && (ep_idx >= USB_VENUS_OUT_EP_NUM)) {
		LOG_WRN("OUT endpoint address out of range");
		return -1;
	}

	if ((USB_EP_DIR_IS_IN(cfg->ep_addr)) && (ep_idx >= USB_VENUS_IN_EP_NUM)) {
		LOG_WRN("IN endpoint address out of range");
		return -1;
	}

	return 0;
}

int usb_dc_ep_configure(const struct usb_dc_ep_cfg_data *const cfg)
{
	uint8_t ep;

	if (!cfg) {
		return -EINVAL;
	}

	ep = cfg->ep_addr;

	if (!usb_venus_ctrl.attached || !usb_venus_ep_is_valid(ep)) {
		LOG_ERR("Not attached / Invalid endpoint: EP 0x%x", ep);
		return -EINVAL;
	}

	usb_venus_ep_set(ep, cfg->ep_mps, cfg->ep_type);

	return 0;
}

int usb_dc_ep_set_stall(const uint8_t ep)
{
	uint8_t ep_idx = USB_EP_GET_IDX(ep);

	if (!usb_venus_ctrl.attached || !usb_venus_ep_is_valid(ep)) {
		LOG_ERR("Not attached / Invalid endpoint: EP 0x%x", ep);
		return -EINVAL;
	}

	//endpoint0
	if (ep_idx == 0) {
		EDPxReg_SEL(ep_idx);
		CSK_USBC->CSR0L |= USB_VENUS_CSR0L_SENDSTALL;
	} else {
		EDPxReg_SEL(ep_idx); //endpoint1-15
		if (USB_EP_DIR_IS_OUT(ep)) {
			CSK_USBC->RXCSRL |= USB_VENUS_RXCSRL_SENDSTALL;
		} else {
			CSK_USBC->TXCSRL |= USB_VENUS_TXCSRL_SENDSTALL;
		}
	}
	return 0;
}

int usb_dc_ep_clear_stall(const uint8_t ep)
{
	uint8_t ep_idx = USB_EP_GET_IDX(ep);

	if (!usb_venus_ctrl.attached || !usb_venus_ep_is_valid(ep)) {
		LOG_ERR("Not attached / Invalid endpoint: EP 0x%x", ep);
		return -EINVAL;
	}

	if (!ep_idx) {
		/* Not possible to clear stall for EP0 */
		return -EINVAL;
	}
	EDPxReg_SEL(ep_idx);
	if (USB_EP_DIR_IS_OUT(ep)) {
		CSK_USBC->RXCSRL &= ~USB_VENUS_RXCSRL_SENTSTALL_MASK;
	} else {
		CSK_USBC->TXCSRL &= ~USB_VENUS_TXCSRL_SENDSTALL_MASK;
	}

	return 0;
}

int usb_dc_ep_halt(const uint8_t ep)
{
	if (!usb_venus_ctrl.attached || !usb_venus_ep_is_valid(ep)) {
		LOG_ERR("Not attached / Invalid endpoint: EP 0x%x", ep);
		return -EINVAL;
	}

	/* Cannot disable EPx, just set stall */
	return usb_dc_ep_set_stall(ep);
}

int usb_dc_ep_is_stalled(const uint8_t ep, uint8_t *const stalled)
{
	uint8_t ep_idx = USB_EP_GET_IDX(ep);

	if (!usb_venus_ctrl.attached || !usb_venus_ep_is_valid(ep)) {
		LOG_ERR("Not attached / Invalid endpoint: EP 0x%x", ep);
		return -EINVAL;
	}

	if (!stalled) {
		return -EINVAL;
	}

	*stalled = 0U;

	EDPxReg_SEL(ep_idx);
	if (ep_idx == 0) {
		if (CSK_USBC->CSR0L & USB_VENUS_CSR0L_SENDSTALL_MASK) {
			*stalled = 1U;
		}
	} else {
		if (USB_EP_DIR_IS_OUT(ep)) {
			if (CSK_USBC->RXCSRL & USB_VENUS_RXCSRL_SENTSTALL_MASK) {
				*stalled = 1U;
			}
		} else {
			if (CSK_USBC->TXCSRL & USB_VENUS_TXCSRL_SENTSTALL_MASK) {
				*stalled = 1U;
			}
		}
	}
	return 0;
}

int usb_dc_ep_enable(const uint8_t ep)
{
	uint8_t ep_idx = USB_EP_GET_IDX(ep);
	LOG_INF("usb_dc_ep_enable");
	
	if (!usb_venus_ctrl.attached || !usb_venus_ep_is_valid(ep)) {
		LOG_ERR("Not attached / Invalid endpoint: EP 0x%x", ep);
		return -EINVAL;
	}

	/* enable EP interrupts */
	if (ep_idx) {
		if (USB_EP_DIR_IS_OUT(ep)) {
			CSK_USBC->INTRRXE |= USB_VENUS_DAINT_IN_EP_INT(ep_idx);
			usb_venus_ctrl.out_ep_ctrl[ep_idx].ep_ena = 1U;
		} else {
			CSK_USBC->INTRTXE |= USB_VENUS_DAINT_OUT_EP_INT(ep_idx);
			usb_venus_ctrl.in_ep_ctrl[ep_idx].ep_ena = 1U;
		}
	} else {
		/* Endpoint0 in/out interrupt enable */
		CSK_USBC->INTRTXE |= USB_VENUS_DAINT_OUT_EP_INT(ep_idx);
		usb_venus_ctrl.out_ep_ctrl[ep_idx].ep_ena = 1U;
		usb_venus_ctrl.in_ep_ctrl[ep_idx].ep_ena = 1U;
	}

	return 0;
}

int usb_dc_ep_disable(const uint8_t ep)
{
	uint8_t ep_idx = USB_EP_GET_IDX(ep);

	if (!usb_venus_ctrl.attached || !usb_venus_ep_is_valid(ep)) {
		LOG_ERR("Not attached / Invalid endpoint: EP 0x%x", ep);
		return -EINVAL;
	}

	/* Disable EP interrupts */
	if (ep_idx) {
		if (USB_EP_DIR_IS_OUT(ep)) {
			CSK_USBC->INTRRXE &= ~USB_VENUS_DAINT_OUT_EP_INT(ep_idx);
		} else {
			CSK_USBC->INTRTXE &= ~USB_VENUS_DAINT_IN_EP_INT(ep_idx);
		}
	} else {
		/* Endpoint0 in/out interrupt disable */
		CSK_USBC->INTRTXE &= ~USB_VENUS_DAINT_OUT_EP_INT(ep_idx);
	}

	/* release fifo */
	if (USB_EP_DIR_IS_OUT(ep)) {
		if (usb_venus_ctrl.rxfifo_alloc_addr>=
		    (usb_venus_ctrl.out_ep_ctrl[ep_idx].fifo_size >> 3)) {
			usb_venus_ctrl.rxfifo_alloc_addr -=
				(usb_venus_ctrl.out_ep_ctrl[ep_idx].fifo_size >> 3);
			LOG_DBG("OUT EP 0x%x fifo address is 0x%x", ep,
				usb_venus_ctrl.rxfifo_alloc_addr);
		} else
		{

			LOG_ERR("EP 0x%x fifo address error", ep);
		}
	} else {
		if ((usb_venus_ctrl.txfifo_alloc_addr-(0x400>>3)) >=
		    (usb_venus_ctrl.in_ep_ctrl[ep_idx].fifo_size >> 3)) {
			usb_venus_ctrl.txfifo_alloc_addr -=
				(usb_venus_ctrl.in_ep_ctrl[ep_idx].fifo_size >> 3);
			LOG_DBG("IN EP 0x%x fifo address is 0x%x", ep,
				usb_venus_ctrl.txfifo_alloc_addr);
		} else
		{
			LOG_ERR("EP 0x%x fifo address error", ep);

		}
	}

	/* De-activate, disable and set NAK for Ep */
	if (USB_EP_DIR_IS_OUT(ep)) {
		usb_venus_ctrl.out_ep_ctrl[ep_idx].ep_ena = 0U;
	} else {
		usb_venus_ctrl.in_ep_ctrl[ep_idx].ep_ena = 0U;
	}

	return 0;
}

int usb_dc_ep_flush(const uint8_t ep)
{
	uint8_t ep_idx = USB_EP_GET_IDX(ep);

	if (!usb_venus_ctrl.attached || !usb_venus_ep_is_valid(ep)) {
		LOG_ERR("Not attached / Invalid endpoint: EP 0x%x", ep);
		return -EINVAL;
	}

	/* Each endpoint has dedicated Tx FIFO */
	EDPxReg_SEL(ep_idx);
	if (!ep_idx) {
		if (USB_EP_DIR_IS_OUT(ep)) {
			CSK_USBC->RXCSRL &= ~USB_VENUS_RXCSRL_FLUSHFIFO;
		} else {
			CSK_USBC->TXCSRL &= ~USB_VENUS_TXCSRL_FLUSHFIFO;
		}
	} else {
		/* Endpoint0 fifo flush */
		CSK_USBC->CSR0H &= ~USB_VENUS_CSR0H_FLUSHFIFO;
	}

	return 0;
}

int usb_dc_ep_write(const uint8_t ep, const uint8_t *const data, const uint32_t data_len,
		    uint32_t *const ret_bytes)
{
	int ret;

	if (!usb_venus_ctrl.attached || !usb_venus_ep_is_valid(ep)) {
		LOG_ERR("Not attached / Invalid endpoint: EP 0x%x", ep);
		return -EINVAL;
	}

	/* Check if IN ep */
	if (USB_EP_GET_DIR(ep) != USB_EP_DIR_IN) {
		return -EINVAL;
	}

	/* Check if ep enabled */
	if (!usb_venus_ep_is_enabled(ep)) {
		return -EINVAL;
	}

	ret = usb_venus_tx(ep, data, data_len);
	if (ret < 0) {
		return ret;
	}

	if (ret_bytes) {
		*ret_bytes = ret;
	}

	/* !!!!!Change index to EP0, otherwise EP0 control transfer will not respond when index is not zero!!!!! */
	EDPxReg_SEL(0);
	return 0;
}

int usb_dc_ep_read_wait(uint8_t ep, uint8_t *data, uint32_t max_data_len, uint32_t *read_bytes)
{
	uint8_t ep_idx = USB_EP_GET_IDX(ep);
	uint32_t i, j, data_len, bytes_to_copy;
	struct usb_setup_packet *setup_tmp = NULL;

	if (!usb_venus_ctrl.attached || !usb_venus_ep_is_valid(ep)) {
		LOG_ERR("Not attached / Invalid endpoint: EP 0x%x", ep);
		return -EINVAL;
	}

	/* Check if OUT ep */
	if (USB_EP_GET_DIR(ep) != USB_EP_DIR_OUT) {
		LOG_ERR("Wrong endpoint direction");
		return -EINVAL;
	}

	/* Allow to read 0 bytes */
	if (!data && max_data_len) {
		LOG_ERR("Wrong arguments");
		return -EINVAL;
	}

	data_len = usb_venus_ctrl.out_ep_ctrl[ep_idx].data_len; //received packet count in FIFO
	if (data_len > max_data_len) {
		LOG_ERR("Too much rx data!, data_len: %d, max_data_len: %d", data_len, max_data_len);
		bytes_to_copy = max_data_len;
	} else {
		bytes_to_copy = data_len;
	}

	LOG_DBG("Read EP%d, req %d, read %d bytes", ep, max_data_len, bytes_to_copy);

	if (ep_idx == 0) {
		/* Data in the FIFOs to be read per 32-bit words */
		for (i = 0U; i < (bytes_to_copy & ~0x3); i += 4U) {
			*(uint32_t *)(data + i) = CSK_USBC->FIFOX[ep_idx];
		}
		if (bytes_to_copy & 0x3) {
			/* Not multiple of 4 */
			uint32_t last_dw = CSK_USBC->FIFOX[ep_idx];

			for (j = 0U; j < (bytes_to_copy & 0x3); j++) {
				*(data + i + j) = (sys_cpu_to_le32(last_dw) >> (j * 8U)) & 0xFF;
			}
		}

		if (usb_venus_ctrl.status == USB_VENUS_STS_SETUP) {
			setup_tmp = (struct usb_setup_packet *)data;
			if ((setup_tmp->wLength > 0) &&
			    (USB_REQTYPE_GET_DIR(setup_tmp->bmRequestType) == USB_REQTYPE_DIR_TO_DEVICE)) {
				//data phase expected
				usb_venus_ctrl.status = USB_VENUS_STS_OUT;
			}
		} else if (usb_venus_ctrl.status == USB_VENUS_STS_OUT) {
			//OUT data phase
			if (bytes_to_copy < usb_venus_ctrl.out_ep_ctrl[ep_idx].mps) {
				//return to setup phase
				usb_venus_ctrl.status = USB_VENUS_STS_SETUP;
			}
		}
	} else {
		if (usb_venus_ctrl.out_ep_ctrl[ep_idx].dma_info.enable == 0) {
			/* Data in the FIFOs to be read per 32-bit words */
			//dma mode disable
			for (i = 0U; i < (bytes_to_copy & ~0x3); i += 4U) {
				*(uint32_t *)(data + i) = CSK_USBC->FIFOX[ep_idx];
			}
			if (bytes_to_copy & 0x3) {
				/* Not multiple of 4 */
				uint32_t last_dw = CSK_USBC->FIFOX[ep_idx];

				for (j = 0U; j < (bytes_to_copy & 0x3); j++) {
					*(data + i + j) =
						(sys_cpu_to_le32(last_dw) >> (j * 8U)) & 0xFF;
				}
			}

			// LOG_HEXDUMP_DBG(data, bytes_to_copy, "raw data:");
		} else {
			//			if(DMA_MULTIPLE){
			//dma channel index begin from 0
			if (usb_venus_dma_multiple_packet_setting(ep, (uint32_t)data,
								  max_data_len)) {
				LOG_ERR("Wrong arguments");
				return -EINVAL;
			}
			//			}
			//			else{
			//				//dma single packet
			//				if(usb_venus_dma_single_packet_setting(ep, (uint32_t)data, max_data_len)){
			//					LOG_ERR("Wrong arguments");
			//					return -EINVAL;
			//				}
			//			}
		}
	}

	if (read_bytes) {
		*read_bytes = bytes_to_copy;
	}

	return 0;
}

int usb_dc_ep_read_continue(uint8_t ep)
{
	uint8_t ep_idx = USB_EP_GET_IDX(ep);

	if (!usb_venus_ctrl.attached || !usb_venus_ep_is_valid(ep)) {
		LOG_ERR("Not attached / Invalid endpoint: EP 0x%x", ep);
		return -EINVAL;
	}

	/* Check if OUT ep */
	if (USB_EP_GET_DIR(ep) != USB_EP_DIR_OUT) {
		LOG_ERR("Wrong endpoint direction");
		return -EINVAL;
	}

	if(ep_idx == 0){
		if (usb_venus_ctrl.status == USB_VENUS_STS_OUT){
			EDPxReg_SEL(ep_idx);
			CSK_USBC->CSR0L |= USB_VENUS_CSR0L_SERVICEDRXPKTRDY;
		}
	}else{
		if (usb_venus_ctrl.out_ep_ctrl[ep_idx].dma_info.enable == 0) {
			EDPxReg_SEL(ep_idx);
			CSK_USBC->RXCSRL &= ~USB_VENUS_RXCSRL_RXPKTRDY_MASK;
		}
	}

	/* !!!!!Change index to EP0, otherwise EP0 control transfer will not respond when index is not zero!!!!! */
	EDPxReg_SEL(0);

	return 0;
}

int usb_dc_ep_read(const uint8_t ep, uint8_t *const data, const uint32_t max_data_len,
		   uint32_t *const read_bytes)
{
	if (usb_dc_ep_read_wait(ep, data, max_data_len, read_bytes) != 0) {
		return -EINVAL;
	}

	if (!data && !max_data_len) {
		/* When both buffer and max data to read are zero the above
		 * call would fetch the data len and we simply return.
		 */
		return 0;
	}

	if (usb_dc_ep_read_continue(ep) != 0) {
		return -EINVAL;
	}

	return 0;
}

int usb_dc_ep_set_callback(const uint8_t ep, const usb_dc_ep_callback cb)
{
	uint8_t ep_idx = USB_EP_GET_IDX(ep);

	if (!usb_venus_ctrl.attached || !usb_venus_ep_is_valid(ep)) {
		LOG_ERR("Not attached / Invalid endpoint: EP 0x%x", ep);
		return -EINVAL;
	}

	if (USB_EP_DIR_IS_IN(ep)) {
		usb_venus_ctrl.in_ep_ctrl[ep_idx].cb = cb;
	} else {
		usb_venus_ctrl.out_ep_ctrl[ep_idx].cb = cb;
	}

	return 0;
}

int usb_dc_ep_set_dma_callback(const uint8_t ep, const usb_dc_ep_callback cb)
{
	uint8_t ep_idx = USB_EP_GET_IDX(ep);

	if (!usb_venus_ctrl.attached || !usb_venus_ep_is_valid(ep)) {
		LOG_ERR("Not attached / Invalid endpoint: EP 0x%x", ep);
		return -EINVAL;
	}

	if (USB_EP_DIR_IS_IN(ep)) {
		usb_venus_ctrl.in_ep_ctrl[ep_idx].dma_info.cb = cb;
	} else {
		usb_venus_ctrl.out_ep_ctrl[ep_idx].dma_info.cb = cb;
	}

	return 0;
}

int usb_dc_ep_enable_dma(const uint8_t ep)
{
	uint8_t ep_idx = USB_EP_GET_IDX(ep);

	if (!usb_venus_ctrl.attached || !usb_venus_ep_is_valid(ep)) {
		LOG_ERR("Not attached / Invalid endpoint: EP 0x%x", ep);
		return -EINVAL;
	}

	if (USB_EP_DIR_IS_IN(ep)) {
		usb_venus_ctrl.in_ep_ctrl[ep_idx].dma_info.enable = true;
	} else {
		usb_venus_ctrl.out_ep_ctrl[ep_idx].dma_info.enable = true;
	}

	return 0;
}

int usb_dc_ep_disable_dma(const uint8_t ep)
{
	uint32_t ep_idx, dir, dma_channel;
	ep_idx = USB_EP_GET_IDX(ep);
	dir = USB_EP_GET_DIR(ep) >> 7;

	if (ep_idx == 0) {
		return -EINVAL;
	}

	if (!usb_venus_ctrl.attached || !usb_venus_ep_is_valid(ep)) {
		LOG_ERR("Not attached / Invalid endpoint: EP 0x%x", ep);
		return -EINVAL;
	}

	if (USB_EP_DIR_IS_IN(ep)) {
		usb_venus_ctrl.in_ep_ctrl[ep_idx].dma_info.enable = false;
		dma_channel = usb_venus_ctrl.in_ep_ctrl[ep_idx].dma_info.channel_num;

		usb_venus_ctrl.in_ep_ctrl[ep_idx].dma_info.addr = 0;
		usb_venus_ctrl.in_ep_ctrl[ep_idx].dma_info.channel_num =
			USB_VENUS_DMA_CHANNEL_NOT_ASSIGNED;
		usb_venus_ctrl.in_ep_ctrl[ep_idx].dma_info.length = 0;

	} else {
		usb_venus_ctrl.out_ep_ctrl[ep_idx].dma_info.enable = false;
		dma_channel = usb_venus_ctrl.out_ep_ctrl[ep_idx].dma_info.channel_num;

		usb_venus_ctrl.out_ep_ctrl[ep_idx].dma_info.addr = 0;
		usb_venus_ctrl.out_ep_ctrl[ep_idx].dma_info.channel_num =
			USB_VENUS_DMA_CHANNEL_NOT_ASSIGNED;
		usb_venus_ctrl.out_ep_ctrl[ep_idx].dma_info.length = 0;
	}
	usb_dma_clear_channel_active_flag(dma_channel);
	CSK_USBC->USB_DMA[dma_channel].ADDR = 0;
	CSK_USBC->USB_DMA[dma_channel].COUNT = 0;
	CSK_USBC->USB_DMA[dma_channel].CNTL = 0;

	return 0;
}

void usb_dc_set_status_callback(const usb_dc_status_callback cb)
{
	usb_venus_ctrl.status_cb = cb;
}

int usb_dc_ep_mps(const uint8_t ep)
{
	enum usb_venus_out_ep_idx ep_idx = USB_EP_GET_IDX(ep);

	if (!usb_venus_ctrl.attached || !usb_venus_ep_is_valid(ep)) {
		LOG_ERR("Not attached / Invalid endpoint: EP 0x%x", ep);
		return -EINVAL;
	}

	if (USB_EP_DIR_IS_OUT(ep)) {
		return usb_venus_ctrl.out_ep_ctrl[ep_idx].mps;
	} else {
		return usb_venus_ctrl.in_ep_ctrl[ep_idx].mps;
	}
}

/* Handle interrupts on a control endpoint */
static void usb_venus_ep0_isr(void)
{
	uint8_t ep_idx = 0;
	usb_dc_ep_callback ep_cb;
	struct cb_msg msg;

	ep_cb = usb_venus_ctrl.out_ep_ctrl[ep_idx].cb;

	//select endpoint0
	EDPxReg_SEL(ep_idx);
	//endpoint0 setupend interrupt
	if (CSK_USBC->CSR0L & USB_VENUS_CSR0L_SETUPEND) {
		CSK_USBC->CSR0L |= USB_VENUS_CSR0L_SERVICEDSETUPEND;
		usb_venus_ctrl.status = USB_VENUS_STS_SETUP;
		LOG_DBG("Endpoint0 setupend interrupt generated");
	}

	if (CSK_USBC->CSR0L & USB_VENUS_CSR0L_SENTSTALL) {
		CSK_USBC->CSR0L &= ~USB_VENUS_CSR0L_SENTSTALL;
		usb_venus_ctrl.status = USB_VENUS_STS_SETUP;
		LOG_DBG("Endpoint0 sentstall interrupt generated");
	}

	switch (usb_venus_ctrl.status) {
	case USB_VENUS_STS_SETUP:
		/* Call the registered callback if any */
		LOG_DBG("usb ep0 setup phase(idle state)");
		if (ep_cb) {
			//rx packet ready & rx packet is not Zero-byte
			if (CSK_USBC->CSR0L & USB_VENUS_CSR0L_RXPKTRDY) {
				usb_venus_ctrl.out_ep_ctrl[ep_idx].data_len =
					CSK_USBC->COUNT0 & USB_VENUS_COUNT0_RXCOUNT_MASK;
				LOG_DBG("USB EP%u: usb_venus_ctrl status %u, size %u", ep_idx,
					usb_venus_ctrl.status,
					usb_venus_ctrl.out_ep_ctrl[ep_idx].data_len);
				if (usb_venus_ctrl.out_ep_ctrl[ep_idx].data_len != 0) {
					msg.cb = USB_DC_EP_SETUP;
					msg.ep = ep_idx;
					msg.type = USB_DC_CB_TYPE_EP;
					k_msgq_put(&usb_dc_msgq, &msg, K_NO_WAIT);
				} else {
					//setup state and zero rxpkt for ep0 interrupt
					CSK_USBC->CSR0L |= USB_VENUS_CSR0L_SERVICEDRXPKTRDY;
					LOG_DBG("Zero-byte DATA1 packet sent by host, RxPktRdy is set");
				}
			} else {
				//setup state and zero rxpkt for ep0 interrupt
				LOG_DBG("Zero-byte DATA1 packet sent by host, RxPktRdy is reset");
			}
		}
		break;
	case USB_VENUS_STS_OUT:
		LOG_DBG("usb ep0 out phase(rx state)");
		if (ep_cb) {
			if (CSK_USBC->CSR0L & USB_VENUS_CSR0L_RXPKTRDY) {
				usb_venus_ctrl.out_ep_ctrl[ep_idx].data_len =
					CSK_USBC->COUNT0 & USB_VENUS_COUNT0_RXCOUNT_MASK;

				msg.cb = USB_DC_EP_DATA_OUT;
				msg.ep = ep_idx;
				msg.type = USB_DC_CB_TYPE_EP;
				k_msgq_put(&usb_dc_msgq, &msg, K_NO_WAIT);
			}
		}
		break;
	case USB_VENUS_STS_IN:
		LOG_DBG("usb ep0 in phase(tx state)");
		if (ep_cb) {

			msg.cb = USB_DC_EP_DATA_IN;
			msg.ep = ep_idx;
			msg.type = USB_DC_CB_TYPE_EP;
			k_msgq_put(&usb_dc_msgq, &msg, K_NO_WAIT);
			// ep_cb(USB_EP_GET_ADDR(ep_idx, USB_EP_DIR_IN), USB_DC_EP_DATA_IN);
		}
		break;
	case USB_VENUS_STS_STATUS:
		LOG_DBG("usb ep0 status phase");
		CSK_USBC->FADDR = (usb_venus_ctrl.address) & USB_VENUS_FADDR_ADDR_MASK;
		usb_venus_ctrl.status = USB_VENUS_STS_SETUP;
		break;
	default:
		break;
	}
}

//transfer last packet
static inline void usb_venus_dma_oep_remainpkt_handler(uint8_t ep,
						       enum usb_dc_ep_cb_status_code ep_status)
{
	LOG_DBG("usb_venus_dma_oep_remainpkt_handler");
	LOG_DBG("dma multiple transfer receive remain packets handle");

	/* The DMAReqEnab bit (D13) of the appropriate RxCSR register set to 0. */
	//dma single packet register setting
	uint32_t ep_idx, dir, dma_channel;
	ep_idx = USB_EP_GET_IDX(ep);
	dir = USB_EP_GET_DIR(ep) >> 7;

	EDPxReg_SEL(ep_idx);
	CSK_USBC->RXCSRH &= ~(USB_VENUS_RXCSRH_AUTOCLEAR | USB_VENUS_RXCSRH_DMAREQENAB |
			      USB_VENUS_RXCSRH_DMAREQMODE_1);

	dma_channel = usb_venus_ctrl.out_ep_ctrl[ep_idx].dma_info.channel_num;

	//dma channel index begin from 0
	CSK_USBC->USB_DMA[dma_channel].CNTL =
		USB_VENUS_DMA_CNTL_DMA_ENAB | (dir << USB_VENUS_DMA_CNTL_DMA_DIR_POS) |
		USB_VENUS_DMA_CNTL_DMAMODE_0 | USB_VENUS_DMA_CNTL_DMAIE |
		USB_VENUS_DMA_CNTL_DMA_BRSTM_3 | (ep_idx << USB_VENUS_DMA_CNTL_DMAEP_POS);
}

static inline void usb_venus_int_oep_handler(uint32_t intsr)
{
	uint32_t ep_int_status;
	uint8_t ep_idx;
	usb_dc_ep_callback ep_cb;
	struct cb_msg msg;
	LOG_DBG("usb_venus_int_oep_handler");

	for (ep_idx = 1U; ep_idx < USB_VENUS_OUT_EP_NUM; ep_idx++) {
		if (intsr & (USB_VENUS_INTRRX_EP1_5_POS << ep_idx)) {
			/* Read OUT RX EP interrupt status */
			EDPxReg_SEL(ep_idx);
			ep_int_status = CSK_USBC->RXCSRL;
			usb_venus_ctrl.out_ep_ctrl[ep_idx].data_len = CSK_USBC->RXCOUNT;

			LOG_DBG("USB OUT EP%u interrupt status: 0x%x data_length: 0x%x", ep_idx,
				ep_int_status, usb_venus_ctrl.out_ep_ctrl[ep_idx].data_len);

			if (usb_venus_ctrl.out_ep_ctrl[ep_idx].dma_info.enable) {
				ep_cb = usb_venus_dma_oep_remainpkt_handler;
			} else {
				ep_cb = usb_venus_ctrl.out_ep_ctrl[ep_idx].cb;
			}

			if (ep_cb && (ep_int_status & USB_VENUS_RXCSRL_RXPKTRDY)) {
				msg.cb = USB_DC_EP_DATA_OUT;
				msg.ep = ep_idx;
				msg.type = USB_DC_CB_TYPE_EP;
				k_msgq_put(&usb_dc_msgq, &msg, K_NO_WAIT);
			}

			/* !!!!!Change index to EP0, otherwise EP0 control transfer will not respond when index is not zero!!!!! */
			EDPxReg_SEL(0);
		}
	}
	/* Clear interrupt. */
}

static inline void usb_venus_int_iep_handler(uint32_t intsr)
{
	uint32_t ep_int_status;
	uint8_t ep_idx;
	usb_dc_ep_callback ep_cb;
	struct cb_msg msg;

	LOG_DBG("usb_venus_int_iep_handler");
	for (ep_idx = 1U; ep_idx < USB_VENUS_IN_EP_NUM; ep_idx++) {
		if (intsr & (USB_VENUS_INTRTX_EP1_5_POS << ep_idx)) {
			/* Read IN TX EP interrupt status */
			EDPxReg_SEL(ep_idx);
			ep_int_status = CSK_USBC->TXCSRL;

			LOG_DBG("USB IN EP%u interrupt status: 0x%x", ep_idx, ep_int_status);

			ep_cb = usb_venus_ctrl.in_ep_ctrl[ep_idx].cb;
			if (ep_cb && ((ep_int_status & USB_VENUS_TXCSRL_TXPKTRDY) == 0)) {

				msg.cb = USB_DC_EP_DATA_IN;
				msg.ep = ep_idx;
				msg.type = USB_DC_CB_TYPE_EP;
				k_msgq_put(&usb_dc_msgq, &msg, K_NO_WAIT);
			}

			/* !!!!!Change index to EP0, otherwise EP0 control transfer will not respond when index is not zero!!!!! */
			EDPxReg_SEL(0);
		}
	}

	/* Clear interrupt. */
}

static void usb_venus_dma_isr(uint32_t dmaintsr)
{
	LOG_DBG("usb_venus_dma_isr");
	uint32_t ep_idx, start, end;

	for (uint32_t dma_channel = 0; dma_channel < 6; dma_channel++) {
		if (((CSK_USBC->USB_DMA[dma_channel].CNTL & USB_VENUS_DMA_CNTL_DMAIE) >>
		     USB_VENUS_DMA_CNTL_DMAIE_POS) &&
		    ((dmaintsr & (0x01 << dma_channel)) >> dma_channel)) {
			//interrupt enabled and interrupt flag is set
			if ((CSK_USBC->USB_DMA[dma_channel].CNTL & USB_VENUS_DMA_CNTL_DMA_DIR) ==
			    0) {
				ep_idx = (CSK_USBC->USB_DMA[dma_channel].CNTL &
					  USB_VENUS_DMA_CNTL_DMAEP) >>
					 USB_VENUS_DMA_CNTL_DMAEP_POS;

				usb_dma_clear_channel_active_flag(dma_channel);
				usb_venus_ctrl.out_ep_ctrl[ep_idx].dma_info.channel_num =
					USB_VENUS_DMA_CHANNEL_NOT_ASSIGNED;
				start = usb_venus_ctrl.out_ep_ctrl[ep_idx].dma_info.addr;
				end = usb_venus_ctrl.out_ep_ctrl[ep_idx].dma_info.addr +
				      usb_venus_ctrl.out_ep_ctrl[ep_idx].dma_info.length;
				cache_dma_fast_inv_stage2(start, end);

				EDPxReg_SEL(ep_idx);
				CSK_USBC->RXCSRL &= ~USB_VENUS_RXCSRL_RXPKTRDY_MASK;
				if (usb_venus_ctrl.out_ep_ctrl[ep_idx].dma_info.cb) {
					usb_venus_ctrl.out_ep_ctrl[ep_idx].dma_info.cb(
						USB_EP_GET_ADDR(ep_idx, USB_EP_DIR_OUT),
						USB_DC_EP_DATA_OUT);
				}
				/* !!!!!Change index to EP0, otherwise EP0 control transfer will not respond when index is not zero!!!!! */
				EDPxReg_SEL(0);
			} else {
				//handle "last short packet", set TXPKTRDY in interrupt handle
				ep_idx = (CSK_USBC->USB_DMA[dma_channel].CNTL &
					  USB_VENUS_DMA_CNTL_DMAEP) >>
					 USB_VENUS_DMA_CNTL_DMAEP_POS;
				EDPxReg_SEL(ep_idx);
				CSK_USBC->TXCSRL |= USB_VENUS_TXCSRL_TXPKTRDY;
				usb_dma_clear_channel_active_flag(dma_channel);
				usb_venus_ctrl.in_ep_ctrl[ep_idx].dma_info.channel_num =
					USB_VENUS_DMA_CHANNEL_NOT_ASSIGNED;
				start = usb_venus_ctrl.in_ep_ctrl[ep_idx].dma_info.addr;
				end = usb_venus_ctrl.in_ep_ctrl[ep_idx].dma_info.addr +
				      usb_venus_ctrl.in_ep_ctrl[ep_idx].dma_info.length;
				cache_dma_fast_inv_stage2(start, end);

				if (usb_venus_ctrl.in_ep_ctrl[ep_idx].dma_info.cb) {
					usb_venus_ctrl.in_ep_ctrl[ep_idx].dma_info.cb(
						USB_EP_GET_ADDR(ep_idx, USB_EP_DIR_IN),
						USB_DC_EP_DATA_IN);
				}
				/* !!!!!Change index to EP0, otherwise EP0 control transfer will not respond when index is not zero!!!!! */
				EDPxReg_SEL(0);
			}
		}
	}
}

static void usb_venus_handle_reset(void)
{
	LOG_DBG("USB RESET event");
	struct cb_msg msg;
	/* Inform upper layers */
	usb_venus_ctrl.address = 0;

	usb_venus_ctrl.status = USB_VENUS_STS_SETUP;

	//CSK_USBC->SOFT_RST |= (USB_VENUS_SOFT_RST_NRST | USB_VENUS_SOFT_RST_NRSTX);
	/* enable global EP interrupts */
	/* enable EP0 interrupts */
	CSK_USBC->INTRTXE |= USB_VENUS_INTRTX_EP0;

	/* Set device speed to Full Speed */
	CSK_USBC->POWER &= ~USB_VENUS_POWER_HSENABLE; //Full speed enable

	/* Enable soft connect */
	CSK_USBC->POWER |= USB_VENUS_POWER_SOFTCONN;

	/* Enable usb module interrupt */
	CSK_USBC->INTRUSBE = 0x00;
	//	CSK_USBC->INTRUSBE |= USB_VENUS_INTRUSBE_RESET | USB_VENUS_INTRUSBE_SOF;
	CSK_USBC->INTRUSBE |= USB_VENUS_INTRUSBE_RESET;	
	
	/* Inform upper layers */
	if (usb_venus_ctrl.status_cb) {

		msg.ep = 0U;
		msg.type = USB_DC_CB_TYPE_MGMT;
		msg.cb = USB_DC_RESET;
		k_msgq_put(&usb_dc_msgq, &msg, K_NO_WAIT);
	}
}

static void usb_venus_isr_handler(const void *unused)
{
	uint32_t txsr, rxsr, intsr, dmaintsr, dmaie = 0;
	struct cb_msg msg;

	//endpoint & common interrupt status -> read clear
	intsr = CSK_USBC->INTRUSB & CSK_USBC->INTRUSBE;
	txsr = CSK_USBC->INTRTX & CSK_USBC->INTRTXE;
	rxsr = CSK_USBC->INTRRX & CSK_USBC->INTRRXE;
	for (uint32_t i = 0; i < 6; i++) {
		dmaie |= (((CSK_USBC->USB_DMA[i].CNTL & USB_VENUS_DMA_CNTL_DMAIE) >>
			   USB_VENUS_DMA_CNTL_DMAIE_POS)
			  << i);
	}
	dmaintsr = CSK_USBC->DMA_INTR & dmaie;

	ARG_UNUSED(unused);

	//	LOG_DBG("USB venus interrupt handler entered");
	// LOG_DBG("USB INTRUSB= 0x%x, INTRTX = 0x%x, INTRRX = 0x%x, DMA_INTR = 0x%x", intsr, txsr,
	// 	rxsr, dmaintsr);

	if (intsr & USB_VENUS_INTRUSB_RESET) {
		/* Reset detected */
		usb_venus_handle_reset();
	}

	if (intsr & USB_VENUS_INTRUSB_SUSPEND) {
		LOG_DBG("usb suspend interrupt");
		msg.cb = USB_DC_SUSPEND;
		msg.ep = 0;
		msg.type = USB_DC_CB_TYPE_MGMT;
		k_msgq_put(&usb_dc_msgq, &msg, K_NO_WAIT);
	}

	if (intsr & USB_VENUS_INTRUSB_RESUME) {
		LOG_DBG("usb resume interrupt");
		msg.cb = USB_DC_RESUME;
		msg.ep = 0;
		msg.type = USB_DC_CB_TYPE_MGMT;
		k_msgq_put(&usb_dc_msgq, &msg, K_NO_WAIT);
	}

	if (intsr & USB_VENUS_INTRUSB_SOF) {
		LOG_DBG("usb sof interrupt");
		
		msg.cb = USB_DC_SOF;
		msg.ep = 0;
		msg.type = USB_DC_CB_TYPE_MGMT;
		k_msgq_put(&usb_dc_msgq, &msg, K_NO_WAIT);
	}

	if (dmaintsr & USB_VENUS_DMA_INTR_INT0_5_MASK) {
		LOG_DBG("usb dma interrupt");
		usb_venus_dma_isr(dmaintsr);
	}

	/* EP0 tx&rx endpoint interrupt */
	if (txsr & USB_VENUS_INTRTX_EP0_MASK) {
		//		LOG_DBG("usb endpoint 0 interrupt");
		usb_venus_ep0_isr();
	}

	/* EP1-5 tx endpoint interrupt for IN endpoint */
	if (txsr & USB_VENUS_INTRTX_EP1_5_MASK) {
		LOG_DBG("usb endpoint x IN interrupt");
		usb_venus_int_iep_handler(txsr);
	}

	/* EP1-5 rx endpoint interrupt for OUT endpoint */
	if (rxsr & USB_VENUS_INTRRX_EP1_5_MASK) {
		LOG_DBG("usb endpoint x OUT interrupt");
		usb_venus_int_oep_handler(rxsr);
	}

}



static void usb_csk6001_thread_main(void *arg1, void *unused1, void *unused2)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(unused1);
	ARG_UNUSED(unused2);
	struct cb_msg msg;
	uint8_t ep_addr;
	uint8_t ep_idx;

	while (true) {
		k_msgq_get(&usb_dc_msgq, &msg, K_FOREVER);
		ep_idx = msg.ep;
		if (msg.type == USB_DC_CB_TYPE_EP) {
			switch (msg.cb) {
			case USB_DC_EP_SETUP:
				ep_addr = USB_EP_GET_ADDR(msg.ep, USB_EP_DIR_OUT);
				if (usb_venus_ctrl.out_ep_ctrl[ep_idx].cb) {
					usb_venus_ctrl.out_ep_ctrl[ep_idx].cb(ep_addr,
						USB_DC_EP_SETUP);
				}
				break;
			case USB_DC_EP_DATA_OUT:
				ep_addr = USB_EP_GET_ADDR(msg.ep, USB_EP_DIR_OUT);
				if (usb_venus_ctrl.out_ep_ctrl[ep_idx].cb) {
					usb_venus_ctrl.out_ep_ctrl[ep_idx].cb(ep_addr,
						USB_DC_EP_DATA_OUT);
				}
				break;
			case USB_DC_EP_DATA_IN:
				ep_addr = USB_EP_GET_ADDR(msg.ep, USB_EP_DIR_IN);
				if (usb_venus_ctrl.in_ep_ctrl[ep_idx].cb) {
					usb_venus_ctrl.in_ep_ctrl[ep_idx].cb(ep_addr,
						USB_DC_EP_DATA_IN);
				}
				break;
			default:
				LOG_ERR("unknown msg");
				break;
			}
		} else if (msg.type == USB_DC_CB_TYPE_MGMT) {
			switch (msg.cb) {
			case USB_DC_RESET:
				if (usb_venus_ctrl.status_cb) {
					usb_venus_ctrl.status_cb(USB_DC_RESET, NULL);
				}
				break;
			case USB_DC_ERROR:
				if (usb_venus_ctrl.status_cb) {
					usb_venus_ctrl.status_cb(USB_DC_ERROR, NULL);
				}
				break;
			case USB_DC_SUSPEND:
				if (usb_venus_ctrl.status_cb) {
					usb_venus_ctrl.status_cb(USB_DC_SUSPEND, NULL);
				}
				break;
			case USB_DC_RESUME:
				if (usb_venus_ctrl.status_cb) {
					usb_venus_ctrl.status_cb(USB_DC_RESUME, NULL);
				}
				break;
			case USB_DC_SOF:
				if (usb_venus_ctrl.status_cb) {
					usb_venus_ctrl.status_cb(USB_DC_SOF, NULL);
				}
				break;
			default:
				LOG_ERR("unknown msg");
				break;
			}
		}
	}
}
