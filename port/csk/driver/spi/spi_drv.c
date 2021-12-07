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

#include <string.h>
#include "common/trace.h"
#include "spi_drv.h"
#include "adapter.h"
#include "serial_drv.h"
#include "netdev_if.h"
#include "vhci_drv.h"

#include "hosted_gpio.h"
#include "hosted_spi.h"
#include "hosted/hosted.h"

csk_spi_handle_t spi_handle;

#define TO_SLAVE_QUEUE_SIZE               	10
#define FROM_SLAVE_QUEUE_SIZE             	10

#define PROCESS_RX_TASK_STACK_SIZE        	CONFIG_HOSTED_PROCESS_RX_STACK
#define TRANSACTION_TASK_STACK_SIZE       	CONFIG_HOSTED_TRANSACTION_STACK
#define PROCESS_RX_TASK_PRIORITY			CONFIG_HOSTED_PROCESS_RX_PRIORITY
#define TRANSACTION_TASK_PRIORITY			CONFIG_HOSTED_TRANSACTION_PRIORITY

K_THREAD_STACK_DEFINE(transaction_thread_stack_area, TRANSACTION_TASK_STACK_SIZE);
K_THREAD_STACK_DEFINE(process_thread_stack_area, PROCESS_RX_TASK_STACK_SIZE);
struct k_thread transaction_thread_data;
struct k_thread process_thread_data;
static char __aligned(4) to_slave_msgq_buffer[TO_SLAVE_QUEUE_SIZE * sizeof(interface_buffer_handle_t)];
static char __aligned(4) from_slave_msgq_buffer[FROM_SLAVE_QUEUE_SIZE * sizeof(interface_buffer_handle_t)];

#define MAX_PAYLOAD_SIZE (MAX_SPI_BUFFER_SIZE-sizeof(struct esp_payload_header))

typedef enum hardware_type_e {
	HARDWARE_TYPE_ESP32,
	HARDWARE_TYPE_ESP32S2,
	HARDWARE_TYPE_INVALID,
}hardware_type_t;

static stm_ret_t spi_transaction_esp32(uint8_t * txbuff);

static int esp_netdev_open(netdev_handle_t netdev);
static int esp_netdev_close(netdev_handle_t netdev);
static int esp_netdev_xmit(netdev_handle_t netdev, struct net_pbuf *net_buf);

extern void hosted_priv_event_handler(struct esp_priv_event *priv_event);

static struct esp_private *esp_priv[MAX_NETWORK_INTERFACES];
static uint8_t hardware_type = HARDWARE_TYPE_INVALID;

static struct netdev_ops esp_net_ops = {
	.netdev_open = esp_netdev_open,
	.netdev_close = esp_netdev_close,
	.netdev_xmit = esp_netdev_xmit,
};

extern bool hosted_is_synced();
extern int hosted_sync_set(bool set);

/** Exported variables **/

static struct k_sem* osSemaphore = NULL;
K_MUTEX_DEFINE(mutex_spi_trans);
/* Queue declaration */
static struct k_msgq *to_slave_queue = NULL;
static struct k_msgq *from_slave_queue = NULL;

/* callback of event handler */
static void (*spi_drv_evt_handler_fp) (uint8_t);

/** function declaration **/
/** Exported functions **/
static void transaction_task(void *arg1, void *arg2, void *arg3);
static void process_rx_task(void *arg1, void *arg2, void *arg3);
static uint8_t * get_tx_buffer(uint8_t *is_valid_tx_buf);
static void deinit_netdev(void);

/**
  * @brief  get private interface of expected type and number
  * @param  if_type - interface type
  *         if_num - interface number
  * @retval interface handle if found, else NULL
  */
static struct esp_private * get_priv(uint8_t if_type, uint8_t if_num)
{
	int i = 0;

	for (i = 0; i < MAX_NETWORK_INTERFACES; i++) {
		if((esp_priv[i]) &&
			(esp_priv[i]->if_type == if_type) &&
			(esp_priv[i]->if_num == if_num))
			return esp_priv[i];
	}

	return NULL;
}

/**
  * @brief  open virtual network device
  * @param  netdev - network device
  * @retval 0 on success
  */
static int esp_netdev_open(netdev_handle_t netdev)
{
	return STM_OK;
}

/**
  * @brief  close virtual network device
  * @param  netdev - network device
  * @retval 0 on success
  */
static int esp_netdev_close(netdev_handle_t netdev)
{
	return STM_OK;
}

