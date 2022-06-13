// #include <net/net_pkt.h>
// #include <net/net_if.h>
#include <net/net_offload.h>
#include "csk_offload.h"
#include "csk_net_offload.h"

struct csk_offload_api {
	void (*init)(struct csk_offload_dev *iface);
};

static const struct csk_offload_api csk_offload_iface_api = {
	.init = csk_net_offload_init
};


static int csk_offload_init(const struct device *dev)
{
    return 0;
}


NET_DEVICE_OFFLOAD_INIT(csk_offload, "csk_offload_drv",
			csk_offload_init, NULL,
			NULL, NULL,
			CONFIG_WIFI_INIT_PRIORITY, &csk_offload_iface_api,
			1472);
