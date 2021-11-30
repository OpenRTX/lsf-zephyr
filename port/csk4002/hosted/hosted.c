#include <stdlib.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "common/util.h"
#include "common/trace.h"
#include "commands.h"
#include "platform_wrapper.h"
#include "esp_event.h"

#include "venus_log.h"
#include "hosted_gpio.h"
#include "spi_drv.h"
#include "netdev_api.h"
#include "adapter.h"

#include "hosted/hosted.h"

#define MAC_ADDR_LEN    6

const char *HOSTED_LINK_EVENT_BASE = "hosted_link_event";
const char *HOSTED_WIFI_EVENT_BASE = "hosted_wifi_event";

static QueueHandle_t control_msg_queue = NULL;
static SemaphoreHandle_t priv_event_mutex = NULL;
static esp_hosted_sta_config_t internal_sta_config;
static esp_hosted_softap_config_t internal_ap_config;

typedef enum {
    CMD_WIFI_STA_CONNECT,
    CMD_WIFI_STA_DISCONNECT,
    CMD_WIFI_AP_START,
    CMD_WIFI_AP_STOP,
    CMD_WIFI_MAX,
} control_command_t;

typedef enum {
    REP_WIFI_STA_CONNECTED,
    REP_WIFI_STA_DISCONNECTED,
} control_report_t;

typedef struct {
    control_command_t command;
    void* arg;
} control_message_t;

static void reset_slave(void);
static void spi_driver_event_handler(uint8_t event);
static void hosted_control_task(void *arg);


static inline is_wifi_control_cmd(control_command_t cmd);
static inline wifi_cmd_handler(control_message_t *control_message);
static ble_cmd_handler(control_message_t *control_message);

int hosted_init(void)
{
	reset_slave();
	network_init(); //init virturl network interface for wifi
    //init virturl hci interface for ble
	stm_spi_init(spi_driver_event_handler);
    control_msg_queue = xQueueCreate(10, sizeof(control_message_t));
    assert(control_msg_queue);
    priv_event_mutex = xSemaphoreCreateMutex();
    assert(control_msg_queue);
    BaseType_t os_ret = xTaskCreate(hosted_control_task, "hosted_control_task", 4 * 1024, NULL, 21, NULL);
    assert(os_ret == pdPASS);
    return SUCCESS;
}

int hosted_wifi_sta_connect(esp_hosted_sta_config_t *sta_config, uint32_t xTicksToWait)
{
    memcpy(&internal_sta_config, sta_config, sizeof(esp_hosted_sta_config_t));
    control_message_t ctrl_msg = {
        .command = CMD_WIFI_STA_CONNECT,
        .arg = NULL
    };
    BaseType_t os_ret = xQueueSend(control_msg_queue, &ctrl_msg, xTicksToWait);
    if (os_ret != pdPASS) {
        return HOSTED_ERR_TIMEOUT;
    }
    return HOSTED_ERR_OK;
}

int hosted_wifi_sta_disconnect(uint32_t xTicksToWait)
{
    control_message_t ctrl_msg = {
        .command = CMD_WIFI_STA_DISCONNECT,
        .arg = NULL
    };
    BaseType_t os_ret = xQueueSend(control_msg_queue, &ctrl_msg, xTicksToWait);
    if (os_ret != pdPASS) {
        return HOSTED_ERR_TIMEOUT;
    }
    return HOSTED_ERR_OK;
}

int hosted_wifi_ap_start(esp_hosted_softap_config_t *ap_config, uint32_t xTicksToWait)
{
    memcpy(&internal_ap_config, ap_config, sizeof(esp_hosted_softap_config_t));
    control_message_t ctrl_msg = {
        .command = CMD_WIFI_AP_START,
        .arg = NULL
    };
    BaseType_t os_ret = xQueueSend(control_msg_queue, &ctrl_msg, xTicksToWait);
    if (os_ret != pdPASS) {
        return HOSTED_ERR_TIMEOUT;
    }
    return HOSTED_ERR_OK;
}

int hosted_wifi_ap_stop(uint32_t xTicksToWait)
{
    control_message_t ctrl_msg = {
        .command = CMD_WIFI_AP_STOP,
        .arg = NULL
    };
    BaseType_t os_ret = xQueueSend(control_msg_queue, &ctrl_msg, xTicksToWait);
    if (os_ret != pdPASS) {
        return HOSTED_ERR_TIMEOUT;
    }
    return HOSTED_ERR_OK;
}

hosted_wifi_err_reason_t hosted_wifi_get_failed_reason(void *event_data)
{
    hosted_wifi_err_reason_t *res = event_data;
    return *((hosted_wifi_err_reason_t *)event_data);
}

uint8_t *hosted_wifi_ap_get_sta_mac(void *event_data)
{
    return (uint8_t *)event_data;
}

static inline is_wifi_control_cmd(control_command_t cmd)
{
    return (cmd >= 0 && cmd < CMD_WIFI_MAX);
}

