/*
 * IMS IPSEC PCSCF module - Kernel Expire Listener implementation
 *
 * Copyright (C) 2026 Harish S <toharishs@gmail.com>
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ipsec_evt.h"
#include "../../core/dprint.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <libmnl/libmnl.h>
#include <linux/rtnetlink.h>
#include <linux/xfrm.h>

static int expire_cb(const struct nlmsghdr *nlh, void *data)
{
	if(nlh->nlmsg_type != XFRM_MSG_EXPIRE
			|| mnl_nlmsg_get_payload_len(nlh)
					   < sizeof(struct xfrm_user_expire)) {
		return MNL_CB_OK;
	}

	struct xfrm_user_expire *ue = mnl_nlmsg_get_payload(nlh);
	uint32_t spi = ntohl(ue->state.id.spi);
	int hard = ue->hard;

	LM_INFO("XFRM_MSG_EXPIRE received for SPI 0x%x (%s expire)\n", spi,
			hard ? "hard" : "soft");

	return MNL_CB_OK;
}

void ipsec_start_expire_listener(void)
{
	struct mnl_socket *nl = mnl_socket_open(NETLINK_XFRM);
	if(!nl) {
		LM_ERR("Failed to open XFRM Netlink socket in expire listener: %s\n",
				strerror(errno));
		return;
	}

	if(mnl_socket_bind(nl, XFRMGRP_EXPIRE, MNL_SOCKET_AUTOPID) < 0) {
		LM_ERR("Failed to bind XFRM Netlink socket to XFRMGRP_EXPIRE: %s\n",
				strerror(errno));
		mnl_socket_close(nl);
		return;
	}

	LM_INFO("XFRM Netlink Expire Listener initialized (listening on group "
			"XFRMGRP_EXPIRE)\n");

	char buf[MNL_SOCKET_BUFFER_SIZE];
	int ret;
	while((ret = mnl_socket_recvfrom(nl, buf, sizeof(buf))) > 0) {
		ret = mnl_cb_run(buf, ret, 0, 0, expire_cb, NULL);
		if(ret <= MNL_CB_STOP)
			break;
	}

	mnl_socket_close(nl);
}
