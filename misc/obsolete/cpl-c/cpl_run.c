/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * -------
 * 2003-01-23 : created (bogdan)
 * 2003-09-11 : build_lump_rpl() merged into add_lump_rpl() (bogdan)
 * 2004-06-14 : all global variables merged into cpl_env and cpl_fct;
 *              append_branches param added to lookup node (bogdan)
 * 2004-06-14 : flag CPL_IS_STATEFUL is set now immediately after the 
 *              transaction is created (bogdan)
*/

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../str.h"
#include "../../dprint.h"
#include "../../parser/msg_parser.h"
#include "../../data_lump_rpl.h"
#include "../../modules/tm/tm_load.h"
#include "../usrloc/usrloc.h"
#include "CPL_tree.h"
#include "loc_set.h"
#include "cpl_utils.h"
#include "cpl_nonsig.h"
#include "cpl_sig.h"
#include "cpl_env.h"
#include "cpl_run.h"


#define EO_SCRIPT            ((char*)0xffffffff)
#define DEFAULT_ACTION       ((char*)0xfffffffe)
#define CPL_SCRIPT_ERROR     ((char*)0xfffffffd)
#define CPL_RUNTIME_ERROR    ((char*)0xfffffffc)
#define CPL_TO_CONTINUE      ((char*)0xfffffffb)

#define HDR_NOT_FOUND        ((char*)0xffffffff)
#define UNDEF_CHAR           (0xff)



#define check_overflow_by_ptr(_ptr_,_intr_,_error_) \
	do {\
		if ( (char*)(_ptr_)>(_intr_)->script.len+(_intr_)->script.s ) {\
			LOG(L_ERR,"ERROR:cpl_c: overflow detected ip=%p ptr=%p in "\
			"func. %s, line %d\n",(_intr_)->ip,_ptr_,__FILE__,__LINE__);\
			goto _error_; \
		} \
	}while(0)

#define check_overflow_by_offset(_len_,_intr_,_error_) \
	do {\
		if ( (char*)((_intr_)->ip+(_len_)) > \
		(_intr_)->script.len+(_intr_)->script.s ) {\
			LOG(L_ERR,"ERROR:cpl_c: overflow detected ip=%p offset=%d in "\
			"func. %s, line %d\n",(_intr_)->ip,_len_,__FILE__,__LINE__);\
			goto _error_; \
		} \
	}while(0)

#define get_first_child(_node_) \
	((NR_OF_KIDS(_node_)==0)?DEFAULT_ACTION:(_node_)+KID_OFFSET(_node_,0))

#define get_basic_attr(_p_,_code_,_n_,_intr_,_error_) \
	do{\
		check_overflow_by_ptr( (_p_)+BASIC_ATTR_SIZE, _intr_, _error_);\
		_code_ = ntohs( *((unsigned short*)(_p_)) );\
		_n_ =  ntohs( *((unsigned short*)((_p_)+2)) );\
		(_p_) += 4;\
	}while(0)

#define get_str_attr(_p_,_s_,_len_,_intr_,_error_,_FIXUP_) \
	do{\
		if ( ((int)(_len_))-(_FIXUP_)<=0 ) {\
			LOG(L_ERR,"ERROR:cpl_c:%s:%d: attribute is an empty string\n",\
				__FILE__,__LINE__);\
			goto _error_; \
		} else {\
			check_overflow_by_ptr( (_p_)+(_len_), _intr_, _error_);\
			_s_ = _p_;\
			(_p_) += (_len_) + 1*(((_len_)&0x0001)==1);\
			(_len_) -= (_FIXUP_);\
		}\
	}while(0)



struct cpl_interpreter* new_cpl_interpreter( struct sip_msg *msg, str *script)
{
	struct cpl_interpreter *intr = 0;

	intr = (struct cpl_interpreter*)shm_malloc(sizeof(struct cpl_interpreter));
	if (!intr) {
		LOG(L_ERR,"ERROR:build_cpl_interpreter: no more free memory!\n");
		goto error;
	}
	memset( intr, 0, sizeof(struct cpl_interpreter));

	/* init the interpreter*/
	intr->script.s = script->s;
	intr->script.len = script->len;
	intr->recv_time = time(0);
	intr->ip = script->s;
	intr->msg = msg;

	/* check the beginning of the script */
	if ( NODE_TYPE(intr->ip)!=CPL_NODE ) {
		LOG(L_ERR,"ERROR:build_cpl_interpreter: first node is not CPL!!\n");
		goto error;
	}

	return intr;
error:
	return 0;
}



