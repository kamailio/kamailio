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
 * --------
 * 2003-04-04  convenience inbound-uri parser parse_orig_ruri
 *             introduced (jiri)
 * 2003-04-11  new parse_uri introduced (better, parses also some parameters,
 *              works in one pass) (andrei)
 * 2003-04-11  ser_error is now set in parse_uri (andrei)
 * 2003-04-26  ZSW (jiri)
 */


#include "parse_uri.h"
#include <string.h>
#include "../dprint.h"
/* #ifdef PARSE_URI_OLD */ /* ZSW */
#include "../ut.h"   /* q_memchr */
/* #endif */
#include "../error.h"

/* buf= pointer to begining of uri (sip:x@foo.bar:5060;a=b?h=i)
 * len= len of uri
 * returns: fills uri & returns <0 on error or 0 if ok 
 */
#ifndef PARSE_URI_OLD
int parse_uri(char* buf, int len, struct sip_uri* uri)
{
	enum states  {	URI_INIT, URI_USER, URI_PASSWORD, URI_PASSWORD_ALPHA,
					URI_HOST, URI_HOST_P,
					URI_HOST6_P, URI_HOST6_END, URI_PORT, 
					URI_PARAM, URI_PARAM_P, URI_VAL_P, URI_HEADERS,
					/* param states */
					/* transport */
					PT_T, PT_R, PT_A, PT_N, PT_S, PT_P, PT_O, PT_R2, PT_T2,
					PT_eq,
					/* ttl */
					      PTTL_T2, PTTL_L, PTTL_eq,
					/* user */
					PU_U, PU_S, PU_E, PU_R, PU_eq,
					/* method */
					PM_M, PM_E, PM_T, PM_H, PM_O, PM_D, PM_eq,
					/* maddr */
					      PMA_A, PMA_D, PMA_D2, PMA_R, PMA_eq,
					/* lr */
					PLR_L, PLR_R_FIN,
					/* transport values */
					/* udp */
					VU_U, VU_D, VU_P_FIN,
					/* tcp */
					VT_T, VT_C, VT_P_FIN,
					/* tls */
					      VTLS_L, VTLS_S_FIN,
					/* sctp */
					VS_S, VS_C, VS_T, VS_P_FIN
	};
	register enum states state;
	char* s;
	char* b; /* param start */
	char *v; /* value start */
	str* param; /* current param */
	str* param_val; /* current param val */
	str user;
	str password;
	int port_no;
	register char* p;
	char* end;
	char* pass;
	int found_user;
	int error_headers;
	
#define case_port( ch, var) \
	case ch: \
			 (var)=(var)*10+ch-'0'; \
			 break
			 
#define still_at_user  \
						if (found_user==0){ \
							user.s=uri->host.s; \
							if (pass){\
								user.len=pass-user.s; \
								password.s=pass+1; \
								password.len=p-password.s; \
							}else{ \
								user.len=p-user.s; \
							}\
							/* everything else is 0 */ \
							memset(uri, 0, sizeof(struct sip_uri)); \
							/* copy user & pass */ \
							uri->user=user; \
							if (pass)	uri->passwd=password;  \
							s=p+1; \
							found_user=1;\
							error_headers=0; \
							state=URI_HOST; \
						}else goto error_bad_char 

#define check_host_end \
					case ':': \
						/* found the host */ \
						uri->host.s=s; \
						uri->host.len=p-s; \
						state=URI_PORT; \
						s=p+1; \
						break; \
					case ';': \
						uri->host.s=s; \
						uri->host.len=p-s; \
						state=URI_PARAM; \
						s=p+1; \
						break; \
					case '?': \
						uri->host.s=s; \
						uri->host.len=p-s; \
						state=URI_HEADERS; \
						s=p+1; \
						break; \
					case '&': \
					case '@': \
						goto error_bad_char 


#define param_set(t_start, v_start) \
					param->s=(t_start);\
					param->len=(p-(t_start));\
					param_val->s=(v_start); \
					param_val->len=(p-(v_start)) 

#define semicolon_case \
					case';': \
						if (pass){ \
							found_user=1;/* no user, pass cannot contain ';'*/ \
							pass=0; \
						} \
						state=URI_PARAM   /* new param */ 

#define question_case \
					case '?': \
						uri->params.s=s; \
						uri->params.len=p-s; \
						state=URI_HEADERS; \
						s=p+1; \
						if (pass){ \
							found_user=1;/* no user, pass cannot contain '?'*/ \
							pass=0; \
						}

#define colon_case \
					case ':': \
						if (found_user==0){ \
							/*might be pass but only if user not found yet*/ \
							if (pass){ \
								found_user=1; /* no user */ \
								pass=0; \
							}else{ \
								pass=p; \
							} \
						} \
						state=URI_PARAM_P /* generic param */

#define param_common_cases \
					case '@': \
						/* ughhh, this is still the user */ \
						still_at_user; \
						break; \
					semicolon_case; \
						break; \
					question_case; \
						break; \
					colon_case; \
						break

#define value_common_cases \
					case '@': \
						/* ughhh, this is still the user */ \
						still_at_user; \
						break; \
					semicolon_case; \
						param_set(b, v); \
						break; \
					question_case; \
						param_set(b, v); \
						break; \
					colon_case; \
						state=URI_VAL_P; \
						break

#define param_switch(old_state, c1, c2, new_state) \
			case old_state: \
				switch(*p){ \
					case c1: \
					case c2: \
						state=(new_state); \
						break; \
					param_common_cases; \
					default: \
						state=URI_PARAM_P; \
				} \
				break
#define param_switch1(old_state, c1, new_state) \
			case old_state: \
				switch(*p){ \
					case c1: \
						state=(new_state); \
						break; \
					param_common_cases; \
					default: \
						state=URI_PARAM_P; \
				} \
				break
#define param_switch_big(old_state, c1, c2, d1, d2, new_state_c, new_state_d) \
			case old_state : \
				switch(*p){ \
					case c1: \
					case c2: \
						state=(new_state_c); \
						break; \
					case d1: \
					case d2: \
						state=(new_state_d); \
						break; \
					param_common_cases; \
					default: \
						state=URI_PARAM_P; \
				} \
				break
#define value_switch(old_state, c1, c2, new_state) \
			case old_state: \
				switch(*p){ \
					case c1: \
					case c2: \
						state=(new_state); \
						break; \
					value_common_cases; \
					default: \
						state=URI_VAL_P; \
				} \
				break
#define value_switch_big(old_state, c1, c2, d1, d2, new_state_c, new_state_d) \
			case old_state: \
				switch(*p){ \
					case c1: \
					case c2: \
						state=(new_state_c); \
						break; \
					case d1: \
					case d2: \
						state=(new_state_d); \
						break; \
					value_common_cases; \
					default: \
						state=URI_VAL_P; \
				} \
				break

#define transport_fin(c_state, proto_no) \
			case c_state: \
				switch(*p){ \
					case '@': \
						still_at_user; \
						break; \
					semicolon_case; \
						param_set(b, v); \
						uri->proto=(proto_no); \
						break; \
					question_case; \
						param_set(b, v); \
						uri->proto=(proto_no); \
						break; \
					colon_case;  \
					default: \
						state=URI_VAL_P; \
						break; \
				} \
				break
			
	

	/* init */
	end=buf+len;
	p=buf+4;
	found_user=0;
	error_headers=0;
	b=v=0;
	param=param_val=0;
	pass=0;
	port_no=0;
	state=URI_INIT;
	memset(uri, 0, sizeof(struct sip_uri)); /* zero it all, just to be sure*/
	/*look for sip:*/
	if (len<4) goto error_too_short;
	if (! ( ((buf[0]|0x20)=='s')&&((buf[1]|0x20)=='i')&&((buf[2]|0x20)=='p')&&
		     (buf[3]==':') ) ) goto error_bad_uri;
	
	s=p;
	for(;p<end; p++){
		switch((unsigned char)state){
			case URI_INIT:
				switch(*p){
					case '[':
						/* uri =  [ipv6address]... */
						state=URI_HOST6_P;
						s=p;
						break;
					case ']':
						/* invalid, no uri can start with ']' */
					case ':':
						/* the same as above for ':' */
						goto error_bad_char;
					case '@': /* error no user part, or
								 be forgiving and accept it ? */
					default:
						state=URI_USER;
				}
				break; 
			case URI_USER:
				switch(*p){
					case '@':
						/* found the user*/
						uri->user.s=s;
						uri->user.len=p-s;
						state=URI_HOST;
						found_user=1;
						s=p+1; /* skip '@' */
						break;
					case ':':
						/* found the user, or the host? */
						uri->user.s=s;
						uri->user.len=p-s;
						state=URI_PASSWORD;
						s=p+1; /* skip ':' */
						break;
					case ';':
						/* this could be still the user or
						 * params?*/
						uri->host.s=s;
						uri->host.len=p-s;
						state=URI_PARAM;
						s=p+1;
						break;
					case '?': /* still user or headers? */
						uri->host.s=s;
						uri->host.len=p-s;
						state=URI_HEADERS;
						s=p+1;
						break;
						/* almost anything permitted in the user part */
					case '[':
					case ']': /* the user part cannot contain "[]" */
						goto error_bad_char;
				}
				break;
			case URI_PASSWORD: /* this can also be the port (missing user)*/
				switch(*p){
					case '@':
						/* found the password*/
						uri->passwd.s=s;
						uri->passwd.len=p-s;
						port_no=0;
						state=URI_HOST;
						found_user=1;
						s=p+1; /* skip '@' */
						break;
					case ';':
						/* upps this is the port */
						uri->port.s=s;
						uri->port.len=p-s;
						uri->port_no=port_no;
						/* user contains in fact the host */
						uri->host.s=uri->user.s;
						uri->host.len=uri->user.len;
						uri->user.s=0;
						uri->user.len=0;
						state=URI_PARAM;
						found_user=1; /*  there is no user part */
						s=p+1;
						break;
					case '?':
						/* upps this is the port */
						uri->port.s=s;
						uri->port.len=p-s;
						uri->port_no=port_no;
						/* user contains in fact the host */
						uri->host.s=uri->user.s;
						uri->host.len=uri->user.len;
						uri->user.s=0;
						uri->user.len=0;
						state=URI_HEADERS;
						found_user=1; /*  there is no user part */
						s=p+1;
						break;
					case_port('0', port_no);
					case_port('1', port_no);
					case_port('2', port_no);
					case_port('3', port_no);
					case_port('4', port_no);
					case_port('5', port_no);
					case_port('6', port_no);
					case_port('7', port_no);
					case_port('8', port_no);
					case_port('9', port_no);
					case '[':
					case ']':
					case ':':
						goto error_bad_char;
					default:
						/* it can't be the port, non number found */
						port_no=0;
						state=URI_PASSWORD_ALPHA;
				}
				break;
			case URI_PASSWORD_ALPHA:
				switch(*p){
					case '@':
						/* found the password*/
						uri->passwd.s=s;
						uri->passwd.len=p-s;
						state=URI_HOST;
						found_user=1;
						s=p+1; /* skip '@' */
						break;
					case ';': /* contains non-numbers => cannot be port no*/
					case '?':
						goto error_bad_port;
					case '[':
					case ']':
					case ':':
						goto error_bad_char;
				}
				break;
			case URI_HOST:
				switch(*p){
					case '[':
						state=URI_HOST6_P;
						break;
					case ':': 
					case ';':
					case '?': /* null host name ->invalid */
					case '&':
					case '@': /*chars not allowed in hosts names */
						goto error_bad_host;
					default:
						state=URI_HOST_P;
				}
				break;
			case URI_HOST_P:
				switch(*p){
					check_host_end;
				}
				break;
			case URI_HOST6_END:
				switch(*p){
					check_host_end;
					default: /*no chars allowed after [ipv6] */
						goto error_bad_host;
				}
				break;
			case URI_HOST6_P:
				switch(*p){
					case ']':
						state=URI_HOST6_END;
						break;
					case '[':
					case '&':
					case '@':
					case ';':
					case '?':
						goto error_bad_host;
				}
				break;
			case URI_PORT:
				switch(*p){
					case ';':
						uri->port.s=s;
						uri->port.len=p-s;
						uri->port_no=port_no;
						state=URI_PARAM;
						s=p+1;
						break;
					case '?':
						uri->port.s=s;
						uri->port.len=p-s;
						uri->port_no=port_no;
						state=URI_HEADERS;
						s=p+1;
						break;
					case_port('0', port_no);
					case_port('1', port_no);
					case_port('2', port_no);
					case_port('3', port_no);
					case_port('4', port_no);
					case_port('5', port_no);
					case_port('6', port_no);
					case_port('7', port_no);
					case_port('8', port_no);
					case_port('9', port_no);
					case '&':
					case '@':
					case ':':
					default:
						goto error_bad_port;
				}
				break;
			case URI_PARAM: /* beginning of a new param */
				switch(*p){
					param_common_cases;
					/* recognized params */
					case 't':
					case 'T':
						b=p;
						state=PT_T;
						break;
					case 'u':
					case 'U':
						b=p;
						state=PU_U;
						break;
					case 'm':
					case 'M':
						b=p;
						state=PM_M;
						break;
					case 'l':
					case 'L':
						b=p;
						state=PLR_L;
						break;
					default:
						state=URI_PARAM_P;
				}
				break;
			case URI_PARAM_P: /* ignore current param */
				/* supported params:
				 *  maddr, transport, ttl, lr, user, method  */
				switch(*p){
					param_common_cases;
				};
				break;
			/* ugly but fast param names parsing */
			/*transport */
			param_switch_big(PT_T,  'r', 'R', 't', 'T', PT_R, PTTL_T2);
			param_switch(PT_R,  'a', 'A', PT_A);
			param_switch(PT_A,  'n', 'N', PT_N);
			param_switch(PT_N,  's', 'S', PT_S);
			param_switch(PT_S,  'p', 'P', PT_P);
			param_switch(PT_P,  'o', 'O', PT_O);
			param_switch(PT_O,  'r', 'R', PT_R2);
			param_switch(PT_R2, 't', 'T', PT_T2);
			param_switch1(PT_T2, '=',  PT_eq);
			/* value parsing */
			case PT_eq:
				param=&uri->transport;
				param_val=&uri->transport_val;
				switch (*p){
					param_common_cases;
					case 'u':
					case 'U':
						v=p;
						state=VU_U;
						break;
					case 't':
					case 'T':
						v=p;
						state=VT_T;
						break;
					case 's':
					case 'S':
						v=p;
						state=VS_S;
						break;
					default:
						v=p;
						state=URI_VAL_P;
				}
				break;
				/* generic value */
			case URI_VAL_P:
				switch(*p){
					value_common_cases;
				}
				break;
			/* udp */
			value_switch(VU_U,  'd', 'D', VU_D);
			value_switch(VU_D,  'p', 'P', VU_P_FIN);
			transport_fin(VU_P_FIN, PROTO_UDP);
			/* tcp */
			value_switch_big(VT_T,  'c', 'C', 'l', 'L', VT_C, VTLS_L);
			value_switch(VT_C,  'p', 'P', VT_P_FIN);
			transport_fin(VT_P_FIN, PROTO_TCP);
			/* tls */
			value_switch(VTLS_L, 's', 'S', VTLS_S_FIN);
			transport_fin(VTLS_S_FIN, PROTO_TLS);
			/* sctp */
			value_switch(VS_S, 'c', 'C', VS_C);
			value_switch(VS_C, 't', 'T', VS_T);
			value_switch(VS_T, 'p', 'P', VS_P_FIN);
			transport_fin(VS_P_FIN, PROTO_SCTP);
			
			/* ttl */
			param_switch(PTTL_T2,  'l', 'L', PTTL_L);
			param_switch1(PTTL_L,  '=', PTTL_eq);
			case PTTL_eq:
				param=&uri->ttl;
				param_val=&uri->ttl_val;
				switch(*p){
					param_common_cases;
					default:
						v=p;
						state=URI_VAL_P;
				}
				break;
			
			/* user param */
			param_switch(PU_U, 's', 'S', PU_S);
			param_switch(PU_S, 'e', 'E', PU_E);
			param_switch(PU_E, 'r', 'R', PU_R);
			param_switch1(PU_R, '=', PU_eq);
			case PU_eq:
				param=&uri->user_param;
				param_val=&uri->user_param_val;
				switch(*p){
					param_common_cases;
					default:
						v=p;
						state=URI_VAL_P;
				}
				break;
			
			/* method*/
			param_switch_big(PM_M, 'e', 'E', 'a', 'A', PM_E, PMA_A);
			param_switch(PM_E, 't', 'T', PM_T);
			param_switch(PM_T, 'h', 'H', PM_H);
			param_switch(PM_H, 'o', 'O', PM_O);
			param_switch(PM_O, 'd', 'D', PM_D);
			param_switch1(PM_D, '=', PM_eq);
			case PM_eq:
				param=&uri->method;
				param_val=&uri->method_val;
				switch(*p){
					param_common_cases;
					default:
						v=p;
						state=URI_VAL_P;
				}
				break;

			/*maddr*/
			param_switch(PMA_A,  'd', 'D', PMA_D);
			param_switch(PMA_D,  'd', 'D', PMA_D2);
			param_switch(PMA_D2, 'r', 'R', PMA_R);
			param_switch1(PMA_R, '=', PMA_eq);
			case PMA_eq:
				param=&uri->maddr;
				param_val=&uri->maddr_val;
				switch(*p){
					param_common_cases;
					default:
						v=p;
						state=URI_VAL_P;
				}
				break;
			
			/* lr */
			param_switch(PLR_L,  'r', 'R', PLR_R_FIN);
			case PLR_R_FIN:
				switch(*p){
					case '@':
						still_at_user; 
						break;
					semicolon_case; 
						uri->lr.s=b;
						uri->lr.len=(p-b);
						break;
					question_case; 
						uri->lr.s=b;
						uri->lr.len=(p-b);
						break;
					colon_case;
						break;
					default:
						state=URI_PARAM_P;
				}
				break;
						
				
				
			case URI_HEADERS:
				/* for now nobody needs them so we completely ignore the 
				 * headers (they are not allowed in request uri) --andrei */
				switch(*p){
					case '@':
						/* yak, we are still at user */
						still_at_user;
						break;
					case ';':
						/* we might be still parsing user, try it */
						if (found_user) goto error_bad_char;
						error_headers=1; /* if this is not the user
											we have an error */
						/* if pass is set => it cannot be user:pass
						 * => error (';') is illegal in a header */
						if (pass) goto error_headers;
						break;
					case ':':
						if (found_user==0){
							/*might be pass but only if user not found yet*/
							if (pass){
								found_user=1; /* no user */
								pass=0;
							}else{
								pass=p;
							}
						}
						break;
					case '?':
						if (pass){
							found_user=1; /* no user, pass cannot conaint '?'*/
							pass=0;
						}
						break;
				}
				break;
			default:
				goto error_bug;
		}
	}
	/*end of uri */
	switch (state){
		case URI_INIT: /* error empy uri */
			goto error_too_short;
		case URI_USER:
			/* this is the host, it can't be the user */
			if (found_user) goto error_bad_uri;
			uri->host.s=s;
			uri->host.len=p-s;
			state=URI_HOST;
			break;
		case URI_PASSWORD:
			/* this is the port, it can't be the passwd */
			if (found_user) goto error_bad_port;
			uri->port.s=s;
			uri->port.len=p-s;
			uri->port_no=port_no;
			uri->host=uri->user;
			uri->user.s=0;
			uri->user.len=0;
			break;
		case URI_PASSWORD_ALPHA:
			/* this is the port, it can't be the passwd */
			goto error_bad_port;
		case URI_HOST_P:
		case URI_HOST6_END:
			uri->host.s=s;
			uri->host.len=p-s;
			break;
		case URI_HOST: /* error: null host */
		case URI_HOST6_P: /* error: unterminated ipv6 reference*/
			goto error_bad_host;
		case URI_PORT:
			uri->port.s=s;
			uri->port.len=p-s;
			uri->port_no=port_no;
			break;
		case URI_PARAM:
		case URI_PARAM_P:
		/* intermediate param states */
		case PT_T: /* transport */
		case PT_R:
		case PT_A:
		case PT_N:
		case PT_S:
		case PT_P:
		case PT_O:
		case PT_R2:
		case PT_T2:
		case PT_eq: /* ignore empty transport params */
		case PTTL_T2: /* ttl */
		case PTTL_L:
		case PTTL_eq:
		case PU_U:  /* user */
		case PU_S:
		case PU_E:
		case PU_R:
		case PU_eq:
		case PM_M: /* method */
		case PM_E:
		case PM_T:
		case PM_H:
		case PM_O:
		case PM_D:
		case PM_eq:
		case PLR_L: /* lr */
			uri->params.s=s;
			uri->params.len=p-s;
			break;
		/* fin param states */
		case PLR_R_FIN:
			uri->params.s=s;
			uri->params.len=p-s;
			uri->lr.s=b;
			uri->lr.len=p-b;
			break;
		case URI_VAL_P:
		/* intermediate value states */
		case VU_U:
		case VU_D:
		case VT_T:
		case VT_C:
		case VTLS_L:
		case VS_S:
		case VS_C:
		case VS_T:
			uri->params.s=s;
			uri->params.len=p-s;
			param_set(b, v);
			break;
		/* fin value states */
		case VU_P_FIN:
			uri->params.s=s;
			uri->params.len=p-s;
			param_set(b, v);
			uri->proto=PROTO_UDP;
			break;
		case VT_P_FIN:
			uri->params.s=s;
			uri->params.len=p-s;
			param_set(b, v);
			uri->proto=PROTO_TCP;
			break;
		case VTLS_S_FIN:
			uri->params.s=s;
			uri->params.len=p-s;
			param_set(b, v);
			uri->proto=PROTO_TLS;
			break;
		case VS_P_FIN:
			uri->params.s=s;
			uri->params.len=p-s;
			param_set(b, v);
			uri->proto=PROTO_SCTP;
			break;
		/* headers */
		case URI_HEADERS:
			uri->headers.s=s;
			uri->headers.len=p-s;
			if (error_headers) goto error_headers;
			break;
		default:
			goto error_bug;
	}
	
#if EXTRA_DEBUG
	/* do stuff */
	DBG("parsed uri:\n user=<%.*s>(%d)\n passwd=<%.*s>(%d)\n host=<%.*s>(%d)\n"
			" port=<%.*s>(%d): %d\n params=<%.*s>(%d)\n headers=<%.*s>(%d)\n",
			uri->user.len, ZSW(uri->user.s), uri->user.len,
			uri->passwd.len, ZSW(uri->passwd.s), uri->passwd.len,
			uri->host.len, ZSW(uri->host.s), uri->host.len,
			uri->port.len, ZSW(uri->port.s), uri->port.len, uri->port_no,
			uri->params.len, ZSW(uri->params.s), uri->params.len,
			uri->headers.len, ZSW(uri->headers.s), uri->headers.len
		);
	DBG(" uri params:\n   transport=<%.*s>, val=<%.*s>, proto=%d\n",
			uri->transport.len, ZSW(uri->transport.s), uri->transport_val.len,
			ZSW(uri->transport_val.s), uri->proto);
	DBG("   user-param=<%.*s>, val=<%.*s>\n",
			uri->user_param.len, ZSW(uri->user_param.s), 
			uri->user_param_val.len, ZSW(uri->user_param_val.s));
	DBG("   method=<%.*s>, val=<%.*s>\n",
			uri->method.len, ZSW(uri->method.s), 
			uri->method_val.len, ZSW(uri->method_val.s));
	DBG("   ttl=<%.*s>, val=<%.*s>\n",
			uri->ttl.len, ZSW(uri->ttl.s), 
			uri->ttl_val.len, ZSW(uri->ttl_val.s));
	DBG("   maddr=<%.*s>, val=<%.*s>\n",
			uri->maddr.len, ZSW(uri->maddr.s), 
			uri->maddr_val.len, ZSW(uri->maddr_val.s));
	DBG("   lr=<%.*s>\n", uri->lr.len, ZSW(uri->lr.s)); 
#endif
	return 0;
	
error_too_short:
	LOG(L_ERR, "ERROR: parse_uri: uri too short: <%.*s> (%d)\n",
			len, ZSW(buf), len);
	ser_error=E_BAD_URI;
	return E_BAD_URI;
error_bad_char:
	LOG(L_ERR, "ERROR: parse_uri: bad char '%c' in state %d"
			" parsed: <%.*s> (%d) / <%.*s> (%d)\n",
			*p, state, (int)(p-buf), ZSW(buf), (int)(p-buf), len, ZSW(buf), len);
	return E_BAD_URI;
error_bad_host:
	LOG(L_ERR, "ERROR: parse_uri: bad host in uri (error at char %c in"
			" state %d) parsed: <%.*s>(%d) /<%.*s> (%d)\n",
			*p, state, (int)(p-buf), ZSW(buf), (int)(p-buf), len, ZSW(buf), len);
	ser_error=E_BAD_URI;
	return E_BAD_URI;
error_bad_port:
	LOG(L_ERR, "ERROR: parse_uri: bad port in uri (error at char %c in"
			" state %d) parsed: <%.*s>(%d) /<%.*s> (%d)\n",
			*p, state, (int)(p-buf), ZSW(buf), (int)(p-buf), len, ZSW(buf), len);
	ser_error=E_BAD_URI;
	return E_BAD_URI;
error_bad_uri:
	LOG(L_ERR, "ERROR: parse_uri: bad uri,  state %d"
			" parsed: <%.*s> (%d) / <%.*s> (%d)\n",
			 state, (int)(p-buf), ZSW(buf), (int)(p-buf), len, ZSW(buf), len);
	ser_error=E_BAD_URI;
	return E_BAD_URI;
error_headers:
	LOG(L_ERR, "ERROR: parse_uri: bad uri headers: <%.*s>(%d)"
			" / <%.*s>(%d)\n",
			uri->headers.len, ZSW(uri->headers.s), uri->headers.len,
			len, ZSW(buf), len);
	ser_error=E_BAD_URI;
	return E_BAD_URI;
error_bug:
	LOG(L_CRIT, "BUG: parse_uri: bad  state %d"
			" parsed: <%.*s> (%d) / <%.*s> (%d)\n",
			 state, (int)(p-buf), ZSW(buf), (int)(p-buf), len, ZSW(buf), len);
	ser_error=E_BAD_URI;
	return E_BAD_URI;
}