static inline wifi_cmd_handler(control_message_t *control_message)
{
    if (control_message->command == CMD_WIFI_STA_CONNECT) {
        esp_hosted_control_config_t sta_config;
        memcpy(&sta_config.station, &internal_sta_config, sizeof(esp_hosted_sta_config_t));
        int conn_ret = wifi_set_ap_config(sta_config);
        if (conn_ret == SUCCESS) {
            esp_event_post(HOSTED_WIFI_EVENT_BASE, HOSTED_WIFI_EVT_STA_CONNECTED, &conn_ret, sizeof(int), portMAX_DELAY);
        } else {
            hosted_wifi_err_reason_t sta_err_reason;
            if (conn_ret == NO_AP_FOUND) {
                sta_err_reason = WIFI_ERR_AP_NOT_FOUND;
            } else if (conn_ret == INVALID_PASSWORD) {
                sta_err_reason = WIFI_ERR_INVALID_PASSWORD;
            } else {
                sta_err_reason = WIFI_ERR_INVALID_PASSWORD;  //Fixme: esp32 bug, report invalid reason
            }
            esp_event_post(HOSTED_WIFI_EVENT_BASE, HOSTED_WIFI_EVT_STA_FAILED, &sta_err_reason, sizeof(hosted_wifi_err_reason_t), portMAX_DELAY);
        }
    } else if (control_message->command == CMD_WIFI_STA_DISCONNECT) {
        xSemaphoreTake(priv_event_mutex, portMAX_DELAY);
        int disconn_ret = wifi_disconnect_ap();
        if (disconn_ret == SUCCESS) {
            esp_event_post(HOSTED_WIFI_EVENT_BASE, HOSTED_WIFI_EVT_STA_DISCONNECTED, &disconn_ret, sizeof(int), portMAX_DELAY);
        } else {
            esp_event_post(HOSTED_WIFI_EVENT_BASE, HOSTED_WIFI_EVT_STA_FAILED, &disconn_ret, sizeof(int), portMAX_DELAY);
        }
        xSemaphoreGive(priv_event_mutex);
    } else if (control_message->command == CMD_WIFI_AP_START) {
        esp_hosted_control_config_t ap_config;
        memcpy(&ap_config.softap, &internal_ap_config, sizeof(esp_hosted_softap_config_t));
        int start_ret = wifi_set_softap_config(ap_config);
        if (start_ret == SUCCESS) {
            esp_event_post(HOSTED_WIFI_EVENT_BASE, HOSTED_WIFI_EVT_AP_STARTED, &start_ret, sizeof(int), portMAX_DELAY);
        } else {
            esp_event_post(HOSTED_WIFI_EVENT_BASE, HOSTED_WIFI_EVT_AP_FAILED, &start_ret, sizeof(int), portMAX_DELAY);
        }
    } else if (control_message->command == CMD_WIFI_AP_STOP) {
        int stop_ret = wifi_stop_softap();
        if (stop_ret == SUCCESS) {
            esp_event_post(HOSTED_WIFI_EVENT_BASE, HOSTED_WIFI_EVT_AP_STOPED, &stop_ret, sizeof(int), portMAX_DELAY);
        } else {
            esp_event_post(HOSTED_WIFI_EVENT_BASE, HOSTED_WIFI_EVT_AP_FAILED, &stop_ret, sizeof(int), portMAX_DELAY);
        }
    }
}

static ble_cmd_handler(control_message_t *control_message)
{
    return; //ble command handler stub
}

static inline void control_path_init(void)
{
	control_path_platform_init();
}

static void reset_slave(void)
{
	hosted_gpio_init();
	HAL_GPIO_WritePin(GPIO_RESET_GPIO_Port, GPIO_RESET_Pin, GPIO_PIN_RESET);

    // SysTick_Delay_Ms(4); //about 4ms
	hard_delay(50); //about 4ms
	HAL_GPIO_WritePin(GPIO_RESET_GPIO_Port, GPIO_RESET_Pin, GPIO_PIN_SET);

	/* stop spi transactions short time to avoid slave sync issues */
    // SysTick_Delay_Ms(4300); //about 4300ms
	hard_delay(50000); //about 4300ms
}

static void spi_driver_event_handler(uint8_t event)
{
	switch(event) {
		case SPI_DRIVER_ACTIVE: {
			/* Initiate control path now */
			control_path_init();  //init control path and control task
            esp_event_post(HOSTED_LINK_EVENT_BASE, HOSTED_LINK_EVT_ACTIVE, NULL, 0, portMAX_DELAY);
			break;
		}
		default:
		break;
	}
}

void hosted_priv_event_handler(struct esp_priv_event *priv_event)
{
    if (priv_event->event_type == ESP_PRIV_EVENT_WIFI_STA_DISCONN) {
        /* TODO: remove this mutex, currently just prevent for post twice wifi disconnected event*/
        BaseType_t os_ret = xSemaphoreTake(priv_event_mutex, 0);
        if (os_ret == pdPASS) {
            esp_event_post(HOSTED_WIFI_EVENT_BASE, HOSTED_WIFI_EVT_STA_DISCONNECTED, NULL, 0, portMAX_DELAY);
            xSemaphoreGive(priv_event_mutex);
        } else {
            //hosted control task is waitting for wifi station event, just pass
        }
    } else if (priv_event->event_type == ESP_PRIV_EVENT_WIFI_AP_STACONN) {
        //event data = mac address len + assigned id = 7
        esp_event_post(HOSTED_WIFI_EVENT_BASE, HOSTED_WIFI_EVT_AP_STACONNECTED, priv_event->event_data, sizeof(uint8_t) * (MAC_ADDR_LEN + 1), portMAX_DELAY);
    } else if (priv_event->event_type == ESP_PRIV_EVENT_WIFI_AP_STADISCONN) {
        esp_event_post(HOSTED_WIFI_EVENT_BASE, HOSTED_WIFI_EVT_AP_STADISCONNECTED, priv_event->event_data, sizeof(uint8_t) * (MAC_ADDR_LEN + 1), portMAX_DELAY);
    }
}

static void hosted_control_task(void *arg)
{
    while (1) {
        control_message_t control_message;
        xQueueReceive(control_msg_queue, &control_message, portMAX_DELAY);
        if (is_wifi_control_cmd(control_message.command)) {
            wifi_cmd_handler(&control_message);
        } else {
            ble_cmd_handler(&control_message);
        }
    }
}
