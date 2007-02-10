/*
 * $Id$
 *
 * scanner for cfg files
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 *  2003-10-02  added {,set_}advertised_{address,port} (andrei)
 *  2003-10-07  added hex and octal numbers support (andrei)
 *  2003-10-10  replaced len_gt w/ msg:len (andrei)
 *  2003-10-13  added fifo_dir (andrei)
 *  2003-10-28  added tcp_accept_aliases (andrei)
 *  2003-11-29  added {tcp_send, tcp_connect, tls_*}_timeout (andrei)
 *  2004-03-30  added DISABLE_CORE and OPEN_FD_LIMIT (andrei)
 *  2004-04-28  added sock_mode (replaces fifo_mode), sock_user &
 *               sock_group  (andrei)
 *  2004-05-03  applied multicast support patch from janakj
 *              added MCAST_TTL (andrei)
 *  2004-10-08  more escapes: \", \xHH, \nnn and minor optimizations (andrei)
 *  2004-10-19  added FROM_URI and TO_URI (andrei)
 *  2004-11-30  added force_send_socket
 *  2005-07-08  added tcp_connection_lifetime, tcp_poll_method,
 *               tcp_max_connections (andrei)
 *  2005-07-11  added dns_retr_{time,no}, dns_servers_no, dns_use_search_list,
 *              dns_try_ipv6 (andrei)
 *  2005-12-11  added onsend_route, snd_{ip,port,proto,af},
 *              to_{ip,port} (andrei)
 *  2005-12-12  separated drop, exit, break, return, added RETCODE (andrei)
 *  2005-12-19  select framework (mma)
 *  2006-09-11  added dns cache (use, flags, ttls, mem ,gc) & dst blacklist
 *              options (andrei)
 *  2006-10-13  added STUN_ALLOW_STUN, STUN_ALLOW_FP, STUN_REFRESH_INTERVAL
 *              (vlada)
 */


%{
	#include "dprint.h"
	#include "globals.h"
	#include "mem/mem.h"
	#include <string.h>
	#include <stdlib.h>
	#include "ip_addr.h"
	#include "usr_avp.h"
	#include "select.h"
	#include "cfg.tab.h"

	/* states */
	#define INITIAL_S		0
	#define COMMENT_S		1
	#define COMMENT_LN_S	        2
	#define STRING_S		3
	#define ATTR_S                  4
        #define SELECT_S                5

	#define STR_BUF_ALLOC_UNIT	128
	struct str_buf{
		char* s;
		char* crt;
		int left;
	};


	static int comment_nest=0;
	static int state=0, old_state=0, old_initial=0;
	static struct str_buf s_buf;
	int line=1;
	int column=1;
	int startcolumn=1;

	static char* addchar(struct str_buf *, char);
	static char* addstr(struct str_buf *, char*, int);
	static void count();


%}

/* start conditions */
%x STRING1 STRING2 COMMENT COMMENT_LN ATTR SELECT

/* action keywords */
FORWARD	forward
FORWARD_TCP	forward_tcp
FORWARD_UDP	forward_udp
FORWARD_TLS	forward_tls
DROP	"drop"|"exit"
RETURN	"return"
BREAK	"break"
SEND	send
SEND_TCP	send_tcp
LOG		log
ERROR	error
ROUTE	route
ROUTE_FAILURE failure_route
ROUTE_ONREPLY onreply_route
ROUTE_BRANCH branch_route
ROUTE_SEND onsend_route
EXEC	exec
FORCE_RPORT		"force_rport"|"add_rport"
FORCE_TCP_ALIAS		"force_tcp_alias"|"add_tcp_alias"
SETFLAG		setflag
RESETFLAG	resetflag
ISFLAGSET	isflagset
FLAGS_DECL	"flags"|"bool"
SETAVPFLAG	setavpflag
RESETAVPFLAG	resetavpflag
ISAVPFLAGSET	isavpflagset
AVPFLAGS_DECL	avpflags
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
SET_ADV_ADDRESS	"set_advertised_address"
SET_ADV_PORT	"set_advertised_port"
FORCE_SEND_SOCKET	"force_send_socket"

