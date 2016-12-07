/*
 * via parsing automaton
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
 */

/** @file
 * @brief Parser :: Via parsing automation
 *
 * @ingroup parser
 */

#include "../comp_defs.h"
#include <stdlib.h>
#include <string.h>
#include "../dprint.h"
#include "../ut.h"
#include "../mem/mem.h"
#include "../ip_addr.h"
#include "parse_via.h"
#include "parse_def.h"
#include "msg_parser.h"



/** \brief main via states (uri:port ...) */
enum {	         
	F_HOST, P_HOST,
	L_PORT, F_PORT, P_PORT,
	L_PARAM, F_PARAM, P_PARAM,
	L_VIA, F_VIA,
	F_COMMENT, P_COMMENT,
	F_IP6HOST, P_IP6HOST, 
	F_CRLF,
	F_LF,
	F_CR,
	END_OF_HEADER
};


/** \brief first via part state */
enum {
	F_SIP = 100,
	SIP1, SIP2, FIN_SIP,
	L_VER, F_VER,
	VER1, VER2, FIN_VER,
	UDP1, UDP2, FIN_UDP,
	TCP_TLS1, TCP2, FIN_TCP,
	          TLS2, FIN_TLS,
	SCTP1, SCTP2, SCTP3, FIN_SCTP,
	WS_WSS1, WS_WSS2, FIN_WSS,
	OTHER_PROTO,
	L_PROTO, F_PROTO
};


/** \brief param related states
 * WARNING: keep in sync with parse_via.h, PARAM_HIDDEN, ...
 */
enum {	
	L_VALUE = 200, F_VALUE, P_VALUE, P_STRING,
	HIDDEN1, HIDDEN2, HIDDEN3, HIDDEN4, HIDDEN5,
	TTL1, TTL2,
	BRANCH1, BRANCH2, BRANCH3, BRANCH4, BRANCH5,
	MADDR1, MADDR2, MADDR3, MADDR4,
	RECEIVED1, RECEIVED2, RECEIVED3, RECEIVED4, RECEIVED5, RECEIVED6,
	RECEIVED7,
	RPORT1, RPORT2, RPORT3,
	ALIAS1, ALIAS2, ALIAS3, ALIAS4,
#ifdef USE_COMP
	COMP1, COMP2, COMP3, 
	/* values */
	L_COMP_VALUE, F_COMP_VALUE,
	V_COMP_S, V_SIGCOMP_I, V_SIGCOMP_G, V_SIGCOMP_C, V_SIGCOMP_O, V_SIGCOMP_M,
	FIN_V_SIGCOMP_P,
	V_SERGZ_E, V_SERGZ_R, V_SERGZ_G, FIN_V_SERGZ_Z,
#endif
	     /* fin states (227-...)*/
	FIN_HIDDEN = 230, FIN_TTL, FIN_BRANCH,
	FIN_MADDR, FIN_RECEIVED, FIN_RPORT, FIN_I, FIN_ALIAS
#ifdef USE_COMP
	, FIN_COMP
#endif
	     /*GEN_PARAM,
	       PARAM_ERROR*/ /* declared in msg_parser.h*/
};


/* entry state must be F_PARAM, or saved_state=F_PARAM and
 * state=F_{LF,CR,CRLF}!
 * output state = L_PARAM or F_PARAM or END_OF_HEADER
 * (and saved_state= last state); everything else => error 
 * WARNING: param->start must be filled before, it's used in param->size 
 * computation.
 */
