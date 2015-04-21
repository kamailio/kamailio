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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
 *  2007-06-07  added SHM_FORCE_ALLOC, MLOCK_PAGES, REAL_TIME, RT_PRIO,
 *              RT_POLICY, RT_TIMER1_PRIO, RT_TIMER1_POLICY, RT_TIMER2_PRIO,
 *              RT_TIMER2_POLICY (andrei)
 *  2007-06-16  added DNS_SRV_LB, DNS_TRY_NAPTR (andrei)
 *  2007-06-18  added DNS_{UDP,TCP,TLS}_PREF (andrei)
 *  2007-09-10  introduced phone2tel option which allows NOT to consider
 *              user=phone URIs as TEL URIs (jiri)
 *  2007-10-10  added DNS_SEARCH_FMATCH (mma)
 *  2007-11-28  added TCP_OPT_{FD_CACHE, DEFER_ACCEPT, DELAYED_ACK, SYNCNT,
 *              LINGER2, KEEPALIVE, KEEPIDLE, KEEPINTVL, KEEPCNT} (andrei)
 *  2008-01-24  added CFG_DESCRIPTION used by cfg_var (Miklos)
 *  2008-11-28  added support for kamailio pvars and avp/pvar guessing (andrei)
 *  2008-12-11  added support for "string1" "string2" (andrei)
 *  2009-03-10  added SET_USERPHONE action (Miklos)
 *  2009-04-24  add strlen, strempty and defined operators (andrei)
 *  2009-03-07  RETCODE, it's now  a core pvar (andrei)
 *  2010-01-10  added SHM_MEM_SZ (andrei)
 *  2010-02-17 added DST_BLST_{UDP,TCP,TLS,SCTP}_IMASK (andrei)
*/

%option noinput

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
	#include "sr_compat.h"
	#include "ppcfg.h"

	/* states */
	#define INITIAL_S		0
	#define COMMENT_S		1
	#define COMMENT_LN_S	        2
	#define STRING_S		3
	#define ATTR_S                  4  /* avp/attr */
	#define SELECT_S                5
	#define AVP_PVAR_S              6  /* avp or pvar */
	#define PVAR_P_S                7  /* pvar: $(...)  or $foo(...)*/
	#define PVARID_S                8  /* $foo.bar...*/
	#define STR_BETWEEN_S		9
	#define LINECOMMENT_S           10
	#define DEFINE_S                11
	#define DEFINE_EOL_S            12
	#define IFDEF_S                 13
	#define IFDEF_EOL_S             14
	#define IFDEF_SKIP_S            15
	#define DEFINE_DATA_S           16
	#define EVRT_NAME_S             17

	#define STR_BUF_ALLOC_UNIT	128
	struct str_buf{
		char* s;
		char* crt;
		int left;
	};


	static int comment_nest=0;
	static int p_nest=0;
	static int state=0, old_state=0, old_initial=0;
	static struct str_buf s_buf;
	int line=1;
	int column=1;
	int startcolumn=1;
	int startline=1;
	char *finame = 0;
	char *routename = 0;
	char *default_routename = 0;

	static int ign_lines=0;
	static int ign_columns=0;
	char* yy_number_str=0; /* str correspondent for the current NUMBER token */
	int r = 0;
	str *sdef = 0;

	static char* addchar(struct str_buf *, char);
	static char* addstr(struct str_buf *, char*, int);
	static void count();
	static void count_more();
	static void count_ignore();

	#define MAX_INCLUDE_DEPTH 10
	static struct sr_yy_state {
		YY_BUFFER_STATE state;
		int line;
		int column;
		int startcolumn;
		int startline;
		char *finame;
		char *routename;
	} include_stack[MAX_INCLUDE_DEPTH];
	static int include_stack_ptr = 0;

	static int sr_push_yy_state(char *fin, int mode);
	static int sr_pop_yy_state();

	static struct sr_yy_fname {
		char *fname;
		struct sr_yy_fname *next;
	} *sr_yy_fname_list = 0;

	static str  *pp_define_get(int len, const char * text);
	static int  pp_ifdef_type(int pos);
	static void pp_ifdef_var(int len, const char * text);
	static void pp_ifdef();
	static void pp_else();
	static void pp_endif();

%}

/* start conditions */
%x STRING1 STRING2 STR_BETWEEN COMMENT COMMENT_LN ATTR SELECT AVP_PVAR PVAR_P 
%x PVARID INCLF IMPTF EVRTNAME
%x LINECOMMENT DEFINE_ID DEFINE_EOL DEFINE_DATA IFDEF_ID IFDEF_EOL IFDEF_SKIP

/* config script types : #!SER  or #!KAMAILIO or #!MAX_COMPAT */
SER_CFG			SER
KAMAILIO_CFG	KAMAILIO|OPENSER
MAXCOMPAT_CFG	MAXCOMPAT|ALL

/* action keywords */
FORWARD	forward
FORWARD_TCP	forward_tcp
FORWARD_UDP	forward_udp
FORWARD_TLS	forward_tls
FORWARD_SCTP	forward_sctp
DROP	"drop"
EXIT	"exit"
RETURN	"return"
BREAK	"break"
LOG		log
ERROR	error
ROUTE	route
ROUTE_REQUEST request_route
ROUTE_FAILURE failure_route
ROUTE_REPLY reply_route
ROUTE_ONREPLY onreply_route
ROUTE_BRANCH branch_route
ROUTE_SEND onsend_route
ROUTE_EVENT event_route
EXEC	exec
FORCE_RPORT		"force_rport"|"add_rport"
ADD_LOCAL_RPORT		"add_local_rport"
FORCE_TCP_ALIAS		"force_tcp_alias"|"add_tcp_alias"
UDP_MTU		"udp_mtu"
UDP_MTU_TRY_PROTO	"udp_mtu_try_proto"
UDP4_RAW		"udp4_raw"
UDP4_RAW_MTU	"udp4_raw_mtu"
UDP4_RAW_TTL	"udp4_raw_ttl"
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
SET_HOSTPORTTRANS	"rewritehostporttrans"|"sethostporttrans"|"sethpt"
SET_USER		"rewriteuser"|"setuser"|"setu"
SET_USERPASS	"rewriteuserpass"|"setuserpass"|"setup"
SET_PORT		"rewriteport"|"setport"|"setp"
SET_URI			"rewriteuri"|"seturi"
REVERT_URI		"revert_uri"
PREFIX			"prefix"
STRIP			"strip"
STRIP_TAIL		"strip_tail"
SET_USERPHONE		"userphone"
REMOVE_BRANCH	"remove_branch"
CLEAR_BRANCHES	"clear_branches"
IF				"if"
ELSE			"else"
SET_ADV_ADDRESS	"set_advertised_address"
SET_ADV_PORT	"set_advertised_port"
FORCE_SEND_SOCKET	"force_send_socket"
SET_FWD_NO_CONNECT		"set_forward_no_connect"
SET_RPL_NO_CONNECT	"set_reply_no_connect"
SET_FWD_CLOSE	"set_forward_close"
SET_RPL_CLOSE	"set_reply_close"
SWITCH			"switch"
CASE			"case"
DEFAULT			"default"
WHILE			"while"

CFG_SELECT	"cfg_select"
CFG_RESET	"cfg_reset"

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
RETCODE	\$\?|\$retcode|\$rc
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
BIN_NOT     "~"
BIN_XOR     "^"
BIN_LSHIFT     "<<"
BIN_RSHIFT     ">>"
PLUS	"+"
MINUS	"-"
MODULO	"mod"
STRLEN	"strlen"
STREMPTY	"strempty"
DEFINED		"defined"
STREQ	eq
INTEQ	ieq
STRDIFF	ne
INTDIFF	ine
INTCAST	\(int\)
STRCAST \(str\)

/* Attribute specification */
ATTR_MARK   "%"
VAR_MARK    "$"
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

/* avp prefix */
AVP_PREF	(([ft][rud]?)|g)\.