/*ACTION LVALUES*/
URIHOST			"uri:host"
URIPORT			"uri:port"

MAX_LEN			"max_len"


/* condition keywords */
METHOD	method
/* hack -- the second element in first line is referable
   as either uri or status; it only would makes sense to
   call it "uri" from route{} and status from onreply_route{}
*/
URI		"uri"|"status"
FROM_URI	"from_uri"
TO_URI		"to_uri"
SRCIP	src_ip
SRCPORT	src_port
DSTIP	dst_ip
DSTPORT	dst_port
SNDIP	snd_ip
SNDPORT	snd_port
SNDPROTO	snd_proto|to_proto
SNDAF		snd_af|to_af
TOIP	to_ip
TOPORT	to_port
PROTO	proto
AF		af
MYSELF	myself
MSGLEN			"msg:len"
RETCODE	\$\?|\$retcode
/* operators */
EQUAL	=
EQUAL_T	==
GT	>
LT	<
GTE	>=
LTE	<=
DIFF	!=
MATCH	=~
ADDEQ     "+="
NOT		!|"not"
LOG_AND		"and"|"&&"
BIN_AND         "&"
LOG_OR		"or"|"||"
BIN_OR          "|"
PLUS	"+"
MINUS	"-"

/* Attribute specification */
ATTR_MARK   "$"|"%"
SELECT_MARK  "@"
ATTR_FROM         "f"
ATTR_TO           "t"
ATTR_FROMURI      "fr"
ATTR_TOURI       "tr"
ATTR_FROMUSER     "fu"
ATTR_TOUSER       "tu"
ATTR_FROMDOMAIN   "fd"
ATTR_TODOMAIN     "td"
ATTR_GLOBAL       "g"

/* config vars. */
DEBUG	debug
FORK	fork
LOGSTDERROR	log_stderror
LOGFACILITY	log_facility
LISTEN		listen
ALIAS		alias
DNS		 dns
REV_DNS	 rev_dns
DNS_TRY_IPV6	dns_try_ipv6
DNS_RETR_TIME	dns_retr_time
DNS_RETR_NO		dns_retr_no
DNS_SERVERS_NO	dns_servers_no
DNS_USE_SEARCH	dns_use_search_list
/* dns cache */
DNS_USE_CACHE	use_dns_cache
DNS_USE_FAILOVER	use_dns_failover
DNS_CACHE_FLAGS		dns_cache_flags
DNS_CACHE_NEG_TTL	dns_cache_negative_ttl
DNS_CACHE_MIN_TTL	dns_cache_min_ttl
DNS_CACHE_MAX_TTL	dns_cache_max_ttl
DNS_CACHE_MEM		dns_cache_mem
DNS_CACHE_GC_INT	dns_cache_gc_interval
/* blacklist */
USE_DST_BLST		use_dst_blacklist
DST_BLST_MEM		dst_blacklist_mem
DST_BLST_TTL		dst_blacklist_expire|dst_blacklist_ttl
DST_BLST_GC_INT		dst_blacklist_gc_interval


