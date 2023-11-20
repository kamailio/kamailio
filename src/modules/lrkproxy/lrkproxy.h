/*
 * Copyright (C) 2003-2008 Sippy Software, Inc., http://www.sippysoft.com
 * Copyright (C) 2020 Mojtaba Esfandiari.S, Nasim-Telecom
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


#ifndef _LRKPROXY_H
#define _LRKPROXY_H

#include <sys/uio.h>
#include "../../core/str.h"

/* Handy macros */
#define STR2IOVEC(sx, ix)        \
	do {                         \
		(ix).iov_base = (sx).s;  \
		(ix).iov_len = (sx).len; \
	} while(0)
#define SZ2IOVEC(sx, ix)           \
	do {                           \
		(ix).iov_base = (sx);      \
		(ix).iov_len = strlen(sx); \
	} while(0)

#define CR '\r'
#define LF '\n'
#define EOB '\0'

enum lrk_operation
{
	OP_OFFER = 1,
	OP_ANSWER,
	OP_DELETE,
	OP_PING,
	OP_GETINFO,
	OP_SETCONNT,

	OP_ANY,
};


enum lrk_alg
{
	LRK_LINER = 0,
	LRK_RR
};

struct lrkp_node_conf
{
	int start_port;
	int end_port;
	int current_port;
	char internal_ip[20];
	char external_ip[20];
};

struct lrkp_node
{
	unsigned int idx; /* overall index */
	str ln_url;		  /* unparsed, deletable */
	int ln_umode;
	char *ln_address;	/* substring of rn_url */
	int ln_enable;		/* found unaccessible? */
	unsigned ln_weight; /* for load balancing */
						//    unsigned int		ln_recheck_ticks;
						//    int                 ln_rep_supported;
						//    int                 ln_ptl_supported;
	struct lrkp_node_conf *lrkp_n_c;
	struct lrkp_node *ln_next;
};


struct lrkp_set
{
	unsigned int id_set;
	unsigned weight_sum;
	unsigned int lrkp_node_count;
	int set_disabled;
	unsigned int set_recheck_ticks;
	struct lrkp_node *ln_first;
	struct lrkp_node *ln_last;
	struct lrkp_set *lset_next;
};


struct lrkp_set_head
{
	struct lrkp_set *lset_first;
	struct lrkp_set *lset_last;
};
/* Functions from nathelper */
//struct lrkp_node *lrkp_node(str, int);
struct lrkp_node *select_lrkp_node(int);
char *send_lrkp_command(struct lrkp_node *, struct iovec *, int, int);

struct lrkp_set *get_lrkp_set(str *set_name);
int insert_lrkp_node(struct lrkp_set *const lrkp_list, const str *const url,
		const int weight, const int enable);


#endif //_LRKPROXY_H