void free_cpl_interpreter(struct cpl_interpreter *intr)
{
	if (intr) {
		empty_location_set( &(intr->loc_set) );
		if (intr->script.s)
			shm_free( intr->script.s);
		if (intr->user.s)
			shm_free(intr->user.s);
		if (intr->flags&CPL_RURI_DUPLICATED)
			shm_free(intr->ruri);
		if (intr->flags&CPL_TO_DUPLICATED)
			shm_free(intr->to);
		if (intr->flags&CPL_FROM_DUPLICATED)
			shm_free(intr->from);
		if (intr->flags&CPL_SUBJECT_DUPLICATED)
			shm_free(intr->subject);
		if (intr->flags&CPL_ORGANIZATION_DUPLICATED)
			shm_free(intr->organization);
		if (intr->flags&CPL_USERAGENT_DUPLICATED)
			shm_free(intr->user_agent);
		if (intr->flags&CPL_ACCEPTLANG_DUPLICATED)
			shm_free(intr->accept_language);
		if (intr->flags&CPL_PRIORITY_DUPLICATED)
			shm_free(intr->priority);
		shm_free(intr);
	}
}



/* UPDATED + CHECKED
 */
static inline char *run_cpl_node( struct cpl_interpreter *intr )
{
	char *kid;
	unsigned char start;
	int i;

	start = (intr->flags&CPL_RUN_INCOMING)?INCOMING_NODE:OUTGOING_NODE;
	/* look for the starting node (incoming or outgoing) */
	for(i=0;i<NR_OF_KIDS(intr->ip);i++) {
		kid= intr->ip + KID_OFFSET(intr->ip,i);
		if ( NODE_TYPE(kid)==start ) {
			return get_first_child(kid);
		} else
		if (NODE_TYPE(kid)==SUBACTION_NODE ||
		NODE_TYPE(kid)==ANCILLARY_NODE ||
		NODE_TYPE(kid)==INCOMING_NODE  ||
		NODE_TYPE(kid)==OUTGOING_NODE ) {
			continue;
		} else {
			LOG(L_ERR,"ERROR:cpl_c:run_cpl_node: unknown child type (%d) "
				"for CPL node!!\n",NODE_TYPE(kid));
			return CPL_SCRIPT_ERROR;
		}
	}

	DBG("DEBUG:cpl_c:run_cpl_node: CPL node has no %d subnode -> default\n",
		start);
	return DEFAULT_ACTION;
}



/* UPDATED + CHECKED
 */
static inline char *run_lookup( struct cpl_interpreter *intr )
{
	unsigned short attr_name;
	unsigned short n;
	unsigned char  clear;
	char *p;
	char *kid;
	char *failure_kid = 0;
	char *success_kid = 0;
	char *notfound_kid = 0;
	int  i;
	time_t      tc;
	urecord_t*  r;
	ucontact_t* contact;

	clear = NO_VAL;

	/* check the params */
	for( i=NR_OF_ATTR(intr->ip),p=ATTR_PTR(intr->ip) ; i>0 ; i-- ) {
		get_basic_attr(p,attr_name,n,intr,script_error);
		switch (attr_name) {
			case CLEAR_ATTR:
				if (n!=YES_VAL && n!=NO_VAL)
					LOG(L_WARN,"WARNING:run_lookup: invalid value (%u) found"
						" for param. CLEAR in LOOKUP node -> using "
						"default (%u)!\n",n,clear);
				else
					clear = n;
				break;
			default:
				LOG(L_ERR,"ERROR:run_lookup: unknown attribute (%d) in "
					"LOOKUP node\n",attr_name);
				goto script_error;
		}
	}

	/* check the kids */
	for( i=0 ; i<NR_OF_KIDS(intr->ip) ; i++ ) {
		kid = intr->ip + KID_OFFSET(intr->ip,i);
		check_overflow_by_ptr( kid+SIMPLE_NODE_SIZE(kid), intr, script_error);
		switch ( NODE_TYPE(kid) ) {
			case SUCCESS_NODE :
				success_kid = kid;
				break;
			case NOTFOUND_NODE:
				notfound_kid = kid;
				break;
			case FAILURE_NODE:
				failure_kid = kid;
				break;
			default:
				LOG(L_ERR,"ERROR:run_lookup: unknown output node type"
					" (%d) for LOOKUP node\n",NODE_TYPE(kid));
				goto script_error;
		}
	}

	kid = failure_kid;

	if (cpl_env.lu_domain) {
		/* fetch user's contacts via usrloc */
		tc = time(0);
		cpl_fct.ulb.lock_udomain( cpl_env.lu_domain );
		i = cpl_fct.ulb.get_urecord( cpl_env.lu_domain, &intr->user, &r);
		if (i < 0) {
			/* failure */
			LOG(L_ERR, "ERROR:run_lookup: Error while querying usrloc\n");
			cpl_fct.ulb.unlock_udomain( cpl_env.lu_domain );
		} else if (i > 0) {
			/* not found */
			DBG("DBG:cpl-c:run_lookup: '%.*s' Not found in usrloc\n",
				intr->user.len, intr->user.s);
			cpl_fct.ulb.unlock_udomain( cpl_env.lu_domain );
			kid = notfound_kid;
		} else {
			contact = r->contacts;
			/* skip expired contacts */
			while ( contact && contact->expires <= tc)
				contact = contact->next;
			/* any contacts left? */
			if (contact) {
				/* clear loc set if requested */
				if (clear)
					empty_location_set( &(intr->loc_set) );
				/* start adding locations to set */
				do {
					DBG("DBG:cpl-c:run_lookup: adding <%.*s>q=%d\n",
						contact->c.len,contact->c.s,(int)(10*contact->q));
					if (add_location( &(intr->loc_set), &contact->c,
					(int)(10*contact->q),
					CPL_LOC_DUPL|((contact->flags & FL_NAT)*CPL_LOC_NATED)
					)==-1) {
						LOG(L_ERR,"ERROR:cpl-c:run_lookup: unable to add "
							"location to set :-(\n");
						cpl_fct.ulb.unlock_udomain( cpl_env.lu_domain );
						goto runtime_error;
					}
					contact = contact->next;
				}while( contact && cpl_env.lu_append_branches);
				/* set the flag for modifying the location set */
				intr->flags |= CPL_LOC_SET_MODIFIED;
				/* we found a valid contact */
				kid = success_kid;
			} else {
				/* no valid contact found */
				kid = notfound_kid;
			}
			cpl_fct.ulb.unlock_udomain( cpl_env.lu_domain );
		}

	}

	if (kid)
		return get_first_child(kid);
	return DEFAULT_ACTION;
runtime_error:
	return CPL_RUNTIME_ERROR;
script_error:
	return CPL_SCRIPT_ERROR;
}



