/*
 * $Id$
 *
 *  cfg grammar
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

#include "config.h"

#ifdef DEBUG_DMALLOC
#include <dmalloc.h>
#endif


extern int yylex();
void yyerror(char* s);
char* tmp;
void* f_tmp;

%}

%union {
	int intval;
	unsigned uval;
	char* strval;
	struct expr* expr;
	struct action* action;
	struct net* ipnet;
	struct ip_addr* ipaddr;
}

/* terminals */


/* keywords */
%token FORWARD
%token SEND
%token DROP
%token LOG_TOK
%token ERROR
%token ROUTE
%token EXEC
%token SET_HOST
%token SET_HOSTPORT
%token PREFIX
%token STRIP
%token SET_USER
%token SET_USERPASS
%token SET_PORT
%token SET_URI
%token IF
%token ELSE
%token URIHOST
%token URIPORT
%token MAX_LEN
%token SETFLAG
%token RESETFLAG
%token ISFLAGSET
%token LEN_GT
%token METHOD
%token URI
%token SRCIP
%token DSTIP

/* config vars. */
%token DEBUG
%token FORK
%token LOGSTDERROR
%token LISTEN
%token DNS
%token REV_DNS
%token PORT
%token STAT
%token CHILDREN
%token CHECK_VIA
%token LOOP_CHECKS
%token LOADMODULE
%token MODPARAM
%token MAXBUFFER



/* operators */
%nonassoc EQUAL
%nonassoc EQUAL_T
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


/*non-terminals */
%type <expr> exp, exp_elem /*, condition*/
%type <action> action, actions, cmd, if_cmd, stm
%type <ipaddr> ipv4, ipv6, ip
%type <ipnet> ipnet
%type <strval> host
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
		| route_stm 
		| CR	/* null statement*/
	;

