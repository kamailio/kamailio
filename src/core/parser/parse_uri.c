/*
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
 */

/** @file
 * @brief Parser :: Parse URI's
 *
 * @ingroup parser
 */


#include "../globals.h"
#include "parse_uri.h"
#include <string.h>
#include "../dprint.h"
#include "../ut.h"   /* q_memchr */
#include "../error.h"
#include "../core_stats.h"

/* buf= pointer to begining of uri (sip:x@foo.bar:5060;a=b?h=i)
 * len= len of uri
 * returns: fills uri & returns <0 on error or 0 if ok 
 */
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
					PLR_L, PLR_R_FIN, PLR_eq,
					/* r2 */
					PR2_R, PR2_2_FIN, PR2_eq,
					/* gr */
					PGR_G, PGR_R_FIN, PGR_eq,
#ifdef USE_COMP
					/* comp */
					PCOMP_C, PCOMP_O, PCOMP_M, PCOMP_P, PCOMP_eq,
					
					/* comp values */
					/* sigcomp */
					VCOMP_S, VCOMP_SIGC_I, VCOMP_SIGC_G,
					VCOMP_SIGC_C, VCOMP_SIGC_O,  VCOMP_SIGC_M,
					VCOMP_SIGC_P_FIN,
					/* sergz */
							VCOMP_SGZ_E, VCOMP_SGZ_R, VCOMP_SGZ_G,
							VCOMP_SGZ_Z_FIN,
#endif
					
					/* transport values */
					/* udp */
					VU_U, VU_D, VU_P_FIN,
					/* tcp */
					VT_T, VT_C, VT_P_FIN,
					/* tls */
					      VTLS_L, VTLS_S_FIN,
					/* sctp */
					VS_S, VS_C, VS_T, VS_P_FIN,
					/* ws */
					VW_W, VW_S_FIN
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
	unsigned int scheme;
	uri_type backup_urit;
	uri_flags backup_urif;

#ifdef USE_COMP
	str comp_str; /* not returned for now */
	str comp_val; /* not returned for now */
#endif
	
#define SIP_SCH		0x3a706973
#define SIPS_SCH	0x73706973
#define TEL_SCH		0x3a6c6574
#define URN_SCH		0x3a6e7275
	
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
							/* save the uri type/scheme */ \
							backup_urit=uri->type; \
							backup_urif=uri->flags; \
							/* everything else is 0 */ \
							memset(uri, 0, sizeof(struct sip_uri)); \
							/* restore the scheme & flags, copy user & pass */ \
							uri->type=backup_urit; \
							uri->flags=backup_urif; \
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
			

#ifdef USE_COMP
#define comp_fin(c_state, comp_no) \
			case c_state: \
				switch(*p){ \
					case '@': \
						still_at_user; \
						break; \
					semicolon_case; \
						/* param_set(b, v); */ \
						uri->comp=(comp_no); \
						break; \
					question_case; \
						/* param_set(b, v) */; \
						uri->comp=(comp_no); \
						break; \
					colon_case;  \
					default: \
						state=URI_VAL_P; \
						break; \
				} \
				break
			
#endif

	/* init */
	end=buf+len;
	p=buf+4;
	found_user=0;
	error_headers=0;
	b=v=0;
	param=param_val=0;
	pass=0;
	password.s=0; /* fixes gcc 4.0 warning */
	password.len=0;
	port_no=0;
	state=URI_INIT;
	memset(uri, 0, sizeof(struct sip_uri)); /* zero it all, just to be sure*/
	/*look for sip:, sips: ,tel: or urn:*/
	if (len<5) goto error_too_short;
	scheme=buf[0]+(buf[1]<<8)+(buf[2]<<16)+(buf[3]<<24);
	scheme|=0x20202020;
	if (scheme==SIP_SCH){
		uri->type=SIP_URI_T;
	}else if(scheme==SIPS_SCH){
		if(buf[4]==':'){ p++; uri->type=SIPS_URI_T;}
		else goto error_bad_uri;
	}else if (scheme==TEL_SCH){
		uri->type=TEL_URI_T;
	}else if (scheme==URN_SCH){
		uri->type=URN_URI_T;
	}else goto error_bad_uri;
	
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
					case '.':
					case '-':
					case '(':
					case ')':
						/* tel uri visual separators, set flag meaning, that
						 * user should be normalized before usage
						 */
						uri->flags|=URI_USER_NORMALIZE;
						break;
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
					case 'r':
					case 'R':
						b=p;
						state=PR2_R;
						break;
					case 'g':
					case 'G':
						b=p;
						state=PGR_G;
						break;
#ifdef USE_COMP
					case 'c':
					case 'C':
						b=p;
						state=PCOMP_C;
						break;
