/*
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
#include "../tm/tm_load.h"
#include "CPL_tree.h"
#include "loc_set.h"
#include "cpl_utils.h"
#include "cpl_nonsig.h"
#include "cpl_sig.h"
#include "cpl_run.h"


#define EO_SCRIPT            ((unsigned char*)0xffffffff)
#define DEFAULT_ACTION       ((unsigned char*)0xfffffffe)
#define CPL_SCRIPT_ERROR     ((unsigned char*)0xfffffffd)
#define CPL_RUNTIME_ERROR    ((unsigned char*)0xfffffffc)
#define CPL_TO_CONTINUE      ((unsigned char*)0xfffffffb)

#define HDR_NOT_FOUND        ((char*)0xffffffff)
#define UNDEF_CHAR           (0xff)

#define check_overflow_by_ptr(_ptr_,_intr_,_error_) \
	do {\
		if ( (char*)(_ptr_)>(_intr_)->script.len+(_intr_)->script.s ) {\
			LOG(L_ERR,"ERROR:cpl_c: overflow detected ip=%p ptr=%p in "\
			"func. %s, line %d\n",(_intr_)->ip,_ptr_,__FUNCTION__,__LINE__);\
			goto _error_; \
		} \
	}while(0)

#define check_overflow_by_offset(_len_,_intr_,_error_) \
	do {\
		if ( (char*)((_intr_)->ip+(_len_)) > \
		(_intr_)->script.len+(_intr_)->script.s ) {\
			LOG(L_ERR,"ERROR:cpl_c: overflow detected ip=%p offset=%d in "\
			"func. %s, line %d\n",(_intr_)->ip,_len_,__FUNCTION__,__LINE__);\
			goto _error_; \
		} \
	}while(0)

#define get_first_child(_node_) \
	((NR_OF_KIDS(_node_)==0)?DEFAULT_ACTION:(_node_)+KID_OFFSET(_node_,0))


extern int    (*sl_send_rpl)(struct sip_msg*, char*, char*);
extern char   *log_dir;
extern struct tm_binds cpl_tmb;



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

	/* check the begining of the script */
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
	DBG("******** freeing -> intr=%p\n",intr);
	if (intr) {
		if (intr->script.s)
			shm_free( intr->script.s);
		empty_location_set( &(intr->loc_set) );
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



inline unsigned char *run_cpl_node( struct cpl_interpreter *intr )
{
	unsigned char *kid;
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
			LOG(L_ERR,"ERROR:run_cpl_node: unknown child type (%d) "
				"for CPL node!!\n",NODE_TYPE(kid));
			return CPL_SCRIPT_ERROR;
		}
	}

	DBG("DEBUG:cpl_c:run_cpl_node: CPL node has no %d subnode -> default\n",
		start);
	return DEFAULT_ACTION;
}