PORT	port
STAT	statistics
MAXBUFFER maxbuffer
CHILDREN children
CHECK_VIA	check_via
SYN_BRANCH syn_branch
MEMLOG		"memlog"|"mem_log"
MEMDBG		"memdbg"|"mem_dbg"
SIP_WARNING sip_warning
SERVER_SIGNATURE server_signature
REPLY_TO_VIA reply_to_via
USER		"user"|"uid"
GROUP		"group"|"gid"
CHROOT		"chroot"
WDIR		"workdir"|"wdir"
MHOMED		mhomed
DISABLE_TCP		"disable_tcp"
TCP_CHILDREN	"tcp_children"
TCP_ACCEPT_ALIASES	"tcp_accept_aliases"
TCP_SEND_TIMEOUT	"tcp_send_timeout"
TCP_CONNECT_TIMEOUT	"tcp_connect_timeout"
TCP_CON_LIFETIME	"tcp_connection_lifetime"
TCP_POLL_METHOD		"tcp_poll_method"
TCP_MAX_CONNECTIONS	"tcp_max_connections"
DISABLE_TLS		"disable_tls"|"tls_disable"
ENABLE_TLS		"enable_tls"|"tls_enable"
TLSLOG			"tlslog"|"tls_log"
TLS_PORT_NO		"tls_port_no"
TLS_METHOD		"tls_method"
TLS_VERIFY		"tls_verify"
TLS_REQUIRE_CERTIFICATE "tls_require_certificate"
TLS_CERTIFICATE	"tls_certificate"
TLS_PRIVATE_KEY "tls_private_key"
TLS_CA_LIST		"tls_ca_list"
TLS_HANDSHAKE_TIMEOUT	"tls_handshake_timeout"
TLS_SEND_TIMEOUT	"tls_send_timeout"
ADVERTISED_ADDRESS	"advertised_address"
ADVERTISED_PORT		"advertised_port"
DISABLE_CORE		"disable_core_dump"
OPEN_FD_LIMIT		"open_files_limit"
MCAST_LOOPBACK		"mcast_loopback"
MCAST_TTL		"mcast_ttl"
TOS			"tos"
KILL_TIMEOUT	"exit_timeout"|"ser_kill_timeout"

/* stun config variables */
STUN_REFRESH_INTERVAL "stun_refresh_interval"
STUN_ALLOW_STUN "stun_allow_stun"
STUN_ALLOW_FP "stun_allow_fp"

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
ID			{LETTER}{ALPHANUM}*
HEX			[0-9a-fA-F]
HEXNUMBER	0x{HEX}+
OCTNUMBER	0[0-7]+
DECNUMBER       0|-?([1-9]{DIGIT}*)
BINNUMBER       [0-1]+b
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
COLON		":"
STAR		\*
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
<INITIAL>{RETURN}	{ count(); yylval.strval=yytext; return RETURN; }
<INITIAL>{BREAK}	{ count(); yylval.strval=yytext; return BREAK; }
<INITIAL>{SEND}	{ count(); yylval.strval=yytext; return SEND; }
<INITIAL>{SEND_TCP}	{ count(); yylval.strval=yytext; return SEND_TCP; }
<INITIAL>{LOG}	{ count(); yylval.strval=yytext; return LOG_TOK; }
<INITIAL>{ERROR}	{ count(); yylval.strval=yytext; return ERROR; }
<INITIAL>{SETFLAG}	{ count(); yylval.strval=yytext; return SETFLAG; }
<INITIAL>{RESETFLAG}	{ count(); yylval.strval=yytext; return RESETFLAG; }
<INITIAL>{ISFLAGSET}	{ count(); yylval.strval=yytext; return ISFLAGSET; }
<INITIAL>{FLAGS_DECL}	{ count(); yylval.strval=yytext; return FLAGS_DECL; }
<INITIAL>{SETAVPFLAG}	{ count(); yylval.strval=yytext; return SETAVPFLAG; }
<INITIAL>{RESETAVPFLAG}	{ count(); yylval.strval=yytext; return RESETAVPFLAG; }
<INITIAL>{ISAVPFLAGSET}	{ count(); yylval.strval=yytext; return ISAVPFLAGSET; }
<INITIAL>{AVPFLAGS_DECL}	{ count(); yylval.strval=yytext; return AVPFLAGS_DECL; }
<INITIAL>{MSGLEN}	{ count(); yylval.strval=yytext; return MSGLEN; }
<INITIAL>{RETCODE}	{ count(); yylval.strval=yytext; return RETCODE; }
<INITIAL>{ROUTE}	{ count(); yylval.strval=yytext; return ROUTE; }
<INITIAL>{ROUTE_ONREPLY}	{ count(); yylval.strval=yytext;
								return ROUTE_ONREPLY; }
<INITIAL>{ROUTE_FAILURE}	{ count(); yylval.strval=yytext;
								return ROUTE_FAILURE; }
