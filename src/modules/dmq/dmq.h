/**
 * dmq module - distributed message queue
 *
 * Copyright (C) 2011 Bucur Marius - Ovidiu
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#ifndef _DMQ_H_
#define _DMQ_H_

#include "../../core/dprint.h"
#include "../../core/error.h"
#include "../../core/sr_module.h"
#include "../../core/str_list.h"
#include "../../modules/tm/tm_load.h"
#include "../../core/parser/parse_uri.h"
#include "../../modules/sl/sl.h"
#include "peer.h"
#include "worker.h"

#define DEFAULT_NUM_WORKERS 2
#define MIN_PING_INTERVAL 5

extern int dmq_num_workers;
extern int dmq_worker_usleep;
extern dmq_worker_t *dmq_workers;
extern dmq_peer_t *dmq_notification_peer;
extern str dmq_server_address;
extern dmq_peer_list_t *dmq_peer_list;
extern str dmq_request_method;
extern str dmq_server_socket;
extern sip_uri_t dmq_server_uri;
extern str_list_t *dmq_notification_address_list;
extern int dmq_multi_notify;
/* sl and tm */
extern struct tm_binds _dmq_tmb;
extern sl_api_t _dmq_slb;

extern str dmq_200_rpl;
extern str dmq_400_rpl;
extern str dmq_500_rpl;
extern str dmq_404_rpl;

#endif