inline unsigned char *run_lookup( struct cpl_interpreter *intr )
{
	unsigned char *kid;
	unsigned char *failure_kid = 0;
	unsigned char *p;
	int len;
	int i;

	for( i=NR_OF_ATTR(intr->ip),p=ATTR_PTR(intr->ip) ; i>0 ; i-- ) {
		switch (*p) {
			case SOURCE_ATTR:
			case USE_ATTR:
			case IGNORE_ATTR:
				check_overflow_by_ptr( p+2, intr, script_error);
				len = *((unsigned short*)(p+1));
				check_overflow_by_ptr( p+2+len, intr, script_error);
				p += 3+len;
				break;
			case TIMEOUT_ATTR:
			case CLEAR_ATTR:
				check_overflow_by_ptr( p+1, intr, script_error);
				p += 2;
				break;
			default:
				LOG(L_ERR,"ERROR:run_lookup: unknown attribute (%d) in"
					"LOCATION node\n",*p);
				goto script_error;
		}
	}

	for( i=0 ; i<NR_OF_KIDS(intr->ip) ; i++ ) {
		kid = intr->ip + KID_OFFSET(intr->ip,i);
		check_overflow_by_ptr( kid+SIMPLE_NODE_SIZE(kid), intr, script_error);
		switch ( NODE_TYPE(kid) ) {
			case SUCCESS_NODE :
			case NOTFOUND_NODE:
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

	LOG(L_NOTICE,"NOTICE:cpl_c:run_lookup: this node failes by default - "
		"to unsecure and tiem consuming to be exectuted\n");
	if (failure_kid)
		return get_first_child(failure_kid);
	return DEFAULT_ACTION;
script_error:
	return CPL_SCRIPT_ERROR;
}



inline unsigned char *run_location( struct cpl_interpreter *intr )
{
	unsigned char n;
	unsigned char *p;
	unsigned char prio;
	unsigned char clear;
	str url;
	int i;

	clear = NO_VAL;
	prio = 10;
	url.s = (char*)UNDEF_CHAR;

	/* sanity check */
	if (NR_OF_KIDS(intr->ip)>1) {
		LOG(L_ERR,"ERROR:run_location: LOCATION node suppose to have max "
			"one child, not %d!\n",NR_OF_KIDS(intr->ip));
		goto script_error;
	}

	for( i=NR_OF_ATTR(intr->ip),p=ATTR_PTR(intr->ip) ; i>0 ; i-- ) {
		switch (*p) {
			case URL_ATTR:
				check_overflow_by_ptr( p+2, intr, script_error);
				url.len = *((unsigned short*)(p+1));
				check_overflow_by_ptr( p+2+url.len, intr, script_error);
				url.s = ((url.len)?(p+3):0);
				p += 3+url.len;
				break;
			case PRIORITY_ATTR:
				check_overflow_by_ptr( p+1, intr, script_error);
				n = (unsigned char)(*(p+1));
				if ( n>10)
					LOG(L_WARN,"WARNING:run_location: invalid value (%u) found"
						" for param. PRIORITY in LOCATION node -> using "
						"default (%u)!\n",n,prio);
				else
					prio = n;
				p += 2;
				break;
			case CLEAR_ATTR:
				check_overflow_by_ptr( p+1, intr, script_error);
				n = (unsigned char)(*(p+1));
				if (n!=YES_VAL && n!=NO_VAL)
					LOG(L_WARN,"WARNING:run_location: invalid value (%u) found"
						" for param. CLEAR in LOCATION node -> using "
						"default (%u)!\n",n,clear);
				else
					clear = n;
				p += 2;
				break;
			default:
				LOG(L_ERR,"ERROR:run_location: unknown attribute (%d) in"
					"LOCATION node\n",*p);
				goto script_error;
		}
	}

	if (url.s==(char*)UNDEF_CHAR) {
		LOG(L_ERR,"ERROR:run_location: param. URL missing in LOCATION node\n");
		goto script_error;
	}

	if (clear)
		empty_location_set( &(intr->loc_set) );
	if (add_location( &(intr->loc_set), url.s, url.len, prio )==-1) {
		LOG(L_ERR,"ERROR:run_location: unable to add location to set :-(\n");
		goto runtime_error;
	}
	/* set the flag for modifing the location set */
	intr->flags |= CPL_LOC_SET_MODIFIED;

	return get_first_child(intr->ip);
runtime_error:
	return CPL_RUNTIME_ERROR;
script_error:
	return CPL_SCRIPT_ERROR;
}



inline unsigned char *run_remove_location( struct cpl_interpreter *intr )
{
	unsigned char *p;
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
		switch (*p) {
			case LOCATION_ATTR:
				check_overflow_by_ptr( p+2, intr, script_error);
				url.len = *((unsigned short*)(p+1));
				check_overflow_by_ptr( p+2+url.len, intr, script_error);
				url.s = ((url.len)?(p+3):0);
				p += 3+url.len;
				break;
			default:
				LOG(L_ERR,"ERROR:run_remove_location: unknown attribute "
					"(%d) in REMOVE_LOCATION node\n",*p);
				goto script_error;
		}
	}

	if (url.s==(char*)UNDEF_CHAR || url.len==0) {
		DBG("DEBUG:run_remove_location: remove all locs from loc_set\n");
		empty_location_set( &(intr->loc_set) );
	} else {
		remove_location( &(intr->loc_set), url.s, url.len );
	}
	/* set the flag for modifing the location set */
	intr->flags |= CPL_LOC_SET_MODIFIED;

done:
	return get_first_child(intr->ip);
script_error:
	return CPL_SCRIPT_ERROR;
}



inline unsigned char *run_sub( struct cpl_interpreter *intr )
{
	unsigned char  *p;
	unsigned short offset;
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
	check_overflow_by_ptr( p+2, intr, script_error);
	if (*p!=REF_ATTR) {
		LOG(L_ERR,"ERROR:cpl_c:run_sub: invalid attr. %d (expected %d)in "
			"SUB node\n", *p, REF_ATTR);
		goto script_error;
	}
	/* get the attr's value */
	offset = *((unsigned short*)(p+1));
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
	if (NR_OF_KIDS(p)!=1 || NR_OF_ATTR(p)!=0 ) {
		LOG(L_ERR,"ERROR:cpl_c:run_sub: inavlid subaction node reached "
		"(kids=%d,attrs=%d); expected (1,0)!\n",NR_OF_KIDS(p),NR_OF_ATTR(p));
		goto script_error;
	}

	return get_first_child(p);
script_error:
	return CPL_SCRIPT_ERROR;
}



inline unsigned char *run_reject( struct cpl_interpreter *intr )
{
	unsigned char *p;
	unsigned short status;
	char *reason_s;
	int   reason_len;
	int i;

	reason_s = (char*)UNDEF_CHAR;
	status = UNDEF_CHAR;

	/* sanity check */
	if (NR_OF_KIDS(intr->ip)!=0) {
		LOG(L_ERR,"ERROR:run_reject: REJECT node doesn't suppose to have any "
			"sub-nodes. Found %d!\n",NR_OF_KIDS(intr->ip));
		goto script_error;
	}

	for( i=NR_OF_ATTR(intr->ip),p=ATTR_PTR(intr->ip) ; i>0 ; i-- ) {
		switch (*p) {
			case STATUS_ATTR:
				check_overflow_by_ptr( p+2, intr, script_error);
				status = *((unsigned short*)(p+1));
				p += 3;
				break;
			case REASON_ATTR:
				check_overflow_by_ptr( p+2, intr, script_error);
				reason_len = *((unsigned short*)(p+1));
				check_overflow_by_ptr( p+2+reason_len, intr, script_error);
				reason_s = p+3;
				p += 3+reason_len;
				break;
			default:
				LOG(L_ERR,"ERROR:run_reject: unknown attribute "
					"(%d) in REJECT node\n",*p);
				goto script_error;
		}
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
			default:
				LOG(L_ERR,"ERROR:run_reject: unknown value (%d) for attribute"
					" STATUS in reject node\n",status);
				goto script_error;
		}
	}

	if ( cpl_tmb.t_reply(intr->msg, (int)status, reason_s ) ) {
		LOG(L_ERR,"ERROR:run_reject: unable to send reject reply!\n");
		goto runtime_error;
	}

	return EO_SCRIPT;
runtime_error:
	return CPL_RUNTIME_ERROR;
script_error:
	return CPL_SCRIPT_ERROR;
}