/* UPDATED + CHECKED
 */
static inline char *run_location( struct cpl_interpreter *intr )
{
	unsigned short attr_name;
	unsigned short n;
	char  *p;
	unsigned char  prio;
	unsigned char  clear;
	str url;
	int i;

	clear = NO_VAL;
	prio = 10;
	url.s = (char*)UNDEF_CHAR;
	url.len = 0; /* fixes gcc 4.0 warning */

	/* sanity check */
	if (NR_OF_KIDS(intr->ip)>1) {
		LOG(L_ERR,"ERROR:run_location: LOCATION node suppose to have max "
			"one child, not %d!\n",NR_OF_KIDS(intr->ip));
		goto script_error;
	}

	for( i=NR_OF_ATTR(intr->ip),p=ATTR_PTR(intr->ip) ; i>0 ; i-- ) {
		get_basic_attr(p,attr_name,n,intr,script_error);
		switch (attr_name) {
			case URL_ATTR:
				url.len = n;
				get_str_attr( p, url.s, url.len, intr, script_error,1);
				break;
			case PRIORITY_ATTR:
				if ( n>10)
					LOG(L_WARN,"WARNING:run_location: invalid value (%u) found"
						" for param. PRIORITY in LOCATION node -> using "
						"default (%u)!\n",n,prio);
				else
					prio = n;
				break;
			case CLEAR_ATTR:
				if (n!=YES_VAL && n!=NO_VAL)
					LOG(L_WARN,"WARNING:run_location: invalid value (%u) found"
						" for param. CLEAR in LOCATION node -> using "
						"default (%u)!\n",n,clear);
				else
					clear = n;
				break;
			default:
				LOG(L_ERR,"ERROR:run_location: unknown attribute (%d) in "
					"LOCATION node\n",attr_name);
				goto script_error;
		}
	}

	if (url.s==(char*)UNDEF_CHAR) {
		LOG(L_ERR,"ERROR:run_location: param. URL missing in LOCATION node\n");
		goto script_error;
	}

	if (clear)
		empty_location_set( &(intr->loc_set) );
	if (add_location( &(intr->loc_set), &url, prio, 0/*no dup*/ )==-1) {
		LOG(L_ERR,"ERROR:run_location: unable to add location to set :-(\n");
		goto runtime_error;
	}
	/* set the flag for modifying the location set */
	intr->flags |= CPL_LOC_SET_MODIFIED;

	return get_first_child(intr->ip);
runtime_error:
	return CPL_RUNTIME_ERROR;
script_error:
	return CPL_SCRIPT_ERROR;
}



/* UPDATED + CHECKED
 */
