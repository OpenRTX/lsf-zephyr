// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

/** Includes **/
#include "string.h"
#include "vhci_drv.h"
#include "vhci_if.h"
#include "adapter.h"
#include "spi_drv.h"
#include "trace.h"

/** Macros / Constants **/
#define MAX_VHCI_INTF 2
#define TO_VHCI_INFT_QUEUE_SIZE 100

typedef enum { INIT, ACTIVE, DESTROY } vhci_state_e;

/* data structures needed for vhci driver */
static struct k_msgq to_vhci_intf_queue[MAX_VHCI_INTF];
//static QueueHandle_t to_vhci_intf_queue[MAX_VHCI_INTF];
static vhci_handle_t *interface_handle_g[MAX_VHCI_INTF] = {NULL};
static uint8_t conn_num = 0;

/** Function Declarations **/
static int vhci_open(vhci_handle_t *vhci_hdl);
static uint8_t *vhci_read(const vhci_handle_t *vhci_hdl, uint16_t *rlen);
static int vhci_write(const vhci_handle_t *vhci_hdl, uint8_t *wbuffer, const uint16_t wlen);
static void vhci_cleanup(vhci_handle_t *vhci_hdl);
static int vhci_close(vhci_handle_t *vhci_hdl);

/* define vhci interface */
static struct vhci_operations vhci_fops = {
		.open = vhci_open,
		.read = vhci_read,
		.write = vhci_write,
		.close = vhci_close,
};

/** function definition **/

/** Local Functions **/

/**
 * @brief Open new vhci interface
 * @param  vhci_hdl - handle of vhci interface
 * @retval 0 if success, -1 on failure
 */
static char __aligned(4) msgq_buffer[TO_VHCI_INFT_QUEUE_SIZE * sizeof(interface_buffer_handle_t)];
static int
vhci_open(vhci_handle_t *vhci_hdl)
{
	if (!vhci_hdl) {
		printf("vhci invalid hdr\n\r");
		return STM_FAIL;
	}

//	if (vhci_hdl->queue) {
//		/* clean up earlier queue */
//		vQueueDelete(vhci_hdl->queue);
//	}
	/* Queue - vhci rx */
//	vhci_hdl->queue = xQueueCreate(TO_VHCI_INFT_QUEUE_SIZE, sizeof(interface_buffer_handle_t));
    vhci_hdl->queue = k_malloc(sizeof(struct k_msgq));
    if (vhci_hdl->queue == NULL) {
        vhci_cleanup(vhci_hdl);
        return STM_FAIL;
    }
    k_msgq_init(vhci_hdl->queue, msgq_buffer, sizeof(interface_buffer_handle_t), TO_VHCI_INFT_QUEUE_SIZE);
//	if (!vhci_hdl->queue) {
//		vhci_cleanup(vhci_hdl);
//		return STM_FAIL;
//	}

	vhci_hdl->state = ACTIVE;
	return STM_OK;
}

/**
 * @brief Get vhci handle for iface_num
 * @param  iface_num - vhci connection number
 * @retval vhci_hdl - output handle of vhci interface
 */
static vhci_handle_t *
get_vhci_handle(const uint8_t iface_num)
{
	if ((iface_num < MAX_VHCI_INTF) && (interface_handle_g[iface_num]) &&
			(interface_handle_g[iface_num]->state == ACTIVE)) {
		return interface_handle_g[iface_num];
	}
	return NULL;
}

/**
 * @brief Close vhci interface
 * @param  vhci_hdl - handle
 * @retval rbuffer - ready buffer read on vhci inerface
 */
static int
vhci_close(vhci_handle_t *vhci_hdl)
{
	if (vhci_hdl->queue) {
        k_free(vhci_hdl->queue);
        vhci_hdl->queue = NULL;
//		vQueueDelete(vhci_hdl->queue);
//		vhci_hdl->queue = NULL;
	}

	/* reset connection */
	if (conn_num > 0) {
		interface_handle_g[--conn_num] = NULL;
	}

	if (vhci_hdl) {
		k_free(vhci_hdl);
		vhci_hdl = NULL;
	}
	return STM_OK;
}

/**
 * @brief  vhci interface read non blocking
 * @param  vhci_hdl - handle
 *         rlen - output param, number of bytes read
 * @retval rbuffer - ready buffer read on vhci inerface
 */