/* config vars. */
DEBUG	debug
FORK	fork
FORK_DELAY	fork_delay
MODINIT_DELAY	modinit_delay
LOGSTDERROR	log_stderror
LOGFACILITY	log_facility
LOGNAME		log_name
LOGCOLOR	log_color
LOGPREFIX	log_prefix
LISTEN		listen
ADVERTISE	advertise|ADVERTISE
ALIAS		alias
SR_AUTO_ALIASES	auto_aliases
DNS		 dns
REV_DNS	 rev_dns
DNS_TRY_IPV6	dns_try_ipv6
DNS_TRY_NAPTR	dns_try_naptr
DNS_SRV_LB		dns_srv_lb|dns_srv_loadbalancing
DNS_UDP_PREF	dns_udp_pref|dns_udp_preference
DNS_TCP_PREF	dns_tcp_pref|dns_tcp_preference
DNS_TLS_PREF	dns_tls_pref|dns_tls_preference
DNS_SCTP_PREF	dns_sctp_pref|dns_sctp_preference
DNS_RETR_TIME	dns_retr_time
DNS_RETR_NO		dns_retr_no
DNS_SERVERS_NO	dns_servers_no
DNS_USE_SEARCH	dns_use_search_list
DNS_SEARCH_FMATCH	dns_search_full_match
DNS_NAPTR_IGNORE_RFC	dns_naptr_ignore_rfc
/* dns cache */
DNS_CACHE_INIT	dns_cache_init
DNS_USE_CACHE	use_dns_cache
DNS_USE_FAILOVER	use_dns_failover
DNS_CACHE_FLAGS		dns_cache_flags
DNS_CACHE_NEG_TTL	dns_cache_negative_ttl
DNS_CACHE_MIN_TTL	dns_cache_min_ttl
DNS_CACHE_MAX_TTL	dns_cache_max_ttl
DNS_CACHE_MEM		dns_cache_mem
DNS_CACHE_GC_INT	dns_cache_gc_interval
DNS_CACHE_DEL_NONEXP	dns_cache_del_nonexp|dns_cache_delete_nonexpired
DNS_CACHE_REC_PREF	dns_cache_rec_pref
/* ipv6 auto bind */
AUTO_BIND_IPV6		auto_bind_ipv6
/* blacklist */
DST_BLST_INIT	dst_blacklist_init
USE_DST_BLST		use_dst_blacklist
DST_BLST_MEM		dst_blacklist_mem
DST_BLST_TTL		dst_blacklist_expire|dst_blacklist_ttl
DST_BLST_GC_INT		dst_blacklist_gc_interval
DST_BLST_UDP_IMASK	dst_blacklist_udp_imask
DST_BLST_TCP_IMASK	dst_blacklist_tcp_imask
DST_BLST_TLS_IMASK	dst_blacklist_tls_imask
DST_BLST_SCTP_IMASK	dst_blacklist_sctp_imask


PORT	port
STAT	statistics
MAXBUFFER maxbuffer
SQL_BUFFER_SIZE sql_buffer_size
CHILDREN children
SOCKET_WORKERS socket_workers
ASYNC_WORKERS async_workers
CHECK_VIA	check_via
PHONE2TEL	phone2tel
MEMLOG		"memlog"|"mem_log"
MEMDBG		"memdbg"|"mem_dbg"
MEMSUM		"mem_summary"
MEMSAFETY	"mem_safety"
MEMJOIN		"mem_join"
CORELOG		"corelog"|"core_log"
SIP_WARNING sip_warning
SERVER_SIGNATURE server_signature
SERVER_HEADER server_header
USER_AGENT_HEADER user_agent_header
REPLY_TO_VIA reply_to_via
USER		"user"|"uid"
GROUP		"group"|"gid"
CHROOT		"chroot"
WDIR		"workdir"|"wdir"
RUNDIR		"rundir"|"run_dir"
MHOMED		mhomed
DISABLE_TCP		"disable_tcp"
TCP_CHILDREN	"tcp_children"
TCP_ACCEPT_ALIASES	"tcp_accept_aliases"
TCP_SEND_TIMEOUT	"tcp_send_timeout"
TCP_CONNECT_TIMEOUT	"tcp_connect_timeout"
TCP_CON_LIFETIME	"tcp_connection_lifetime"
TCP_POLL_METHOD		"tcp_poll_method"
TCP_MAX_CONNECTIONS	"tcp_max_connections"
TLS_MAX_CONNECTIONS	"tls_max_connections"
TCP_NO_CONNECT		"tcp_no_connect"
TCP_SOURCE_IPV4		"tcp_source_ipv4"
TCP_SOURCE_IPV6		"tcp_source_ipv6"
TCP_OPT_FD_CACHE	"tcp_fd_cache"
TCP_OPT_BUF_WRITE	"tcp_buf_write"|"tcp_async"
TCP_OPT_CONN_WQ_MAX	"tcp_conn_wq_max"
TCP_OPT_WQ_MAX		"tcp_wq_max"
TCP_OPT_RD_BUF		"tcp_rd_buf_size"
TCP_OPT_WQ_BLK		"tcp_wq_blk_size"
TCP_OPT_DEFER_ACCEPT "tcp_defer_accept"
TCP_OPT_DELAYED_ACK	"tcp_delayed_ack"
TCP_OPT_SYNCNT		"tcp_syncnt"
TCP_OPT_LINGER2		"tcp_linger2"
TCP_OPT_KEEPALIVE	"tcp_keepalive"
TCP_OPT_KEEPIDLE	"tcp_keepidle"
TCP_OPT_KEEPINTVL	"tcp_keepintvl"
TCP_OPT_KEEPCNT		"tcp_keepcnt"
TCP_OPT_CRLF_PING	"tcp_crlf_ping"
TCP_OPT_ACCEPT_NO_CL	"tcp_accept_no_cl"
TCP_CLONE_RCVBUF	"tcp_clone_rcvbuf"
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
DISABLE_SCTP	"disable_sctp"
ENABLE_SCTP	"enable_sctp"
SCTP_CHILDREN	"sctp_children"

ADVERTISED_ADDRESS	"advertised_address"
ADVERTISED_PORT		"advertised_port"
DISABLE_CORE		"disable_core_dump"
OPEN_FD_LIMIT		"open_files_limit"
SHM_MEM_SZ		"shm"|"shm_mem"|"shm_mem_size"
SHM_FORCE_ALLOC		"shm_force_alloc"
MLOCK_PAGES			"mlock_pages"
REAL_TIME			"real_time"
RT_PRIO				"rt_prio"
RT_POLICY			"rt_policy"
RT_TIMER1_PRIO		"rt_timer1_prio"|"rt_fast_timer_prio"|"rt_ftimer_prio"
RT_TIMER1_POLICY	"rt_timer1_policy"|"rt_ftimer_policy"
RT_TIMER2_PRIO		"rt_timer2_prio"|"rt_stimer_prio"
RT_TIMER2_POLICY	"rt_timer2_policy"|"rt_stimer_policy"
MCAST_LOOPBACK		"mcast_loopback"
MCAST_TTL		"mcast_ttl"
TOS			"tos"
PMTU_DISCOVERY	"pmtu_discovery"
KILL_TIMEOUT	"exit_timeout"|"ser_kill_timeout"
MAX_WLOOPS		"max_while_loops"
PVBUFSIZE		"pv_buffer_size"
PVBUFSLOTS		"pv_buffer_slots"
HTTP_REPLY_PARSE		"http_reply_hack"|"http_reply_parse"
VERSION_TABLE_CFG		"version_table"

SERVER_ID     "server_id"

MAX_RECURSIVE_LEVEL		"max_recursive_level"
MAX_BRANCHES_PARAM		"max_branches"|"max_branches"

LATENCY_LOG				latency_log
LATENCY_LIMIT_DB		latency_limit_db
LATENCY_LIMIT_ACTION	latency_limit_action

MSG_TIME	msg_time
ONSEND_RT_REPLY		"onsend_route_reply"
CFG_DESCRIPTION		"description"|"descr"|"desc"

LOADMODULE	loadmodule
LOADPATH	"loadpath"|"mpath"
MODPARAM        modparam

/* values */
YES			"yes"|"true"|"on"|"enable"
NO			"no"|"false"|"off"|"disable"
UDP			"udp"|"UDP"
TCP			"tcp"|"TCP"
TLS			"tls"|"TLS"
SCTP		"sctp"|"SCTP"
WS		"ws"|"WS"
WSS		"wss"|"WSS"
INET		"inet"|"INET"
INET6		"inet6"|"INET6"
SSLv23			"sslv23"|"SSLv23"|"SSLV23"
SSLv2			"sslv2"|"SSLv2"|"SSLV2"
SSLv3			"sslv3"|"SSLv3"|"SSLV3"
TLSv1			"tlsv1"|"TLSv1"|"TLSV1"

LETTER		[a-zA-Z]
DIGIT		[0-9]
LETTER_     {LETTER}|[_]
ALPHANUM    {LETTER_}|{DIGIT}
ID          {LETTER_}{ALPHANUM}*
NUM_ID		{ALPHANUM}+
HEX			[0-9a-fA-F]
HEXNUMBER	0x{HEX}+
OCTNUMBER	0[0-7]+
DECNUMBER       0|([1-9]{DIGIT}*)
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
EVENT_RT_NAME [a-zA-Z][0-9a-zA-Z-]*(":"[a-zA-Z][0-9a-zA-Z-]*)+


COM_LINE	"#"|"//"
COM_START	"/\*"
COM_END		"\*/"

/* start of pre-processing directives */
PREP_START	"#!"|"!!"

DEFINE       "define"|"def"
IFDEF        ifdef
IFNDEF       ifndef
ENDIF        endif
TRYDEF       "trydefine"|"trydef"
REDEF        "redefine"|"redef"

/* else is already defined */

EAT_ABLE	[\ \t\b\r]

/* pre-processing blocks */
SUBST       subst
SUBSTDEF    substdef
SUBSTDEFS   substdefs

/* include files */
INCLUDEFILE     "include_file"
IMPORTFILE      "import_file"

%%


<INITIAL>{EAT_ABLE}	{ count(); }

