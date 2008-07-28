/*
 * $Id$
 *
 * Header file for USRLOC MI functions
 *
 * Copyright (C) 2006 Voice Sistem SRL
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * ---------
 *
 * 2006-12-01  created (bogdan)
 */

/*! \file
 *  \brief USRLOC - Usrloc MI functions
 *  \ingroup usrloc
 *
 * - Module: \ref usrloc
 */

#include <string.h>
#include <stdio.h>
#include "../../mi/mi.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../qvalue.h"
#include "../../ip_addr.h"
#include "ul_mi.h"
#include "dlist.h"
#include "udomain.h"
#include "utime.h"
#include "ul_mod.h"



#define MI_UL_CSEQ 1
static str mi_ul_cid = str_init("dfjrewr12386fd6-343@openser.mi");
static str mi_ul_ua  = str_init("OpenSER MI Server");




/************************ helper functions ****************************/

static inline udomain_t* mi_find_domain(str* table)
{
	dlist_t* dom;

	for( dom=root ; dom ; dom=dom->next ) {
		if ((dom->name.len == table->len) &&
		!memcmp(dom->name.s, table->s, table->len))
			return dom->d;
	}
	return 0;
}

static inline int mi_fix_aor(str *aor)
{
	char *p;

	p = memchr( aor->s, '@', aor->len);
	if (use_domain) {
		if (p==NULL)
			return -1;
	} else {
		if (p)
			aor->len = p - aor->s;
	}
	strlower(aor);

	return 0;
}



static inline int mi_add_aor_node(struct mi_node *parent, urecord_t* r, time_t t, int short_dump)
{
	struct mi_node *anode;
	struct mi_node *cnode;
	struct mi_node *node;
	struct mi_attr *attr;
	ucontact_t* c;
	char *p;
	int len;

	anode = add_mi_node_child( parent, MI_DUP_VALUE, "AOR", 3,
			r->aor.s, r->aor.len);
	if (anode==0)
		return -1;

	if (short_dump)
		return 0;

	for( c=r->contacts ; c ; c=c->next) {
		/* contact */
		cnode = add_mi_node_child( anode, MI_DUP_VALUE, "Contact", 7,
			c->c.s, c->c.len);
		if (cnode==0)
			return -1;

		/* expires */
		if (c->expires == 0) {
			node = add_mi_node_child( cnode, 0, "Expires", 7, "permanent", 9);
		} else if (c->expires == UL_EXPIRED_TIME) {
			node = add_mi_node_child( cnode, 0, "Expires", 7, "deleted", 7);
		} else if (t > c->expires) {
			node = add_mi_node_child( cnode, 0, "Expires", 7, "expired", 7);
		} else {
			p = int2str((unsigned long)(c->expires - t), &len);
			node = add_mi_node_child( cnode, MI_DUP_VALUE, "Expires", 7,p,len);
		}
		if (node==0)
			return -1;

		/* q */
		p = q2str(c->q, (unsigned int*)&len);
		attr = add_mi_attr( cnode, MI_DUP_VALUE, "Q", 1, p, len);
		if (attr==0)
			return -1;

		/* callid */
		node = add_mi_node_child( cnode, MI_DUP_VALUE, "Callid", 6,
			c->callid.s, c->callid.len);
		if (node==0)
			return -1;

		/* cseq */
		p = int2str((unsigned long)c->cseq, &len);
		node = add_mi_node_child( cnode, MI_DUP_VALUE, "Cseq", 4, p, len);
		if (node==0)
			return -1;

		/* User-Agent */
		if (c->user_agent.len) {
			node = add_mi_node_child( cnode, MI_DUP_VALUE, "User-agent", 10,
				c->user_agent.s, c->user_agent.len);
			if (node==0)
				return -1;
		}

		/* received */
		if (c->received.len) {
			node = add_mi_node_child( cnode, MI_DUP_VALUE, "Received", 8,
				c->received.s, c->received.len);
			if (node==0)
				return -1;
		}

		/* path */
		if (c->path.len) {
			node = add_mi_node_child( cnode, MI_DUP_VALUE, "Path", 4,
				c->path.s, c->path.len);
			if (node==0)
				return -1;
		}

		/* state */
		if (c->state == CS_NEW) {
			node = add_mi_node_child( cnode, 0, "State", 5, "CS_NEW", 6);
		} else if (c->state == CS_SYNC) {
			node = add_mi_node_child( cnode, 0, "State", 5, "CS_SYNC", 7);
		} else if (c->state== CS_DIRTY) {
			node = add_mi_node_child( cnode, 0, "State", 5, "CS_DIRTY", 8);
		} else {
			node = add_mi_node_child( cnode, 0, "State", 5, "CS_UNKNOWN", 10);
		}
		if (node==0)
			return -1;

		/* flags */
		p = int2str((unsigned long)c->flags, &len);
		node = add_mi_node_child( cnode, MI_DUP_VALUE, "Flags", 5, p, len);
		if (node==0)
			return -1;

		/* cflags */
		p = int2str((unsigned long)c->cflags, &len);
		node = add_mi_node_child( cnode, MI_DUP_VALUE, "Cflags", 5, p, len);
		if (node==0)
			return -1;

		/* socket */
		if (c->sock) {
			node = add_mi_node_child( cnode, 0, "Socket", 6,
				c->sock->sock_str.s, c->sock->sock_str.len);
			if (node==0)
				return -1;
		}

		/* methods */
		p = int2str((unsigned long)c->methods, &len);
		node = add_mi_node_child( cnode, MI_DUP_VALUE, "Methods", 7, p, len);
		if (node==0)
			return -1;

	} /* for */

	return 0;
}