<INITIAL>{ROUTE_BRANCH} { count(); yylval.strval=yytext; return ROUTE_BRANCH; }
<INITIAL>{ROUTE_SEND} { count(); yylval.strval=yytext; return ROUTE_SEND; }
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
<INITIAL>{FORCE_TCP_ALIAS}	{ count(); yylval.strval=yytext;
								return FORCE_TCP_ALIAS; }
<INITIAL>{IF}	{ count(); yylval.strval=yytext; return IF; }
<INITIAL>{ELSE}	{ count(); yylval.strval=yytext; return ELSE; }

<INITIAL>{SET_ADV_ADDRESS}	{ count(); yylval.strval=yytext;
										return SET_ADV_ADDRESS; }
<INITIAL>{SET_ADV_PORT}	{ count(); yylval.strval=yytext;
										return SET_ADV_PORT; }
<INITIAL>{FORCE_SEND_SOCKET}	{	count(); yylval.strval=yytext;
									return FORCE_SEND_SOCKET; }

<INITIAL>{URIHOST}	{ count(); yylval.strval=yytext; return URIHOST; }
<INITIAL>{URIPORT}	{ count(); yylval.strval=yytext; return URIPORT; }

<INITIAL>{MAX_LEN}	{ count(); yylval.strval=yytext; return MAX_LEN; }

<INITIAL>{METHOD}	{ count(); yylval.strval=yytext; return METHOD; }
<INITIAL>{URI}	{ count(); yylval.strval=yytext; return URI; }
<INITIAL>{FROM_URI}	{ count(); yylval.strval=yytext; return FROM_URI; }
<INITIAL>{TO_URI}	{ count(); yylval.strval=yytext; return TO_URI; }
<INITIAL>{SRCIP}	{ count(); yylval.strval=yytext; return SRCIP; }
<INITIAL>{SRCPORT}	{ count(); yylval.strval=yytext; return SRCPORT; }
<INITIAL>{DSTIP}	{ count(); yylval.strval=yytext; return DSTIP; }
<INITIAL>{DSTPORT}	{ count(); yylval.strval=yytext; return DSTPORT; }
<INITIAL>{SNDIP}	{ count(); yylval.strval=yytext; return SNDIP; }
<INITIAL>{SNDPORT}	{ count(); yylval.strval=yytext; return SNDPORT; }
<INITIAL>{SNDPROTO}	{ count(); yylval.strval=yytext; return SNDPROTO; }
<INITIAL>{SNDAF}	{ count(); yylval.strval=yytext; return SNDAF; }
<INITIAL>{TOIP}		{ count(); yylval.strval=yytext; return TOIP; }
<INITIAL>{TOPORT}	{ count(); yylval.strval=yytext; return TOPORT; }
<INITIAL>{PROTO}	{ count(); yylval.strval=yytext; return PROTO; }
<INITIAL>{AF}	{ count(); yylval.strval=yytext; return AF; }
<INITIAL>{MYSELF}	{ count(); yylval.strval=yytext; return MYSELF; }

<INITIAL>{DEBUG}	{ count(); yylval.strval=yytext; return DEBUG_V; }
<INITIAL>{FORK}		{ count(); yylval.strval=yytext; return FORK; }
<INITIAL>{LOGSTDERROR}	{ yylval.strval=yytext; return LOGSTDERROR; }
<INITIAL>{LOGFACILITY}	{ yylval.strval=yytext; return LOGFACILITY; }
<INITIAL>{LISTEN}	{ count(); yylval.strval=yytext; return LISTEN; }
<INITIAL>{ALIAS}	{ count(); yylval.strval=yytext; return ALIAS; }
<INITIAL>{DNS}	{ count(); yylval.strval=yytext; return DNS; }
<INITIAL>{REV_DNS}	{ count(); yylval.strval=yytext; return REV_DNS; }
<INITIAL>{DNS_TRY_IPV6}	{ count(); yylval.strval=yytext;
								return DNS_TRY_IPV6; }
<INITIAL>{DNS_RETR_TIME}	{ count(); yylval.strval=yytext;
								return DNS_RETR_TIME; }
