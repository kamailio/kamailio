/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/*! \file
 *  \brief USRLOC - Usrloc MI functions
 *  \ingroup usrloc
 *
 * - Module: \ref usrloc
 */

#include <string.h>
#include <stdio.h>
#include "../../lib/kmi/mi.h"
#include "../../lib/srutils/sruid.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../qvalue.h"
#include "../../ip_addr.h"
#include "../../dset.h"
#include "ul_mi.h"
#include "dlist.h"
#include "udomain.h"
#include "utime.h"
#include "ul_mod.h"
#include "usrloc.h"

/*! CSEQ nr used */
static int MI_UL_CSEQ = 0;
/*! call-id used for ul_add and ul_rm_contact */
static str mi_ul_cid = {0,0}; /* str_init("dfjrewr12386fd6-343@kamailio.mi");*/
/*! user agent used for ul_add */
static str mi_ul_ua  = str_init("SIP Router MI Server");
/*! path used for ul_add and ul_rm_contact */
static str mi_ul_path = str_init("dummypath");

extern sruid_t _ul_sruid;

static char mi_ul_cid_buf[32];

/************************ helper functions ****************************/

static void set_mi_ul_cid(void)
{
	int i;
    char charset[] = "0123456789"
                     "abcdefghijklmnopqrstuvwxyz"
                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

	if(mi_ul_cid.s!=NULL) return;

	for(i=0; i<19; i++) {
        mi_ul_cid_buf[i] = charset[rand()%(sizeof(charset) - 1)];
    }
    memcpy(mi_ul_cid_buf+i, "@kamailio.mi", sizeof("@kamailio.mi"));
    mi_ul_cid.s = mi_ul_cid_buf;
	mi_ul_cid.len = strlen(mi_ul_cid.s);
}

/*!
 * \brief Search a domain in the global domain list
 * \param table domain (table) name
 * \return pointer to domain if found, 0 if not found
 */
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


/*!
 * \brief Convert address of record
 *
 * Convert an address of record string to lower case, and truncate
 * it when use_domain is not set.
 * \param aor address of record
 * \return 0 on success, -1 on error
 */
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
	if(!get_aor_case_sensitive())
		strlower(aor);

	return 0;
}


/*!
 * \brief Add a node for a address of record
 * \param parent parent node
 * \param r printed record
 * \param t actual time
 * \param short_dump 0 means that all informations will be included, 1 that only the AOR is printed
 * \return 0 on success, -1 on failure
 */
static inline int mi_add_aor_node(struct mi_node *parent, urecord_t* r, time_t t, int short_dump)
{
	struct mi_node *anode, *cnode, *node;
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

#if 0
	/* aor hash */
	p = int2str((unsigned long)r->aorhash, &len);
	node = add_mi_node_child( anode, MI_DUP_VALUE, "HashID", 6, p, len);
	if (node==0)
		return -1;
#endif

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

		/* ruid */
		if (c->ruid.len) {
			node = add_mi_node_child( cnode, MI_DUP_VALUE, "Ruid", 4,
				c->ruid.s, c->ruid.len);
			if (node==0)
				return -1;
		}

		/* instance */
		if (c->instance.len) {
			node = add_mi_node_child( cnode, MI_DUP_VALUE, "Instance", 8,
				c->instance.s, c->instance.len);
			if (node==0)
				return -1;
		}

		/* reg-id */
		p = int2str((unsigned long)c->reg_id, &len);
		node = add_mi_node_child( cnode, MI_DUP_VALUE, "Reg-Id", 6, p, len);
		if (node==0)
			return -1;

		/* last keepalive */
		p = int2str((unsigned long)c->last_keepalive, &len);
		node = add_mi_node_child( cnode, MI_DUP_VALUE, "Last-Keepalive",
					14, p, len);
		if (node==0)
			return -1;

		/* last modified */
		p = int2str((unsigned long)c->last_modified, &len);
		node = add_mi_node_child( cnode, MI_DUP_VALUE, "Last-Modified",
					13, p, len);
		if (node==0)
			return -1;

	} /* for */

	return 0;
}


/*************************** MI functions *****************************/

/*!
 * \brief Delete a address of record including its contacts
 * \param cmd mi_root containing the parameter
 * \param param not used
 * \note expects 2 nodes: the table name and the AOR
 * \return mi_root with the result
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


/*!
 * \brief Delete a contact from an AOR record
 * \param cmd mi_root containing the parameter
 * \param param not used
 * \note expects 3 nodes: the table name, the AOR and contact
 * \return mi_root with the result or 0 on failure
 */
