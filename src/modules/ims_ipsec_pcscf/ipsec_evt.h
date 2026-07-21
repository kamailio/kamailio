/*
 * IMS IPSEC PCSCF module - Kernel Expire Listener header
 *
 * Copyright (C) 2026 Harish S <toharishs@gmail.com>
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef IMS_IPSEC_PCSCF_IPSEC_EVT_H
#define IMS_IPSEC_PCSCF_IPSEC_EVT_H

#include <stdint.h>

#ifndef XFRMGRP_EXPIRE
#define XFRMGRP_EXPIRE 4
#endif

#ifndef MNL_SOCKET_BUFFER_SIZE
#define MNL_SOCKET_BUFFER_SIZE 8192
#endif

void ipsec_start_expire_listener(void);

#endif /* IMS_IPSEC_PCSCF_IPSEC_EVT_H */