<INITIAL>{DNS_RETR_NO}	{ count(); yylval.strval=yytext;
								return DNS_RETR_NO; }
<INITIAL>{DNS_SERVERS_NO}	{ count(); yylval.strval=yytext;
								return DNS_SERVERS_NO; }
<INITIAL>{DNS_USE_SEARCH}	{ count(); yylval.strval=yytext;
								return DNS_USE_SEARCH; }
<INITIAL>{DNS_USE_CACHE}	{ count(); yylval.strval=yytext;
								return DNS_USE_CACHE; }
<INITIAL>{DNS_USE_FAILOVER}	{ count(); yylval.strval=yytext;
								return DNS_USE_FAILOVER; }
<INITIAL>{DNS_CACHE_FLAGS}	{ count(); yylval.strval=yytext;
								return DNS_CACHE_FLAGS; }
<INITIAL>{DNS_CACHE_NEG_TTL}	{ count(); yylval.strval=yytext;
								return DNS_CACHE_NEG_TTL; }
<INITIAL>{DNS_CACHE_MIN_TTL}	{ count(); yylval.strval=yytext;
								return DNS_CACHE_MIN_TTL; }
<INITIAL>{DNS_CACHE_MAX_TTL}	{ count(); yylval.strval=yytext;
								return DNS_CACHE_MAX_TTL; }
<INITIAL>{DNS_CACHE_MEM}	{ count(); yylval.strval=yytext;
								return DNS_CACHE_MEM; }
<INITIAL>{DNS_CACHE_GC_INT}	{ count(); yylval.strval=yytext;
								return DNS_CACHE_GC_INT; }
<INITIAL>{USE_DST_BLST}	{ count(); yylval.strval=yytext;
								return USE_DST_BLST; }
<INITIAL>{DST_BLST_MEM}	{ count(); yylval.strval=yytext;
								return DST_BLST_MEM; }
<INITIAL>{DST_BLST_TTL}	{ count(); yylval.strval=yytext;
								return DST_BLST_TTL; }
<INITIAL>{DST_BLST_GC_INT}	{ count(); yylval.strval=yytext;
								return DST_BLST_GC_INT; }
<INITIAL>{PORT}	{ count(); yylval.strval=yytext; return PORT; }
<INITIAL>{STAT}	{ count(); yylval.strval=yytext; return STAT; }
<INITIAL>{MAXBUFFER}	{ count(); yylval.strval=yytext; return MAXBUFFER; }
<INITIAL>{CHILDREN}	{ count(); yylval.strval=yytext; return CHILDREN; }
<INITIAL>{CHECK_VIA}	{ count(); yylval.strval=yytext; return CHECK_VIA; }
<INITIAL>{SYN_BRANCH}	{ count(); yylval.strval=yytext; return SYN_BRANCH; }
<INITIAL>{MEMLOG}	{ count(); yylval.strval=yytext; return MEMLOG; }
<INITIAL>{MEMDBG}	{ count(); yylval.strval=yytext; return MEMDBG; }
<INITIAL>{SIP_WARNING}	{ count(); yylval.strval=yytext; return SIP_WARNING; }
<INITIAL>{USER}		{ count(); yylval.strval=yytext; return USER; }
<INITIAL>{GROUP}	{ count(); yylval.strval=yytext; return GROUP; }
<INITIAL>{CHROOT}	{ count(); yylval.strval=yytext; return CHROOT; }
<INITIAL>{WDIR}	{ count(); yylval.strval=yytext; return WDIR; }
<INITIAL>{MHOMED}	{ count(); yylval.strval=yytext; return MHOMED; }
<INITIAL>{DISABLE_TCP}	{ count(); yylval.strval=yytext; return DISABLE_TCP; }
<INITIAL>{TCP_CHILDREN}	{ count(); yylval.strval=yytext; return TCP_CHILDREN; }
<INITIAL>{TCP_ACCEPT_ALIASES}	{ count(); yylval.strval=yytext;
									return TCP_ACCEPT_ALIASES; }
<INITIAL>{TCP_SEND_TIMEOUT}		{ count(); yylval.strval=yytext;
									return TCP_SEND_TIMEOUT; }
