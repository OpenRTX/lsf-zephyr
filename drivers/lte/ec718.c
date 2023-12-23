/*
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT zephyr_lte_ec718

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(csk_ec718, LOG_LEVEL_INF);

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include <zephyr/net/offloaded_netdev.h>

#include <zephyr/kernel.h>
#include <zephyr/net/ethernet.h>
#include <zephyr/net/net_pkt.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_offload.h>
#include <zephyr/net/socket_offload.h>
#include <zephyr/sys/fdtable.h>

#include "protocol_socket_host.h"

#include "sockets_internal.h"

#define SD_TO_OBJ(sd) ((void *)(sd + 1))
#define OBJ_TO_SD(obj) (((int)obj) - 1)

struct ec718_drv_data {
	/*
	 * Network interface of the sim module.
	 */
	struct net_if *netif;
    uint8_t mac_addr[6];
};

static struct ec718_drv_data s_ec718_drv_data;


static int offload_connect(void *obj, const struct sockaddr *addr,
			      socklen_t addrlen)
{
    int sockfd = OBJ_TO_SD(obj);

	int ret = proto_sock_connect(sockfd, addr, (uint32_t)addrlen);

    return ret;
}

static ssize_t offload_sendto(void *obj, const void *buf, size_t len,
				 int flags, const struct sockaddr *addr,
				 socklen_t addrlen)
{
	ssize_t ret;
	int sockfd = OBJ_TO_SD(obj);

	if (addr == NULL) {
		// printk(" send \n");
		ret = proto_sock_send(sockfd, buf, len, flags);
	} else {
		ret = proto_sock_sendto(sockfd, buf, len, flags, addr, (uint32_t)addrlen);
	}
	return ret;
}

static uint32_t start_tick = 0;


static ssize_t offload_recvfrom(void *obj, void *buf, size_t len, int flags,
				   struct sockaddr *addr, socklen_t *addrlen)
{
	ssize_t ret;
	int sockfd = OBJ_TO_SD(obj);

	if (addr == NULL) {
		ret = proto_sock_recv(sockfd, buf, len, flags);
	} else {
		ret = proto_sock_recvfrom(sockfd, buf, len, flags, addr, (uint32_t)addrlen);
	}
	return ret;
}

static ssize_t offload_read(void *obj, void *buffer, size_t count)
{
	return offload_recvfrom(obj, buffer, count, 0, NULL, 0);
}

static int offload_setsockopt(void *obj, int level, int optname,
				 const void *optval, socklen_t optlen)
{
	int ret;
	int ps_optname;
	int sockfd = OBJ_TO_SD(obj);

	if (optname == SO_SNDTIMEO) {
		ps_optname = PROTO_SOCK_SNDTIMEO;
		struct timeval *timeout = optval;
		struct proto_sock_timeval ps_timeout = {0};
		ps_timeout.tv_sec = (uint64_t)timeout->tv_sec;
		ps_timeout.tv_usec = (uint32_t)timeout->tv_usec;
		ret = proto_sock_set_socketopt(sockfd, level, ps_optname, &ps_timeout, sizeof(ps_timeout));
	} else if (optname == SO_RCVTIMEO) {
		ps_optname = PROTO_SOCK_RCVTIMEO;
		struct timeval *timeout = optval;
		struct proto_sock_timeval ps_timeout = {0};
		ps_timeout.tv_sec = (uint64_t)timeout->tv_sec;
		ps_timeout.tv_usec = (uint32_t)timeout->tv_usec;
		ret = proto_sock_set_socketopt(sockfd, level, ps_optname, &ps_timeout, sizeof(ps_timeout));
	} else if (optname == SO_KEEPALIVE) {
		ps_optname = PROTO_SOCK_KEEPALIVE;
		ret = proto_sock_set_socketopt(sockfd, level, ps_optname, optval, optlen);
	} else {
		return -ENOTSUP;
	}

	return ret;
}

static int offload_close(void *obj)
{
	printk(" close \n");
	int sockfd = OBJ_TO_SD(obj);
	
	int ret = proto_sock_close(sockfd);
	return ret;
}

static ssize_t offload_write(void *obj, const void *buffer,
					  size_t count)
{
	return offload_sendto(obj, buffer, count, 0, NULL, 0);
}

static int dummy_get(sa_family_t family,
				enum net_sock_type type,
				enum net_ip_protocol ip_proto,
				struct net_context **context)
{
	// LOG_ERR("NET_SOCKETS_OFFLOAD must be configured for this driver");
	return -1;
}

