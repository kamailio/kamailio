/*
 * $Id$
 *
 *  cfg grammar
 */

%{

%}

%union {
	int intval;
	char* strval;
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
%token NUMBER
%token ID
%token STRING

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



%%


cfg:	statements
	;

statements:	statements statement {printf("got <> <>\n");}
			| statement {printf("got a statement<>\n"); }
	;

statement:	assign_stm CR
			| route_stm CR
			| CR	/* null statement*/
	;

assign_stm:	DEBUG EQUAL NUMBER |
			FORK  EQUAL NUMBER |
			LOGSTDERROR EQUAL NUMBER |
			DNS EQUAL NUMBER |
			REV_DNS EQUAL NUMBER |
			LISTEN EQUAL ipv4 |
			LISTEN EQUAL ID |
			LISTEN EQUAL STRING 
	;


ipv4:	NUMBER DOT NUMBER DOT NUMBER DOT NUMBER
	;

route_stm:	ROUTE LBRACE rules RBRACE |
			ROUTE LBRACK NUMBER RBRACK LBRACE rules RBRACE
	;

rules:	rules rule |
		rule
	 ;

rule:	condition	actions CR
	 	| CR  /* null rule */
	;

condition:	exp_elem |
			LPAREN exp RPAREN
	;

exp:	exp AND condition |
		exp OR  condition |
		NOT condition |
		condition
	;

exp_elem:	METHOD EQUAL_T STRING |
			METHOD EQUAL_T ID |
			METHOD MATCH STRING |
			METHOD MATCH ID |
			URI EQUAL_T STRING |
			URI EQUAL_T ID |
			URI MATCH STRING |
			URI MATCH ID |
			SRCIP EQUAL_T net4 |
			SRCIP EQUAL_T STRING |
			SRCIP EQUAL_T host |
			SRCIP MATCH STRING 
			DSTIP EQUAL_T net4 |
			DSTIP EQUAL_T STRING |
			DSTIP MATCH STRING
	;

net4:	ipv4 SLASH ipv4 |
		ipv4 SLASH NUMBER |
		ipv4
	;

host:	ID |
		host DOT ID
	;


actions:	actions action |
			action
	;

action:		cmd SEMICOLON |
			SEMICOLON /* null action */

cmd:		FORWARD LPAREN host RPAREN |
			FORWARD LPAREN STRING RPAREN |
			FORWARD LPAREN ipv4 RPAREN |
			SEND LPAREN host RPAREN |
			SEND LPAREN STRING RPAREN |
			SEND LPAREN ipv4 RPAREN |
			DROP LPAREN RPAREN |
			DROP |
			LOG LPAREN STRING RPAREN |
			ERROR LPAREN STRING COMMA STRING RPAREN |
			ROUTE LPAREN NUMBER RPAREN
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
