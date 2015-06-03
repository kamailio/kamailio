/**
 * $Id$
 *
 * Copyright (C) 2010 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
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

		       
#ifndef _MTREE_H_
#define _MTREE_H_

#include "../../str.h"
#include "../../parser/msg_parser.h"
#include "../../lib/kmi/mi.h"
#include "../../rpc.h"

#define MT_TREE_SVAL	0	
#define MT_TREE_DW	1
#define MT_TREE_IVAL	2

typedef union {
    int n;
    str s;
} is_t;

typedef struct _mt_dw
{
	unsigned int dstid;
	unsigned int weight;
	struct _mt_dw *next;
} mt_dw_t;

typedef struct _mt_is
{
    is_t tvalue;
    struct _mt_is *next;
} mt_is_t;

typedef struct _mt_node
{
    mt_is_t *tvalues;
    void *data;
    struct _mt_node *child;
} mt_node_t;

#define MT_MAX_DEPTH	64

#define MT_NODE_SIZE	mt_char_list.len

typedef struct _m_tree
{
	str tname;
	str dbtable;
	int type;
	int multi;
	unsigned int nrnodes;
	unsigned int nritems;
	unsigned int memsize;
	unsigned int reload_count;
	unsigned int reload_time;
	mt_node_t *head;
	struct _m_tree *next;
} m_tree_t;


/* prefix tree operations */
int mt_add_to_tree(m_tree_t *pt, str *tprefix, str *svalue);

m_tree_t* mt_get_tree(str *tname);
m_tree_t* mt_get_first_tree();

is_t* mt_get_tvalue(m_tree_t *pt, str *tomatch, int *len);
int mt_match_prefix(struct sip_msg *msg, m_tree_t *pt,
		    str *tomatch, int mode);

m_tree_t* mt_init_tree(str* tname, str* dbtable, int type, int multi);
void mt_free_tree(m_tree_t *pt);
int mt_print_tree(m_tree_t *pt);
void mt_free_node(mt_node_t *pn, int type);

void mt_char_table_init(void);
int mt_node_set_payload(mt_node_t *node, int type);
int mt_node_unset_payload(mt_node_t *node, int type);

int mt_table_spec(char* val);
void mt_destroy_trees(void);
int mt_defined_trees(void);

m_tree_t *mt_swap_list_head(m_tree_t *ntree);
int mt_init_list_head(void);
m_tree_t *mt_add_tree(m_tree_t **dpt, str *tname, str *dbtable,
		      int type, int multi);

int mt_mi_match_prefix(struct mi_node *rpl, m_tree_t *pt,
		    str *tomatch, int mode);
int mt_rpc_match_prefix(rpc_t* rpc, void* ctx, m_tree_t *pt,
		    str *tomatch, int mode);
#endif

