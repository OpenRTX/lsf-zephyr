#include <zephyr.h>

#include <stdlib.h>
#include <sys/printk.h>
#include <sys/sys_heap.h>

#include <net/net_if.h>
#include <net/net_core.h>
#include <net/net_context.h>
#include <net/net_mgmt.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(csk_wifi_sta, LOG_LEVEL_DBG);

#include "csk6/csk_wifi.h"

static csk_wifi_event_cb_t wifi_event_cb;
static struct net_mgmt_event_callback dhcp_cb;
static void handler_cb(struct net_mgmt_event_callback *cb, uint32_t mgmt_event, struct net_if *iface)
{
    if (mgmt_event != NET_EVENT_IPV4_DHCP_BOUND) {
        return;
    }

    char buf[NET_IPV4_ADDR_LEN];

    printk("Your address: %s\n", net_addr_ntop(AF_INET, &iface->config.dhcpv4.requested_ip, buf, sizeof(buf)));
    printk("Lease time: %u seconds\n", iface->config.dhcpv4.lease_time);
    printk("Subnet: %s\n", net_addr_ntop(AF_INET, &iface->config.ip.ipv4->netmask, buf, sizeof(buf)));
    printk("Router: %s\n", net_addr_ntop(AF_INET, &iface->config.ip.ipv4->gw, buf, sizeof(buf)));

}


static void wifi_event_handler(csk_wifi_event_t events, void *event_data, uint32_t data_len, void *arg)
{
    if (events & CSK_WIFI_EVT_STA_CONNECTED) {
        printk("[WiFi sta] connected\n");
    } else if (events & CSK_WIFI_EVT_STA_DISCONNECTED) {
        printk("[WiFi sta] disconnected\n");
    } else {
        abort();
    }
}

void network_domain_task(void){
    csk_wifi_init();
    wifi_event_cb.handler = &wifi_event_handler;
    wifi_event_cb.events = CSK_WIFI_EVT_STA_CONNECTED | CSK_WIFI_EVT_STA_DISCONNECTED;
    wifi_event_cb.arg = NULL;
    csk_wifi_add_callback(&wifi_event_cb);

    csk_wifi_sta_config_t sta_config = {
        .ssid = CONFIG_EXAMPLE_WIFI_SSID,
        .pwd = CONFIG_EXAMPLE_WIFI_PASSWORD,
        .encryption_mode = CSK_WIFI_AUTH_WPA2_PSK
    };

    int retry_count = 0;
    csk_wifi_result_t wifi_result;
    do {
        printk("connecting to wifi: %s ...\n", sta_config.ssid);
        int ret = csk_wifi_sta_connect(&sta_config, &wifi_result, K_FOREVER);
        if (ret == 0) {
            break;
        } else {
            if (wifi_result == CSK_WIFI_ERR_STA_FAILED) {
                retry_count++;
                printk("retry to connecting wifi ... %d\n", retry_count);
            } else {
                printk("AP not found or invalid password\n");
                abort();
            }
        }
    } while (retry_count < 10);
    net_mgmt_init_event_callback(&dhcp_cb, handler_cb, NET_EVENT_IPV4_DHCP_BOUND);
    net_mgmt_add_event_callback(&dhcp_cb);
    struct net_if *iface = net_if_get_default();
    if (!iface) {
        printk("wifi interface not available");
        return;
    }
    net_dhcpv4_start(iface);

	while (1) {
		k_sleep(K_MSEC(1000));
	}
}


struct k_thread network_thread;

K_KERNEL_STACK_DEFINE(network_stack, 512);

void network_domain(void){
	k_thread_create(&network_thread, network_stack,
				K_KERNEL_STACK_SIZEOF(network_stack), &network_domain_task, NULL, NULL, NULL,
				2, 0, K_NO_WAIT);
	k_thread_system_pool_assign(&network_thread);
}