/**
  * @brief  transmit on virtual network device
  * @param  netdev - network device
  *         net_buf - buffer to transmit
  * @retval None
  */
static int esp_netdev_xmit(netdev_handle_t netdev, struct net_pbuf *net_buf)
{
	struct esp_private *priv;
	int ret;

	if (!netdev || !net_buf)
		return STM_FAIL;
	priv = (struct esp_private *) netdev_get_priv(netdev);

	if (!priv)
		return STM_FAIL;

	ret = send_to_slave(priv->if_type, priv->if_num,
			net_buf->payload, net_buf->len);
	k_free(net_buf);

	return ret;
}

/**
  * @brief  create virtual network device
  * @param  None
  * @retval None
  */
static int init_netdev(void)
{
	void *ndev = NULL;
	int i = 0;
	struct esp_private *priv = NULL;
	char *if_name = STA_INTERFACE;
	uint8_t if_type = ESP_STA_IF;

	for (i = 0; i < MAX_NETWORK_INTERFACES; i++) {
		/* Alloc and init netdev */
		ndev = netdev_alloc(sizeof(struct esp_private), if_name);
		if (!ndev) {
			deinit_netdev();
			return STM_FAIL;
		}

		priv = (struct esp_private *) netdev_get_priv(ndev);
		if (!priv) {
			deinit_netdev();
			return STM_FAIL;
		}

		priv->netdev = ndev;
		priv->if_type = if_type;
		priv->if_num = 0;

		if (netdev_register(ndev, &esp_net_ops)) {
			deinit_netdev();
			return STM_FAIL;
		}

		if_name = SOFTAP_INTERFACE;
		if_type = ESP_AP_IF;

		esp_priv[i] = priv;
	}

	return STM_OK;
}

/**
  * @brief  destroy virtual network device
  * @param  None
  * @retval None
  */
static void deinit_netdev(void)
{
	for (int i = 0; i < MAX_NETWORK_INTERFACES; i++) {
		if (esp_priv[i]) {
			if (esp_priv[i]->netdev) {
				netdev_unregister(esp_priv[i]->netdev);
				netdev_free(esp_priv[i]->netdev);
			}
			esp_priv[i] = NULL;
		}
	}
}

static void set_hardware_type(void)
{
	hardware_type = HARDWARE_TYPE_ESP32;  //Currently we only support esp32
}

/** function definition **/

/** Exported Functions **/
/**
  * @brief  spi driver initialize
  * @param  spi_drv_evt_handler - event handler of type spi_drv_events_e
  * @retval None
  */
void spi_link_init(void(*spi_drv_evt_handler)(uint8_t))
{
	csk_spi_config_t spi_config = {
		.spi_port = 0,
		.spi_parameter_config = {
			.spi_mode = SPI_MASTER,
			.data_bit_len = 32,
			.spi_clk = 10 * 1000 * 1000,
			.spi_byte_order = SPI_BYTE_LSB,
			.spi_sample_mode = SPI_CPOL0_CPHA1,
		},
	};
	int ret = hosted_spi_init(&spi_config, &spi_handle);
	if (ret != 0) {
        printk("[%s] csk spi init failed", __FUNCTION__ );
        return;
	}
	stm_ret_t retval = STM_OK;
	/* Check if supported board */
	set_hardware_type();

	/* register callback */
	spi_drv_evt_handler_fp = spi_drv_evt_handler;

	retval = init_netdev();
	if (retval) {
		printf("netdev failed to init\n\r");
		assert(retval==STM_OK);
	}

	/* spi handshake semaphore */
    osSemaphore = k_malloc(sizeof(struct k_sem));
    if (osSemaphore == NULL) {
        printk("[%s] no mem", __FUNCTION__ );
        return;
    }
    k_sem_init(osSemaphore, 1, 1);  // give the first init signal

	/* Queue - tx */
    to_slave_queue = k_malloc(sizeof(struct k_msgq));
    if (to_slave_queue == NULL) {
        printk("[%s] to_slave_queue no mem\n", __FUNCTION__ );
        return;
    }
	k_msgq_init(to_slave_queue, to_slave_msgq_buffer, sizeof(interface_buffer_handle_t), TO_SLAVE_QUEUE_SIZE);

	/* Queue - rx */
    from_slave_queue = k_malloc(sizeof(struct k_msgq));
    if (from_slave_queue == NULL) {
        printk("[%s] no mem\n", __FUNCTION__ );
        return;
    }
	k_msgq_init(from_slave_queue, from_slave_msgq_buffer, sizeof(interface_buffer_handle_t), FROM_SLAVE_QUEUE_SIZE);
    k_thread_create(&transaction_thread_data,
                    transaction_thread_stack_area, K_THREAD_STACK_SIZEOF(transaction_thread_stack_area),
                    transaction_task, NULL, NULL, NULL,
                    TRANSACTION_TASK_PRIORITY, 0, K_NO_WAIT);
    k_thread_create(&process_thread_data,
                    process_thread_stack_area, K_THREAD_STACK_SIZEOF(process_thread_stack_area),
                    process_rx_task, NULL, NULL, NULL,
                    PROCESS_RX_TASK_PRIORITY, 0, K_NO_WAIT);
}

