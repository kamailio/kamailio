/**
 * Copyright (C) 2022 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _SIPREPO_DATA_
#define _SIPREPO_DATA_

#include <time.h>

#include "../../core/parser/msg_parser.h"

typedef struct siprepo_msg {
	unsigned int hid;
	int mtype;
	str msgid;
	str callid;
	str ftag;
	str cseqnum;
	str cseqmet;
	str vbranch;
	str dbuf;
	unsigned int cseqmetid;
	int msgno;
	int pid;
	flag_t mflags;
	receive_info_t rcv;
	time_t itime;
	struct siprepo_msg *next;
	struct siprepo_msg *prev;
} siprepo_msg_t;

typedef struct siprepo_slot {
	siprepo_msg_t *plist;
	gen_lock_t lock;
} siprepo_slot_t;

int siprepo_table_init(void);
int siprepo_msg_set(sip_msg_t *msg, str *msgid);
int siprepo_msg_rm(sip_msg_t *msg, str *callid, str *msgid);
int siprepo_msg_pull(sip_msg_t *msg, str *callid, str *msgid, str *rname);
int siprepo_msg_check(sip_msg_t *msg);
void siprepo_msg_timer(unsigned int ticks, int worker, void *param);

#endif