static struct net_offload offload_funcs = {
	.get = dummy_get,
	.bind = NULL,
	.listen = NULL,
	.connect = NULL,
	.accept = NULL,
	.send = NULL,
	.sendto = NULL,
	.recv = NULL,
	.put = NULL,
};

static const struct socket_op_vtable ec718_socket_fd_op_vtable = {
	.fd_vtable = {
		.read = offload_read,
		.write = offload_write,
		.close = offload_close,
		.ioctl = NULL,
	},
	.bind = NULL,
	.connect = offload_connect,
	.listen = NULL,
	.accept = NULL,
	.sendto = offload_sendto,
	.sendmsg = NULL,
	.recvfrom = offload_recvfrom,
	.getsockopt = NULL,
	.setsockopt = offload_setsockopt,
};

static int construct_zsock_addrinfo(struct proto_sock_addrinfo *ps_info, struct zsock_addrinfo **res)
{
	int ret = 0;
	struct zsock_addrinfo *ai;
	struct sockaddr *ai_addr;

	ai = k_calloc(1, sizeof(struct zsock_addrinfo));
	if (!ai) {
		ret = -ENOMEM;
		goto __exit;
	} else {
		/* Now, alloc the embedded sockaddr struct: */
		ai_addr = k_calloc(1, sizeof(struct sockaddr));
		if (!ai_addr) {
			k_free(ai);
			ret = -ENOMEM;
			goto __exit;
		}
	}

	if (ps_info->ai_family == PROTO_SOCK_AF_INET) {
		ai->ai_family = AF_INET;
	} else {
		ai->ai_family = AF_INET6;
	}

	if (ai->ai_family == AF_INET) {
		struct proto_sockaddr_in *ps_addr_in = (struct proto_sockaddr_in *) &ps_info->ai_addr;
		net_sin(ai_addr)->sin_family = ai->ai_family;
		net_sin(ai_addr)->sin_addr.s_addr = ps_addr_in->sin_addr.s_addr;
		net_sin(ai_addr)->sin_port = ps_addr_in->sin_port;
		ai->ai_addrlen = sizeof(struct sockaddr_in);
	} else {
		k_free(ai);
		k_free(ai_addr);
		return -ENOTSUP; //Fixme: add ipv6 support
	}
	if (ps_info->ai_socktype == 0) {
		ai->ai_socktype = 0;
	} else {
		if (ps_info->ai_socktype == PROTO_SOCK_AI_STREAM) {
			ai->ai_socktype = SOCK_STREAM;
		} else if (ps_info->ai_socktype == PROTO_SOCK_AI_DGRAM) {
			ai->ai_socktype = SOCK_DGRAM;
		} else {
			ai->ai_socktype = SOCK_RAW;
		}
	}
	ai->ai_addr = ai_addr;
	ai->ai_next = *res;
	*res = ai;

__exit:
	return ret;
}

static int offload_getaddrinfo(const char *node, const char *service,
				  const struct zsock_addrinfo *hints,
				  struct zsock_addrinfo **res)
{
	int ret = 0;
	struct proto_sock_addrinfo *ps_res = NULL;
	struct proto_sock_addrinfo ps_hints = {0};

	if (hints) {
		ps_hints.ai_flags |= ((hints->ai_flags & AI_PASSIVE) ? PROTO_SOCK_AI_PASSIVE : 0);
		ps_hints.ai_flags |= ((hints->ai_flags & AI_NUMERICHOST) ? PROTO_SOCK_AI_NUMERICHOST : 0);
		if (hints->ai_family == AF_UNSPEC) {
			ps_hints.ai_family = PROTO_SOCK_AF_UNSPEC;
		} else if (hints->ai_family == AF_INET) {
			ps_hints.ai_family = PROTO_SOCK_AF_INET;
		} else if (hints->ai_family == AF_INET6) {
			ps_hints.ai_family = PROTO_SOCK_AF_INET6;
		} else {
			return -ENOTSUP;
		}
		if (hints->ai_socktype == 0) {
			ps_hints.ai_socktype = 0;
		} else {
			if (hints->ai_socktype == SOCK_STREAM) {
				ps_hints.ai_socktype = 	PROTO_SOCK_AI_STREAM;
			} else {
				ps_hints.ai_socktype = 	PROTO_SOCK_AI_DGRAM;
			}
		}
		if (hints->ai_protocol == 0) {
			ps_hints.ai_protocol = 0;
		} else {
			if (hints->ai_protocol == IPPROTO_TCP) {
				ps_hints.ai_protocol = PROTO_SOCK_IPPROTO_TCP;
			} else if (hints->ai_protocol == IPPROTO_UDP) {
				ps_hints.ai_protocol = PROTO_SOCK_IPPROTO_UDP;
			} else {
				return -ENOTSUP;
			}
		}
	}

