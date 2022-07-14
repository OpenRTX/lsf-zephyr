/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <init.h>
#include <sys/byteorder.h>

#include <drivers/bluetooth/hci_driver.h>

#include "spi_drv.h"
#include "netdev_api.h"
#include "vhci_if.h"
#include "hosted/hosted.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(csk_ble, CONFIG_BT_LOG_LEVEL);

#define BLE_INIT_TIMEOUT		K_SECONDS(3)

#define HCI_CMD                 0x01
#define HCI_ACL                 0x02
#define HCI_SCO                 0x03
#define HCI_EVT                 0x04
#define HCI_ISO                 0x05

static vhci_handle_t *vhci_hdl;

static int bt_csk_send(struct net_buf *buf)
{
	int err = 0;
	uint8_t pkt_indicator;

	switch (bt_buf_get_type(buf)) {
	case BT_BUF_ACL_OUT:
		pkt_indicator = HCI_ACL;
		break;
	case BT_BUF_CMD:
		pkt_indicator = HCI_CMD;
		break;
	case BT_BUF_ISO_OUT:
		pkt_indicator = HCI_ISO;
		break;
	default:
		LOG_ERR("Unknown type %u", bt_buf_get_type(buf));
		goto done;
	}
	net_buf_push_u8(buf, pkt_indicator);

	LOG_HEXDUMP_DBG(buf->data, buf->len, "Final HCI buffer:");

	uint8_t *send_buf = k_malloc(buf->len);
	memcpy(send_buf, buf->data, buf->len);
	vhci_hdl->fops->write(vhci_hdl, send_buf, buf->len);

done:
	net_buf_unref(buf);
	return err;
}


static int bt_csk_open(void)
{
	int err;
	err = vhci_hdl->fops->open(vhci_hdl);
	return 0;
}

static bool is_hci_event_discardable(const uint8_t *evt_data)
{
	uint8_t evt_type = evt_data[0];

	switch (evt_type) {
#if defined(CONFIG_BT_BREDR)
	case BT_HCI_EVT_INQUIRY_RESULT_WITH_RSSI:
	case BT_HCI_EVT_EXTENDED_INQUIRY_RESULT:
		return true;
#endif
	case BT_HCI_EVT_LE_META_EVENT: {
		uint8_t subevt_type = evt_data[sizeof(struct bt_hci_evt_hdr)];

		switch (subevt_type) {
		case BT_HCI_EVT_LE_ADVERTISING_REPORT:
			return true;
		case BT_HCI_EVT_LE_EXT_ADVERTISING_REPORT:
			return true;
		default:
			return false;
		}
	}
	default:
		return false;
	}
}

static struct net_buf *bt_csk_evt_recv(uint8_t *data, size_t remaining)
{
	bool discardable = false;
	struct bt_hci_evt_hdr hdr;
	struct net_buf *buf;

	if (remaining < sizeof(hdr)) {
		LOG_ERR("Not enough data for event header");
		return NULL;
	}

	discardable = is_hci_event_discardable(data);

	memcpy((void *)&hdr, data, sizeof(hdr));
	data += sizeof(hdr);
	remaining -= sizeof(hdr);

	if (remaining != hdr.len) {
		LOG_ERR("Event payload length is not correct");
		return NULL;
	}
	LOG_DBG("len %u", hdr.len);

	buf = bt_buf_get_evt(hdr.evt, discardable, K_NO_WAIT);
	if (!buf) {
		if (discardable) {
			LOG_DBG("Discardable buffer pool full, ignoring event");
		} else {
			LOG_ERR("No available event buffers!");
		}
		return buf;
	}

	net_buf_add_mem(buf, &hdr, sizeof(hdr));
	net_buf_add_mem(buf, data, remaining);

	return buf;
}

static struct net_buf *bt_csk_acl_recv(uint8_t *data, size_t remaining)
{
	struct bt_hci_acl_hdr hdr;
	struct net_buf *buf;

	if (remaining < sizeof(hdr)) {
		LOG_ERR("Not enough data for ACL header");
		return NULL;
	}