<INITIAL>{TCP_CONNECT_TIMEOUT}		{ count(); yylval.strval=yytext;
									return TCP_CONNECT_TIMEOUT; }
<INITIAL>{TCP_CON_LIFETIME}		{ count(); yylval.strval=yytext;
									return TCP_CON_LIFETIME; }
<INITIAL>{TCP_POLL_METHOD}		{ count(); yylval.strval=yytext;
									return TCP_POLL_METHOD; }
<INITIAL>{TCP_MAX_CONNECTIONS}	{ count(); yylval.strval=yytext;
									return TCP_MAX_CONNECTIONS; }
<INITIAL>{DISABLE_TLS}	{ count(); yylval.strval=yytext; return DISABLE_TLS; }
<INITIAL>{ENABLE_TLS}	{ count(); yylval.strval=yytext; return ENABLE_TLS; }
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
<INITIAL>{TLS_HANDSHAKE_TIMEOUT}	{ count(); yylval.strval=yytext;
										return TLS_HANDSHAKE_TIMEOUT; }
<INITIAL>{TLS_SEND_TIMEOUT}	{ count(); yylval.strval=yytext;
										return TLS_SEND_TIMEOUT; }
<INITIAL>{SERVER_SIGNATURE}	{ count(); yylval.strval=yytext; return SERVER_SIGNATURE; }
<INITIAL>{REPLY_TO_VIA}	{ count(); yylval.strval=yytext; return REPLY_TO_VIA; }
<INITIAL>{ADVERTISED_ADDRESS}	{	count(); yylval.strval=yytext;
									return ADVERTISED_ADDRESS; }
<INITIAL>{ADVERTISED_PORT}		{	count(); yylval.strval=yytext;
									return ADVERTISED_PORT; }
<INITIAL>{DISABLE_CORE}		{	count(); yylval.strval=yytext;
									return DISABLE_CORE; }
<INITIAL>{OPEN_FD_LIMIT}		{	count(); yylval.strval=yytext;
									return OPEN_FD_LIMIT; }
<INITIAL>{MCAST_LOOPBACK}		{	count(); yylval.strval=yytext;
									return MCAST_LOOPBACK; }
<INITIAL>{MCAST_TTL}		{	count(); yylval.strval=yytext;
									return MCAST_TTL; }
<INITIAL>{TOS}			{	count(); yylval.strval=yytext;
									return TOS; }
<INITIAL>{KILL_TIMEOUT}			{	count(); yylval.strval=yytext;
									return KILL_TIMEOUT; }
<INITIAL>{LOADMODULE}	{ count(); yylval.strval=yytext; return LOADMODULE; }
<INITIAL>{MODPARAM}     { count(); yylval.strval=yytext; return MODPARAM; }

<INITIAL>{STUN_REFRESH_INTERVAL} { count(); yylval.strval=yytext; return STUN_REFRESH_INTERVAL;}
<INITIAL>{STUN_ALLOW_STUN} { count(); yylval.strval=yytext; return STUN_ALLOW_STUN;}
<INITIAL>{STUN_ALLOW_FP} { count(); yylval.strval=yytext; return STUN_ALLOW_FP;}

<INITIAL>{EQUAL}	{ count(); return EQUAL; }
<INITIAL>{ADDEQ}          { count(); return ADDEQ; }
<INITIAL>{EQUAL_T}	{ count(); return EQUAL_T; }
<INITIAL>{GT}	{ count(); return GT; }
<INITIAL>{LT}	{ count(); return LT; }
<INITIAL>{GTE}	{ count(); return GTE; }
<INITIAL>{LTE}	{ count(); return LTE; }
<INITIAL>{DIFF}	{ count(); return DIFF; }
<INITIAL>{MATCH}	{ count(); return MATCH; }
<INITIAL>{NOT}		{ count(); return NOT; }
<INITIAL>{LOG_AND}	{ count(); return LOG_AND; }
<INITIAL>{BIN_AND}	{ count(); return BIN_AND; }
<INITIAL>{LOG_OR}	{ count(); return LOG_OR;  }
<INITIAL>{BIN_OR}	{ count(); return BIN_OR;  }
<INITIAL>{PLUS}		{ count(); return PLUS; }
<INITIAL>{MINUS}	{ count(); return MINUS; }

