/*
 * $Id$
 *
 * scanner for cfg files
 */


%{
	#include "cfg.tab.h"
	#include <string.h>

	/* states */
	#define INITIAL_S		0
	#define COMMENT_S		1
	#define COMMENT_LN_S	2
	#define STRING_S		3

	
	static int comment_nest=0;
	static int state=0;
	static char* str=0;
	int line=0;
	int column=0;

	static char* addstr(char*, char**);
	static void count();


%}

/* start conditions */
%x STRING1 STRING2 COMMENT COMMENT_LN

/* action keywords */
FORWARD	forward
DROP	drop
SEND	send
LOG		log
ERROR	error
ROUTE	route
/* condition keywords */
METHOD	method
URI		uri
SRCIP	src_ip
DSTIP	dst_ip
/* operators */
EQUAL	=
EQUAL_T	==
MATCH	~=
NOT		!
AND		"and"|"&&"|"&"
OR		"or"|"||"|"|"

/* config vars. */
DEBUG	debug
FORK	fork
LOGSTDERROR	log_stderror
LISTEN		listen
DNS		 dns
REV_DNS	 rev_dns

/* values */
YES			"yes"|"true"|"on"|"enable"
NO			"no"|"false"|"off"|"disable"

LETTER		[a-zA-Z]
DIGIT		[0-9]
ALPHANUM	{LETTER}|{DIGIT}
NUMBER		{DIGIT}+
ID			{LETTER}{ALPHANUM}*
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
COMMA		,
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
<INITIAL>{LOG}	{ count(); yylval.strval=yytext; return LOG; }
<INITIAL>{ERROR}	{ count(); yylval.strval=yytext; return ERROR; }
<INITIAL>{ROUTE}	{ count(); yylval.strval=yytext; return ROUTE; }

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

<INITIAL>{EQUAL}	{ count(); return EQUAL; }
<INITIAL>{EQUAL_T}	{ count(); return EQUAL_T; }
<INITIAL>{MATCH}	{ count(); return MATCH; }
<INITIAL>{NOT}		{ count(); return NOT; }
<INITIAL>{AND}		{ count(); return AND; }
<INITIAL>{OR}		{ count(); return OR;  }

<INITIAL>{NUMBER}		{ count(); yylval.intval=atoi(yytext);
							return NUMBER; }
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
<INITIAL>{CR}		{ count(); return CR; }


<INITIAL>{QUOTES} { count(); state=STRING_S; BEGIN(STRING1); }
<INITIAL>{TICK} { count(); state=STRING_S; BEGIN(STRING2); }


<STRING1>{QUOTES} { count(); state=INITIAL_S; BEGIN(INITIAL); 
						yytext[yyleng-1]=0; yyleng--;
						addstr(yytext, &str);
						if (str){
							printf("Found string1 <%s>\n", str);
							yyleng=strlen(str)+1;
							memcpy(yytext, str, yyleng);
							free(str);
							str=0;
						}else{
							printf("WARNING: empty string\n");
						}
						yylval.strval=yytext; return STRING;
					}
<STRING2>{TICK}  { count(); state=INITIAL_S; BEGIN(INITIAL); 
						yytext[yyleng-1]=0; yyleng--;
						printf("Found string1 <%s>\n", yytext);
						yylval.strval=yytext; return STRING;
					}
<STRING2>.|{EAT_ABLE}|{CR}	{ yymore(); }

<STRING1>\\n		{ count(); yytext[yyleng-2]='\n';yytext[yyleng-1]=0; 
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

<INITIAL>{ID}			{ count(); yylval.strval=yytext; return ID; }


<<EOF>>							{
									switch(state){
										case STRING_S: 
											printf("Unexpected EOF: closed string\n");
											if (str) {free(str); str=0;}
											break;
										case COMMENT_S:
											printf("Unexpected EOF:%d comments open\n", comment_nest);
											break;
										case COMMENT_LN_S:
											printf("Unexpected EOF: comment line open\n");
											break;
									}
									return 0;
								}
			
%%

static char* addstr(char * src, char ** dest)
{
	char *tmp;
	int len1, len2;
	
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
	fprintf(stderr, "lex:addstr: memory allocation error\n");
	return 0;
}



static void count()
{
	int i;
	
	for (i=0; i<yyleng;i++){
		if (yytext[i]=='\n'){
			line++;
			column=0;
		}else if (yytext[i]=='\t'){
			column+=8 -(column%8);
		}else{
			column++;
		}
	}
}


