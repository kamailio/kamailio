/*
 * $Id$
 *
 *  cfg grammar
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
 /*
 * History:
 * ---------
 * 2003-01-29  src_port added (jiri)
 * 2003-01-23  mhomed added (jiri)
 * 2003-03-19  replaced all mallocs/frees with pkg_malloc/pkg_free (andrei)
 * 2003-03-19  Added support for route type in find_export (janakj)
 * 2003-03-20  Regex support in modparam (janakj)
 * 2003-04-01  added dst_port, proto , af (andrei)
 * 2003-04-05  s/reply_route/failure_route, onreply_route introduced (jiri)
 * 2003-04-12  added force_rport, chroot and wdir (andrei)
 * 2003-04-15  added tcp_children, disable_tcp (andrei)
 * 2003-04-22  strip_tail added (jiri)
 * 2003-07-03  tls* (disable, certificate, private_key, ca_list, verify, 
 *              require_certificate added (andrei)
 * 2003-07-06  more tls config. vars added: tls_method, tls_port_no (andrei)
 * 2003-10-02  added {,set_}advertised_{address,port} (andrei)
 * 2003-10-10  added <,>,<=,>=, != operators support
 *             added msg:len (andrei)
 * 2003-10-11  if(){} doesn't require a ';' after it anymore (andrei)
 * 2003-10-13  added FIFO_DIR & proto:host:port listen/alias support (andrei)
 */


%{

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include "route_struct.h"
#include "globals.h"
#include "route.h"
#include "dprint.h"
#include "sr_module.h"
#include "modparam.h"
#include "ip_addr.h"
#include "name_alias.h"
#include "ut.h"

#include "config.h"
#ifdef USE_TLS
#include "tls/tls_config.h"
#endif

#ifdef DEBUG_DMALLOC
#include <dmalloc.h>
#endif

/* hack to avoid alloca usage in the generated C file (needed for compiler
 with no built in alloca, like icc*/
#undef _ALLOCA_H

struct id_list{
	char* s;
	int proto;
	int port;
	struct id_list* next;
};

extern int yylex();
void yyerror(char* s);
static char* tmp;
static int i_tmp;
static void* f_tmp;
static struct id_list* lst_tmp;
static int rt;  /* Type of route block for find_export */
static str* str_tmp;

void warn(char* s);
static struct id_list* mk_listen_id(char*, int, int);
 

%}

%union {
	long intval;
	unsigned long uval;
	char* strval;
	struct expr* expr;
	struct action* action;
	struct net* ipnet;
	struct ip_addr* ipaddr;
	struct id_list* idlst;
}

/* terminals */


/* keywords */
%token FORWARD
%token FORWARD_TCP
%token FORWARD_TLS
%token FORWARD_UDP
%token SEND
%token SEND_TCP
%token DROP
%token LOG_TOK
%token ERROR
%token ROUTE
%token ROUTE_FAILURE
%token ROUTE_ONREPLY
%token EXEC
%token SET_HOST
%token SET_HOSTPORT
%token PREFIX
%token STRIP
%token STRIP_TAIL
%token APPEND_BRANCH
%token SET_USER
%token SET_USERPASS
%token SET_PORT
%token SET_URI
%token REVERT_URI
%token FORCE_RPORT
%token IF
%token ELSE
%token SET_ADV_ADDRESS
%token SET_ADV_PORT
%token URIHOST
%token URIPORT
%token MAX_LEN
%token SETFLAG
%token RESETFLAG
%token ISFLAGSET
%token METHOD
%token URI
%token SRCIP
%token SRCPORT
%token DSTIP
%token DSTPORT
%token PROTO
%token AF
%token MYSELF
%token MSGLEN 
%token UDP
%token TCP
%token TLS

/* config vars. */
%token DEBUG
%token FORK
%token LOGSTDERROR
%token LISTEN
%token ALIAS
%token DNS
%token REV_DNS
%token PORT
%token STAT
%token CHILDREN
%token CHECK_VIA
%token SYN_BRANCH
%token MEMLOG
%token SIP_WARNING
%token FIFO
%token FIFO_DIR
%token FIFO_MODE
%token SERVER_SIGNATURE
%token REPLY_TO_VIA
%token LOADMODULE
%token MODPARAM
%token MAXBUFFER
%token USER
%token GROUP
%token CHROOT
%token WDIR
%token MHOMED
%token DISABLE_TCP
%token TCP_CHILDREN
%token DISABLE_TLS
%token TLSLOG
%token TLS_PORT_NO
%token TLS_METHOD
%token SSLv23
%token SSLv2
%token SSLv3
%token TLSv1
%token TLS_VERIFY
%token TLS_REQUIRE_CERTIFICATE
%token TLS_CERTIFICATE
%token TLS_PRIVATE_KEY
%token TLS_CA_LIST
%token ADVERTISED_ADDRESS
%token ADVERTISED_PORT





/* operators */
%nonassoc EQUAL
%nonassoc EQUAL_T
%nonassoc GT
%nonassoc LT
%nonassoc GTE
%nonassoc LTE
%nonassoc DIFF
%nonassoc MATCH
%left OR
%left AND
%left NOT

/* values */
%token <intval> NUMBER
%token <strval> ID
%token <strval> STRING
%token <strval> IPV6ADDR

/* other */
%token COMMA
%token SEMICOLON
%token RPAREN
%token LPAREN
%token LBRACE
%token RBRACE
%token LBRACK
%token RBRACK
%token SLASH
%token DOT
%token CR
%token COLON
%token STAR


/*non-terminals */
%type <expr> exp exp_elem /*, condition*/
%type <action> action actions cmd if_cmd stm
%type <ipaddr> ipv4 ipv6 ipv6addr ip
%type <ipnet> ipnet
%type <strval> host
%type <strval> listen_id
%type <idlst>  id_lst
%type <idlst>  phostport
%type <intval> proto port
%type <intval> equalop strop intop
/*%type <route_el> rules;
  %type <route_el> rule;
*/



%%


cfg:	statements
	;

statements:	statements statement {}
		| statement {}
		| statements error { yyerror(""); YYABORT;}
	;

statement:	assign_stm 
		| module_stm
		| {rt=REQUEST_ROUTE;} route_stm 
		| {rt=FAILURE_ROUTE;} failure_route_stm
		| {rt=ONREPLY_ROUTE;} onreply_route_stm

		| CR	/* null statement*/
	;