struct mi_root* mi_usrloc_rm_contact(struct mi_root *cmd, void *param)
{
	struct mi_node *node;
	udomain_t *dom;
	urecord_t *rec;
	ucontact_t* con;
	str *aor, *contact;
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
	set_mi_ul_cid();
	ret = get_ucontact( rec, contact, &mi_ul_cid, &mi_ul_path, MI_UL_CSEQ+1, &con);
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


/*!
 * \brief Dump the content of the usrloc
 * \param cmd mi_root containing the parameter
 * \param param not used
 * \return mi_root with the result or 0 on failure
 */
struct mi_root* mi_usrloc_dump(struct mi_root *cmd, void *param)
{
	struct mi_root *rpl_tree;
	struct mi_node *rpl, *node;
	struct mi_attr *attr;
	struct urecord* r;
	dlist_t* dl;
	udomain_t* dom;
	time_t t;
	char *p;
	int max, len, n, i, short_dump;

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


/*!
 * \brief Flush the usrloc memory cache to DB
 * \param cmd mi_root containing the parameter
 * \param param not used
 * \return mi_root with the result or 0 on failure
 */
struct mi_root* mi_usrloc_flush(struct mi_root *cmd, void *param)
{
	struct mi_root *rpl_tree;

	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	if (rpl_tree==NULL)
		return 0;

	synchronize_all_udomains(0, 1);
	return rpl_tree;
}


/*!
 * \brief Add a new contact for an address of record
 * \param cmd mi_root containing the parameter
 * \param param not used
 * \note Expects 9 nodes: table name, AOR, contact, expires, Q,
 * path, flags, cflags, methods
 * \return mi_root with the result
 */
struct mi_root* mi_usrloc_add(struct mi_root *cmd, void *param)
{
	ucontact_info_t ci;
	urecord_t* r;
	ucontact_t* c;
	struct mi_node *node;
	udomain_t *dom;
	str *aor, *contact;
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

	/* path value (param 6) */
	node = node->next;
	if(strncmp(node->value.s, "0", 1) != 0 && node->value.len > 1)
		ci.path = &node->value;

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

	if(sruid_next(&_ul_sruid)<0)
		goto error;
	ci.ruid = _ul_sruid.uid;

	lock_udomain( dom, aor);

	set_mi_ul_cid();
	n = get_urecord( dom, aor, &r);
	if ( n==1) {
		if (insert_urecord( dom, aor, &r) < 0)
			goto lock_error;
		c = 0;
	} else {
		if (get_ucontact( r, contact, &mi_ul_cid, &mi_ul_path, MI_UL_CSEQ+1, &c) < 0)
			goto lock_error;
	}

	get_act_time();

	ci.callid = &mi_ul_cid;
	ci.user_agent = &mi_ul_ua;
	ci.cseq = ++MI_UL_CSEQ;
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
error:
	return init_mi_tree( 500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
}


/*!
 * \brief Dumps the contacts of an AOR
 * \param cmd mi_root containing the parameter
 * \param param not used
 * \note expects 2 nodes: the table name and the AOR
 * \return mi_root with the result or 0 on failure
 */
struct mi_root* mi_usrloc_show_contact(struct mi_root *cmd, void *param)
{
	struct mi_root *rpl_tree;
	struct mi_node *rpl, *node;
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
				"<%.*s>;q=%s;expires=%d;flags=0x%X;cflags=0x%X;state=%d;socket=<%.*s>;"
				"methods=0x%X"
				"%s%.*s%s" /*received*/
				"%s%.*s%s" /*user-agent*/
				"%s%.*s%s" /*path*/
				"%s%.*s" /*instance*/
			        ";reg-id=%u",
				con->c.len, ZSW(con->c.s),
				q2str(con->q, 0), (int)(con->expires - act_time),
				con->flags, con->cflags, con->state,
				con->sock?con->sock->sock_str.len:3,
					con->sock?con->sock->sock_str.s:"NULL",
				con->methods,
				con->received.len?";received=<":"",con->received.len,
					ZSW(con->received.s), con->received.len?">":"",
				con->user_agent.len?";user_agent=<":"",con->user_agent.len,
					ZSW(con->user_agent.s), con->user_agent.len?">":"",
				con->path.len?";path=<":"", con->path.len,
					ZSW(con->path.s), con->path.len?">":"",
				con->instance.len?";+sip.instance=":"", con->instance.len,
				        ZSW(con->instance.s),
				con->reg_id
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