static inline char *run_remove_location( struct cpl_interpreter *intr )
{
	unsigned short attr_name;
	unsigned short n;
	char *p;
	str url;
	int i;

	url.s = (char*)UNDEF_CHAR;

	/* sanity check */
	if (NR_OF_KIDS(intr->ip)>1) {
		LOG(L_ERR,"ERROR:cpl_c:run_remove_location: REMOVE_LOCATION node "
			"suppose to have max one child, not %d!\n",
			NR_OF_KIDS(intr->ip));
		goto script_error;
	}

	/* dirty hack to speed things up in when loc set is already empty */
	if (intr->loc_set==0)
		goto done;

	for( i=NR_OF_ATTR(intr->ip),p=ATTR_PTR(intr->ip) ; i>0 ; i-- ) {
		get_basic_attr(p,attr_name,n,intr,script_error);
		switch (attr_name) {
			case LOCATION_ATTR:
				url.len = n;
				get_str_attr( p, url.s, url.len, intr, script_error,1);
				break;
			default:
				LOG(L_ERR,"ERROR:run_remove_location: unknown attribute "
					"(%d) in REMOVE_LOCATION node\n",attr_name);
				goto script_error;
		}
	}

	if (url.s==(char*)UNDEF_CHAR) {
		DBG("DEBUG:run_remove_location: remove all locs from loc_set\n");
		empty_location_set( &(intr->loc_set) );
	} else {
		remove_location( &(intr->loc_set), url.s, url.len );
	}
	/* set the flag for modifying the location set */
	intr->flags |= CPL_LOC_SET_MODIFIED;

done:
	return get_first_child(intr->ip);
script_error:
	return CPL_SCRIPT_ERROR;
}



/* UPDATED + CHECKED
 */
static inline char *run_sub( struct cpl_interpreter *intr )
{
	char  *p;
	unsigned short offset;
	unsigned short attr_name;
	int i;

	/* sanity check */
	if (NR_OF_KIDS(intr->ip)!=0) {
		LOG(L_ERR,"ERROR:cpl_c:run_sub: SUB node doesn't suppose to have any "
			"sub-nodes. Found %d!\n",NR_OF_KIDS(intr->ip));
		goto script_error;
	}

	/* check the number of attr */
	i = NR_OF_ATTR( intr->ip );
	if (i!=1) {
		LOG(L_ERR,"ERROR:cpl_c:run_sub: incorrect nr. of attr. %d (<>1) in "
			"SUB node\n",i);
		goto script_error;
	}
	/* get attr's name */
	p = ATTR_PTR(intr->ip);
	get_basic_attr( p, attr_name, offset, intr, script_error);
	if (attr_name!=REF_ATTR) {
		LOG(L_ERR,"ERROR:cpl_c:run_sub: invalid attr. %d (expected %d)in "
			"SUB node\n", attr_name, REF_ATTR);
		goto script_error;
	}
	/* make the jump */
	p = intr->ip - offset;
	/* check the destination pointer -> are we still inside the buffer ;-) */
	if (((char*)p)<intr->script.s) {
		LOG(L_ERR,"ERROR:cpl_c:run_sub: jump offset lower than the script "
			"beginning -> underflow!\n");
		goto script_error;
	}
	check_overflow_by_ptr( p+SIMPLE_NODE_SIZE(intr->ip), intr, script_error);
	/* check to see if we hit a subaction node */
	if ( NODE_TYPE(p)!=SUBACTION_NODE ) {
		LOG(L_ERR,"ERROR:cpl_c:run_sub: sub. jump hit a nonsubaction node!\n");
		goto script_error;
	}
	if ( NR_OF_ATTR(p)!=0 ) {
		LOG(L_ERR,"ERROR:cpl_c:run_sub: invalid subaction node reached "
		"(attrs=%d); expected (0)!\n",NR_OF_ATTR(p));
		goto script_error;
	}

	return get_first_child(p);
script_error:
	return CPL_SCRIPT_ERROR;
}



/* UPDATED + CHECKED
 */
