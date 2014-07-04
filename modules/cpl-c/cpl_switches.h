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
 * 2003-06-27: file created (bogdan)
 */

#include "cpl_time.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_uri.h"



/* UPDATED + CHECKED
 */
static inline char *run_address_switch( struct cpl_interpreter *intr )
{
	static str def_port_str = {"5060",4};
	unsigned short field, subfield;
	char  *p;
	char  *kid;
	unsigned short  attr_name;
	unsigned short n;
	int i;
	int k;
	str cpl_val;
	str *msg_val;
	str *uri;
	struct sip_uri parsed_uri;

	field = subfield = UNDEF_CHAR;
	msg_val = 0;

	p=ATTR_PTR(intr->ip);
	/* parse the attributes */
	for( i=NR_OF_ATTR(intr->ip) ; i>0 ; i-- ) {
		get_basic_attr( p, attr_name, n, intr, script_error);
		switch (attr_name) {
			case FIELD_ATTR:
				if (field!=UNDEF_CHAR) {
					LM_ERR("multiple FIELD attrs found\n");
					goto script_error;
				}
				field = n;
				break;
			case SUBFIELD_ATTR:
				if (subfield!=UNDEF_CHAR) {
					LM_ERR("multiple SUBFIELD attrs found\n");
					goto script_error;
				}
				subfield = n; break;
			default:
				LM_ERR("unknown attribute "
					"(%d) in ADDRESS_SWITCH node\n",*p);
				goto script_error;
		}
	}

	if (field==UNDEF_CHAR) {
		LM_ERR("mandatory param FIELD no found\n");
		goto script_error;
	}

	/* test the condition from all the sub-nodes */
	for( i=0 ; i<NR_OF_KIDS(intr->ip) ; i++ ) {
		kid = intr->ip + KID_OFFSET(intr->ip,i);
		check_overflow_by_ptr( kid+SIMPLE_NODE_SIZE(kid), intr, script_error);
		switch ( NODE_TYPE(kid) ) {
			case NOT_PRESENT_NODE:
				LM_DBG("NOT_PRESENT node found ->"
					"skipping (useless in this case)\n");
				break;
			case OTHERWISE_NODE :
				if (i!=NR_OF_KIDS(intr->ip)-1) {
					LM_ERR("OTHERWISE node not found as the last sub-node!\n");
					goto script_error;
				}
				LM_DBG("matching on OTHERWISE node\n");
				return get_first_child(kid);
			case ADDRESS_NODE :
				/* check the number of attributes */
				if (NR_OF_ATTR(kid)!=1) {
					LM_ERR("incorrect nr of attrs "
						"(%d) in ADDRESS node\n",NR_OF_ATTR(kid));
					goto script_error;
				}
				/* get the attribute name */
				p = ATTR_PTR(kid);
				get_basic_attr( p, attr_name, cpl_val.len, intr, script_error);
				if (attr_name!=IS_ATTR && attr_name!=CONTAINS_ATTR &&
				attr_name!=SUBDOMAIN_OF_ATTR) {
					LM_ERR("unknown attribute "
						"(%d) in ADDRESS node\n",attr_name);
					goto script_error;
				}
				/* get attribute value */
				get_str_attr( p, cpl_val.s, cpl_val.len, intr, script_error,1);
				LM_DBG("testing ADDRESS branch "
					" attr_name=%d attr_val=[%.*s](%d)..\n",
					attr_name,cpl_val.len,cpl_val.s,cpl_val.len);
				/* extract the needed value from the message */
				if (!msg_val) {
					switch (field) {
						case ORIGIN_VAL: /* FROM */
							if (!intr->from) {
								/* get the header */
								if (parse_from_header( intr->msg )<0)
									goto runtime_error;
								intr->from = &(get_from(intr->msg)->uri);
							}
							uri = intr->from;
						break;
						case DESTINATION_VAL: /* RURI */
							if (!intr->ruri)
								intr->ruri = GET_RURI( intr->msg );
							uri = intr->ruri;
							break;
						case ORIGINAL_DESTINATION_VAL: /* TO */
							if (!intr->to) {
								/* get and parse the header */
								if (!intr->msg->to &&
								(parse_headers(intr->msg,HDR_TO_F,0)==-1 ||
								!intr->msg->to)) {
									LM_ERR("bad msg or missing TO header\n");
									goto runtime_error;
								}
								intr->to = &(get_to(intr->msg)->uri);
							}
							uri = intr->to;
							break;
						default:
							LM_ERR("unknown "
								"attribute (%d) in ADDRESS node\n",field);
							goto script_error;
					}
					LM_DBG("extracted uri is <%.*s>\n",
						uri->len, uri->s);
					switch (subfield) {
						case UNDEF_CHAR:
							msg_val = uri;
							break;
						case USER_VAL:
							if (parse_uri( uri->s, uri->len, &parsed_uri)<0)
								goto runtime_error;
							msg_val = &(parsed_uri.user);
							break;
						case HOST_VAL:
							if (parse_uri( uri->s, uri->len, &parsed_uri)<0)
								goto runtime_error;
							msg_val = &(parsed_uri.host);
							break;
						case PORT_VAL:
							if (parse_uri( uri->s, uri->len, &parsed_uri)<0)
								goto runtime_error;
							if (parsed_uri.port.len!=0)
								msg_val = &(parsed_uri.port);
							else
								msg_val = &def_port_str;
							break;
						case TEL_VAL:
							if (parse_uri( uri->s, uri->len, &parsed_uri)<0)
								goto runtime_error;
							if (parsed_uri.user_param_val.len==5 &&
							memcmp(parsed_uri.user_param_val.s,"phone",5)==0)
								msg_val = &(parsed_uri.user);
							break;
						case ADDRESS_TYPE_VAL:
						case DISPLAY_VAL:
						default:
							LM_ERR("unsupported "
								"value attribute (%d) in ADDRESS node\n",
								subfield);
							goto script_error;
					}
					LM_DBG("extracted val. is <%.*s>\n",
						(msg_val==0)?0:msg_val->len, (msg_val==0)?0:msg_val->s);
				}
				/* does the value from script match the one from message? */
				switch (attr_name) {
					case IS_ATTR:
						if ( (!msg_val && !cpl_val.s) ||
						(msg_val && msg_val->len==cpl_val.len &&
						strncasecmp(msg_val->s,cpl_val.s,cpl_val.len)==0)) {
							LM_DBG("matching on ADDRESS node (IS)\n");
							return get_first_child(kid);
						}
						break;
					case CONTAINS_ATTR:
						if (subfield!=DISPLAY_VAL) {
							LM_WARN("operator "
							"CONTAINS applies only to DISPLAY -> ignored\n");
						} else {
							if ( msg_val && cpl_val.len<=msg_val->len &&
							strcasestr_str(msg_val, &cpl_val)!=0 ) {
								LM_DBG("matching on "
									"ADDRESS node (CONTAINS)\n");
								return get_first_child(kid);
							}
						}
						break;
					case SUBDOMAIN_OF_ATTR:
						switch (subfield) {
							case HOST_VAL:
								k = msg_val->len - cpl_val.len;
								if (k>=0 && (k==0 || msg_val->s[k-1]=='.') &&
								!strncasecmp(cpl_val.s,msg_val->s+k,cpl_val.len)
								) {
									LM_DBG("matching on "
										"ADDRESS node (SUBDOMAIN_OF)\n");
									return get_first_child(kid);
								}
								break;
							case TEL_VAL:
								if (msg_val==0) break;
								if (msg_val->len>=cpl_val.len && !strncasecmp(
								cpl_val.s,msg_val->s,cpl_val.len)) {
									LM_DBG("matching on "
										"ADDRESS node (SUBDOMAIN_OF)\n");
									return get_first_child(kid);
								}
								break;
							default:
								LM_WARN("operator SUBDOMAIN_OF applies "
									"only to HOST or TEL -> ignored\n");
						}
						break;
				}
				break;
			default:
				LM_ERR("unknown output node type "
					"(%d) for ADDRESS_SWITCH node\n",NODE_TYPE(kid));
				goto script_error;
		}
	}

	/* none of the branches of ADDRESS_SWITCH matched -> go for default */
	return DEFAULT_ACTION;
runtime_error:
	return CPL_RUNTIME_ERROR;
script_error:
	return CPL_SCRIPT_ERROR;
}



