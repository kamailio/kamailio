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
#include "CPL_tree.h"
#include "loc_set.h"
#include "cpl_utils.h"
#include "cpl_run.h"


#define EO_SCRIPT            ((unsigned char*)0xffffffff)
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


extern int (*sl_send_rpl)(struct sip_msg*, char*, char*);


/* include all inline functions for processing the switches */
#include "cpl_switches.h"




struct cpl_interpreter* build_cpl_interpreter( struct sip_msg *msg,
											str *script, unsigned int type)
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
	intr->type = (type==CPL_INCOMING_TYPE)?INCOMING_NODE:OUTGOING_NODE;

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
	if (intr) {
		if (intr->script.s)
			shm_free( intr->script.s);
		empty_location_set( &(intr->loc_set) );
		shm_free(intr);
	}
}



inline unsigned char *run_cpl_node( struct cpl_interpreter *intr )
{
	unsigned char *kid;
	int i;

	for(i=0;i<NR_OF_KIDS(intr->ip);i++) {
		kid= intr->ip + KID_OFFSET(intr->ip,i);
		if ( NODE_TYPE(kid)==intr->type ) {
			return ((NR_OF_KIDS(kid)==0)?EO_SCRIPT:kid+KID_OFFSET(kid,0));
		} else if (NODE_TYPE(kid)==SUBACTION_NODE ||
		NODE_TYPE(kid)==ANCILLARY_NODE || NODE_TYPE(kid)==INCOMING_NODE ||
		NODE_TYPE(kid)==OUTGOING_NODE ) {
			continue;
		} else {
			LOG(L_ERR,"ERROR:run_cpl_node: unknown child type (%d) "
				"for CPL node!!\n",NODE_TYPE(kid));
			goto error;
		}
	}

	LOG(L_ERR,"ERROR:run_cpl_node: bad CPL node - subnode %d not found",
		intr->type);
error:
	return 0;
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

	for( i=NR_OF_ATTR(intr->ip),p=ATTR_PTR(intr->ip) ; i>0 ; i-- ) {
		switch (*p) {
			case URL_ATTR:
				check_overflow_by_ptr( p+2, intr, error);
				url.len = *((unsigned short*)(p+1));
				check_overflow_by_ptr( p+2+url.len, intr, error);
				url.s = ((url.len)?(p+3):0);
				p += 3+url.len;
				break;
			case PRIORITY_ATTR:
				check_overflow_by_ptr( p+1, intr, error);
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
				check_overflow_by_ptr( p+1, intr, error);
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
				goto error;
		}
	}

	if (url.s==(char*)UNDEF_CHAR) {
		LOG(L_ERR,"ERROR:run_location: param. URL missing in LOCATION node\n");
		goto error;
	}

	if (clear)
		empty_location_set( &(intr->loc_set) );
	if (add_location( &(intr->loc_set), url.s, url.len, prio )==-1) {
		LOG(L_ERR,"ERROR:run_location: unable to add location to set :-(\n");
		goto error;
	}

	return ((NR_OF_KIDS(intr->ip)==0)?EO_SCRIPT:
		(intr->ip+KID_OFFSET(intr->ip,0)));
error:
	return 0;
}



inline unsigned char *run_remove_location( struct cpl_interpreter *intr )
{
	unsigned char *p;
	str url;
	int i;

	url.s = (char*)UNDEF_CHAR;

	for( i=NR_OF_ATTR(intr->ip),p=ATTR_PTR(intr->ip) ; i>0 ; i-- ) {
		switch (*p) {
			case LOCATION_ATTR:
				check_overflow_by_ptr( p+2, intr, error);
				url.len = *((unsigned short*)(p+1));
				check_overflow_by_ptr( p+2+url.len, intr, error);
				url.s = ((url.len)?(p+3):0);
				p += 3+url.len;
				break;
			default:
				LOG(L_WARN,"WARNING:run_remove_location: unknown attribute "
					"(%d) in REMOVE_LOCATION node -> ignoring..\n",*p);
				goto error;
		}
	}

	if (url.s==(char*)UNDEF_CHAR || url.len==0) {
		DBG("DEBUG:run_remove_location: remove al llocs from loc_set\n");
		empty_location_set( &(intr->loc_set) );
	} else {
		remove_location( &(intr->loc_set), url.s, url.len );
	}

	return ((NR_OF_KIDS(intr->ip)==0)?EO_SCRIPT:
		(intr->ip+KID_OFFSET(intr->ip,0)));
error:
	return 0;
}