/**
  * @brief EXTI line detection callback, used as SPI handshake GPIO
  * @param GPIO_Pin: Specifies the pins connected EXTI line
  * @retval None
  */
void spi_sync_callback(uint16_t pin)
{
	if ( (pin == GPIO_DATA_READY_Pin) ||
	     (pin == GPIO_HANDSHAKE_Pin) )
	{
        if (!hosted_is_synced()) {
            hosted_sync_set(true);
        }
        if (osSemaphore != NULL) {
            k_sem_give(osSemaphore);
        }
	}
}

/**
  * @brief  Schedule SPI transaction if -
  *         a. valid TX buffer is ready at SPI host (STM)
  *         b. valid TX buffer is ready at SPI peripheral (ESP)
  *         c. Dummy transaction is expected from SPI peripheral (ESP)
  * @param  argument: Not used
  * @retval None
  */

static void check_and_execute_spi_transaction(void)
{
	uint8_t * txbuff = NULL;
	uint8_t is_valid_tx_buf = 0;
	int gpio_handshake = GPIO_PIN_RESET;
	int gpio_rx_data_ready = GPIO_PIN_RESET;


	/* handshake line SET -> slave ready for next transaction */
	gpio_handshake = csk_read_pin(GPIO_HANDSHAKE_GPIO_Port,
			GPIO_HANDSHAKE_Pin);

	/* data ready line SET -> slave wants to send something */
	gpio_rx_data_ready = csk_read_pin(GPIO_DATA_READY_GPIO_Port,
			GPIO_DATA_READY_Pin);

	if (gpio_handshake == GPIO_PIN_SET) {

		/* Get next tx buffer to be sent */
		txbuff = get_tx_buffer(&is_valid_tx_buf);

		if ( (gpio_rx_data_ready == GPIO_PIN_SET) ||
		     (is_valid_tx_buf) ) {

			/* Execute transaction only if EITHER holds true-
			 * a. A valid tx buffer to be transmitted towards slave
			 * b. Slave wants to send something (Rx for host)
			 */
            k_mutex_lock(&mutex_spi_trans, K_FOREVER);
			spi_transaction_esp32(txbuff);
            k_mutex_unlock(&mutex_spi_trans);
		}
	}
}

/**
  * @brief  Send to slave via SPI
  * @param  iface_type -type of interface
  *         iface_num - interface number
  *         wbuffer - tx buffer
  *         wlen - size of wbuffer
  * @retval sendbuf - Tx buffer
  */
stm_ret_t send_to_slave(uint8_t iface_type, uint8_t iface_num,
		uint8_t * wbuffer, uint16_t wlen)
{
	interface_buffer_handle_t buf_handle = {0};

	if (!wbuffer || !wlen || (wlen > MAX_PAYLOAD_SIZE)) {
		printf("write fail: buff(%p) 0? OR (0<len(%u)<=max_poss_len(%u))?\n\r",
				wbuffer, wlen, MAX_PAYLOAD_SIZE);
		if(wbuffer) {
			k_free(wbuffer);
			wbuffer = NULL;
		}
		return STM_FAIL;
	}
	memset(&buf_handle, 0, sizeof(buf_handle));

	buf_handle.if_type = iface_type;
	buf_handle.if_num = iface_num;
	buf_handle.payload_len = wlen;
	buf_handle.payload = wbuffer;
	buf_handle.priv_buffer_handle = wbuffer;
	buf_handle.free_buf_handle = k_free;

	k_msgq_put(to_slave_queue, &buf_handle, K_FOREVER);

	check_and_execute_spi_transaction();

	return STM_OK;
}