/*************************** MI functions *****************************/

/*! \brief
 * Expects 2 nodes: the table name and the AOR
 */
struct mi_root* mi_usrloc_rm_aor(struct mi_root *cmd, void *param)
{
	struct mi_node *node;
	udomain_t *dom;
	str *aor;

	node = cmd->node.kids;
	if (node==NULL || node->next==NULL || node->next->next!=NULL)
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	/* look for table */
	dom = mi_find_domain( &node->value );
	if (dom==NULL)
		return init_mi_tree( 404, "Table not found", 15);

	/* process the aor */
	aor = &node->next->value;
	if ( mi_fix_aor(aor)!=0 )
		return init_mi_tree( 400, "Domain missing in AOR", 21);

	lock_udomain( dom, aor);
	if (delete_urecord( dom, aor, 0) < 0) {
		unlock_udomain( dom, aor);
		return init_mi_tree( 500, "Failed to delete AOR", 20);
	}

	unlock_udomain( dom, aor);
	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
}


/*! \brief
 * Expects 3 nodes: the table name, the AOR and contact
 */
struct mi_root* mi_usrloc_rm_contact(struct mi_root *cmd, void *param)
{
	struct mi_node *node;
	udomain_t *dom;
	urecord_t *rec;
	ucontact_t* con;
	str *aor;
	str *contact;
	int ret;

	node = cmd->node.kids;
	if (node==NULL || node->next==NULL || node->next->next==NULL ||
	node->next->next->next!=NULL)
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	/* look for table */
	dom = mi_find_domain( &node->value );
	if (dom==NULL)
		return init_mi_tree( 404, "Table not found", 15);

	/* process the aor */
	aor = &node->next->value;
	if ( mi_fix_aor(aor)!=0 )
		return init_mi_tree( 400, "Domain missing in AOR", 21);

	lock_udomain( dom, aor);

	ret = get_urecord( dom, aor, &rec);
	if (ret == 1) {
		unlock_udomain( dom, aor);
		return init_mi_tree( 404, "AOR not found", 13);
	}

	contact = &node->next->next->value;
	ret = get_ucontact( rec, contact, &mi_ul_cid, MI_UL_CSEQ+1, &con);
	if (ret < 0) {
		unlock_udomain( dom, aor);
		return 0;
	}
	if (ret > 0) {
		unlock_udomain( dom, aor);
		return init_mi_tree( 404, "Contact not found", 17);
	}

	if (delete_ucontact(rec, con) < 0) {
		unlock_udomain( dom, aor);
		return 0;
	}

	release_urecord(rec);
	unlock_udomain( dom, aor);
	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
}