/* UPDATED + CHECKED
 */
static inline char *run_string_switch( struct cpl_interpreter *intr )
{
	unsigned short field;
	char *p;
	char *kid;
	char *not_present_node;
	unsigned short attr_name;
	int i;
	str cpl_val;
	str msg_val;

	not_present_node = 0;
	msg_val.s = 0;
	msg_val.len = 0;

	/* parse the attribute */
	if (NR_OF_ATTR(intr->ip)!=1) {
		LM_ERR("node should have 1 attr, not"
			" (%d)\n",NR_OF_ATTR(intr->ip));
		goto script_error;
	}
	p=ATTR_PTR(intr->ip);
	get_basic_attr( p, attr_name, field, intr, script_error);
	if (attr_name!=FIELD_ATTR) {
		LM_ERR("unknown param type (%d)"
			" for STRING_SWITCH node\n",*p);
		goto script_error;
	}

	for( i=0 ; i<NR_OF_KIDS(intr->ip) ; i++ ) {
		kid = intr->ip + KID_OFFSET(intr->ip,i);
		check_overflow_by_ptr( kid+SIMPLE_NODE_SIZE(kid), intr, script_error);
		switch ( NODE_TYPE(kid) ) {
			case NOT_PRESENT_NODE:
				if (not_present_node) {
					LM_ERR("NOT_PRESENT node found twice!\n");
					goto script_error;
				}
				not_present_node = kid;
				break;
			case OTHERWISE_NODE :
				if (i!=NR_OF_KIDS(intr->ip)-1) {
					LM_ERR("OTHERWISE node "
						"not found as the last sub-node!\n");
					goto script_error;
				}
				LM_DBG("matching on OTHERWISE node\n");
				return get_first_child(kid);
			case STRING_NODE :
				/* check the number of attributes */
				if (NR_OF_ATTR(kid)!=1) {
					LM_ERR("incorrect nr of attrs "
						"(%d) in STRING node (expected 1)\n",NR_OF_ATTR(kid));
					goto script_error;
				}
				/* get the attribute name */
				p = ATTR_PTR(kid);
				get_basic_attr( p, attr_name, cpl_val.len, intr, script_error);
				if (attr_name!=IS_ATTR && attr_name!=CONTAINS_ATTR ) {
					LM_ERR("unknown attribute "
						"(%d) in STRING node\n",attr_name);
					goto script_error;
				}
				/* get attribute value */
				get_str_attr( p, cpl_val.s, cpl_val.len, intr, script_error,1);
				LM_DBG("testing STRING branch "
					"attr_name=%d attr_val=[%.*s](%d)..\n",
					attr_name,cpl_val.len,cpl_val.s,cpl_val.len);
				if (!msg_val.s) {
					switch (field) {
						case SUBJECT_VAL: /* SUBJECT */
							if (intr->subject==STR_NOT_FOUND)
								goto not_present;
							if (!intr->subject) {
								/* get the subject header */
								if (!intr->msg->subject) {
									if (parse_headers(intr->msg,
									HDR_SUBJECT_F,0)==-1) {
										LM_ERR("bad SUBJECT header\n");
										goto runtime_error;
									} else if (!intr->msg->subject) {
										/* hdr not present */
										intr->subject = STR_NOT_FOUND;
										goto not_present;
									}
								}
								intr->subject =
									&(intr->msg->subject->body);
							}
							trim_len( msg_val.len,msg_val.s,
								*(intr->subject));
							break;
						case ORGANIZATION_VAL: /* ORGANIZATION */
							if (intr->organization==STR_NOT_FOUND)
								goto not_present;
							if (!intr->organization) {
								/* get the organization header */
								if (!intr->msg->organization) {
									if (parse_headers(intr->msg,
									HDR_ORGANIZATION_F,0)==-1) {
										LM_ERR("bad ORGANIZATION hdr\n");
										goto runtime_error;
									} else if (!intr->msg->organization) {
										/* hdr not present */
										intr->organization = STR_NOT_FOUND;
										goto not_present;
									}
								}
								intr->organization =
									&(intr->msg->organization->body);
							}
							trim_len( msg_val.len,msg_val.s,
								*(intr->organization));
							break;
						case USER_AGENT_VAL: /* User Agent */
							if (intr->user_agent==STR_NOT_FOUND)
								goto not_present;
							if (!intr->user_agent) {
								/* get the  header */
								if (!intr->msg->user_agent) {
									if (parse_headers(intr->msg,
									HDR_USERAGENT_F,0)==-1) {
										LM_ERR("bad USERAGENT hdr\n");
										goto runtime_error;
									} else if (!intr->msg->user_agent) {
										/* hdr not present */
										intr->user_agent = STR_NOT_FOUND;
										goto not_present;
									}
								}
								intr->user_agent =
									&(intr->msg->user_agent->body);
							}
							trim_len( msg_val.len,msg_val.s,
								*(intr->user_agent));
							break;
						default:
							LM_ERR("unknown "
								"attribute (%d) in STRING node\n",field);
							goto script_error;
					}
					LM_DBG("extracted msg string is "
						"<%.*s>\n",msg_val.len, msg_val.s);
				}
				/* does the value from script match the one from message? */
				switch (attr_name) {
					case IS_ATTR:
						if ( (!msg_val.s && !cpl_val.s) ||
						(msg_val.len==cpl_val.len &&
						strncasecmp(msg_val.s,cpl_val.s,cpl_val.len)==0)) {
							LM_DBG("matching on STRING node (IS)\n");
							return get_first_child(kid);
						}
						break;
					case CONTAINS_ATTR:
						if (cpl_val.len<=msg_val.len &&
						strcasestr_str(&msg_val, &cpl_val)!=0 ) {
							LM_DBG("matching on STRING node (CONTAINS)\n");
							return get_first_child(kid);
						}
						break;
				}
				break;
			default:
				LM_ERR("unknown output node type "
					"(%d) for STRING_SWITCH node\n",NODE_TYPE(kid));
				goto script_error;
		}
	}

	/* none of the branches of STRING_SWITCH matched -> go for default */
	return DEFAULT_ACTION;
not_present:
	LM_DBG("required hdr not present in sip msg\n");
	if (not_present_node)
		return get_first_child(not_present_node);
	/* look for the NOT_PRESENT node */
	LM_DBG("searching for NOT_PRESENT sub-node..\n");
	for(; i<NR_OF_KIDS(intr->ip) ; i++ ) {
		kid = intr->ip + KID_OFFSET(intr->ip,i);
		check_overflow_by_ptr( kid+SIMPLE_NODE_SIZE(kid), intr, script_error);
		if (NODE_TYPE(kid)==NOT_PRESENT_NODE)
			return get_first_child(kid);
	}
	return DEFAULT_ACTION;
runtime_error:
	return CPL_RUNTIME_ERROR;
script_error:
	return CPL_SCRIPT_ERROR;
}