/** Local functions **/

/**
  * @brief  Full duplex transaction SPI transaction for ESP32 hardware
  * @param  txbuff: TX SPI buffer
  * @retval STM_OK / STM_FAIL
  */
static stm_ret_t spi_transaction_esp32(uint8_t * txbuff)
{
	uint8_t *rxbuff = NULL;
	interface_buffer_handle_t buf_handle = {0};
	struct  esp_payload_header *payload_header;
	uint16_t len, offset;
	// HAL_StatusTypeDef retval = HAL_ERROR;

	/* Allocate rx buffer */
	rxbuff = (uint8_t *)k_malloc(MAX_SPI_BUFFER_SIZE);
	assert(rxbuff);
	memset(rxbuff, 0, MAX_SPI_BUFFER_SIZE);

	if(!txbuff) {
		/* Even though, there is nothing to send,
		 * valid resetted txbuff is needed for SPI driver
		 */
		txbuff = (uint8_t *)k_malloc(MAX_SPI_BUFFER_SIZE);
		assert(txbuff);
		memset(txbuff, 0, MAX_SPI_BUFFER_SIZE);
	}
	int retval = hosted_spi_send_recv(spi_handle, (uint8_t *) txbuff, (uint8_t *) rxbuff, MAX_SPI_BUFFER_SIZE / 4,
                                      K_FOREVER);
    switch(retval)
	{
		case 0:
			/* create buffer rx handle, used for processing */
			payload_header = (struct esp_payload_header *) rxbuff;

			/* Fetch length and offset from payload header */
			len = le16toh(payload_header->len);
			offset = le16toh(payload_header->offset);

			if ((!len) ||
				(len > MAX_PAYLOAD_SIZE) ||
				(offset != sizeof(struct esp_payload_header))) {
				/* Free up buffer, as one of following -
				 * 1. no payload to process
				 * 2. input packet size > driver capacity
				 * 3. payload header size mismatch,
				 * wrong header/bit packing?
				 * */
				if (rxbuff) {
					k_free(rxbuff);
					rxbuff = NULL;
				}
				/* Give chance to other tasks */
                k_yield();

			} else {
				buf_handle.priv_buffer_handle = rxbuff;
				buf_handle.free_buf_handle = k_free;
				buf_handle.payload_len = len;
				buf_handle.if_type     = payload_header->if_type;
				buf_handle.if_num      = payload_header->if_num;
				buf_handle.payload     = rxbuff + offset;

                k_msgq_put(from_slave_queue, &buf_handle, K_FOREVER);
			}

			/* Free input TX buffer */
			if (txbuff) {
				k_free(txbuff);
				txbuff = NULL;
			}
			break;

		default:
			printf("default handler: Error in SPI transaction\n\r");
			goto done;
			break;
	}

	return STM_OK;

done:
	/* error cases, abort */
	if (txbuff) {
		k_free(txbuff);
		txbuff = NULL;
	}

	if (rxbuff) {
		k_free(rxbuff);
		rxbuff = NULL;
	}
	return STM_FAIL;
}

/**
  * @brief  Task for SPI transaction
  * @param  argument: Not used
  * @retval None
  */
static void transaction_task(void *arg1, void *arg2, void *arg3)
{
	if (hardware_type == HARDWARE_TYPE_ESP32) {
		//Do nothing
	} else {
		printf("Unsupported slave hardware type\n\r");
		assert(hardware_type != HARDWARE_TYPE_INVALID);
	}

	for (;;) {

		if (osSemaphore != NULL) {
			/* Wait till slave is ready for next transaction */
            if (k_sem_take(osSemaphore, K_FOREVER) == 0) {
                check_and_execute_spi_transaction();
            }
		}
	}
}

/**
  * @brief  RX processing task
  * @param  argument: Not used
  * @retval None
  */
