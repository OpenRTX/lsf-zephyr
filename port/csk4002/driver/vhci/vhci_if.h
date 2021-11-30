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

/*prevent recursive inclusion */
#ifndef __VHCI_IF_H
#define __VHCI_IF_H

#ifdef __cplusplus
extern "C" {
#endif

/** includes **/
// #include "cmsis_os.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "common/common.h"

struct vhci_operations;

/* vhci interface handle */
typedef struct vhci_handle_s {
	QueueHandle_t queue;
	uint8_t if_type;
	uint8_t if_num;
	struct vhci_operations *fops;
	uint8_t state;
	void (*vhci_rx_callback)(void);
} vhci_handle_t;

/* vhci interface */
struct vhci_operations {
	/**
	 * @brief Open new vhci interface
	 * @param  vhci_hdl - handle of vhci interface
	 * @retval 0 if success, -1 on failure
	 */
	int (*open)(vhci_handle_t *vhci_hdl);

	/**
	 * @brief  vhci interface read non blocking
	 *         This is non blocking receive
	 *         In case higher layer using vhci interface needs to make
	 *         blocking read, it should register vhci_rx_callback through
	 *         vhci_init.
	 *
	 *         vhci_rx_callback is notification mechanism to implementer of
	 *         vhci interface. Higher layer would understand there is data
	 *         is ready through this notification. Then higer layer should do
	 *         vhci_read API to receive actual data.
	 *
	 *         As an another design option, vhci_rx_callback can also be
	 *         thought of incoming data indication, i.e. asynchronous rx
	 *         indication, which can be used by higher layer in seperate
	 *         dedicated rx task to receive and process rx data.
	 *
	 * @param  vhci_hdl - handle
	 *         rlen - output param, number of bytes read
	 *
	 * @retval rbuffer - ready buffer read on vhci inerface
	 */
	uint8_t *(*read)(const vhci_handle_t *vhci_hdl, uint16_t *rlen);

	/**
	 * @brief vhci interface write
	 * @param  vhci_hdl - handle
	 *         wlen - number of bytes to write
	 *         wbuffer - buffer to send
	 * @retval STM_FAIL/STM_OK
	 */
	int (*write)(const vhci_handle_t *vhci_hdl, uint8_t *wbuffer, const uint16_t wlen);

	/**
	 * @brief close - Close vhci interface
	 * @param  vhci_hdl - handle
	 * @retval rbuffer - ready buffer read on vhci inerface
	 */
	int (*close)(vhci_handle_t *vhci_hdl);
};

/**
 * @brief vhci_init - create and return new vhci interface
 * @param  vhci_rx_callback - callback to be invoked on rx data
 * @retval vhci_hdl - output handle of vhci interface
 */
vhci_handle_t *vhci_init(void (*rx_data_ind)(void));

#ifdef __cplusplus
}
#endif

#endif /* __VHCI_IF_H */
