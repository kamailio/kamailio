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


inline unsigned char *run_proxy( struct cpl_interpreter *intr )
{
	unsigned char *kid;
	unsigned char *p;
	int i;
	str *s;
	/* attributes (init.-ed with default values) */
	unsigned char timeout_attr = 20;
	unsigned char recurse_attr = YES_VAL;
	unsigned char ordering_attr = PARALLEL_VAL;
	/* subnodes */
	unsigned char *failure_kid = 0;
	unsigned char *default_kid = 0;

	/* indentify the attributes */
	for( i=NR_OF_ATTR(intr->ip),p=ATTR_PTR(intr->ip) ; i>0 ; i-- ) {
		switch (*p) {
			case TIMEOUT_ATTR:
				check_overflow_by_ptr( p+2, intr, script_error);
				timeout_attr = *((unsigned short*)(p+1));
				p += 3;
				break;
			case RECURSE_ATTR:
				check_overflow_by_ptr( p+1, intr, script_error);
				recurse_attr = (unsigned char)(*(p+1));
				if ( recurse_attr!=NO_VAL && recurse_attr!=YES_VAL ) {
					LOG(L_ERR,"ERROR:run_proxy: invalid value (%u) found"
						" for attr. RECURSE in PROXY node!\n",recurse_attr);
					goto script_error;
				}
				p += 2;
				break;
			case ORDERING_ATTR:
				check_overflow_by_ptr( p+1, intr, script_error);
				ordering_attr = (unsigned char)(*(p+1));
				if (ordering_attr!=PARALLEL_VAL &&
				ordering_attr!=SEQUENTIAL_VAL && ordering_attr!=FIRSTONLY_VAL){
					LOG(L_ERR,"ERROR:run_proxy: invalid value (%u) found"
						" for attr. ORDERING in PROXY node!\n",ordering_attr);
					goto script_error;
				}
				p += 2;
				break;
			default:
				LOG(L_ERR,"ERROR:run_proxy: unknown attribute (%d) in"
					"PROXY node\n",*p);
				goto script_error;
		}
	}

	/* this is quite an "expensiv" node to run, so let's maek some checkings
	 * before getting deeply into it */
	for( i=0 ; i<NR_OF_KIDS(intr->ip) ; i++ ) {
		kid = intr->ip + KID_OFFSET(intr->ip,i);
		check_overflow_by_ptr( kid+SIMPLE_NODE_SIZE(kid), intr, script_error);
		switch ( NODE_TYPE(kid) ) {
			case BUSY_NODE :
			case NOANSWER_NODE:
			case REDIRECTION_NODE:
				break;
			case FAILURE_NODE:
				failure_kid = kid;
				break;
			case DEFAULT_NODE:
				default_kid = kid;
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
			if (failure_kid)
				return get_first_child(failure_kid);
			else if (default_kid)
				return get_first_child(default_kid);
			else return DEFAULT_ACTION;
	}

	/* if it's the first execution of a proxy node, force parsing of the needed
	 * headers */
	if (!(intr->flags&CPL_PROXY_DONE)) {
		/* requested URI - mandatory in SIP msg (cannot be STR_NOT_FOUND) */
		s = GET_RURI( intr->msg );
		duplicate_str( s , intr->ruri );
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
		/* FROM header - mandatory in SIP msg (cannot be STR_NOT_FOUND) */
		if (!intr->from) {
			if (parse_from_header( intr->msg )==-1)
				goto runtime_error;
			s = &(get_from(intr->msg)->uri);
		} else {
			s = intr->from;
		}
		duplicate_str( s , intr->from );
		/* SUBJECT header - optional in SIP msg (can be STR_NOT_FOUND) */
		if (intr->subject!=STR_NOT_FOUND) {
			search_and_duplicate_hdr(intr,subject,HDR_SUBJECT,s);
		}
		/* ORGANIZATION header - optional in SIP msg (can be STR_NOT_FOUND) */
		if (intr->organization!=STR_NOT_FOUND) {
			search_and_duplicate_hdr(intr,organization,HDR_ORGANIZATION,s);
		}
		/* USER_AGENT header - optional in SIP msg (can be STR_NOT_FOUND) */
		if (intr->user_agent!=STR_NOT_FOUND) {
			search_and_duplicate_hdr(intr,user_agent,HDR_USERAGENT,s);
		}
		/* ACCEPT_LANGUAG header - optional in SIP msg
		 * (can be STR_NOT_FOUND) */
		if (intr->accept_language!=STR_NOT_FOUND) {
			search_and_duplicate_hdr(intr,accept_language,
				HDR_ACCEPTLANGUAGE,s);
		}
		/* PRIORITY header - optional in SIP msg (can be STR_NOT_FOUND) */
		if (intr->priority!=STR_NOT_FOUND) {
			search_and_duplicate_hdr(intr,priority,HDR_PRIORITY,s);
		}
	}

	return DEFAULT_ACTION;
script_error:
	return CPL_SCRIPT_ERROR;
mem_error:
	LOG(L_ERR,"ERROR:run_proxy: no more free shm memory\n");
runtime_error:
	return CPL_RUNTIME_ERROR;
}
