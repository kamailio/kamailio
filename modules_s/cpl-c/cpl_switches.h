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
 * 2003-06-27: file created (bogdan)
 */

#include "cpl_time.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_uri.c"



inline unsigned char *run_address_switch( struct cpl_interpreter *intr )
{
	unsigned char field, subfield;
	unsigned char *p;
	unsigned char *kid;
	unsigned char attr_name;
	int i;
	str cpl_val;
	str *msg_val;
	str *uri;
	struct sip_uri parsed_uri;

	field = subfield = UNDEF_CHAR;
	msg_val = 0;

	i=NR_OF_ATTR(intr->ip);
	p=ATTR_PTR(intr->ip);
	check_overflow_by_ptr( p+2*i, intr, script_error);
	/* parse the attributes */
	for( ; i>0 ; i-- ) {
		if (*p==FIELD_ATTR)
			field = *(p+1);
		else if (*p==SUBFIELD_ATTR)
			subfield = *(p+1);
		else {
			LOG(L_ERR,"ERROR:cpl_c:run_address_switch: unknown attribute (%d) "
				"in ADDRESS_SWITCH node\n",*p);
			goto script_error;
		}
		p += 2;
	}

	if (field==UNDEF_CHAR) {
		LOG(L_ERR,"ERROR:cpl_c:run_address_switch: mandatory param FIELD "
			"no found\n");
		goto script_error;
	}

	/* test the condition from all the sub-nodes */
	for( i=0 ; i<NR_OF_KIDS(intr->ip) ; i++ ) {
		kid = intr->ip + KID_OFFSET(intr->ip,i);
		check_overflow_by_ptr( kid+SIMPLE_NODE_SIZE(kid), intr, script_error);
		switch ( NODE_TYPE(kid) ) {
			case OTHERWISE_NODE :
				DBG("DEBUG:run_address_switch: matching on OTHERWISE node\n");
				return get_first_child(kid);
			case ADDRESS_NODE :
				/* check the number of attributes */
				if (NR_OF_ATTR(kid)!=1) {
					LOG(L_ERR,"ERROR:run_address_switch: incorect nr of attrs "
						"(%d) in ADDRESS node -> skipping\n",NR_OF_ATTR(kid));
					goto script_error;
				}
				/* get the attribute name */
				p = ATTR_PTR(kid);
				check_overflow_by_ptr( p+2, intr, script_error);
				attr_name = *(p++);
				if (attr_name!=IS_ATTR && attr_name!=CONTAINS_ATTR &&
				attr_name!=SUBDOMAIN_OF_ATTR) {
					LOG(L_ERR,"ERROR:run_address_switch: unknown attribut "
						"(%d) in ADDRESS node ->skipping branch\n",attr_name);
					goto script_error;
				}
				/* get attribute value */
				cpl_val.len = *((unsigned short*)p);
				check_overflow_by_ptr( p+1+cpl_val.len, intr, script_error);
				cpl_val.s = ((cpl_val.len)?(p+2):0);
				DBG("DEBUG:run_address_switch: testing ADDRESS branch "
					" attr_name=%d attr_val=[%.*s](%d)..\n",
					attr_name,cpl_val.len,cpl_val.s,cpl_val.len);
				/* extract the needed value from the message */
				if (!msg_val) {
					switch (field) {
						case ORIGIN_VAL: /* FROM */
							if (!intr->from) {
								/* get the header */
								if (parse_from_header( intr->msg )==-1)
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
								(parse_headers(intr->msg,HDR_TO,0)==-1 ||
								!intr->msg->to)) {
									LOG(L_ERR,"ERROR:run_address_switch: bad "
										"msg or missing TO header\n");
									goto runtime_error;
								}
								intr->to = &(get_to(intr->msg)->uri);
							}
							uri = intr->to;
							break;
						default:
							LOG(L_ERR,"ERROR:run_address_switch: unknown "
								"attribute (%d) in ADDRESS node\n",field);
							goto script_error;
					}
					DBG("DEBUG:run_address_switch: extracted uri is <%.*s>\n",
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
							msg_val = &(parsed_uri.port);
							break;
						case TEL_VAL:
							if (parse_uri( uri->s, uri->len, &parsed_uri)<0)
								goto runtime_error;
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
							goto script_error;
					}
					DBG("DEBUG:run_address_switch: extracted val. is <%.*s>\n",
						(msg_val->len==0)?0:msg_val->len, msg_val->s);
				}
				/* does the value from script match the one from message? */
				switch (attr_name) {
					case IS_ATTR:
						if ( (!msg_val && !cpl_val.s) ||
						(msg_val && msg_val->len==cpl_val.len &&
						strncasecmp(msg_val->s,cpl_val.s,cpl_val.len)==0)) {
							DBG("DEBUG:run_address_switch: matching on "
								"ADDRESS node (IS)\n");
							return get_first_child(kid);
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
								return get_first_child(kid);
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
								return get_first_child(kid);
							}
						}
						break;
				}
				break;
			default:
				LOG(L_ERR,"ERROR:run_address_switch: unknown output node type "
					"(%d) for ADDRESS_SWITCH node\n",NODE_TYPE(kid));
				goto script_error;
		}
	}

	/* none of the branches of ADDRESS_SWITCH mached -> go for default */
	return DEFAULT_ACTION;