static inline char *run_reject( struct cpl_interpreter *intr )
{
	unsigned short attr_name;
	unsigned short status;
	unsigned short n;
	char *reason_s;
	char *p;
	int i;

	reason_s = (char*)UNDEF_CHAR;
	status = UNDEF_CHAR;

	/* sanity check */
	if (NR_OF_KIDS(intr->ip)!=0) {
		LOG(L_ERR,"ERROR:cpl_c:run_reject: REJECT node doesn't suppose to have "
			"any sub-nodes. Found %d!\n",NR_OF_KIDS(intr->ip));
		goto script_error;
	}

	for( i=NR_OF_ATTR(intr->ip),p=ATTR_PTR(intr->ip) ; i>0 ; i-- ) {
		get_basic_attr( p, attr_name, n, intr, script_error);
		switch (attr_name) {
			case STATUS_ATTR:
				status = n;
				break;
			case REASON_ATTR:
				get_str_attr( p, reason_s, n, intr, script_error,1);
				break;
			default:
				LOG(L_ERR,"ERROR:cpl_c:run_reject: unknown attribute "
					"(%d) in REJECT node\n",attr_name);
				goto script_error;
		}
	}

	if (status==UNDEF_CHAR) {
		LOG(L_ERR,"ERROR:cpl_c:run_reject: mandatory attribute STATUS "
			"not found\n");
		goto script_error;
	}
	if (status<400 || status>=700) {
		LOG(L_ERR,"ERROR:cpl_c:run_reject: bad attribute STATUS "
			"(%d)\n",status);
		goto script_error;
	}

	if (reason_s==(char*)UNDEF_CHAR ) {
		switch (status) {
			case 486:
				reason_s = "Busy Here";
				break;
			case 404:
				reason_s = "Not Found";
				break;
			case 603:
				reason_s = "Decline";
				break;
			case 500:
				reason_s = "Internal Server Error";
				break;
			default:
				reason_s = "Generic Error";
		}
	}

	/* if still stateless and FORCE_STATEFUL set -> build the transaction */
	if ( !(intr->flags&CPL_IS_STATEFUL) && intr->flags&CPL_FORCE_STATEFUL) {
		i = cpl_fct.tmb.t_newtran( intr->msg );
		if (i<0) {
			LOG(L_ERR,"ERROR:cpl-c:run_reject: failed to build new "
				"transaction!\n");
			goto runtime_error;
		} else if (i==0) {
			LOG(L_ERR,"ERROR:cpl-c:run_reject: processed INVITE is a "
				"retransmission!\n");
			/* instead of generating an error is better just to break the
			 * script by returning EO_SCRIPT */
			return EO_SCRIPT;
		}
		intr->flags |= CPL_IS_STATEFUL;
	}

	/* send the reply */
	if ( intr->flags&CPL_IS_STATEFUL ) {
		/* reply statefully */
		i = cpl_fct.tmb.t_reply(intr->msg, (int)status, reason_s );
	} else {
		/* reply statelessly */
		i = cpl_fct.slb.zreply(intr->msg, status, reason_s );
	}

	if ( i!=1 ) {
		LOG(L_ERR,"ERROR:run_reject: unable to send reject reply!\n");
		goto runtime_error;
	}

	return EO_SCRIPT;
runtime_error:
	return CPL_RUNTIME_ERROR;
script_error:
	return CPL_SCRIPT_ERROR;
}



/* UPDATED + CHECKED
 */