<INITIAL>{FORWARD}	{count(); yylval.strval=yytext; return FORWARD; }
<INITIAL>{FORWARD_TCP}	{count(); yylval.strval=yytext; return FORWARD_TCP; }
<INITIAL>{FORWARD_TLS}	{count(); yylval.strval=yytext; return FORWARD_TLS; }
<INITIAL>{FORWARD_SCTP}	{count(); yylval.strval=yytext; return FORWARD_SCTP;}
<INITIAL>{FORWARD_UDP}	{count(); yylval.strval=yytext; return FORWARD_UDP; }
<INITIAL>{DROP}	{ count(); yylval.strval=yytext; return DROP; }
<INITIAL>{EXIT}	{ count(); yylval.strval=yytext; return EXIT; }
<INITIAL>{RETURN}	{ count(); yylval.strval=yytext; return RETURN; }
<INITIAL>{BREAK}	{ count(); yylval.strval=yytext; return BREAK; }
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
<INITIAL>{ROUTE}	{ count(); default_routename="DEFAULT_ROUTE";
						yylval.strval=yytext; return ROUTE; }
<INITIAL>{ROUTE_REQUEST}	{ count(); default_routename="DEFAULT_ROUTE";
								yylval.strval=yytext; return ROUTE_REQUEST; }
<INITIAL>{ROUTE_ONREPLY}	{ count(); default_routename="DEFAULT_ONREPLY";
								yylval.strval=yytext;
								return ROUTE_ONREPLY; }
<INITIAL>{ROUTE_REPLY}	{ count(); default_routename="DEFAULT_ONREPLY";
							yylval.strval=yytext; return ROUTE_REPLY; }
<INITIAL>{ROUTE_FAILURE}	{ count(); default_routename="DEFAULT_FAILURE";
								yylval.strval=yytext;
								return ROUTE_FAILURE; }
<INITIAL>{ROUTE_BRANCH} { count(); default_routename="DEFAULT_BRANCH";
							yylval.strval=yytext; return ROUTE_BRANCH; }
<INITIAL>{ROUTE_SEND} { count(); default_routename="DEFAULT_SEND";
							yylval.strval=yytext; return ROUTE_SEND; }
<INITIAL>{ROUTE_EVENT} { count(); default_routename="DEFAULT_EVENT";
							yylval.strval=yytext;
							state=EVRT_NAME_S; BEGIN(EVRTNAME);
							return ROUTE_EVENT; }
<EVRTNAME>{LBRACK}          { count(); return LBRACK; }
<EVRTNAME>{EAT_ABLE}|{CR}				{ count(); };
<EVRTNAME>{EVENT_RT_NAME}	{ count();
								addstr(&s_buf, yytext, yyleng);
								yylval.strval=s_buf.s;
								memset(&s_buf, 0, sizeof(s_buf));
								return EVENT_RT_NAME; }
<EVRTNAME>{RBRACK}          { count();
								state=INITIAL_S; BEGIN(INITIAL);
								return RBRACK; }
<INITIAL>{EXEC}	{ count(); yylval.strval=yytext; return EXEC; }
<INITIAL>{SET_HOST}	{ count(); yylval.strval=yytext; return SET_HOST; }
<INITIAL>{SET_HOSTPORT}	{ count(); yylval.strval=yytext; return SET_HOSTPORT; }
<INITIAL>{SET_HOSTPORTTRANS}	{ count(); yylval.strval=yytext; return SET_HOSTPORTTRANS; }
<INITIAL>{SET_USER}	{ count(); yylval.strval=yytext; return SET_USER; }
<INITIAL>{SET_USERPASS}	{ count(); yylval.strval=yytext; return SET_USERPASS; }
<INITIAL>{SET_PORT}	{ count(); yylval.strval=yytext; return SET_PORT; }
<INITIAL>{SET_URI}	{ count(); yylval.strval=yytext; return SET_URI; }
<INITIAL>{REVERT_URI}	{ count(); yylval.strval=yytext; return REVERT_URI; }
<INITIAL>{PREFIX}	{ count(); yylval.strval=yytext; return PREFIX; }
<INITIAL>{STRIP}	{ count(); yylval.strval=yytext; return STRIP; }
<INITIAL>{STRIP_TAIL}	{ count(); yylval.strval=yytext; return STRIP_TAIL; }
<INITIAL>{REMOVE_BRANCH}	{ count(); yylval.strval=yytext;
								return REMOVE_BRANCH; }
<INITIAL>{CLEAR_BRANCHES}	{ count(); yylval.strval=yytext;
								return CLEAR_BRANCHES; }
<INITIAL>{SET_USERPHONE}	{ count(); yylval.strval=yytext;
								return SET_USERPHONE; }
<INITIAL>{FORCE_RPORT}	{ count(); yylval.strval=yytext; return FORCE_RPORT; }
<INITIAL>{ADD_LOCAL_RPORT}	{ count(); yylval.strval=yytext;
								return ADD_LOCAL_RPORT; }
<INITIAL>{FORCE_TCP_ALIAS}	{ count(); yylval.strval=yytext;
								return FORCE_TCP_ALIAS; }
<INITIAL>{UDP_MTU}	{ count(); yylval.strval=yytext; return UDP_MTU; }
<INITIAL>{UDP_MTU_TRY_PROTO}	{ count(); yylval.strval=yytext;
									return UDP_MTU_TRY_PROTO; }
<INITIAL>{UDP4_RAW}	{ count(); yylval.strval=yytext; return UDP4_RAW; }
<INITIAL>{UDP4_RAW_MTU}	{ count(); yylval.strval=yytext; return UDP4_RAW_MTU; }
<INITIAL>{UDP4_RAW_TTL}	{ count(); yylval.strval=yytext; return UDP4_RAW_TTL; }
<INITIAL>{IF}	{ count(); yylval.strval=yytext; return IF; }
<INITIAL>{ELSE}	{ count(); yylval.strval=yytext; return ELSE; }

<INITIAL>{SET_ADV_ADDRESS}	{ count(); yylval.strval=yytext;
										return SET_ADV_ADDRESS; }
<INITIAL>{SET_ADV_PORT}	{ count(); yylval.strval=yytext;
										return SET_ADV_PORT; }
<INITIAL>{FORCE_SEND_SOCKET}	{	count(); yylval.strval=yytext;
									return FORCE_SEND_SOCKET; }
<INITIAL>{SET_FWD_NO_CONNECT}	{ count(); yylval.strval=yytext;
									return SET_FWD_NO_CONNECT; }
<INITIAL>{SET_RPL_NO_CONNECT}	{ count(); yylval.strval=yytext;
									return SET_RPL_NO_CONNECT; }
<INITIAL>{SET_FWD_CLOSE}		{ count(); yylval.strval=yytext;
									return SET_FWD_CLOSE; }
<INITIAL>{SET_RPL_CLOSE}		{ count(); yylval.strval=yytext;
									return SET_RPL_CLOSE; }
<INITIAL>{SWITCH}	{ count(); yylval.strval=yytext; return SWITCH; }
<INITIAL>{CASE}	{ count(); yylval.strval=yytext; return CASE; }
<INITIAL>{DEFAULT}	{ count(); yylval.strval=yytext; return DEFAULT; }
<INITIAL>{WHILE}	{ count(); yylval.strval=yytext; return WHILE; }

<INITIAL>{INCLUDEFILE}  { count(); BEGIN(INCLF); }
<INITIAL>{PREP_START}{INCLUDEFILE}  { count(); BEGIN(INCLF); }

<INITIAL>{IMPORTFILE}  { count(); BEGIN(IMPTF); }
<INITIAL>{PREP_START}{IMPORTFILE}  { count(); BEGIN(IMPTF); }

<INITIAL>{CFG_SELECT}	{ count(); yylval.strval=yytext; return CFG_SELECT; }
<INITIAL>{CFG_RESET}	{ count(); yylval.strval=yytext; return CFG_RESET; }

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
<INITIAL>{FORK_DELAY}	{ count(); yylval.strval=yytext; return FORK_DELAY; }
<INITIAL>{MODINIT_DELAY}	{ count(); yylval.strval=yytext; return MODINIT_DELAY; }
<INITIAL>{LOGSTDERROR}	{ yylval.strval=yytext; return LOGSTDERROR; }
<INITIAL>{LOGFACILITY}	{ yylval.strval=yytext; return LOGFACILITY; }
<INITIAL>{LOGNAME}	{ yylval.strval=yytext; return LOGNAME; }
<INITIAL>{LOGCOLOR}	{ yylval.strval=yytext; return LOGCOLOR; }
<INITIAL>{LOGPREFIX}	{ yylval.strval=yytext; return LOGPREFIX; }
<INITIAL>{LISTEN}	{ count(); yylval.strval=yytext; return LISTEN; }
<INITIAL>{ADVERTISE}	{ count(); yylval.strval=yytext; return ADVERTISE; }
<INITIAL>{ALIAS}	{ count(); yylval.strval=yytext; return ALIAS; }
<INITIAL>{SR_AUTO_ALIASES}	{ count(); yylval.strval=yytext;
									return SR_AUTO_ALIASES; }
<INITIAL>{DNS}	{ count(); yylval.strval=yytext; return DNS; }
<INITIAL>{REV_DNS}	{ count(); yylval.strval=yytext; return REV_DNS; }
<INITIAL>{DNS_TRY_IPV6}	{ count(); yylval.strval=yytext;
								return DNS_TRY_IPV6; }