inline unsigned char *run_sub( struct cpl_interpreter *intr )
{
	unsigned char  *p;
	unsigned short offset;
	int i;

	/* check the number of attr */
	i = NR_OF_ATTR( intr->ip );
	if (i!=1) {
		LOG(L_ERR,"ERROR:cpl_c:run_sub: incorrect nr. of attr. %d (<>1) in "
			"SUB node\n",i);
		goto error;
	}
	/* get attr's name */
	p = ATTR_PTR(intr->ip);
	check_overflow_by_ptr( p+2, intr, error);
	if (*p!=REF_ATTR) {
		LOG(L_ERR,"ERROR:cpl_c:run_sub: invalid attr. %d (expected %d)in "
			"SUB node\n", *p, REF_ATTR);
		goto error;
	}
	/* get the attr's value */
	offset = *((unsigned short*)(p+1));
	/* make the jump */
	p = intr->ip - offset;
	/* check the destination pointer -> are we still inside the buffer ;-) */
	if (((char*)p)<intr->script.s) {
		LOG(L_ERR,"ERROR:cpl_c:run_sub: jump offset lower than the script "
			"beginning -> underflow!\n");
		goto error;
	}
	check_overflow_by_ptr( p+SIMPLE_NODE_SIZE(intr->ip), intr, error);
	/* check to see if we hit a subaction node */
	if ( NODE_TYPE(p)!=SUBACTION_NODE ) {
		LOG(L_ERR,"ERROR:cpl_c:run_sub: sub. jump hit a nonsubaction node!\n");
		goto error;
	}
	if (NR_OF_KIDS(p)!=1 || NR_OF_ATTR(p)!=0 ) {
		LOG(L_ERR,"ERROR:cpl_c:run_sub: inavlid subaction node reached "
		"(kids=%d,attrs=%d); expected (1,0)!\n",NR_OF_KIDS(p),NR_OF_ATTR(p));
		goto error;
	}

	return ((NR_OF_KIDS(p)==0)?EO_SCRIPT:(p+KID_OFFSET(p,0)));
error:
	return 0;
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
		goto error;
	}

	for( i=NR_OF_ATTR(intr->ip),p=ATTR_PTR(intr->ip) ; i>0 ; i-- ) {
		switch (*p) {
			case STATUS_ATTR:
				check_overflow_by_ptr( p+2, intr, error);
				status = *((unsigned short*)(p+1));
				p += 3;
				break;
			case REASON_ATTR:
				check_overflow_by_ptr( p+2, intr, error);
				reason_len = *((unsigned short*)(p+1));
				check_overflow_by_ptr( p+2+reason_len, intr, error);
				reason_s = p+3;
				p += 3+reason_len;
				break;
			default:
				LOG(L_WARN,"WARNING:run_reject: unknown attribute "
					"(%d) in REJECT node -> ignoring..\n",*p);
				goto error;
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
				goto error;
		}
	}

	if (sl_send_rpl( intr->msg, (char*)(int)status, reason_s )<0) {
		LOG(L_ERR,"ERROR:run_reject: unable to send reject reply!\n");
		goto error;
	}

	return EO_SCRIPT;
error:
	return 0;
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
		goto error;
	}

	/* read the attributes of the REDIRECT node*/
	for( i=NR_OF_ATTR(intr->ip),p=ATTR_PTR(intr->ip) ; i>0 ; i-- ) {
		switch (*p) {
			case PERMANENT_ATTR:
				check_overflow_by_ptr( p+1, intr, error);
				permanent = *(p+1);
				if (permanent!=YES_VAL && permanent!=NO_VAL) {
					LOG(L_ERR,"ERROR:run_redirect: unsupported value (%d) "
						"in attribute PERMANENT for REDIRECT node",permanent);
					goto error;
				}
				p += 2;
				break;
			default:
				LOG(L_WARN,"WARNING:run_redirect: unknown attribute "
					"(%d) in REDIRECT node -> ignoring..\n",*p);
				goto error;
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
		LOG(L_ERR,"ERROR:cpl_redirect: out of pkg memory!\n");
		goto error;
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
		goto error;
	}
	add_lump_rpl( intr->msg , lump );

	/* send the reply */
	if (permanent)
		i = sl_send_rpl( intr->msg, (char*)301, "Moved permanently" );
	else
		i = sl_send_rpl( intr->msg, (char*)302, "Moved temporarily" );
	if (i<0) {
		LOG(L_ERR,"ERROR:run_redirect: unable to send redirect reply!\n");
		goto error;
	}

	return EO_SCRIPT;
error:
	return 0;
}





int run_cpl_script( struct cpl_interpreter *intr )
{
	do {
		check_overflow_by_offset( SIMPLE_NODE_SIZE(intr->ip), intr, error);
		switch ( NODE_TYPE(intr->ip) ) {
			case CPL_NODE:
				DBG("DEBUG:run_cpl_script:processing CPL node \n");
				intr->ip = run_cpl_node( intr );
				break;
			case ADDRESS_SWITCH_NODE:
				DBG("DEBUG:run_cpl_script:processing address-switch node\n");
				intr->ip = run_address_switch( intr );
				break;
			case STRING_SWITCH_NODE:
				DBG("DEBUG:run_cpl_script:processing string-switch node\n");
				intr->ip = run_string_switch( intr );
				break;
			case PRIORITY_SWITCH_NODE:
				DBG("DEBUG:run_cpl_script:processing priority-switch node\n");
				intr->ip = run_priority_switch( intr );
				break;
			case TIME_SWITCH_NODE:
				DBG("DEBUG:run_cpl_script:processing time-switch node\n");
				intr->ip = run_time_switch( intr );
				break;
			case LOCATION_NODE:
				DBG("DEBUG:run_cpl_script:processing location node\n");
				intr->ip = run_location( intr );
				break;
			case REMOVE_LOCATION_NODE:
				DBG("DEBUG:run_cpl_script:processing remove_location node\n");
				intr->ip = run_remove_location( intr );
				break;
			case REJECT_NODE:
				DBG("DEBUG:run_cpl_script:processing reject node\n");
				intr->ip = run_reject( intr );
				break;
			case REDIRECT_NODE:
				DBG("DEBUG:run_cpl_script:processing redirect node\n");
				intr->ip = run_redirect( intr );
				break;
			case SUB_NODE:
				DBG("DEBUG:run_cpl_script:processing sub node\n");
				intr->ip = run_sub( intr );
				break;
			default:
				LOG(L_ERR,"ERROR:run_cpl_script: unknown type node (%d)\n",
					NODE_TYPE(intr->ip));
				goto error;
		}
		if (intr->ip==0)
			goto error;
	}while(intr->ip!=EO_SCRIPT);

	DBG("DEBUG:run_cpl_script: script interpretation done!\n");
	return SCRIPT_END;
error:
	LOG(L_ERR,"ERROR:run_cpl_script: generic error\n");
	return -1;
}
