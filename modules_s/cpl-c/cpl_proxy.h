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
 *
 * History:
 * -------
 * 2003-07-29: file created (bogdan)
 */

#include "../tm/h_table.h"


#define duplicate_str( _orig_ , _new_ ) \
	do {\
		(_new_) = (str*)shm_malloc(sizeof(str)+(_orig_)->len);\
		if (!(_new_)) goto mem_error;\
		(_new_)->len = (_orig_)->len;\
		(_new_)->s = (char*)((_new_))+sizeof(str);\
		memcpy((_new_)->s,(_orig_)->s,(_orig_)->len);\
	} while(0)

#define search_and_duplicate_hdr( _intr_ , _field_ , _name_ , _sfoo_ ) \
	do {\
		if (!(_intr_)->_field_) {\
			if (!(_intr_)->msg->_field_) { \
				if (parse_headers((_intr_)->msg,_name_,0)==-1) {\
					LOG(L_ERR,"ERROR:run_proxy: bad %u hdr\n",_name_);\
					goto runtime_error;\
				} else if ( !(_intr_)->msg->_field_) {\
					(_intr_)->_field_ = STR_NOT_FOUND;\
				} else {\
					(_sfoo_) = &((_intr_)->msg->_field_->body);\
					duplicate_str( (_sfoo_) , (_intr_)->_field_ );\
				}\
			} else {\
				(_sfoo_) = &((_intr_)->msg->_field_->body);\
				duplicate_str( (_sfoo_) , (_intr_)->_field_ );\
			}\
		} else {\
			(_sfoo_) = (_intr_)->_field_;\
			duplicate_str( (_sfoo_) , (_intr_)->_field_ );\
		}\
	}while(0)




static void failed_reply( struct cell* t, struct sip_msg* msg, int code,
																void *param )
{
	struct cpl_interpreter *intr = (struct cpl_interpreter*)param;
	struct location        *loc  = 0;
	int rez;

	DBG("DEBUG:cpl-c:failed_reply: ------------------------------>\n"
		" ---------------> failed reply from proxy was sent\n");

	intr->flags |= CPL_PROXY_DONE;
	intr->msg = t->uas.request;

	/* if it's a redirect-> do I have to added to the location set ? */
	/* TO DO
	 */

	/* the current proxing failed -> do I have another location to try ?
	 * This applyes only for SERIAL forking or if RECURSE is set */
	if (intr->proxy.last_to_proxy) {
		/* continue proxying */
		DBG("DEBUG:cpl-c:failed_reply: resuming proxying....\n");
		switch (intr->proxy.ordering) {
			case PARALLEL_VAL:
				/* I get here only if I got a 3xx and RECURSE in on ->
				 * forward to all location from location set */
				intr->proxy.last_to_proxy = 0;
				cpl_proxy_to_loc_set(intr->msg,&(intr->loc_set),intr->flags );
				break;
			case SEQUENTIAL_VAL:
				/* place a new brach to the next location from loc. set*/
				loc = remove_first_location( &(intr->loc_set) );
				/*print_location_set(intr->loc_set);*/
				/* update (if necessary) the last_to_proxy location  */
				if (intr->proxy.last_to_proxy==loc)
					intr->proxy.last_to_proxy = 0;
				cpl_proxy_to_loc_set(intr->msg,&loc,intr->flags );
				break;
			default:
				LOG(L_CRIT,"BUG:cpl_c:failed_reply: unexpected ordering found "
					"when continuing proxying (%d)\n",intr->proxy.ordering);
				goto error;
		}
	} else {
		/* done with proxying.... -> process the final response */
		DBG("DEBUG:cpl-c:failed_reply:final_reply: got a final %d\n",code);
		intr->ip = 0;
		if (code==486 || code==600) {
			/* busy response */
			intr->ip = intr->proxy.busy;
		} else if (code==408) {
			/* request timeout -> no response */
			intr->ip = intr->proxy.noanswer;
		} else if ((code/100)==3) {
			/* redirection */
			intr->ip = intr->proxy.redirect;
		} else {
			intr->ip = intr->proxy.failure;
		}

		if (intr->ip==0)
			intr->ip = (intr->proxy.default_)?
				intr->proxy.default_:DEFAULT_ACTION;
		if (intr->ip!=DEFAULT_ACTION)
			intr->ip = get_first_child( intr->ip );

		if( intr->ip==DEFAULT_ACTION)
			rez = run_default(intr);
		else
			rez = run_cpl_script(intr);
		switch ( rez ) {
			case SCRIPT_END:
			case SCRIPT_TO_BE_CONTINUED:
				return;
			case SCRIPT_RUN_ERROR:
			case SCRIPT_FORMAT_ERROR:
				goto error;
			default:
				LOG(L_CRIT,"BUG:cpl-c:failed_reply: improper rezult %d\n",
					rez);
				goto error;
		}
	}

error:
	/* in case of error the default response choosed by ser at the last
	 * proxying will be forwarded to the UAC */
	return;
}