static /*inline*/ char* parse_via_param(char* const p, const char* const end,
									unsigned char* const pstate, 
				    					unsigned char* const psaved_state,
									struct via_param* const param,
									const struct via_body* vb)
{
	char* tmp;
	register unsigned char state;
	unsigned char saved_state;

#define value_case(c, C, oldstate, newstate, start_of_value) \
			case (c): \
			case (C): \
				switch(state){ \
					case (oldstate): \
						state=(newstate); \
						if ((start_of_value))  param->value.s=tmp; \
						break; \
					default_value_cases \
				} \
				break

#define value_case_double(c, C, oldstate1, newstate1, oldstate2, newstate2) \
			case (c): \
			case (C): \
				switch(state){ \
					case (oldstate1): \
						state=(newstate1); \
						break; \
					case (oldstate2): \
						state=(newstate2); \
						break; \
					default_value_cases \
				} \
				break

#define default_value_cases \
					case F_VALUE: \
						state=P_VALUE; \
						param->value.s=tmp; \
						break; \
					case P_VALUE: \
					case P_STRING: \
						break; \
					case F_LF: \
					case F_CR:  \
					case F_CRLF: \
						state=END_OF_HEADER; \
						goto end_via; \
					default: \
						switch(state){ \
							case F_COMP_VALUE: \
								comp_unexpected_char; \
								state=P_VALUE; \
								param->value.s=tmp; \
								break; \
							comp_states_cases \
							comp_fin_states_cases \
								comp_unexpected_char; \
								state=P_VALUE; \
								break; \
							default: \
								LOG(L_ERR, "ERROR: parse_via_param: invalid " \
									"char <%c> in state %d\n", *tmp, state); \
								goto error; \
						}

#define comp_states_cases \
					case V_COMP_S: \
					case V_SIGCOMP_I: \
					case V_SIGCOMP_G: \
					case V_SIGCOMP_C: \
					case V_SIGCOMP_O: \
					case V_SIGCOMP_M: \
					case V_SERGZ_E: \
					case V_SERGZ_R: \
					case V_SERGZ_G: 

#define comp_fin_states_cases \
					case FIN_V_SIGCOMP_P: \
					case FIN_V_SERGZ_Z:


/* if unrecognized/bad comp, don't return error, just ignore comp */
#define comp_unexpected_char \
							LOG(L_ERR, "parse_via_param: bad/unrecognized" \
									" comp method\n"); \
							vb->comp_no=0
								


	state=*pstate;
	saved_state=*psaved_state;
	param->type=PARAM_ERROR;

	for (tmp=p;tmp<end;tmp++){
		switch(*tmp){
			case ' ':
			case '\t':
				switch(state){
					case FIN_HIDDEN:
					case FIN_ALIAS:
						param->type=state;
						param->name.len=tmp-param->name.s;
						state=L_PARAM;
						goto endofparam;
					case FIN_BRANCH:
					case FIN_TTL:
					case FIN_MADDR:
					case FIN_RECEIVED:
					case FIN_RPORT:
					case FIN_I:
						param->type=state;
						param->name.len=tmp-param->name.s;
						state=L_VALUE;
						goto find_value;
#ifdef USE_COMP
					case FIN_COMP:
						param->type=state;
						param->name.len=tmp-param->name.s;
						state=L_COMP_VALUE;
						goto find_value;
#endif
					case F_PARAM:
						break;
					case F_LF:
					case F_CR:
					case F_CRLF:
						state=saved_state;
						break;
					case GEN_PARAM:
					default:
						param->type=GEN_PARAM;
						param->name.len=tmp-param->name.s;
						state=L_VALUE;
						goto find_value;
				}
				break;
			/* \n and \r*/
			case '\n':
				switch(state){
					case FIN_HIDDEN:
					case FIN_ALIAS:
						param->type=state;
						param->name.len=tmp-param->name.s;
						param->size=tmp-param->start; 
						saved_state=L_PARAM;
						state=F_LF;
						goto endofparam;
					case FIN_BRANCH:
					case FIN_TTL:
					case FIN_MADDR:
					case FIN_RECEIVED:
					case FIN_I:
					case FIN_RPORT:
						param->type=state;
						param->name.len=tmp-param->name.s;
						param->size=tmp-param->start; 
						saved_state=L_VALUE;
						state=F_LF;
						goto find_value;
#ifdef USE_COMP
					case FIN_COMP:
						param->type=state;
						param->name.len=tmp-param->name.s;
						param->size=tmp-param->start; 
						saved_state=L_COMP_VALUE;
						state=F_LF;
						goto find_value;
#endif
					case F_PARAM:
						saved_state=state;
						state=F_LF;
						break;
					case F_LF:
					case F_CRLF:
						state=END_OF_HEADER;
						goto end_via;
					case F_CR:
						state=F_CRLF;
						break;
					case GEN_PARAM:
					default:
						param->type=GEN_PARAM;
						saved_state=L_VALUE;
						param->name.len=tmp-param->name.s;
						param->size=tmp-param->start; 
						state=F_LF;
						goto find_value;
				}
				break;
			case '\r':
				switch(state){
					case FIN_HIDDEN:
					case FIN_ALIAS:
						param->type=state;
						param->name.len=tmp-param->name.s;
						param->size=tmp-param->start; 
						saved_state=L_PARAM;
						state=F_CR;
						goto endofparam;
					case FIN_BRANCH:
					case FIN_TTL:
					case FIN_MADDR:
					case FIN_RECEIVED:
					case FIN_I:
					case FIN_RPORT:
						param->type=state;
						param->name.len=tmp-param->name.s;
						param->size=tmp-param->start; 
						saved_state=L_VALUE;
						state=F_CR;
						goto find_value;
#ifdef USE_COMP
					case FIN_COMP:
						param->type=state;
						param->name.len=tmp-param->name.s;
						param->size=tmp-param->start; 
						saved_state=L_COMP_VALUE;
						state=F_LF;
						goto find_value;
#endif
					case F_PARAM:
						saved_state=state;
						state=F_CR;
						break;
					case F_CR:
					case F_CRLF:
						state=END_OF_HEADER;
						goto end_via;
					case GEN_PARAM:
					default:
						param->type=GEN_PARAM;
						param->name.len=tmp-param->name.s;
						param->size=tmp-param->start; 
						saved_state=L_VALUE;
						state=F_CR;
						goto find_value;
				}
				break;

			case '=':
				switch(state){
					case FIN_BRANCH:
					case FIN_TTL:
					case FIN_MADDR:
					case FIN_RECEIVED:
					case FIN_RPORT:
					case FIN_I:
						param->type=state;
						param->name.len=tmp-param->name.s;
						state=F_VALUE;
						goto find_value;
#ifdef USE_COMP
					case FIN_COMP:
						param->type=state;
						param->name.len=tmp-param->name.s;
						state=F_COMP_VALUE;
						goto find_value;
#endif
					case F_PARAM:
					case FIN_HIDDEN:
					case FIN_ALIAS:
						LOG(L_ERR, "ERROR: parse_via: invalid char <%c> in"
								" state %d\n", *tmp, state);
						goto error;
					case F_CR:
					case F_LF:
					case F_CRLF:
						state=END_OF_HEADER;
						goto end_via;
					case GEN_PARAM:
					default:
						param->type=GEN_PARAM;
						param->name.len=tmp-param->name.s;
						state=F_VALUE;
						goto find_value;
				}
				break;
			case ';':
				switch(state){
					case FIN_HIDDEN:
					case FIN_RPORT: /* rport can appear w/o a value */
					case FIN_ALIAS:
						param->type=state;
						param->name.len=tmp-param->name.s;
						state=F_PARAM;
						goto endofparam;
					case FIN_BRANCH:
					case FIN_MADDR:
					case FIN_TTL:
					case FIN_RECEIVED:
					case FIN_I:
#ifdef USE_COMP
					case FIN_COMP:
#endif
						LOG(L_ERR, "ERROR: parse_via: invalid char <%c> in"
								" state %d\n", *tmp, state);
						goto error;
					case F_CR:
					case F_LF:
					case F_CRLF:
						state=END_OF_HEADER;
						goto end_via;
					case GEN_PARAM:
					default:
						param->type=GEN_PARAM;
						param->name.len=tmp-param->name.s;
						state=F_PARAM;
						goto endofparam;
				}
				break;
			case ',':
				switch(state){
					case FIN_HIDDEN:
					case FIN_RPORT:
					case FIN_ALIAS:
						param->type=state;
						param->name.len=tmp-param->name.s;
						state=F_VIA;
						goto endofvalue;
					case FIN_BRANCH:
					case FIN_MADDR:
					case FIN_TTL:
					case FIN_RECEIVED:
					case FIN_I:
#ifdef USE_COMP
					case FIN_COMP:
#endif
						LOG(L_ERR, "ERROR: parse_via_param: new via found" 
								"(',') when '=' expected (state %d=)\n",
								state);
						goto error; /* or we could ignore this bad param*/
					case F_CR:
					case F_LF:
					case F_CRLF:
						state=END_OF_HEADER;
						goto end_via;
					case GEN_PARAM:
					default:
						param->type=GEN_PARAM;
						param->name.len=tmp-param->name.s;
						state=F_VIA;
						goto endofvalue;
				}
				break; 

				/* param names */
			case 'h':
			case 'H':
				switch(state){
					case F_PARAM:
						state=HIDDEN1;
						param->name.s=tmp;
						break;
					case BRANCH5:
						state=FIN_BRANCH;
						break;
					case GEN_PARAM:
						break;
					case F_CR:
					case F_LF:
					case F_CRLF:
						state=END_OF_HEADER;
						goto end_via;
					default:
						state=GEN_PARAM;
				}
				break;
			case 'i':
			case 'I':
				switch(state){
					case F_PARAM:
						state=FIN_I;
						param->name.s=tmp;
						break;
					case HIDDEN1:
						state=HIDDEN2;
						break;
					case RECEIVED4:
						state=RECEIVED5;
						break;
					case ALIAS2:
						state=ALIAS3;
						break;
					case GEN_PARAM:
						break;
					case F_CR:
					case F_LF:
					case F_CRLF:
						state=END_OF_HEADER;
						goto end_via;
					default:
						state=GEN_PARAM;
				}
				break;
			case 'd':
			case 'D':
				switch(state){
					case F_PARAM:
						state=GEN_PARAM;
						param->name.s=tmp;
						break;
					case HIDDEN2:
						state=HIDDEN3;
						break;
					case HIDDEN3:
						state=HIDDEN4;
						break;
					case MADDR2:
						state=MADDR3;
						break;
					case MADDR3:
						state=MADDR4;
						break;
					case RECEIVED7:
						state=FIN_RECEIVED;
						break;
					case GEN_PARAM:
						break;
					case F_CR:
					case F_LF:
					case F_CRLF:
						state=END_OF_HEADER;
						goto end_via;
					default:
						state=GEN_PARAM;
				}
				break;
			case 'e':
			case 'E':
				switch(state){
					case F_PARAM:
						state=GEN_PARAM;
						param->name.s=tmp;
						break;
					case HIDDEN4:
						state=HIDDEN5;
						break;
					case RECEIVED1:
						state=RECEIVED2;
						break;
					case RECEIVED3:
						state=RECEIVED4;
						break;
					case RECEIVED6:
						state=RECEIVED7;
						break;
					case GEN_PARAM:
						break;
					case F_CR:
					case F_LF:
					case F_CRLF:
						state=END_OF_HEADER;
						goto end_via;
					default:
						state=GEN_PARAM;
				}
				break;
			case 'n':
			case 'N':
				switch(state){
					case F_PARAM:
						state=GEN_PARAM;
						param->name.s=tmp;
						break;
					case HIDDEN5:
						state=FIN_HIDDEN;
						break;
					case BRANCH3:
						state=BRANCH4;
						break;
					case GEN_PARAM:
						break;
					case F_CR:
					case F_LF:
					case F_CRLF:
						state=END_OF_HEADER;
						goto end_via;
					default:
						state=GEN_PARAM;
				}
				break;
			case 't':
			case 'T':
				switch(state){
					case F_PARAM:
						state=TTL1;
						param->name.s=tmp;
						break;
					case TTL1:
						state=TTL2;
						break;
					case RPORT3:
						state=FIN_RPORT;
						break;
					case GEN_PARAM:
						break;
					case F_CR:
					case F_LF:
					case F_CRLF:
						state=END_OF_HEADER;
						goto end_via;
					default:
						state=GEN_PARAM;
				}
				break;
			case 'l':
			case 'L':
				switch(state){
					case F_PARAM:
						state=GEN_PARAM;
						param->name.s=tmp;
						break;
					case TTL2:
						state=FIN_TTL;
						break;
					case ALIAS1:
						state=ALIAS2;
						break;
					case GEN_PARAM:
						break;
					case F_CR:
					case F_LF:
					case F_CRLF:
						state=END_OF_HEADER;
						goto end_via;
					default:
						state=GEN_PARAM;
				}
				break;
			case 'm':
			case 'M':
				switch(state){
					case F_PARAM:
						state=MADDR1;
						param->name.s=tmp;
						break;
#ifdef USE_COMP
					case COMP2:
						state=COMP3;
						break;
#endif
					case GEN_PARAM:
						break;
					case F_CR:
					case F_LF:
					case F_CRLF:
						state=END_OF_HEADER;
						goto end_via;
					default:
						state=GEN_PARAM;
				}
				break;
			case 'a':
			case 'A':
				switch(state){
					case F_PARAM:
						state=ALIAS1;
						param->name.s=tmp;
						break;
					case MADDR1:
						state=MADDR2;
						break;
					case BRANCH2:
						state=BRANCH3;
						break;
					case ALIAS3:
						state=ALIAS4;
						break;
					case GEN_PARAM:
						break;
					case F_CR:
					case F_LF:
					case F_CRLF:
						state=END_OF_HEADER;
						goto end_via;
					default:
						state=GEN_PARAM;
				}
				break;
			case 'r':
			case 'R':
				switch(state){
					case MADDR4:
						state=FIN_MADDR;
						break;
					case F_PARAM:
						state=RECEIVED1;
						param->name.s=tmp;
						break;
					case BRANCH1:
						state=BRANCH2;
						break;
					case RPORT2:
						state=RPORT3;
						break;
					case GEN_PARAM:
						break;
					case F_CR:
					case F_LF:
					case F_CRLF:
						state=END_OF_HEADER;
						goto end_via;
					default:
						state=GEN_PARAM;
				}
				break;
			case 'c':
			case 'C':
				switch(state){
					case F_PARAM:
#ifdef USE_COMP
						state=COMP1;
#else
						state=GEN_PARAM;
#endif
						param->name.s=tmp;
						break;
					case RECEIVED2:
						state=RECEIVED3;
						break;
					case BRANCH4:
						state=BRANCH5;
						break;
					case GEN_PARAM:
						break;
					case F_CR:
					case F_LF:
					case F_CRLF:
						state=END_OF_HEADER;
						goto end_via;
					default:
						state=GEN_PARAM;
				}
				break;
			case 'v':
			case 'V':
				switch(state){
					case F_PARAM:
						state=GEN_PARAM;
						param->name.s=tmp;
						break;
					case RECEIVED5:
						state=RECEIVED6;
						break;
					case GEN_PARAM:
						break;
					case F_CR:
					case F_LF:
					case F_CRLF:
						state=END_OF_HEADER;
						goto end_via;
					default:
						state=GEN_PARAM;
				}
				break;
			case 'b':
			case 'B':
				switch(state){
					case F_PARAM:
						state=BRANCH1;
						param->name.s=tmp;
						break;
					case GEN_PARAM:
						break;
					case F_CR:
					case F_LF:
					case F_CRLF:
						state=END_OF_HEADER;
						goto end_via;
					default:
						state=GEN_PARAM;
				}
				break;
			case 'p':
			case 'P':
				switch(state){
					case F_PARAM:
						state=GEN_PARAM;
						param->name.s=tmp;
						break;
					case RECEIVED1:
						state=RPORT1;
						break;
#ifdef USE_COMP
					case COMP3:
						state=FIN_COMP;
						break;
#endif
					case F_CR:
					case F_LF:
					case F_CRLF:
						state=END_OF_HEADER;
						goto end_via;
					default:
						state=GEN_PARAM;
				}
				break;
			case 'o':
			case 'O':
				switch(state){
					case F_PARAM:
						state=GEN_PARAM;
						param->name.s=tmp;
						break;
					case RPORT1:
						state=RPORT2;
						break;
#ifdef USE_COMP
					case COMP1:
						state=COMP2;
						break;
#endif
					case F_CR:
					case F_LF:
					case F_CRLF:
						state=END_OF_HEADER;
						goto end_via;
					default:
						state=GEN_PARAM;
				}
				break;
			case 's':
			case 'S':
				switch(state){
					case F_PARAM:
						state=GEN_PARAM;
						param->name.s=tmp;
						break;
					case ALIAS4:
						state=FIN_ALIAS;
						break;
					case F_CR:
					case F_LF:
					case F_CRLF:
						state=END_OF_HEADER;
						goto end_via;
					default:
						state=GEN_PARAM;
				}
				break;
			default:
				switch(state){
					case F_PARAM:
						state=GEN_PARAM;
						param->name.s=tmp;
						break;
					case GEN_PARAM:
						break;
					case F_CR:
					case F_LF:
					case F_CRLF:
						state=END_OF_HEADER;
						goto end_via;
					default:
						state=GEN_PARAM;
				}
		}
	}/* for tmp*/

	/* end of packet? => error, no cr/lf,',' found!!!*/
	saved_state=state;
	state=END_OF_HEADER;
	goto error;
	
 find_value:
	tmp++;
	for(;*tmp;tmp++){
		switch(*tmp){
			case ' ':
			case '\t':
				switch(state){
					case L_VALUE:
					case F_VALUE: /*eat space*/
						break; 
					case P_VALUE:
						state=L_PARAM;
						param->value.len=tmp-param->value.s;
						goto endofvalue;
#ifdef USE_COMP
					case L_COMP_VALUE:
					case F_COMP_VALUE:
						break; /* eat space */
					case FIN_V_SIGCOMP_P:
						state=L_PARAM;
						param->value.len=tmp-param->value.s;
						vb->comp_no=COMP_SIGCOMP;
						goto endofvalue;
					case FIN_V_SERGZ_Z:
						state=L_PARAM;
						param->value.len=tmp-param->value.s;
						vb->comp_no=COMP_SIGCOMP;
						goto endofvalue;
					comp_states_cases
						state=L_PARAM;
						param->value.len=tmp-param->value.s;
						comp_unexpected_char;
						goto endofvalue;
#endif
					case P_STRING:
						break;
					case F_CR:
					case F_LF:
					case F_CRLF:
						state=saved_state;
						break;
					default:
						LOG(L_ERR, "ERROR: parse_via: invalid char <%c>"
								" in state %d\n", *tmp, state);
						goto error;
				}
				break;
			case '\n':
				switch(state){
					case L_VALUE:
					case F_VALUE: /*eat space*/
#ifdef USE_COMP
					case L_COMP_VALUE:
					case F_COMP_VALUE:
#endif
					case P_STRING:
						saved_state=state;
						param->size=tmp-param->start;
						state=F_LF;
						break;
					case P_VALUE:
						saved_state=L_PARAM;
						state=F_LF;
						param->value.len=tmp-param->value.s;
						goto endofvalue;
#ifdef USE_COMP
					case FIN_V_SIGCOMP_P:
						saved_state=L_PARAM;
						state=F_LF;
						param->value.len=tmp-param->value.s;
						vb->comp_no=COMP_SIGCOMP;
						goto endofvalue;
					case FIN_V_SERGZ_Z:
						saved_state=L_PARAM;
						state=F_LF;
						param->value.len=tmp-param->value.s;
						vb->comp_no=COMP_SIGCOMP;
						goto endofvalue;
					comp_states_cases
						saved_state=L_PARAM;
						state=F_LF;
						param->value.len=tmp-param->value.s;
						comp_unexpected_char;
						goto endofvalue;
#endif
					case F_LF:
					case F_CRLF:
						state=END_OF_HEADER;
						goto end_via;
					case F_CR:
						state=F_CRLF;
						break;
					default:
						LOG(L_ERR, "ERROR: parse_via: invalid char <%c>"
								" in state %d\n", *tmp, state);
						goto error;
				}
				break;
			case '\r':
				switch(state){
					case L_VALUE:
					case F_VALUE: /*eat space*/
#ifdef USE_COMP
					case L_COMP_VALUE:
					case F_COMP_VALUE:
#endif
					case P_STRING:
						saved_state=state;
						param->size=tmp-param->start;
						state=F_CR;
						break;
					case P_VALUE:
						param->value.len=tmp-param->value.s;
						saved_state=L_PARAM;
						state=F_CR;
						goto endofvalue;
#ifdef USE_COMP
					case FIN_V_SIGCOMP_P:
						saved_state=L_PARAM;
						state=F_CR;
						param->value.len=tmp-param->value.s;
						vb->comp_no=COMP_SIGCOMP;
						goto endofvalue;
					case FIN_V_SERGZ_Z:
						saved_state=L_PARAM;
						state=F_CR;
						param->value.len=tmp-param->value.s;
						vb->comp_no=COMP_SIGCOMP;
						goto endofvalue;
					comp_states_cases
						saved_state=L_PARAM;
						state=F_CR;
						param->value.len=tmp-param->value.s;
						comp_unexpected_char;
						goto endofvalue;
#endif
					case F_LF:
					case F_CR:
					case F_CRLF:
						state=END_OF_HEADER;
						goto end_via;
					default:
						LOG(L_ERR, "ERROR: parse_via: invalid char <%c>"
								" in state %d\n", *tmp, state);
						goto error;
				}
				break;

			case '=':
				switch(state){
					case L_VALUE:
						state=F_VALUE;
						break;
#ifdef USE_COMP
					case L_COMP_VALUE:
						state=F_COMP_VALUE;
						break;
					/* '=' in any other COMP value state is an error,
					 * and it will be catched by the default branch */
#endif
					case P_STRING:
						break;
					case F_LF:
					case F_CR:
					case F_CRLF:
						state=END_OF_HEADER;
						goto end_via;
					default:
						LOG(L_ERR, "ERROR: parse_via: invalid char <%c>"
								" in state %d\n", *tmp, state);
						goto error;
				}
				break;
			case ';':
				switch(state){
					case P_VALUE:
						param->value.len=tmp-param->value.s;
						state=F_PARAM;
						goto endofvalue;
					case F_VALUE:
						param->value.len=0;
						state=F_PARAM;
						goto endofvalue;
					case P_STRING:
						break; /* what to do? */
					case F_LF:
					case F_CR:
					case F_CRLF:
						state=END_OF_HEADER;
						goto end_via;
#ifdef USE_COMP
					case L_COMP_VALUE:
						comp_unexpected_char;
						/* we want to contine with no comp */
						state=F_PARAM;
						param->value.len=0;
						param->value.s=0;
						goto endofvalue;
					case F_COMP_VALUE:
						comp_unexpected_char;
						param->value.len=0;
						state=F_PARAM;
						goto endofvalue;
					comp_states_cases
						comp_unexpected_char;
						param->value.len=tmp-param->value.s;
						state=F_PARAM;
						goto endofvalue;
					case FIN_V_SIGCOMP_P:
						vb->comp_no=COMP_SIGCOMP;
						param->value.len=tmp-param->value.s;
						state=F_PARAM;
						goto endofvalue;
					case FIN_V_SERGZ_Z:
						vb->comp_no=COMP_SIGCOMP;
						param->value.len=tmp-param->value.s;
						state=F_PARAM;
						goto endofvalue;
#endif
					case L_VALUE:
						param->value.len=0;
						param->value.s=0; /* null value */
						state=F_PARAM;
						goto endofvalue;
					default:
						LOG(L_ERR, "ERROR: parse_via: invalid char <%c>"
								" in state %d\n", *tmp, state);
						goto error;
				}
				break;
			case ',':
				switch(state){
					case P_VALUE:
						param->value.len=tmp-param->value.s;
						state=F_VIA;
						goto endofvalue;
					case P_STRING:
					case F_LF:
					case F_CR:
					case F_CRLF:
						state=END_OF_HEADER;
						goto end_via;
#ifdef USE_COMP
					case L_COMP_VALUE:
						comp_unexpected_char;
						/* we want to contine with no comp */
						state=F_VIA;
						param->value.len=0;
						param->value.s=0;
						goto endofvalue;
					case F_COMP_VALUE:
						comp_unexpected_char;
						param->value.len=0;
						state=F_VIA;
						goto endofvalue;
					comp_states_cases
						comp_unexpected_char;
						param->value.len=tmp-param->value.s;
						state=F_VIA;
						goto endofvalue;
					case FIN_V_SIGCOMP_P:
						vb->comp_no=COMP_SIGCOMP;
						param->value.len=tmp-param->value.s;
						state=F_VIA;
						goto endofvalue;
					case FIN_V_SERGZ_Z:
						vb->comp_no=COMP_SIGCOMP;
						param->value.len=tmp-param->value.s;
						state=F_VIA;
						goto endofvalue;
#endif
					case L_VALUE:
						if (param->type==FIN_RPORT){
							param->value.len=0;
							param->value.s=0; /* null value */
							state=F_VIA;
							goto endofvalue;
						};
						/* no break */
					default:
						LOG(L_ERR, "ERROR: parse_via: invalid char <%c>"
								" in state %d\n", *tmp, state);
						goto error;
				}
				break; /* what to do? */
			case '"':
				switch(state){
					case F_VALUE:
						state=P_STRING;
						param->value.s=tmp+1;
						break;
					case P_STRING:
						state=L_PARAM;
						param->value.len=tmp-param->value.s;
						goto endofvalue;
					case F_LF:
					case F_CR:
					case F_CRLF:
						state=END_OF_HEADER;
						goto end_via;
					default:
						LOG(L_ERR, "ERROR: parse_via: invalid char <%c>"
								" in state %d\n", *tmp, state);
						goto error;
				}
				break;
#ifdef USE_COMP
			value_case('s', 'S', F_COMP_VALUE, V_COMP_S, 1);
			value_case('i', 'I', V_COMP_S, V_SIGCOMP_I, 0);
			value_case_double('g', 'G', V_SIGCOMP_I, V_SIGCOMP_G,
					                    V_SERGZ_R,   V_SERGZ_G);
			value_case('c', 'C', V_SIGCOMP_G, V_SIGCOMP_C, 0);
			value_case('o', 'O', V_SIGCOMP_C, V_SIGCOMP_O, 0);
			value_case('m', 'M', V_SIGCOMP_O, V_SIGCOMP_M, 0);
			value_case('p', 'P', V_SIGCOMP_M, FIN_V_SIGCOMP_P, 0);
			
			value_case('e', 'E', V_COMP_S, V_SERGZ_E, 0);
			value_case('r', 'R', V_SERGZ_E, V_SERGZ_R, 0);
			value_case('z', 'Z', V_SERGZ_G, FIN_V_SERGZ_Z, 0);
#endif /* USE_COMP */
				
			default:
				switch(state){
					case F_VALUE:
						state=P_VALUE;
						param->value.s=tmp;
						break;
					case P_VALUE:
					case P_STRING:
						break;
					case F_LF:
					case F_CR:
					case F_CRLF:
						state=END_OF_HEADER;
						goto end_via;
					default:
#ifdef USE_COMP
						switch(state){
							case F_COMP_VALUE:
								comp_unexpected_char;
								state=P_VALUE;
								param->value.s=tmp;
								break;
							comp_states_cases
							comp_fin_states_cases
								comp_unexpected_char;
								state=P_VALUE;
								break;
							default:
								LOG(L_ERR, "ERROR: parse_via_param: invalid "
									"char <%c> in state %d\n", *tmp, state);
								goto error;
						}
#else
						LOG(L_ERR, "ERROR: parse_via: invalid char <%c>"
								" in state %d\n", *tmp, state);
						goto error;
#endif /* USE_COMP */
				}
		}
	} /* for2 tmp*/

	/* end of buff and no CR/LF =>error*/
	saved_state=state;
	state=END_OF_HEADER;
	goto error;
	
 endofparam:
 endofvalue:
	param->size=tmp-param->start;
normal_exit:
	*pstate=state;
	*psaved_state=saved_state;
	DBG("Found param type %d, <%.*s> = <%.*s>; state=%d\n", param->type,
			param->name.len, ZSW(param->name.s), 
			(param->value.len?param->value.len:3),
			(param->value.len?param->value.s:"n/a"), state);
	return tmp;
	
 end_via:
	     /* if we are here we found an "unexpected" end of via
	      *  (cr/lf). This is valid only if the param type is GEN_PARAM or
		  *  RPORT (the only ones which can miss the value; HIDDEN is a 
		  *  special case )*/
	if ((param->type==GEN_PARAM)||(param->type==PARAM_RPORT)){
		saved_state=L_PARAM; /* change the saved_state, we have an unknown
		                        param. w/o a value */
		/* param->size should be computed before */
		goto normal_exit;
	}
	*pstate=state;
	*psaved_state=saved_state;
	DBG("Error on  param type %d, <%.*s>, state=%d, saved_state=%d\n",
		param->type, param->name.len, ZSW(param->name.s), state, saved_state);

 error:
	LOG(L_ERR, "error: parse_via_param\n");
	param->type=PARAM_ERROR;
	*pstate=PARAM_ERROR;
	*psaved_state=state;
	return tmp;
}