	ret = proto_sock_getaddrinfo(node, service, &ps_hints, &ps_res);
	if (ret < 0 || ps_res == NULL) {
		LOG_ERR("could not resovle: %s ret: %d", node, ret);
		return ret;
	}

	*res = NULL;
	for (struct proto_sock_addrinfo *ps_res_temp = ps_res;
		ps_res_temp != NULL; ps_res_temp = ps_res_temp->ai_next) {
			ret = construct_zsock_addrinfo(ps_res_temp, res);
			if (ret != 0) {
				LOG_ERR("failed to construct zsock addr");
				break;
			}
	}

	proto_sock_freeaddrinfo(ps_res);
	return ret;
}

static void offload_freeaddrinfo(struct zsock_addrinfo *res)
{
	while (res != NULL) {
		struct zsock_addrinfo *res_temp = res;
		res = res->ai_next;
		k_free(res_temp->ai_addr);
		k_free(res_temp);
	}
}

const struct socket_dns_offload offload_dns_ops = {
	.getaddrinfo = offload_getaddrinfo,
	.freeaddrinfo = offload_freeaddrinfo,
};

static int offload_socket_create(int family, int type, int proto)
{
	int ps_family, ps_type, ps_proto;
    int fd = z_reserve_fd();

	if (fd < 0) {
		printk("------get fd failed\n");
		return -1;
	}

	if (family == 0) {
		ps_family = 0;
	} else {
		if (family == AF_INET) {
			ps_family = PROTO_SOCK_AF_INET;
		} else {
			ps_family = PROTO_SOCK_AF_INET6;
		}
	}
	if (type == 0) {
		ps_type = 0;
	} else {
		if (type == SOCK_STREAM) {
			ps_type = PROTO_SOCK_AI_STREAM;
		} else if (type == SOCK_DGRAM) {
			ps_type = PROTO_SOCK_AI_DGRAM;
		} else {
			return -ENOTSUP;
		}
	}
	if (proto == 0) {
		ps_proto = 0;
	} else {
		if (proto == IPPROTO_TCP) {
			ps_proto = PROTO_SOCK_IPPROTO_TCP;
		} else if (proto == IPPROTO_UDP) {
			ps_proto = PROTO_SOCK_IPPROTO_UDP;
		} else {
			return -ENOTSUP;
		}
	}
	int sockfd = proto_sock_socket(ps_family, ps_type, ps_proto);
	if (sockfd < 0) {
		z_free_fd(fd);
		return -1;
	}

    z_finalize_fd(fd, SD_TO_OBJ(sockfd),
		      (const struct fd_op_vtable *)
					&ec718_socket_fd_op_vtable);
    return fd;
}

#include "ec718_conn.h"

static void net_iface_init(struct net_if *iface)
{
    const struct device *dev = net_if_get_device(iface);
	struct ec718_drv_data *data = dev->data;
    net_if_set_link_addr(iface, data->mac_addr, sizeof(data->mac_addr), NET_LINK_ETHERNET);
    data->netif = iface;
    iface->if_dev->offload = &offload_funcs;
	socket_offload_dns_register(&offload_dns_ops);
}

static struct offloaded_if_api api_funcs = {
	.iface_api.init = net_iface_init,
};


static int ec718_init(const struct device *dev)
{
    // printk("-------------- %s\n", __func__);
    return 0;
}

static bool offload_is_supported(int family, int type, int proto)
{
    return true;
}



NET_DEVICE_OFFLOAD_INIT(ec718_dev, "ec718_net", ec718_init, NULL, &s_ec718_drv_data,
                        NULL, 80, &api_funcs, 1472);
// NET_DEVICE_DT_INST_OFFLOAD_DEFINE(0, ec718_init, NULL, &s_ec718_drv_data, NULL,
// 				  80, &api_funcs, 1024);

NET_SOCKET_OFFLOAD_REGISTER(ec718_lte, CONFIG_NET_SOCKETS_OFFLOAD_PRIORITY,
			    AF_UNSPEC, offload_is_supported, offload_socket_create);

