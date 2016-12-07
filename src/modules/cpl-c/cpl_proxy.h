/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * History:
 * -------
 * 2003-07-29: file created (bogdan)
 * 2004-06-14: flag CPL_IS_STATEFUL is set now immediately after the 
 *             transaction is created (bogdan)
 */

#include "../../modules/tm/h_table.h"
#include "../../parser/contact/parse_contact.h"


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
					LM_ERR("bad %llx hdr\n",_name_);\
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



static inline int parse_q(str *q, unsigned int *prio)
{
	if (q->s[0]=='0')
		*prio=0;
	else if (q->s[0]=='1')
		*prio=10;
	else
		goto error;
	if (q->s[1]!='.')
		goto error;
	if (q->s[2]<'0' || q->s[2]>'9')
		goto error;
	*prio += q->s[2] - '0';
	if (*prio>10)
		goto error;

	return 0;
error:
	LM_ERR("bad q param <%.*s>\n",q->len,q->s);
	return -1;
}



static inline int add_contacts_to_loc_set(struct sip_msg* msg,
													struct location **loc_set)
{
	struct sip_uri uri;
	struct contact *contacts;
	unsigned int prio;

	/* we need to have the contact header */
	if (msg->contact==0) {
		/* find and parse the Contact header */
		if ((parse_headers(msg, HDR_CONTACT_F, 0)==-1) || (msg->contact==0)) {
			LM_ERR("error parsing or no Contact hdr found!\n");
			goto error;
		}
	}

	/* extract from contact header the all the addresses */
	if (parse_contact( msg->contact )!=0) {
		LM_ERR("unable to parse Contact hdr!\n");
		goto error;
	}

	/* in contact hdr, in parsed attr, we should have a list of contacts */
	if ( msg->contact->parsed ) {
		contacts = ((struct contact_body*)msg->contact->parsed)->contacts;
		for( ; contacts ; contacts=contacts->next) {
			/* check if the contact is a valid sip uri */
			if (parse_uri( contacts->uri.s, contacts->uri.len , &uri)!=0) {
				continue;
			}
			/* convert the q param to int value (if any) */
			if (contacts->q) {
				if (parse_q( &(contacts->q->body), &prio )!=0)
					continue;
			} else {
				prio = 10; /* set default to minimum */
			}
			/* add the uri to location set */
			if (add_location(loc_set,&contacts->uri,0,prio,CPL_LOC_DUPL)!=0) {
				LM_ERR("unable to add <%.*s>\n",
					contacts->uri.len,contacts->uri.s);
			}
		}
	}

	return 0;
error:
	return -1;
}