listen_id:	ip			{	tmp=ip_addr2a($1);
		 					if(tmp==0){
								LOG(L_CRIT, "ERROR: cfg. parser: bad ip "
										"addresss.\n");
								$$=0;
							}else{
								$$=pkg_malloc(strlen(tmp)+1);
								if ($$==0){
									LOG(L_CRIT, "ERROR: cfg. parser: out of "
											"memory.\n");
								}else{
									strncpy($$, tmp, strlen(tmp)+1);
								}
							}
						}
		 |	STRING			{	$$=pkg_malloc(strlen($1)+1);
		 					if ($$==0){
									LOG(L_CRIT, "ERROR: cfg. parser: out of "
											"memory.\n");
							}else{
									strncpy($$, $1, strlen($1)+1);
							}
						}
		 |	host		{	$$=pkg_malloc(strlen($1)+1);
		 					if ($$==0){
									LOG(L_CRIT, "ERROR: cfg. parser: out of "
											"memory.\n");
							}else{
									strncpy($$, $1, strlen($1)+1);
							}
						}
	;

proto:	  UDP	{ $$=PROTO_UDP; }
		| TCP	{ $$=PROTO_TCP; }
		| TLS	{ $$=PROTO_TLS; }
		;

port:	  NUMBER	{ $$=$1; }
		| STAR		{ $$=0; }
;

phostport:	listen_id				{ $$=mk_listen_id($1, 0, 0); }
			| listen_id COLON port	{ $$=mk_listen_id($1, 0, $3); }
			| proto COLON listen_id	{ $$=mk_listen_id($3, $1, 0); }
			| proto COLON listen_id COLON port	{ $$=mk_listen_id($3, $1, $5);}
			| listen_id COLON error { $$=0; yyerror(" port number expected"); }
			;

id_lst:		phostport		{  $$=$1 ; }
		| phostport id_lst	{ $$=$1; $$->next=$2; }
		;


assign_stm:	DEBUG EQUAL NUMBER { debug=$3; }
		| DEBUG EQUAL error  { yyerror("number  expected"); }
		| FORK  EQUAL NUMBER { dont_fork= ! $3; }
		| FORK  EQUAL error  { yyerror("boolean value expected"); }
		| LOGSTDERROR EQUAL NUMBER { if (!config_check) log_stderr=$3; }
		| LOGSTDERROR EQUAL error { yyerror("boolean value expected"); }
		| DNS EQUAL NUMBER   { received_dns|= ($3)?DO_DNS:0; }
		| DNS EQUAL error { yyerror("boolean value expected"); }
		| REV_DNS EQUAL NUMBER { received_dns|= ($3)?DO_REV_DNS:0; }
		| REV_DNS EQUAL error { yyerror("boolean value expected"); }
		| PORT EQUAL NUMBER   { port_no=$3; 
								if (sock_no>0) 
									sock_info[sock_no-1].port_no=port_no;
							  }
		| STAT EQUAL STRING {
					#ifdef STATS
							stat_file=$3;
					#endif
							}
		| MAXBUFFER EQUAL NUMBER { maxbuffer=$3; }
		| MAXBUFFER EQUAL error { yyerror("number expected"); }
		| PORT EQUAL error    { yyerror("number expected"); } 
		| CHILDREN EQUAL NUMBER { children_no=$3; }
		| CHILDREN EQUAL error { yyerror("number expected"); } 
		| CHECK_VIA EQUAL NUMBER { check_via=$3; }
		| CHECK_VIA EQUAL error { yyerror("boolean value expected"); }
		| SYN_BRANCH EQUAL NUMBER { syn_branch=$3; }
		| SYN_BRANCH EQUAL error { yyerror("boolean value expected"); }
		| MEMLOG EQUAL NUMBER { memlog=$3; }
		| MEMLOG EQUAL error { yyerror("int value expected"); }
		| SIP_WARNING EQUAL NUMBER { sip_warning=$3; }
		| SIP_WARNING EQUAL error { yyerror("boolean value expected"); }
		| FIFO EQUAL STRING { fifo=$3; }
		| FIFO EQUAL error { yyerror("string value expected"); }
		| FIFO_DIR EQUAL STRING { fifo_dir=$3; }
		| FIFO_DIR EQUAL error { yyerror("string value expected"); }
		| FIFO_MODE EQUAL NUMBER { fifo_mode=$3; }
		| FIFO_MODE EQUAL error { yyerror("int value expected"); }
		| USER EQUAL STRING     { user=$3; }
		| USER EQUAL ID         { user=$3; }
		| USER EQUAL error      { yyerror("string value expected"); }
		| GROUP EQUAL STRING     { group=$3; }
		| GROUP EQUAL ID         { group=$3; }
		| GROUP EQUAL error      { yyerror("string value expected"); }
		| CHROOT EQUAL STRING     { chroot_dir=$3; }
		| CHROOT EQUAL ID         { chroot_dir=$3; }
		| CHROOT EQUAL error      { yyerror("string value expected"); }
		| WDIR EQUAL STRING     { working_dir=$3; }
		| WDIR EQUAL ID         { working_dir=$3; }
		| WDIR EQUAL error      { yyerror("string value expected"); }
		| MHOMED EQUAL NUMBER { mhomed=$3; }
		| MHOMED EQUAL error { yyerror("boolean value expected"); }
		| DISABLE_TCP EQUAL NUMBER {
									#ifdef USE_TCP
										tcp_disable=$3;
									#else
										warn("tcp support not compiled in");
									#endif
									}
		| DISABLE_TCP EQUAL error { yyerror("boolean value expected"); }
		| TCP_CHILDREN EQUAL NUMBER {
									#ifdef USE_TCP
										tcp_children_no=$3;
									#else
										warn("tcp support not compiled in");
									#endif
									}
		| TCP_CHILDREN EQUAL error { yyerror("number expected"); }
		| DISABLE_TLS EQUAL NUMBER {
									#ifdef USE_TLS
										tls_disable=$3;
									#else
										warn("tls support not compiled in");
									#endif
									}
		| DISABLE_TLS EQUAL error { yyerror("boolean value expected"); }
		| TLSLOG EQUAL NUMBER 		{ 
									#ifdef USE_TLS
										tls_log=$3;
									#else
										warn("tls support not compiled in");
									#endif
									}
		| TLSLOG EQUAL error { yyerror("int value expected"); }
		| TLS_PORT_NO EQUAL NUMBER {
									#ifdef USE_TLS
										tls_port_no=$3;
									#else
										warn("tls support not compiled in");
									#endif
									}
		| TLS_PORT_NO EQUAL error { yyerror("number expected"); }
		| TLS_METHOD EQUAL SSLv23 {
									#ifdef USE_TLS
										tls_method=TLS_USE_SSLv23;
									#else
										warn("tls support not compiled in");
									#endif
									}
		| TLS_METHOD EQUAL SSLv2 {
									#ifdef USE_TLS
										tls_method=TLS_USE_SSLv2;
									#else
										warn("tls support not compiled in");
									#endif
									}
		| TLS_METHOD EQUAL SSLv3 {
									#ifdef USE_TLS
										tls_method=TLS_USE_SSLv3;
									#else
										warn("tls support not compiled in");
									#endif
									}
		| TLS_METHOD EQUAL TLSv1 {
									#ifdef USE_TLS
										tls_method=TLS_USE_TLSv1;
									#else
										warn("tls support not compiled in");
									#endif
									}
		| TLS_METHOD EQUAL error {
									#ifdef USE_TLS
										yyerror("SSLv23, SSLv2, SSLv3 or TLSv1"
													" expected");
									#else
										warn("tls support not compiled in");
									#endif
									}
										
		| TLS_VERIFY EQUAL NUMBER {
									#ifdef USE_TLS
										tls_verify_cert=$3;
									#else
										warn("tls support not compiled in");
									#endif
									}
		| TLS_VERIFY EQUAL error { yyerror("boolean value expected"); }
		| TLS_REQUIRE_CERTIFICATE EQUAL NUMBER {
									#ifdef USE_TLS
										tls_require_cert=$3;
									#else
										warn( "tls support not compiled in");
									#endif
									}
		| TLS_REQUIRE_CERTIFICATE EQUAL error { yyerror("boolean value"
																" expected"); }
		| TLS_CERTIFICATE EQUAL STRING { 
									#ifdef USE_TLS
											tls_cert_file=$3;
									#else
										warn("tls support not compiled in");
									#endif
									}
		| TLS_CERTIFICATE EQUAL error { yyerror("string value expected"); }
		| TLS_PRIVATE_KEY EQUAL STRING { 
									#ifdef USE_TLS
											tls_pkey_file=$3;
									#else
										warn("tls support not compiled in");
									#endif
									}
		| TLS_PRIVATE_KEY EQUAL error { yyerror("string value expected"); }
		| TLS_CA_LIST EQUAL STRING { 
									#ifdef USE_TLS
											tls_ca_file=$3;
									#else
										warn("tls support not compiled in");
									#endif
									}
		| TLS_CA_LIST EQUAL error { yyerror("string value expected"); }
		| SERVER_SIGNATURE EQUAL NUMBER { server_signature=$3; }
		| SERVER_SIGNATURE EQUAL error { yyerror("boolean value expected"); }
		| REPLY_TO_VIA EQUAL NUMBER { reply_to_via=$3; }
		| REPLY_TO_VIA EQUAL error { yyerror("boolean value expected"); }
		| LISTEN EQUAL id_lst {
							for(lst_tmp=$3; lst_tmp; lst_tmp=lst_tmp->next){
								if (sock_no < MAX_LISTEN){
									sock_info[sock_no].name.s=(char*)
											pkg_malloc(strlen(lst_tmp->s)+1);
									if (sock_info[sock_no].name.s==0){
										LOG(L_CRIT, "ERROR: cfg. parser:"
													" out of memory.\n");
										break;
									}else{
										strncpy(sock_info[sock_no].name.s,
												lst_tmp->s,
												strlen(lst_tmp->s)+1);
										sock_info[sock_no].name.len=
													strlen(lst_tmp->s);
										sock_info[sock_no].port_no=
													lst_tmp->port;
										sock_no++;
									}
								}else{
									LOG(L_CRIT, "ERROR: cfg. parser: "
												"too many listen addresses"
												"(max. %d).\n", MAX_LISTEN);
									break;
								}
							}
							 }
		| LISTEN EQUAL  error { yyerror("ip address or hostname"
						"expected"); }
		| ALIAS EQUAL  id_lst { 
							for(lst_tmp=$3; lst_tmp; lst_tmp=lst_tmp->next)
								add_alias(lst_tmp->s, strlen(lst_tmp->s), 
											lst_tmp->port);
							  }
		| ALIAS  EQUAL error  { yyerror(" hostname expected"); }
		| ADVERTISED_ADDRESS EQUAL listen_id {
								default_global_address.s=$3;
								default_global_address.len=strlen($3);
								}
		|ADVERTISED_ADDRESS EQUAL error {yyerror("ip address or hostname "
												"expected"); }
		| ADVERTISED_PORT EQUAL NUMBER {
								tmp=int2str($3, &i_tmp);
								if ((default_global_port.s=pkg_malloc(i_tmp))
										==0){
										LOG(L_CRIT, "ERROR: cfg. parser:"
													" out of memory.\n");
										default_global_port.len=0;
								}else{
									default_global_port.len=i_tmp;
									memcpy(default_global_port.s, tmp,
											default_global_port.len);
								};
								}
		|ADVERTISED_PORT EQUAL error {yyerror("ip address or hostname "
												"expected"); }
		| error EQUAL { yyerror("unknown config variable"); }
	;