runtime_error:
	return CPL_RUNTIME_ERROR;
script_error:
	return CPL_SCRIPT_ERROR;
}



inline unsigned char *run_string_switch( struct cpl_interpreter *intr )
{
	unsigned char field;
	unsigned char *p;
	unsigned char *kid;
	unsigned char attr_name;
	int i;
	str cpl_val;
	str msg_val;

	field = UNDEF_CHAR;
	msg_val.s = 0;
	msg_val.len = 0;

	i=NR_OF_ATTR(intr->ip);
	p=ATTR_PTR(intr->ip);
	check_overflow_by_ptr( p+2*i, intr, script_error);
	/* parse the attributes */
	for( ; i>0 ; i-- ) {
		if (*p==FIELD_ATTR)
			field = *(p+1);
		else {
			LOG(L_ERR,"ERROR:cpl_c:run_string_switch: unknown param type (%d)"
				" for STRING_SWITCH node\n",*p);
			goto script_error;
		}
		p += 2;
	}

	if (field==UNDEF_CHAR) {
		LOG(L_ERR,"ERROR:cpl_c:run_string_switch: mandatory param FIELD "
			"no found\n");
		goto script_error;
	}

	for( i=0 ; i<NR_OF_KIDS(intr->ip) ; i++ ) {
		kid = intr->ip + KID_OFFSET(intr->ip,i);
		check_overflow_by_ptr( kid+SIMPLE_NODE_SIZE(kid), intr, script_error);
		switch ( NODE_TYPE(kid) ) {
			case OTHERWISE_NODE :
				DBG("DEBUG:run_string_switch: matching on OTHERWISE node\n");
				return get_first_child(kid);
			case STRING_NODE :
				/* check the number of attributes */
				if (NR_OF_ATTR(kid)!=1) {
					LOG(L_ERR,"ERROR:run_string_switch: incorect nr of attrs "
						"(%d) in STRING node -> skipping\n",NR_OF_ATTR(kid));
					goto script_error;
				}
				/* get the attribute name */
				p = ATTR_PTR(kid);
				check_overflow_by_ptr( p+2, intr, script_error);
				attr_name = *(p++);
				if (attr_name!=IS_ATTR && attr_name!=CONTAINS_ATTR ) {
					LOG(L_ERR,"ERROR:run_string_switch: unknown attribut "
						"(%d) in STRING node ->skipping branch\n",attr_name);
					goto script_error;
				}
				/* get attribute value */
				cpl_val.len = *((unsigned short*)p);
				check_overflow_by_ptr( p+1+cpl_val.len, intr, script_error);
				cpl_val.s = ((cpl_val.len)?(p+2):0);
				DBG("DEBUG:run_string_switch: testing STRING branch "
					"attr_name=%d attr_val=[%.*s](%d)..\n",
					attr_name,cpl_val.len,cpl_val.s,cpl_val.len);
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
									goto runtime_error;
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
									goto runtime_error;
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
									goto runtime_error;
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
							goto script_error;
					}
					DBG("DEBUG:run_string_switch: extracted msg string is "
						"<%.*s>\n",msg_val.len, msg_val.s);
				}
				/* does the value from script match the one from message? */
				switch (attr_name) {
					case IS_ATTR:
						if ( (!msg_val.s && !cpl_val.s) ||
						(msg_val.len==cpl_val.len &&
						strncasecmp(msg_val.s,cpl_val.s,cpl_val.len)==0)) {
							DBG("DEBUG:run_string_switch: matching on "
								"STRING node (IS)\n");
							return get_first_child(kid);
						}
						break;
					case CONTAINS_ATTR:
						if (cpl_val.len<=msg_val.len && 
						strcasestr_str(&msg_val, &cpl_val)!=0 ) {
							DBG("DEBUG:run_string_switch: matching on "
								"STRING node (CONTAINS)\n");
							return get_first_child(kid);
						}
						break;
				}
				break;
			default:
				LOG(L_ERR,"ERROR:run_string_switch: unknown output node type "
					"(%d) for STRING_SWITCH node\n",NODE_TYPE(kid));
				goto script_error;
		}
	}

	/* none of the branches of STRING_SWITCH mached -> go for default */
	return DEFAULT_ACTION;