struct mi_root* mi_usrloc_dump(struct mi_root *cmd, void *param)
{
	struct mi_root *rpl_tree;
	struct mi_node *rpl;
	struct mi_node *node;
	struct mi_attr *attr;
	struct urecord* r;
	dlist_t* dl;
	udomain_t* dom;
	time_t t;
	char *p;
	int max;
	int len;
	int n;
	int i;
	int short_dump;

	node = cmd->node.kids;
	if (node && node->next)
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	if (node && node->value.len==5 && !strncasecmp(node->value.s, "brief", 5)){
		/* short version */
		short_dump = 1;
	} else {
		short_dump = 0;
	}

	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	if (rpl_tree==NULL)
		return 0;
	rpl = &rpl_tree->node;
	t = time(0);

	for( dl=root ; dl ; dl=dl->next ) {
		/* add a domain node */
		node = add_mi_node_child( rpl, 0, "Domain", 6,
					dl->name.s, dl->name.len);
		if (node==0)
			goto error;

		dom = dl->d;
		/* add some attributes to the domain node */
		p= int2str((unsigned long)dom->size, &len);
		attr = add_mi_attr( node, MI_DUP_VALUE, "table", 5, p, len);
		if (attr==0)
			goto error;

		/* add the entries per hash */
		for(i=0,n=0,max=0; i<dom->size; i++) {
			lock_ulslot( dom, i);

			n += dom->table[i].n;
			if(max<dom->table[i].n)
				max= dom->table[i].n;
			for( r = dom->table[i].first ; r ; r=r->next ) {
				/* add entry */
				if (mi_add_aor_node( node, r, t, short_dump)!=0) {
					unlock_ulslot( dom, i);
					goto error;
				}
			}

			unlock_ulslot( dom, i);
		}

		/* add more attributes to the domain node */
		p= int2str((unsigned long)n, &len);
		attr = add_mi_attr( node, MI_DUP_VALUE, "records", 7, p, len);
		if (attr==0)
			goto error;

		p= int2str((unsigned long)max, &len);
		attr = add_mi_attr( node, MI_DUP_VALUE, "max_slot", 8, p, len);
		if (attr==0)
			goto error;

	}

	return rpl_tree;
error:
	free_mi_tree(rpl_tree);
	return 0;
}


struct mi_root* mi_usrloc_flush(struct mi_root *cmd, void *param)
{
	struct mi_root *rpl_tree;

	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	if (rpl_tree==NULL)
		return 0;

	synchronize_all_udomains();
	return rpl_tree;
}


/*! \brief
 * Expects 7 nodes: 
 *        table name,
 *        AOR
 *        contact
 *        expires
 *        Q
 *        useless - backward compat.
 *        flags
 *        cflags
 *        methods
 */
struct mi_root* mi_usrloc_add(struct mi_root *cmd, void *param)
{
	ucontact_info_t ci;
	urecord_t* r;
	ucontact_t* c;
	struct mi_node *node;
	udomain_t *dom;
	str *aor;
	str *contact;
	unsigned int ui_val;
	int n;

	for( n=0,node = cmd->node.kids; n<9 && node ; n++,node=node->next );
	if (n!=9 || node!=0)
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	node = cmd->node.kids;

	/* look for table (param 1) */
	dom = mi_find_domain( &node->value );
	if (dom==NULL)
		return init_mi_tree( 404, "Table not found", 15);

	/* process the aor (param 2) */
	node = node->next;
	aor = &node->value;
	if ( mi_fix_aor(aor)!=0 )
		return init_mi_tree( 400, "Domain missing in AOR", 21);

	/* contact (param 3) */
	node = node->next;
	contact = &node->value;

	memset( &ci, 0, sizeof(ucontact_info_t));

	/* expire (param 4) */
	node = node->next;
	if (str2int( &node->value, &ui_val) < 0)
		goto bad_syntax;
	ci.expires = ui_val;

	/* q value (param 5) */
	node = node->next;
	if (str2q( &ci.q, node->value.s, node->value.len) < 0)
		goto bad_syntax;

	/* unused value (param 6) FIXME */
	node = node->next;

	/* flags value (param 7) */
	node = node->next;
	if (str2int( &node->value, (unsigned int*)&ci.flags) < 0)
		goto bad_syntax;