	buf = bt_buf_get_rx(BT_BUF_ACL_IN, K_NO_WAIT);
	if (buf) {
		memcpy((void *)&hdr, data, sizeof(hdr));
		data += sizeof(hdr);
		remaining -= sizeof(hdr);

		net_buf_add_mem(buf, &hdr, sizeof(hdr));
	} else {
		LOG_ERR("No available ACL buffers!");
		return NULL;
	}

	if (remaining != sys_le16_to_cpu(hdr.len)) {
		LOG_ERR("ACL payload length is not correct");
		net_buf_unref(buf);
		return NULL;
	}

	LOG_DBG("len %u", remaining);
	net_buf_add_mem(buf, data, remaining);

	return buf;
}

static struct net_buf *bt_csk_iso_recv(uint8_t *data, size_t remaining)
{
	struct bt_hci_iso_hdr hdr;
	struct net_buf *buf;

	if (remaining < sizeof(hdr)) {
		LOG_ERR("Not enough data for ISO header");
		return NULL;
	}

	buf = bt_buf_get_rx(BT_BUF_ISO_IN, K_NO_WAIT);
	if (buf) {
		memcpy((void *)&hdr, data, sizeof(hdr));
		data += sizeof(hdr);
		remaining -= sizeof(hdr);

		net_buf_add_mem(buf, &hdr, sizeof(hdr));
	} else {
		LOG_ERR("No available ISO buffers!");
		return NULL;
	}

	if (remaining != sys_le16_to_cpu(hdr.len)) {
		LOG_ERR("ISO payload length is not correct");
		net_buf_unref(buf);
		return NULL;
	}

	LOG_DBG("len %zu", remaining);
	net_buf_add_mem(buf, data, remaining);

	return buf;
}

static int hci_csk_host_rcv_pkt(uint8_t *data, uint16_t len)
{
	uint8_t pkt_indicator;
	struct net_buf *buf = NULL;
	size_t remaining = len;

	LOG_HEXDUMP_DBG(data, len, "host packet data:");

	pkt_indicator = *data++;
	remaining -= sizeof(pkt_indicator);

	switch (pkt_indicator) {
	case HCI_EVT:
		buf = bt_csk_evt_recv(data, remaining);
		break;

	case HCI_ACL:
		buf = bt_csk_acl_recv(data, remaining);
		break;

	case HCI_SCO:
		buf = bt_csk_iso_recv(data, remaining);
		break;

	default:
		LOG_ERR("Unknown HCI type %u", pkt_indicator);
		return -1;
	}

	if (buf) {
		LOG_DBG("Calling bt_recv(%p)", buf);

		bt_recv(buf);
	}

	return 0;
}

static void hci_recv_ready(void)
{
	uint16_t rx_buf_len = 0;
	uint8_t *read_buf;
	read_buf = vhci_hdl->fops->read(vhci_hdl, &rx_buf_len);
	LOG_HEXDUMP_DBG(read_buf, rx_buf_len, "ctrl packet data:");
	hci_csk_host_rcv_pkt(read_buf, rx_buf_len);
	k_free(read_buf);
}

static hosted_event_callback_t s_driver_event_cb;
K_SEM_DEFINE(init_sem, 0, 1);

static void hosted_event_handler(hosted_event_t events, void *arg)
{
    if (events & HOSTED_LINK_EVT_ACTIVE) {
        k_sem_give(&init_sem);
		printk("hosted init done\n");
    } else {

    }
}

static const struct bt_hci_driver drv = {
	.name           = "CSK BLE",
	.open           = bt_csk_open,
	.send           = bt_csk_send,
	.bus            = BT_HCI_DRIVER_BUS_IPM,
#if defined(CONFIG_BT_DRIVER_QUIRK_NO_AUTO_DLE)
	.quirks         = BT_QUIRK_NO_AUTO_DLE,
#endif
};

static int csk_ble_init(const struct device *unused)
{
	ARG_UNUSED(unused);
	s_driver_event_cb.events = HOSTED_LINK_EVT_ACTIVE;
    s_driver_event_cb.handler = &hosted_event_handler;
    hosted_add_callback(&s_driver_event_cb);
    hosted_init();
	int ret = k_sem_take(&init_sem, BLE_INIT_TIMEOUT);
	if (ret != 0) {
		return ret;
	}
	vhci_hdl = vhci_init(hci_recv_ready);

	bt_hci_driver_register(&drv);

	return 0;
}

SYS_INIT(csk_ble_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