/* the purpose of the final reply is to trash down the interpreter structure!
 * it's the safest place to do that, since this callback it's called only once
 * per transaction ;-) */
static void final_reply( struct cell* t, struct sip_msg* msg, int code,
																void *param )
{
	DBG("DEBUG:cpl-c:failed_reply: ----------------------------->\n"
		" ---------------> final reply from proxy received\n");

	free_cpl_interpreter( (struct cpl_interpreter*)param );
}




inline unsigned char *run_proxy( struct cpl_interpreter *intr )
{
	unsigned char *kid;
	unsigned char *p;
	unsigned char attr;
	int i;
	str *s;
	struct location *loc;

	intr->proxy.ordering = PARALLEL_VAL;
	intr->proxy.recurse = YES_VAL;

	/* indentify the attributes */
	for( i=NR_OF_ATTR(intr->ip),p=ATTR_PTR(intr->ip) ; i>0 ; i-- ) {
		switch (*p) {
			case TIMEOUT_ATTR:
				check_overflow_by_ptr( p+2, intr, script_error);
				attr = *((unsigned short*)(p+1));
				p += 3;
				break;
			case RECURSE_ATTR:
				check_overflow_by_ptr( p+1, intr, script_error);
				attr = (unsigned char)(*(p+1));
				if ( attr!=NO_VAL && attr!=YES_VAL ) {
					LOG(L_ERR,"ERROR:run_proxy: invalid value (%u) found"
						" for attr. RECURSE in PROXY node!\n",attr);
					goto script_error;
				}
				intr->proxy.recurse = attr;
				p += 2;
				break;
			case ORDERING_ATTR:
				check_overflow_by_ptr( p+1, intr, script_error);
				attr = (unsigned char)(*(p+1));
				if (attr!=PARALLEL_VAL && attr!=SEQUENTIAL_VAL &&
				attr!=FIRSTONLY_VAL){
					LOG(L_ERR,"ERROR:run_proxy: invalid value (%u) found"
						" for attr. ORDERING in PROXY node!\n",attr);
					goto script_error;
				}
				intr->proxy.ordering = attr;
				p += 2;
				break;
			default:
				LOG(L_ERR,"ERROR:run_proxy: unknown attribute (%d) in"
					"PROXY node\n",*p);
				goto script_error;
		}
	}

	intr->proxy.busy = intr->proxy.noanswer = 0;
	intr->proxy.redirect = intr->proxy.failure = intr->proxy.default_ = 0;

	/* this is quite an "expensiv" node to run, so let's maek some checkings
	 * before getting deeply into it */
	for( i=0 ; i<NR_OF_KIDS(intr->ip) ; i++ ) {
		kid = intr->ip + KID_OFFSET(intr->ip,i);
		check_overflow_by_ptr( kid+SIMPLE_NODE_SIZE(kid), intr, script_error);
		switch ( NODE_TYPE(kid) ) {
			case BUSY_NODE :
				intr->proxy.busy = kid;
				break;
			case NOANSWER_NODE:
				intr->proxy.noanswer = kid;
				break;
			case REDIRECTION_NODE:
				intr->proxy.redirect = kid;
				break;
			case FAILURE_NODE:
				intr->proxy.failure = kid;
				break;
			case DEFAULT_NODE:
				intr->proxy.default_ = kid;
				break;
			default:
				LOG(L_ERR,"ERROR:run_proxy: unknown output node type"
					" (%d) for PROXY node\n",NODE_TYPE(kid));
				goto script_error;
		}
	}

	/* if the location set if empty, I will go directly on failure/default */
	if (intr->loc_set==0) {
		DBG("DEBUG:run_proxy: location set found empty -> going on "
			"failure/default branch\n");
			if (intr->proxy.failure)
				return get_first_child(intr->proxy.failure);
			else if (intr->proxy.default_)
				return get_first_child(intr->proxy.default_);
			else return DEFAULT_ACTION;
	}

	/* if it's the first execution of a proxy node, force parsing of the needed
	 * headers and duplicate them in shared memory */
	if (!(intr->flags&CPL_PROXY_DONE)) {
		/* requested URI - mandatory in SIP msg (cannot be STR_NOT_FOUND) */
		s = GET_RURI( intr->msg );
		duplicate_str( s , intr->ruri );
		intr->flags |= CPL_RURI_DUPLICATED;
		/* TO header - mandatory in SIP msg (cannot be STR_NOT_FOUND) */
		if (!intr->to) {
			if (!intr->msg->to &&
			(parse_headers(intr->msg,HDR_TO,0)==-1 || !intr->msg->to)) {
				LOG(L_ERR,"ERROR:run_proxy: bad msg or missing TO header\n");
				goto runtime_error;
			}
			s = &(get_to(intr->msg)->uri);
		} else {
			s = intr->to;
		}
		duplicate_str( s , intr->to );
		intr->flags |= CPL_TO_DUPLICATED;
		/* FROM header - mandatory in SIP msg (cannot be STR_NOT_FOUND) */
		if (!intr->from) {
			if (parse_from_header( intr->msg )==-1)
				goto runtime_error;
			s = &(get_from(intr->msg)->uri);
		} else {
			s = intr->from;
		}
		duplicate_str( s , intr->from );
		intr->flags |= CPL_FROM_DUPLICATED;
		/* SUBJECT header - optional in SIP msg (can be STR_NOT_FOUND) */
		if (intr->subject!=STR_NOT_FOUND) {
			search_and_duplicate_hdr(intr,subject,HDR_SUBJECT,s);
			if (intr->subject!=STR_NOT_FOUND)
				intr->flags |= CPL_SUBJECT_DUPLICATED;
		}
		/* ORGANIZATION header - optional in SIP msg (can be STR_NOT_FOUND) */
		if ( intr->organization!=STR_NOT_FOUND) {
			search_and_duplicate_hdr(intr,organization,HDR_ORGANIZATION,s);
			if ( intr->organization!=STR_NOT_FOUND)
				intr->flags |= CPL_ORGANIZATION_DUPLICATED;
		}
		/* USER_AGENT header - optional in SIP msg (can be STR_NOT_FOUND) */
		if (intr->user_agent!=STR_NOT_FOUND) {
			search_and_duplicate_hdr(intr,user_agent,HDR_USERAGENT,s);
			if (intr->user_agent!=STR_NOT_FOUND)
				intr->flags |= CPL_USERAGENT_DUPLICATED;
		}
		/* ACCEPT_LANGUAG header - optional in SIP msg
		 * (can be STR_NOT_FOUND) */
		if (intr->accept_language!=STR_NOT_FOUND) {
			search_and_duplicate_hdr(intr,accept_language,
				HDR_ACCEPTLANGUAGE,s);
			if (intr->accept_language!=STR_NOT_FOUND)
				intr->flags |= CPL_ACCEPTLANG_DUPLICATED;
		}
		/* PRIORITY header - optional in SIP msg (can be STR_NOT_FOUND) */
		if (intr->priority!=STR_NOT_FOUND) {
			search_and_duplicate_hdr(intr,priority,HDR_PRIORITY,s);
			if (intr->priority!=STR_NOT_FOUND)
				intr->flags |= CPL_PRIORITY_DUPLICATED;
		}
	}

	/* as I am interested in getting the responses back - I need to install
	 * some callback functions for replies  */
	if (cpl_tmb.register_req_cb( intr->msg, TMCB_RESPONSE_OUT, final_reply,
	intr) <= 0 ) {
		LOG(L_ERR, "ERROR:cpl_c:run_proxy: failed to register "
			"TMCB_RESPONSE_OUT callback\n");
		goto runtime_error;
	}
	if (cpl_tmb.register_req_cb( intr->msg, TMCB_ON_FAILURE, failed_reply,
	intr) <= 0 ) {
		LOG(L_ERR, "ERROR:cpl_c:run_proxy: failed to register "
			"TMCB_ON_FAILURE callback\n");
		goto runtime_error;
	}

	switch (intr->proxy.ordering) {
		case FIRSTONLY_VAL:
			/* forward the request only to the first address from loc. set */
			/* location set cannot be empty -> was checked before */
			loc = remove_first_location( &(intr->loc_set) );
			intr->proxy.last_to_proxy = 0;
			cpl_proxy_to_loc_set(intr->msg,&loc,intr->flags );
			break;
		case PARALLEL_VAL:
			/* forward to all location from location set */
			intr->proxy.last_to_proxy = 0;
			cpl_proxy_to_loc_set(intr->msg,&(intr->loc_set),intr->flags );
			break;
		case SEQUENTIAL_VAL:
			/* forward the request one at the time to all addresses from
			 * loc. set; location set cannot be empty -> was checked before */
			/* use the first location from set */
			loc = remove_first_location( &(intr->loc_set) );
			/* set as the last_to_proxy the last location from set */
			intr->proxy.last_to_proxy = intr->loc_set;
			while (intr->proxy.last_to_proxy&&intr->proxy.last_to_proxy->next)
				intr->proxy.last_to_proxy = intr->proxy.last_to_proxy->next;
			cpl_proxy_to_loc_set(intr->msg,&loc,intr->flags );
			break;
	}

	return CPL_TO_CONTINUE;
script_error:
	return CPL_SCRIPT_ERROR;
mem_error:
	LOG(L_ERR,"ERROR:run_proxy: no more free shm memory\n");
runtime_error:
	return CPL_RUNTIME_ERROR;
}