module_stm:	LOADMODULE STRING	{ DBG("loading module %s\n", $2);
		  						  if (load_module($2)!=0){
								  		yyerror("failed to load module");
								  }
								}
		 | LOADMODULE error	{ yyerror("string expected");  }
                 | MODPARAM LPAREN STRING COMMA STRING COMMA STRING RPAREN {
			 if (set_mod_param_regex($3, $5, STR_PARAM, $7) != 0) {
				 yyerror("Can't set module parameter");
			 }
		   }
                 | MODPARAM LPAREN STRING COMMA STRING COMMA NUMBER RPAREN {
			 if (set_mod_param_regex($3, $5, INT_PARAM, (void*)$7) != 0) {
				 yyerror("Can't set module parameter");
			 }
		   }
                 | MODPARAM error { yyerror("Invalid arguments"); }
		 ;


ip:		 ipv4  { $$=$1; }
		|ipv6  { $$=$1; }
		;

ipv4:	NUMBER DOT NUMBER DOT NUMBER DOT NUMBER { 
											$$=pkg_malloc(
													sizeof(struct ip_addr));
											if ($$==0){
												LOG(L_CRIT, "ERROR: cfg. "
													"parser: out of memory.\n"
													);
											}else{
												memset($$, 0, 
													sizeof(struct ip_addr));
												$$->af=AF_INET;
												$$->len=4;
												if (($1>255) || ($1<0) ||
													($3>255) || ($3<0) ||
													($5>255) || ($5<0) ||
													($7>255) || ($7<0)){
													yyerror("invalid ipv4"
															"address");
													$$->u.addr32[0]=0;
													/* $$=0; */
												}else{
													$$->u.addr[0]=$1;
													$$->u.addr[1]=$3;
													$$->u.addr[2]=$5;
													$$->u.addr[3]=$7;
													/*
													$$=htonl( ($1<<24)|
													($3<<16)| ($5<<8)|$7 );
													*/
												}
											}
												}
	;

ipv6addr:	IPV6ADDR {
					$$=pkg_malloc(sizeof(struct ip_addr));
					if ($$==0){
						LOG(L_CRIT, "ERROR: cfg. parser: out of memory.\n");
					}else{
						memset($$, 0, sizeof(struct ip_addr));
						$$->af=AF_INET6;
						$$->len=16;
					#ifdef USE_IPV6
						if (inet_pton(AF_INET6, $1, $$->u.addr)<=0){
							yyerror("bad ipv6 address");
						}
					#else
						yyerror("ipv6 address & no ipv6 support compiled in");
						YYABORT;
					#endif
					}
				}
	;