static uint8_t *
vhci_read(const vhci_handle_t *vhci_hdl, uint16_t *rlen)
{
	/* This is a non-blocking call */
	interface_buffer_handle_t buf_handle = {0};

	/* Initial value */
	*rlen = 0;

	/* check if vhci interface valid */
	if ((!vhci_hdl) || (vhci_hdl->state != ACTIVE)) {
		printf("vhci invalid interface\n\r");
		return NULL;
	}

	/* This is non blocking receive.
	 *
	 * In case higher layer using vhci
	 * interface needs to make blocking read, it should register
	 * vhci_rx_callback through vhci_init.
	 * vhci_rx_callback is notification mechanism to implementer of vhci
	 * interface. Higher layer will understand there is data is ready
	 * through this notification. Then it will call vhci_read API
	 * to receive actual data.
	 *
	 * As an another design option, vhci_rx_callback can also be
	 * thought of incoming data indication, i.e. asynchronous rx
	 * indication, which can be used by higher layer in seperate
	 * dedicated rx task to receive and process rx data.
	 *
	 * In our example, first approach of blocking read is used.
	 */

//    interface_buffer_handle_t *buf_handle = NULL;
//    buf_handle = k_queue_get(vhci_hdl->queue, K_NO_WAIT);
    int ret = k_msgq_get(vhci_hdl->queue, &buf_handle, K_NO_WAIT);
    if (ret != 0) {
        printk("[%s] serial queue recv failed \n", __FUNCTION__ );
        return NULL;
    }

//	if (pdTRUE != xQueueReceive(vhci_hdl->queue, &buf_handle, 0)) {
//		printf("vhci queue recv failed \n\r");
//		return NULL;
//	}

	/* proceed only if payload and length are sane */
	if (!buf_handle.payload || !buf_handle.payload_len) {
		return NULL;
	}

	*rlen = buf_handle.payload_len;

	return buf_handle.payload;
}

/**
 * @brief vhci interface write
 * @param  vhci_hdl - handle
 *         wlen - number of bytes to write
 *         wbuffer - buffer to send
 * @retval STM_FAIL/STM_OK
 */
static int
vhci_write(const vhci_handle_t *vhci_hdl, uint8_t *wbuffer, const uint16_t wlen)
{
	if ((!vhci_hdl) || (vhci_hdl->state != ACTIVE)) {
		printf("vhci invalid interface for write\n\r");
		return STM_FAIL;
	}

	return send_to_slave(vhci_hdl->if_type, vhci_hdl->if_num, wbuffer, wlen);
}

/**
 * @brief vhci rx handler is called by spi driver when there
 *        is incoming data with interface type is vhci.
 * @param  buf_handle - handle
 *         wlen - number of bytes to write
 *         rxbuff - buffer from spi driver
 *         rx_len - size of rxbuff
 * @retval None
 */
stm_ret_t
vhci_rx_handler(uint8_t if_num, uint8_t *rxbuff, uint16_t rx_len)
{
	interface_buffer_handle_t buf_handle = {0};
	vhci_handle_t *vhci_hdl = NULL;

	vhci_hdl = get_vhci_handle(if_num);

	if ((!vhci_hdl) || (vhci_hdl->state != ACTIVE)) {
		printf("vhci interface not registered yet\n\r");
		return STM_FAIL;
	}
	buf_handle.if_type = ESP_HCI_IF;
	buf_handle.if_num = if_num;
	buf_handle.payload_len = rx_len;
	buf_handle.payload = rxbuff;
	buf_handle.priv_buffer_handle = rxbuff;
	buf_handle.free_buf_handle = k_free;

	/* send to vhci queue */
    int ret = k_msgq_put(vhci_hdl->queue, &buf_handle, K_FOREVER);
    if (ret != 0) {
        printf("Failed send vhciif queue[%u]\n\r", if_num);
        return STM_FAIL;
    }
//	if (pdTRUE != xQueueSend(vhci_hdl->queue, &buf_handle, portMAX_DELAY)) {
//		printf("Failed send vhciif queue[%u]\n\r", if_num);
//		return STM_FAIL;
//	}

	/* Indicate higher layer about data ready for consumption */
	if (vhci_hdl->vhci_rx_callback) {
		(*vhci_hdl->vhci_rx_callback)();
	}

	return STM_OK;
}

/**
 * @brief vhci cleanup
 * @param  buf_handle - handle
 * @retval None
 */
static void
vhci_cleanup(vhci_handle_t *vhci_hdl)
{
	vhci_close(vhci_hdl);
	vhci_hdl->state = DESTROY;
}

/** Exported Functions **/

/**
 * @brief create and return new vhci interface
 * @param  vhci_rx_callback - callback to be invoked on rx data
 * @retval vhci_hdl - output handle of vhci interface
 */
vhci_handle_t *
vhci_init(void (*vhci_rx_callback)(void))
{
	vhci_handle_t *vhci_hdl = NULL;

	/* Check if more vhci interfaces be created */
	if ((conn_num + 1) < MAX_VHCI_INTF) {
		vhci_hdl = (vhci_handle_t *)k_malloc(sizeof(vhci_handle_t));
		if (!vhci_hdl) {
			printf("vhci interface - malloc failed\n\r");
			return NULL;
		}

		vhci_hdl->if_type = ESP_HCI_IF;
		vhci_hdl->if_num = conn_num;
		vhci_hdl->queue = &to_vhci_intf_queue[conn_num];
//		vhci_hdl->queue = to_vhci_intf_queue[conn_num];
		vhci_hdl->state = INIT;
		vhci_hdl->fops = &vhci_fops;
		vhci_hdl->vhci_rx_callback = vhci_rx_callback;
		interface_handle_g[conn_num] = vhci_hdl;
		conn_num++;

	} else {
		printf("Number of vhci interface connections overflow\n\r");
		return NULL;
	}

	return vhci_hdl;
}
