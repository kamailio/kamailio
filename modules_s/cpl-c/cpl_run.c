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
#include "../../parser/parse_from.h"
#include "../../parser/parse_uri.c"
#include "../../data_lump_rpl.h"
#include "CPL_tree.h"
#include "loc_set.h"
#include "cpl_utils.h"
#include "cpl_run.h"


#define MAX_SIZE_STATIC_BUF  256
#define EO_SCRIPT            ((unsigned char*)0xffffffff)
#define HDR_NOT_FOUND        ((char*)0xffffffff)
#define UNDEF_CHAR           (0xff)

#define check_overflow_by_ptr(_ptr_,_intr_,_error_) \
	do {\
		if ( (char*)(_ptr_)>(_intr_)->script.len+(_intr_)->script.s ) \
			goto _error_; \
	}while(0)

#define check_overflow_by_offset(_len_,_intr_,_error_) \
	do {\
		if ( (char*)((_intr_)->ip+(_len_)) > \
		(_intr_)->script.len+(_intr_)->script.s ) \
			goto _error_; \
	}while(0)


extern int (*sl_send_rpl)(struct sip_msg*, char*, char*);



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
		NODE_TYPE(kid)==ANCILLARY_NODE) {
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



inline unsigned char *run_address_switch( struct cpl_interpreter *intr )
{
	unsigned char field, subfield;
	unsigned char *p;
	unsigned char *kid;
	int i;
	str cpl_val;
	str *msg_val;
	str *uri;
	struct sip_uri parsed_uri;

	field = subfield = UNDEF_CHAR;
	msg_val = 0;

	DBG(">>>> kids=%d\n",NR_OF_KIDS(intr->ip));
	for( i=NR_OF_ATTR(intr->ip),p=ATTR_PTR(intr->ip) ; i>0 ; i-- ) {
		if (*p==FIELD_ATTR)
			field = *(p+1);
		else if (*p==SUBFIELD_ATTR)
			subfield = *(p+1);
		else {
			LOG(L_ERR,"ERROR:run_address_switch: unknown param type (%d) for "
				"ADDRESS_SWITCH node\n",*p);
			goto error;
		}
		p += 2;
	}

	if (field==UNDEF_CHAR) {
		LOG(L_ERR,"ERROR:run_address_switch: param FIELD no found\n");
		goto error;
	}

	for( i=0 ; i<NR_OF_KIDS(intr->ip) ; i++ ) {
		kid = intr->ip + KID_OFFSET(intr->ip,i);
		switch ( NODE_TYPE(kid) ) {
			case OTHERWISE_NODE :
				DBG("DEBUG:run_address_switch: matching on OTHERWISE node\n");
				return ((NR_OF_KIDS(kid)==0)?EO_SCRIPT:kid+KID_OFFSET(kid,0));
			case ADDRESS_NODE :
				if (NR_OF_ATTR(kid)!=1)
					goto error;
				p = ATTR_PTR(kid);
				cpl_val.len = *((unsigned short*)(p+1));
				cpl_val.s = ((cpl_val.len)?(p+3):0);
				DBG("DEBUG:run_address_switch: testing ADDRESS branch "
					"(%d)[%.*s](%d)..\n",*p,cpl_val.len,cpl_val.s,cpl_val.len);
				if (!msg_val) {
					switch (field) {
						case ORIGIN_VAL: /* FROM */
							if (!intr->from) {
								/* get the header */
								if (parse_from_header( intr->msg )==-1)
									goto error;
								intr->from = &(get_from(intr->msg)->uri);
							}
							uri = intr->from;
						break;
						case DESTINATION_VAL: /* RURI */
							if (!intr->ruri) {
								intr->ruri = GET_RURI( intr->msg );
							}
							uri = intr->ruri;
							break;
						case ORIGINAL_DESTINATION_VAL: /* TO */
							if (!intr->to) {
								/* get and parse the header */
								if (!intr->msg->to &&
								(parse_headers(intr->msg,HDR_TO,0)==-1 ||
								!intr->msg->to)) {
									LOG(L_ERR,"ERROR:run_address_switch: bad "
										"msg or missing TO header\n");
									goto error;
								}
								intr->to = &(get_to(intr->msg)->uri);
							}
							uri = intr->to;
							break;
						default:
							LOG(L_ERR,"ERROR:run_address_switch: unknown "
								"attribute (%d) in ADDRESS node\n",field);
							goto error;
					}
					DBG("DEBUG:run_address_switch: extracted uri is <%.*s>\n",
						uri->len, uri->s);
					switch (subfield) {
						case UNDEF_CHAR:
							msg_val = uri;
							break;
						case USER_VAL:
							if (parse_uri( uri->s, uri->len, &parsed_uri)<0)
								goto error;
							msg_val = &(parsed_uri.user);
							break;
						case HOST_VAL:
							if (parse_uri( uri->s, uri->len, &parsed_uri)<0)
								goto error;
							msg_val = &(parsed_uri.host);
							break;
						case PORT_VAL:
							if (parse_uri( uri->s, uri->len, &parsed_uri)<0)
								goto error;
							msg_val = &(parsed_uri.port);
							break;
						case TEL_VAL:
							if (parse_uri( uri->s, uri->len, &parsed_uri)<0)
								goto error;
							if (parsed_uri.user_param.len==5 &&
							memcmp(parsed_uri.user_param.s,"phone",5)==0)
								msg_val = &(parsed_uri.user);
							break;
						case ADDRESS_TYPE_VAL:
						case DISPLAY_VAL:
						default:
							LOG(L_ERR,"ERROR:run_address_switch: unsupported "
								"value attribute (%d) in ADDRESS node\n",
								subfield);
							goto error;
					}
					DBG("DEBUG:run_address_switch: extracted val. is <%.*s>\n",
						(msg_val->len==0)?0:msg_val->len, msg_val->s);
				}
				/* does the value from script match the one from message? */
				switch (*p) {
					case IS_ATTR:
						if ( (!msg_val && !cpl_val.s) ||
						(msg_val && msg_val->len==cpl_val.len &&
						strncasecmp(msg_val->s,cpl_val.s,cpl_val.len)==0)) {
							DBG("DEBUG:run_address_switch: matching on "
								"ADDRESS node (IS)\n");
							return ((NR_OF_KIDS(kid)==0)?EO_SCRIPT:
								kid+KID_OFFSET(kid,0));
						}
						break;
					case CONTAINS_ATTR:
						if (subfield!=DISPLAY_VAL) {
							LOG(L_WARN,"WARNING:run_addres_switch: operator "
							"CONTAINS applys only to DISPLAY -> ignored\n");
						} else {
							if ( msg_val && cpl_val.len<=msg_val->len &&
							strcasestr_str(msg_val, &cpl_val)!=0 ) {
								DBG("DEBUG:run_address_switch: matching on "
									"ADDRESS node (CONTAINS)\n");
								return ((NR_OF_KIDS(kid)==0)?EO_SCRIPT:
									kid+KID_OFFSET(kid,0));
							}
						}
						break;
					case SUBDOMAIN_OF_ATTR:
						if (subfield!=HOST_VAL && subfield!=TEL_VAL) {
							LOG(L_WARN,"WARNING:run_addres_switch: operator "
								"SUBDOMAIN_OF applys only to HOST or TEL ->"
								" ignored\n");
						} else {
							if (msg_val && msg_val->len>=cpl_val.len &&
							strncasecmp( cpl_val.s, msg_val->s+(msg_val->len-
							cpl_val.len), cpl_val.len)==0 && msg_val->s[1+
							msg_val->len-cpl_val.len]=='.') {
								DBG("DEBUG:run_address_switch: matching on "
									"ADDRESS node (SUBDOMAIN_OF)\n");
								return ((NR_OF_KIDS(kid)==0)?EO_SCRIPT:
									kid+KID_OFFSET(kid,0));
							}
						}
						break;
					default:
						LOG(L_ERR,"ERROR:run_address_switch: unknown attribut"
							" (%d) in ADDRESS node\n",*p);
						goto error;
				}
				break;
			default:
				LOG(L_ERR,"ERROR:run_address_switch: unknown output node type "
					"(%d) for ADDRESS_SWITCH node\n",NODE_TYPE(kid));
		}
	}

error:
	return 0;
}



inline unsigned char *run_string_switch( struct cpl_interpreter *intr )
{
	unsigned char field;
	unsigned char *p;
	unsigned char *kid;
	int i;
	str cpl_val;
	str msg_val;

	field = UNDEF_CHAR;
	msg_val.s = 0;
	msg_val.len = 0;

	DBG(">>>> kids=%d\n",NR_OF_KIDS(intr->ip));
	for( i=NR_OF_ATTR(intr->ip),p=ATTR_PTR(intr->ip) ; i>0 ; i-- ) {
		if (*p==FIELD_ATTR)
			field = *(p+1);
		else {
			LOG(L_ERR,"ERROR:run_string_switch: unknown param type (%d) for "
				"STRING_SWITCH node\n",*p);
			goto error;
		}
		p += 2;
	}

	if (field==UNDEF_CHAR) {
		LOG(L_ERR,"ERROR:run_string_switch: mandatory param FIELD no found\n");
		goto error;
	}

	for( i=0 ; i<NR_OF_KIDS(intr->ip) ; i++ ) {
		kid = intr->ip + KID_OFFSET(intr->ip,i);
		switch ( NODE_TYPE(kid) ) {
			case OTHERWISE_NODE :
				DBG("DEBUG:run_string_switch: matching on OTHERWISE node\n");
				return ((NR_OF_KIDS(kid)==0)?EO_SCRIPT:kid+KID_OFFSET(kid,0));
			case STRING_NODE :
				if (NR_OF_ATTR(kid)!=1)
					goto error;
				p = ATTR_PTR(kid);
				cpl_val.len = *((unsigned short*)(p+1));
				cpl_val.s = ((cpl_val.len)?(p+3):0);
				DBG("DEBUG:run_string_switch: testing STRING branch "
					"(%d)[%.*s](%d)..\n",*p,cpl_val.len,cpl_val.s,cpl_val.len);
				if (!msg_val.s) {
					switch (field) {
						case SUBJECT_VAL: /* SUBJECT */
							if (!intr->subject) {
								/* get the subject header */
								if (!intr->msg->subject &&
								(parse_headers(intr->msg,HDR_SUBJECT,0)==-1 ||
								!intr->msg->subject)) {
									LOG(L_ERR,"ERROR:run_string_switch: bad "
										"msg or missing SUBJECT header\n");
									goto error;
								}
								intr->subject =
									&(intr->msg->subject->body);
							}
							trim_len( msg_val.len,msg_val.s,
								*(intr->subject));
							break;
						case ORGANIZATION_VAL: /* ORGANIZATION */
							if (!intr->organization) {
								/* get the organization header */
								if (!intr->msg->organization &&
								(parse_headers(intr->msg,HDR_ORGANIZATION,0)
								==-1 || !intr->msg->organization)) {
									LOG(L_ERR,"ERROR:run_string_switch: bad "
										"msg or missing ORGANIZATION hdr\n");
									goto error;
								}
								intr->organization =
									&(intr->msg->organization->body);
							}
							trim_len( msg_val.len,msg_val.s,
								*(intr->organization));
							break;
						case USER_AGENT_VAL: /* User Agent */
							if (!intr->user_agent) {
								/* get the  header */
								if (!intr->msg->user_agent &&
								(parse_headers(intr->msg,HDR_USERAGENT,0)
								==-1 || !intr->msg->user_agent)) {
									LOG(L_ERR,"ERROR:run_string_switch: bad "
										"msg or missing USERAGENT hdr\n");
									goto error;
								}
								intr->user_agent =
									&(intr->msg->user_agent->body);
							}
							trim_len( msg_val.len,msg_val.s,
								*(intr->user_agent));
							break;
						default:
							LOG(L_ERR,"ERROR:run_string_switch: unknown "
								"attribute (%d) in STRING node\n",field);
							goto error;
					}
					DBG("DEBUG:run_string_switch: extracted msg string is "
						"<%.*s>\n",msg_val.len, msg_val.s);
				}
				/* does the value from script match the one from message? */
				switch (*p) {
					case IS_ATTR:
						if ( (!msg_val.s && !cpl_val.s) ||
						(msg_val.len==cpl_val.len &&
						strncasecmp(msg_val.s,cpl_val.s,cpl_val.len)==0)) {
							DBG("DEBUG:run_string_switch: matching on "
								"STRING node (IS)\n");
							return ((NR_OF_KIDS(kid)==0)?EO_SCRIPT:
								kid+KID_OFFSET(kid,0));
						}
						break;
					case CONTAINS_ATTR:
						if (cpl_val.len<=msg_val.len && 
						strcasestr_str(&msg_val, &cpl_val)!=0 ) {
							DBG("DEBUG:run_string_switch: matching on "
								"STRING node (CONTAINS)\n");
							return ((NR_OF_KIDS(kid)==0)?EO_SCRIPT:
								kid+KID_OFFSET(kid,0));
						}
						break;
					default:
						LOG(L_ERR,"ERROR:run_string_switch: unknown attribut"
							" (%d) in STRING node\n",*p);
						goto error;
				}
				break;
			default:
				LOG(L_ERR,"ERROR:run_string_switch: unknown output node type "
					"(%d) for STRING_SWITCH node\n",NODE_TYPE(kid));
		}
	}

error:
	return 0;
}




inline unsigned char *run_priority_switch( struct cpl_interpreter *intr )
{
	static str default_val={"normal",6};
	unsigned char *p;
	unsigned char *kid;
	unsigned char attr_name;
	unsigned char attr_val;
	unsigned char msg_attr_val;
	unsigned char msg_prio;
	int i;
	str cpl_val;
	str msg_val;

	msg_val.s = 0;
	msg_val.len = 0;

	DBG(">>>> kids=%d\n",NR_OF_KIDS(intr->ip));

	for( i=0 ; i<NR_OF_KIDS(intr->ip) ; i++ ) {
		kid = intr->ip + KID_OFFSET(intr->ip,i);
		switch ( NODE_TYPE(kid) ) {
			case OTHERWISE_NODE :
				DBG("DEBUG:run_priority_switch: matching on OTHERWISE node\n");
				return ((NR_OF_KIDS(kid)==0)?EO_SCRIPT:kid+KID_OFFSET(kid,0));
			case PRIORITY_NODE :
				if (NR_OF_ATTR(kid)!=1)
					goto error;
				/* get the attribute */
				p = ATTR_PTR(kid);
				check_overflow_by_ptr( p+3, intr, error);
				/* attribute's name */
				attr_name = (*(p++));
				if (attr_name!=LESS_ATTR && attr_name!=GREATER_ATTR &&
				attr_name!=EQUAL_ATTR){
					LOG(L_ERR,"ERROR:run_priority_switch: unknown attribut "
						"(%d) in PRIORITY node ->skipping branch\n",attr_name);
					continue; /* for cycle for all kids */
				}
				/* attribute's encoded value */
				attr_val = (*(p++));
				if (attr_val!=EMERGENCY_VAL && attr_val!=URGENT_VAL &&
				attr_val!=NORMAL_VAL && attr_val!=NON_URGENT_VAL &&
				attr_val!=UNKNOWN_PRIO_VAL) {
					LOG(L_ERR,"ERROR:run_priority_switch: unknown encoded "
						"value (%d) for attribute (*d) in PRIORITY node "
						"-> skipping branch\n",*p);
					continue; /* for cycle for all kids */
				}
				if (attr_val==UNKNOWN_PRIO_VAL && attr_name!=EQUAL_ATTR) {
					LOG(L_ERR,"ERROR:cpl_c:run_priority_switch: bad PRIORITY "
						"brach: attr_name=EQUAL doesn't match attr_val=UNKNOWN"
						" -> skipping branch\n");
					continue; /* for cycle for all kids */
				}
				/* attribute's value */
				cpl_val.len = *((unsigned short*)(p));
				check_overflow_by_ptr( p+1+cpl_val.len, intr, error);
				cpl_val.s = ((cpl_val.len)?(p+2):0);

				DBG("DEBUG:run_priority_switch: testing PRIORITY branch "
					"(attr=%d,val=%d) [%.*s](%d)..\n",
					attr_name,attr_val,cpl_val.len,cpl_val.s,cpl_val.len);
				if (!msg_val.s) {
					if (!intr->subject) {
						/* get the PRIORITY header from message */
						if (!intr->msg->priority &&
						(parse_headers(intr->msg,HDR_PRIORITY,0)==-1 ||
						!intr->msg->priority)) {
							LOG(L_WARN,"WARNING:run_priority_switch: bad "
								"msg or missing PRIORITY header -> using "
								"default value \"normal\"!\n");
							intr->priority = &default_val;
						} else {
							intr->priority =
								&(intr->msg->priority->body);
						}
					}
					trim_len( msg_val.len, msg_val.s, *(intr->priority));
					/* encode attribute's value from SIP message */
					if ( msg_val.len==EMERGENCY_STR_LEN &&
					!strncasecmp(msg_val.s,EMERGENCY_STR,msg_val.len) ) {
						msg_attr_val = EMERGENCY_VAL;
					} else if ( msg_val.len==URGENT_STR_LEN &&
					!strncasecmp(msg_val.s,URGENT_STR,msg_val.len) ) {
						msg_attr_val = URGENT_VAL;
					} else if ( msg_val.len==NORMAL_STR_LEN &&
					!strncasecmp(msg_val.s,NORMAL_STR,msg_val.len) ) {
						msg_attr_val = NORMAL_VAL;
					} else if ( msg_val.len==NON_URGENT_STR_LEN &&
					!strncasecmp(msg_val.s,NON_URGENT_STR,msg_val.len) ) {
						msg_attr_val = NON_URGENT_VAL;
					} else {
						msg_attr_val = UNKNOWN_PRIO_VAL;
					}
					DBG("DEBUG:run_priority_switch: extracted msg priority is "
						"<%.*s> decoded as [%d]\n",
						msg_val.len,msg_val.s,msg_attr_val);
				}
				DBG("DEBUG:run_priority_switch: using msg string <%.*s>\n",
					msg_val.len, msg_val.s);
				/* attr_val (from cpl) cannot be UNKNOWN - we already
				 * check it -> check only for msg_attr_val for non-EQUAL op */
				if (msg_attr_val==UNKNOWN_PRIO_VAL && attr_name!=EQUAL_ATTR) {
					LOG(L_NOTICE,"NOTICE:run_priority_switch: UNKNOWN "
						"value found in sip_msg when tring a LESS/GREATER "
						"cmp -> force the value to default \"normal\"\n");
					msg_prio = NORMAL_VAL;
				} else {
					msg_prio = msg_attr_val;
				}
				/* does the value from script match the one from message? */
				switch (attr_name) {
					case LESS_ATTR:
						switch (attr_val) {
							case EMERGENCY_VAL:
								if (msg_prio!=EMERGENCY_VAL) break; /*OK*/
								else continue; /* for cycle for all kids */
							case URGENT_VAL:
								if (msg_prio!=EMERGENCY_VAL &&
									msg_prio!=URGENT_VAL) break; /* OK */
								else continue; /* for cycle for all kids */
							case NORMAL_VAL:
								if (msg_prio==NON_URGENT_VAL) break; /*OK*/
								else continue; /* for cycle for all kids */
							case NON_URGENT_VAL:
								continue; /* for cycle for all kids */
						}
						break;
					case GREATER_ATTR:
						switch (attr_val) {
							case EMERGENCY_VAL:
								continue; /* for cycle for all kids */
							case URGENT_VAL:
								if (msg_prio!=EMERGENCY_VAL) break; /*OK*/
								else continue; /* for cycle for all kids */
							case NORMAL_VAL:
								if (msg_prio!=NON_URGENT_VAL &&
									msg_prio!=NORMAL_VAL) break; /*OK*/
								else continue; /* for cycle for all kids */
							case NON_URGENT_VAL:
								if (msg_prio!=NON_URGENT_VAL) break; /*OK*/
								else continue; /* for cycle for all kids */
						}
						break;
					case EQUAL_ATTR:
						if ( attr_val==msg_prio ) {
							if (attr_val==UNKNOWN_PRIO_VAL) {
								if ( msg_val.len==cpl_val.len &&
								!strncasecmp(msg_val.s,cpl_val.s,msg_val.len)){
									break; /* OK */
								}
							} else {
								break; /* OK */
							}
						}
						continue; /* for cycle for all kids */
						break;
				} /* end switch for attr_name */
				DBG("DEBUG:run_priority_switch: matching current "
					"PRIORITY node\n");
				return ((NR_OF_KIDS(kid)==0)?EO_SCRIPT:
					kid+KID_OFFSET(kid,0));
				break;
			default:
				LOG(L_ERR,"ERROR:run_priority_switch: unknown output node type"
					" (%d) for PRIORITY_SWITCH node\n",NODE_TYPE(kid));
		} /* end switch for NODE_TYPE */
	} /* end for for all kids */

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
				url.len = *((unsigned short*)(p+1));
				url.s = ((url.len)?(p+3):0);
				p += 3+url.len;
				break;
			case PRIORITY_ATTR:
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
	add_location( &(intr->loc_set), url.s, url.len, prio );

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
				url.len = *((unsigned short*)(p+1));
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



inline unsigned char *run_reject( struct cpl_interpreter *intr )
{
	static char buffer[MAX_SIZE_STATIC_BUF];
	unsigned char *p;
	unsigned short status;
	char *reason;
	int reason_len;
	int len;
	int i;

	reason = (char*)UNDEF_CHAR;
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
				status = *((unsigned short*)(p+1));
				p += 3;
				break;
			case REASON_ATTR:
				len = *((unsigned short*)(p+1));
				if (len) {
					reason_len = (len>MAX_SIZE_STATIC_BUF-1)?
						(MAX_SIZE_STATIC_BUF-1):(len);
					memcpy( buffer, p+3, reason_len );
					buffer[reason_len] = 0;
					reason = buffer;
				}
				p += 3+len;
				break;
			default:
				LOG(L_WARN,"WARNING:run_reject: unknown attribute "
					"(%d) in REJECT node -> ignoring..\n",*p);
				goto error;
		}
	}

	if (reason==(char*)UNDEF_CHAR ) {
		switch (status) {
			case 486:
				reason = "Busy Here";
				break;
			case 404:
				reason = "Not Found";
				break;
			case 603:
				reason = "Decline";
				break;
			case 500:
				reason = "Internal Server Error";
			default:
				LOG(L_ERR,"ERROR:run_reject: unknown value (%d) for attribute"
					" STATUS in reject node\n",status);
				goto error;
		}
	}

	if (sl_send_rpl( intr->msg, (char*)(int)status, reason )<0) {
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