/*
 * call it with a vb initialized to 0
 * returns: pointer after the parsed parts and sets vb->error
 * WARNING: don't forget to cleanup on error with free_via_list(vb)!
 */
char* parse_via(char* buffer, const char* const end, struct via_body* const vbody)
{
	char* tmp;
	char* param_start;
	unsigned char state;
	unsigned char saved_state;
	int c_nest;
	int err;
	struct via_body* vb;
	struct via_param* param;

	vb=vbody; /* keep orignal vbody value, needed to set the error member
				 in case of multiple via bodies in the same header */
parse_again:
	vb->error=PARSE_ERROR;
	/* parse start of via ( SIP/2.0/UDP    )*/
	state=F_SIP;
	saved_state=F_SIP; /* fixes gcc 4.0 warning */
	param_start=0;
	for(tmp=buffer;tmp<end;tmp++){
		switch(*tmp){
			case ' ':
			case'\t':
				switch(state){
					case L_VER: /* eat space */
					case L_PROTO:
					case F_SIP:
					case F_VER:
					case F_PROTO:
						break;
					case FIN_UDP:
						vb->transport.len=tmp-vb->transport.s;
						vb->proto=PROTO_UDP;
						state=F_HOST; /* start looking for host*/
						goto main_via;
					case FIN_TCP:
						/* finished proto parsing */
						vb->transport.len=tmp-vb->transport.s;
						vb->proto=PROTO_TCP;
						state=F_HOST; /* start looking for host*/
						goto main_via;
					case FIN_TLS:
						/* finished proto parsing */
						vb->transport.len=tmp-vb->transport.s;
						vb->proto=PROTO_TLS;
						state=F_HOST; /* start looking for host*/
						goto main_via;
					case FIN_SCTP:
						/* finished proto parsing */
						vb->transport.len=tmp-vb->transport.s;
						vb->proto=PROTO_SCTP;
						state=F_HOST; /* start looking for host*/
						goto main_via;
					case WS_WSS2:
						/* finished proto parsing */
						vb->transport.len=tmp-vb->transport.s;
						vb->proto=PROTO_WS;
						state=F_HOST; /* start looking for host*/
						goto main_via;
					case FIN_WSS:
						/* finished proto parsing */
						vb->transport.len=tmp-vb->transport.s;
						vb->proto=PROTO_WSS;
						state=F_HOST; /* start looking for host*/
						goto main_via;
					case OTHER_PROTO:
						/* finished proto parsing */
						vb->transport.len=tmp-vb->transport.s;
						vb->proto=PROTO_OTHER;
						state=F_HOST; /* start looking for host*/
						goto main_via;
					case UDP1:
					case UDP2:
					case TCP_TLS1:
					case TCP2:
					case TLS2:
					case SCTP1:
					case SCTP2:
					case SCTP3:
					case WS_WSS1:
						/* finished proto parsing */
						vb->transport.len=tmp-vb->transport.s;
						vb->proto=PROTO_OTHER;
						state=F_HOST; /* start looking for host*/
						goto main_via;
					case FIN_SIP:
						vb->name.len=tmp-vb->name.s;
						state=L_VER;
						break;
					case FIN_VER:
						vb->version.len=tmp-vb->version.s;
						state=L_PROTO;
						break;
					case F_LF:
					case F_CRLF:
					case F_CR: /* header continues on this line */
						state=saved_state;
						break;
					default:
						LOG(L_ERR, "ERROR: parse_via: bad char <%c> on"
								" state %d\n", *tmp, state);
						goto error;
				}
				break;
			case '\n':
				switch(state){
					case L_VER:
					case F_SIP:
					case F_VER:
					case F_PROTO:
					case L_PROTO:
						saved_state=state;
						state=F_LF;
						break;
					case FIN_UDP:
						vb->transport.len=tmp-vb->transport.s;
						vb->proto=PROTO_UDP;
						state=F_LF;
						saved_state=F_HOST; /* start looking for host*/
						goto main_via;
					case FIN_TCP:
						vb->transport.len=tmp-vb->transport.s;
						vb->proto=PROTO_TCP;
						state=F_LF;
						saved_state=F_HOST; /* start looking for host*/
						goto main_via;
					case FIN_TLS:
						vb->transport.len=tmp-vb->transport.s;
						vb->proto=PROTO_TLS;
						state=F_LF;
						saved_state=F_HOST; /* start looking for host*/
						goto main_via;
					case FIN_SCTP:
						/* finished proto parsing */
						vb->transport.len=tmp-vb->transport.s;
						vb->proto=PROTO_SCTP;
						state=F_LF;
						saved_state=F_HOST; /* start looking for host*/
						goto main_via;
					case WS_WSS2:
						/* finished proto parsing */
						vb->transport.len=tmp-vb->transport.s;
						vb->proto=PROTO_WS;
						state=F_LF;
						saved_state=F_HOST; /* start looking for host*/
						goto main_via;
					case FIN_WSS:
						/* finished proto parsing */
						vb->transport.len=tmp-vb->transport.s;
						vb->proto=PROTO_WSS;
						state=F_LF;
						saved_state=F_HOST; /* start looking for host*/
						goto main_via;
					case OTHER_PROTO:
						/* finished proto parsing */
						vb->transport.len=tmp-vb->transport.s;
						vb->proto=PROTO_OTHER;
						state=F_LF;
						saved_state=F_HOST; /* start looking for host*/
						goto main_via;
					case UDP1:
					case UDP2:
					case TCP_TLS1:
					case TCP2:
					case TLS2:
					case SCTP1:
					case SCTP2:
					case SCTP3:
					case WS_WSS1:
						/* finished proto parsing */
						vb->transport.len=tmp-vb->transport.s;
						vb->proto=PROTO_OTHER;
						state=F_LF;
						saved_state=F_HOST; /* start looking for host*/
						goto main_via;
					case FIN_SIP:
						vb->name.len=tmp-vb->name.s;
						state=F_LF;
						saved_state=L_VER;
						break;
					case FIN_VER:
						vb->version.len=tmp-vb->version.s;
						state=F_LF;
						saved_state=L_PROTO;
						break;
					case F_CR:
						state=F_CRLF;
						break;
					case F_LF:
					case F_CRLF:
						state=saved_state;
						goto endofheader;
					default:
						LOG(L_ERR, "ERROR: parse_via: bad char <%c> on"
								" state %d\n", *tmp, state);
						goto error;
				}
				break;
			case '\r':
				switch(state){
					case L_VER:
					case F_SIP:
					case F_VER:
					case F_PROTO:
					case L_PROTO:
						saved_state=state;
						state=F_CR;
						break;
					case FIN_UDP:
						vb->transport.len=tmp-vb->transport.s;
						vb->proto=PROTO_UDP;
						state=F_CR;
						saved_state=F_HOST;
						goto main_via;
					case FIN_TCP:
						vb->transport.len=tmp-vb->transport.s;
						vb->proto=PROTO_TCP;
						state=F_CR;
						saved_state=F_HOST;
						goto main_via;
					case FIN_TLS:
						vb->transport.len=tmp-vb->transport.s;
						vb->proto=PROTO_TLS;
						state=F_CR;
						saved_state=F_HOST;
						goto main_via;
					case FIN_SCTP:
						vb->transport.len=tmp-vb->transport.s;
						vb->proto=PROTO_SCTP;
						state=F_CR;
						saved_state=F_HOST;
						goto main_via;
					case WS_WSS2:
						vb->transport.len=tmp-vb->transport.s;
						vb->proto=PROTO_WS;
						state=F_CR;
						saved_state=F_HOST;
						goto main_via;
					case FIN_WSS:
						vb->transport.len=tmp-vb->transport.s;
						vb->proto=PROTO_WSS;
						state=F_CR;
						saved_state=F_HOST;
						goto main_via;
					case OTHER_PROTO:
						vb->transport.len=tmp-vb->transport.s;
						vb->proto=PROTO_OTHER;
						state=F_CR;
						saved_state=F_HOST;
						goto main_via;
					case UDP1:
					case UDP2:
					case TCP_TLS1:
					case TCP2:
					case TLS2:
					case SCTP1:
					case SCTP2:
					case SCTP3:
					case WS_WSS1:
						/* finished proto parsing */
						vb->transport.len=tmp-vb->transport.s;
						vb->proto=PROTO_OTHER;
						state=F_CR;
						saved_state=F_HOST;
						goto main_via;
					case FIN_SIP:
						vb->name.len=tmp-vb->name.s;
						state=F_CR;
						saved_state=L_VER;
						break;
					case FIN_VER:
						vb->version.len=tmp-vb->version.s;
						state=F_CR;
						saved_state=L_PROTO;
						break;
					case F_LF: /*end of line ?next header?*/
					case F_CR:
					case F_CRLF:
						state=saved_state;
						goto endofheader;
					default:
						LOG(L_ERR, "ERROR: parse_via: bad char <%c> on"
								" state %d\n", *tmp, state);
						goto error;
				}
				break;
			
			case '/':
				switch(state){
					case FIN_SIP:
						vb->name.len=tmp-vb->name.s;
						state=F_VER;
						break;
					case FIN_VER:
						vb->version.len=tmp-vb->version.s;
						state=F_PROTO;
						break;
					case L_VER:
						state=F_VER;
						break;
					case L_PROTO:
						state=F_PROTO;
						break;
					default:
						LOG(L_ERR, "ERROR: parse_via: bad char <%c> on"
								" state %d\n", *tmp, state);
						goto error;
				}
				break;
				/* match SIP*/
			case 'S':
			case 's':
				switch(state){
					case F_SIP:
						state=SIP1;
						vb->name.s=tmp;
						break;
					case TLS2:
						state=FIN_TLS;
						break;
					case WS_WSS1:
						state=WS_WSS2;
						break;
					case WS_WSS2:
						state=FIN_WSS;
						break;
					case F_PROTO:
						state=SCTP1;
						vb->transport.s=tmp;
						break;
					case OTHER_PROTO:
						break;
					case UDP1:
					case UDP2:
					case FIN_UDP:
					case TCP_TLS1:
					case TCP2:
					case FIN_TCP:
					case FIN_TLS:
					case SCTP1:
					case SCTP2:
					case SCTP3:
					case FIN_WSS:
					case FIN_SCTP:
						state=OTHER_PROTO;
						break;
					default:
						LOG(L_ERR, "ERROR: parse_via: bad char <%c> on"
								" state %d\n", *tmp, state);
						goto error;
				}
				break;
			case 'I':
			case 'i':
				switch(state){
					case SIP1:
						state=SIP2;
						break;
					case OTHER_PROTO:
						break;
					case UDP1:
					case UDP2:
					case FIN_UDP:
					case TCP_TLS1:
					case TCP2:
					case FIN_TCP:
					case TLS2:
					case FIN_TLS:
					case SCTP1:
					case SCTP2:
					case SCTP3:
					case FIN_SCTP:
					case WS_WSS1:
					case WS_WSS2:
					case FIN_WSS:
						state=OTHER_PROTO;
						break;
					default:
						LOG(L_ERR, "ERROR: parse_via: bad char <%c> on"
								" state %d\n", *tmp, state);
						goto error;
				}
				break;
			case 'p':
			case 'P':
				switch(state){
					case SIP2:
						state=FIN_SIP;
						break;
					/* allow p in PROTO */
					case UDP2:
						state=FIN_UDP;
						break;
					case TCP2:
						state=FIN_TCP;
						break;
					case SCTP3:
						state=FIN_SCTP;
						break;
					case OTHER_PROTO:
						break;
					case UDP1:
					case FIN_UDP:
					case TCP_TLS1:
					case FIN_TCP:
					case TLS2:
					case FIN_TLS:
					case SCTP1:
					case SCTP2:
					case FIN_SCTP:
					case WS_WSS1:
					case WS_WSS2:
					case FIN_WSS:
						state=OTHER_PROTO;
						break;
					default:
						LOG(L_ERR, "ERROR: parse_via: bad char <%c> on"
								" state %d\n", *tmp, state);
						goto error;
				}
				break;
			case 'U':
			case 'u':
				switch(state){
					case F_PROTO:
						state=UDP1;
						vb->transport.s=tmp;
						break;
					case OTHER_PROTO:
						break;
					case UDP1:
					case UDP2:
					case FIN_UDP:
					case TCP_TLS1:
					case TCP2:
					case FIN_TCP:
					case TLS2:
					case FIN_TLS:
					case SCTP1:
					case SCTP2:
					case SCTP3:
					case FIN_SCTP:
					case WS_WSS1:
					case WS_WSS2:
					case FIN_WSS:
						state=OTHER_PROTO;
						break;
					default:
						LOG(L_ERR, "ERROR: parse_via: bad char <%c> on"
								" state %d\n", *tmp, state);
						goto error;
				}
				break;
			case 'D':
			case 'd':
				switch(state){
					case UDP1:
						state=UDP2;
						break;
					case OTHER_PROTO:
						break;
					case UDP2:
					case FIN_UDP:
					case TCP_TLS1:
					case TCP2:
					case FIN_TCP:
					case TLS2:
					case FIN_TLS:
					case SCTP1:
					case SCTP2:
					case SCTP3:
					case FIN_SCTP:
					case WS_WSS1:
					case WS_WSS2:
					case FIN_WSS:
						state=OTHER_PROTO;
						break;
					default:
						LOG(L_ERR, "ERROR: parse_via: bad char <%c> on"
								" state %d\n", *tmp, state);
						goto error;
				}
				break;
			case 'T':
			case 't':
				switch(state){
					case F_PROTO:
						state=TCP_TLS1;
						vb->transport.s=tmp;
						break;
					case SCTP2:
						state=SCTP3;
						break;
					case OTHER_PROTO:
						break;
					case UDP1:
					case UDP2:
					case FIN_UDP:
					case TCP_TLS1:
					case TCP2:
					case FIN_TCP:
					case TLS2:
					case FIN_TLS:
					case SCTP1:
					case SCTP3:
					case FIN_SCTP:
					case WS_WSS1:
					case WS_WSS2:
					case FIN_WSS:
						state=OTHER_PROTO;
						break;
					default:
						LOG(L_ERR, "ERROR: parse_via: bad char <%c> on"
								" state %d\n", *tmp, state);
						goto error;
				}
				break;
			case 'C':
			case 'c':
				switch(state){
					case TCP_TLS1:
						state=TCP2;
						break;
					case SCTP1:
						state=SCTP2;
						break;
					case OTHER_PROTO:
						break;
					case UDP1:
					case UDP2:
					case FIN_UDP:
					case TCP2:
					case FIN_TCP:
					case TLS2:
					case FIN_TLS:
					case SCTP2:
					case SCTP3:
					case FIN_SCTP:
					case WS_WSS1:
					case WS_WSS2:
					case FIN_WSS:
						state=OTHER_PROTO;
						break;
					default:
						LOG(L_ERR, "ERROR: parse_via: bad char <%c> on"
								" state %d\n", *tmp, state);
						goto error;
				}
				break;
			case 'L':
			case 'l':
				switch(state){
					case TCP_TLS1:
						state=TLS2;
						break;
					case OTHER_PROTO:
						break;
					case UDP1:
					case UDP2:
					case FIN_UDP:
					case TCP2:
					case FIN_TCP:
					case TLS2:
					case FIN_TLS:
					case SCTP1:
					case SCTP2:
					case SCTP3:
					case FIN_SCTP:
					case WS_WSS1:
					case WS_WSS2:
					case FIN_WSS:
						state=OTHER_PROTO;
						break;
					default:
						LOG(L_ERR, "ERROR: parse_via: bad char <%c> on"
								" state %d\n", *tmp, state);
						goto error;
				}
				break;
			case 'W':
			case 'w':
				switch(state){
					case F_PROTO:
						state=WS_WSS1;
						vb->transport.s=tmp;
						break;
					case OTHER_PROTO:
						break;
					case UDP1:
					case UDP2:
					case FIN_UDP:
					case TCP_TLS1:
					case TCP2:
					case FIN_TCP:
					case TLS2:
					case FIN_TLS:
					case SCTP1:
					case SCTP2:
					case SCTP3:
					case FIN_SCTP:
					case WS_WSS1:
					case WS_WSS2:
					case FIN_WSS:
						state=OTHER_PROTO;
						break;
					default:
						LOG(L_ERR, "ERROR: parse_via: bad char <%c> on"
								" state %d\n", *tmp, state);
						goto error;
				}
				break;
			/*match 2.0*/
			case '2':
				switch(state){
					case F_VER:
						state=VER1;
						vb->version.s=tmp;
						break;
					case OTHER_PROTO:
						break;
					case UDP1:
					case UDP2:
					case FIN_UDP:
					case TCP_TLS1:
					case TCP2:
					case FIN_TCP:
					case TLS2:
					case FIN_TLS:
					case SCTP1:
					case SCTP2:
					case SCTP3:
					case FIN_SCTP:
					case WS_WSS1:
					case WS_WSS2:
					case FIN_WSS:
						state=OTHER_PROTO;
						break;
					default:
						LOG(L_ERR, "ERROR: parse_via: bad char <%c> on"
								" state %d\n", *tmp, state);
						goto error;
				}
				break;
			case '.':
				switch(state){
					case VER1:
						state=VER2;
						break;
					case OTHER_PROTO:
						break;
					case UDP1:
					case UDP2:
					case FIN_UDP:
					case TCP_TLS1:
					case TCP2:
					case FIN_TCP:
					case TLS2:
					case FIN_TLS:
					case SCTP1:
					case SCTP2:
					case SCTP3:
					case FIN_SCTP:
					case WS_WSS1:
					case WS_WSS2:
					case FIN_WSS:
						state=OTHER_PROTO;
						break;
					default:
						LOG(L_ERR, "ERROR: parse_via: bad char <%c> on"
								" state %d\n", *tmp, state);
						goto error;
				}
				 break;
			case '0':
				switch(state){
					case VER2:
						state=FIN_VER;
						break;
					case OTHER_PROTO:
						break;
					case UDP1:
					case UDP2:
					case FIN_UDP:
					case TCP_TLS1:
					case TCP2:
					case FIN_TCP:
					case TLS2:
					case FIN_TLS:
					case SCTP1:
					case SCTP2:
					case SCTP3:
					case FIN_SCTP:
					case WS_WSS1:
					case WS_WSS2:
					case FIN_WSS:
						state=OTHER_PROTO;
						break;
					default:
						LOG(L_ERR, "ERROR: parse_via: bad char <%c> on"
								" state %d\n", *tmp, state);
						goto error;
				}
				break;
			default:
				switch(state){
					case F_PROTO:
						state=OTHER_PROTO;
						vb->transport.s=tmp;
						break;
					case OTHER_PROTO:
						break;
					case UDP1:
					case UDP2:
					case FIN_UDP:
					case TCP_TLS1:
					case TCP2:
					case FIN_TCP:
					case TLS2:
					case FIN_TLS:
					case SCTP1:
					case SCTP2:
					case SCTP3:
					case FIN_SCTP:
					case WS_WSS1:
					case WS_WSS2:
					case FIN_WSS:
						state=OTHER_PROTO;
						break;
					default:
						LOG(L_ERR, "ERROR: parse_via: bad char <%c> on"
						" state %d\n", *tmp, state);
						goto error;
				}
				break;
		}
	} /* for tmp*/

	/* we should not be here! if everything is ok > main_via*/
	LOG(L_ERR, "ERROR: parse_via: bad via: end of packet on state=%d\n",
			state);
	goto error;

 main_via:
	/* inc tmp to point to the next char*/
	tmp++;
	c_nest=0;
	/*state should always be F_HOST here*/;
	for(;*tmp;tmp++){
		switch(*tmp){
		case ' ':
		case '\t':
			switch(state){
					case F_HOST:/*eat the spaces*/
						break;
					case P_HOST:
						 /*mark end of host*/
						 vb->host.len=tmp-vb->host.s;
						 state=L_PORT;
						 break;
					case L_PORT: /*eat the spaces*/
					case F_PORT:
						break;
					case P_PORT:
						/*end of port */
						vb->port_str.len=tmp-vb->port_str.s;
						state=L_PARAM;
						break;
					case L_PARAM: /* eat the space */
					case F_PARAM:
						break;
					case P_PARAM:
					/*	*tmp=0;*/ /*!?end of param*/
						state=L_PARAM;
						break;
					case L_VIA:
					case F_VIA: /* eat the space */
						break;
					case F_COMMENT:
					case P_COMMENT:
						break;
					case F_IP6HOST: /*no spaces allowed*/
					case P_IP6HOST:
						LOG(L_ERR, "ERROR:parse_via: bad ipv6 reference\n");
						goto error;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now =' '*/
						state=saved_state;
						break;
					default:
						LOG(L_CRIT,"BUG: parse_via"
							" on <%c>, state=%d\n",*tmp, state);
						goto  error;
				}
			break;
			case '\n':
				switch(state){
					case F_HOST:/*eat the spaces*/
					case L_PORT: /*eat the spaces*/
					case F_PORT:
					case L_PARAM: /* eat the space */
					case F_PARAM:
					case F_VIA: /* eat the space */
					case L_VIA:
					case F_COMMENT:
					case P_COMMENT:
					case F_IP6HOST:
					case P_IP6HOST:
						saved_state=state;
						state=F_LF;
						break;
					case P_HOST:
						 /*mark end of host*/
						 vb->host.len=tmp-vb->host.s;
						 saved_state=L_PORT;
						 state=F_LF;
						 break;
					case P_PORT:
						/*end of port */
						vb->port_str.len=tmp-vb->port_str.s;
						saved_state=L_PARAM;
						state=F_LF;
						break;
					case P_PARAM:
					/*	*tmp=0;*/ /*!?end of param*/
						saved_state=L_PARAM;
						state=F_LF;
						break;
					case F_CR:
						state=F_CRLF;
						break;
					case F_CRLF:
					case F_LF:
						state=saved_state;
						goto endofheader;
					default:
						LOG(L_CRIT,"BUG: parse_via"
							" on <%c>\n",*tmp);
						goto  error;
				}
			break;
		case '\r':
				switch(state){
					case F_HOST:/*eat the spaces*/
					case L_PORT: /*eat the spaces*/
					case F_PORT:
					case L_PARAM: /* eat the space */
					case F_PARAM:
					case F_VIA: /* eat the space */
					case L_VIA:
					case F_COMMENT:
					case P_COMMENT:
					case F_IP6HOST:
					case P_IP6HOST:
						saved_state=state;
						state=F_CR;
						break;
					case P_HOST:
						 /*mark end of host*/
						 vb->host.len=tmp-vb->host.s;
						 saved_state=L_PORT;
						 state=F_CR;
						 break;
					case P_PORT:
						/*end of port */
						vb->port_str.len=tmp-vb->port_str.s;
						saved_state=L_PARAM;
						state=F_CR;
						break;
					case P_PARAM:
					/*	*tmp=0;*/ /*!?end of param*/
						saved_state=L_PARAM;
						state=F_CR;
						break;
					case F_CRLF:
					case F_CR:
					case F_LF:
						state=saved_state;
						goto endofheader;
					default:
						LOG(L_CRIT,"BUG: parse_via"
							" on <%c>\n",*tmp);
						goto  error;
				}
			break;
			
			case ':':
				switch(state){
					case F_HOST:
					case F_IP6HOST:
						state=P_IP6HOST;
						break;
					case P_IP6HOST:
						break;
					case P_HOST:
						/*mark  end of host*/
						vb->host.len=tmp-vb->host.s;
						state=F_PORT;
						break;
					case L_PORT:
						state=F_PORT;
						break;
					case P_PORT:
						LOG(L_ERR, "ERROR:parse_via:"
							" bad port\n");
						goto error;
					case L_PARAM:
					case F_PARAM:
					case F_PORT:
					case P_PARAM:
						LOG(L_ERR, "ERROR:parse_via:"
						" bad char <%c> in state %d\n",
							*tmp,state);
						goto error;
					case L_VIA:
					case F_VIA:
						LOG(L_ERR, "ERROR:parse_via:"
						" bad char in compact via\n");
						goto error;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					case F_COMMENT:/*everything is allowed in a comment*/
						vb->comment.s=tmp;
						state=P_COMMENT;
						break;
					case P_COMMENT: /*everything is allowed in a comment*/
						break;
					default:
						LOG(L_CRIT,"BUG: parse_via"
							" on <%c> state %d\n",
							*tmp, state);
						goto error;
				}
				break;
			case ';':
				switch(state){
					case F_HOST:
					case F_IP6HOST:
						LOG(L_ERR,"ERROR:parse_via:"
							" no host found\n");
						goto error;
					case P_IP6HOST:
						LOG(L_ERR, "ERROR:parse_via: bad ipv6 reference\n");
						goto error;
					case P_HOST:
						vb->host.len=tmp-vb->host.s;
						state=F_PARAM;
						param_start=tmp+1;
						break;
					case P_PORT:
						/*mark the end*/
						vb->port_str.len=tmp-vb->port_str.s;
					case L_PORT:
					case L_PARAM:
						state=F_PARAM;
						param_start=tmp+1;
						break;
					case F_PORT:
						LOG(L_ERR, "ERROR:parse_via:"
						" bad char <%c> in state %d\n",
							*tmp,state);
						goto error;
					case F_PARAM:
						LOG(L_ERR,  "ERROR:parse_via:"
							" null param?\n");
						goto error;
					case P_PARAM:
						/*hmm next, param?*/
						state=F_PARAM;
						param_start=tmp+1;
						break;
					case L_VIA:
					case F_VIA:
						LOG(L_ERR, "ERROR:parse_via:"
						" bad char <%c> in next via\n",
							*tmp);
						goto error;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					case F_COMMENT:/*everything is allowed in a comment*/
						vb->comment.s=tmp;
						state=P_COMMENT;
						break;
					case P_COMMENT: /*everything is allowed in a comment*/
						break;
					
					default:
						LOG(L_CRIT,"BUG: parse_via"
							" on <%c> state %d\n",
							*tmp, state);
						goto  error;
				}
			break;
			case ',':
				switch(state){
					case F_HOST:
					case F_IP6HOST:
						LOG(L_ERR,"ERROR:parse_via:"
							" no host found\n");
						goto error;
					case P_IP6HOST:
						LOG(L_ERR, "ERROR:parse_via: bad ipv6 reference\n");
						goto error;
					case P_HOST:
						/*mark the end*/
						vb->host.len=tmp-vb->host.s;
						state=F_VIA;
						break;
					case P_PORT:
						/*mark the end*/
						vb->port_str.len=tmp-vb->port_str.s;
						state=F_VIA;
						break;
					case L_PORT:
					case L_PARAM:
					case P_PARAM:
					case L_VIA:
						state=F_VIA;
						break;
					case F_PORT:
					case F_PARAM:
						LOG(L_ERR, "ERROR:parse_via:"
						" invalid char <%c> in state"
						" %d\n", *tmp,state);
						goto error;
					case F_VIA:
						/* do  nothing,  eat ","*/
						break;	
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					case F_COMMENT:/*everything is allowed in a comment*/
						vb->comment.s=tmp;
						state=P_COMMENT;
						break;
					case P_COMMENT: /*everything is allowed in a comment*/
						break;
					default:
						LOG(L_CRIT,"BUG: parse_via"
							" on <%c> state %d\n",
							*tmp, state);
						goto  error;
				}
			break;
			case '(':
				switch(state){
					case F_HOST:
					case F_PORT:
					case F_PARAM:
					case F_VIA:
					case F_IP6HOST:
					case P_IP6HOST: /*must be terminated in ']'*/
						LOG(L_ERR,"ERROR:parse_via"
							" on <%c> state %d\n",
							*tmp, state);
						goto  error;
					case P_HOST:
						/*mark the end*/
						vb->host.len=tmp-vb->host.s;
						state=F_COMMENT;
						c_nest++;
						break;
					case P_PORT:
						/*mark the end*/
						vb->port_str.len=tmp-vb->port_str.s;
						state=F_COMMENT;
						c_nest++;
						break;
					case P_PARAM:
						/*mark the end*/
						vb->params.len=tmp-vb->params.s;
						state=F_COMMENT;
						c_nest++;
						break;
					case L_PORT:
					case L_PARAM:
					case L_VIA:
						state=F_COMMENT;
						vb->params.len=tmp-vb->params.s;
						c_nest++;
						break;
					case P_COMMENT:
					case F_COMMENT:
						c_nest++;
						break;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					default:
						LOG(L_CRIT,"BUG: parse_via"
							" on <%c> state %d\n",
							*tmp, state);
						goto  error;
				}
			break;
			case ')':
				switch(state){
					case F_COMMENT:
					case P_COMMENT:
						if (c_nest){
							c_nest--;
							if(c_nest==0){
								state=L_VIA;
								vb->comment.len=tmp-vb->comment.s;
								break;
							}
						}else{
							LOG(L_ERR,"ERROR:"
							    "parse_via: "
							    "missing '(' - "
							    "nesting = %d\n",
							    c_nest);
							 goto error;
						}
						break;
					case F_HOST:
					case F_PORT:
					case F_PARAM:
					case F_VIA:
					case P_HOST:
					case P_PORT:
					case P_PARAM:
					case L_PORT:
					case L_PARAM:
					case L_VIA:
					case F_IP6HOST:
					case P_IP6HOST:
						LOG(L_ERR,"ERROR:parse_via"
							" on <%c> state %d\n",
							*tmp, state);
						goto  error;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					default:
						LOG(L_CRIT,"BUG: parse_via"
							" on <%c> state %d\n",
							*tmp, state);
						goto  error;
				}
				break;
			case '[':
				switch(state){
					case F_HOST:
						vb->host.s=tmp; /* mark start here (include [])*/
						state=F_IP6HOST;
						break;
					case F_COMMENT:/*everything is allowed in a comment*/
						vb->comment.s=tmp;
						state=P_COMMENT;
						break;
					case P_COMMENT:
						break;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					default:
						LOG(L_ERR,"ERROR:parse_via"
							" on <%c> state %d\n",
							*tmp, state);
						goto  error;
				}
				break;
			case ']':
				switch(state){
					case P_IP6HOST:
						/*mark the end*/
						vb->host.len=(tmp-vb->host.s)+1; /* include "]" */
						state=L_PORT;
						break;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					case F_COMMENT:/*everything is allowed in a comment*/
						vb->comment.s=tmp;
						state=P_COMMENT;
						break;
					case P_COMMENT:
						break;
					default:
						LOG(L_ERR,"ERROR:parse_via"
							" on <%c> state %d\n",
							*tmp, state);
						goto  error;
				}
				break;
						
			default:
				switch(state){
					case F_HOST:
						state=P_HOST;
						vb->host.s=tmp;
						break;
					case P_HOST:
						break;
					case F_PORT:
						state=P_PORT;
						vb->port_str.s=tmp;
						break;
					case P_PORT:
						/*check if number?*/
						break;
					case F_PARAM:
						/*state=P_PARAM*/;
						if(vb->params.s==0) vb->params.s=param_start;
						param=pkg_malloc(sizeof(struct via_param));
						if (param==0){
							LOG(L_ERR, "ERROR:parse_via: mem. allocation"
									" error\n");
							goto error;
						}
						memset(param,0, sizeof(struct via_param));
						param->start=param_start;
						tmp=parse_via_param(tmp, end, &state, &saved_state,
											param, vb);

						switch(state){
							case F_PARAM:
								param_start=tmp+1;
							case L_PARAM:
							case F_LF:
							case F_CR:
								break;
							case F_VIA:
								vb->params.len=param->start+param->size
												-vb->params.s;
								break;
							case END_OF_HEADER:
								vb->params.len=param->start+param->size
												-vb->params.s;
								break;
							case PARAM_ERROR:
								pkg_free(param);
								goto error;
							default:
								pkg_free(param);
								LOG(L_ERR, "ERROR: parse_via after"
										" parse_via_param: invalid"
										" char <%c> on state %d\n",
										*tmp, state);
								goto error;
						}
						/*add param to the list*/
						if (vb->last_param)	vb->last_param->next=param;
						else				vb->param_lst=param;
						vb->last_param=param;
						/* update param. shortcuts */
						switch(param->type){
							case PARAM_BRANCH:
								vb->branch=param;
								break;
							case PARAM_RECEIVED:
								vb->received=param;
								break;
							case PARAM_RPORT:
								vb->rport=param;
								break;
							case PARAM_I:
								vb->i=param;
								break;
							case PARAM_ALIAS:
								vb->alias=param;
								break;
#ifdef USE_COMP
							case PARAM_COMP:
								vb->comp=param;
								/*  moved comp value parsing in via_param */
								/*
								if  ((param->value.len==SIGCOMP_NAME_LEN) &&
									(strncasecmp(param->value.s, SIGCOMP_NAME,
												SIGCOMP_NAME_LEN)==0)){
									vb->comp_no=COMP_SIGCOMP;
								}else if ((param->value.len==SERGZ_NAME_LEN) &&
										(strncasecmp(param->value.s,
													SERGZ_NAME,
													SERGZ_NAME_LEN)==0)){
									vb->comp_no=COMP_SERGZ;
								}else{
									LOG(L_ERR, "ERROR: parse_via: unrecognized"
										" compression method in comp=%.*s\n",
										param->value.len, param->value.s);
								}
								*/
								break;
#endif
						}
						
						if (state==END_OF_HEADER){
							state=saved_state;
							goto endofheader;
						}
						break;
					case P_PARAM:
						break;
					case F_VIA:
						/*vb->next=tmp;*/ /*???*/
						goto nextvia;
					case L_PORT:
					case L_PARAM:
					case L_VIA:
						LOG(L_ERR,"ERROR:parse_via"
							" on <%c> state %d (default)\n",
							*tmp, state);
						goto  error;
					case F_COMMENT:
						state=P_COMMENT;
						vb->comment.s=tmp;
						break;
					case P_COMMENT:
						break;
					case F_IP6HOST:
						state=P_IP6HOST;
						break;
					case P_IP6HOST:
						break;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					default:
						LOG(L_ERR, "BUG:parse_via:"
							" invalid char <%c>"
							" in state %d\n",
							*tmp, state);
						goto error;
				}


		}
	}

	DBG("end of packet reached, state=%d\n", state);
	goto endofpacket; /*end of packet, probably should be goto error*/
	
endofheader:
	state=saved_state;
	DBG("end of header reached, state=%d\n", state);
endofpacket:
	/* check if error*/
	switch(state){
		case P_HOST:
		case L_PORT:
		case P_PORT:
		case L_PARAM:
		case P_PARAM:
		case P_VALUE:
		case GEN_PARAM:
		case FIN_HIDDEN:
		case L_VIA:
			break;
		default:
			LOG(L_ERR, "ERROR: parse_via: invalid via - end of header in"
					" state %d\n", state);
			goto error;
	}


	/*
	if (proto) printf("<SIP/2.0/%s>\n", proto);
	if (host) printf("host= <%s>\n", host);
	if (port_str) printf("port= <%s>\n", port_str);
	if (param) printf("params= <%s>\n", param);
	if (comment) printf("comment= <%s>\n", comment);
	if(next_via) printf("next_via= <%s>\n", next_via);
	*/
	/*DBG("parse_via: rest=<%s>\n", tmp);*/

	vb->error=PARSE_OK;
	vb->bsize=tmp-buffer;
	if (vb->port_str.s){
		vb->port=str2s(vb->port_str.s, vb->port_str.len, &err);
		if (err){
					LOG(L_ERR, "ERROR: parse_via: invalid port number <%.*s>\n",
						vb->port_str.len, ZSW(vb->port_str.s));
					goto error;
		}
	}
	return tmp;
nextvia:
	DBG("parse_via: next_via\n");
	vb->error=PARSE_OK;
	vb->bsize=tmp-buffer;
	if (vb->port_str.s){
		vb->port=str2s(vb->port_str.s, vb->port_str.len, &err);
		if (err){
					LOG(L_ERR, "ERROR: parse_via: invalid port number <%.*s>\n",
						vb->port_str.len, ZSW(vb->port_str.s));
					goto error;
		}
	}
	vb->next=pkg_malloc(sizeof(struct via_body));
	if (vb->next==0){
		LOG(L_ERR, "ERROR: parse_via: out of memory\n");
		goto error;
	}
	vb=vb->next;
	memset(vb, 0, sizeof(struct via_body));
	buffer=tmp;
	goto parse_again;

error:
	if (end>buffer){
		LOG(L_ERR, "ERROR: parse_via on: <%.*s>\n", (int)(end-buffer), ZSW(buffer));
	}
	if ((tmp>buffer)&&(tmp<end)){
		LOG(L_ERR, "ERROR: parse_via parse error, parsed so far:<%.*s>\n",
				(int)(tmp-buffer), ZSW(buffer) );
	}else{
		LOG(L_ERR, "ERROR: parse_via: via parse error\n");
	}
	vb->error=PARSE_ERROR;
	vbody->error=PARSE_ERROR; /* make sure the first via body is marked
								 as bad also */
	return tmp;
}