ipv6:	ipv6addr { $$=$1; }
	| LBRACK ipv6addr RBRACK {$$=$2; }
;


route_stm:  ROUTE LBRACE actions RBRACE { push($3, &rlist[DEFAULT_RT]); }

	    | ROUTE LBRACK NUMBER RBRACK LBRACE actions RBRACE { 
										if (($3<RT_NO) && ($3>=0)){
											push($6, &rlist[$3]);
										}else{
											yyerror("invalid routing"
													"table number");
											YYABORT; }
										}
		| ROUTE error { yyerror("invalid  route  statement"); }
	;

failure_route_stm: ROUTE_FAILURE LBRACK NUMBER RBRACK LBRACE actions RBRACE {
										if (($3<FAILURE_RT_NO)&&($3>=1)){
											push($6, &failure_rlist[$3]);
										} else {
											yyerror("invalid reply routing"
												"table number");
											YYABORT; }
										}
		| ROUTE_FAILURE error { yyerror("invalid failure_route statement"); }
	;

onreply_route_stm: ROUTE_ONREPLY LBRACK NUMBER RBRACK LBRACE actions RBRACE {
										if (($3<ONREPLY_RT_NO)&&($3>=1)){
											push($6, &onreply_rlist[$3]);
										} else {
											yyerror("invalid reply routing"
												"table number");
											YYABORT; }
										}
		| ROUTE_ONREPLY error { yyerror("invalid failure_route statement"); }
	;
/*
rules:	rules rule { push($2, &$1); $$=$1; }
	| rule {$$=$1; }
	| rules error { $$=0; yyerror("invalid rule"); }
	 ;

rule:	condition	actions CR {
								$$=0;
								if (add_rule($1, $2, &$$)<0) {
									yyerror("error calling add_rule");
									YYABORT;
								}
							  }
	| CR		{ $$=0;}
	| condition error { $$=0; yyerror("bad actions in rule"); }
	;

condition:	exp {$$=$1;}
*/

exp:	exp AND exp 	{ $$=mk_exp(AND_OP, $1, $3); }
	| exp OR  exp		{ $$=mk_exp(OR_OP, $1, $3);  }
	| NOT exp 			{ $$=mk_exp(NOT_OP, $2, 0);  }
	| LPAREN exp RPAREN	{ $$=$2; }
	| exp_elem			{ $$=$1; }
	;

equalop:	  EQUAL_T {$$=EQUAL_OP; }
			| DIFF	{$$=DIFF_OP; }
		;
		
intop:	equalop	{$$=$1; }
		|  GT	{$$=GT_OP; }
		| LT	{$$=LT_OP; }
		| GTE	{$$=GTE_OP; }
		| LTE	{$$=LTE_OP; }
		;
		
strop:	equalop	{$$=$1; }
		| MATCH	{$$=MATCH_OP; }
		;

exp_elem:	METHOD strop STRING	{$$= mk_elem(	$2, STRING_ST, 
													METHOD_O, $3);
									}
		| METHOD strop  ID	{$$ = mk_elem(	$2, STRING_ST,
											METHOD_O, $3); 
				 			}
		| METHOD strop error { $$=0; yyerror("string expected"); }
		| METHOD error	{ $$=0; yyerror("invalid operator,"
										"== , !=, or =~ expected");
						}
		| URI strop STRING 	{$$ = mk_elem(	$2, STRING_ST,
												URI_O, $3); 
				 				}
		| URI strop host 	{$$ = mk_elem(	$2, STRING_ST,
											URI_O, $3); 
				 			}
		| URI equalop MYSELF    { $$=mk_elem(	$2, MYSELF_ST,
												URI_O, 0);
								}
		| URI strop error { $$=0; yyerror("string or MYSELF expected"); }
		| URI error	{ $$=0; yyerror("invalid operator,"
									" == , != or =~ expected");
					}
		| SRCPORT intop NUMBER	{ $$=mk_elem(	$2, NUMBER_ST,
												SRCPORT_O, (void *) $3 ); }
		| SRCPORT intop error { $$=0; yyerror("number expected"); }
		| SRCPORT error { $$=0; yyerror("==, !=, <,>, >= or <=  expected"); }
		| DSTPORT intop NUMBER	{ $$=mk_elem(	$2, NUMBER_ST,
												DSTPORT_O, (void *) $3 ); }
		| DSTPORT intop error { $$=0; yyerror("number expected"); }
		| DSTPORT error { $$=0; yyerror("==, !=, <,>, >= or <=  expected"); }
		| PROTO intop proto	{ $$=mk_elem(	$2, NUMBER_ST,
												PROTO_O, (void *) $3 ); }
		| PROTO intop error { $$=0;
								yyerror("protocol expected (udp, tcp or tls)");
							}
		| PROTO error { $$=0; yyerror("equal/!= operator expected"); }
		| AF intop NUMBER	{ $$=mk_elem(	$2, NUMBER_ST,
												AF_O, (void *) $3 ); }
		| AF intop error { $$=0; yyerror("number expected"); }
		| AF error { $$=0; yyerror("equal/!= operator expected"); }
		| MSGLEN intop NUMBER	{ $$=mk_elem(	$2, NUMBER_ST,
												MSGLEN_O, (void *) $3 ); }
		| MSGLEN intop MAX_LEN	{ $$=mk_elem(	$2, NUMBER_ST,
												MSGLEN_O, (void *) BUF_SIZE); }
		| MSGLEN intop error { $$=0; yyerror("number expected"); }
		| MSGLEN error { $$=0; yyerror("equal/!= operator expected"); }
		| SRCIP equalop ipnet	{ $$=mk_elem(	$2, NET_ST,
												SRCIP_O, $3);
								}
		| SRCIP strop STRING	{ $$=mk_elem(	$2, STRING_ST,
												SRCIP_O, $3);
								}
		| SRCIP strop host	{ $$=mk_elem(	$2, STRING_ST,
												SRCIP_O, $3);
								}
		| SRCIP equalop MYSELF  { $$=mk_elem(	$2, MYSELF_ST,
												SRCIP_O, 0);
								}
		| SRCIP strop error { $$=0; yyerror( "ip address or hostname"
						 "expected" ); }
		| SRCIP error  { $$=0; 
						 yyerror("invalid operator, ==, != or =~ expected");}
		| DSTIP equalop ipnet	{ $$=mk_elem(	$2, NET_ST,
												DSTIP_O, $3);
								}
		| DSTIP strop STRING	{ $$=mk_elem(	$2, STRING_ST,
												DSTIP_O, $3);
								}
		| DSTIP strop host	{ $$=mk_elem(	$2, STRING_ST,
												DSTIP_O, $3);
								}
		| DSTIP equalop MYSELF  { $$=mk_elem(	$2, MYSELF_ST,
												DSTIP_O, 0);
								}
		| DSTIP strop error { $$=0; yyerror( "ip address or hostname"
						 			"expected" ); }
		| DSTIP error { $$=0; 
						yyerror("invalid operator, ==, != or =~ expected");}
		| MYSELF equalop URI    { $$=mk_elem(	$2, MYSELF_ST,
												URI_O, 0);
								}
		| MYSELF equalop SRCIP  { $$=mk_elem(	$2, MYSELF_ST,
												SRCIP_O, 0);
								}
		| MYSELF equalop DSTIP  { $$=mk_elem(	$2, MYSELF_ST,
												DSTIP_O, 0);
								}
		| MYSELF equalop error {	$$=0; 
									yyerror(" URI, SRCIP or DSTIP expected"); }
		| MYSELF error	{ $$=0; 
							yyerror ("invalid operator, == or != expected");
						}
		| stm				{ $$=mk_elem( NO_OP, ACTIONS_ST, ACTION_O, $1 ); }
		| NUMBER		{$$=mk_elem( NO_OP, NUMBER_ST, NUMBER_O, (void*)$1 ); }
	;