	/* branch flags value (param 8) */
	node = node->next;
	if (str2int( &node->value, (unsigned int*)&ci.cflags) < 0)
		goto bad_syntax;

	/* methods value (param 9) */
	node = node->next;
	if (str2int( &node->value, (unsigned int*)&ci.methods) < 0)
		goto bad_syntax;

	lock_udomain( dom, aor);

	n = get_urecord( dom, aor, &r);
	if ( n==1) {
		if (insert_urecord( dom, aor, &r) < 0)
			goto lock_error;
		c = 0;
	} else {
		if (get_ucontact( r, contact, &mi_ul_cid, MI_UL_CSEQ+1, &c) < 0)
			goto lock_error;
	}

	get_act_time();

	ci.callid = &mi_ul_cid;
	ci.user_agent = &mi_ul_ua;
	ci.cseq = MI_UL_CSEQ;
	/* 0 expires means permanent contact */
	if (ci.expires!=0)
		ci.expires += act_time;

	if (c) {
		if (update_ucontact( r, c, &ci) < 0)
			goto release_error;
	} else {
		if ( insert_ucontact( r, contact, &ci, &c) < 0 )
			goto release_error;
	}

	release_urecord(r);

	unlock_udomain( dom, aor);

	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
bad_syntax:
	return init_mi_tree( 400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);
release_error:
	release_urecord(r);
lock_error:
	unlock_udomain( dom, aor);
	return init_mi_tree( 500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
}


/*! \brief
 * Expects 2 nodes: the table name and the AOR
 */
struct mi_root* mi_usrloc_show_contact(struct mi_root *cmd, void *param)
{
	struct mi_root *rpl_tree;
	struct mi_node *rpl;
	struct mi_node *node;
	udomain_t *dom;
	urecord_t *rec;
	ucontact_t* con;
	str *aor;
	int ret;

	node = cmd->node.kids;
	if (node==NULL || node->next==NULL || node->next->next!=NULL)
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	/* look for table */
	dom = mi_find_domain( &node->value );
	if (dom==NULL)
		return init_mi_tree( 404, "Table not found", 15);

	/* process the aor */
	aor = &node->next->value;
	if ( mi_fix_aor(aor)!=0 )
		return init_mi_tree( 400, "Domain missing in AOR", 21);

	lock_udomain( dom, aor);

	ret = get_urecord( dom, aor, &rec);
	if (ret == 1) {
		unlock_udomain( dom, aor);
		return init_mi_tree( 404, "AOR not found", 13);
	}

	get_act_time();
	rpl_tree = 0;
	rpl = 0;

	for( con=rec->contacts ; con ; con=con->next) {
		if (VALID_CONTACT( con, act_time)) {
			if (rpl_tree==0) {
				rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
				if (rpl_tree==0)
					goto error;
				rpl = &rpl_tree->node;
			}

			node = addf_mi_node_child( rpl, 0, "Contact", 7,
				"<%.*s>;q=%s;expires=%d;flags=0x%X;cflags=0x%X;socket=<%.*s>;"
				"methods=0x%X"
				"%s%.*s%s" /*received*/
				"%s%.*s%s" /*user-agent*/
				"%s%.*s%s", /*path*/
				con->c.len, ZSW(con->c.s),
				q2str(con->q, 0), (int)(con->expires - act_time),
				con->flags, con->cflags,
				con->sock?con->sock->sock_str.len:3,
					con->sock?con->sock->sock_str.s:"NULL",
				con->methods,
				con->received.len?";received=<":"",con->received.len,
					ZSW(con->received.s), con->received.len?">":"",
				con->user_agent.len?";user_agent=<":"",con->user_agent.len,
					ZSW(con->user_agent.s), con->user_agent.len?">":"",
				con->path.len?";path=<":"", con->path.len,
					ZSW(con->path.s), con->path.len?">":""
				);
			if (node==0)
				goto error;
		}
	}

	unlock_udomain( dom, aor);

	if (rpl_tree==0)
		return init_mi_tree( 404 , "AOR has no contacts", 18);

	return rpl_tree;
error:
	if (rpl_tree)
		free_mi_tree( rpl_tree );
	unlock_udomain( dom, aor);
	return 0;
}


