/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>
#include <device.h>

#include <net/net_if.h>
#include <net/net_core.h>
#include <net/net_context.h>
#include <net/net_mgmt.h>

#include "csk6/csk_wifi.h"

//#include "iperf.h"

#include <net/socket.h>
#include "hal/gpio.h"
#include "hal/Driver_GPIO.h"
#include <hal/IOMuxManager.h>

extern void* GPIOA(void);
void Wifi_Test_Pin_Set(uint8_t pin, uint8_t value)
{
    GPIO_PinWrite(GPIOA(), (1 << pin), value);
}

#define NET_SIZE    1024

static void tcp_client_task(void *arg1, void *arg2, void *arg3);
K_THREAD_DEFINE(tcp_client_thread, 2048,
                tcp_client_task, NULL, NULL, NULL,
                5, 0, 0);

uint8_t lorem_ipsum[NET_SIZE];

#ifndef TARGET_IP_ADDRESS
	#define TARGET_IP_ADDRESS 		"192.168.31.22"
#endif
#ifndef TARGET_PORT
	#define TARGET_PORT			12345
#endif

extern void Wifi_Test_Pin_Set(uint8_t pin, uint8_t value);
K_SEM_DEFINE(ip_sem, 0, 1);

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

    k_sem_give(&ip_sem);
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

extern struct k_heap _system_heap;

static int sendall(int sock, const void *buf, size_t len)
{
	while (len) {
        // printk("sendall:%d\n", len);
		int out_len = send(sock, buf, len, 0);

		if (out_len < 0) {
			return out_len;
		}
		buf = (const char *)buf + out_len;
		len -= out_len;
	}

	return 0;
}


uint32_t test_get_speed(uint32_t time)
{
    uint32_t speed = 1000000 / time;
    speed = 8 * speed * NET_SIZE;

    return speed >> 10;

}

#pragma GCC push_options
#pragma GCC optimize("O2")
#define AVERNUM 20
void test_info(uint32_t time)
{
    static uint32_t sum = 0;
    static uint8_t cnt = 0;

    if(unlikely(time < 300000))
    {
        return;
    }
 
    sum += time;
    cnt++;

    if(cnt >= AVERNUM)
    {
        sum = sum/AVERNUM;
        //cycle = 3.3ns
        sum = sum * 33;
        sum = sum / 10000;
        printk("socket average time:%d(us) speed:%d(kb/s)\n", sum, test_get_speed(sum));
        sum = 0;
        cnt = 0;
    }
}
#pragma GCC pop_options

#define PRINT_HEAP_INFO()  {extern struct k_heap _system_heap;sys_heap_print_info(&_system_heap.heap,0);}
static void tcp_client_task(void *arg1, void *arg2, void *arg3)
{
    uint32_t time = 0;
	printk("Please startup a tcp server and pass parameters into the program\n");
    k_sem_take(&ip_sem, K_FOREVER);
	printk("wifi connected event is token\n");

    for(uint32_t i = 0; i < sizeof(lorem_ipsum); i++)
    {
        lorem_ipsum[i] = i;
    }

    int socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (socket_fd < 0) {
        printk("create socket failed\n");
        goto __cleanup;
    }

    struct sockaddr_in socket_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(TARGET_PORT),
    };
    inet_pton(AF_INET, TARGET_IP_ADDRESS, &socket_addr.sin_addr);
    int conn_ret = connect(socket_fd, (const struct sockaddr *)&socket_addr, sizeof(socket_addr));
    if (conn_ret < 0) {
        printk("connect socket failed, closing socket... %d\n", errno);
        goto __cleanup;
    }

    while(1){
        // PRINT_HEAP_INFO();
        time = k_cycle_get_32();
        int wd_ret = sendall(socket_fd, lorem_ipsum, sizeof(lorem_ipsum) - 1);
        time = k_cycle_get_32() - time;
        if (wd_ret < 0) {
            printk("write socket failed, closing socket... %d\n", errno);
            close(socket_fd);
            goto __cleanup;
        }
        test_info(time);
        // printk("write socket successful  %d time:%d(cycle)\n", count, time);
        k_msleep(100);
    }
__cleanup:
        close(socket_fd);

    printk("task delete\n");
}

csk_wifi_event_cb_t wifi_event_cb;
void main(void)
{
    csk_wifi_init();
    // csk_wifi_event_cb_t *wifi_event_cb = k_malloc(sizeof(csk_wifi_event_cb_t));
    wifi_event_cb.handler = &wifi_event_handler;
    wifi_event_cb.events = CSK_WIFI_EVT_STA_CONNECTED | CSK_WIFI_EVT_STA_DISCONNECTED;
    wifi_event_cb.arg = NULL;
    csk_wifi_add_callback(&wifi_event_cb);

    csk_wifi_result_t wifi_result;
    csk_wifi_sta_config_t sta_config = {
        .ssid = "TreeNewbee",
        .pwd = "wifishare123@",
    };
    csk_wifi_sta_connect(&sta_config, &wifi_result, K_FOREVER);

    net_mgmt_init_event_callback(&dhcp_cb, handler_cb, NET_EVENT_IPV4_DHCP_BOUND);
    net_mgmt_add_event_callback(&dhcp_cb);
    struct net_if *iface = net_if_get_default();
    if (!iface) {
        printk("wifi interface not available");
        return;
    }
    net_dhcpv4_start(iface);
}
