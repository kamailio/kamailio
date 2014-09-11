/* $Id: nathelper.c 1808 2007-03-10 17:36:19Z bogdan_iancu $
 *
 * Copyright (C) 2003 Porta Software Ltd
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
 * History:
 * ---------
 * 2007-04-13   splitted from nathelper.c (ancuta)
*/


#ifndef _RTPPROXY_H
#define _RTPPROXY_H

#include <sys/uio.h>
#include "../../str.h"

/* Handy macros */
#define STR2IOVEC(sx, ix)       do {(ix).iov_base = (sx).s; (ix).iov_len = (sx).len;} while(0)
#define SZ2IOVEC(sx, ix)        do {(ix).iov_base = (sx); (ix).iov_len = strlen(sx);} while(0)

struct rtpp_node {
	unsigned int		idx;			/* overall index */
	str					rn_url;			/* unparsed, deletable */
	int					rn_umode;
	char				*rn_address;	/* substring of rn_url */
	int					rn_disabled;	/* found unaccessible? */
	unsigned			rn_weight;		/* for load balancing */
	unsigned int		rn_recheck_ticks;
        int                     rn_rep_supported;
        int                     rn_ptl_supported;
	struct rtpp_node	*rn_next;
};


struct rtpp_set{
	unsigned int 		id_set;
	unsigned			weight_sum;
	unsigned int		rtpp_node_count;
	int 				set_disabled;
	unsigned int		set_recheck_ticks;
	struct rtpp_node	*rn_first;
	struct rtpp_node	*rn_last;
	struct rtpp_set     *rset_next;
};


struct rtpp_set_head{
	struct rtpp_set		*rset_first;
	struct rtpp_set		*rset_last;
};

/* Functions from nathelper */
struct rtpp_node *select_rtpp_node(str, int);
char *send_rtpp_command(struct rtpp_node *, struct iovec *, int);

struct rtpp_set *get_rtpp_set(str *set_name);
int insert_rtpp_node(struct rtpp_set *const rtpp_list, const str *const url, const int weight, const int disabled);

int set_rtp_inst_pvar(struct sip_msg *msg, const str * const uri);

int init_rtpproxy_db(void);

extern str rtpp_db_url;
extern str rtpp_table_name;

#endif