runtime_error:
	return CPL_RUNTIME_ERROR;
script_error:
	return CPL_SCRIPT_ERROR;
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
	msg_attr_val = NORMAL_VAL;

	for( i=0 ; i<NR_OF_KIDS(intr->ip) ; i++ ) {
		kid = intr->ip + KID_OFFSET(intr->ip,i);
		check_overflow_by_ptr( kid+SIMPLE_NODE_SIZE(kid), intr, script_error);
		switch ( NODE_TYPE(kid) ) {
			case OTHERWISE_NODE :
				DBG("DEBUG:run_priority_switch: matching on OTHERWISE node\n");
				return ((NR_OF_KIDS(kid)==0)?EO_SCRIPT:kid+KID_OFFSET(kid,0));
			case PRIORITY_NODE :
				if (NR_OF_ATTR(kid)!=1)
					goto script_error;
				/* get the attribute */
				p = ATTR_PTR(kid);
				check_overflow_by_ptr( p+3, intr, script_error);
				/* attribute's name */
				attr_name = (*(p++));
				if (attr_name!=LESS_ATTR && attr_name!=GREATER_ATTR &&
				attr_name!=EQUAL_ATTR){
					LOG(L_ERR,"ERROR:run_priority_switch: unknown attribut "
						"(%d) in PRIORITY node ->skipping branch\n",attr_name);
					goto script_error;
				}
				/* attribute's encoded value */
				attr_val = (*(p++));
				if (attr_val!=EMERGENCY_VAL && attr_val!=URGENT_VAL &&
				attr_val!=NORMAL_VAL && attr_val!=NON_URGENT_VAL &&
				attr_val!=UNKNOWN_PRIO_VAL) {
					LOG(L_ERR,"ERROR:run_priority_switch: unknown encoded "
						"value (%d) for attribute (*d) in PRIORITY node "
						"-> skipping branch\n",*p);
					goto script_error;
				}
				if (attr_val==UNKNOWN_PRIO_VAL && attr_name!=EQUAL_ATTR) {
					LOG(L_ERR,"ERROR:cpl_c:run_priority_switch: bad PRIORITY "
						"brach: attr_name=EQUAL doesn't match attr_val=UNKNOWN"
						" -> skipping branch\n");
					goto script_error;
				}
				/* attribute's value */
				cpl_val.len = *((unsigned short*)(p));
				check_overflow_by_ptr( p+1+cpl_val.len, intr, script_error);
				cpl_val.s = ((cpl_val.len)?(p+2):0);

				DBG("DEBUG:run_priority_switch: testing PRIORITY branch "
					"(attr=%d,val=%d) [%.*s](%d)..\n",
					attr_name,attr_val,cpl_val.len,cpl_val.s,cpl_val.len);
				if (!msg_val.s) {
					if (!intr->priority) {
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
				return get_first_child(kid);
				break;
			default:
				LOG(L_ERR,"ERROR:run_priority_switch: unknown output node type"
					" (%d) for PRIORITY_SWITCH node\n",NODE_TYPE(kid));
				goto script_error;
		} /* end switch for NODE_TYPE */
	} /* end for for all kids */

	/* none of the branches of PRIORITY_SWITCH mached -> go for default */
	return DEFAULT_ACTION;
script_error:
	return CPL_SCRIPT_ERROR;
}



inline unsigned char *run_time_switch( struct cpl_interpreter *intr )
{
	unsigned char  *p;
	unsigned char  *kid;
	unsigned char  attr_name;
	unsigned short attr_len;
	unsigned char  *attr_str;
	unsigned char  flags;
	int nr_attrs;
	int i,j;
	ac_tm_t att;
	tmrec_t trt;

	/* I'm totally ignoring any attributes in the time-switch node
	 * let me make it work and I'll about this time zone, etc */
	DBG("DEBUG:run_priority_switch: checking recv. time stamp <%d>\n",
		intr->recv_time);

	for( i=0 ; i<NR_OF_KIDS(intr->ip) ; i++ ) {
		kid = intr->ip + KID_OFFSET(intr->ip,i);
		check_overflow_by_ptr( kid+SIMPLE_NODE_SIZE(kid), intr, script_error);
		switch ( NODE_TYPE(kid) ) {
			case OTHERWISE_NODE :
				DBG("DEBUG:run_time_switch: matching on OTHERWISE node\n");
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
				flags = 0;
				for(j=0;j<nr_attrs;j++) {
					check_overflow_by_ptr( p+2, intr, script_error);
					/* attribute's name */
					attr_name = (*(p++));
					/* attribute's value's len */
					attr_len = *((unsigned short*)(p));
					check_overflow_by_ptr( p+1+attr_len, intr, script_error);
					attr_str = ((attr_len)?(p+2):0);
					p += 2+attr_len;
					/* process the attribute */
					DBG("DEBUG:cpl_c:run_time_node: attribute [%d] found :"
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
							LOG(L_ERR,"ERROR:cpl_c:run_time_switch: "
								"unsupported attribute [%d] found in TIME "
								"node\n",attr_name);
							goto script_error;
					} /* end attribute switch */
				} /* end for*/
				/* check the mandatory attributes */
				if ( flags!=((1<<0)|(1<<1)) ) {
					LOG(L_ERR,"ERROR:cpl_c:run_time_switch: attribute DTSTART"
						",DTEND,DURATION missing or multi-present\n");
					goto script_error;
				}
				/* does the recv_time match the specified interval?  */
				j = check_tmrec( &trt, &att, 0);
				ac_tm_free( &att );
				tmrec_free( &trt );
				switch  (j) {
					case 0:
						DBG("DEBUG:run_time_switch: matching current "
							"TIME node\n");
						return get_first_child(kid);
					case -1:
						LOG(L_ERR,"ERROR:cpl_c:run_time_switch: check_tmrec "
							"ret. err. when testing time cond. !\n");
						goto runtime_error;
						break;
					case 1:
						DBG("DEBUG:cpl_c:run_time_switch: time cond. doesn't"
							" match !\n");
						break;
				}
				break;
			default:
				LOG(L_ERR,"ERROR:run_priority_switch: unknown output node type"
					" (%d) for PRIORITY_SWITCH node\n",NODE_TYPE(kid));
				goto script_error;
		} /* end switch for NODE_TYPE */
	} /* end for for all kids */


	/* none of the branches of TIME_SWITCH mached -> go for default */
	ac_tm_free( &att );
	tmrec_free( &trt );
	return DEFAULT_ACTION;
runtime_error:
	ac_tm_free( &att );
	tmrec_free( &trt );
	return CPL_RUNTIME_ERROR;
parse_err:
	LOG(L_ERR,"ERROR:run_priority_switch: error parsing attr [%d][%s]\n",
		attr_name,attr_str?(char*)attr_str:"NULL");
script_error:
	ac_tm_free( &att );
	tmrec_free( &trt );
	return CPL_SCRIPT_ERROR;
}




inline unsigned char *run_language_switch( struct cpl_interpreter *intr )
{
	unsigned char  *kid;
	int i;

	for( i=0 ; i<NR_OF_KIDS(intr->ip) ; i++ ) {
		kid = intr->ip + KID_OFFSET(intr->ip,i);
		check_overflow_by_ptr( kid+SIMPLE_NODE_SIZE(kid), intr, script_error);
		switch ( NODE_TYPE(kid) ) {
			case OTHERWISE_NODE :
				DBG("DEBUG:run_language_switch: matching on OTHERWISE node\n");
				return get_first_child(kid);
			case TIME_NODE :
				LOG(L_ERR,"ERROR:cpl_c:run_language_switch: branch doesn't "
					"matche\n");
				break;
			default:
				LOG(L_ERR,"ERROR:cpl_c:run_language_switch: unknown output "
					"node type (%d) for LANGUAGE_SWITCH node\n",
					NODE_TYPE(kid));
				goto script_error;
		} /* end switch for NODE_TYPE */
	} /* end for for all kids */

	return DEFAULT_ACTION;
script_error:
	return CPL_SCRIPT_ERROR;
}