inline unsigned char *run_redirect( struct cpl_interpreter *intr )
{
	struct location *loc;
	struct lump_rpl *lump;
	unsigned char *p;
	unsigned char permanent;
	str lump_str;
	char *cp;
	int i;

	permanent = NO_VAL;

	/* sanity check */
	if (NR_OF_KIDS(intr->ip)!=0) {
		LOG(L_ERR,"ERROR:run_redirect: REDIRECT node doesn't suppose to have "
			"any sub-nodes. Found %d!\n",NR_OF_KIDS(intr->ip));
		goto script_error;
	}

	/* read the attributes of the REDIRECT node*/
	for( i=NR_OF_ATTR(intr->ip),p=ATTR_PTR(intr->ip) ; i>0 ; i-- ) {
		switch (*p) {
			case PERMANENT_ATTR:
				check_overflow_by_ptr( p+1, intr, script_error);
				permanent = *(p+1);
				if (permanent!=YES_VAL && permanent!=NO_VAL) {
					LOG(L_ERR,"ERROR:run_redirect: unsupported value (%d) "
						"in attribute PERMANENT for REDIRECT node",permanent);
					goto script_error;
				}
				p += 2;
				break;
			default:
				LOG(L_ERR,"ERROR:run_redirect: unknown attribute "
					"(%d) in REDIRECT node\n",*p);
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
		LOG(L_ERR,"ERROR:cpl_c:cpl_redirect: out of pkg memory!\n");
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

	/* add the lump to the reply */
	lump = build_lump_rpl( lump_str.s , lump_str.len );
	if(!lump) {
		LOG(L_ERR,"ERROR:cpl_redirect: unable to build lump_rpl! \n");
		pkg_free( lump_str.s );
		goto runtime_error;
	}
	add_lump_rpl( intr->msg , lump );

	/* send the reply */
	if (permanent)
		i = cpl_tmb.t_reply( intr->msg, (int)301, "Moved permanently" );
	else
		i = cpl_tmb.t_reply( intr->msg, (int)302, "Moved temporarily" );
	if (i<0) {
		LOG(L_ERR,"ERROR:run_redirect: unable to send redirect reply!\n");
		goto runtime_error;
	}

	return EO_SCRIPT;
runtime_error:
	return CPL_RUNTIME_ERROR;
script_error:
	return CPL_SCRIPT_ERROR;
}



inline unsigned char *run_log( struct cpl_interpreter *intr )
{
	unsigned char *p;
	unsigned char *attr_ptr;
	str buf;
	str name;
	str comment;
	str user;
	int i;

	buf.s = name.s = comment.s = 0;
	buf.len = name.len = comment.len = 0;

	/* sanity check */
	if (NR_OF_KIDS(intr->ip)>1) {
		LOG(L_ERR,"ERROR:run_log: LOG node suppose to have max one child, "
			"not %d!\n",NR_OF_KIDS(intr->ip));
		goto script_error;
	}

	/* is loging enabled? */
	if ( log_dir==0 )
		goto done;

	/* read the attributes of the LOG node*/
	attr_ptr = ATTR_PTR(intr->ip);
	for( i=NR_OF_ATTR(intr->ip),p=attr_ptr ; i>0 ; i-- ) {
		switch (*p) {
			case NAME_ATTR:
				check_overflow_by_ptr( p+2, intr, script_error);
				name.len = *((unsigned short*)(p+1));
				check_overflow_by_ptr( p+2+name.len, intr, script_error);
				name.s = name.len?((char*)0 + (p-attr_ptr) + 3):0;
				p += 3+name.len;
				break;
			case COMMENT_ATTR:
				check_overflow_by_ptr( p+2, intr, script_error);
				comment.len = *((unsigned short*)(p+1));
				check_overflow_by_ptr( p+2+comment.len, intr, script_error);
				comment.s = comment.len?((char*)0 + (p-attr_ptr) + 3):0;
				p += 3+comment.len;
				break;
			default:
				LOG(L_ERR,"ERROR:run_log: unknown attribute "
					"(%d) in LOG node\n",*p);
				goto script_error;
		}
	}

	if (comment.len==0) {
		LOG(L_WARN,"WARNING:cpl_c:run_log: LOG node has no comment attr -> "
			"skipping\n");
		goto done;
	}

	buf.len = intr->user.len + (p - attr_ptr);
	/* duplicate the attrs in shm memory */
	buf.s = (char*)shm_malloc( buf.len );
	if (!buf.s) {
		LOG(L_ERR,"ERROR:cpl_c:run_log: no more shm memory!\n");
		goto runtime_error;
	}
	memcpy( buf.s, intr->user.s, intr->user.len);
	memcpy( buf.s+intr->user.len, attr_ptr, buf.len );

	/* updates the pointer inside the new buffer */
	user.len = intr->user.len;
	user.s = buf.s;
	comment.s += (buf.s-(char*)0) + user.len;
	if (name.s) name.s += (buf.s-(char*)0) + user.len;

	/* send the command */
	write_cpl_cmd( CPL_LOG_CMD, &user, &name, &comment );

done:
	return get_first_child(intr->ip);
runtime_error:
	return CPL_RUNTIME_ERROR;
script_error:
	return CPL_SCRIPT_ERROR;
}



inline unsigned char *run_mail( struct cpl_interpreter *intr )
{
	unsigned char *p;
	str buf;
	str subject;
	str body;
	str to;
	int i;

	buf.s = to.s = subject.s = body.s = 0;
	buf.len = to.len = subject.len = body.len = 0;

	/* sanity check */
	if (NR_OF_KIDS(intr->ip)>1) {
		LOG(L_ERR,"ERROR:cpl_c:run_mail: MAIL node suppose to have max one"
			" child, not %d!\n",NR_OF_KIDS(intr->ip));
		goto script_error;
	}

	/* read the attributes of the MAIL node*/
	for( i=NR_OF_ATTR(intr->ip),p=ATTR_PTR(intr->ip) ; i>0 ; i-- ) {
		switch (*p) {
			case TO_ATTR:
				check_overflow_by_ptr( p+2, intr, script_error);
				to.len = *((unsigned short*)(p+1));
				check_overflow_by_ptr( p+2+to.len, intr, script_error);
				to.s = to.len?(p+3):0;
				p += 3+to.len;
				break;
			case SUBJECT_ATTR:
				check_overflow_by_ptr( p+2, intr, script_error);
				subject.len = *((unsigned short*)(p+1));
				check_overflow_by_ptr( p+2+subject.len, intr, script_error);
				subject.s = subject.len?(p+3):0;
				p += 3+subject.len;
				break;
			case BODY_ATTR:
				check_overflow_by_ptr( p+2, intr, script_error);
				body.len = *((unsigned short*)(p+1));
				check_overflow_by_ptr( p+2+body.len, intr, script_error);
				body.s = body.len?(p+3):0;
				p += 3+body.len;
				break;
			default:
				LOG(L_ERR,"ERROR:run_mail: unknown attribute "
					"(%d) in MAIL node\n",*p);
				goto script_error;
		}
	}

	if (to.len==0) {
		LOG(L_ERR,"ERROR:cpl_c:run_mail: email has an empty TO hdr!\n");
		goto script_error;
	}
	if (body.len==0 && subject.len) {
		LOG(L_WARN,"WARNINGLcpl_c:run_mail: I refuse to send email with no "
			"body and subject -> skipping...\n");
		goto done;
	}

	buf.len = to.len + subject.len + body.len;
	/* duplicate the attrs in shm memory */
	buf.s = (char*)shm_malloc( buf.len );
	if (!buf.s) {
		LOG(L_ERR,"ERROR:cpl_c:run_mail: no more shm memory!\n");
		goto runtime_error;
	}
	p = buf.s;
	/* copy the TO */
	memcpy( p, to.s, to.len );
	to.s = p;
	p += to.len;
	/* copy the subject */
	memcpy( p, subject.s, subject.len );
	subject.s = p;
	p += subject.len;
	/* copy the body */
	memcpy( p, body.s, body.len );
	body.s = p;
	p += body.len;

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
		/* no signalling operations */
		if ( !(intr->flags&CPL_LOC_SET_MODIFIED) ) {
			/*  no location modifications */
			if (intr->loc_set==0 ) {
				/* case 1 : no location modifications or signalling operations
				 * performed, location set empty ->
				 * Look up the user's location through whatever mechanism the
				 * server would use if no CPL script were in effect */
				return SCRIPT_DEFAULT;
			} else {
				/* case 2 : no location modifications or signalling operations
				 * performed, location set non-empty: (This can only happen 
				 * for outgoing calls.) ->
				 * Proxy the call to the addresses in the location set */
				LOG(L_ERR,"ERROR:cpl_c:run_default: case 2 reached -"
					"unimplemented\n");
				return SCRIPT_RUN_ERROR;
			}
		} else {
			/* case 3 : location modifications performed, no signalling 
			 * operations ->
			 * Proxy the call to the addresses in the location set */
			if (cpl_proxy_to_loc_set( intr->msg, &(intr->loc_set), 0 )==0)
				return SCRIPT_END;
			return SCRIPT_RUN_ERROR;
		}
	} else {
		/* case 4 : proxy operation previously taken -> return whatever the 
		 * "best" response is of all accumulated responses to the call to this
		 * point, according to the rules of the underlying signalling
		 * protocol. */
		/* we will let ser to choose and forward one of the replies -> for this
		 * nothinh must be done */
		return SCRIPT_END;
	}
	return SCRIPT_RUN_ERROR;
}



/* include all inline functions for processing the switches */
#include "cpl_switches.h"
/* include inline function for running proxy node */
#include "cpl_proxy.h"



int run_cpl_script( struct cpl_interpreter *intr )
{
	do {
		check_overflow_by_offset( SIMPLE_NODE_SIZE(intr->ip), intr, error);
		switch ( NODE_TYPE(intr->ip) ) {
			case CPL_NODE:
				DBG("DEBUG:run_cpl_script: processing CPL node \n");
				intr->ip = run_cpl_node( intr );
				break;
			case ADDRESS_SWITCH_NODE:
				DBG("DEBUG:run_cpl_script: processing address-switch node\n");
				intr->ip = run_address_switch( intr );
				break;
			case STRING_SWITCH_NODE:
				DBG("DEBUG:run_cpl_script: processing string-switch node\n");
				intr->ip = run_string_switch( intr );
				break;
			case PRIORITY_SWITCH_NODE:
				DBG("DEBUG:run_cpl_script: processing priority-switch node\n");
				intr->ip = run_priority_switch( intr );
				break;
			case TIME_SWITCH_NODE:
				DBG("DEBUG:run_cpl_script: processing time-switch node\n");
				intr->ip = run_time_switch( intr );
				break;
			case LANGUAGE_SWITCH_NODE:
				DBG("DEBUG:run_cpl_script: processing language-switch node\n");
				intr->ip = run_language_switch( intr );
				break;
			case LOOKUP_NODE:
				DBG("DEBUG:run_cpl_script: processing lookup node\n");
				intr->ip = run_lookup( intr );
				break;
			case LOCATION_NODE:
				DBG("DEBUG:run_cpl_script: processing location node\n");
				intr->ip = run_location( intr );
				break;
			case REMOVE_LOCATION_NODE:
				DBG("DEBUG:run_cpl_script: processing remove_location node\n");
				intr->ip = run_remove_location( intr );
				break;
			case PROXY_NODE:
				DBG("DEBUG:run_cpl_script: processing proxy node\n");
				intr->ip = run_proxy( intr );
				break;
			case REJECT_NODE:
				DBG("DEBUG:run_cpl_script: processing reject node\n");
				intr->ip = run_reject( intr );
				break;
			case REDIRECT_NODE:
				DBG("DEBUG:run_cpl_script: processing redirect node\n");
				intr->ip = run_redirect( intr );
				break;
			case LOG_NODE:
				DBG("DEBUG:run_cpl_script: processing log node\n");
				intr->ip = run_log( intr );
				break;
			case MAIL_NODE:
				DBG("DEBUG:run_cpl_script: processing mail node\n");
				intr->ip = run_mail( intr );
				break;
			case SUB_NODE:
				DBG("DEBUG:run_cpl_script: processing sub node\n");
				intr->ip = run_sub( intr );
				break;
			default:
				LOG(L_ERR,"ERROR:run_cpl_script: unknown type node (%d)\n",
					NODE_TYPE(intr->ip));
				goto error;
		}

		if (intr->ip==CPL_RUNTIME_ERROR) {
			LOG(L_ERR,"ERROR:cpl_c:run_cpl_script: runtime error\n");
			return SCRIPT_RUN_ERROR;
		} else if (intr->ip==CPL_SCRIPT_ERROR) {
			LOG(L_ERR,"ERROR:cpl_c:run_cpl_script: script error\n");
			return SCRIPT_FORMAT_ERROR;
		} else if (intr->ip==DEFAULT_ACTION) {
			DBG("DEBUG:cpl_c:run_cpl_script: running default action\n");
			return run_default(intr);
		} else if (intr->ip==EO_SCRIPT) {
			DBG("DEBUG:cpl_c:run_cpl_script: script interpretation done!\n");
			return SCRIPT_END;
		} else if (intr->ip==CPL_TO_CONTINUE) {
			DBG("DEBUG:cpl_c:run_cpl_script: done for the moment; waiting "
				"after signaling!\n");
			return SCRIPT_TO_BE_CONTINUED;
		}
	}while(1);

error:
	return SCRIPT_FORMAT_ERROR;
}
