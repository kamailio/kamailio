/*
 * $Id$
 *
 * scanner for cfg files
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
 * -------
 *  2003-01-29  src_port added (jiri)
 *  2003-01-23  mhomed added (jiri)
 *  2003-03-19  replaced all the mallocs/frees w/ pkg_malloc/pkg_free (andrei)
 *  2003-04-01  added dst_port, proto (tcp, udp, tls), af(inet, inet6) (andrei)
 *  2003-04-05  s/reply_route/failure_route, onreply_route introduced (jiri)
 *  2003-04-12  added force_rport, chdir and wdir (andrei)
 *  2003-04-22  strip_tail added (jiri)
 *  2003-07-03  tls* (disable, certificate, private_key, ca_list, verify, 
 *               require_certificate added (andrei)
 *  2003-07-06  more tls config. vars added: tls_method, tls_port_no (andrei)
 */


%{
	#include "cfg.tab.h"
	#include "dprint.h"
	#include "globals.h"
	#include "mem/mem.h"
	#include <string.h>
	#include <stdlib.h>
	#include "ip_addr.h"


	/* states */
	#define INITIAL_S		0
	#define COMMENT_S		1
	#define COMMENT_LN_S	2
	#define STRING_S		3

	
	static int comment_nest=0;
	static int state=0;
	static char* tstr=0;
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
FORWARD_TCP	forward_tcp
FORWARD_UDP	forward_udp
FORWARD_TLS	forward_tls
DROP	"drop"|"break"
SEND	send
SEND_TCP	send_tcp
LOG		log
ERROR	error
ROUTE	route
ROUTE_FAILURE failure_route
ROUTE_ONREPLY onreply_route
EXEC	exec
FORCE_RPORT		"force_rport"|"add_rport"
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
REVERT_URI		"revert_uri"
PREFIX			"prefix"
STRIP			"strip"
STRIP_TAIL		"strip_tail"
APPEND_BRANCH	"append_branch"
IF				"if"
ELSE			"else"

/*ACTION LVALUES*/
URIHOST			"uri:host"
URIPORT			"uri:port"

MAX_LEN			"max_len"


/* condition keywords */
METHOD	method
/* hack -- the second element in first line is referrable
   as either uri or status; it only would makes sense to
   call it "uri" from route{} and status from onreply_route{}
*/
URI		"uri"|"status"
SRCIP	src_ip
SRCPORT	src_port
DSTIP	dst_ip
DSTPORT	dst_port
PROTO	proto
AF		af
MYSELF	myself
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
ALIAS		alias
DNS		 dns
REV_DNS	 rev_dns
PORT	port
STAT	statistics
MAXBUFFER maxbuffer
CHILDREN children
CHECK_VIA	check_via
SYN_BRANCH syn_branch
MEMLOG		"memlog"|"mem_log"
SIP_WARNING sip_warning
FIFO fifo
FIFO_MODE fifo_mode
SERVER_SIGNATURE server_signature
REPLY_TO_VIA reply_to_via
USER		"user"|"uid"
GROUP		"group"|"gid"
CHROOT		"chroot"
WDIR		"workdir"|"wdir"
MHOMED		mhomed
DISABLE_TCP		"disable_tcp"
TCP_CHILDREN	"tcp_children"
DISABLE_TLS		"disable_tls"
TLSLOG			"tlslog"|"tls_log"
TLS_PORT_NO		"tls_port_no"
TLS_METHOD		"tls_method"
TLS_VERIFY		"tls_verify"
TLS_REQUIRE_CERTIFICATE "tls_require_certificate"
TLS_CERTIFICATE	"tls_certificate"
TLS_PRIVATE_KEY "tls_private_key"
TLS_CA_LIST		"tls_ca_list"

LOADMODULE	loadmodule
MODPARAM        modparam

/* values */
YES			"yes"|"true"|"on"|"enable"
NO			"no"|"false"|"off"|"disable"
UDP			"udp"|"UDP"
TCP			"tcp"|"TCP"
TLS			"tls"|"TLS"
INET		"inet"|"INET"
INET6		"inet6"|"INET6"
SSLv23			"sslv23"|"SSLv23"|"SSLV23"
SSLv2			"sslv2"|"SSLv2"|"SSLV2"
SSLv3			"sslv3"|"SSLv3"|"SSLV3"
TLSv1			"tlsv1"|"TLSv1"|"TLSV1"

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
<INITIAL>{FORWARD_TCP}	{count(); yylval.strval=yytext; return FORWARD_TCP; }
<INITIAL>{FORWARD_TLS}	{count(); yylval.strval=yytext; return FORWARD_TLS; }
<INITIAL>{FORWARD_UDP}	{count(); yylval.strval=yytext; return FORWARD_UDP; }
<INITIAL>{DROP}	{ count(); yylval.strval=yytext; return DROP; }
<INITIAL>{SEND}	{ count(); yylval.strval=yytext; return SEND; }
<INITIAL>{SEND_TCP}	{ count(); yylval.strval=yytext; return SEND_TCP; }
<INITIAL>{LOG}	{ count(); yylval.strval=yytext; return LOG_TOK; }
<INITIAL>{ERROR}	{ count(); yylval.strval=yytext; return ERROR; }
<INITIAL>{SETFLAG}	{ count(); yylval.strval=yytext; return SETFLAG; }
<INITIAL>{RESETFLAG}	{ count(); yylval.strval=yytext; return RESETFLAG; }
<INITIAL>{ISFLAGSET}	{ count(); yylval.strval=yytext; return ISFLAGSET; }
<INITIAL>{LEN_GT}	{ count(); yylval.strval=yytext; return LEN_GT; }
<INITIAL>{ROUTE}	{ count(); yylval.strval=yytext; return ROUTE; }
<INITIAL>{ROUTE_ONREPLY}	{ count(); yylval.strval=yytext;
								return ROUTE_ONREPLY; }
<INITIAL>{ROUTE_FAILURE}	{ count(); yylval.strval=yytext;
								return ROUTE_FAILURE; }
<INITIAL>{EXEC}	{ count(); yylval.strval=yytext; return EXEC; }
<INITIAL>{SET_HOST}	{ count(); yylval.strval=yytext; return SET_HOST; }
<INITIAL>{SET_HOSTPORT}	{ count(); yylval.strval=yytext; return SET_HOSTPORT; }
<INITIAL>{SET_USER}	{ count(); yylval.strval=yytext; return SET_USER; }
<INITIAL>{SET_USERPASS}	{ count(); yylval.strval=yytext; return SET_USERPASS; }
<INITIAL>{SET_PORT}	{ count(); yylval.strval=yytext; return SET_PORT; }
<INITIAL>{SET_URI}	{ count(); yylval.strval=yytext; return SET_URI; }
<INITIAL>{REVERT_URI}	{ count(); yylval.strval=yytext; return REVERT_URI; }
<INITIAL>{PREFIX}	{ count(); yylval.strval=yytext; return PREFIX; }
<INITIAL>{STRIP}	{ count(); yylval.strval=yytext; return STRIP; }
<INITIAL>{STRIP_TAIL}	{ count(); yylval.strval=yytext; return STRIP_TAIL; }
<INITIAL>{APPEND_BRANCH}	{ count(); yylval.strval=yytext; 
								return APPEND_BRANCH; }
<INITIAL>{FORCE_RPORT}	{ count(); yylval.strval=yytext; return FORCE_RPORT; }
	
<INITIAL>{IF}	{ count(); yylval.strval=yytext; return IF; }
<INITIAL>{ELSE}	{ count(); yylval.strval=yytext; return ELSE; }

<INITIAL>{URIHOST}	{ count(); yylval.strval=yytext; return URIHOST; }
<INITIAL>{URIPORT}	{ count(); yylval.strval=yytext; return URIPORT; }

<INITIAL>{MAX_LEN}	{ count(); yylval.strval=yytext; return MAX_LEN; }

<INITIAL>{METHOD}	{ count(); yylval.strval=yytext; return METHOD; }
<INITIAL>{URI}	{ count(); yylval.strval=yytext; return URI; }
<INITIAL>{SRCIP}	{ count(); yylval.strval=yytext; return SRCIP; }
<INITIAL>{SRCPORT}	{ count(); yylval.strval=yytext; return SRCPORT; }
<INITIAL>{DSTIP}	{ count(); yylval.strval=yytext; return DSTIP; }
<INITIAL>{DSTPORT}	{ count(); yylval.strval=yytext; return DSTPORT; }
<INITIAL>{PROTO}	{ count(); yylval.strval=yytext; return PROTO; }
<INITIAL>{AF}	{ count(); yylval.strval=yytext; return AF; }
<INITIAL>{MYSELF}	{ count(); yylval.strval=yytext; return MYSELF; }

<INITIAL>{DEBUG}	{ count(); yylval.strval=yytext; return DEBUG; }
<INITIAL>{FORK}		{ count(); yylval.strval=yytext; return FORK; }
<INITIAL>{LOGSTDERROR}	{ yylval.strval=yytext; return LOGSTDERROR; }
<INITIAL>{LISTEN}	{ count(); yylval.strval=yytext; return LISTEN; }
<INITIAL>{ALIAS}	{ count(); yylval.strval=yytext; return ALIAS; }
<INITIAL>{DNS}	{ count(); yylval.strval=yytext; return DNS; }
<INITIAL>{REV_DNS}	{ count(); yylval.strval=yytext; return REV_DNS; }
<INITIAL>{PORT}	{ count(); yylval.strval=yytext; return PORT; }
<INITIAL>{STAT}	{ count(); yylval.strval=yytext; return STAT; }
<INITIAL>{MAXBUFFER}	{ count(); yylval.strval=yytext; return MAXBUFFER; }
<INITIAL>{CHILDREN}	{ count(); yylval.strval=yytext; return CHILDREN; }
<INITIAL>{CHECK_VIA}	{ count(); yylval.strval=yytext; return CHECK_VIA; }
<INITIAL>{SYN_BRANCH}	{ count(); yylval.strval=yytext; return SYN_BRANCH; }
<INITIAL>{MEMLOG}	{ count(); yylval.strval=yytext; return MEMLOG; }
<INITIAL>{SIP_WARNING}	{ count(); yylval.strval=yytext; return SIP_WARNING; }
<INITIAL>{USER}		{ count(); yylval.strval=yytext; return USER; }
<INITIAL>{GROUP}	{ count(); yylval.strval=yytext; return GROUP; }
<INITIAL>{CHROOT}	{ count(); yylval.strval=yytext; return CHROOT; }
<INITIAL>{WDIR}	{ count(); yylval.strval=yytext; return WDIR; }
<INITIAL>{MHOMED}	{ count(); yylval.strval=yytext; return MHOMED; }
<INITIAL>{DISABLE_TCP}	{ count(); yylval.strval=yytext; return DISABLE_TCP; }
<INITIAL>{TCP_CHILDREN}	{ count(); yylval.strval=yytext; return TCP_CHILDREN; }
<INITIAL>{DISABLE_TLS}	{ count(); yylval.strval=yytext; return DISABLE_TLS; }
<INITIAL>{TLSLOG}		{ count(); yylval.strval=yytext; return TLS_PORT_NO; }
<INITIAL>{TLS_PORT_NO}	{ count(); yylval.strval=yytext; return TLS_PORT_NO; }
<INITIAL>{TLS_METHOD}	{ count(); yylval.strval=yytext; return TLS_METHOD; }
<INITIAL>{TLS_VERIFY}	{ count(); yylval.strval=yytext; return TLS_VERIFY; }
<INITIAL>{TLS_REQUIRE_CERTIFICATE}	{ count(); yylval.strval=yytext;
										return TLS_REQUIRE_CERTIFICATE; }
<INITIAL>{TLS_CERTIFICATE}	{ count(); yylval.strval=yytext; 
										return TLS_CERTIFICATE; }
<INITIAL>{TLS_PRIVATE_KEY}	{ count(); yylval.strval=yytext; 
										return TLS_PRIVATE_KEY; }
<INITIAL>{TLS_CA_LIST}	{ count(); yylval.strval=yytext; 
										return TLS_CA_LIST; }
<INITIAL>{FIFO}	{ count(); yylval.strval=yytext; return FIFO; }
<INITIAL>{FIFO_MODE}	{ count(); yylval.strval=yytext; return FIFO_MODE; }
<INITIAL>{SERVER_SIGNATURE}	{ count(); yylval.strval=yytext; return SERVER_SIGNATURE; }
<INITIAL>{REPLY_TO_VIA}	{ count(); yylval.strval=yytext; return REPLY_TO_VIA; }
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
<INITIAL>{TCP}			{ count(); yylval.intval=PROTO_TCP; return NUMBER; }
<INITIAL>{UDP}			{ count(); yylval.intval=PROTO_UDP; return NUMBER; }
<INITIAL>{TLS}			{ count(); yylval.intval=PROTO_TLS; return NUMBER; }
<INITIAL>{INET}			{ count(); yylval.intval=AF_INET; return NUMBER; }
<INITIAL>{INET6}		{ count();
						#ifdef USE_IPV6
						  yylval.intval=AF_INET6;
						#else
						  yylval.intval=-1; /* no match*/
						#endif
						  return NUMBER; }
<INITIAL>{SSLv23}		{ count(); yylval.strval=yytext; return SSLv23; }
<INITIAL>{SSLv2}		{ count(); yylval.strval=yytext; return SSLv2; }
<INITIAL>{SSLv3}		{ count(); yylval.strval=yytext; return SSLv3; }
<INITIAL>{TLSv1}		{ count(); yylval.strval=yytext; return TLSv1; }

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
						addstr(yytext, &tstr);
						yylval.strval=tstr; tstr=0;
						return STRING;
					}