static inline void free_via_param_list(struct via_param* vp)
{
	struct via_param* foo;
	while(vp){
		foo=vp;
		vp=vp->next;
		pkg_free(foo);
	}
}


void free_via_list(struct via_body* vb)
{
	struct via_body* foo;
	while(vb){
		foo=vb;
		vb=vb->next;
		if (foo->param_lst) free_via_param_list(foo->param_lst);
		pkg_free(foo);
	}
}

int parse_via_header( struct sip_msg *msg, int n, struct via_body** q)
{
	struct hdr_field *p;
	struct via_body *pp = NULL;
	int i;
	
	switch (n) {
	case 1:
	case 2:
		if (!msg->h_via1 && (parse_headers(msg,HDR_VIA_F,0)==-1 || !msg->h_via1)) {
                        DBG("bad msg or missing VIA1 header \n");
                        return -1;
                }
		pp = msg->h_via1->parsed;
		if (n==1) break;
		pp = pp->next;
		if (pp) break;
		
                if (!msg->h_via2 && (parse_headers(msg,HDR_VIA2_F,0)==-1 || !msg->h_via2)) {
                        DBG("bad msg or missing VIA2 header \n");
                        return -1;
                }
                pp = msg->h_via2->parsed;
                break;
	default:	
	        if (!msg->eoh && (parse_headers(msg,HDR_EOH_F,0)==-1 || !msg->eoh)) {
        	        ERR("bad msg while parsing to EOH \n");
	                return -1;
		}
		p = msg->h_via1;
		i = n;
		while (i && p) {
		        if (p->type == HDR_VIA_T) {
		        	i--;
		        	pp = p->parsed;
		        	while (i && (pp->next)) {
		        		i--;
		        		pp = pp->next;
		        	}
		        }
			p = p->next;
		}
		if (i > 0) {
			DBG("missing VIA[%d] header\n", n);
			return -1;
		}
	}
	if (pp) {
		*q = pp;
		return 0;
	} else
		return -1;
}