<INITIAL>{DNS_TRY_NAPTR}	{ count(); yylval.strval=yytext;
								return DNS_TRY_NAPTR; }
<INITIAL>{DNS_SRV_LB}	{ count(); yylval.strval=yytext;
								return DNS_SRV_LB; }
<INITIAL>{DNS_UDP_PREF}	{ count(); yylval.strval=yytext;
								return DNS_UDP_PREF; }
<INITIAL>{DNS_TCP_PREF}	{ count(); yylval.strval=yytext;
								return DNS_TCP_PREF; }
<INITIAL>{DNS_TLS_PREF}	{ count(); yylval.strval=yytext;
								return DNS_TLS_PREF; }
<INITIAL>{DNS_SCTP_PREF}	{ count(); yylval.strval=yytext;
								return DNS_SCTP_PREF; }
<INITIAL>{DNS_RETR_TIME}	{ count(); yylval.strval=yytext;
								return DNS_RETR_TIME; }
<INITIAL>{DNS_RETR_NO}	{ count(); yylval.strval=yytext;
								return DNS_RETR_NO; }
<INITIAL>{DNS_SERVERS_NO}	{ count(); yylval.strval=yytext;
								return DNS_SERVERS_NO; }
<INITIAL>{DNS_USE_SEARCH}	{ count(); yylval.strval=yytext;
								return DNS_USE_SEARCH; }
<INITIAL>{DNS_SEARCH_FMATCH}	{ count(); yylval.strval=yytext;
								return DNS_SEARCH_FMATCH; }
<INITIAL>{DNS_NAPTR_IGNORE_RFC}	{ count(); yylval.strval=yytext;
								return DNS_NAPTR_IGNORE_RFC; }
<INITIAL>{DNS_CACHE_INIT}	{ count(); yylval.strval=yytext;
								return DNS_CACHE_INIT; }
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
<INITIAL>{DNS_CACHE_DEL_NONEXP}	{ count(); yylval.strval=yytext;
								return DNS_CACHE_DEL_NONEXP; }
<INITIAL>{DNS_CACHE_REC_PREF}	{ count(); yylval.strval=yytext;
								return DNS_CACHE_REC_PREF; }
<INITIAL>{AUTO_BIND_IPV6}	{ count(); yylval.strval=yytext;
								return AUTO_BIND_IPV6; }
<INITIAL>{DST_BLST_INIT}	{ count(); yylval.strval=yytext;
								return DST_BLST_INIT; }
<INITIAL>{USE_DST_BLST}	{ count(); yylval.strval=yytext;
								return USE_DST_BLST; }
<INITIAL>{DST_BLST_MEM}	{ count(); yylval.strval=yytext;
								return DST_BLST_MEM; }
<INITIAL>{DST_BLST_TTL}	{ count(); yylval.strval=yytext;
								return DST_BLST_TTL; }
<INITIAL>{DST_BLST_GC_INT}	{ count(); yylval.strval=yytext;
								return DST_BLST_GC_INT; }
<INITIAL>{DST_BLST_UDP_IMASK}	{ count(); yylval.strval=yytext;
								return DST_BLST_UDP_IMASK; }
<INITIAL>{DST_BLST_TCP_IMASK}	{ count(); yylval.strval=yytext;
								return DST_BLST_TCP_IMASK; }
<INITIAL>{DST_BLST_TLS_IMASK}	{ count(); yylval.strval=yytext;
								return DST_BLST_TLS_IMASK; }
<INITIAL>{DST_BLST_SCTP_IMASK}	{ count(); yylval.strval=yytext;
								return DST_BLST_SCTP_IMASK; }
<INITIAL>{PORT}	{ count(); yylval.strval=yytext; return PORT; }
<INITIAL>{STAT}	{ count(); yylval.strval=yytext; return STAT; }
<INITIAL>{MAXBUFFER}	{ count(); yylval.strval=yytext; return MAXBUFFER; }
<INITIAL>{SQL_BUFFER_SIZE}	{ count(); yylval.strval=yytext; return SQL_BUFFER_SIZE; }
<INITIAL>{CHILDREN}	{ count(); yylval.strval=yytext; return CHILDREN; }
<INITIAL>{SOCKET_WORKERS}	{ count(); yylval.strval=yytext; return SOCKET_WORKERS; }
<INITIAL>{ASYNC_WORKERS}	{ count(); yylval.strval=yytext; return ASYNC_WORKERS; }
<INITIAL>{CHECK_VIA}	{ count(); yylval.strval=yytext; return CHECK_VIA; }
<INITIAL>{PHONE2TEL}	{ count(); yylval.strval=yytext; return PHONE2TEL; }
<INITIAL>{MEMLOG}	{ count(); yylval.strval=yytext; return MEMLOG; }
<INITIAL>{MEMDBG}	{ count(); yylval.strval=yytext; return MEMDBG; }
<INITIAL>{MEMSUM}	{ count(); yylval.strval=yytext; return MEMSUM; }
<INITIAL>{MEMSAFETY}	{ count(); yylval.strval=yytext; return MEMSAFETY; }
<INITIAL>{MEMJOIN}	{ count(); yylval.strval=yytext; return MEMJOIN; }
<INITIAL>{CORELOG}	{ count(); yylval.strval=yytext; return CORELOG; }
<INITIAL>{SIP_WARNING}	{ count(); yylval.strval=yytext; return SIP_WARNING; }
<INITIAL>{USER}		{ count(); yylval.strval=yytext; return USER; }
<INITIAL>{GROUP}	{ count(); yylval.strval=yytext; return GROUP; }
<INITIAL>{CHROOT}	{ count(); yylval.strval=yytext; return CHROOT; }
<INITIAL>{WDIR}	{ count(); yylval.strval=yytext; return WDIR; }
<INITIAL>{RUNDIR}	{ count(); yylval.strval=yytext; return RUNDIR; }
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
<INITIAL>{TLS_MAX_CONNECTIONS}	{ count(); yylval.strval=yytext;
									return TLS_MAX_CONNECTIONS; }
<INITIAL>{TCP_NO_CONNECT}		{ count(); yylval.strval=yytext;
									return TCP_NO_CONNECT; }
<INITIAL>{TCP_SOURCE_IPV4}		{ count(); yylval.strval=yytext;
									return TCP_SOURCE_IPV4; }
<INITIAL>{TCP_SOURCE_IPV6}		{ count(); yylval.strval=yytext;
									return TCP_SOURCE_IPV6; }
<INITIAL>{TCP_OPT_FD_CACHE}		{ count(); yylval.strval=yytext;
									return TCP_OPT_FD_CACHE; }
<INITIAL>{TCP_OPT_CONN_WQ_MAX}	{ count(); yylval.strval=yytext;
									return TCP_OPT_CONN_WQ_MAX; }
<INITIAL>{TCP_OPT_WQ_MAX}	{ count(); yylval.strval=yytext;
									return TCP_OPT_WQ_MAX; }
<INITIAL>{TCP_OPT_RD_BUF}	{ count(); yylval.strval=yytext;
									return TCP_OPT_RD_BUF; }
<INITIAL>{TCP_OPT_WQ_BLK}	{ count(); yylval.strval=yytext;
									return TCP_OPT_WQ_BLK; }
<INITIAL>{TCP_OPT_BUF_WRITE}	{ count(); yylval.strval=yytext;
									return TCP_OPT_BUF_WRITE; }
<INITIAL>{TCP_OPT_DEFER_ACCEPT}	{ count(); yylval.strval=yytext;
									return TCP_OPT_DEFER_ACCEPT; }
<INITIAL>{TCP_OPT_DELAYED_ACK}	{ count(); yylval.strval=yytext;
									return TCP_OPT_DELAYED_ACK; }
<INITIAL>{TCP_OPT_SYNCNT}		{ count(); yylval.strval=yytext;
									return TCP_OPT_SYNCNT; }
<INITIAL>{TCP_OPT_LINGER2}		{ count(); yylval.strval=yytext;
									return TCP_OPT_LINGER2; }
<INITIAL>{TCP_OPT_KEEPALIVE}	{ count(); yylval.strval=yytext;
									return TCP_OPT_KEEPALIVE; }
<INITIAL>{TCP_OPT_KEEPIDLE}		{ count(); yylval.strval=yytext;
									return TCP_OPT_KEEPIDLE; }
<INITIAL>{TCP_OPT_KEEPINTVL}	{ count(); yylval.strval=yytext;
									return TCP_OPT_KEEPINTVL; }
<INITIAL>{TCP_OPT_KEEPCNT}	{ count(); yylval.strval=yytext;
									return TCP_OPT_KEEPCNT; }
<INITIAL>{TCP_OPT_CRLF_PING}	{ count(); yylval.strval=yytext;
									return TCP_OPT_CRLF_PING; }
<INITIAL>{TCP_OPT_ACCEPT_NO_CL}	{ count(); yylval.strval=yytext;
									return TCP_OPT_ACCEPT_NO_CL; }
