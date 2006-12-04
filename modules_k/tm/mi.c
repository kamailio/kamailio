/*
 * $Id$
 *
 * Header file for TM MI functions
 *
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2006-12-04  created (bogdan)
 */

#include <stdlib.h>
#include "mi.h"
#include "h_table.h"
#include "t_lookup.h"
#include "t_reply.h"


struct mi_root*  mi_tm_uac_dlg(struct mi_root* cmd_tree, void* param)
{
	return 0;
}


struct mi_root* mi_tm_cancel(struct mi_root* cmd_tree, void* param)
{
	return 0;
}


struct mi_root* mi_tm_hash(struct mi_root* cmd_tree, void* param)
{
	struct mi_root* rpl_tree= NULL;
	struct mi_node* rpl;
	struct mi_node* node;
	struct mi_attr* attr;
	struct s_table* tm_t;
	char *p;
	int i;
	int len;

	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	if (rpl_tree==0)
		return 0;
	rpl = &rpl_tree->node;
	tm_t = get_tm_table();

	for (i=0; i<TM_TABLE_ENTRIES; i++) {
		p = int2str((unsigned long)i, &len );
		node = add_mi_node_child(rpl, MI_DUP_VALUE , 0, 0, p, len);
		if(node == NULL)
			goto error;

		p = int2str((unsigned long)tm_t->entrys[i].cur_entries, &len );
		attr = add_mi_attr(node, MI_DUP_VALUE, "Current", 7, p, len );
		if(attr == NULL)
			goto error;

		p = int2str((unsigned long)tm_t->entrys[i].acc_entries, &len );
		attr = add_mi_attr(node, MI_DUP_VALUE, "Total", 5, p, len );
		if(attr == NULL)
			goto error;
	}

	return rpl_tree;
error:
	free_mi_tree(rpl_tree);
	return init_mi_tree( 500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
}


/*
  Syntax of "t_reply" :
  code
  reason
  trans_id
  to_tag
  new headers
  [Body]
*/
struct mi_root* mi_tm_reply(struct mi_root* cmd_tree, void* param)
{
	struct mi_node* node;
	unsigned int hash_index;
	unsigned int hash_label;
	unsigned int rpl_code;
	struct cell *trans;
	str *reason;
	str *totag;
	str *new_hdrs;
	str *body;
	str tmp;
	char *p;
	int n;

	for( n=0,node = cmd_tree->node.kids; n<6 && node ; n++,node=node->next );
	if ( !(n==5 || n==6) || node!=0)
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	/* get all info from the command */

	/* reply code (param 1) */
	node = cmd_tree->node.kids;
	if (str2int( &node->value, &rpl_code)!=0 || rpl_code>=700)
		return init_mi_tree( 400, "Invalid reply code", 18);

	/* reason text (param 2) */
	node = node->next;
	reason = &node->value;

	/* trans_id (param 3) */
	node = node->next;
	tmp = node->value;
	p = memchr( tmp.s, ':', tmp.len);
	if ( p==NULL)
		return init_mi_tree( 400, "Invalid trans_id", 16);

	tmp.len = p-tmp.s;
	if( str2int( &tmp, &hash_index)!=0 )
		return init_mi_tree( 400, "Invalid index in trans_id", 25);

	tmp.s = p+1;
	tmp.len = (node->value.s+node->value.len) - tmp.s;
	if( str2int( &tmp, &hash_label)!=0 )
		return init_mi_tree( 400, "Invalid label in trans_id", 25);

	if( t_lookup_ident( &trans, hash_index, hash_label)<0 )
		return init_mi_tree( 404, "Transaction not found", 21);

	/* to_tag (param 4) */
	node = node->next;
	totag = &node->value;

	/* new headers (param 5) */
	node = node->next;
	if (node->value.len==1 && (node->value.s[0]=='.' || node->value.s[0]==' '))
		new_hdrs = 0;
	else 
		new_hdrs = &node->value;

	/* body (param 5 - optional) */
	node = node->next;
	if (node)
		body = &node->value;
	else
		body = 0;

	/* it's refcounted now, t_reply_with body unrefs for me -- I can 
	 * continue but may not use T anymore  */
	n = t_reply_with_body( trans, rpl_code, reason, body, new_hdrs, totag);

	if (n<0)
		return init_mi_tree( 500, "Reply failed", 12);

	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
}

