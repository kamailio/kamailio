/*
 * $Id$
 *
 * scanner for cfg files
 */


%{
	#include "cfg.tab.h"
	#include "dprint.h"
	#include "globals.h"
	#include <string.h>
	#include <stdlib.h>

#ifdef DEBUG_DMALLOC
#include <dmalloc.h>
#endif

	/* states */
	#define INITIAL_S		0
	#define COMMENT_S		1
	#define COMMENT_LN_S	2
	#define STRING_S		3

	
	static int comment_nest=0;
	static int state=0;
	static char* str=0;
	int line=1;
	int column=1;
	int startcolumn=1;

	static char* addstr(char*, char**);
	static void count();


%}

/* start conditions */
%x STRING1 STRING2 COMMENT COMMENT_LN

/* action keywords */
FORWARD	forward
DROP	"drop"|"break"
SEND	send
LOG		log
ERROR	error
ROUTE	route
EXEC	exec
SETFLAG		setflag
RESETFLAG	resetflag
ISFLAGSET	isflagset
LEN_GT		len_gt
SET_HOST		"rewritehost"|"sethost"|"seth"
SET_HOSTPORT	"rewritehostport"|"sethostport"|"sethp"
SET_USER		"rewriteuser"|"setuser"|"setu"
SET_USERPASS	"rewriteuserpass"|"setuserpass"|"setup"
SET_PORT		"rewriteport"|"setport"|"setp"
SET_URI			"rewriteuri"|"seturi"
PREFIX			"prefix"
STRIP			"strip"
IF				"if"
ELSE			"else"

/*ACTION LVALUES*/
URIHOST			"uri:host"
URIPORT			"uri:port"

MAX_LEN			"max_len"


/* condition keywords */
METHOD	method
URI		uri
SRCIP	src_ip
DSTIP	dst_ip
/* operators */
EQUAL	=
EQUAL_T	==
MATCH	=~
NOT		!|"not"
AND		"and"|"&&"|"&"
OR		"or"|"||"|"|"

/* config vars. */
DEBUG	debug
FORK	fork
LOGSTDERROR	log_stderror
LISTEN		listen
DNS		 dns
REV_DNS	 rev_dns
PORT	port
STAT	statistics
MAXBUFFER maxbuffer
CHILDREN children
CHECK_VIA	check_via
LOOP_CHECKS	loop_checks

LOADMODULE	loadmodule
MODPARAM        modparam

/* values */
YES			"yes"|"true"|"on"|"enable"
NO			"no"|"false"|"off"|"disable"

