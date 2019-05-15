/*
 * siptrace module - helper module to trace sip messages
 *
 * Copyright (C) 2017 Daniel-Constantin Mierla (asipto.com)
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

#ifndef _SIPTRACE_DATA_H_
#define _SIPTRACE_DATA_H_

#include "../../core/str.h"
#include "../../core/usr_avp.h"

#ifdef STATISTICS
#include "../../core/counters.h"
#endif

#define XHEADERS_BUFSIZE 512
#define SIPTRACE_ADDR_MAX (IP_ADDR_MAX_STR_SIZE + 14)

typedef struct _siptrace_data
{
	struct usr_avp *avp;
	int_str avp_value;
	struct search_state state;
	str body;
	str callid;
	str method;
	str status;
	char *dir;
	str fromtag;
	str fromip;
	str totag;
	str toip;
	char toip_buff[SIPTRACE_ADDR_MAX];
	char fromip_buff[SIPTRACE_ADDR_MAX];
	struct timeval tv;
#ifdef STATISTICS
	stat_var *stat;
#endif
} siptrace_data_t;

enum UriState { STRACE_UNUSED_URI = 0, STRACE_RAW_URI = 1, STRACE_PARSED_URI = 2};

typedef struct {
	str correlation_id;
	union {
		str dup_uri;
		dest_info_t dest_info;
	} u;
	enum UriState uriState;
} siptrace_info_t;


enum siptrace_type_t {SIPTRACE_NONE=0, SIPTRACE_MESSAGE = 'm',
	SIPTRACE_TRANSACTION = 't', SIPTRACE_DIALOG = 'd'};

#endif
