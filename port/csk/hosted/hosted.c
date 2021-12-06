#include <stdlib.h>
#include "common/util.h"
#include "commands.h"
#include "platform_wrapper.h"

#include "hosted_gpio.h"
#include "spi_drv.h"
#include "netdev_api.h"
#include "adapter.h"

#include "hosted/hosted.h"

K_SEM_DEFINE(hosted_sync_sem, 0, 1);
static sys_slist_t hosted_callback_list;
static uint8_t hosted_init_status = 0;

static void reset_slave(void);
static void spi_driver_event_handler(uint8_t event);

int hosted_init()
{
	reset_slave();
	network_init(); //init virturl network interface for wifi
	spi_link_init(spi_driver_event_handler);
    hosted_init_status = 1;
    return SUCCESS;
}

bool hosted_is_inited(void)
{
    return hosted_init_status == 1;
}

bool hosted_event_is_set(uint32_t events)
{
    sys_slist_t *list = &hosted_callback_list;
    struct hosted_event_callback *cb, *tmp;
    SYS_SLIST_FOR_EACH_CONTAINER_SAFE(list, cb, tmp, node) {
        if (cb->events & events) {
            return true;
        }
    }
    return false;
}

int hosted_add_callback(struct hosted_event_callback *hosted_event_cb)
{
    sys_slist_prepend(&hosted_callback_list, &hosted_event_cb->node);
    return 0;
}

static inline void hosted_fire_callbacks(sys_slist_t *list, uint32_t events)
{
    struct hosted_event_callback *cb, *tmp;

    SYS_SLIST_FOR_EACH_CONTAINER_SAFE(list, cb, tmp, node) {
        if (cb->events & events) {
            cb->handler(cb->events & events, cb->arg);
        }
    }
}

int hosted_sync_set(bool set)
{
    if (set) {
        k_sem_give(&hosted_sync_sem);
    }
    return 0;
}

int hosted_wait_sync(k_timeout_t timeout)
{
    return k_sem_take(&hosted_sync_sem, timeout);
}

bool hosted_is_synced()
{
    if (k_sem_count_get(&hosted_sync_sem) == 0) {
        int handshake_status = csk_read_pin(GPIO_HANDSHAKE_GPIO_Port, GPIO_HANDSHAKE_Pin);
        int dataready_status = csk_read_pin(GPIO_HANDSHAKE_GPIO_Port, GPIO_HANDSHAKE_Pin);
        if (handshake_status == 1 && dataready_status == 1) {  //detect init signal
            return false;
        }
    }
    return true;
}

static inline void control_path_init(void)
{
	control_path_platform_init();
}

static void reset_slave(void)
{
	hosted_gpio_init();
	csk_write_pin(GPIO_RESET_GPIO_Port, GPIO_RESET_Pin, GPIO_PIN_RESET);

    k_msleep(4);
	csk_write_pin(GPIO_RESET_GPIO_Port, GPIO_RESET_Pin, GPIO_PIN_SET);

	/* stop spi transactions short time to avoid slave sync issues */
    hosted_wait_sync(K_FOREVER);
}

static void spi_driver_event_handler(uint8_t event)
{
	switch(event) {
		case SPI_DRIVER_ACTIVE: {
			/* Initiate control path now */
            control_path_init();  //init control path and control task
            hosted_fire_callbacks(&hosted_callback_list, HOSTED_LINK_EVT_ACTIVE);
			break;
		}
		default:
		break;
	}
}

void hosted_priv_event_handler(struct esp_priv_event *priv_event)
{
    if (priv_event->event_type == ESP_PRIV_EVENT_WIFI_STA_DISCONN) {
        hosted_fire_callbacks(&hosted_callback_list, HOSTED_WIFI_EVT_STA_DISCONNECTED);
    } else if (priv_event->event_type == ESP_PRIV_EVENT_WIFI_AP_STACONN) {
        hosted_fire_callbacks(&hosted_callback_list, HOSTED_WIFI_EVT_AP_STACONNECTED);
    } else if (priv_event->event_type == ESP_PRIV_EVENT_WIFI_AP_STADISCONN) {
        hosted_fire_callbacks(&hosted_callback_list, HOSTED_WIFI_EVT_AP_STADISCONNECTED);
    }
}