LETTER		[a-zA-Z]
DIGIT		[0-9]
ALPHANUM	{LETTER}|{DIGIT}|[_]
NUMBER		{DIGIT}+
ID			{LETTER}{ALPHANUM}*
HEX			[0-9a-fA-F]
HEX4		{HEX}{1,4}
IPV6ADDR	({HEX4}":"){7}{HEX4}|({HEX4}":"){1,7}(":"{HEX4}){1,7}|":"(":"{HEX4}){1,7}|({HEX4}":"){1,7}":"|"::"
QUOTES		\"
TICK		\'
SLASH		"/"
SEMICOLON	;
RPAREN		\)
LPAREN		\(
LBRACE		\{
RBRACE		\}
LBRACK		\[
RBRACK		\]
COMMA		","
DOT			\.
CR			\n



COM_LINE	#
COM_START	"/\*"
COM_END		"\*/"

EAT_ABLE	[\ \t\b\r]

%%


<INITIAL>{EAT_ABLE}	{ count(); }

<INITIAL>{FORWARD}	{count(); yylval.strval=yytext; return FORWARD; }
<INITIAL>{DROP}	{ count(); yylval.strval=yytext; return DROP; }
<INITIAL>{SEND}	{ count(); yylval.strval=yytext; return SEND; }
<INITIAL>{LOG}	{ count(); yylval.strval=yytext; return LOG_TOK; }
<INITIAL>{ERROR}	{ count(); yylval.strval=yytext; return ERROR; }
<INITIAL>{SETFLAG}	{ count(); yylval.strval=yytext; return SETFLAG; }
<INITIAL>{RESETFLAG}	{ count(); yylval.strval=yytext; return RESETFLAG; }
<INITIAL>{ISFLAGSET}	{ count(); yylval.strval=yytext; return ISFLAGSET; }
<INITIAL>{LEN_GT}	{ count(); yylval.strval=yytext; return LEN_GT; }
<INITIAL>{ROUTE}	{ count(); yylval.strval=yytext; return ROUTE; }
<INITIAL>{EXEC}	{ count(); yylval.strval=yytext; return EXEC; }
<INITIAL>{SET_HOST}	{ count(); yylval.strval=yytext; return SET_HOST; }
<INITIAL>{SET_HOSTPORT}	{ count(); yylval.strval=yytext; return SET_HOSTPORT; }
<INITIAL>{SET_USER}	{ count(); yylval.strval=yytext; return SET_USER; }
<INITIAL>{SET_USERPASS}	{ count(); yylval.strval=yytext; return SET_USERPASS; }
<INITIAL>{SET_PORT}	{ count(); yylval.strval=yytext; return SET_PORT; }
<INITIAL>{SET_URI}	{ count(); yylval.strval=yytext; return SET_URI; }
<INITIAL>{PREFIX}	{ count(); yylval.strval=yytext; return PREFIX; }
<INITIAL>{STRIP}	{ count(); yylval.strval=yytext; return STRIP; }
<INITIAL>{IF}	{ count(); yylval.strval=yytext; return IF; }
<INITIAL>{ELSE}	{ count(); yylval.strval=yytext; return ELSE; }

<INITIAL>{URIHOST}	{ count(); yylval.strval=yytext; return URIHOST; }
<INITIAL>{URIPORT}	{ count(); yylval.strval=yytext; return URIPORT; }

<INITIAL>{MAX_LEN}	{ count(); yylval.strval=yytext; return MAX_LEN; }

<INITIAL>{METHOD}	{ count(); yylval.strval=yytext; return METHOD; }
<INITIAL>{URI}	{ count(); yylval.strval=yytext; return URI; }
<INITIAL>{SRCIP}	{ count(); yylval.strval=yytext; return SRCIP; }
<INITIAL>{DSTIP}	{ count(); yylval.strval=yytext; return DSTIP; }

<INITIAL>{DEBUG}	{ count(); yylval.strval=yytext; return DEBUG; }
<INITIAL>{FORK}		{ count(); yylval.strval=yytext; return FORK; }
<INITIAL>{LOGSTDERROR}	{ yylval.strval=yytext; return LOGSTDERROR; }
<INITIAL>{LISTEN}	{ count(); yylval.strval=yytext; return LISTEN; }
<INITIAL>{DNS}	{ count(); yylval.strval=yytext; return DNS; }
<INITIAL>{REV_DNS}	{ count(); yylval.strval=yytext; return REV_DNS; }
<INITIAL>{PORT}	{ count(); yylval.strval=yytext; return PORT; }
<INITIAL>{STAT}	{ count(); yylval.strval=yytext; return STAT; }
<INITIAL>{MAXBUFFER}	{ count(); yylval.strval=yytext; return MAXBUFFER; }
<INITIAL>{CHILDREN}	{ count(); yylval.strval=yytext; return CHILDREN; }
<INITIAL>{CHECK_VIA}	{ count(); yylval.strval=yytext; return CHECK_VIA; }
<INITIAL>{LOOP_CHECKS}	{ count(); yylval.strval=yytext; return LOOP_CHECKS; }
<INITIAL>{LOADMODULE}	{ count(); yylval.strval=yytext; return LOADMODULE; }
<INITIAL>{MODPARAM}     { count(); yylval.strval=yytext; return MODPARAM; }

<INITIAL>{EQUAL}	{ count(); return EQUAL; }
<INITIAL>{EQUAL_T}	{ count(); return EQUAL_T; }
<INITIAL>{MATCH}	{ count(); return MATCH; }
<INITIAL>{NOT}		{ count(); return NOT; }
<INITIAL>{AND}		{ count(); return AND; }
<INITIAL>{OR}		{ count(); return OR;  }



<INITIAL>{IPV6ADDR}		{ count(); yylval.strval=yytext; return IPV6ADDR; }
<INITIAL>{NUMBER}		{ count(); yylval.intval=atoi(yytext);return NUMBER; }
<INITIAL>{YES}			{ count(); yylval.intval=1; return NUMBER; }
<INITIAL>{NO}			{ count(); yylval.intval=0; return NUMBER; }

<INITIAL>{COMMA}		{ count(); return COMMA; }
<INITIAL>{SEMICOLON}	{ count(); return SEMICOLON; }
<INITIAL>{RPAREN}	{ count(); return RPAREN; }
<INITIAL>{LPAREN}	{ count(); return LPAREN; }
<INITIAL>{LBRACE}	{ count(); return LBRACE; }
<INITIAL>{RBRACE}	{ count(); return RBRACE; }
<INITIAL>{LBRACK}	{ count(); return LBRACK; }
<INITIAL>{RBRACK}	{ count(); return RBRACK; }
<INITIAL>{SLASH}	{ count(); return SLASH; }
<INITIAL>{DOT}		{ count(); return DOT; }
<INITIAL>\\{CR}		{count(); } /* eat the escaped CR */
<INITIAL>{CR}		{ count();/* return CR;*/ }


<INITIAL>{QUOTES} { count(); state=STRING_S; BEGIN(STRING1); }
<INITIAL>{TICK} { count(); state=STRING_S; BEGIN(STRING2); }


<STRING1>{QUOTES} { count(); state=INITIAL_S; BEGIN(INITIAL); 
						yytext[yyleng-1]=0; yyleng--;
						addstr(yytext, &str);
						yylval.strval=str; str=0;
						return STRING;
					}
<STRING2>{TICK}  { count(); state=INITIAL_S; BEGIN(INITIAL); 
						yytext[yyleng-1]=0; yyleng--;
						addstr(yytext, &str);
						yylval.strval=str;
						str=0;
						return STRING;
					}
<STRING2>.|{EAT_ABLE}|{CR}	{ yymore(); }

<STRING1>\\n		{ count(); yytext[yyleng-2]='\n';yytext[yyleng-1]=0; 
						yyleng--; addstr(yytext, &str); }
<STRING1>\\r		{ count(); yytext[yyleng-2]='\r';yytext[yyleng-1]=0; 
						yyleng--; addstr(yytext, &str); }
<STRING1>\\a		{ count(); yytext[yyleng-2]='\a';yytext[yyleng-1]=0; 
						yyleng--; addstr(yytext, &str); }
<STRING1>\\t		{ count(); yytext[yyleng-2]='\t';yytext[yyleng-1]=0; 
						yyleng--; addstr(yytext, &str); }
<STRING1>\\\\		{ count(); yytext[yyleng-2]='\\';yytext[yyleng-1]=0; 
						yyleng--; addstr(yytext, &str); } 
<STRING1>.|{EAT_ABLE}|{CR}	{ yymore(); }


<INITIAL,COMMENT>{COM_START}	{ count(); comment_nest++; state=COMMENT_S;
										BEGIN(COMMENT); }
<COMMENT>{COM_END}				{ count(); comment_nest--;
										if (comment_nest==0){
											state=INITIAL_S;
											BEGIN(INITIAL);
										}
								}
<COMMENT>.|{EAT_ABLE}|{CR}				{ count(); };

<INITIAL>{COM_LINE}.*{CR}	{ count(); } 

<INITIAL>{ID}			{ count(); addstr(yytext, &str);
						  yylval.strval=str; str=0; return ID; }


<<EOF>>							{
									switch(state){
										case STRING_S: 
											LOG(L_CRIT, "ERROR: cfg. parser: unexpected EOF in"
														" unclosed string\n");
											if (str) {free(str); str=0;}
											break;
										case COMMENT_S:
											LOG(L_CRIT, "ERROR: cfg. parser: unexpected EOF:"
														" %d comments open\n", comment_nest);
											break;
										case COMMENT_LN_S:
											LOG(L_CRIT, "ERROR: unexpected EOF:"
														"comment line open\n");
											break;
									}
									return 0;
								}
			
%%

static char* addstr(char * src, char ** dest)
{
	char *tmp;
	unsigned len1, len2;
	
	if (*dest==0){
		*dest=strdup(src);
	}else{
		len1=strlen(*dest);
		len2=strlen(src);
		tmp=malloc(len1+len2+1);
		if (tmp==0) goto error;
		memcpy(tmp, *dest, len1);
		memcpy(tmp+len1, src, len2);
		tmp[len1+len2]=0;
		free(*dest);
		*dest=tmp;
	}
	return *dest;
error:
	LOG(L_CRIT, "ERROR:lex:addstr: memory allocation error\n");
	return 0;
}



static void count()
{
	int i;
	
	startcolumn=column;
	for (i=0; i<yyleng;i++){
		if (yytext[i]=='\n'){
			line++;
			column=startcolumn=1;
		}else if (yytext[i]=='\t'){
			column++;
			/*column+=8 -(column%8);*/
		}else{
			column++;
		}
	}
}


