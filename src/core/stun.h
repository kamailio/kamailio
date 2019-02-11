/*
 * Portions Copyright (C) 2013 Crocodile RCS Ltd
 *
 * Based on "ser_stun.h". Copyright (C) 2001-2003 FhG Fokus
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

#ifndef _STUN_H
#define _STUN_H

#include "ip_addr.h"

/* type redefinition */
typedef unsigned char UCHAR_T;
typedef unsigned short USHORT_T;
typedef unsigned int UINT_T;
typedef unsigned long ULONG_T;

#define MAGIC_COOKIE	0x2112A442
#define TRANSACTION_ID	12

struct transaction_id {
        UINT_T magic_cookie;
        UCHAR_T id[TRANSACTION_ID];
};

struct stun_hdr {
        USHORT_T type;
        USHORT_T len;
        struct transaction_id id;
};

struct stun_attr {
        USHORT_T type;
        USHORT_T len;
};

typedef struct stun_event_info {
	char *buf;
	unsigned int len;
	struct receive_info *rcv;
} stun_event_info_t;

int stun_process_msg(char* buf, unsigned int len, struct receive_info* ri);

#endif /* _STUN_H */