ipnet:	ip SLASH ip	{ $$=mk_net($1, $3); } 
	| ip SLASH NUMBER 	{	if (($3<0) || ($3>$1->len*8)){
								yyerror("invalid bit number in netmask");
								$$=0;
							}else{
								$$=mk_net_bitlen($1, $3);
							/*
								$$=mk_net($1, 
										htonl( ($3)?~( (1<<(32-$3))-1 ):0 ) );
							*/
							}
						}
	| ip				{ $$=mk_net_bitlen($1, $1->len*8); }
	| ip SLASH error	{ $$=0;
						 yyerror("netmask (eg:255.0.0.0 or 8) expected");
						}
	;

host:	ID				{ $$=$1; }
	| host DOT ID		{ $$=(char*)pkg_malloc(strlen($1)+1+strlen($3)+1);
						  if ($$==0){
						  	LOG(L_CRIT, "ERROR: cfg. parser: memory allocation"
										" failure while parsing host\n");
						  }else{
						  	memcpy($$, $1, strlen($1));
						  	$$[strlen($1)]='.';
						  	memcpy($$+strlen($1)+1, $3, strlen($3));
						  	$$[strlen($1)+1+strlen($3)]=0;
						  }
						  pkg_free($1); pkg_free($3);
						}
	| host DOT error { $$=0; pkg_free($1); yyerror("invalid hostname"); }
	;


stm:		cmd						{ $$=$1; }
		|	if_cmd					{ $$=$1; }
		|	LBRACE actions RBRACE	{ $$=$2; }
	;

actions:	actions action	{$$=append_action($1, $2); }
		| action			{$$=$1;}
		| actions error { $$=0; yyerror("bad command"); }
	;

action:		cmd SEMICOLON {$$=$1;}
		| if_cmd {$$=$1;}
		| SEMICOLON /* null action */ {$$=0;}
		| cmd error { $$=0; yyerror("bad command: missing ';'?"); }
	;

if_cmd:		IF exp stm				{ $$=mk_action3( IF_T,
													 EXPR_ST,
													 ACTIONS_ST,
													 NOSUBTYPE,
													 $2,
													 $3,
													 0);
									}
		|	IF exp stm ELSE stm		{ $$=mk_action3( IF_T,
													 EXPR_ST,
													 ACTIONS_ST,
													 ACTIONS_ST,
													 $2,
													 $3,
													 $5);
									}
	;