<INITIAL>{TCP_CLONE_RCVBUF}		{ count(); yylval.strval=yytext;
									return TCP_CLONE_RCVBUF; }
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
<INITIAL>{DISABLE_SCTP}	{ count(); yylval.strval=yytext; return DISABLE_SCTP;}
<INITIAL>{ENABLE_SCTP}	{ count(); yylval.strval=yytext; return ENABLE_SCTP;}
<INITIAL>{SCTP_CHILDREN}	{ count(); yylval.strval=yytext;
										return SCTP_CHILDREN; }
<INITIAL>{SERVER_SIGNATURE}	{ count(); yylval.strval=yytext; return SERVER_SIGNATURE; }
<INITIAL>{SERVER_HEADER}	{ count(); yylval.strval=yytext; return SERVER_HEADER; }
<INITIAL>{USER_AGENT_HEADER}	{ count(); yylval.strval=yytext; return USER_AGENT_HEADER; }
<INITIAL>{REPLY_TO_VIA}	{ count(); yylval.strval=yytext; return REPLY_TO_VIA; }
<INITIAL>{ADVERTISED_ADDRESS}	{	count(); yylval.strval=yytext;
									return ADVERTISED_ADDRESS; }
<INITIAL>{ADVERTISED_PORT}		{	count(); yylval.strval=yytext;
									return ADVERTISED_PORT; }
<INITIAL>{DISABLE_CORE}		{	count(); yylval.strval=yytext;
									return DISABLE_CORE; }
<INITIAL>{OPEN_FD_LIMIT}		{	count(); yylval.strval=yytext;
									return OPEN_FD_LIMIT; }
<INITIAL>{SHM_MEM_SZ}		{	count(); yylval.strval=yytext;
									return SHM_MEM_SZ; }
<INITIAL>{SHM_FORCE_ALLOC}		{	count(); yylval.strval=yytext;
									return SHM_FORCE_ALLOC; }
<INITIAL>{MLOCK_PAGES}		{	count(); yylval.strval=yytext;
									return MLOCK_PAGES; }
<INITIAL>{REAL_TIME}		{	count(); yylval.strval=yytext;
									return REAL_TIME; }
<INITIAL>{RT_PRIO}		{	count(); yylval.strval=yytext;
									return RT_PRIO; }
<INITIAL>{RT_POLICY}		{	count(); yylval.strval=yytext;
									return RT_POLICY; }
<INITIAL>{RT_TIMER1_PRIO}		{	count(); yylval.strval=yytext;
									return RT_TIMER1_PRIO; }
<INITIAL>{RT_TIMER1_POLICY}		{	count(); yylval.strval=yytext;
									return RT_TIMER1_POLICY; }
<INITIAL>{RT_TIMER2_PRIO}		{	count(); yylval.strval=yytext;
									return RT_TIMER2_PRIO; }
<INITIAL>{RT_TIMER2_POLICY}		{	count(); yylval.strval=yytext;
									return RT_TIMER2_POLICY; }
<INITIAL>{MCAST_LOOPBACK}		{	count(); yylval.strval=yytext;
									return MCAST_LOOPBACK; }
<INITIAL>{MCAST_TTL}		{	count(); yylval.strval=yytext;
									return MCAST_TTL; }
<INITIAL>{TOS}			{	count(); yylval.strval=yytext;
									return TOS; }
<INITIAL>{PMTU_DISCOVERY}		{	count(); yylval.strval=yytext;
									return PMTU_DISCOVERY; }
<INITIAL>{KILL_TIMEOUT}			{	count(); yylval.strval=yytext;
									return KILL_TIMEOUT; }
<INITIAL>{MAX_WLOOPS}			{	count(); yylval.strval=yytext;
									return MAX_WLOOPS; }
<INITIAL>{PVBUFSIZE}			{	count(); yylval.strval=yytext;
									return PVBUFSIZE; }
<INITIAL>{PVBUFSLOTS}			{	count(); yylval.strval=yytext;
									return PVBUFSLOTS; }
<INITIAL>{HTTP_REPLY_PARSE}		{	count(); yylval.strval=yytext;
									return HTTP_REPLY_PARSE; }
<INITIAL>{VERSION_TABLE_CFG}  { count(); yylval.strval=yytext; return VERSION_TABLE_CFG;}
<INITIAL>{SERVER_ID}  { count(); yylval.strval=yytext; return SERVER_ID;}
<INITIAL>{MAX_RECURSIVE_LEVEL}  { count(); yylval.strval=yytext; return MAX_RECURSIVE_LEVEL;}
<INITIAL>{MAX_BRANCHES_PARAM}  { count(); yylval.strval=yytext; return MAX_BRANCHES_PARAM;}
<INITIAL>{LATENCY_LOG}  { count(); yylval.strval=yytext; return LATENCY_LOG;}
<INITIAL>{MSG_TIME}  { count(); yylval.strval=yytext; return MSG_TIME;}
<INITIAL>{ONSEND_RT_REPLY}	{ count(); yylval.strval=yytext; return ONSEND_RT_REPLY; }
<INITIAL>{LATENCY_LIMIT_DB}  { count(); yylval.strval=yytext; return LATENCY_LIMIT_DB;}
<INITIAL>{LATENCY_LIMIT_ACTION}  { count(); yylval.strval=yytext; return LATENCY_LIMIT_ACTION;}
<INITIAL>{CFG_DESCRIPTION}	{ count(); yylval.strval=yytext; return CFG_DESCRIPTION; }
<INITIAL>{LOADMODULE}	{ count(); yylval.strval=yytext; return LOADMODULE; }
<INITIAL>{LOADPATH}		{ count(); yylval.strval=yytext; return LOADPATH; }
<INITIAL>{MODPARAM}     { count(); yylval.strval=yytext; return MODPARAM; }

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
<INITIAL>{BIN_NOT}	{ count(); return BIN_NOT;  }
<INITIAL>{BIN_XOR}	{ count(); return BIN_XOR;  }
<INITIAL>{BIN_LSHIFT}	{ count(); return BIN_LSHIFT;  }
<INITIAL>{BIN_RSHIFT}	{ count(); return BIN_RSHIFT;  }
<INITIAL>{PLUS}		{ count(); return PLUS; }
<INITIAL>{MINUS}	{ count(); return MINUS; }
<INITIAL>{MODULO}	{ count(); return MODULO; }
<INITIAL>{STRLEN}	{ count(); return STRLEN; }
<INITIAL>{STREMPTY}	{ count(); return STREMPTY; }
<INITIAL>{DEFINED}	{ count(); return DEFINED; }
<INITIAL>{STREQ}	{ count(); return STREQ; }
<INITIAL>{INTEQ}	{ count(); return INTEQ; }
<INITIAL>{STRDIFF}	{ count(); return STRDIFF; }
<INITIAL>{INTDIFF}	{ count(); return INTDIFF; }
<INITIAL>{INTCAST}	{ count(); return INTCAST; }
<INITIAL>{STRCAST}	{ count(); return STRCAST; }

<INITIAL>{SELECT_MARK}  { count(); state = SELECT_S; BEGIN(SELECT); return SELECT_MARK; }
<SELECT>{ID}		{ count(); addstr(&s_buf, yytext, yyleng);
                          yylval.strval=s_buf.s;
                          memset(&s_buf, 0, sizeof(s_buf));
                          return ID;
                        }
<SELECT>{DOT}           { count(); return DOT; }
<SELECT>{LBRACK}        { count(); return LBRACK; }
<SELECT>{RBRACK}        { count(); return RBRACK; }
<SELECT>{DECNUMBER}	{ count(); yylval.intval=atoi(yytext);
						yy_number_str=yytext; return NUMBER; }
<SELECT>{HEXNUMBER}	{ count(); yylval.intval=(int)strtol(yytext, 0, 16);
						yy_number_str=yytext; return NUMBER; }
<SELECT>{OCTNUMBER}	{ count(); yylval.intval=(int)strtol(yytext, 0, 8);
						yy_number_str=yytext; return NUMBER; }
<SELECT>{BINNUMBER}	{ count(); yylval.intval=(int)strtol(yytext, 0, 2);
						yy_number_str=yytext; return NUMBER; }


<INITIAL>{ATTR_MARK}    { count(); state = ATTR_S; BEGIN(ATTR);
							return ATTR_MARK; }
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
<ATTR>{STAR}			{ count(); return STAR; }
<ATTR>{DECNUMBER}		{ count(); yylval.intval=atoi(yytext);
							yy_number_str=yytext; return NUMBER; }
<ATTR>{ID}				{ count(); addstr(&s_buf, yytext, yyleng);
							yylval.strval=s_buf.s;
							memset(&s_buf, 0, sizeof(s_buf));
							state = INITIAL_S;
							BEGIN(INITIAL);
							return ID;
						}

<INITIAL>{VAR_MARK}{LPAREN}	{
								switch(sr_cfg_compat){
									case SR_COMPAT_SER:
										state=ATTR_S; BEGIN(ATTR);
										yyless(1);
										count();
										return ATTR_MARK;
										break;
									case SR_COMPAT_KAMAILIO:
									case SR_COMPAT_MAX:
									default:
										state = PVAR_P_S; BEGIN(PVAR_P);
										p_nest=1; yymore();
										break;
								}
							}
	/* eat everything between 2 () and return PVAR token and a string
	   containing everything (including $ and ()) */