#else /* PARSE_URI_OLD */

int parse_uri(char *buf, int len, struct sip_uri* uri)
{
	char* next, *end;
	char *user, *passwd, *host, *port, *params, *headers, *ipv6;
	int host_len, port_len, params_len, headers_len;
	int err;
	int ret;
	
	
	ret=0;
	host_len=0;
	end=buf+len;
	memset(uri, 0, sizeof(struct sip_uri)); /* zero it all, just to be sure */
	/* look for "sip:"*/;
	next=q_memchr(buf, ':',  len);
	if ((next==0)||(strncasecmp(buf,"sip",next-buf)!=0)){
		LOG(L_DBG, "ERROR: parse_uri: bad sip uri\n");
		ser_error=ret=E_BAD_URI;
		return ret;
	}
	buf=next+1; /* next char after ':' */
	if (buf>end){
		LOG(L_DBG, "ERROR: parse_uri: uri too short\n");
		ser_error=ret=E_BAD_URI;
		return ret;
	}
	/*look for '@' */
	next=q_memchr(buf,'@', end-buf);
	if (next==0){
		/* no '@' found, => no userinfo */
		uri->user.s=0;
		uri->passwd.s=0;
		host=buf;
	}else{
		/* found it */
		user=buf;
		/* try to find passwd */
		passwd=q_memchr(user,':', next-user);
		if (passwd==0){
			/* no ':' found => no password */
			uri->passwd.s=0;
			uri->user.s=user;
			uri->user.len=next-user;
		}else{
			uri->user.s=user;
			uri->user.len=passwd-user;
			passwd++; /*skip ':' */
			uri->passwd.s=passwd;
			uri->passwd.len=next-passwd;
		}
		host=next+1; /* skip '@' */
	}
	/* try to find the rest */
	if(host>=end){
		LOG(L_DBG, "ERROR: parse_uri: missing hostport\n");
		ser_error=ret=E_UNSPEC;
		return ret;
	}
	next=host;
	ipv6=q_memchr(host, '[', end-host);
	if (ipv6){
		host=ipv6+1; /* skip '[' in "[3ffe::abbcd]" */
		if (host>=end){
			LOG(L_DBG, "ERROR: parse_uri: bad ipv6 uri\n");
			ret=E_UNSPEC;
			return ret;
		}
		ipv6=q_memchr(host, ']', end-host);
		if ((ipv6==0)||(ipv6==host)){
			LOG(L_DBG, "ERROR: parse_uri: bad ipv6 uri - null address"
					" or missing ']'\n");
			ret=E_UNSPEC;
			return ret;
		}
		host_len=ipv6-host;
		next=ipv6;
	}

		
	headers=q_memchr(next,'?',end-next);
	params=q_memchr(next,';',end-next);
	port=q_memchr(next,':',end-next);
	if (host_len==0){ /* host not ipv6 addr */
		host_len=(port)?port-host:(params)?params-host:(headers)?headers-host:
				end-host;
	}
	/* get host */
	uri->host.s=host;
	uri->host.len=host_len;

	/* get port*/
	if ((port)&&(port+1<end)){
		port++;
		if ( ((params) &&(params<port))||((headers) &&(headers<port)) ){
			/* error -> invalid uri we found ';' or '?' before ':' */
			LOG(L_DBG, "ERROR: parse_uri: malformed sip uri\n");
			ser_error=ret=E_BAD_URI;
			return ret;
		}
		port_len=(params)?params-port:(headers)?headers-port:end-port;
		uri->port.s=port;
		uri->port.len=port_len;
	}else uri->port.s=0;
	/* get params */
	if ((params)&&(params+1<end)){
		params++;
		if ((headers) && (headers<params)){
			/* error -> invalid uri we found '?' or '?' before ';' */
			LOG(L_DBG, "ERROR: parse_uri: malformed sip uri\n");
			ser_error=ret=E_BAD_URI;
			return ret;
		}
		params_len=(headers)?headers-params:end-params;
		uri->params.s=params;
		uri->params.len=params_len;
	}else uri->params.s=0;
	/*get headers */
	if ((headers)&&(headers+1<end)){
		headers++;
		headers_len=end-headers;
		uri->headers.s=headers;
		uri->headers.len=headers_len;
	}else uri->headers.s=0;

	err=0;
	if (uri->port.s) uri->port_no=str2s(uri->port.s, uri->port.len, &err);
	if (err){
		LOG(L_DBG, "ERROR: parse_uri: bad port number in sip uri: %.*s\n",
				uri->port.len, ZSW(uri->port.s));
		ser_error=ret=E_BAD_URI;
		return ret;
	}

	return ret;
}
#endif