<STRING2>{TICK}  { count(); state=INITIAL_S; BEGIN(INITIAL); 
						yytext[yyleng-1]=0; yyleng--;
						addstr(yytext, &tstr);
						yylval.strval=tstr;
						tstr=0;
						return STRING;
					}
<STRING2>.|{EAT_ABLE}|{CR}	{ yymore(); }

<STRING1>\\n		{ count(); yytext[yyleng-2]='\n';yytext[yyleng-1]=0; 
						yyleng--; addstr(yytext, &tstr); }
<STRING1>\\r		{ count(); yytext[yyleng-2]='\r';yytext[yyleng-1]=0; 
						yyleng--; addstr(yytext, &tstr); }
<STRING1>\\a		{ count(); yytext[yyleng-2]='\a';yytext[yyleng-1]=0; 
						yyleng--; addstr(yytext, &tstr); }
<STRING1>\\t		{ count(); yytext[yyleng-2]='\t';yytext[yyleng-1]=0; 
						yyleng--; addstr(yytext, &tstr); }
<STRING1>\\\\		{ count(); yytext[yyleng-2]='\\';yytext[yyleng-1]=0; 
						yyleng--; addstr(yytext, &tstr); } 
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

<INITIAL>{ID}			{ count(); addstr(yytext, &tstr);
						  yylval.strval=tstr; tstr=0; return ID; }


<<EOF>>							{
									switch(state){
										case STRING_S: 
											LOG(L_CRIT, "ERROR: cfg. parser: unexpected EOF in"
														" unclosed string\n");
											if (tstr) {pkg_free(tstr);tstr=0;}
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
		len1 = strlen(src);
		*dest = pkg_malloc(len1 + 1);
		if (*dest == 0) goto error;
		memcpy(*dest, src, len1 + 1);
	}else{
		len1=strlen(*dest);
		len2=strlen(src);
		tmp=pkg_malloc(len1+len2+1);
		if (tmp==0) goto error;
		memcpy(tmp, *dest, len1);
		memcpy(tmp+len1, src, len2);
		tmp[len1+len2]=0;
		pkg_free(*dest);
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