static inline char *run_redirect( struct cpl_interpreter *intr )
{
	struct location *loc;
	struct lump_rpl *lump;
	unsigned short attr_name;
	unsigned short permanent;
	unsigned short n;
	char *p;
	str lump_str;
	char *cp;
	int i;

	permanent = NO_VAL;

	/* sanity check */
	if (NR_OF_KIDS(intr->ip)!=0) {
		LOG(L_ERR,"ERROR:cpl-c:run_redirect: REDIRECT node doesn't suppose "
			"to have any sub-nodes. Found %d!\n",NR_OF_KIDS(intr->ip));
		goto script_error;
	}

	/* read the attributes of the REDIRECT node*/
	for( i=NR_OF_ATTR(intr->ip),p=ATTR_PTR(intr->ip) ; i>0 ; i-- ) {
		get_basic_attr( p, attr_name, n, intr, script_error);
		switch (attr_name) {
			case PERMANENT_ATTR:
				if (n!=YES_VAL && n!=NO_VAL) {
					LOG(L_ERR,"ERROR:cpl-c:run_redirect: unsupported value (%d)"
						" in attribute PERMANENT for REDIRECT node",n);
					goto script_error;
				}
				permanent = n;
				break;
			default:
				LOG(L_ERR,"ERROR:run_redirect: unknown attribute "
					"(%d) in REDIRECT node\n",attr_name);
				goto script_error;
		}
	}

	/* build the lump for Contact header */
	lump_str.len = 9 /*"Contact: "*/;
	for(loc=intr->loc_set;loc;loc=loc->next)
		lump_str.len += 1/*"<"*/ + loc->addr.uri.len + 7/*">;q=x.x"*/ +
			2*(loc->next!=0)/*" ,"*/;
	lump_str.len += CRLF_LEN;

	lump_str.s = pkg_malloc( lump_str.len );
	if(!lump_str.s) {
		LOG(L_ERR,"ERROR:cpl-c:run_redirect: out of pkg memory!\n");
		goto runtime_error;
	}
	cp = lump_str.s;
	memcpy( cp , "Contact: " , 9);
	cp += 9;
	for(loc=intr->loc_set;loc;loc=loc->next) {
		*(cp++) = '<';
		memcpy(cp,loc->addr.uri.s,loc->addr.uri.len);
		cp += loc->addr.uri.len;
		memcpy(cp,">;q=",4);
		cp += 4;
		*(cp++) = (loc->addr.priority!=10)?'0':'1';
		*(cp++) = '.';
		*(cp++) = '0'+(loc->addr.priority%10);
		if (loc->next) {
			*(cp++) = ' ';
			*(cp++) = ',';
		}
	}
	memcpy(cp,CRLF,CRLF_LEN);

	/* if still stateless and FORCE_STATEFUL set -> build the transaction */
	if ( !(intr->flags&CPL_IS_STATEFUL) && intr->flags&CPL_FORCE_STATEFUL) {
		i = cpl_fct.tmb.t_newtran( intr->msg );
		if (i<0) {
			LOG(L_ERR,"ERROR:cpl-c:run_redirect: failed to build new "
				"transaction!\n");
			pkg_free( lump_str.s );
			goto runtime_error;
		} else if (i==0) {
			LOG(L_ERR,"ERROR:cpl-c:run_redirect: processed INVITE is a "
				"retransmission!\n");
			/* instead of generating an error is better just to break the
			 * script by returning EO_SCRIPT */
			pkg_free( lump_str.s );
			return EO_SCRIPT;
		}
		intr->flags |= CPL_IS_STATEFUL;
	}

	/* add the lump to the reply */
	lump = add_lump_rpl( intr->msg, lump_str.s , lump_str.len , LUMP_RPL_HDR);
	if(!lump) {
		LOG(L_ERR,"ERROR:cpl-c:run_redirect: unable to add lump_rpl! \n");
		pkg_free( lump_str.s );
		goto runtime_error;
	}

	/* send the reply */
	if ( intr->flags&CPL_IS_STATEFUL ) {
		/* reply statefully */
		if (permanent)
			i = cpl_fct.tmb.t_reply( intr->msg, (int)301, "Moved permanently");
		else
			i = cpl_fct.tmb.t_reply( intr->msg, (int)302, "Moved temporarily");
	} else {
		/* reply statelessly */
		if (permanent)
			i = cpl_fct.slb.zreply( intr->msg, 301, "Moved permanently");
		else
			i = cpl_fct.slb.zreply( intr->msg, 302, "Moved temporarily");
	}

	/* msg which I'm working on can be in private memory or is a clone into
	 * shared memory (if I'm after a failed proxy); So, it's better to removed
	 * by myself the lump that I added previously */
	unlink_lump_rpl( intr->msg, lump);
	free_lump_rpl( lump );

	if (i!=1) {
		LOG(L_ERR,"ERROR:cpl-c:run_redirect: unable to send "
			"redirect reply!\n");
		goto runtime_error;
	}

	return EO_SCRIPT;
runtime_error:
	return CPL_RUNTIME_ERROR;
script_error:
	return CPL_SCRIPT_ERROR;
}



/* UPDATED + CHECKED
 */
static inline char *run_log( struct cpl_interpreter *intr )
{
	char  *p;
	unsigned short attr_name;
	unsigned short n;
	str name    = STR_NULL;
	str comment = STR_NULL;
	str user;
	int i;

	/* sanity check */
	if (NR_OF_KIDS(intr->ip)>1) {
		LOG(L_ERR,"ERROR:cpl_c:run_log: LOG node suppose to have max one child"
			", not %d!\n",NR_OF_KIDS(intr->ip));
		goto script_error;
	}

	/* is logging enabled? */
	if ( cpl_env.log_dir==0 )
		goto done;

	/* read the attributes of the LOG node*/
	p = ATTR_PTR(intr->ip);
	for( i=NR_OF_ATTR(intr->ip); i>0 ; i-- ) {
		get_basic_attr( p, attr_name, n, intr, script_error);
		switch (attr_name) {
			case NAME_ATTR:
				get_str_attr( p, name.s, n, intr, script_error,1);
				name.len = n;
				break;
			case COMMENT_ATTR:
				get_str_attr( p, comment.s, n, intr, script_error,1);
				comment.len = n;
				break;
			default:
				LOG(L_ERR,"ERROR:cpl_c:run_log: unknown attribute "
					"(%d) in LOG node\n",attr_name);
				goto script_error;
		}
	}

	if (comment.len==0) {
		LOG(L_NOTICE,"NOTICE:cpl_c:run_log: LOG node has no comment attr -> "
			"skipping\n");
		goto done;
	}

	user.len = intr->user.len + name.len + comment.len;
	/* duplicate the attrs in shm memory */
	user.s = p = (char*)shm_malloc( user.len );
	if (!user.s) {
		LOG(L_ERR,"ERROR:cpl_c:run_log: no more shm memory!\n");
		goto runtime_error;
	}
	/* copy the user name */
	memcpy( p, intr->user.s, intr->user.len);
	user.len = intr->user.len;
	p += intr->user.len;
	/* copy the log name */
	if (name.len) {
		memcpy( p, name.s, name.len );
		name.s = p;
		p += name.len;
	}
	/* copy the comment */
	memcpy( p, comment.s, comment.len);
	comment.s = p;

	/* send the command */
	write_cpl_cmd( CPL_LOG_CMD, &user, &name, &comment );

done:
	return get_first_child(intr->ip);
runtime_error:
	return CPL_RUNTIME_ERROR;
script_error:
	return CPL_SCRIPT_ERROR;
}