<PVAR_P>{RPAREN}			{	p_nest--;
								if (p_nest==0){
									count();
									addstr(&s_buf, yytext, yyleng);
									r = pp_subst_run(&s_buf.s);
									yylval.strval=s_buf.s;
									memset(&s_buf, 0, sizeof(s_buf));
									state=INITIAL_S;
									BEGIN(INITIAL);
									return PVAR;
								}
								yymore();
							}
<PVAR_P>{LPAREN}			{ p_nest++; yymore(); }
<PVAR_P>.					{ yymore(); }

<PVARID>{ID}|'\.'			{yymore(); }
<PVARID>{LPAREN}			{	state = PVAR_P_S; BEGIN(PVAR_P);
								p_nest=1; yymore(); }
<PVARID>{CR}|{EAT_ABLE}|.	{	yyless(yyleng-1);
								count();
								addstr(&s_buf, yytext, yyleng);
								r = pp_subst_run(&s_buf.s);
								yylval.strval=s_buf.s;
								memset(&s_buf, 0, sizeof(s_buf));
								state=INITIAL_S;
								BEGIN(INITIAL);
								return PVAR;
							}

	/* if found retcode => it's a built-in pvar */
<INITIAL>{RETCODE}			{ count(); yylval.strval=yytext; return PVAR; }

<INITIAL>{VAR_MARK}			{
								switch(sr_cfg_compat){
									case SR_COMPAT_SER:
										count();
										state=ATTR_S; BEGIN(ATTR);
										return ATTR_MARK;
										break;
									case SR_COMPAT_KAMAILIO:
										state=PVARID_S; BEGIN(PVARID);
										yymore();
										break;
									case SR_COMPAT_MAX:
									default: 
										state=AVP_PVAR_S; BEGIN(AVP_PVAR);
										yymore();
										break;
								}
							}
	/* avp prefix detected -> go to avp mode */
<AVP_PVAR>{AVP_PREF}		|
<AVP_PVAR>{ID}{LBRACK}		{ state = ATTR_S; BEGIN(ATTR); yyless(1); count();
							  return ATTR_MARK; }
<AVP_PVAR>{ID}{LPAREN}		{ state = PVAR_P_S; p_nest=1; BEGIN(PVAR_P);
								yymore(); }
<AVP_PVAR>{ID}				{	count(); addstr(&s_buf, yytext, yyleng);
								yylval.strval=s_buf.s;
								memset(&s_buf, 0, sizeof(s_buf));
								state = INITIAL_S;
								BEGIN(INITIAL);
								return AVP_OR_PVAR;
							}

<INITIAL>{IPV6ADDR}		{ count(); yylval.strval=yytext; return IPV6ADDR; }
<INITIAL>{DECNUMBER}	{ count(); yylval.intval=atoi(yytext);
								yy_number_str=yytext; return NUMBER; }
<INITIAL>{HEXNUMBER}	{ count(); yylval.intval=(int)strtol(yytext, 0, 16);
							yy_number_str=yytext; return NUMBER; }
<INITIAL>{OCTNUMBER}	{ count(); yylval.intval=(int)strtol(yytext, 0, 8);
							yy_number_str=yytext; return NUMBER; }
<INITIAL>{BINNUMBER}    { count(); yylval.intval=(int)strtol(yytext, 0, 2);
							yy_number_str=yytext; return NUMBER; }
<INITIAL>{YES}			{ count(); yylval.intval=1;
							yy_number_str=yytext; return NUMBER; }
<INITIAL>{NO}			{ count(); yylval.intval=0;
							yy_number_str=yytext; return NUMBER; }
<INITIAL>{TCP}			{ count(); return TCP; }
<INITIAL>{UDP}			{ count(); return UDP; }
<INITIAL>{TLS}			{ count(); return TLS; }
<INITIAL>{SCTP}			{ count(); return SCTP; }
<INITIAL>{WS}			{ count(); return WS; }
<INITIAL>{WSS}			{ count(); return WSS; }
<INITIAL>{INET}			{ count(); yylval.intval=AF_INET;
							yy_number_str=yytext; return NUMBER; }
<INITIAL>{INET6}		{ count();
						yylval.intval=AF_INET6;
						yy_number_str=yytext;
						return NUMBER; }
<INITIAL>{SSLv23}		{ count(); yylval.strval=yytext; return SSLv23; }
<INITIAL>{SSLv2}		{ count(); yylval.strval=yytext; return SSLv2; }
<INITIAL>{SSLv3}		{ count(); yylval.strval=yytext; return SSLv3; }
<INITIAL>{TLSv1}		{ count(); yylval.strval=yytext; return TLSv1; }

<INITIAL>{COMMA}		{ count(); return COMMA; }
<INITIAL>{SEMICOLON}	{ count(); return SEMICOLON; }
<INITIAL>{COLON}	{ count(); return COLON; }
<INITIAL>{STAR}		{ count(); return STAR; }
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


<INITIAL,SELECT>{QUOTES} { count(); old_initial = YY_START; 
							old_state = state; state=STRING_S;
							BEGIN(STRING1); }
<INITIAL>{TICK} { count(); old_initial = YY_START; old_state = state;
					state=STRING_S; BEGIN(STRING2); }


<STRING1>{QUOTES} { count_more(); 
						yytext[yyleng-1]=0; yyleng--;
						addstr(&s_buf, yytext, yyleng);
						state=STR_BETWEEN_S;
						BEGIN(STR_BETWEEN);
					}
<STRING2>{TICK}  { count_more(); state=old_state; BEGIN(old_initial);
						yytext[yyleng-1]=0; yyleng--;
						addstr(&s_buf, yytext, yyleng);
						r = pp_subst_run(&s_buf.s);
						yylval.strval=s_buf.s;
						memset(&s_buf, 0, sizeof(s_buf));
						return STRING;
					}
<STRING2>.|{EAT_ABLE}|{CR}	{ yymore(); }

<STRING1>\\n		{ count_more(); addchar(&s_buf, '\n'); }
<STRING1>\\r		{ count_more(); addchar(&s_buf, '\r'); }
<STRING1>\\a		{ count_more(); addchar(&s_buf, '\a'); }
<STRING1>\\t		{ count_more(); addchar(&s_buf, '\t'); }
<STRING1>\\{QUOTES}	{ count_more(); addchar(&s_buf, '"');  }
<STRING1>\\\\		{ count_more(); addchar(&s_buf, '\\'); }
<STRING1>\\x{HEX}{1,2}	{ count_more(); addchar(&s_buf,
											(char)strtol(yytext+2, 0, 16)); }
 /* don't allow \[0-7]{1}, it will eat the backreferences from
    subst_uri if allowed (although everybody should use '' in subt_uri) */
<STRING1>\\[0-7]{2,3}	{ count_more(); addchar(&s_buf,
											(char)strtol(yytext+1, 0, 8));  }
<STRING1>\\{CR}		{ count_more(); } /* eat escaped CRs */
<STRING1>.|{EAT_ABLE}|{CR}	{ count_more(); addchar(&s_buf, *yytext); }

<STR_BETWEEN>{EAT_ABLE}|{CR}	{ count_ignore(); }
<STR_BETWEEN>{QUOTES}			{ count_more(); state=STRING_S;
								  BEGIN(STRING1);}
<STR_BETWEEN>.					{	
									yyless(0); /* reparse it */
									/* ignore the whitespace now that is
									  counted, return saved string value */
									state=old_state; BEGIN(old_initial);
									r = pp_subst_run(&s_buf.s);
									yylval.strval=s_buf.s;
									memset(&s_buf, 0, sizeof(s_buf));
									return STRING;
								}

<INITIAL,COMMENT>{COM_START}	{ count(); comment_nest++; state=COMMENT_S;
										BEGIN(COMMENT); }
<COMMENT>{COM_END}				{ count(); comment_nest--;
										if (comment_nest==0){
											state=INITIAL_S;
											BEGIN(INITIAL);
										}
								}
<COMMENT>.|{EAT_ABLE}|{CR}				{ count(); };

<INITIAL>{COM_LINE}!{SER_CFG}{CR}		{ count();
											sr_cfg_compat=SR_COMPAT_SER;}
<INITIAL>{COM_LINE}!{KAMAILIO_CFG}{CR}	{ count(); 
											sr_cfg_compat=SR_COMPAT_KAMAILIO;}
<INITIAL>{COM_LINE}!{MAXCOMPAT_CFG}{CR}	{ count(); 
												sr_cfg_compat=SR_COMPAT_MAX;}

<INITIAL>{PREP_START}{DEFINE}{EAT_ABLE}+	{	count(); pp_define_set_type(0);
											state = DEFINE_S; BEGIN(DEFINE_ID); }
<INITIAL>{PREP_START}{TRYDEF}{EAT_ABLE}+	{	count(); pp_define_set_type(1);
											state = DEFINE_S; BEGIN(DEFINE_ID); }
<INITIAL>{PREP_START}{REDEF}{EAT_ABLE}+	{	count(); pp_define_set_type(2);
											state = DEFINE_S; BEGIN(DEFINE_ID); }
<DEFINE_ID>{ID}                 {	count();
									if (pp_define(yyleng, yytext)) return 1;
									state = DEFINE_EOL_S; BEGIN(DEFINE_EOL); }