<INITIAL>{SELECT_MARK}  { count(); state = SELECT_S; BEGIN(SELECT); return SELECT_MARK; }
<SELECT>{ID}		{ count(); addstr(&s_buf, yytext, yyleng);
                          yylval.strval=s_buf.s;
                          memset(&s_buf, 0, sizeof(s_buf));
                          return ID;
                        }
<SELECT>{DOT}           { count(); return DOT; }
<SELECT>{LBRACK}        { count(); return LBRACK; }
<SELECT>{RBRACK}        { count(); return RBRACK; }
<SELECT>{DECNUMBER}	{ count(); yylval.intval=atoi(yytext);return NUMBER; }
<SELECT>{HEXNUMBER}	{ count(); yylval.intval=(int)strtol(yytext, 0, 16); return NUMBER; }
<SELECT>{OCTNUMBER}	{ count(); yylval.intval=(int)strtol(yytext, 0, 8); return NUMBER; }
<SELECT>{BINNUMBER}     { count(); yylval.intval=(int)strtol(yytext, 0, 2); return NUMBER; }


<INITIAL>{ATTR_MARK}    { count(); state = ATTR_S; BEGIN(ATTR); return ATTR_MARK; }
<ATTR>{ATTR_FROM}       { count(); return ATTR_FROM; }
<ATTR>{ATTR_TO}         { count(); return ATTR_TO; }
<ATTR>{ATTR_FROMURI}    { count(); return ATTR_FROMURI; }
<ATTR>{ATTR_TOURI}      { count(); return ATTR_TOURI; }
<ATTR>{ATTR_FROMUSER}   { count(); return ATTR_FROMUSER; }
<ATTR>{ATTR_TOUSER}     { count(); return ATTR_TOUSER; }
<ATTR>{ATTR_FROMDOMAIN} { count(); return ATTR_FROMDOMAIN; }
<ATTR>{ATTR_TODOMAIN}   { count(); return ATTR_TODOMAIN; }
<ATTR>{ATTR_GLOBAL}     { count(); return ATTR_GLOBAL; }
<ATTR>{DOT}             { count(); return DOT; }
<ATTR>{LBRACK}          { count(); return LBRACK; }
<ATTR>{RBRACK}          { count(); return RBRACK; }
<ATTR>{STAR}		{ count(); return STAR; }
<ATTR>{DECNUMBER}	{ count(); yylval.intval=atoi(yytext);return NUMBER; }
<ATTR>{ID}		{ count(); addstr(&s_buf, yytext, yyleng);
                           yylval.strval=s_buf.s;
			   memset(&s_buf, 0, sizeof(s_buf));
                           state = INITIAL_S;
                           BEGIN(INITIAL);
			   return ID;
                        }

<INITIAL>{IPV6ADDR}		{ count(); yylval.strval=yytext; return IPV6ADDR; }
<INITIAL>{DECNUMBER}		{ count(); yylval.intval=atoi(yytext);return NUMBER; }
<INITIAL>{HEXNUMBER}	{ count(); yylval.intval=(int)strtol(yytext, 0, 16);
							return NUMBER; }
<INITIAL>{OCTNUMBER}	{ count(); yylval.intval=(int)strtol(yytext, 0, 8);
							return NUMBER; }
<INITIAL>{BINNUMBER}    { count(); yylval.intval=(int)strtol(yytext, 0, 2); return NUMBER; }
<INITIAL>{YES}			{ count(); yylval.intval=1; return NUMBER; }
<INITIAL>{NO}			{ count(); yylval.intval=0; return NUMBER; }
<INITIAL>{TCP}			{ count(); return TCP; }
<INITIAL>{UDP}			{ count(); return UDP; }
<INITIAL>{TLS}			{ count(); return TLS; }
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
<INITIAL>{COLON}	{ count(); return COLON; }
<INITIAL>{STAR}	{ count(); return STAR; }
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