static void process_rx_task(void *arg1, void *arg2, void *arg3)
{
	stm_ret_t ret = STM_OK;
	interface_buffer_handle_t buf_handle = {0};
	uint8_t *payload = NULL;
	struct net_pbuf *buffer = NULL;
	struct esp_priv_event *event = NULL;
	struct esp_private *priv = NULL;
	uint8_t *serial_buf = NULL;
	uint8_t *vhci_buf = NULL;

	while (1) {
		int ret = k_msgq_get(from_slave_queue, &buf_handle, K_FOREVER);
        if (ret != 0) {
            continue;
        }

		/* point to payload */
		payload = buf_handle.payload;

		/* process received buffer for all possible interface types */
		if (buf_handle.if_type == ESP_SERIAL_IF) {

			serial_buf = (uint8_t *)k_malloc(buf_handle.payload_len);
			assert(serial_buf);

			memcpy(serial_buf, payload, buf_handle.payload_len);

			/* serial interface path */
			serial_rx_handler(buf_handle.if_num, serial_buf,
					buf_handle.payload_len);

		} else if((buf_handle.if_type == ESP_STA_IF) ||
				(buf_handle.if_type == ESP_AP_IF)) {
			priv = get_priv(buf_handle.if_type, buf_handle.if_num);

			if (priv) {
				buffer = (struct net_pbuf *)k_malloc(sizeof(struct net_pbuf));
				assert(buffer);

				buffer->len = buf_handle.payload_len;
				buffer->payload = k_malloc(buf_handle.payload_len);
				assert(buffer->payload);

				memcpy(buffer->payload, buf_handle.payload,
						buf_handle.payload_len);

				netdev_rx(priv->netdev, buffer);
			}
			
		} else if (buf_handle.if_type == ESP_HCI_IF) {
			vhci_buf = (uint8_t *)k_malloc(buf_handle.payload_len);
			assert(vhci_buf);
			memcpy(vhci_buf, payload, buf_handle.payload_len);

			/* vhci interface path */
			vhci_rx_handler(buf_handle.if_num, vhci_buf,
					buf_handle.payload_len);

		} else if (buf_handle.if_type == ESP_PRIV_IF) {
			/* priv transaction received */
			event = (struct esp_priv_event *) (payload);
			if (event->event_type == ESP_PRIV_EVENT_INIT) {
				/* halt spi transactions for some time,
				 * this is one time delay, to give breathing
				 * time to slave before spi trans start */
				k_msleep(50); //Fixme: remove delay, waiting for esp32 spi slave init done
				if (spi_drv_evt_handler_fp) {
					spi_drv_evt_handler_fp(SPI_DRIVER_ACTIVE);
				}
			} else {
				/* User can re-use this type of transaction */
				hosted_priv_event_handler(event);
			}
		}

		/* Free buffer handle */
		/* When buffer offloaded to other module, that module is
		 * responsible for freeing buffer. In case not offloaded or
		 * failed to offload, buffer should be freed here.
		 */
		if (buf_handle.free_buf_handle) {
			buf_handle.free_buf_handle(buf_handle.priv_buffer_handle);
		}
	}
}


/**
  * @brief  Next TX buffer in SPI transaction
  * @param  argument: Not used
  * @retval sendbuf - Tx buffer
  */
static uint8_t * get_tx_buffer(uint8_t *is_valid_tx_buf)
{
	struct  esp_payload_header *payload_header;
	uint8_t *sendbuf = NULL;
	uint8_t *payload = NULL;
	uint16_t len = 0;
	interface_buffer_handle_t buf_handle = {0};

	*is_valid_tx_buf = 0;

	/* Check if higher layers have anything to transmit, non blocking.
	 * If nothing is expected to send, queue receive will fail.
	 * In that case only payload header with zero payload
	 * length would be transmitted.
	 */
	if (k_msgq_get(to_slave_queue, &buf_handle, K_NO_WAIT) == 0) {
		len = buf_handle.payload_len;
	}

	if (len) {

		sendbuf = (uint8_t *) k_malloc(MAX_SPI_BUFFER_SIZE);
		if (!sendbuf) {
			printk("malloc failed\n\r");
			goto done;
		}

		memset(sendbuf, 0, MAX_SPI_BUFFER_SIZE);

		*is_valid_tx_buf = 1;

		/* Form Tx header */
		payload_header = (struct esp_payload_header *) sendbuf;
		payload = sendbuf + sizeof(struct esp_payload_header);
		payload_header->len     = htole16(len);
		payload_header->offset  = htole16(sizeof(struct esp_payload_header));
        payload_header->if_type = buf_handle.if_type;
        payload_header->if_num = buf_handle.if_num;
		payload_header->reserved1 = 0;

		memcpy(payload, buf_handle.payload, min(len, MAX_PAYLOAD_SIZE));
	}

done:
	/* free allocated buffer */
	if (buf_handle.free_buf_handle)
		buf_handle.free_buf_handle(buf_handle.priv_buffer_handle);

	return sendbuf;
}
