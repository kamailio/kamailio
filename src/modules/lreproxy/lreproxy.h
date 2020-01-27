/*
 * Copyright (C) 2019-2020 Mojtaba Esfandiari.S, Nasim-Telecom

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


#ifndef _LREPROXY_H
#define _LREPROXY_H

#include <sys/uio.h>
#include "../../core/str.h"

/* Handy macros */
#define STR2IOVEC(sx, ix)       do {(ix).iov_base = (sx).s; (ix).iov_len = (sx).len;} while(0)
#define SZ2IOVEC(sx, ix)        do {(ix).iov_base = (sx); (ix).iov_len = strlen(sx);} while(0)

#define CR '\r'
#define LF '\n'
#define EOB '\0'

enum lre_operation {
    OP_OFFER = 1,
    OP_ANSWER,
    OP_DELETE,
    OP_PING,
    OP_GETINFO,
    OP_SETCONNT,

    OP_ANY,
};

//typedef struct lre_sdp_info{
//    int_str ip_addr;
//    int_str port;
//}lre_sdp_info_t;


enum lre_alg{
    LRE_LINER=0,
    LRE_RR
};

struct lrep_node_conf
{
    int start_port;
    int end_port;
    int current_port;
    char internal_ip[20];
    char external_ip[20];
};

struct lrep_node {
    unsigned int		idx;			/* overall index */
    str					ln_url;			/* unparsed, deletable */
    int					ln_umode;
    char				*ln_address;	/* substring of rn_url */
    int					ln_enable;	/* found unaccessible? */
    unsigned			ln_weight;		/* for load balancing */
//    unsigned int		ln_recheck_ticks;
//    int                 ln_rep_supported;
//    int                 ln_ptl_supported;
    struct lrep_node_conf     *lrep_n_c;
    struct lrep_node	*ln_next;
};



struct lrep_set{
    unsigned int 		id_set;
    unsigned			weight_sum;
    unsigned int		lrep_node_count;
    int 				set_disabled;
    unsigned int		set_recheck_ticks;
    struct lrep_node	*ln_first;
    struct lrep_node	*ln_last;
    struct lrep_set     *lset_next;
};



struct lrep_set_head{
    struct lrep_set		*lset_first;
    struct lrep_set		*lset_last;
};
/* Functions from nathelper */
//struct lrep_node *lrep_node(str, int);
struct lrep_node *select_lrep_node(int);
char *send_lrep_command(struct lrep_node *, struct iovec *, int, int);

struct lrep_set *get_lrep_set(str *set_name);
int insert_lrep_node(struct lrep_set *const rtpp_list, const str *const url,
		const int weight, const int enable);

//static int replace_body_total(sip_msg_t *msg, struct lrep_node *n, char *flags, int type);

//int set_rtp_inst_pvar(struct sip_msg *msg, const str * const uri);

//int init_rtpproxy_db(void);

//extern str rtpp_db_url;
//extern str rtpp_table_name;

#endif  //_LREPROXY_H
