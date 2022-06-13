#include <string.h>
// #include "FreeRTOS.h"
// #include "task.h"
#include "lwip/opt.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "netif/etharp.h"
#include "lwip/dhcp.h"
#include "lwip/dns.h"
#include "lwip/timeouts.h"
#include "lwip/init.h"
#include "lwip/tcpip.h"

#include "lwip/prot/dhcp.h"

// #include "ethernetif.h"
// #include "venus_log.h"
#include "network_adapter/priv/wlanif.h"

static struct netif wlanif_sta, wlanif_ap;

void network_stack_init(void)
{
    tcpip_init(NULL, NULL);
    ip_addr_t dns_server0, dns_server1;
    IP4_ADDR(&dns_server0, 8,8,8,8);      //google dns server
    IP4_ADDR(&dns_server1, 119,29,29,29); //tencent dns server 
    dns_setserver(0, &dns_server0);
    dns_setserver(1, &dns_server1);
}

struct netif *wlanif_create_sta(uint8_t *mac_addr_bytes, netif_status_callback_fn netif_status_cb)
{
    wlanif_sta.name[0] = 's';
    wlanif_sta.name[1] = 't';
    memcpy(wlanif_sta.hwaddr, mac_addr_bytes, sizeof(uint8_t) * 6);
	netif_add(&wlanif_sta, NULL, NULL, NULL, NULL, &ethernetif_init, &tcpip_input);

    wlanif_sta.flags |= NETIF_FLAG_LINK_UP;
    wlanif_sta.flags |= NETIF_FLAG_UP;

    netif_set_default(&wlanif_sta);
    if (netif_status_cb) {
        netif_set_status_callback(&wlanif_sta, netif_status_cb);
    }
    if (netif_is_link_up(&wlanif_sta)) {
        netif_set_up(&wlanif_sta);
    } else {
        netif_set_down(&wlanif_sta);
    }
    return &wlanif_sta;
}

void wlanif_delete_sta(void)
{
    netif_remove(&wlanif_sta);
}

struct netif *wlanif_create_ap(uint8_t *mac_addr_bytes, netif_status_callback_fn netif_status_cb)
{
    ip4_addr_t ipaddr;
    ip4_addr_t netmask;
    ip4_addr_t gw;

    IP4_ADDR(&ipaddr, 192, 168, 1, 1);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gw, 192, 168, 1, 1);

    wlanif_ap.name[0] = 'a';
    wlanif_ap.name[1] = 'p';
    memcpy(wlanif_ap.hwaddr, mac_addr_bytes, sizeof(uint8_t) * 6);
    netif_add(&wlanif_ap,  &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &tcpip_input);

    wlanif_ap.flags |= NETIF_FLAG_LINK_UP;
    wlanif_ap.flags |= NETIF_FLAG_UP;

    netif_set_default(&wlanif_ap);
    if (netif_status_cb) {
        netif_set_status_callback(&wlanif_ap, netif_status_cb);
    }
    if (netif_is_link_up(&wlanif_ap)) {
        netif_set_up(&wlanif_ap);
    } else {
        netif_set_down(&wlanif_ap);
    }
    return &wlanif_ap;
}

void wlanif_delete_ap(void)
{
    netif_remove(&wlanif_ap);
}

