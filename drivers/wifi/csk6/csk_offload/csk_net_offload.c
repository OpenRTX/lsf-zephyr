#include <zephyr.h>
#include <kernel.h>
#include <device.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <net/net_offload.h>
#include <net/net_pkt.h>
#include <net/net_if.h>

#include "csk_net_offload.h"

static int csk_net_off_get(sa_family_t family,
			  enum net_sock_type type,
			  enum net_ip_protocol ip_proto,
			  struct net_context **context)
{
    return 0;
}

static int csk_net_off_bind(struct net_context *context,
			   const struct sockaddr *addr,
			   socklen_t addrlen)
{
    return 0;
}

static int csk_net_off_listen(struct net_context *context, int backlog)
{
    return 0;
}

static int csk_net_off_connect(struct net_context *context,
			      const struct sockaddr *addr,
			      socklen_t addrlen,
			      net_context_connect_cb_t cb,
			      int32_t timeout,
			      void *user_data)
{
    return 0;
}

static int csk_net_off_accept(struct net_context *context,
			     net_tcp_accept_cb_t cb, int32_t timeout,
			     void *user_data)
{
    return 0;
}

static int csk_net_off_send(struct net_pkt *pkt,
			   net_context_send_cb_t cb,
			   int32_t timeout,
			   void *user_data)
{
    return 0;
}

static int csk_net_off_sendto(struct net_pkt *pkt,
			     const struct sockaddr *dst_addr,
			     socklen_t addrlen,
			     net_context_send_cb_t cb,
			     int32_t timeout,
			     void *user_data)
{
    return 0;
}

static int csk_net_off_recv(struct net_context *context,
			   net_context_recv_cb_t cb,
			   int32_t timeout,
			   void *user_data)
{
    return 0;
}

static int csk_net_off_put(struct net_context *context)
{
    return 0;
}

static struct net_offload csk_net_offload = {
	.get	       = csk_net_off_get,
	.bind	       = csk_net_off_bind,
	.listen	       = csk_net_off_listen,
	.connect       = csk_net_off_connect,
	.accept	       = csk_net_off_accept,
	.send	       = csk_net_off_send,
	.sendto	       = csk_net_off_sendto,
	.recv	       = csk_net_off_recv,
	.put	       = csk_net_off_put,
};

int csk_net_offload_init(struct csk_offload_dev *csk_offload_dev)
{
    struct net_if *z_iface = csk_offload_dev->z_iface;
    z_iface->if_dev->offload = &csk_net_offload;
    return 0;
}