#endif
					default:
						state=URI_PARAM_P;
				}
				break;
			case URI_PARAM_P: /* ignore current param */
				/* supported params:
				 *  maddr, transport, ttl, lr, user, method, r2  */
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
					case 'w':
					case 'W':
						v=p;
						state=VW_W;
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
			/* ws */
			value_switch(VW_W, 's', 'S', VW_S_FIN);
			transport_fin(VW_S_FIN, PROTO_WS);
			
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
					case '=':
						state=PLR_eq;
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
				/* handle lr=something case */
			case PLR_eq:
				param=&uri->lr;
				param_val=&uri->lr_val;
				switch(*p){
					param_common_cases;
					default:
						v=p;
						state=URI_VAL_P;
				}
				break;
			/* r2 */
			param_switch1(PR2_R,  '2', PR2_2_FIN);
			case PR2_2_FIN:
				switch(*p){
					case '@':
						still_at_user; 
						break;
					case '=':
						state=PR2_eq;
						break;
					semicolon_case; 
						uri->r2.s=b;
						uri->r2.len=(p-b);
						break;
					question_case; 
						uri->r2.s=b;
						uri->r2.len=(p-b);
						break;
					colon_case;
						break;
					default:
						state=URI_PARAM_P;
				}
				break;
				/* handle lr=something case */
			case PR2_eq:
				param=&uri->r2;
				param_val=&uri->r2_val;
				switch(*p){
					param_common_cases;
					default:
						v=p;
						state=URI_VAL_P;
				}
				break;
			/* gr */
			param_switch(PGR_G,  'r', 'R', PGR_R_FIN);
			case PGR_R_FIN:
				switch(*p){
					case '@':
						still_at_user;
						break;
					case '=':
						state=PGR_eq;
						break;
					semicolon_case;
						uri->gr.s=b;
						uri->gr.len=(p-b);
						break;
					question_case;
						uri->gr.s=b;
						uri->gr.len=(p-b);
						break;
					colon_case;
						break;
					default:
						state=URI_PARAM_P;
				}
				break;
				/* handle gr=something case */
			case PGR_eq:
				param=&uri->gr;
				param_val=&uri->gr_val;
				switch(*p){
					param_common_cases;
					default:
						v=p;
						state=URI_VAL_P;
				}
				break;

#ifdef USE_COMP
			param_switch(PCOMP_C,  'o', 'O' , PCOMP_O);
			param_switch(PCOMP_O,  'm', 'M' , PCOMP_M);
			param_switch(PCOMP_M,  'p', 'P' , PCOMP_P);
			param_switch1(PCOMP_P,  '=', PCOMP_eq);
			/* value */
			case PCOMP_eq:
				param=&comp_str;
				param_val=&comp_val;
				switch (*p){
					param_common_cases;
					case 's':
					case 'S':
						v=p;
						state=VCOMP_S;
						break;
					default:
						v=p;
						state=URI_VAL_P;
				}
				break;
			/* sigcomp*/
			value_switch_big(VCOMP_S, 'i', 'I', 'e', 'E',
									VCOMP_SIGC_I, VCOMP_SGZ_E);
			value_switch(VCOMP_SIGC_I, 'g', 'G', VCOMP_SIGC_G);
			value_switch(VCOMP_SIGC_G, 'c', 'C', VCOMP_SIGC_C);
			value_switch(VCOMP_SIGC_C, 'o', 'O', VCOMP_SIGC_O);
			value_switch(VCOMP_SIGC_O, 'm', 'M', VCOMP_SIGC_M);
			value_switch(VCOMP_SIGC_M, 'p', 'P', VCOMP_SIGC_P_FIN);
			comp_fin(VCOMP_SIGC_P_FIN, COMP_SIGCOMP);
			
			/* sergz*/
			value_switch(VCOMP_SGZ_E, 'r', 'R', VCOMP_SGZ_R);
			value_switch(VCOMP_SGZ_R, 'g', 'G', VCOMP_SGZ_G);
			value_switch(VCOMP_SGZ_G, 'z', 'Z', VCOMP_SGZ_Z_FIN);
			comp_fin(VCOMP_SGZ_Z_FIN, COMP_SERGZ);
#endif
				
				
				
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
							found_user=1; /* no user, pass cannot contain '?'*/
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
		case URI_INIT: /* error empty uri */
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
			/* it might be an urn, check scheme and set host */
			if (scheme==URN_SCH){
				uri->host.s=s;
				uri->host.len=p-s;
				DBG("parsed urn scheme...\n");
			/* this is the port, it can't be the passwd */
			}else goto error_bad_port;
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
		case PR2_R: /* r2 */
		case PGR_G: /* gr */
#ifdef USE_COMP
		case PCOMP_C:
		case PCOMP_O:
		case PCOMP_M:
		case PCOMP_P:
		case PCOMP_eq:
#endif
			uri->params.s=s;
			uri->params.len=p-s;
			break;
		/* fin param states */
		case PLR_R_FIN:
		case PLR_eq:
			uri->params.s=s;
			uri->params.len=p-s;
			uri->lr.s=b;
			uri->lr.len=p-b;
			break;
		case PR2_2_FIN:
		case PR2_eq:
			uri->params.s=s;
			uri->params.len=p-s;
			uri->r2.s=b;
			uri->r2.len=p-b;
			break;
		case PGR_R_FIN:
		case PGR_eq:
			uri->params.s=s;
			uri->params.len=p-s;
			uri->gr.s=b;
			uri->gr.len=p-b;
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
		case VW_W:
			uri->params.s=s;
			uri->params.len=p-s;
			param_set(b, v);
			break;
#ifdef USE_COMP
		case VCOMP_S:
		case VCOMP_SIGC_I:
		case VCOMP_SIGC_G:
		case VCOMP_SIGC_C:
		case VCOMP_SIGC_O:
		case VCOMP_SIGC_M:
		case VCOMP_SGZ_E:
		case VCOMP_SGZ_R:
		case VCOMP_SGZ_G:
			/* unrecognized comp method, assume none */
			uri->params.s=s;
			uri->params.len=p-s;
			/* uri->comp=COMP_NONE ; */
			break;
#endif
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
		case VW_S_FIN:
			uri->params.s=s;
			uri->params.len=p-s;
			param_set(b, v);
			uri->proto=PROTO_WS;
			break;
#ifdef USE_COMP
		case VCOMP_SIGC_P_FIN:
			uri->params.s=s;
			uri->params.len=p-s;
			/* param_set(b, v); */
			uri->comp=COMP_SIGCOMP;
			break;
		case VCOMP_SGZ_Z_FIN:
			uri->params.s=s;
			uri->params.len=p-s;
			/* param_set(b, v); */
			uri->comp=COMP_SERGZ;
			break;
#endif
		/* headers */
		case URI_HEADERS:
			uri->headers.s=s;
			uri->headers.len=p-s;
			if (error_headers) goto error_headers;
			break;
		default:
			goto error_bug;
	}
	switch(uri->type){
		case SIPS_URI_T:
		case SIP_URI_T:
			/* save the original sip: URI parameters in sip_params */
			uri->sip_params=uri->params;
			if ((phone2tel) &&
			     (uri->user_param_val.len == 5) &&
				 (strncmp(uri->user_param_val.s, "phone", 5) == 0)
				) {
				uri->type = TEL_URI_T;
				uri->flags |= URI_SIP_USER_PHONE;
				/* move params from user into uri->params */
				p=q_memchr(uri->user.s, ';', uri->user.len);
				if (p){
					/* NOTE: 
					 * specialized uri params (user, maddr, etc.) still hold
					 * the values from the sip-uri envelope
					 * while uri->params point to the params in the embedded tel uri
					 */
					uri->params.s=p+1;
					uri->params.len=uri->user.s+uri->user.len-uri->params.s;
					uri->user.len=p-uri->user.s;
				} else {
					uri->params.s=0;
					uri->params.len=0;
				}
			} else {
				uri->flags&=~URI_USER_NORMALIZE;
			}
			break;
		case TEL_URI_T:
		case TELS_URI_T:
			/* fix tel uris, move the number in uri and empty the host */
			uri->user=uri->host;
			uri->host.s="";
			uri->host.len=0;
			break;
		/* urn: do nothing */
		case URN_URI_T:
			break;
		case ERROR_URI_T:
			LOG(L_ERR, "ERROR: parse_uri unexpected error (BUG?)\n"); 
			goto error_bad_uri;
			break; /* do nothing, avoids a compilation warning */
	}

	if(uri->port.len>5)
		goto error_invalid_port;

#ifdef EXTRA_DEBUG
	/* do stuff */
	DBG("parsed uri:\n type=%d user=<%.*s>(%d)\n passwd=<%.*s>(%d)\n"
			" host=<%.*s>(%d)\n port=<%.*s>(%d): %d\n params=<%.*s>(%d)\n"
			" headers=<%.*s>(%d)\n",
			uri->type,
			uri->user.len, ZSW(uri->user.s), uri->user.len,
			uri->passwd.len, ZSW(uri->passwd.s), uri->passwd.len,
			uri->host.len, ZSW(uri->host.s), uri->host.len,
			uri->port.len, ZSW(uri->port.s), uri->port.len, uri->port_no,
			uri->params.len, ZSW(uri->params.s), uri->params.len,
			uri->headers.len, ZSW(uri->headers.s), uri->headers.len
		);
	DBG(" uri flags : ");
		if (uri->flags & URI_USER_NORMALIZE) DBG("user_need_norm ");
		if (uri->flags & URI_SIP_USER_PHONE) DBG("sip_user_phone ");
		DBG("   value=%d\n",uri->flags);
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
	DBG("   r2=<%.*s>\n", uri->r2.len, ZSW(uri->r2.s));
