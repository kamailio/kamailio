/**
 * Copyright (C) 2016 Daniel-Constantin Mierla (asipto.com)
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _SR_KEMI_H_
#define _SR_KEMI_H_

#include "str.h"
#include "parser/msg_parser.h"

#define SR_KEMIP_NONE	(0)
#define SR_KEMIP_INT	(1<<0)
#define SR_KEMIP_STR	(1<<1)
#define SR_KEMIP_BOOL	(1<<2)

#define SR_KEMI_PARAMS_MAX	6

typedef struct sr_kemi {
	str mname;
	str fname;
	int rtype;
	void *func;
	int ptypes[SR_KEMI_PARAMS_MAX];
} sr_kemi_t;

typedef struct sr_kemi_item {
	sr_kemi_t *item;
	int nparams;
	struct sr_kemi_item *next;
} sr_kemi_item_t;

typedef union {
	int n;
	str s;
} sr_kemi_val_t;

/* only sip_msg_t */
typedef int (*sr_kemi_fm_f)(sip_msg_t*);

/* sip_msg_t and one int|str param */
typedef int (*sr_kemi_fmn_f)(sip_msg_t*, int);
typedef int (*sr_kemi_fms_f)(sip_msg_t*, str*);

/* sip_msg_t and two int|str param */
typedef int (*sr_kemi_fmnn_f)(sip_msg_t*, int, int);
typedef int (*sr_kemi_fmns_f)(sip_msg_t*, int, str*);
typedef int (*sr_kemi_fmsn_f)(sip_msg_t*, str*, int);
typedef int (*sr_kemi_fmss_f)(sip_msg_t*, str*, str*);

/* sip_msg_t and three int|str param */
typedef int (*sr_kemi_fmnnn_f)(sip_msg_t*, int, int, int);
typedef int (*sr_kemi_fmnns_f)(sip_msg_t*, int, int, str*);
typedef int (*sr_kemi_fmnsn_f)(sip_msg_t*, int, str*, int);
typedef int (*sr_kemi_fmnss_f)(sip_msg_t*, int, str*, str*);
typedef int (*sr_kemi_fmsnn_f)(sip_msg_t*, str*, int, int);
typedef int (*sr_kemi_fmsns_f)(sip_msg_t*, str*, int, str*);
typedef int (*sr_kemi_fmssn_f)(sip_msg_t*, str*, str*, int);
typedef int (*sr_kemi_fmsss_f)(sip_msg_t*, str*, str*, str*);

#endif
