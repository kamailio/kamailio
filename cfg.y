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
	char* strval;
	struct expr* expr;
	struct action* action;
}

/* terminals */


/* keywors */
%token FORWARD
%token SEND
%token DROP
%token LOG
%token ERROR
%token ROUTE
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
%left NOT
%left AND
%left OR

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


ipv4:	NUMBER DOT NUMBER DOT NUMBER DOT NUMBER
	;

route_stm:	ROUTE LBRACE rules RBRACE 
		| ROUTE LBRACK NUMBER RBRACK LBRACE rules RBRACE
		| ROUTE error { yyerror("invalid  route  statement"); }
	;

rules:	rules rule
	| rule
	| rules error { yyerror("invalid rule"); }
	 ;

rule:	condition	actions CR
	| CR  /* null rule */
	| condition error { yyerror("bad actions in rule"); }
	;

condition:	exp
	;

exp:	exp AND exp 
	| exp OR  exp 
	| NOT exp 
	| LPAREN exp RPAREN
	| exp_elem
	;

exp_elem:	METHOD EQUAL_T STRING	{$$= mk_elem(	EQUAL_OP,
							STRING_ST, 
							METHOD_O,
							$3);
					}
		| METHOD EQUAL_T ID	{$$ = mk_elem(	EQUAL_OP,
							STRING_ST,
							METHOD_O,
							$3); 
				 	}
		| METHOD EQUAL_T error { $$=0; yyerror("string expected"); }
		| METHOD MATCH STRING 	{$$ = mk_elem(	MATCH_OP,
							STRING_ST,
							METHOD_O,
							$3); 
				 	}
		| METHOD MATCH ID	{$$ = mk_elem(	MATCH_OP,
							STRING_ST,
							METHOD_O,
							$3); 
				 	}
		| METHOD MATCH error { $$=0; yyerror("string expected"); }
		| METHOD error	{ $$=0; 
				  yyerror("invalid operator,"
				  		"== or ~= expected");
				}
		| URI EQUAL_T STRING 	{$$ = mk_elem(	EQUAL_OP,
							STRING_ST,
							URI_O,
							$3); 
				 	}
		| URI EQUAL_T ID 	{$$ = mk_elem(	EQUAL_OP,
							STRING_ST,
							URI_O,
							$3); 
				 	}
		| URI EQUAL_T error { $$=0; yyerror("string expected"); }
		| URI MATCH STRING 
		| URI MATCH ID
		| URI MATCH error {  $$=0; yyerror("string expected"); }
		| URI error	{ $$=0; 
				  yyerror("invalid operator,"
				  		" == or ~= expected");
				}
		| SRCIP EQUAL_T net4 
		| SRCIP EQUAL_T STRING 
		| SRCIP EQUAL_T host
		| SRCIP EQUAL_T error { yyerror( "ip address or hostname"
						 "expected" ); }
		| SRCIP MATCH STRING
		| SRCIP MATCH ID
		| SRCIP MATCH error  { yyerror( "hostname expected"); }
		| SRCIP error  {yyerror("invalid operator, == or ~= expected");}
		| DSTIP EQUAL_T net4 
		| DSTIP EQUAL_T STRING
		| DSTIP EQUAL_T host
		| DSTIP EQUAL_T error { yyerror( "ip address or hostname"
						 "expected" ); }
		| DSTIP MATCH STRING
		| DSTIP MATCH ID
		| DSTIP MATCH error  { yyerror ( "hostname  expected" ); }
		| DSTIP error {yyerror("invalid operator, == or ~= expected");}
	;

net4:	ipv4 SLASH ipv4 
	| ipv4 SLASH NUMBER 
	| ipv4
	| ipv4 SLASH error {yyerror("netmask (eg:255.0.0.0 or 8) expected");}
	;

host:	ID
	| host DOT ID
	| host DOT error { yyerror("invalid hostname"); }
	;


actions:	actions action 
		| action
		| actions error { yyerror("bad command"); }
	;

action:		cmd SEMICOLON
		| SEMICOLON /* null action */
		| cmd error { yyerror("bad command: missing ';'?"); }
	;

cmd:		FORWARD LPAREN host RPAREN 
		| FORWARD LPAREN STRING RPAREN 
		| FORWARD LPAREN ipv4 RPAREN 
		| FORWARD LPAREN host COMMA NUMBER RPAREN
		| FORWARD LPAREN STRING COMMA NUMBER RPAREN
		| FORWARD LPAREN ipv4 COMMA NUMBER RPAREN
		| FORWARD error { yyerror("missing '(' or ')' ?"); }
		| FORWARD LPAREN error RPAREN { yyerror("bad forward"
						"argument"); }
		| SEND LPAREN host RPAREN 
		| SEND LPAREN STRING RPAREN 
		| SEND LPAREN ipv4 RPAREN
		| SEND LPAREN host COMMA NUMBER RPAREN
		| SEND LPAREN STRING COMMA NUMBER RPAREN
		| SEND LPAREN ipv4 COMMA NUMBER RPAREN
		| SEND error { yyerror("missing '(' or ')' ?"); }
		| SEND LPAREN error RPAREN { yyerror("bad send"
						"argument"); }
		| DROP LPAREN RPAREN 
		| DROP 
		| LOG LPAREN STRING RPAREN
		| LOG LPAREN NUMBER COMMA STRING RPAREN
		| LOG error { yyerror("missing '(' or ')' ?"); }
		| LOG LPAREN error RPAREN { yyerror("bad log"
						"argument"); }
		| ERROR LPAREN STRING COMMA STRING RPAREN 
		| ERROR error { yyerror("missing '(' or ')' ?"); }
		| ERROR LPAREN error RPAREN { yyerror("bad error"
						"argument"); }
		| ROUTE LPAREN NUMBER RPAREN
		| ROUTE error { yyerror("missing '(' or ')' ?"); }
		| ROUTE LPAREN error RPAREN { yyerror("bad route"
						"argument"); }
	;


%%

extern int line;
extern int column;
yyerror(char* s)
{
	fprintf(stderr, "parse error (%d,%d): %s\n", line, column, s);
}

int main(int argc, char ** argv)
{
	if (yyparse()!=0)
		fprintf(stderr, "parsing error\n");
}
