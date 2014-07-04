/*
 * $Id$
 *
 * Copyright (C) 2005-2009 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * History:
 * ---------
 *  2005-02-20  first version (cristian)
 *  2005-02-27  ported to 0.9.0 (bogdan)
 */



#ifndef prefix_tree_h
#define prefix_tree_h

#include "../../str.h"
#include "../../ip_addr.h"
#include "dr_time.h"

#define PTREE_CHILDREN 10
#define IS_DECIMAL_DIGIT(d) \
	(((d)>='0') && ((d)<= '9'))

extern int tree_size;

#define INIT_PTREE_NODE(p, n) \
do {\
	(n) = (ptree_t*)shm_malloc(sizeof(ptree_t));\
	if(NULL == (n))\
		goto err_exit;\
	tree_size+=sizeof(ptree_t);\
	memset((n), 0, sizeof(ptree_t));\
	(n)->bp=(p);\
}while(0);


/* list of PSTN gw */
typedef struct pgw_addr_ {
	struct ip_addr ip;
	unsigned short port;
	int type;
	int strip;
	struct pgw_addr_ *next;
}pgw_addr_t;

/* list of PSTN gw */
typedef struct pgw_ {
	/* id matching the one in db */
	long id;
	str pri;
	int strip;
	str ip;
	int type;
	str attrs;
	struct pgw_ *next;
}pgw_t;

/**/
typedef struct pgw_list_ {
	pgw_t *pgw;
	int    grpid;
}pgw_list_t;

/* element containing routing information */
typedef struct rt_info_ {
	unsigned int priority;
	tmrec_t *time_rec;
	/* array of pointers into the PSTN gw list */
	pgw_list_t *pgwl;
	/* length of the PSTN gw array */
	unsigned short pgwa_len;
	/* how many list link this element */
	unsigned short ref_cnt;
	/* script route to be executed */
	int route_idx;
} rt_info_t;

typedef struct rt_info_wrp_ {
	rt_info_t     *rtl;
	struct rt_info_wrp_  *next;
} rt_info_wrp_t;

typedef struct rg_entry_ {
	unsigned int rgid;
	rt_info_wrp_t *rtlw;
} rg_entry_t;

typedef struct ptree_node_ {
	unsigned int rg_len;
	unsigned int rg_pos;
	rg_entry_t *rg;
	struct ptree_ *next;
} ptree_node_t;

typedef struct ptree_ {
	/* backpointer */
	struct ptree_ *bp;
	ptree_node_t ptnode[PTREE_CHILDREN];
} ptree_t;

void 
print_interim(
		int,
		int,
		ptree_t*
		);

int
del_tree(
	ptree_t *
	);

int
add_prefix(
	ptree_t*,
	/* prefix */
	str*,
	rt_info_t *,
	unsigned int
	);

rt_info_t*
get_prefix(
	ptree_t *ptree,
	str* prefix,
	unsigned int rgid
	);

int
add_rt_info(
	ptree_node_t*, 
	rt_info_t*,
	unsigned int
	);

pgw_t*
get_pgw(
	pgw_t*,
	long
	);

void
del_rt_list(
	rt_info_wrp_t *rl
	);

void print_rt(
	rt_info_t*
	);

void
free_rt_info(
	rt_info_t*
	);

rt_info_t*
check_rt(
	ptree_node_t *ptn,
	unsigned int rgid
	);

#endif