/* UPDATED + CHECKED
 */
static inline char *run_priority_switch( struct cpl_interpreter *intr )
{
	static str default_val={"normal",6};
	unsigned short n;
	char *p;
	char *kid;
	char *not_present_node;
	unsigned short attr_name;
	unsigned short attr_val;
	unsigned short msg_attr_val;
	unsigned short msg_prio;
	int i;
	str cpl_val = {0,0};
	str msg_val = {0,0};

	not_present_node = 0;
	msg_attr_val = NORMAL_VAL;

	for( i=0 ; i<NR_OF_KIDS(intr->ip) ; i++ ) {
		kid = intr->ip + KID_OFFSET(intr->ip,i);
		check_overflow_by_ptr( kid+SIMPLE_NODE_SIZE(kid), intr, script_error);
		switch ( NODE_TYPE(kid) ) {
			case NOT_PRESENT_NODE:
				if (not_present_node) {
					LM_ERR("NOT_PRESENT node found twice!\n");
					goto script_error;
				}
				not_present_node = kid;
				break;
			case OTHERWISE_NODE :
				if (i!=NR_OF_KIDS(intr->ip)-1) {
					LM_ERR("OTHERWISE node not found as the last sub-node!\n");
					goto script_error;
				}
				LM_DBG("matching on OTHERWISE node\n");
				return get_first_child(kid);
			case PRIORITY_NODE :
				if (NR_OF_ATTR(kid)!=1)
					goto script_error;
				/* get the attribute */
				p = ATTR_PTR(kid);
				get_basic_attr( p, attr_name, attr_val, intr, script_error);
				if (attr_name!=LESS_ATTR && attr_name!=GREATER_ATTR &&
				attr_name!=EQUAL_ATTR){
					LM_ERR("unknown attribute "
						"(%d) in PRIORITY node\n",attr_name);
					goto script_error;
				}
				/* attribute's encoded value */
				if (attr_val!=EMERGENCY_VAL && attr_val!=URGENT_VAL &&
				attr_val!=NORMAL_VAL && attr_val!=NON_URGENT_VAL &&
				attr_val!=UNKNOWN_PRIO_VAL) {
					LM_ERR("unknown encoded "
						"value (%d) for attribute (*d) in PRIORITY node\n",*p);
					goto script_error;
				}
				if (attr_val==UNKNOWN_PRIO_VAL) {
					if (attr_name!=EQUAL_ATTR) {
						LM_ERR("bad PRIORITY"
							" branch: attr=EQUAL doesn't match val=UNKNOWN\n");
						goto script_error;
					}
					/* if the attr is UNKNOWN, its string value is present  */
					get_basic_attr(p, n,cpl_val.len, intr, script_error);
					if (n!=PRIOSTR_ATTR) {
						LM_ERR("expected PRIOSTR"
							"(%d) attr, found (%d)\n",PRIOSTR_ATTR,n);
						goto script_error;
					}
					get_str_attr(p, cpl_val.s, cpl_val.len,intr,script_error,1);
				}

				LM_DBG("testing PRIORITY branch "
					"(attr=%d,val=%d) [%.*s](%d)..\n",
					attr_name,attr_val,cpl_val.len,cpl_val.s,cpl_val.len);
				if (!msg_val.s) {
					if (!intr->priority) {
						/* get the PRIORITY header from message */
						if (!intr->msg->priority) {
							if (parse_headers(intr->msg,HDR_PRIORITY_F,
							0)==-1) {
								LM_ERR("bad sip msg or PRIORITY header !\n");
								goto runtime_error;
							} else if (!intr->msg->priority) {
								LM_NOTICE("missing PRIORITY header -> using "
									"default value \"normal\"!\n");
								intr->priority = &default_val;
							} else {
								intr->priority =
									&(intr->msg->priority->body);
							}
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
					LM_DBG("extracted msg priority is "
						"<%.*s> decoded as [%d]\n",
						msg_val.len,msg_val.s,msg_attr_val);
				}
				LM_DBG("using msg string <%.*s>\n",
					msg_val.len, msg_val.s);
				/* attr_val (from cpl) cannot be UNKNOWN - we already
				 * check it -> check only for msg_attr_val for non-EQUAL op */
				if (msg_attr_val==UNKNOWN_PRIO_VAL && attr_name!=EQUAL_ATTR) {
					LM_NOTICE("UNKNOWN "
						"value found in sip_msg when string a LESS/GREATER "
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
				LM_DBG("matching current PRIORITY node\n");
				return get_first_child(kid);
				break;
			default:
				LM_ERR("unknown output node type"
					" (%d) for PRIORITY_SWITCH node\n",NODE_TYPE(kid));
				goto script_error;
		} /* end switch for NODE_TYPE */
	} /* end for for all kids */

	/* none of the branches of PRIORITY_SWITCH matched -> go for default */
	return DEFAULT_ACTION;
runtime_error:
	return CPL_RUNTIME_ERROR;
script_error:
	return CPL_SCRIPT_ERROR;
}



inline static int set_TZ(char *tz_env)
{
	LM_DBG("switching TZ as \"%s\"\n",tz_env);
	if (putenv( tz_env )==-1) {
		LM_ERR("setenv failed -> unable to set TZ "
			" \"%s\"\n",tz_env);
		return -1;
	}
	tzset(); /* just to be sure */
	return 0;
}



/* UPDATED + CHECKED
 */
static inline char *run_time_switch( struct cpl_interpreter *intr )
{
	char  *p;
	char  *kid;
	char  *attr_str;
	unsigned short attr_name;
	unsigned short attr_len;
	unsigned char  flags = 0;
	int nr_attrs;
	int i,j;
	str user_tz = {0,0};
	ac_tm_t att;
	tmrec_t trt;

	LM_DBG("checking recv. time stamp <%d>\n",
		intr->recv_time);
	switch (NR_OF_ATTR(intr->ip)) {
		case 1:
			p = ATTR_PTR(intr->ip);
			get_basic_attr( p, attr_name, user_tz.len, intr, script_error);
			if (attr_name!=TZID_ATTR) {
				LM_ERR("bad attribute -> "
					" expected=%d, found=%d\n",TZID_ATTR,attr_name);
				goto script_error;
			}
			get_str_attr( p, user_tz.s, user_tz.len, intr, script_error, 1);
		case 0:
			break;
		default:
			LM_ERR("incorrect number of attr ->"
				" found=%d expected=(0,1)\n",NR_OF_ATTR(intr->ip));
			goto script_error;
	}

	if (user_tz.s && user_tz.len) {
		if (set_TZ(user_tz.s)==-1)
			goto runtime_error;
		flags |= (1<<7);
	}

	for( i=0 ; i<NR_OF_KIDS(intr->ip) ; i++ ) {
		kid = intr->ip + KID_OFFSET(intr->ip,i);
		check_overflow_by_ptr( kid+SIMPLE_NODE_SIZE(kid), intr, script_error);
		switch ( NODE_TYPE(kid) ) {
			case NOT_PRESENT_NODE:
				LM_DBG("NOT_PRESENT node found ->"
					"skipping (useless in this case)\n");
				break;
			case OTHERWISE_NODE :
				if (i!=NR_OF_KIDS(intr->ip)-1) {
					LM_ERR("OTHERWISE node "
						"not found as the last sub-node!\n");
					goto script_error;
				}
				LM_DBG("matching on OTHERWISE node\n");
				return get_first_child(kid);
			case TIME_NODE :
				/* init structures */
				memset( &att, 0, sizeof(att));
				memset( &trt, 0, sizeof(trt));
				if(ac_tm_set_time( &att, intr->recv_time))
					goto runtime_error;
				/* let's see how many attributes we have */
				nr_attrs = NR_OF_ATTR(kid);
				/* get the attributes */
				p = ATTR_PTR(kid);
				for(j=0;j<nr_attrs;j++) {
					/* get the attribute */
					get_basic_attr( p, attr_name, attr_len, intr, script_error);
					get_str_attr( p, attr_str, attr_len, intr, script_error,1);
					/* process the attribute */
					LM_DBG("attribute [%d] found :"
						"[%s]\n",attr_name, attr_str);
					switch (attr_name) {
						case DTSTART_ATTR:
							if( !attr_str || tr_parse_dtstart(&trt, attr_str))
								goto parse_err;
							flags ^= (1<<0);
							break;
						case DTEND_ATTR:
							if( !attr_str || tr_parse_dtend(&trt, attr_str))
								goto parse_err;
							flags ^= (1<<1);
							break;
						case DURATION_ATTR:
							if( !attr_str || tr_parse_duration(&trt, attr_str))
								goto parse_err;
							flags ^= (1<<1);
							break;
						case FREQ_ATTR:
							if( attr_str && tr_parse_freq(&trt, attr_str))
								goto parse_err;
							break;
						case UNTIL_ATTR:
							if( attr_str && tr_parse_until(&trt, attr_str))
								goto parse_err;
							break;
						case INTERVAL_ATTR:
							if( attr_str && tr_parse_interval(&trt, attr_str))
								goto parse_err;
							break;
						case BYDAY_ATTR:
							if( attr_str && tr_parse_byday(&trt, attr_str))
								goto parse_err;
							break;
						case BYMONTHDAY_ATTR:
							if( attr_str && tr_parse_bymday(&trt, attr_str))
								goto parse_err;
							break;
						case BYYEARDAY_ATTR:
							if( attr_str && tr_parse_byyday(&trt, attr_str))
								goto parse_err;
							break;
						case BYMONTH_ATTR:
							if( attr_str && tr_parse_bymonth(&trt, attr_str))
								goto parse_err;
							break;
						case BYWEEKNO_ATTR:
							if( attr_str && tr_parse_byweekno(&trt, attr_str))
								goto parse_err;
							break;
						case WKST_ATTR:
							if( attr_str && tr_parse_wkst(&trt, attr_str))
								goto parse_err;
							break;
						default:
							LM_ERR("unsupported attribute [%d] found in TIME "
								"node\n",attr_name);
							goto script_error;
					} /* end attribute switch */
				} /* end for*/
				/* check the mandatory attributes */
				if ( (flags&0x03)!=((1<<0)|(1<<1)) ) {
					LM_ERR("attribute DTSTART"
						",DTEND,DURATION missing or multi-present\n");
					goto script_error;
				}
				/* does the recv_time match the specified interval?  */
				j = check_tmrec( &trt, &att, 0);
				/* restore the orig TZ */
				if ( flags&(1<<7) )
					set_TZ(cpl_env.orig_tz.s);
				/* free structs that I don't need any more */
				ac_tm_free( &att );
				tmrec_free( &trt );
				/* let's see the result ;-) */
				switch  (j) {
					case 0:
						LM_DBG("matching current TIME node\n");
						return get_first_child(kid);
					case -1:
						LM_ERR("check_tmrec "
							"ret. err. when testing time cond. !\n");
						goto runtime_error;
						break;
					case 1:
						LM_DBG("time cond. doesn't match !\n");
						break;
				}
				break;
			default:
				LM_ERR("unknown output node"
					" type (%d) for PRIORITY_SWITCH node\n",NODE_TYPE(kid));
				goto script_error;
		} /* end switch for NODE_TYPE */
	} /* end for for all kids */


	/* none of the branches of TIME_SWITCH matched -> go for default */
	ac_tm_free( &att );
	tmrec_free( &trt );
	return DEFAULT_ACTION;
runtime_error:
	if ( flags&(1<<7) )
		set_TZ(cpl_env.orig_tz.s);
	ac_tm_free( &att );
	tmrec_free( &trt );
	return CPL_RUNTIME_ERROR;
parse_err:
	LM_ERR("error parsing attr [%d][%s]\n",
		attr_name,attr_str?(char*)attr_str:"NULL");
script_error:
	if ( flags&(1<<7) )
		set_TZ(cpl_env.orig_tz.s);
	ac_tm_free( &att );
	tmrec_free( &trt );
	return CPL_SCRIPT_ERROR;
}



inline static int is_lang_tag_matching(str *range,str *cpl_tag,str *cpl_subtag)
{
	char *c;
	char *end;
	str tag = {0,0};
	str subtag = {0,0};

	c = range->s;
	end = range->s + range->len;

	while(c<end) {
		/* eat all spaces to first letter */
		while(c<end && (*c==' ' || *c=='\t')) c++;
		if (c==end) goto error;
		/* init tag and subtag */
		tag.len = 0;
		subtag.len = 0;
		/* get the tag */
		tag.s = c;
		if (*c=='*' && (c+1==end||*(c+1)!='-')) {
			tag.len++;
			c++;
		} else while (c<end && ((*c)|0x20)>='a' && ((*c)|0x20)<='z' ) {
			/*DBG("--- tag ---> <%c>[%d]\n",*c,*c);*/
			tag.len++;
			c++;
		}
		if (tag.len==0) goto error;
		if (c<end && *c=='-') {
			/* go for the subtag */
			subtag.s = ++c;
			while (c<end && ((*c)|0x20)>='a' && ((*c)|0x20)<='z' ) {
				/*DBG("--- subtag ---> <%c>[%d]\n",*c,*c);*/
				subtag.len++;
				c++;
			}
			if (subtag.len==0) goto error;
		} else {
			subtag.s = 0;
		}
		if (c<end && *c==';') {
			/* eat all the params to the ',' */
			while(c<end && *c!=',') c++;
			if (c==end) goto no_matche;
		}
		while(c<end && (*c==' '||*c=='\t')) c++;
		if (c==end || *c==',') {
			/* do compare */
			LM_DBG("testing range [%.*s]-[%.*s] against tag [%.*s]-[%.*s]\n",
				tag.len,tag.s,subtag.len,subtag.s,
				cpl_tag->len,cpl_tag->s,cpl_subtag->len,cpl_subtag->s);
			/* language range of "*" is ignored for the purpose of matching*/
			if ( !(tag.len==1 && *tag.s=='*') ) {
				/* does the language tag matches ? */
				if (tag.len==cpl_tag->len && !strncasecmp(tag.s,cpl_tag->s,
				tag.len)) {
					/* if the subtag of the range is void -> matche */
					if (subtag.len==0)
						return 1;
					/* the subtags equals -> matche */
					if (subtag.len==cpl_subtag->len &&
					!strncasecmp(subtag.s,cpl_subtag->s,subtag.len) )
						return 1;
				}
			}
			/* if ',' go for the next language range */
			if (*c==',') c++;
		} else {
			goto error;
		}
	}

no_matche:
	return 0;
error:
	LM_ERR("parse error in Accept-"
		"Language body <%.*s> at char <%c>[%d] offset %ld!\n",
		range->len,range->s,*c,*c,(long)(c-range->s));
	return -1;
}



/* UPDATED + CHECKED
 */
static inline char *run_language_switch( struct cpl_interpreter *intr )
{
	char  *p;
	char  *kid;
	char  *not_present_node;
	unsigned short attr_name;
	int nr_attr;
	int i,j;
	str attr = {0,0};
	str msg_val = {0,0};
	str lang_tag = {0,0};
	str lang_subtag = {0,0};

	not_present_node = 0;

	for( i=0 ; i<NR_OF_KIDS(intr->ip) ; i++ ) {
		kid = intr->ip + KID_OFFSET(intr->ip,i);
		check_overflow_by_ptr( kid+SIMPLE_NODE_SIZE(kid), intr, script_error);
		switch ( NODE_TYPE(kid) ) {
			case NOT_PRESENT_NODE:
				if (not_present_node) {
					LM_ERR("NOT_PRESENT node found twice!\n");
					goto script_error;
				}
				not_present_node = kid;
				break;
			case OTHERWISE_NODE :
				if (i!=NR_OF_KIDS(intr->ip)-1) {
					LM_ERR("OTHERWISE node "
						"not found as the last sub-node!\n");
					goto script_error;
				}
				LM_DBG("matching on OTHERWISE node\n");
				return get_first_child(kid);
			case LANGUAGE_NODE :
				/* check the number of attributes */
				nr_attr = NR_OF_ATTR(kid);
				if (nr_attr<1 || nr_attr>2) {
					LM_ERR("incorrect nr of attrs "
						"(%d) in LANGUAGE node (1 or 2)\n",NR_OF_ATTR(kid));
					goto script_error;
				}
				/* get the attributes */
				p = ATTR_PTR(kid);
				lang_tag.s = lang_subtag.s = 0;
				lang_tag.len = lang_subtag.len = 0;
				for(j=0;j<nr_attr;j++) {
					get_basic_attr( p, attr_name, attr.len, intr, script_error);
					get_str_attr( p, attr.s, attr.len, intr, script_error,0);
					if (attr_name==MATCHES_TAG_ATTR ) {
						lang_tag = attr;
						LM_DBG("language-tag is"
							" [%.*s]\n",attr.len,attr.s);
					}else if (attr_name==MATCHES_SUBTAG_ATTR) {
						lang_subtag = attr;
						LM_DBG("language-subtag"
							" is [%.*s]\n",attr.len,attr.s);
					}else {
						LM_ERR("unknown attribute"
						" (%d) in LANGUAGE node\n",attr_name);
						goto script_error;
					}
				}
				
				/* get the value from the SIP message -> if not yet, do it now
				 * and remember it for the next times */
				if (!msg_val.s) {
					if (intr->accept_language==STR_NOT_FOUND)
						goto not_present;
					if (!intr->accept_language) {
						/* get the accept_language header */
						if (!intr->msg->accept_language) {
							if (parse_headers(intr->msg,
							HDR_ACCEPTLANGUAGE_F,0)==-1) {
								LM_ERR("bad ACCEPT_LANGUAGE header\n");
								goto runtime_error;
							} else if (!intr->msg->accept_language) {
								/* hdr not present */
								intr->accept_language = STR_NOT_FOUND;
								goto not_present;
							}
						}
						intr->subject =
							&(intr->msg->accept_language->body);
					}
				}
				trim_len( msg_val.len,msg_val.s, *(intr->subject));
				LM_DBG("extracted msg string is "
					"<%.*s>\n",msg_val.len, msg_val.s);
				
				/* does the value from script match the one from message? */
				if (msg_val.len && msg_val.s) {
					j = is_lang_tag_matching(&msg_val,&lang_tag,&lang_subtag);
					if (j==1) {
						LM_DBG("matching on LANGUAGE node\n");
						return get_first_child(kid);
					}else if (j==-1) {
						goto runtime_error;
					}
				}
				break;
			default:
				LM_ERR("unknown output "
					"node type (%d) for LANGUAGE_SWITCH node\n",
					NODE_TYPE(kid));
				goto script_error;
		} /* end switch for NODE_TYPE */
	} /* end for for all kids */

	return DEFAULT_ACTION;
not_present:
	LM_DBG("required hdr not present in sip msg\n");
	if (not_present_node)
		return get_first_child(not_present_node);
	/* look for the NOT_PRESENT node */
	LM_DBG("searching for NOT_PRESENT sub-node..\n");
	for(; i<NR_OF_KIDS(intr->ip) ; i++ ) {
		kid = intr->ip + KID_OFFSET(intr->ip,i);
		check_overflow_by_ptr( kid+SIMPLE_NODE_SIZE(kid), intr, script_error);
		if (NODE_TYPE(kid)==NOT_PRESENT_NODE)
			return get_first_child(kid);
	}
	return DEFAULT_ACTION;
runtime_error:
	return CPL_RUNTIME_ERROR;
script_error:
	return CPL_SCRIPT_ERROR;
}