<DEFINE_EOL>{EAT_ABLE}			{	count(); }
<DEFINE_EOL>{CR}				{	count();
									state = INITIAL; BEGIN(INITIAL); }
<DEFINE_EOL>.                   {	count();
									addstr(&s_buf, yytext, yyleng);
									state = DEFINE_DATA_S; BEGIN(DEFINE_DATA); }
<DEFINE_DATA>\\{CR}		{	count(); } /* eat the escaped CR */
<DEFINE_DATA>{CR}		{	count();
							if (pp_define_set(strlen(s_buf.s), s_buf.s)) return 1;
							memset(&s_buf, 0, sizeof(s_buf));
							state = INITIAL; BEGIN(INITIAL); }
<DEFINE_DATA>.          {	count();
							addstr(&s_buf, yytext, yyleng); }

<INITIAL>{PREP_START}{SUBST}	{ count();  return SUBST;}
<INITIAL>{PREP_START}{SUBSTDEF}	{ count();  return SUBSTDEF;}
<INITIAL>{PREP_START}{SUBSTDEFS}	{ count();  return SUBSTDEFS;}

<INITIAL,IFDEF_SKIP>{PREP_START}{IFDEF}{EAT_ABLE}+    { count();
								if (pp_ifdef_type(1)) return 1;
								state = IFDEF_S; BEGIN(IFDEF_ID); }
<INITIAL,IFDEF_SKIP>{PREP_START}{IFNDEF}{EAT_ABLE}+    { count();
								if (pp_ifdef_type(0)) return 1;
								state = IFDEF_S; BEGIN(IFDEF_ID); }
<IFDEF_ID>{ID}                { count();
                                pp_ifdef_var(yyleng, yytext);
                                state = IFDEF_EOL_S; BEGIN(IFDEF_EOL); }
<IFDEF_EOL>{EAT_ABLE}*{CR}    { count(); pp_ifdef(); }

<INITIAL,IFDEF_SKIP>{PREP_START}{ELSE}{EAT_ABLE}*{CR}    { count(); pp_else(); }

<INITIAL,IFDEF_SKIP>{PREP_START}{ENDIF}{EAT_ABLE}*{CR}    { count();
															pp_endif(); }

 /* we're in an ifdef that evaluated to false -- throw it away */
<IFDEF_SKIP>.|{CR}    { count(); }

 /* this is split so the shebangs match more, giving them priority */
<INITIAL>{COM_LINE}        { count(); state = LINECOMMENT_S;
								BEGIN(LINECOMMENT); }
<LINECOMMENT>.*{CR}        { count(); state = INITIAL_S; BEGIN(INITIAL); }

<INITIAL>{ID}		{	if ((sdef = pp_define_get(yyleng, yytext))!=NULL) {
							for (r=sdef->len-1; r>=0; r--)
								unput(sdef->s[r]); /* reverse order */
						} else {
							count();
							addstr(&s_buf, yytext, yyleng);
							yylval.strval=s_buf.s;
							memset(&s_buf, 0, sizeof(s_buf));
							return ID;
						}
					}
<INITIAL>{NUM_ID}			{ count(); addstr(&s_buf, yytext, yyleng);
									yylval.strval=s_buf.s;
									memset(&s_buf, 0, sizeof(s_buf));
									return NUM_ID; }

<SELECT>.               { unput(yytext[0]); state = INITIAL_S; BEGIN(INITIAL); } /* Rescan the token in INITIAL state */

<INCLF>[ \t]*      /* eat the whitespace */
<INCLF>[^ \t\r\n]+   { /* get the include file name */
				memset(&s_buf, 0, sizeof(s_buf));
				addstr(&s_buf, yytext, yyleng);
				r = pp_subst_run(&s_buf.s);
				if(sr_push_yy_state(s_buf.s, 0)<0)
				{
					LOG(L_CRIT, "error at %s line %d\n", (finame)?finame:"cfg", line);
					exit(-1);
				}
				memset(&s_buf, 0, sizeof(s_buf));
				BEGIN(INITIAL);
}

<IMPTF>[ \t]*      /* eat the whitespace */
<IMPTF>[^ \t\r\n]+   { /* get the import file name */
				memset(&s_buf, 0, sizeof(s_buf));
				addstr(&s_buf, yytext, yyleng);
				r = pp_subst_run(&s_buf.s);
				if(sr_push_yy_state(s_buf.s, 1)<0)
				{
					LOG(L_CRIT, "error at %s line %d\n", (finame)?finame:"cfg", line);
					exit(-1);
				}
				memset(&s_buf, 0, sizeof(s_buf));
				BEGIN(INITIAL);
}


<<EOF>>							{
									switch(state){
										case STR_BETWEEN_S:
											state=old_state;
											BEGIN(old_initial);
											r = pp_subst_run(&s_buf.s);
											yylval.strval=s_buf.s;
											memset(&s_buf, 0, sizeof(s_buf));
											return STRING;
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
										case  ATTR_S:
											LOG(L_CRIT, "ERROR: unexpected EOF"
													" while parsing"
													" avp name\n");
											break;
										case PVARID_S:
											p_nest=0;
										case PVAR_P_S: 
											LOG(L_CRIT, "ERROR: unexpected EOF"
													" while parsing pvar name"
													" (%d paranthesis open)\n",
													p_nest);
											break;
										case AVP_PVAR_S:
											LOG(L_CRIT, "ERROR: unexpected EOF"
													" while parsing"
													" avp or pvar name\n");
									}
									if(sr_pop_yy_state()<0)
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
	LOG(L_CRIT, "ERROR:lex:addstr: try to increase pkg size with"
					" -M parameter\n");
	exit(-1);
}



/** helper function for count_*(). */
static void count_lc(int* l, int* c)
{
	int i;
	for (i=0; i<yyleng;i++){
		if (yytext[i]=='\n'){
			(*l)++;
			(*c)=1;
		}else if (yytext[i]=='\t'){
			(*c)++;
			/*(*c)+=8 -((*c)%8);*/
		}else{
			(*c)++;
		}
	}
}



/* helper function */
static void count_restore_ignored()
{
	if (ign_lines) /* ignored line(s) => column has changed */
		column=ign_columns;
	else
		column+=ign_columns;
	line+=ign_lines;
	ign_lines=ign_columns=0;
}



/** count/record position for stuff added to the current token. */
static void count_more()
{
	count_restore_ignored();
	count_lc(&line, &column);
}



/** count/record position for a new token. */
static void count()
{
	count_restore_ignored();
	startline=line;
	startcolumn=column;
	count_more();
}



/** record discarded stuff (not contained in the token) so that
    the next token position can be adjusted properly*/
static void count_ignore()
{
	count_lc(&ign_lines, &ign_columns);
}


/* replacement yywrap, removes libfl dependency */
int yywrap()
{
	return 1;
}

