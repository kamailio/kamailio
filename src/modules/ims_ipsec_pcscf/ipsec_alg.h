/*
 * IMS IPSEC PCSCF module - 3GPP Rel-18 IPsec Netlink algorithm header
 *
 * Copyright (C) 2026 Harish S <toharishs@gmail.com>
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef IMS_IPSEC_PCSCF_IPSEC_ALG_H
#define IMS_IPSEC_PCSCF_IPSEC_ALG_H

#include "../../core/str.h"
#include <libmnl/libmnl.h>
#include <linux/rtnetlink.h>
#include <linux/xfrm.h>

#ifndef XFRMA_ALG_AEAD
#define XFRMA_ALG_AEAD 22
#endif

#ifndef XFRMA_ALG_AUTH_TRUNC
#define XFRMA_ALG_AUTH_TRUNC 23
#endif

int is_aead_alg(const str *ealg);
int is_auth_trunc_alg(const str *alg);
int ipsec_put_aead_attr(
		struct nlmsghdr *nlh, const str *ck, const str *ik, const str *ealg);
int ipsec_put_auth_trunc_attr(
		struct nlmsghdr *nlh, const str *ik, const str *alg);

#endif /* IMS_IPSEC_PCSCF_IPSEC_ALG_H */