<INITIAL,SELECT>{QUOTES} { count(); old_initial = YY_START; old_state = state; state=STRING_S; BEGIN(STRING1); }
<INITIAL>{TICK} { count(); old_initial = YY_START; old_state = state; state=STRING_S; BEGIN(STRING2); }


<STRING1>{QUOTES} { count(); state=old_state; BEGIN(old_initial);
						yytext[yyleng-1]=0; yyleng--;
						addstr(&s_buf, yytext, yyleng);
						yylval.strval=s_buf.s;
						memset(&s_buf, 0, sizeof(s_buf));
						return STRING;
					}
<STRING2>{TICK}  { count(); state=old_state; BEGIN(old_initial);
						yytext[yyleng-1]=0; yyleng--;
						addstr(&s_buf, yytext, yyleng);
						yylval.strval=s_buf.s;
						memset(&s_buf, 0, sizeof(s_buf));
						return STRING;
					}
<STRING2>.|{EAT_ABLE}|{CR}	{ yymore(); }

<STRING1>\\n		{ count(); addchar(&s_buf, '\n'); }
<STRING1>\\r		{ count(); addchar(&s_buf, '\r'); }
<STRING1>\\a		{ count(); addchar(&s_buf, '\a'); }
<STRING1>\\t		{ count(); addchar(&s_buf, '\t'); }
<STRING1>\\{QUOTES}	{ count(); addchar(&s_buf, '"');  }
<STRING1>\\\\		{ count(); addchar(&s_buf, '\\'); }
<STRING1>\\x{HEX}{1,2}	{ count(); addchar(&s_buf,
											(char)strtol(yytext+2, 0, 16)); }
 /* don't allow \[0-7]{1}, it will eat the backreferences from
    subst_uri if allowed (although everybody should use '' in subt_uri) */
<STRING1>\\[0-7]{2,3}	{ count(); addchar(&s_buf,
											(char)strtol(yytext+1, 0, 8));  }
<STRING1>\\{CR}		{ count(); } /* eat escaped CRs */
<STRING1>.|{EAT_ABLE}|{CR}	{ addchar(&s_buf, *yytext); }


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

<INITIAL>{ID}			{ count(); addstr(&s_buf, yytext, yyleng);
									yylval.strval=s_buf.s;
									memset(&s_buf, 0, sizeof(s_buf));
									return ID; }

<SELECT>.               { unput(yytext[0]); state = INITIAL_S; BEGIN(INITIAL); } /* Rescan the token in INITIAL state */


<<EOF>>							{
									switch(state){
										case STRING_S:
											LOG(L_CRIT, "ERROR: cfg. parser: unexpected EOF in"
														" unclosed string\n");
											if (s_buf.s){
												pkg_free(s_buf.s);
												memset(&s_buf, 0,
															sizeof(s_buf));
											}
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


static char* addchar(struct str_buf* dst, char c)
{
	return addstr(dst, &c, 1);
}



static char* addstr(struct str_buf* dst_b, char* src, int len)
{
	char *tmp;
	unsigned size;
	unsigned used;

	if (dst_b->left<(len+1)){
		used=(unsigned)(dst_b->crt-dst_b->s);
		size=used+len+1;
		/* round up to next multiple */
		size+= STR_BUF_ALLOC_UNIT-size%STR_BUF_ALLOC_UNIT;
		tmp=pkg_malloc(size);
		if (tmp==0) goto error;
		if (dst_b->s){
			memcpy(tmp, dst_b->s, used);
			pkg_free(dst_b->s);
		}
		dst_b->s=tmp;
		dst_b->crt=dst_b->s+used;
		dst_b->left=size-used;
	}
	memcpy(dst_b->crt, src, len);
	dst_b->crt+=len;
	*(dst_b->crt)=0;
	dst_b->left-=len;

	return dst_b->s;
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



/* replacement yywrap, removes libfl dependency */
int yywrap()
{
	return 1;
}