static int sr_push_yy_state(char *fin, int mode)
{
	struct sr_yy_fname *fn = NULL;
	FILE *fp = NULL;
	char *x = NULL;
	char *newf = NULL;
#define MAX_INCLUDE_FNAME	128
	char fbuf[MAX_INCLUDE_FNAME];
	int i, j, l;
	char *tmpfiname = 0;

	if ( include_stack_ptr >= MAX_INCLUDE_DEPTH )
	{
		LOG(L_CRIT, "too many includes\n");
		return -1;
	}
	l = strlen(fin);
	if(l>=MAX_INCLUDE_FNAME)
	{
		LOG(L_CRIT, "included file name too long: %s\n", fin);
		return -1;
	}
	if(fin[0]!='"' || fin[l-1]!='"')
	{
		LOG(L_CRIT, "included file name must be between quotes: %s\n", fin);
		return -1;
	}
	j = 0;
	for(i=1; i<l-1; i++)
	{
		switch(fin[i]) {
			case '\\':
				if(i+1==l-1)
				{
					LOG(L_CRIT, "invalid escape at %d in included file name: %s\n", i, fin);
					return -1;
				}
				i++;
				switch(fin[i]) {
					case 't':
						fbuf[j++] = '\t';
					break;
					case 'n':
						fbuf[j++] = '\n';
					break;
					case 'r':
						fbuf[j++] = '\r';
					break;
					default:
						fbuf[j++] = fin[i];
				}
			break;
			default:
				fbuf[j++] = fin[i];
		}
	}
	if(j==0)
	{
		LOG(L_CRIT, "invalid included file name: %s\n", fin);
		return -1;
	}
	fbuf[j] = '\0';

	fp = fopen(fbuf, "r" );

	if ( ! fp )
	{
		tmpfiname = (finame==0)?cfg_file:finame;
		if(tmpfiname==0 || fbuf[0]=='/')
		{
			if(mode==0)
			{
				LOG(L_CRIT, "cannot open included file: %s\n", fin);
				return -1;
			} else {
				LOG(L_DBG, "importing file ignored: %s\n", fin);
				return 0;
			}
		}
		x = strrchr(tmpfiname, '/');
		if(x==NULL)
		{
			/* nothing else to try */
			if(mode==0)
			{
				LOG(L_CRIT, "cannot open included file: %s\n", fin);
				return -1;
			} else {
				LOG(L_DBG, "importing file ignored: %s\n", fin);
				return 0;
			}
		}

		newf = (char*)pkg_malloc(x-tmpfiname+strlen(fbuf)+2);
		if(newf==0)
		{
			LOG(L_CRIT, "no more pkg\n");
			return -1;
		}
		newf[0] = '\0';
		strncat(newf, tmpfiname, x-tmpfiname);
		strcat(newf, "/");
		strcat(newf, fbuf);

		fp = fopen(newf, "r" );
		if ( fp==NULL )
		{
			pkg_free(newf);
			if(mode==0)
			{
				LOG(L_CRIT, "cannot open included file: %s (%s)\n", fbuf, newf);
				return -1;
			} else {
				LOG(L_DBG, "importing file ignored: %s (%s)\n", fbuf, newf);
				return 0;
			}
		}
		LOG(L_DBG, "including file: %s (%s)\n", fbuf, newf);
	} else {
		newf = fbuf;
	}

	include_stack[include_stack_ptr].state = YY_CURRENT_BUFFER;
	include_stack[include_stack_ptr].line = line;
	include_stack[include_stack_ptr].column = column;
	include_stack[include_stack_ptr].startline = startline;
	include_stack[include_stack_ptr].startcolumn = startcolumn;
	include_stack[include_stack_ptr].finame = finame;
	include_stack[include_stack_ptr].routename = routename;
	include_stack_ptr++;

	line=1;
	column=1;
	startline=1;
	startcolumn=1;

	yyin = fp;

	/* make a copy in PKG if does not exist */
	fn = sr_yy_fname_list;
	while(fn!=0)
	{
		if(strcmp(fn->fname, newf)==0)
		{
			if(newf!=fbuf)
				pkg_free(newf);
			newf = fbuf;
			break;
		}
		fn = fn->next;
	}
	if(fn==0)
	{
		fn = (struct sr_yy_fname*)pkg_malloc(sizeof(struct sr_yy_fname));
		if(fn==0)
		{
			if(newf!=fbuf)
				pkg_free(newf);
			LOG(L_CRIT, "no more pkg\n");
			return -1;
		}
		if(newf==fbuf)
		{
			fn->fname = (char*)pkg_malloc(strlen(fbuf)+1);
			if(fn->fname==0)
			{
				pkg_free(fn);
				LOG(L_CRIT, "no more pkg!\n");
				return -1;
			}
			strcpy(fn->fname, fbuf);
		} else {
			fn->fname = newf;
		}
		fn->next = sr_yy_fname_list;
		sr_yy_fname_list = fn;
	}

	finame = fn->fname;

	yy_switch_to_buffer( yy_create_buffer(yyin, YY_BUF_SIZE ) );

	return 0;

}

static int sr_pop_yy_state()
{
	include_stack_ptr--;
	if (include_stack_ptr<0 )
		return -1;

	yy_delete_buffer( YY_CURRENT_BUFFER );
	yy_switch_to_buffer(include_stack[include_stack_ptr].state);
	line=include_stack[include_stack_ptr].line;
	column=include_stack[include_stack_ptr].column;
	startline=include_stack[include_stack_ptr].startline;
	startcolumn=include_stack[include_stack_ptr].startcolumn;
	finame = include_stack[include_stack_ptr].finame;
	return 0;
}

/* define/ifdef support */

#define MAX_DEFINES    256
static str pp_defines[MAX_DEFINES][2];
static int pp_num_defines = 0;
static int pp_define_type = 0;
static int pp_define_index = -1;

/* pp_ifdef_stack[i] is 1 if the ifdef test at depth i is either
 * ifdef(defined), ifndef(undefined), or the opposite of these
 * two, but in an else branch
 */
#define MAX_IFDEFS    256
static int pp_ifdef_stack[MAX_IFDEFS];
static int pp_sptr = 0; /* stack pointer */

static int pp_lookup(int len, const char * text)
{
	str var = {(char *)text, len};
	int i;

	for (i=0; i<pp_num_defines; i++)
		if (STR_EQ(pp_defines[i][0], var))
			return i;

	return -1;
}

int pp_define_set_type(int type)
{
	pp_define_type = type;
	return 0;
}

int pp_define(int len, const char * text)
{
	int ppos;

	LM_DBG("defining id: %.*s\n", len, text);

	if (pp_num_defines == MAX_DEFINES) {
		LOG(L_CRIT, "ERROR: too many defines -- adjust MAX_DEFINES\n");
		return -1;
	}

	pp_define_index = -1;
	ppos = pp_lookup(len, text);
	if(ppos >= 0) {
		if(pp_define_type==1) {
			LOG(L_DBG, "ignoring - already defined: %.*s\n", len, text);
			pp_define_index = -2;
			return 0;
		} else if(pp_define_type==2) {
			LOG(L_DBG, "redefining: %.*s\n", len, text);
			pp_define_index = ppos;
			if(pp_defines[ppos][1].s != NULL) {
				pkg_free(pp_defines[ppos][1].s);
				pp_defines[ppos][1].len = 0;
				pp_defines[ppos][1].s = NULL;
			}
			return 0;
		} else {
			LOG(L_CRIT, "ERROR: already defined: %.*s\n", len, text);
			return -1;
		}
	}

	pp_defines[pp_num_defines][0].len = len;
	pp_defines[pp_num_defines][0].s = (char*)pkg_malloc(len+1);
	if(pp_defines[pp_num_defines][0].s==NULL) {
		LOG(L_CRIT, "no more memory to define: %.*s\n", len, text);
		return -1;
	}
	memcpy(pp_defines[pp_num_defines][0].s, text, len);
	pp_defines[pp_num_defines][1].len = 0;
	pp_defines[pp_num_defines][1].s = NULL;
	pp_define_index = pp_num_defines;
	pp_num_defines++;

	return 0;
}

int pp_define_set(int len, char *text)
{
	int ppos;

	if(pp_define_index == -2) {
		/* #!trydef that should be ignored */
		return 0;
	}

	if(pp_define_index < 0) {
		/* invalid position in define table */
		LOG(L_BUG, "BUG: the index in define table not set yet\n");
		return -1;
	}
	if(len<=0) {
		LOG(L_DBG, "no define value - ignoring\n");
		return 0;
	}
	if (pp_num_defines == MAX_DEFINES) {
		LOG(L_CRIT, "ERROR: too many defines -- adjust MAX_DEFINES\n");
		return -1;
	}
	if (pp_num_defines == 0) {
		LOG(L_BUG, "BUG: setting define value, but no define id yet\n");
		return -1;
	}

	ppos = pp_define_index;
	if (pp_defines[ppos][0].s == NULL) {
		LOG(L_BUG, "BUG: last define ID is null\n");
		return -1;
	}

	if (pp_defines[ppos][1].s != NULL) {
		LOG(L_BUG, "BUG: ID %.*s [%d] overwritten\n",
			pp_defines[ppos][0].len,
			pp_defines[ppos][0].s, ppos);
		return -1;
	}

	pp_defines[ppos][1].len = len;
	pp_defines[ppos][1].s = text;
	LM_DBG("### setting define ID [%.*s] value [%.*s]\n",
			pp_defines[ppos][0].len,
			pp_defines[ppos][0].s,
			pp_defines[ppos][1].len,
			pp_defines[ppos][1].s);
	return 0;
}

static str *pp_define_get(int len, const char * text)
{
	str var = {(char *)text, len};
	int i;

	for (i=0; i<pp_num_defines; i++)
	{
		if (STR_EQ(pp_defines[i][0], var))
		{
			if(pp_defines[i][0].s!=NULL)
			{
				LM_DBG("### returning define ID [%.*s] value [%.*s]\n",
					pp_defines[i][0].len,
					pp_defines[i][0].s,
					pp_defines[i][1].len,
					pp_defines[i][1].s);
				return &pp_defines[i][1];
			}
			return NULL;
		}
	}
	return NULL;
}

static int pp_ifdef_type(int type)
{
	if (pp_sptr == MAX_IFDEFS) {
		LOG(L_CRIT, "ERROR: too many nested ifdefs -- adjust MAX_IFDEFS\n");
		return -1;
	}

	pp_ifdef_stack[pp_sptr] = type;
	pp_ifdef_level_update(1);
	return 0;
}

/* this sets the result of the if[n]def expr:
 * ifdef  defined   -> 1
 * ifdef  undefined -> 0
 * ifndef defined   -> 0
 * ifndef undefined -> 1
 */
static void pp_ifdef_var(int len, const char * text)
{
	pp_ifdef_stack[pp_sptr] ^= (pp_lookup(len, text) < 0);
}

static void pp_update_state()
{
	int i;

	for (i=0; i<pp_sptr; i++)
		if (! pp_ifdef_stack[i]) {
			state = IFDEF_SKIP_S; BEGIN(IFDEF_SKIP);
			return;
		}

	state = INITIAL; BEGIN(INITIAL);
}

static void pp_ifdef()
{
	pp_sptr++;
	pp_update_state();
}

static void pp_else()
{
	pp_ifdef_stack[pp_sptr-1] ^= 1;
	pp_update_state();
}

static void pp_endif()
{
	pp_sptr--;
	pp_ifdef_level_update(-1);
	pp_update_state();
}