#ifdef USE_COMP
	DBG("   comp=%d\n", uri->comp);
#endif

#endif
	return 0;
	
error_too_short:
	DBG("parse_uri: uri too short: <%.*s> (%d)\n",
		len, ZSW(buf), len);
	goto error_exit;
error_bad_char:
	DBG("parse_uri: bad char '%c' in state %d"
		" parsed: <%.*s> (%d) / <%.*s> (%d)\n",
		*p, state, (int)(p-buf), ZSW(buf), (int)(p-buf),
		len, ZSW(buf), len);
	goto error_exit;
error_bad_host:
	DBG("parse_uri: bad host in uri (error at char %c in"
		" state %d) parsed: <%.*s>(%d) /<%.*s> (%d)\n",
		*p, state, (int)(p-buf), ZSW(buf), (int)(p-buf),
		len, ZSW(buf), len);
	goto error_exit;
error_bad_port:
	DBG("parse_uri: bad port in uri (error at char %c in"
		" state %d) parsed: <%.*s>(%d) /<%.*s> (%d)\n",
		*p, state, (int)(p-buf), ZSW(buf), (int)(p-buf),
		len, ZSW(buf), len);
	goto error_exit;
error_invalid_port:
	DBG("parse_uri: bad port in uri: [%.*s] in <%.*s>\n",
			uri->port.len, uri->port.s, len, ZSW(buf));
	goto error_exit;
error_bad_uri:
	DBG("parse_uri: bad uri,  state %d"
		" parsed: <%.*s> (%d) / <%.*s> (%d)\n",
		state, (int)(p-buf), ZSW(buf), (int)(p-buf), len,
		ZSW(buf), len);
	goto error_exit;
error_headers:
	DBG("parse_uri: bad uri headers: <%.*s>(%d)"
		" / <%.*s>(%d)\n",
		uri->headers.len, ZSW(uri->headers.s), uri->headers.len,
		len, ZSW(buf), len);
	goto error_exit;
error_bug:
	LOG(L_CRIT, "BUG: parse_uri: bad  state %d"
			" parsed: <%.*s> (%d) / <%.*s> (%d)\n",
			 state, (int)(p-buf), ZSW(buf), (int)(p-buf), len, ZSW(buf), len);
error_exit:
	ser_error=E_BAD_URI;
	uri->type=ERROR_URI_T;
	STATS_BAD_URI();
	return E_BAD_URI;
}


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
		DBG("ERROR: parse_sip_msg_uri: bad uri <%.*s>\n",
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

inline int normalize_tel_user(char* res, str* src) {
	int i, l;
	l=0;
	for (i=0; i<src->len; i++) {
		switch (src->s[i]) {
			case '-':
			case '.':
			case '(':
			case ')':
				break;
			default:
				res[l++]=src->s[i];
		}
	}  
	return l;
}


str	s_sip  = STR_STATIC_INIT("sip");
str	s_sips = STR_STATIC_INIT("sips");
str	s_tel  = STR_STATIC_INIT("tel");
str	s_tels = STR_STATIC_INIT("tels");
str	s_urn  = STR_STATIC_INIT("urn");
static str	s_null = STR_STATIC_INIT("");

inline void uri_type_to_str(uri_type type, str *s) {
	switch (type) {
	case SIP_URI_T:
		*s = s_sip;
		break;
	case SIPS_URI_T:
		*s = s_sips;
		break;
	case TEL_URI_T:
		*s = s_tel;
		break;
	case TELS_URI_T:
		*s = s_tels;
		break;
	case URN_URI_T:
		*s = s_urn;
		break;
	default:
		*s = s_null;
	}
}

static str	s_udp  = STR_STATIC_INIT("udp");
static str	s_tcp  = STR_STATIC_INIT("tcp");
static str	s_tls  = STR_STATIC_INIT("tls");
static str	s_sctp = STR_STATIC_INIT("sctp");
static str	s_ws   = STR_STATIC_INIT("ws");

inline void proto_type_to_str(unsigned short type, str *s) {
	switch (type) {
	case PROTO_UDP:
		*s = s_udp;
		break;
	case PROTO_TCP:
		*s = s_tcp;
		break;
	case PROTO_TLS:
		*s = s_tls;
		break;
	case PROTO_SCTP:
		*s = s_sctp;
		break;
	case PROTO_WS:
	case PROTO_WSS:
		*s = s_ws;
		break;
	default:
		*s = s_null;
	}
}