/* UPDATED + CHECKED
 */
static inline char *run_mail( struct cpl_interpreter *intr )
{
	unsigned short attr_name;
	unsigned short n;
	char  *p;
	str subject = STR_NULL;
	str body    = STR_NULL;
	str to      = STR_NULL;
	int i;

	/* sanity check */
	if (NR_OF_KIDS(intr->ip)>1) {
		LOG(L_ERR,"ERROR:cpl_c:run_mail: MAIL node suppose to have max one"
			" child, not %d!\n",NR_OF_KIDS(intr->ip));
		goto script_error;
	}

	/* read the attributes of the MAIL node*/
	for( i=NR_OF_ATTR(intr->ip),p=ATTR_PTR(intr->ip) ; i>0 ; i-- ) {
		get_basic_attr(p, attr_name, n, intr, script_error);
		switch (attr_name) {
			case TO_ATTR:
				get_str_attr(p, to.s, n, intr, script_error,0);
				to.len = n;
				break;
			case SUBJECT_ATTR:
				get_str_attr(p, subject.s, n, intr, script_error,0);
				subject.len = n;
				break;
			case BODY_ATTR:
				get_str_attr(p, body.s, n, intr, script_error,0);
				body.len = n;
				break;
			default:
				LOG(L_ERR,"ERROR:run_mail: unknown attribute "
					"(%d) in MAIL node\n",attr_name);
				goto script_error;
		}
	}

	if (to.len==0) {
		LOG(L_ERR,"ERROR:cpl_c:run_mail: email has an empty TO hdr!\n");
		goto script_error;
	}
	if (body.len==0 && subject.len==0) {
		LOG(L_WARN,"WARNING:cpl_c:run_mail: I refuse to send email with no "
			"body and no subject -> skipping...\n");
		goto done;
	}

	/* duplicate the attrs in shm memory */
	p = (char*)shm_malloc( to.len + subject.len + body.len );
	if (!p) {
		LOG(L_ERR,"ERROR:cpl_c:run_mail: no more shm memory!\n");
		goto runtime_error;
	}
	/* copy the TO */
	memcpy( p, to.s, to.len );
	to.s = p;
	p += to.len;
	/* copy the subject */
	if (subject.len) {
		memcpy( p, subject.s, subject.len );
		subject.s = p;
		p += subject.len;
	}
	/* copy the body */
	if (body.len) {
		memcpy( p, body.s, body.len );
		body.s = p;
		p += body.len;
	}

	/* send the command */
	write_cpl_cmd( CPL_MAIL_CMD, &to, &subject, &body);

done:
	return get_first_child(intr->ip);
runtime_error:
	return CPL_RUNTIME_ERROR;
script_error:
	return CPL_SCRIPT_ERROR;
}



static inline int run_default( struct cpl_interpreter *intr )
{
	if (!(intr->flags&CPL_PROXY_DONE)) {
		/* no signaling operations */
		if ( !(intr->flags&CPL_LOC_SET_MODIFIED) ) {
			/*  no location modifications */
			if (intr->loc_set==0 ) {
				/* case 1 : no location modifications or signaling operations
				 * performed, location set empty ->
				 * Look up the user's location through whatever mechanism the
				 * server would use if no CPL script were in effect */
				return SCRIPT_DEFAULT;
			} else {
				/* case 2 : no location modifications or signaling operations
				 * performed, location set non-empty: (This can only happen 
				 * for outgoing calls.) ->
				 * Proxy the call to the address in the location set.
				 * With other words, let ser to continue processing the
				 * request as nothing happened */
				return SCRIPT_DEFAULT;
			}
		} else {
			/* case 3 : location modifications performed, no signaling 
			 * operations ->
			 * Proxy the call to the addresses in the location set */
			if (!cpl_proxy_to_loc_set(intr->msg,&(intr->loc_set),intr->flags))
				return SCRIPT_END;
			return SCRIPT_RUN_ERROR;
		}
	} else {
		/* case 4 : proxy operation previously taken -> return whatever the 
		 * "best" response is of all accumulated responses to the call to this
		 * point, according to the rules of the underlying signaling
		 * protocol. */
		/* we will let ser to choose and forward one of the replies -> for this
		 * nothing must be done */
		return SCRIPT_END;
	}
	/*return SCRIPT_RUN_ERROR;*/
}