static void reply_callback( struct cell* t, int type, struct tmcb_params* ps)
{
	struct cpl_interpreter *intr = (struct cpl_interpreter*)(*(ps->param));
	struct location        *loc  = 0;
	int rez;

	if (intr==0) {
		LM_WARN("param=0 for callback %d, transaction=%p \n",type,t);
		return;
	}

	if (type&TMCB_RESPONSE_OUT) {
		/* the purpose of the final reply is to trash down the interpreter
		 * structure! it's the safest place to do that, since this callback
		 * it's called only once per transaction for final codes (>=200) ;-) */
		if (ps->code>=200) {
			LM_DBG("code=%d, final reply received\n", ps->code);
			/* CPL interpretation done, call established -> destroy */
			free_cpl_interpreter( intr );
			/* set to zero the param callback*/
			*(ps->param) = 0;
		}
		return;
	} else if (! (type&TMCB_ON_FAILURE)) {
		LM_ERR("unknown type %d\n",type);
		goto exit;
	}

	LM_DBG("negativ reply received\n");

	intr->flags |= CPL_PROXY_DONE;
	intr->msg = ps->req;

	/* is the negative reply triggered by a cancel from UAC side? */
	if (was_cancelled(t)) {
		/* stop whole interpretation */
		return;
	}

	/* if it's a redirect-> do I have to added to the location set ? */
	if (intr->proxy.recurse && (ps->code)/100==3) {
		LM_DBG("recurse level %d processing..\n",intr->proxy.recurse);
		intr->proxy.recurse--;
		/* get the locations from the Contact */
		add_contacts_to_loc_set( ps->rpl, &(intr->loc_set));
		switch (intr->proxy.ordering) {
			case SEQUENTIAL_VAL:
				/* update the last_to_proxy to last location from set */
				if (intr->proxy.last_to_proxy==0) {
					/* the pointer went through entire old set -> set it to the
					 * updated set, from the beginning  */
					if (intr->loc_set==0)
						/* the updated set is also empty -> proxy ended */
						break;
					intr->proxy.last_to_proxy = intr->loc_set;
				}
				while(intr->proxy.last_to_proxy->next)
					intr->proxy.last_to_proxy=intr->proxy.last_to_proxy->next;
				break;
			case PARALLEL_VAL:
				/* push the whole new location set to be proxy */
				intr->proxy.last_to_proxy = intr->loc_set;
				break;
			case FIRSTONLY_VAL:
				intr->proxy.last_to_proxy = 0;
				break;
		}
	}

	/* the current proxying failed -> do I have another location to try ?
	 * This applies only for SERIAL forking or if RECURSE is set */
	if (intr->proxy.last_to_proxy && !(no_new_branches(t)) ) {
		/* continue proxying */
		LM_DBG("resuming proxying....\n");
		switch (intr->proxy.ordering) {
			case PARALLEL_VAL:
				/* I get here only if I got a 3xx and RECURSE in on ->
				 * forward to all location from location set */
				intr->proxy.last_to_proxy = 0;
				cpl_proxy_to_loc_set(intr->msg,&(intr->loc_set),intr->flags );
				break;
			case SEQUENTIAL_VAL:
				/* place a new branch to the next location from loc. set*/
				loc = remove_first_location( &(intr->loc_set) );
				/*print_location_set(intr->loc_set);*/
				/* update (if necessary) the last_to_proxy location  */
				if (intr->proxy.last_to_proxy==loc)
					intr->proxy.last_to_proxy = 0;
				cpl_proxy_to_loc_set(intr->msg,&loc,intr->flags );
				break;
			default:
				LM_CRIT("unexpected ordering found "
					"when continuing proxying (%d)\n",intr->proxy.ordering);
				goto exit;
		}
		/* nothing more to be done */
		return;
	} else {
		/* done with proxying.... -> process the final response */
		LM_DBG("final_reply: got a final %d\n",ps->code);
		intr->ip = 0;
		if (ps->code==486 || ps->code==600) {
			/* busy response */
			intr->ip = intr->proxy.busy;
		} else if (ps->code==408) {
			/* request timeout -> no response */
			intr->ip = intr->proxy.noanswer;
		} else if (((ps->code)/100)==3) {
			/* redirection */
			/* add to the location list all the addresses from Contact */
			add_contacts_to_loc_set( ps->rpl, &(intr->loc_set));
			print_location_set( intr->loc_set );
			intr->ip = intr->proxy.redirect;
		} else {
			/* generic failure */
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
			rez = cpl_run_script(intr);
		switch ( rez ) {
			case SCRIPT_END:
				/* we don't need to free the interpreter here since it will 
				 * be freed in the final_reply callback */
			case SCRIPT_TO_BE_CONTINUED:
				return;
			case SCRIPT_RUN_ERROR:
			case SCRIPT_FORMAT_ERROR:
				goto exit;
			default:
				LM_CRIT("improper result %d\n",
					rez);
				goto exit;
		}
	}

exit:
	/* in case of error the default response chosen by ser at the last
	 * proxying will be forwarded to the UAC */
	free_cpl_interpreter( intr );
	/* set to zero the param callback*/
	*(ps->param) = 0;
	return;
}



static inline char *run_proxy( struct cpl_interpreter *intr )
{
	unsigned short attr_name;
	unsigned short n;
	int_str is_val;
	char *kid;
	char *p;
	int i;
	str *s;
	struct location *loc;

	intr->proxy.ordering = PARALLEL_VAL;
	intr->proxy.recurse = (unsigned short)cpl_env.proxy_recurse;

	/* identify the attributes */
	for( i=NR_OF_ATTR(intr->ip),p=ATTR_PTR(intr->ip) ; i>0 ; i-- ) {
		get_basic_attr( p, attr_name, n, intr, script_error);
		switch (attr_name) {
			case TIMEOUT_ATTR:
				if (cpl_env.timer_avp.n || cpl_env.timer_avp.s.s) {
					is_val.n = n;
					if ( add_avp( cpl_env.timer_avp_type,
					cpl_env.timer_avp, is_val)<0) {
						LM_ERR("unable to set timer AVP\n");
						/* continue */
					}
				}
				break;
			case RECURSE_ATTR:
				switch (n) {
					case NO_VAL:
						intr->proxy.recurse = 0;
						break;
					case YES_VAL:
						/* already set as default */
						break;
					default:
						LM_ERR("invalid value (%u) found"
							" for attr. RECURSE in PROXY node!\n",n);
						goto script_error;
				}
				break;
			case ORDERING_ATTR:
				if (n!=PARALLEL_VAL && n!=SEQUENTIAL_VAL && n!=FIRSTONLY_VAL){
					LM_ERR("invalid value (%u) found"
						" for attr. ORDERING in PROXY node!\n",n);
					goto script_error;
				}
				intr->proxy.ordering = n;
				break;
			default:
				LM_ERR("unknown attribute (%d) in"
					"PROXY node\n",attr_name);
				goto script_error;
		}
	}

	intr->proxy.busy = intr->proxy.noanswer = 0;
	intr->proxy.redirect = intr->proxy.failure = intr->proxy.default_ = 0;

	/* this is quite an "expensive" node to run, so let's make some checking
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
				LM_ERR("unknown output node type"
					" (%d) for PROXY node\n",NODE_TYPE(kid));
				goto script_error;
		}
	}

	/* if the location set if empty, I will go directly on failure/default */
	if (intr->loc_set==0) {
		LM_DBG("location set found empty -> going on "
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
		/* user name is already in shared memory */
		/* requested URI - mandatory in SIP msg (cannot be STR_NOT_FOUND) */
		s = GET_RURI( intr->msg );
		duplicate_str( s , intr->ruri );
		intr->flags |= CPL_RURI_DUPLICATED;
		/* TO header - mandatory in SIP msg (cannot be STR_NOT_FOUND) */
		if (!intr->to) {
			if (!intr->msg->to &&
			(parse_headers(intr->msg,HDR_TO_F,0)==-1 || !intr->msg->to)) {
				LM_ERR("bad msg or missing TO header\n");
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
			if (parse_from_header( intr->msg )<0)
				goto runtime_error;
			s = &(get_from(intr->msg)->uri);
		} else {
			s = intr->from;
		}
		duplicate_str( s , intr->from );
		intr->flags |= CPL_FROM_DUPLICATED;
		/* SUBJECT header - optional in SIP msg (can be STR_NOT_FOUND) */
		if (intr->subject!=STR_NOT_FOUND) {
			search_and_duplicate_hdr(intr,subject,HDR_SUBJECT_F,s);
			if (intr->subject!=STR_NOT_FOUND)
				intr->flags |= CPL_SUBJECT_DUPLICATED;
		}
		/* ORGANIZATION header - optional in SIP msg (can be STR_NOT_FOUND) */
		if ( intr->organization!=STR_NOT_FOUND) {
			search_and_duplicate_hdr(intr,organization,HDR_ORGANIZATION_F,s);
			if ( intr->organization!=STR_NOT_FOUND)
				intr->flags |= CPL_ORGANIZATION_DUPLICATED;
		}
		/* USER_AGENT header - optional in SIP msg (can be STR_NOT_FOUND) */
		if (intr->user_agent!=STR_NOT_FOUND) {
			search_and_duplicate_hdr(intr,user_agent,HDR_USERAGENT_F,s);
			if (intr->user_agent!=STR_NOT_FOUND)
				intr->flags |= CPL_USERAGENT_DUPLICATED;
		}
		/* ACCEPT_LANGUAGE header - optional in SIP msg
		 * (can be STR_NOT_FOUND) */
		if (intr->accept_language!=STR_NOT_FOUND) {
			search_and_duplicate_hdr(intr,accept_language,
				HDR_ACCEPTLANGUAGE_F,s);
			if (intr->accept_language!=STR_NOT_FOUND)
				intr->flags |= CPL_ACCEPTLANG_DUPLICATED;
		}
		/* PRIORITY header - optional in SIP msg (can be STR_NOT_FOUND) */
		if (intr->priority!=STR_NOT_FOUND) {
			search_and_duplicate_hdr(intr,priority,HDR_PRIORITY_F,s);
			if (intr->priority!=STR_NOT_FOUND)
				intr->flags |= CPL_PRIORITY_DUPLICATED;
		}

		/* now is the first time doing proxy, so I can still be stateless;
		 * as proxy is done all the time stateful, I have to switch from
		 * stateless to stateful if necessary.  */
		if ( !(intr->flags&CPL_IS_STATEFUL) ) {
			i = cpl_fct.tmb.t_newtran( intr->msg );
			if (i<0) {
				LM_ERR("failed to build new transaction!\n");
				goto runtime_error;
			} else if (i==0) {
				LM_ERR("processed INVITE is a retransmission!\n");
				/* instead of generating an error is better just to break the
				 * script by returning EO_SCRIPT */
				return EO_SCRIPT;
			}
			intr->flags |= CPL_IS_STATEFUL;
		}

		/* as I am interested in getting the responses back - I need to install
		 * some callback functions for replies  */
		if (cpl_fct.tmb.register_tmcb(intr->msg,0,
		TMCB_ON_FAILURE|TMCB_RESPONSE_OUT,reply_callback,(void*)intr,0) <= 0 ) {
			LM_ERR("failed to register TMCB_RESPONSE_OUT callback\n");
			goto runtime_error;
		}
	}

	switch (intr->proxy.ordering) {
		case FIRSTONLY_VAL:
			/* forward the request only to the first address from loc. set */
			/* location set cannot be empty -> was checked before */
			loc = remove_first_location( &(intr->loc_set) );
			intr->proxy.last_to_proxy = 0;
			/* set the new ip before proxy -> otherwise race cond with rpls */
			intr->ip = CPL_TO_CONTINUE;
			if (cpl_proxy_to_loc_set(intr->msg,&loc,intr->flags )==-1)
				goto runtime_error;
			break;
		case PARALLEL_VAL:
			/* forward to all location from location set */
			intr->proxy.last_to_proxy = 0;
			/* set the new ip before proxy -> otherwise race cond with rpls */
			intr->ip = CPL_TO_CONTINUE;
			if (cpl_proxy_to_loc_set(intr->msg,&(intr->loc_set),intr->flags)
			==-1)
				goto runtime_error;
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
			/* set the new ip before proxy -> otherwise race cond with rpls */
			intr->ip = CPL_TO_CONTINUE;
			if (cpl_proxy_to_loc_set(intr->msg,&loc,intr->flags)==-1)
				goto runtime_error;
			break;
	}

	return CPL_TO_CONTINUE;
script_error:
	return CPL_SCRIPT_ERROR;
mem_error:
	LM_ERR("no more free shm memory\n");
runtime_error:
	return CPL_RUNTIME_ERROR;
}
