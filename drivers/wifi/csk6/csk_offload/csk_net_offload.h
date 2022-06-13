#pragma once

#include "csk_offload.h"

struct csk_offload_dev {
	void *z_iface;
};

int csk_net_offload_init(struct csk_offload_dev *csk_offload_dev);
