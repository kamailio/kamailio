/*
 * siptrace module - helper module to trace sip messages
 *
 * Copyright (C) 2017 kamailio.org
 *
 * This file is part of Kamailio, a free SIP server.
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
 *
 */

#ifndef _SIPTRACE_SEND_H_
#define _SIPTRACE_SEND_H_

#include "../../core/parser/msg_parser.h"
#include "../../core/ip_addr.h"
#include "siptrace_data.h"

int siptrace_copy_proto(int proto, char *buf);
int sip_trace_prepare(sip_msg_t *msg);
int sip_trace_xheaders_write(struct _siptrace_data *sto);
int sip_trace_xheaders_read(struct _siptrace_data *sto);
int sip_trace_xheaders_free(struct _siptrace_data *sto);
int trace_send_duplicate(char *buf, int len, struct dest_info *dst2);

#endif