assign_stm:	DEBUG EQUAL NUMBER { debug=$3; }
		| DEBUG EQUAL error  { yyerror("number  expected"); }
		| FORK  EQUAL NUMBER { dont_fork= ! $3; }
		| FORK  EQUAL error  { yyerror("boolean value expected"); }
		| LOGSTDERROR EQUAL NUMBER { log_stderr=$3; }
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
		| LOOP_CHECKS EQUAL NUMBER { loop_checks=$3; }
		| LOOP_CHECKS EQUAL error { yyerror("boolean value expected"); }
		| LISTEN EQUAL ip  {
								if (sock_no< MAX_LISTEN){
									tmp=ip_addr2a($3);
								/*	tmp=inet_ntoa(*(struct in_addr*)&$3);*/
									if (tmp==0){
										LOG(L_CRIT, "ERROR: cfg. parser: "
											" bad ip address: %s\n",
											strerror(errno));
									}else{
										sock_info[sock_no].name.s=
												(char*)malloc(strlen(tmp)+1);
										if (sock_info[sock_no].name.s==0){
											LOG(L_CRIT, "ERROR: cfg. parser: "
														"out of memory.\n");
										}else{
											strncpy(sock_info[sock_no].name.s,
													tmp, strlen(tmp)+1);
											sock_info[sock_no].name.len=
													strlen(tmp);
											sock_info[sock_no].port_no=
													port_no;
											sock_no++;
										}
									}
								}else{
									LOG(L_CRIT, "ERROR: cfg. parser:"
												" too many listen addresses"
												"(max. %d).\n", MAX_LISTEN);
								}
							  }
		| LISTEN EQUAL ID	 {
								if (sock_no < MAX_LISTEN){
									sock_info[sock_no].name.s=
												(char*)malloc(strlen($3)+1);
									if (sock_info[sock_no].name.s==0){
										LOG(L_CRIT, "ERROR: cfg. parser:"
														" out of memory.\n");
									}else{
										strncpy(sock_info[sock_no].name.s, $3,
													strlen($3)+1);
										sock_info[sock_no].name.len=strlen($3);
										sock_info[sock_no].port_no= port_no;
										sock_no++;
									}
								}else{
									LOG(L_CRIT, "ERROR: cfg. parser: "
												"too many listen addresses"
												"(max. %d).\n", MAX_LISTEN);
								}
							  }
		| LISTEN EQUAL STRING {
								if (sock_no < MAX_LISTEN){
									sock_info[sock_no].name.s=
										(char*)malloc(strlen($3)+1);
									if (sock_info[sock_no].name.s==0){
										LOG(L_CRIT, "ERROR: cfg. parser:"
													" out of memory.\n");
									}else{
										strncpy(sock_info[sock_no].name.s, $3,
												strlen($3)+1);
										sock_info[sock_no].name.len=strlen($3);
										sock_info[sock_no].port_no=port_no;
										sock_no++;
									}
								}else{
									LOG(L_CRIT, "ERROR: cfg. parser: "
												"too many listen addresses"
												"(max. %d).\n", MAX_LISTEN);
								}
							  }
		| LISTEN EQUAL  error { yyerror("ip address or hostname"
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
			 if (set_mod_param($3, $5, STR_PARAM, $7) != 0) {
				 yyerror("Can't set module parameter");
			 }
		   }
                 | MODPARAM LPAREN STRING COMMA STRING COMMA NUMBER RPAREN {
			 if (set_mod_param($3, $5, INT_PARAM, (void*)$7) != 0) {
				 yyerror("Can't set module parameter");
			 }
		   }
                 | MODPARAM error { yyerror("Invalid arguments"); }
		 ;


ip:		 ipv4  { $$=$1; }
		|ipv6  { $$=$1; }
		;

ipv4:	NUMBER DOT NUMBER DOT NUMBER DOT NUMBER { 
											$$=malloc(sizeof(struct ip_addr));
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

ipv6:	IPV6ADDR {
					$$=malloc(sizeof(struct ip_addr));
					if ($$==0){
						LOG(L_CRIT, "ERROR: cfg. parser: out of memory.\n");
					}else{
						memset($$, 0, sizeof(struct ip_addr));
						$$->af=AF_INET6;
						$$->len=16;
					#ifndef USE_IPV6
						yyerror("ipv6 address & no ipv6 support compiled in");
						YYABORT;
					#endif
						if (inet_pton(AF_INET6, $1, $$->u.addr)<=0){
							yyerror("bad ipv6 address");
						}
					}
				}
	;


route_stm:	ROUTE LBRACE actions RBRACE { push($3, &rlist[DEFAULT_RT]); }

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

exp_elem:	METHOD EQUAL_T STRING	{$$= mk_elem(	EQUAL_OP, STRING_ST, 
													METHOD_O, $3);
									}
		| METHOD EQUAL_T ID	{$$ = mk_elem(	EQUAL_OP, STRING_ST,
											METHOD_O, $3); 
				 			}
		| METHOD EQUAL_T error { $$=0; yyerror("string expected"); }
		| METHOD MATCH STRING	{$$ = mk_elem(	MATCH_OP, STRING_ST,
												METHOD_O, $3); 
				 				}
		| METHOD MATCH ID	{$$ = mk_elem(	MATCH_OP, STRING_ST,
											METHOD_O, $3); 
				 			}
		| METHOD MATCH error { $$=0; yyerror("string expected"); }
		| METHOD error	{ $$=0; yyerror("invalid operator,"
										"== or =~ expected");
						}
		| URI EQUAL_T STRING 	{$$ = mk_elem(	EQUAL_OP, STRING_ST,
												URI_O, $3); 
				 				}
		| URI EQUAL_T ID 	{$$ = mk_elem(	EQUAL_OP, STRING_ST,
											URI_O, $3); 
				 			}
		| URI EQUAL_T error { $$=0; yyerror("string expected"); }
		| URI MATCH STRING	{ $$=mk_elem(	MATCH_OP, STRING_ST,
											URI_O, $3);
							}
		| URI MATCH ID		{ $$=mk_elem(	MATCH_OP, STRING_ST,
											URI_O, $3);
							}
		| URI MATCH error {  $$=0; yyerror("string expected"); }
		| URI error	{ $$=0; yyerror("invalid operator,"
				  					" == or =~ expected");
					}
		| SRCIP EQUAL_T ipnet	{ $$=mk_elem(	EQUAL_OP, NET_ST,
												SRCIP_O, $3);
								}
		| SRCIP EQUAL_T STRING	{ $$=mk_elem(	EQUAL_OP, STRING_ST,
												SRCIP_O, $3);
								}
		| SRCIP EQUAL_T host	{ $$=mk_elem(	EQUAL_OP, STRING_ST,
												SRCIP_O, $3);
								}
		| SRCIP EQUAL_T error { $$=0; yyerror( "ip address or hostname"
						 "expected" ); }
		| SRCIP MATCH STRING	{ $$=mk_elem(	MATCH_OP, STRING_ST,
												SRCIP_O, $3);
								}
		| SRCIP MATCH ID		{ $$=mk_elem(	MATCH_OP, STRING_ST,
												SRCIP_O, $3);
								}
		| SRCIP MATCH error  { $$=0; yyerror( "hostname expected"); }
		| SRCIP error  { $$=0; 
						 yyerror("invalid operator, == or =~ expected");}
		| DSTIP EQUAL_T ipnet	{ $$=mk_elem(	EQUAL_OP, NET_ST,
												DSTIP_O, $3);
								}
		| DSTIP EQUAL_T STRING	{ $$=mk_elem(	EQUAL_OP, STRING_ST,
												DSTIP_O, $3);
								}
		| DSTIP EQUAL_T host	{ $$=mk_elem(	EQUAL_OP, STRING_ST,
												DSTIP_O, $3);
								}
		| DSTIP EQUAL_T error { $$=0; yyerror( "ip address or hostname"
						 			"expected" ); }
		| DSTIP MATCH STRING	{ $$=mk_elem(	MATCH_OP, STRING_ST,
												DSTIP_O, $3);
								}
		| DSTIP MATCH ID	{ $$=mk_elem(	MATCH_OP, STRING_ST,
											DSTIP_O, $3);
							}
		| DSTIP MATCH error  { $$=0; yyerror ( "hostname  expected" ); }
		| DSTIP error { $$=0; 
						yyerror("invalid operator, == or =~ expected");}
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
	| host DOT ID		{ $$=(char*)malloc(strlen($1)+1+strlen($3)+1);
						  if ($$==0){
						  	LOG(L_CRIT, "ERROR: cfg. parser: memory allocation"
										" failure while parsing host\n");
						  }else{
						  	memcpy($$, $1, strlen($1));
						  	$$[strlen($1)]='.';
						  	memcpy($$+strlen($1)+1, $3, strlen($3));
						  	$$[strlen($1)+1+strlen($3)]=0;
						  }
						  free($1); free($3);
						};
	| host DOT error { $$=0; free($1); yyerror("invalid hostname"); }
	;


stm:		cmd						{ $$=$1; }
		|	LBRACE actions RBRACE	{ $$=$2; }
	;

actions:	actions action	{$$=append_action($1, $2); }
		| action			{$$=$1;}
		| actions error { $$=0; yyerror("bad command"); }
	;

action:		cmd SEMICOLON {$$=$1;}
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

		| LEN_GT LPAREN NUMBER RPAREN {$$=mk_action( LEN_GT_T, NUMBER_ST, 0,
													(void *)$3, 0 ); }
		| LEN_GT LPAREN MAX_LEN RPAREN {$$=mk_action( LEN_GT_T, NUMBER_ST, 0,
													(void *) BUF_SIZE, 0 ); }
		| LEN_GT error { $$=0; yyerror("missing '(' or ')'?"); }

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
		| STRIP LPAREN NUMBER RPAREN { $$=mk_action(STRIP_T, NUMBER_ST,
														0, (void *) $3, 0); }
		| STRIP error { $$=0; yyerror("missing '(' or ')' ?"); }
		| STRIP LPAREN error RPAREN { $$=0; yyerror("bad argument, "
														"number expected"); }

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
		| ID LPAREN RPAREN			{ f_tmp=(void*)find_export($1, 0);
									   if (f_tmp==0){
										yyerror("unknown command, missing"
										" loadmodule?\n");
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
		| ID LPAREN STRING RPAREN { f_tmp=(void*)find_export($1, 1);
									if (f_tmp==0){
										yyerror("unknown command, missing"
										" loadmodule?\n");
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
								  { f_tmp=(void*)find_export($1, 2);
									if (f_tmp==0){
										yyerror("unknown command, missing"
										" loadmodule?\n");
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
		| if_cmd		{ $$=$1; }
	;


%%

extern int line;
extern int column;
extern int startcolumn;
void yyerror(char* s)
{
	LOG(L_CRIT, "parse error (%d,%d-%d): %s\n", line, startcolumn, 
			column, s);
	cfg_errors++;
}

/*
int main(int argc, char ** argv)
{
	if (yyparse()!=0)
		fprintf(stderr, "parsing error\n");
}
*/