/* include all inline functions for processing the switches */
#include "cpl_switches.h"
/* include inline function for running proxy node */
#include "cpl_proxy.h"



int cpl_run_script( struct cpl_interpreter *intr )
{
	char *new_ip;

	do {
		check_overflow_by_offset( SIMPLE_NODE_SIZE(intr->ip), intr, error);
		switch ( NODE_TYPE(intr->ip) ) {
			case CPL_NODE:
				DBG("DEBUG:cpl_run_script: processing CPL node \n");
				new_ip = run_cpl_node( intr ); /*UPDATED&TESTED*/
				break;
			case ADDRESS_SWITCH_NODE:
				DBG("DEBUG:cpl_run_script: processing address-switch node\n");
				new_ip = run_address_switch( intr ); /*UPDATED&TESTED*/
				break;
			case STRING_SWITCH_NODE:
				DBG("DEBUG:cpl_run_script: processing string-switch node\n");
				new_ip = run_string_switch( intr ); /*UPDATED&TESTED*/
				break;
			case PRIORITY_SWITCH_NODE:
				DBG("DEBUG:cpl_run_script: processing priority-switch node\n");
				new_ip = run_priority_switch( intr ); /*UPDATED&TESTED*/
				break;
			case TIME_SWITCH_NODE:
				DBG("DEBUG:cpl_run_script: processing time-switch node\n");
				new_ip = run_time_switch( intr ); /*UPDATED&TESTED*/
				break;
			case LANGUAGE_SWITCH_NODE:
				DBG("DEBUG:cpl_run_script: processing language-switch node\n");
				new_ip = run_language_switch( intr ); /*UPDATED&TESTED*/
				break;
			case LOOKUP_NODE:
				DBG("DEBUG:cpl_run_script: processing lookup node\n");
				new_ip = run_lookup( intr ); /*UPDATED&TESTED*/
				break;
			case LOCATION_NODE:
				DBG("DEBUG:cpl_run_script: processing location node\n");
				new_ip = run_location( intr ); /*UPDATED&TESTED*/
				break;
			case REMOVE_LOCATION_NODE:
				DBG("DEBUG:cpl_run_script: processing remove_location node\n");
				new_ip = run_remove_location( intr ); /*UPDATED&TESTED*/
				break;
			case PROXY_NODE:
				DBG("DEBUG:cpl_run_script: processing proxy node\n");
				new_ip = run_proxy( intr );/*UPDATED&TESTED*/
				break;
			case REJECT_NODE:
				DBG("DEBUG:cpl_run_script: processing reject node\n");
				new_ip = run_reject( intr ); /*UPDATED&TESTED*/
				break;
			case REDIRECT_NODE:
				DBG("DEBUG:cpl_run_script: processing redirect node\n");
				new_ip = run_redirect( intr ); /*UPDATED&TESTED*/
				break;
			case LOG_NODE:
				DBG("DEBUG:cpl_run_script: processing log node\n");
				new_ip = run_log( intr ); /*UPDATED&TESTED*/
				break;
			case MAIL_NODE:
				DBG("DEBUG:cpl_run_script: processing mail node\n");
				new_ip = run_mail( intr ); /*UPDATED&TESTED*/
				break;
			case SUB_NODE:
				DBG("DEBUG:cpl_run_script: processing sub node\n");
				new_ip = run_sub( intr ); /*UPDATED&TESTED*/
				break;
			default:
				LOG(L_ERR,"ERROR:cpl_run_script: unknown type node (%d)\n",
					NODE_TYPE(intr->ip));
				goto error;
		}

		if (new_ip==CPL_RUNTIME_ERROR) {
			LOG(L_ERR,"ERROR:cpl_c:cpl_run_script: runtime error\n");
			return SCRIPT_RUN_ERROR;
		} else if (new_ip==CPL_SCRIPT_ERROR) {
			LOG(L_ERR,"ERROR:cpl_c:cpl_run_script: script error\n");
			return SCRIPT_FORMAT_ERROR;
		} else if (new_ip==DEFAULT_ACTION) {
			DBG("DEBUG:cpl_c:cpl_run_script: running default action\n");
			return run_default(intr);
		} else if (new_ip==EO_SCRIPT) {
			DBG("DEBUG:cpl_c:cpl_run_script: script interpretation done!\n");
			return SCRIPT_END;
		} else if (new_ip==CPL_TO_CONTINUE) {
			DBG("DEBUG:cpl_c:cpl_run_script: done for the moment; waiting "
				"after signaling!\n");
			return SCRIPT_TO_BE_CONTINUED;
		}
		/* move to the new instruction */
		intr->ip = new_ip;
	}while(1);

error:
	return SCRIPT_FORMAT_ERROR;
}
