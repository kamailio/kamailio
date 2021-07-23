/*
 * Copyright (C) 2003 Porta Software Ltd
 * Copyright (C) 2014-2015 Sipwise GmbH, http://www.sipwise.com
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
*/


#ifndef _RTPENGINE_H
#define _RTPENGINE_H

#include "bencode.h"
#include "../../core/str.h"
#include "../../core/locking.h"

#define RTPENGINE_MIN_RECHECK_TICKS		0
#define RTPENGINE_MAX_RECHECK_TICKS		((unsigned int)-1)

enum rtpe_operation {
        OP_OFFER = 1,
        OP_ANSWER,
        OP_DELETE,
        OP_START_RECORDING,
        OP_STOP_RECORDING,
        OP_QUERY,
        OP_PING,
        OP_BLOCK_DTMF,
        OP_UNBLOCK_DTMF,
        OP_BLOCK_MEDIA,
        OP_UNBLOCK_MEDIA,
        OP_SILENCE_MEDIA,
        OP_UNSILENCE_MEDIA,
        OP_START_FORWARDING,
        OP_STOP_FORWARDING,
        OP_PLAY_MEDIA,
        OP_STOP_MEDIA,
        OP_PLAY_DTMF,

        OP_ANY,
};

struct rtpp_node {
	unsigned int		idx;			/* overall index */
	str			rn_url;			/* unparsed, deletable */
	enum {
		RNU_UNKNOWN = -1,
		RNU_LOCAL = 0,
		RNU_UDP = 1,
		RNU_UDP6 = 6,
		RNU_WS = 2,
		RNU_WSS = 3,
	}			rn_umode;
	char			*rn_address;		/* substring of rn_url */
	int			rn_disabled;		/* found unaccessible? */
	unsigned int		rn_weight;		/* for load balancing */
	unsigned int		rn_displayed;		/* for delete at db reload */
	unsigned int		rn_recheck_ticks;
	struct rtpp_node	*rn_next;
};


struct rtpp_set {
	unsigned int 		id_set;
	unsigned int		weight_sum;
	unsigned int		rtpp_node_count;
	int 			set_disabled;
	unsigned int		set_recheck_ticks;
	struct rtpp_node	*rn_first;
	struct rtpp_node	*rn_last;
	struct rtpp_set	 	*rset_next;
	gen_lock_t		*rset_lock;
};


struct rtpp_set_head {
	struct rtpp_set		*rset_first;
	struct rtpp_set		*rset_last;
	gen_lock_t		*rset_head_lock;
};


struct rtpp_node *get_rtpp_node(struct rtpp_set *rtpp_list, str *url);
struct rtpp_set *get_rtpp_set(unsigned int set_id);
int add_rtpengine_socks(struct rtpp_set *rtpp_list, char *rtpengine,
		unsigned int weight, int disabled, unsigned int ticks, int isDB);

int rtpengine_delete_node(struct rtpp_node *rtpp_node);
int rtpengine_delete_node_set(struct rtpp_set *rtpp_list);
int rtpengine_delete_node_all();


int init_rtpengine_db(void);

extern str rtpp_db_url;
extern str rtpp_table_name;
extern str rtpp_setid_col;
extern str rtpp_url_col;
extern str rtpp_weight_col;
extern str rtpp_disabled_col;

enum hash_algo_t { RTP_HASH_CALLID, RTP_HASH_SHA1_CALLID, RTP_HASH_CRC32_CALLID };

#endif
