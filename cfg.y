/*
 * $Id$
 *
 *  cfg grammar
 */

%{

#include "route_struct.h"

%}

%union {
	int intval;
	unsigned uval;
	char* strval;
	struct expr* expr;
	struct action* action;
	struct net* net;
}

/* terminals */


/* keywors */
%token FORWARD
%token SEND
%token DROP
%token LOG
%token ERROR
%token ROUTE
%token EXEC

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
%type <expr> exp, condition,  exp_elem
%type <action> action, actions, cmd
%type <uval> ipv4
%type <net> net4
%type <strval> host



%%


cfg:	statements
	;

statements:	statements statement {printf("got <> <>\n");}
		| statement {printf("got a statement<>\n"); }
		| statements error { yyerror(""); }
	;

statement:	assign_stm CR
		| route_stm CR
		| CR	/* null statement*/
	;

assign_stm:	DEBUG EQUAL NUMBER 
		| DEBUG EQUAL error  { yyerror("number  expected"); }
		| FORK  EQUAL NUMBER
		| FORK  EQUAL error  { yyerror("boolean value expected"); }
		| LOGSTDERROR EQUAL NUMBER 
		| LOGSTDERROR EQUAL error { yyerror("boolean value expected"); }
		| DNS EQUAL NUMBER
		| DNS EQUAL error { yyerror("boolean value expected"); }
		| REV_DNS EQUAL NUMBER 
		| REV_DNS EQUAL error { yyerror("boolean value expected"); }
		| LISTEN EQUAL ipv4 
		| LISTEN EQUAL ID 
		| LISTEN EQUAL STRING
		| LISTEN EQUAL  error { yyerror("ip address or hostname"
						"expected"); }
		| error EQUAL { yyerror("unknown config variable"); }
	;


ipv4:	NUMBER DOT NUMBER DOT NUMBER DOT NUMBER { 
											if (($1>255) || ($1<0) ||
												($3>255) || ($3<0) ||
												($5>255) || ($5<0) ||
												($7>255) || ($7<0)){
												yyerror("invalid ipv4"
														"address");
												$$=0;
											}else{
												$$=($1<<24)|($3<<16)|
													($5<<8)|$7;
											}
												}
	;

route_stm:	ROUTE LBRACE rules RBRACE 
		| ROUTE LBRACK NUMBER RBRACK LBRACE rules RBRACE
		| ROUTE error { yyerror("invalid  route  statement"); }
	;

rules:	rules rule
	| rule
	| rules error { yyerror("invalid rule"); }
	 ;

rule:	condition	actions CR {
								printf("Got a new rule!\n");
								printf("expr: "); print_expr($1);
								printf("\n  -> actions: ");
								print_action($2); printf("\n");
							   }
	| CR  /* null rule */
	| condition error { yyerror("bad actions in rule"); }
	;

condition:	exp {$$=$1;}
	;

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
										"== or ~= expected");
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
				  					" == or ~= expected");
					}
		| SRCIP EQUAL_T net4	{ $$=mk_elem(	EQUAL_OP, NET_ST,
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
						 yyerror("invalid operator, == or ~= expected");}
		| DSTIP EQUAL_T net4	{ $$=mk_elem(	EQUAL_OP, NET_ST,
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
						yyerror("invalid operator, == or ~= expected");}
	;

net4:	ipv4 SLASH ipv4	{ $$=mk_net($1, $3); } 
	| ipv4 SLASH NUMBER {	if (($3>32)|($3<0)){
								yyerror("invalid bit number in netmask");
								$$=0;
							}else{
								$$=mk_net($1, (1<<$3)-1);
							}
						}
	| ipv4				{ $$=mk_net($1, 0xffffffff); }
	| ipv4 SLASH error { $$=0;
						 yyerror("netmask (eg:255.0.0.0 or 8) expected");}
	;

host:	ID				{ $$=$1; }
	| host DOT ID		{ $$=(char*)malloc(strlen($1)+1+strlen($3)+1);
						  if ($$==0){
						  	fprintf(stderr, "memory allocation failure"
						 				" while parsing host\n");
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


actions:	actions action	{$$=append_action($1, $2); }
		| action			{$$=$1;}
		| actions error { $$=0; yyerror("bad command"); }
	;

action:		cmd SEMICOLON {$$=$1;}
		| SEMICOLON /* null action */ {$$=0;}
		| cmd error { $$=0; yyerror("bad command: missing ';'?"); }
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
		| FORWARD LPAREN ipv4 RPAREN	{ $$=mk_action(	FORWARD_T,
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
		| FORWARD LPAREN ipv4 COMMA NUMBER RPAREN { $$=mk_action(FORWARD_T,
																 IP_ST,
																 NUMBER_ST,
																 (void*)$3,
																(void*)$5);
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
		| SEND LPAREN ipv4 RPAREN	{ $$=mk_action(	SEND_T,
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
		| SEND LPAREN ipv4 COMMA NUMBER RPAREN { $$=mk_action(	SEND_T,
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
		| LOG LPAREN STRING RPAREN	{$$=mk_action(	LOG_T, NUMBER_ST, 
													STRING_ST,(void*)4,$3);
									}
		| LOG LPAREN NUMBER COMMA STRING RPAREN	{$$=mk_action(	LOG_T,
																NUMBER_ST, 
																STRING_ST,
																(void*)$3,
																$5);
												}
		| LOG error { $$=0; yyerror("missing '(' or ')' ?"); }
		| LOG LPAREN error RPAREN { $$=0; yyerror("bad log"
									"argument"); }
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
	;


%%

extern int line;
extern int column;
extern int startcolumn;
yyerror(char* s)
{
	fprintf(stderr, "parse error (%d,%d-%d): %s\n", line, startcolumn, 
			column, s);
}

/*
int main(int argc, char ** argv)
{
	if (yyparse()!=0)
		fprintf(stderr, "parsing error\n");
}
*/