cmd:		FORWARD LPAREN host RPAREN	{ $$=mk_action(	FORWARD_T,
														STRING_ST,
														NUMBER_ST,
														$3,
														0);
										}
		| FORWARD LPAREN STRING RPAREN	{ $$=mk_action(	FORWARD_T,
														STRING_ST,
														NUMBER_ST,
														$3,
														0);
										}
		| FORWARD LPAREN ip RPAREN	{ $$=mk_action(	FORWARD_T,
														IP_ST,
														NUMBER_ST,
														(void*)$3,
														0);
										}
		| FORWARD LPAREN host COMMA NUMBER RPAREN { $$=mk_action(FORWARD_T,
																 STRING_ST,
																 NUMBER_ST,
																$3,
																(void*)$5);
												 }
		| FORWARD LPAREN STRING COMMA NUMBER RPAREN {$$=mk_action(FORWARD_T,
																 STRING_ST,
																 NUMBER_ST,
																$3,
																(void*)$5);
													}
		| FORWARD LPAREN ip COMMA NUMBER RPAREN { $$=mk_action(FORWARD_T,
																 IP_ST,
																 NUMBER_ST,
																 (void*)$3,
																(void*)$5);
												  }
		| FORWARD LPAREN URIHOST COMMA URIPORT RPAREN {
													$$=mk_action(FORWARD_T,
																 URIHOST_ST,
																 URIPORT_ST,
																0,
																0);
													}
													
									
		| FORWARD LPAREN URIHOST COMMA NUMBER RPAREN {
													$$=mk_action(FORWARD_T,
																 URIHOST_ST,
																 NUMBER_ST,
																0,
																(void*)$5);
													}
		| FORWARD LPAREN URIHOST RPAREN {
													$$=mk_action(FORWARD_T,
																 URIHOST_ST,
																 NUMBER_ST,
																0,
																0);
										}
		| FORWARD error { $$=0; yyerror("missing '(' or ')' ?"); }
		| FORWARD LPAREN error RPAREN { $$=0; yyerror("bad forward"
										"argument"); }
		| FORWARD_UDP LPAREN host RPAREN	{ $$=mk_action(	FORWARD_UDP_T,
														STRING_ST,
														NUMBER_ST,
														$3,
														0);
										}
		| FORWARD_UDP LPAREN STRING RPAREN	{ $$=mk_action(	FORWARD_UDP_T,
														STRING_ST,
														NUMBER_ST,
														$3,
														0);
										}
		| FORWARD_UDP LPAREN ip RPAREN	{ $$=mk_action(	FORWARD_UDP_T,
														IP_ST,
														NUMBER_ST,
														(void*)$3,
														0);
										}
		| FORWARD_UDP LPAREN host COMMA NUMBER RPAREN { $$=mk_action(
																FORWARD_UDP_T,
																 STRING_ST,
																 NUMBER_ST,
																$3,
																(void*)$5);
												 }
		| FORWARD_UDP LPAREN STRING COMMA NUMBER RPAREN {$$=mk_action(
																FORWARD_UDP_T,
																 STRING_ST,
																 NUMBER_ST,
																$3,
																(void*)$5);
													}
		| FORWARD_UDP LPAREN ip COMMA NUMBER RPAREN { $$=mk_action(
																FORWARD_UDP_T,
																 IP_ST,
																 NUMBER_ST,
																 (void*)$3,
																(void*)$5);
												  }
		| FORWARD_UDP LPAREN URIHOST COMMA URIPORT RPAREN {
													$$=mk_action(FORWARD_UDP_T,
																 URIHOST_ST,
																 URIPORT_ST,
																0,
																0);
													}
													
									
		| FORWARD_UDP LPAREN URIHOST COMMA NUMBER RPAREN {
													$$=mk_action(FORWARD_UDP_T,
																 URIHOST_ST,
																 NUMBER_ST,
																0,
																(void*)$5);
													}
		| FORWARD_UDP LPAREN URIHOST RPAREN {
													$$=mk_action(FORWARD_UDP_T,
																 URIHOST_ST,
																 NUMBER_ST,
																0,
																0);
										}
		| FORWARD_UDP error { $$=0; yyerror("missing '(' or ')' ?"); }
		| FORWARD_UDP LPAREN error RPAREN { $$=0; yyerror("bad forward_udp"
										"argument"); }
		| FORWARD_TCP LPAREN host RPAREN	{ $$=mk_action(	FORWARD_TCP_T,
														STRING_ST,
														NUMBER_ST,
														$3,
														0);
										}
		| FORWARD_TCP LPAREN STRING RPAREN	{ $$=mk_action(	FORWARD_TCP_T,
														STRING_ST,
														NUMBER_ST,
														$3,
														0);
										}
		| FORWARD_TCP LPAREN ip RPAREN	{ $$=mk_action(	FORWARD_TCP_T,
														IP_ST,
														NUMBER_ST,
														(void*)$3,
														0);
										}
		| FORWARD_TCP LPAREN host COMMA NUMBER RPAREN { $$=mk_action(
																FORWARD_TCP_T,
																 STRING_ST,
																 NUMBER_ST,
																$3,
																(void*)$5);
												 }
		| FORWARD_TCP LPAREN STRING COMMA NUMBER RPAREN {$$=mk_action(
																FORWARD_TCP_T,
																 STRING_ST,
																 NUMBER_ST,
																$3,
																(void*)$5);
													}
		| FORWARD_TCP LPAREN ip COMMA NUMBER RPAREN { $$=mk_action(FORWARD_TCP_T,
																 IP_ST,
																 NUMBER_ST,
																 (void*)$3,
																(void*)$5);
												  }
		| FORWARD_TCP LPAREN URIHOST COMMA URIPORT RPAREN {
													$$=mk_action(FORWARD_TCP_T,
																 URIHOST_ST,
																 URIPORT_ST,
																0,
																0);
													}
													
									
		| FORWARD_TCP LPAREN URIHOST COMMA NUMBER RPAREN {
													$$=mk_action(FORWARD_TCP_T,
																 URIHOST_ST,
																 NUMBER_ST,
																0,
																(void*)$5);
													}
		| FORWARD_TCP LPAREN URIHOST RPAREN {
													$$=mk_action(FORWARD_TCP_T,
																 URIHOST_ST,
																 NUMBER_ST,
																0,
																0);
										}
		| FORWARD_TCP error { $$=0; yyerror("missing '(' or ')' ?"); }
		| FORWARD_TCP LPAREN error RPAREN { $$=0; yyerror("bad forward_tcp"
										"argument"); }
		| FORWARD_TLS LPAREN host RPAREN	{
										#ifdef USE_TLS
											$$=mk_action(	FORWARD_TLS_T,
														STRING_ST,
														NUMBER_ST,
														$3,
														0);
										#else
											yyerror("tls support not "
													"compiled in");
										#endif
										}
		| FORWARD_TLS LPAREN STRING RPAREN	{
										#ifdef USE_TLS
											$$=mk_action(	FORWARD_TLS_T,
															STRING_ST,
															NUMBER_ST,
															$3,
															0);
										#else
											yyerror("tls support not "
													"compiled in");
										#endif
										}
		| FORWARD_TLS LPAREN ip RPAREN	{ 
										#ifdef USE_TLS
											$$=mk_action(	FORWARD_TLS_T,
															IP_ST,
															NUMBER_ST,
															(void*)$3,
															0);
										#else
											yyerror("tls support not "
													"compiled in");
										#endif
										}
		| FORWARD_TLS LPAREN host COMMA NUMBER RPAREN { 
										#ifdef USE_TLS
											$$=mk_action(	FORWARD_TLS_T,
															 STRING_ST,
															 NUMBER_ST,
															$3,
															(void*)$5);
										#else
											yyerror("tls support not "
													"compiled in");
										#endif
												 }
		| FORWARD_TLS LPAREN STRING COMMA NUMBER RPAREN {
										#ifdef USE_TLS
											$$=mk_action(	FORWARD_TLS_T,
															 STRING_ST,
															 NUMBER_ST,
															$3,
															(void*)$5);
										#else
											yyerror("tls support not "
													"compiled in");
										#endif
													}
		| FORWARD_TLS LPAREN ip COMMA NUMBER RPAREN {
										#ifdef USE_TLS
											$$=mk_action(	FORWARD_TLS_T,
															 IP_ST,
															 NUMBER_ST,
															 (void*)$3,
															(void*)$5);
										#else
											yyerror("tls support not "
													"compiled in");
										#endif
												  }
		| FORWARD_TLS LPAREN URIHOST COMMA URIPORT RPAREN {
										#ifdef USE_TLS
											$$=mk_action(	FORWARD_TLS_T,
															 URIHOST_ST,
															 URIPORT_ST,
															0,
															0);
										#else
											yyerror("tls support not "
													"compiled in");
										#endif
													}
													
									
		| FORWARD_TLS LPAREN URIHOST COMMA NUMBER RPAREN {
										#ifdef USE_TLS
											$$=mk_action(	FORWARD_TLS_T,
															 URIHOST_ST,
															 NUMBER_ST,
															0,
															(void*)$5);
										#else
											yyerror("tls support not "
													"compiled in");
										#endif
													}
		| FORWARD_TLS LPAREN URIHOST RPAREN {
										#ifdef USE_TLS
											$$=mk_action(	FORWARD_TLS_T,
															 URIHOST_ST,
															 NUMBER_ST,
															0,
															0);
										#else
											yyerror("tls support not "
													"compiled in");
										#endif
										}
		| FORWARD_TLS error { $$=0; yyerror("missing '(' or ')' ?"); }
		| FORWARD_TLS LPAREN error RPAREN { $$=0; yyerror("bad forward_tls"
										"argument"); }
		
		| SEND LPAREN host RPAREN	{ $$=mk_action(	SEND_T,
													STRING_ST,
													NUMBER_ST,
													$3,
													0);
									}
		| SEND LPAREN STRING RPAREN { $$=mk_action(	SEND_T,
													STRING_ST,
													NUMBER_ST,
													$3,
													0);
									}
		| SEND LPAREN ip RPAREN		{ $$=mk_action(	SEND_T,
													IP_ST,
													NUMBER_ST,
													(void*)$3,
													0);
									}
		| SEND LPAREN host COMMA NUMBER RPAREN	{ $$=mk_action(	SEND_T,
																STRING_ST,
																NUMBER_ST,
																$3,
																(void*)$5);
												}
		| SEND LPAREN STRING COMMA NUMBER RPAREN {$$=mk_action(	SEND_T,
																STRING_ST,
																NUMBER_ST,
																$3,
																(void*)$5);
												}
		| SEND LPAREN ip COMMA NUMBER RPAREN { $$=mk_action(	SEND_T,
																IP_ST,
																NUMBER_ST,
																(void*)$3,
																(void*)$5);
											   }
		| SEND error { $$=0; yyerror("missing '(' or ')' ?"); }
		| SEND LPAREN error RPAREN { $$=0; yyerror("bad send"
													"argument"); }
		| SEND_TCP LPAREN host RPAREN	{ $$=mk_action(	SEND_TCP_T,
													STRING_ST,
													NUMBER_ST,
													$3,
													0);
									}
		| SEND_TCP LPAREN STRING RPAREN { $$=mk_action(	SEND_TCP_T,
													STRING_ST,
													NUMBER_ST,
													$3,
													0);
									}
		| SEND_TCP LPAREN ip RPAREN		{ $$=mk_action(	SEND_TCP_T,
													IP_ST,
													NUMBER_ST,
													(void*)$3,
													0);
									}
		| SEND_TCP LPAREN host COMMA NUMBER RPAREN	{ $$=mk_action(	SEND_TCP_T,
																STRING_ST,
																NUMBER_ST,
																$3,
																(void*)$5);
												}
		| SEND_TCP LPAREN STRING COMMA NUMBER RPAREN {$$=mk_action(	SEND_TCP_T,
																STRING_ST,
																NUMBER_ST,
																$3,
																(void*)$5);
												}
		| SEND_TCP LPAREN ip COMMA NUMBER RPAREN { $$=mk_action(	SEND_TCP_T,
																IP_ST,
																NUMBER_ST,
																(void*)$3,
																(void*)$5);
											   }
		| SEND_TCP error { $$=0; yyerror("missing '(' or ')' ?"); }
		| SEND_TCP LPAREN error RPAREN { $$=0; yyerror("bad send_tcp"
													"argument"); }
		| DROP LPAREN RPAREN	{$$=mk_action(DROP_T,0, 0, 0, 0); }
		| DROP					{$$=mk_action(DROP_T,0, 0, 0, 0); }
		| LOG_TOK LPAREN STRING RPAREN	{$$=mk_action(	LOG_T, NUMBER_ST, 
													STRING_ST,(void*)4,$3);
									}
		| LOG_TOK LPAREN NUMBER COMMA STRING RPAREN	{$$=mk_action(	LOG_T,
																NUMBER_ST, 
																STRING_ST,
																(void*)$3,
																$5);
												}
		| LOG_TOK error { $$=0; yyerror("missing '(' or ')' ?"); }
		| LOG_TOK LPAREN error RPAREN { $$=0; yyerror("bad log"
									"argument"); }
		| SETFLAG LPAREN NUMBER RPAREN {$$=mk_action( SETFLAG_T, NUMBER_ST, 0,
													(void *)$3, 0 ); }
		| SETFLAG error { $$=0; yyerror("missing '(' or ')'?"); }
		| RESETFLAG LPAREN NUMBER RPAREN {$$=mk_action(	RESETFLAG_T, NUMBER_ST, 0,
													(void *)$3, 0 ); }
		| RESETFLAG error { $$=0; yyerror("missing '(' or ')'?"); }
		| ISFLAGSET LPAREN NUMBER RPAREN {$$=mk_action(	ISFLAGSET_T, NUMBER_ST, 0,
													(void *)$3, 0 ); }
		| ISFLAGSET error { $$=0; yyerror("missing '(' or ')'?"); }
		| ERROR LPAREN STRING COMMA STRING RPAREN {$$=mk_action(ERROR_T,
																STRING_ST, 
																STRING_ST,
																$3,
																$5);
												  }
		| ERROR error { $$=0; yyerror("missing '(' or ')' ?"); }
		| ERROR LPAREN error RPAREN { $$=0; yyerror("bad error"
														"argument"); }
		| ROUTE LPAREN NUMBER RPAREN	{ $$=mk_action(ROUTE_T, NUMBER_ST,
														0, (void*)$3, 0);
										}
		| ROUTE error { $$=0; yyerror("missing '(' or ')' ?"); }
		| ROUTE LPAREN error RPAREN { $$=0; yyerror("bad route"
						"argument"); }
		| EXEC LPAREN STRING RPAREN	{ $$=mk_action(	EXEC_T, STRING_ST, 0,
													$3, 0);
									}
		| SET_HOST LPAREN STRING RPAREN { $$=mk_action(SET_HOST_T, STRING_ST,
														0, $3, 0); }
		| SET_HOST error { $$=0; yyerror("missing '(' or ')' ?"); }
		| SET_HOST LPAREN error RPAREN { $$=0; yyerror("bad argument, "
														"string expected"); }

		| PREFIX LPAREN STRING RPAREN { $$=mk_action(PREFIX_T, STRING_ST,
														0, $3, 0); }
		| PREFIX error { $$=0; yyerror("missing '(' or ')' ?"); }
		| PREFIX LPAREN error RPAREN { $$=0; yyerror("bad argument, "
														"string expected"); }
		| STRIP_TAIL LPAREN NUMBER RPAREN { $$=mk_action(STRIP_TAIL_T, 
									NUMBER_ST, 0, (void *) $3, 0); }
		| STRIP_TAIL error { $$=0; yyerror("missing '(' or ')' ?"); }
		| STRIP_TAIL LPAREN error RPAREN { $$=0; yyerror("bad argument, "
														"number expected"); }

		| STRIP LPAREN NUMBER RPAREN { $$=mk_action(STRIP_T, NUMBER_ST,
														0, (void *) $3, 0); }
		| STRIP error { $$=0; yyerror("missing '(' or ')' ?"); }
		| STRIP LPAREN error RPAREN { $$=0; yyerror("bad argument, "
														"number expected"); }

		| APPEND_BRANCH LPAREN STRING RPAREN { $$=mk_action( APPEND_BRANCH_T,
													STRING_ST, 0, $3, 0) ; }
		| APPEND_BRANCH LPAREN RPAREN { $$=mk_action( APPEND_BRANCH_T,
													STRING_ST, 0, 0, 0 ) ; }
		| APPEND_BRANCH {  $$=mk_action( APPEND_BRANCH_T, STRING_ST, 0, 0, 0 ) ; }

		| SET_HOSTPORT LPAREN STRING RPAREN { $$=mk_action( SET_HOSTPORT_T, 
														STRING_ST, 0, $3, 0); }
		| SET_HOSTPORT error { $$=0; yyerror("missing '(' or ')' ?"); }
		| SET_HOSTPORT LPAREN error RPAREN { $$=0; yyerror("bad argument,"
												" string expected"); }
		| SET_PORT LPAREN STRING RPAREN { $$=mk_action( SET_PORT_T, STRING_ST,
														0, $3, 0); }
		| SET_PORT error { $$=0; yyerror("missing '(' or ')' ?"); }
		| SET_PORT LPAREN error RPAREN { $$=0; yyerror("bad argument, "
														"string expected"); }
		| SET_USER LPAREN STRING RPAREN { $$=mk_action( SET_USER_T, STRING_ST,
														0, $3, 0); }
		| SET_USER error { $$=0; yyerror("missing '(' or ')' ?"); }
		| SET_USER LPAREN error RPAREN { $$=0; yyerror("bad argument, "
														"string expected"); }
		| SET_USERPASS LPAREN STRING RPAREN { $$=mk_action( SET_USERPASS_T, 
														STRING_ST, 0, $3, 0); }
		| SET_USERPASS error { $$=0; yyerror("missing '(' or ')' ?"); }
		| SET_USERPASS LPAREN error RPAREN { $$=0; yyerror("bad argument, "
														"string expected"); }
		| SET_URI LPAREN STRING RPAREN { $$=mk_action( SET_URI_T, STRING_ST, 
														0, $3, 0); }
		| SET_URI error { $$=0; yyerror("missing '(' or ')' ?"); }
		| SET_URI LPAREN error RPAREN { $$=0; yyerror("bad argument, "
										"string expected"); }
		| REVERT_URI LPAREN RPAREN { $$=mk_action( REVERT_URI_T, 0,0,0,0); }
		| REVERT_URI { $$=mk_action( REVERT_URI_T, 0,0,0,0); }
		| FORCE_RPORT LPAREN RPAREN	{$$=mk_action(FORCE_RPORT_T,0, 0, 0, 0); }
		| FORCE_RPORT				{$$=mk_action(FORCE_RPORT_T,0, 0, 0, 0); }
		| SET_ADV_ADDRESS LPAREN listen_id RPAREN {
								$$=0;
								if ((str_tmp=pkg_malloc(sizeof(str)))==0){
										LOG(L_CRIT, "ERROR: cfg. parser:"
													" out of memory.\n");
								}else{
										str_tmp->s=$3;
										str_tmp->len=strlen($3);
										$$=mk_action(SET_ADV_ADDR_T, STR_ST,
										             0, str_tmp, 0);
								}
												  }
		| SET_ADV_ADDRESS LPAREN error RPAREN { $$=0; yyerror("bad argument, "
														"string expected"); }
		| SET_ADV_ADDRESS error {$$=0; yyerror("missing '(' or ')' ?"); }
		| SET_ADV_PORT LPAREN NUMBER RPAREN {
								$$=0;
								tmp=int2str($3, &i_tmp);
								if ((str_tmp=pkg_malloc(sizeof(str)))==0){
										LOG(L_CRIT, "ERROR: cfg. parser:"
													" out of memory.\n");
								}else{
									if ((str_tmp->s=pkg_malloc(i_tmp))==0){
										LOG(L_CRIT, "ERROR: cfg. parser:"
													" out of memory.\n");
									}else{
										memcpy(str_tmp->s, tmp, i_tmp);
										str_tmp->len=i_tmp;
										$$=mk_action(SET_ADV_PORT_T, STR_ST,
													0, str_tmp, 0);
									}
								}
								            }
		| SET_ADV_PORT LPAREN error RPAREN { $$=0; yyerror("bad argument, "
														"string expected"); }
		| SET_ADV_PORT  error {$$=0; yyerror("missing '(' or ')' ?"); }
		| ID LPAREN RPAREN			{ f_tmp=(void*)find_export($1, 0, rt);
									   if (f_tmp==0){
										   if (find_export($1, 0, 0)) {
											   yyerror("Command cannot be used in the block\n");
										   } else {
											   yyerror("unknown command, missing"
												   " loadmodule?\n");
										   }
										$$=0;
									   }else{
										$$=mk_action(	MODULE_T,
														CMDF_ST,
														0,
														f_tmp,
														0
													);
									   }
									}
		| ID LPAREN STRING RPAREN { f_tmp=(void*)find_export($1, 1, rt);
									if (f_tmp==0){
										if (find_export($1, 1, 0)) {
											yyerror("Command cannot be used in the block\n");
										} else {
											yyerror("unknown command, missing"
												" loadmodule?\n");
										}
										$$=0;
									}else{
										$$=mk_action(	MODULE_T,
														CMDF_ST,
														STRING_ST,
														f_tmp,
														$3
													);
									}
								  }
		| ID LPAREN STRING  COMMA STRING RPAREN 
								  { f_tmp=(void*)find_export($1, 2, rt);
									if (f_tmp==0){
										if (find_export($1, 2, 0)) {
											yyerror("Command cannot be used in the block\n");
										} else {
											yyerror("unknown command, missing"
												" loadmodule?\n");
										}
										$$=0;
									}else{
										$$=mk_action3(	MODULE_T,
														CMDF_ST,
														STRING_ST,
														STRING_ST,
														f_tmp,
														$3,
														$5
													);
									}
								  }
		| ID LPAREN error RPAREN { $$=0; yyerror("bad arguments"); }
	;


%%

extern int line;
extern int column;
extern int startcolumn;
void warn(char* s)
{
	LOG(L_WARN, "cfg. warning: (%d,%d-%d): %s\n", line, startcolumn, 
			column, s);
	cfg_errors++;
}

void yyerror(char* s)
{
	LOG(L_CRIT, "parse error (%d,%d-%d): %s\n", line, startcolumn, 
			column, s);
	cfg_errors++;
}


static struct id_list* mk_listen_id(char* host, int proto, int port)
{
	struct id_list* l;
	l=pkg_malloc(sizeof(struct id_list));
	if (l==0){
		LOG(L_CRIT,"ERROR: cfg. parser: out of memory.\n");
	}else{
		l->s=host;
		l->port=port;
		l->proto=proto;
		l->next=0;
	}
	return l;
}


/*
int main(int argc, char ** argv)
{
	if (yyparse()!=0)
		fprintf(stderr, "parsing error\n");
}
*/