static inline int _parse_ruri(str *uri,
	int *status, struct sip_uri *parsed_uri)
{
	if (*status) return 1;

	if (parse_uri(uri->s, uri->len, parsed_uri)<0) {
		LOG(L_ERR, "ERROR: _parse_ruri: bad uri <%.*s>\n", 
				uri->len, ZSW(uri->s));
		*status=0;
		return -1;
	}
	*status=1;
	return 1;
}

int parse_sip_msg_uri(struct sip_msg* msg)
{
	char* tmp;
	int tmp_len;
	if (msg->parsed_uri_ok) return 1;

	if (msg->new_uri.s){
		tmp=msg->new_uri.s;
		tmp_len=msg->new_uri.len;
	}else{
		tmp=msg->first_line.u.request.uri.s;
		tmp_len=msg->first_line.u.request.uri.len;
	}
	if (parse_uri(tmp, tmp_len, &msg->parsed_uri)<0){
		LOG(L_ERR, "ERROR: parse_sip_msg_uri: bad uri <%.*s>\n",
					tmp_len, tmp);
		msg->parsed_uri_ok=0;
		return -1;
	}
	msg->parsed_uri_ok=1;
	return 1;
}

int parse_orig_ruri(struct sip_msg* msg)
{
	int ret;

	ret=_parse_ruri(&REQ_LINE(msg).uri,
		&msg->parsed_orig_ruri_ok, &msg->parsed_orig_ruri);
	if (ret<0) LOG(L_ERR, "ERROR: parse_orig_ruri failed\n");
	return ret;
}
