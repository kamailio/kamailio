/*
 * $Id$
 *
 *  cfg grammar
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
 * 2003-10-24  converted to the new socket_info lists (andrei)
 * 2003-10-28  added tcp_accept_aliases (andrei)
 * 2003-11-20  added {tcp_connect, tcp_send, tls_*}_timeout (andrei)
 * 2004-03-30  added DISABLE_CORE and OPEN_FD_LIMIT (andrei)
 * 2004-04-29  added SOCK_MODE, SOCK_USER & SOCK_GROUP (andrei)
 * 2004-05-03  applied multicast support patch (MCAST_LOOPBACK) from janakj
 *             added MCAST_TTL (andrei)
 * 2004-07-05  src_ip & dst_ip will detect ip addresses between quotes
 *              (andrei)
 * 2004-10-19  added FROM_URI, TO_URI (andrei)
 * 2004-11-30  added force_send_socket (andrei)
 * 2005-07-08  added TCP_CON_LIFETIME, TCP_POLL_METHOD, TCP_MAX_CONNECTIONS
 *              (andrei)
 * 2005-07-11 added DNS_RETR_TIME, DNS_RETR_NO, DNS_SERVERS_NO, DNS_USE_SEARCH,
 *             DNS_TRY_IPV6 (andrei)
 * 2005-07-12  default onreply route added (andrei)
 * 2005-11-16  fixed if (cond) cmd; (andrei)
 * 2005-12-11  added onsend_route support, fcmd (filtered cmd),
 *             snd_{ip,port,proto,af}, to_{ip,proto} (andrei)
 * 2005-12-19  select framework (mma)
 * 2006-01-06  AVP index support (mma)
 * 2005-01-07  optional semicolon in statement, PARAM_STR&PARAM_STRING
 * 2006-02-02  named flags support (andrei)
 * 2006-02-06  named routes support (andrei)
 * 2006-05-30  avp flags (tma)
 * 2006-09-11  added dns cache (use, flags, ttls, mem ,gc) & dst blacklist
 *              options (andrei)
 * 2006-10-13  added STUN_ALLOW_STUN, STUN_ALLOW_FP, STUN_REFRESH_INTERVAL
 *              (vlada)
 * 2007-02-09  separated command needed for tls-in-core and for tls in general
 *              (andrei)
 * 2007-06-07  added SHM_FORCE_ALLOC, MLOCK_PAGES, REAL_TIME, RT_PRIO,
 *              RT_POLICY, RT_TIMER1_PRIO, RT_TIMER1_POLICY, RT_TIMER2_PRIO,
 *              RT_TIMER2_POLICY (andrei)
 * 2007-06-16  added DDNS_SRV_LB, DNS_TRY_NAPTR (andrei)
 * 2007-09-10  introduced phone2tel option which allows NOT to consider
 *             user=phone URIs as TEL URIs (jiri)
 * 2007-10-10  added DNS_SEARCH_FMATCH (mma)
 * 2007-11-28  added TCP_OPT_{FD_CACHE, DEFER_ACCEPT, DELAYED_ACK, SYNCNT,
 *              LINGER2, KEEPALIVE, KEEPIDLE, KEEPINTVL, KEEPCNT} (andrei)
 * 2008-01-24  added cfg_var definition (Miklos)
 * 2008-11-18  support for variable parameter module functions (andrei)
 * 2007-12-03  support for generalised lvalues and rvalues:
 *               lval=rval_expr, where lval=avp|pvar  (andrei)
 * 2007-12-06  expression are now evaluated in terms of rvalues;
 *             NUMBER is now always positive; cleanup (andrei)
 * 2009-01-26  case/switch() support (andrei)
 * 2009-03-10  added SET_USERPHONE action (Miklos)
 * 2009-05-04  switched if to rval_expr (andrei)
 * 2010-01-10  init shm on first mod_param or route block;
 *             added SHM_MEM_SZ (andrei)
 * 2010-02-17  added blacklist imask (DST_BLST_*_IMASK) support (andrei)
*/

%expect 6

%{

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include "route_struct.h"
#include "globals.h"
#ifdef SHM_MEM
#include "shm_init.h"
#endif /* SHM_MEM */
#include "route.h"
#include "switch.h"
#include "dprint.h"
#include "sr_module.h"
#include "modparam.h"
#include "ip_addr.h"
#include "resolve.h"
#include "socket_info.h"
#include "name_alias.h"
#include "ut.h"
#include "dset.h"
#include "select.h"
#include "flags.h"
#include "tcp_init.h"
#include "tcp_options.h"
#include "sctp_core.h"
#include "pvar.h"
#include "lvalue.h"
#include "rvalue.h"
#include "sr_compat.h"
#include "msg_translator.h"
#include "async_task.h"

#include "ppcfg.h"
#include "pvapi.h"
#include "config.h"
#include "cfg_core.h"
#include "cfg/cfg.h"
#ifdef CORE_TLS
#include "tls/tls_config.h"
#endif
#include "timer_ticks.h"

#ifdef DEBUG_DMALLOC
#include <dmalloc.h>
#endif

/* hack to avoid alloca usage in the generated C file (needed for compiler
 with no built in alloca, like icc*/
#undef _ALLOCA_H

#define onsend_check(s) \
	do{\
		if (rt!=ONSEND_ROUTE) yyerror( s " allowed only in onsend_routes");\
	}while(0)

	#define IF_AUTO_BIND_IPV6(x) x

#ifdef USE_DNS_CACHE
	#define IF_DNS_CACHE(x) x
#else
	#define IF_DNS_CACHE(x) warn("dns cache support not compiled in")
#endif

#ifdef USE_DNS_FAILOVER
	#define IF_DNS_FAILOVER(x) x
#else
	#define IF_DNS_FAILOVER(x) warn("dns failover support not compiled in")
#endif

#ifdef USE_NAPTR
	#define IF_NAPTR(x) x
#else
	#define IF_NAPTR(x) warn("dns naptr support not compiled in")
#endif

#ifdef USE_DST_BLACKLIST
	#define IF_DST_BLACKLIST(x) x
#else
	#define IF_DST_BLACKLIST(x) warn("dst blacklist support not compiled in")
#endif

#ifdef USE_SCTP
	#define IF_SCTP(x) x
#else
	#define IF_SCTP(x) warn("sctp support not compiled in")
#endif

#ifdef USE_RAW_SOCKS
	#define IF_RAW_SOCKS(x) x
#else
	#define IF_RAW_SOCKS(x) warn("raw socket support not compiled in")
#endif


extern int yylex();
/* safer then using yytext which can be array or pointer */
extern char* yy_number_str;

static void yyerror(char* s, ...);
static void yyerror_at(struct cfg_pos* pos, char* s, ...);
static char* tmp;
static int i_tmp;
static unsigned u_tmp;
static struct socket_id* lst_tmp;
static struct name_lst*  nl_tmp;
static int rt;  /* Type of route block for find_export */
static str* str_tmp;
static str s_tmp;
static struct ip_addr* ip_tmp;
static struct avp_spec* s_attr;
static select_t sel;
static select_t* sel_ptr;
static pv_spec_t* pv_spec;
static struct action *mod_func_action;
static struct lvalue* lval_tmp;
static struct rvalue* rval_tmp;

static void warn(char* s, ...);
static void warn_at(struct cfg_pos* pos, char* s, ...);
static void get_cpos(struct cfg_pos* pos);
static struct rval_expr* mk_rve_rval(enum rval_type, void* v);
static struct rval_expr* mk_rve1(enum rval_expr_op op, struct rval_expr* rve1);
static struct rval_expr* mk_rve2(enum rval_expr_op op, struct rval_expr* rve1,
									struct rval_expr* rve2);
static int rval_expr_int_check(struct rval_expr *rve);
static int warn_ct_rve(struct rval_expr *rve, char* name);
static struct socket_id* mk_listen_id(char*, int, int);
static struct name_lst* mk_name_lst(char* name, int flags);
static struct socket_id* mk_listen_id2(struct name_lst*, int, int);
static void free_name_lst(struct name_lst* lst);
static void free_socket_id_lst(struct socket_id* i);

static struct case_stms* mk_case_stm(struct rval_expr* ct, int is_re, 
									struct action* a, int* err);
static int case_check_type(struct case_stms* stms);
static int case_check_default(struct case_stms* stms);
static int mod_f_params_pre_fixup(struct action* a);
static void free_mod_func_action(struct action* a);


extern int line;
extern int column;
extern int startcolumn;
extern int startline;
extern char *finame;
extern char *routename;
extern char *default_routename;

#define set_cfg_pos(x) \
	do{\
		if(x) {\
		(x)->cline = line;\
		(x)->cfile = (finame!=0)?finame:((cfg_file!=0)?cfg_file:"default");\
		(x)->rname = (routename!=0)?routename:((default_routename!=0)?default_routename:"DEFAULT");\
		}\
	}while(0)


%}

%union {
	long intval;
	unsigned long uval;
	char* strval;
	struct expr* expr;
	struct action* action;
	struct case_stms* case_stms;
	struct net* ipnet;
	struct ip_addr* ipaddr;
	struct socket_id* sockid;
	struct name_lst* name_l;
	struct avp_spec* attr;
	struct _pv_spec* pvar;
	struct lvalue* lval;
	struct rvalue* rval;
	struct rval_expr* rv_expr;
	select_t* select;
}

/* terminals */


/* keywords */
%token FORWARD
%token FORWARD_TCP
%token FORWARD_TLS
%token FORWARD_SCTP
%token FORWARD_UDP
%token EXIT
%token DROP
%token RETURN
%token BREAK
%token LOG_TOK
%token ERROR
%token ROUTE
%token ROUTE_REQUEST
%token ROUTE_FAILURE
%token ROUTE_ONREPLY
%token ROUTE_REPLY
%token ROUTE_BRANCH
%token ROUTE_SEND
%token ROUTE_EVENT
%token EXEC
%token SET_HOST
%token SET_HOSTPORT
%token SET_HOSTPORTTRANS
%token PREFIX
%token STRIP
%token STRIP_TAIL
%token SET_USERPHONE
%token APPEND_BRANCH
%token REMOVE_BRANCH
%token CLEAR_BRANCHES
%token SET_USER
%token SET_USERPASS
%token SET_PORT
%token SET_URI
%token REVERT_URI
%token FORCE_RPORT
%token ADD_LOCAL_RPORT
%token FORCE_TCP_ALIAS
%token UDP_MTU
%token UDP_MTU_TRY_PROTO
%token UDP4_RAW
%token UDP4_RAW_MTU
%token UDP4_RAW_TTL
%token IF
%token ELSE
%token SET_ADV_ADDRESS
%token SET_ADV_PORT
%token FORCE_SEND_SOCKET
%token SET_FWD_NO_CONNECT
%token SET_RPL_NO_CONNECT
%token SET_FWD_CLOSE
%token SET_RPL_CLOSE
%token SWITCH
%token CASE
%token DEFAULT
%token WHILE
%token CFG_SELECT
%token CFG_RESET
%token URIHOST
%token URIPORT
%token MAX_LEN
%token SETFLAG
%token RESETFLAG
%token ISFLAGSET
%token SETAVPFLAG
%token RESETAVPFLAG
%token ISAVPFLAGSET
%token METHOD
%token URI
%token FROM_URI
%token TO_URI
%token SRCIP
%token SRCPORT
%token DSTIP
%token DSTPORT
%token TOIP
%token TOPORT
%token SNDIP
%token SNDPORT
%token SNDPROTO
%token SNDAF
%token PROTO
%token AF
%token MYSELF
%token MSGLEN
%token UDP
%token TCP
%token TLS
%token SCTP
%token WS
%token WSS

/* config vars. */
%token DEBUG_V
%token FORK
%token FORK_DELAY
%token MODINIT_DELAY
%token LOGSTDERROR
%token LOGFACILITY
%token LOGNAME
%token LOGCOLOR
%token LOGPREFIX
%token LISTEN
%token ADVERTISE
%token ALIAS
%token SR_AUTO_ALIASES
%token DNS
%token REV_DNS
%token DNS_TRY_IPV6
%token DNS_TRY_NAPTR
%token DNS_SRV_LB
%token DNS_UDP_PREF
%token DNS_TCP_PREF
%token DNS_TLS_PREF
%token DNS_SCTP_PREF
%token DNS_RETR_TIME
%token DNS_RETR_NO
%token DNS_SERVERS_NO
%token DNS_USE_SEARCH
%token DNS_SEARCH_FMATCH
%token DNS_NAPTR_IGNORE_RFC
%token DNS_CACHE_INIT
%token DNS_USE_CACHE
%token DNS_USE_FAILOVER
%token DNS_CACHE_FLAGS
%token DNS_CACHE_NEG_TTL
%token DNS_CACHE_MIN_TTL
%token DNS_CACHE_MAX_TTL
%token DNS_CACHE_MEM
%token DNS_CACHE_GC_INT
%token DNS_CACHE_DEL_NONEXP
%token DNS_CACHE_REC_PREF

/* ipv6 auto bind */
%token AUTO_BIND_IPV6

/*blacklist*/
%token DST_BLST_INIT
%token USE_DST_BLST
%token DST_BLST_MEM
%token DST_BLST_TTL
%token DST_BLST_GC_INT
%token DST_BLST_UDP_IMASK
%token DST_BLST_TCP_IMASK
%token DST_BLST_TLS_IMASK
%token DST_BLST_SCTP_IMASK

%token PORT
%token STAT
%token CHILDREN
%token SOCKET_WORKERS
%token ASYNC_WORKERS
%token CHECK_VIA
%token PHONE2TEL
%token MEMLOG
%token MEMDBG
%token MEMSUM
%token MEMSAFETY
%token MEMJOIN
%token CORELOG
%token SIP_WARNING
%token SERVER_SIGNATURE
%token SERVER_HEADER
%token USER_AGENT_HEADER
%token REPLY_TO_VIA
%token LOADMODULE
%token LOADPATH
%token MODPARAM
%token MAXBUFFER
%token SQL_BUFFER_SIZE
%token USER
%token GROUP
%token CHROOT
%token WDIR
%token RUNDIR
%token MHOMED
%token DISABLE_TCP
%token TCP_ACCEPT_ALIASES
%token TCP_CHILDREN
%token TCP_CONNECT_TIMEOUT
%token TCP_SEND_TIMEOUT
%token TCP_CON_LIFETIME
%token TCP_POLL_METHOD
%token TCP_MAX_CONNECTIONS
%token TLS_MAX_CONNECTIONS
%token TCP_NO_CONNECT
%token TCP_SOURCE_IPV4
%token TCP_SOURCE_IPV6
%token TCP_OPT_FD_CACHE
%token TCP_OPT_BUF_WRITE
%token TCP_OPT_CONN_WQ_MAX
%token TCP_OPT_WQ_MAX
%token TCP_OPT_RD_BUF
%token TCP_OPT_WQ_BLK
%token TCP_OPT_DEFER_ACCEPT
%token TCP_OPT_DELAYED_ACK
%token TCP_OPT_SYNCNT
%token TCP_OPT_LINGER2
%token TCP_OPT_KEEPALIVE
%token TCP_OPT_KEEPIDLE
%token TCP_OPT_KEEPINTVL
%token TCP_OPT_KEEPCNT
%token TCP_OPT_CRLF_PING
%token TCP_OPT_ACCEPT_NO_CL
%token TCP_CLONE_RCVBUF
%token DISABLE_TLS
%token ENABLE_TLS
%token TLSLOG
%token TLS_PORT_NO
%token TLS_METHOD
%token TLS_HANDSHAKE_TIMEOUT
%token TLS_SEND_TIMEOUT
%token SSLv23
%token SSLv2
%token SSLv3
%token TLSv1
%token TLS_VERIFY
%token TLS_REQUIRE_CERTIFICATE
%token TLS_CERTIFICATE
%token TLS_PRIVATE_KEY
%token TLS_CA_LIST
%token DISABLE_SCTP
%token ENABLE_SCTP
%token SCTP_CHILDREN
%token ADVERTISED_ADDRESS
%token ADVERTISED_PORT
%token DISABLE_CORE
%token OPEN_FD_LIMIT
%token SHM_MEM_SZ
%token SHM_FORCE_ALLOC
%token MLOCK_PAGES
%token REAL_TIME
%token RT_PRIO
%token RT_POLICY
%token RT_TIMER1_PRIO
%token RT_TIMER1_POLICY
%token RT_TIMER2_PRIO
%token RT_TIMER2_POLICY
%token MCAST_LOOPBACK
%token MCAST_TTL
%token TOS
%token PMTU_DISCOVERY
%token KILL_TIMEOUT
%token MAX_WLOOPS
%token PVBUFSIZE
%token PVBUFSLOTS
%token HTTP_REPLY_PARSE
%token VERSION_TABLE_CFG
%token CFG_DESCRIPTION
%token SERVER_ID
%token MAX_RECURSIVE_LEVEL
%token MAX_BRANCHES_PARAM
%token LATENCY_LOG
%token LATENCY_LIMIT_DB
%token LATENCY_LIMIT_ACTION
%token MSG_TIME
%token ONSEND_RT_REPLY

%token FLAGS_DECL
%token AVPFLAGS_DECL

%token ATTR_MARK
%token SELECT_MARK
%token ATTR_FROM
%token ATTR_TO
%token ATTR_FROMURI
%token ATTR_TOURI
%token ATTR_FROMUSER
%token ATTR_TOUSER
%token ATTR_FROMDOMAIN
%token ATTR_TODOMAIN
%token ATTR_GLOBAL
%token ADDEQ


/*pre-processor*/
%token SUBST
%token SUBSTDEF
%token SUBSTDEFS

/* operators, C like precedence */
%right EQUAL
%left LOG_OR
%left LOG_AND
%left BIN_OR
%left BIN_AND
%left BIN_XOR
%left BIN_LSHIFT
%left BIN_RSHIFT
%left EQUAL_T DIFF MATCH INTEQ INTDIFF STREQ STRDIFF
%left GT LT GTE LTE
%left PLUS MINUS
%left STAR SLASH MODULO
%right NOT UNARY BIN_NOT
%right DEFINED
%right INTCAST STRCAST
%left DOT

/* no precedence, they use () */
%token STRLEN
%token STREMPTY

/* values */
%token <intval> NUMBER
%token <strval> ID
%token <strval> NUM_ID
%token <strval> STRING
%token <strval> IPV6ADDR
%token <strval> PVAR
/* not clear yet if this is an avp or pvar */
%token <strval> AVP_OR_PVAR
%token <strval> EVENT_RT_NAME

/* other */
%token COMMA
%token SEMICOLON
%token RPAREN
%token LPAREN
%token LBRACE
%token RBRACE
%token LBRACK
%token RBRACK
%token CR
%token COLON


/*non-terminals */
/*%type <expr> exp */
%type <expr> exp_elem
%type <intval> intno eint_op eint_op_onsend
%type <intval> eip_op eip_op_onsend
%type <action> action actions cmd fcmd if_cmd stm /*exp_stm*/ assign_action
%type <action> switch_cmd while_cmd ret_cmd
%type <case_stms> single_case case_stms
%type <ipaddr> ipv4 ipv6 ipv6addr ip
%type <ipnet> ipnet
%type <strval> host host_or_if host_if_id
%type <strval> listen_id
%type <name_l> listen_id_lst
%type <name_l> listen_id2
%type <sockid>  id_lst
%type <sockid>  phostport
%type <sockid>  listen_phostport
%type <intval> proto eqproto port
%type <intval> equalop strop cmpop rve_cmpop rve_equalop
%type <intval> uri_type
%type <attr> attr_id
%type <attr> attr_id_num_idx
%type <attr> attr_id_no_idx
%type <attr> attr_id_ass
/*%type <attr> attr_id_val*/
%type <attr> attr_id_any
%type <attr> attr_id_any_str
%type <pvar> pvar
%type <lval> lval
%type <rv_expr> rval rval_expr ct_rval
%type <lval> avp_pvar
/* %type <intval> class_id */
%type <intval> assign_op
%type <select> select_id
%type <strval>	flag_name;
%type <strval>	route_name;
%type <intval> avpflag_oper
%type <intval> rve_un_op
%type <strval> cfg_var_id
/* %type <intval> rve_op */

/*%type <route_el> rules;
  %type <route_el> rule;
*/

%%


cfg:
	statements
	;
statements:
	statements statement {}
	| statement {}
	| statements error { yyerror(""); YYABORT;}
	;
statement:
	assign_stm
	| preprocess_stm
	| flags_decl
	| avpflags_decl
	| module_stm
	| {rt=REQUEST_ROUTE;} route_stm
	| {rt=FAILURE_ROUTE;} failure_route_stm
	| onreply_route_stm
	| {rt=BRANCH_ROUTE;} branch_route_stm
	| {rt=ONSEND_ROUTE;}   send_route_stm
	| {rt=EVENT_ROUTE;}   event_route_stm
	| SEMICOLON	/* null statement */
	| CR	/* null statement*/
	;
listen_id:
	ip {
		if ($1){
			tmp=ip_addr2a($1);
			if (tmp==0) {
				LOG(L_CRIT, "ERROR: cfg. parser: bad ip "
						"address.\n");
				$$=0;
			} else {
				$$=pkg_malloc(strlen(tmp)+1);
				if ($$==0) {
					LOG(L_CRIT, "ERROR: cfg. parser: out of "
							"memory.\n");
				} else {
					strncpy($$, tmp, strlen(tmp)+1);
				}
			}
		}
	}
	| STRING {
		$$=pkg_malloc(strlen($1)+1);
		if ($$==0) {
				LOG(L_CRIT, "ERROR: cfg. parser: out of "
						"memory.\n");
		} else {
				strncpy($$, $1, strlen($1)+1);
		}
	}
	| host_or_if {
		if ($1){
			$$=pkg_malloc(strlen($1)+1);
			if ($$==0) {
					LOG(L_CRIT, "ERROR: cfg. parser: out of "
							"memory.\n");
			} else {
					strncpy($$, $1, strlen($1)+1);
			}
		}
	}
	;


listen_id_lst:
	listen_id	{ $$=mk_name_lst($1, SI_IS_MHOMED); }
	| listen_id COMMA listen_id_lst	{ $$=mk_name_lst($1, SI_IS_MHOMED); 
										if ($$) $$->next=$3;
									}
	;

listen_id2:
	LPAREN listen_id_lst RPAREN { $$=$2; }
	| listen_id	{ $$=mk_name_lst($1, 0); }
	;

proto:
	UDP	{ $$=PROTO_UDP; }
	| TCP	{ $$=PROTO_TCP; }
	| TLS	{ $$=PROTO_TLS; }
	| SCTP	{ $$=PROTO_SCTP; }
	| STAR	{ $$=0; }
	;
eqproto:
	UDP	{ $$=PROTO_UDP; }
	| TCP	{ $$=PROTO_TCP; }
	| TLS	{ $$=PROTO_TLS; }
	| SCTP	{ $$=PROTO_SCTP; }
	| WS	{ $$=PROTO_WS; }
	| WSS	{ $$=PROTO_WSS; }
	| STAR	{ $$=0; }
	;
port:
	NUMBER	{ $$=$1; }
	| STAR	{ $$=0; }
;
phostport:
	listen_id		{ $$=mk_listen_id($1, 0, 0); }
	| listen_id COLON port	{ $$=mk_listen_id($1, 0, $3); }
	| proto COLON listen_id	{ $$=mk_listen_id($3, $1, 0); }
	| proto COLON listen_id COLON port	{ $$=mk_listen_id($3, $1, $5);}
	| listen_id COLON error { $$=0; yyerror(" port number expected"); }
	;

listen_phostport:
	listen_id2		{ $$=mk_listen_id2($1, 0, 0); }
	| listen_id2 COLON port	{ $$=mk_listen_id2($1, 0, $3); }
	| proto COLON listen_id2	{ $$=mk_listen_id2($3, $1, 0); }
	| proto COLON listen_id2 COLON port	{ $$=mk_listen_id2($3, $1, $5);}
	| listen_id2 COLON error { $$=0; yyerror(" port number expected"); }
	;

id_lst:
	listen_phostport		{  $$=$1 ; }
	| listen_phostport id_lst	{ $$=$1;  if ($$) $$->next=$2; }
	;

intno: NUMBER
	|  MINUS NUMBER %prec UNARY { $$=-$2; }
	;

flags_decl:		FLAGS_DECL	flag_list
			|	FLAGS_DECL error { yyerror("flag list expected\n"); }
;
flag_list:		flag_spec
			|	flag_spec COMMA flag_list
;

flag_spec:		flag_name	{ if (register_flag($1,-1)<0)
								yyerror("register flag failed");
						}
			|	flag_name COLON NUMBER {
						if (register_flag($1, $3)<0)
								yyerror("register flag failed");
										}
;

flag_name:		STRING	{ $$=$1; }
			|	ID		{ $$=$1; }
;

avpflags_decl:
	AVPFLAGS_DECL avpflag_list
	| AVPFLAGS_DECL error { yyerror("avpflag list expected\n"); }
	;
avpflag_list:
	avpflag_spec
	| avpflag_spec COMMA avpflag_list
	;
avpflag_spec:
	flag_name {
		if (register_avpflag($1)==0)
			yyerror("cannot declare avpflag");
	}
	;
assign_stm:
	DEBUG_V EQUAL intno { default_core_cfg.debug=$3; }
	| DEBUG_V EQUAL error  { yyerror("number  expected"); }
	| FORK  EQUAL NUMBER { dont_fork= ! $3; }
	| FORK  EQUAL error  { yyerror("boolean value expected"); }
	| FORK_DELAY  EQUAL NUMBER { set_fork_delay($3); }
	| FORK_DELAY  EQUAL error  { yyerror("number expected"); }
	| MODINIT_DELAY  EQUAL NUMBER { set_modinit_delay($3); }
	| MODINIT_DELAY  EQUAL error  { yyerror("number expected"); }
	| LOGSTDERROR EQUAL NUMBER { if (!config_check)  /* if set from cmd line, don't overwrite from yyparse()*/ 
					if(log_stderr == 0) log_stderr=$3; 
				   }
	| LOGSTDERROR EQUAL error { yyerror("boolean value expected"); }
	| LOGFACILITY EQUAL ID {
		if ( (i_tmp=str2facility($3))==-1)
			yyerror("bad facility (see syslog(3) man page)");
		if (!config_check)
			default_core_cfg.log_facility=i_tmp;
	}
	| LOGFACILITY EQUAL error { yyerror("ID expected"); }
	| LOGNAME EQUAL STRING { log_name=$3; }
	| LOGNAME EQUAL error { yyerror("string value expected"); }
	| LOGCOLOR EQUAL NUMBER { log_color=$3; }
	| LOGCOLOR EQUAL error { yyerror("boolean value expected"); }
	| LOGPREFIX EQUAL STRING { log_prefix_fmt=$3; }
	| LOGPREFIX EQUAL error { yyerror("string value expected"); }
	| DNS EQUAL NUMBER   { received_dns|= ($3)?DO_DNS:0; }
	| DNS EQUAL error { yyerror("boolean value expected"); }
	| REV_DNS EQUAL NUMBER { received_dns|= ($3)?DO_REV_DNS:0; }
	| REV_DNS EQUAL error { yyerror("boolean value expected"); }
	| DNS_TRY_IPV6 EQUAL NUMBER   { default_core_cfg.dns_try_ipv6=$3; }
	| DNS_TRY_IPV6 error { yyerror("boolean value expected"); }
	| DNS_TRY_NAPTR EQUAL NUMBER   { IF_NAPTR(default_core_cfg.dns_try_naptr=$3); }
	| DNS_TRY_NAPTR error { yyerror("boolean value expected"); }
	| DNS_SRV_LB EQUAL NUMBER   { IF_DNS_FAILOVER(default_core_cfg.dns_srv_lb=$3); }
	| DNS_SRV_LB error { yyerror("boolean value expected"); }
	| DNS_UDP_PREF EQUAL intno { IF_NAPTR(default_core_cfg.dns_udp_pref=$3);}
	| DNS_UDP_PREF error { yyerror("number expected"); }
	| DNS_TCP_PREF EQUAL intno { IF_NAPTR(default_core_cfg.dns_tcp_pref=$3);}
	| DNS_TCP_PREF error { yyerror("number expected"); }
	| DNS_TLS_PREF EQUAL intno { IF_NAPTR(default_core_cfg.dns_tls_pref=$3);}
	| DNS_TLS_PREF error { yyerror("number expected"); }
	| DNS_SCTP_PREF EQUAL intno { 
								IF_NAPTR(default_core_cfg.dns_sctp_pref=$3); }
	| DNS_SCTP_PREF error { yyerror("number expected"); }
	| DNS_RETR_TIME EQUAL NUMBER   { default_core_cfg.dns_retr_time=$3; }
	| DNS_RETR_TIME error { yyerror("number expected"); }
	| DNS_RETR_NO EQUAL NUMBER   { default_core_cfg.dns_retr_no=$3; }
	| DNS_RETR_NO error { yyerror("number expected"); }
	| DNS_SERVERS_NO EQUAL NUMBER   { default_core_cfg.dns_servers_no=$3; }
	| DNS_SERVERS_NO error { yyerror("number expected"); }
	| DNS_USE_SEARCH EQUAL NUMBER   { default_core_cfg.dns_search_list=$3; }
	| DNS_USE_SEARCH error { yyerror("boolean value expected"); }
	| DNS_SEARCH_FMATCH EQUAL NUMBER   { default_core_cfg.dns_search_fmatch=$3; }
	| DNS_SEARCH_FMATCH error { yyerror("boolean value expected"); }
	| DNS_NAPTR_IGNORE_RFC EQUAL NUMBER   { default_core_cfg.dns_naptr_ignore_rfc=$3; }
	| DNS_NAPTR_IGNORE_RFC error { yyerror("boolean value expected"); }
	| DNS_CACHE_INIT EQUAL NUMBER   { IF_DNS_CACHE(dns_cache_init=$3); }
	| DNS_CACHE_INIT error { yyerror("boolean value expected"); }
	| DNS_USE_CACHE EQUAL NUMBER   { IF_DNS_CACHE(default_core_cfg.use_dns_cache=$3); }
	| DNS_USE_CACHE error { yyerror("boolean value expected"); }
	| DNS_USE_FAILOVER EQUAL NUMBER   { IF_DNS_FAILOVER(default_core_cfg.use_dns_failover=$3);}
	| DNS_USE_FAILOVER error { yyerror("boolean value expected"); }
	| DNS_CACHE_FLAGS EQUAL NUMBER   { IF_DNS_CACHE(default_core_cfg.dns_cache_flags=$3); }
	| DNS_CACHE_FLAGS error { yyerror("boolean value expected"); }
	| DNS_CACHE_NEG_TTL EQUAL NUMBER   { IF_DNS_CACHE(default_core_cfg.dns_neg_cache_ttl=$3); }
	| DNS_CACHE_NEG_TTL error { yyerror("boolean value expected"); }
	| DNS_CACHE_MAX_TTL EQUAL NUMBER   { IF_DNS_CACHE(default_core_cfg.dns_cache_max_ttl=$3); }
	| DNS_CACHE_MAX_TTL error { yyerror("boolean value expected"); }
	| DNS_CACHE_MIN_TTL EQUAL NUMBER   { IF_DNS_CACHE(default_core_cfg.dns_cache_min_ttl=$3); }
	| DNS_CACHE_MIN_TTL error { yyerror("boolean value expected"); }
	| DNS_CACHE_MEM EQUAL NUMBER   { IF_DNS_CACHE(default_core_cfg.dns_cache_max_mem=$3); }
	| DNS_CACHE_MEM error { yyerror("boolean value expected"); }
	| DNS_CACHE_GC_INT EQUAL NUMBER   { IF_DNS_CACHE(dns_timer_interval=$3); }
	| DNS_CACHE_GC_INT error { yyerror("boolean value expected"); }
	| DNS_CACHE_DEL_NONEXP EQUAL NUMBER   { IF_DNS_CACHE(default_core_cfg.dns_cache_del_nonexp=$3); }
	| DNS_CACHE_DEL_NONEXP error { yyerror("boolean value expected"); }
	| DNS_CACHE_REC_PREF EQUAL NUMBER   { IF_DNS_CACHE(default_core_cfg.dns_cache_rec_pref=$3); }
	| DNS_CACHE_REC_PREF error { yyerror("boolean value expected"); }
	| AUTO_BIND_IPV6 EQUAL NUMBER {IF_AUTO_BIND_IPV6(auto_bind_ipv6 = $3);}
	| AUTO_BIND_IPV6 error { yyerror("boolean value expected"); }
	| DST_BLST_INIT EQUAL NUMBER   { IF_DST_BLACKLIST(dst_blacklist_init=$3); }
	| DST_BLST_INIT error { yyerror("boolean value expected"); }
	| USE_DST_BLST EQUAL NUMBER {
		IF_DST_BLACKLIST(default_core_cfg.use_dst_blacklist=$3);
	}
	| USE_DST_BLST error { yyerror("boolean value expected"); }
	| DST_BLST_MEM EQUAL NUMBER {
		IF_DST_BLACKLIST(default_core_cfg.blst_max_mem=$3); 
	}
	| DST_BLST_MEM error { yyerror("boolean value expected"); }
	| DST_BLST_TTL EQUAL NUMBER {
		IF_DST_BLACKLIST(default_core_cfg.blst_timeout=$3);
	}
	| DST_BLST_TTL error { yyerror("boolean value expected"); }
	| DST_BLST_GC_INT EQUAL NUMBER { IF_DST_BLACKLIST(blst_timer_interval=$3);}
	| DST_BLST_GC_INT error { yyerror("boolean value expected"); }
	| DST_BLST_UDP_IMASK EQUAL NUMBER {
		IF_DST_BLACKLIST(default_core_cfg.blst_udp_imask=$3);
	}
	| DST_BLST_UDP_IMASK error { yyerror("number(flags) expected"); }
	| DST_BLST_TCP_IMASK EQUAL NUMBER {
		IF_DST_BLACKLIST(default_core_cfg.blst_tcp_imask=$3);
	}
	| DST_BLST_TCP_IMASK error { yyerror("number(flags) expected"); }
	| DST_BLST_TLS_IMASK EQUAL NUMBER {
		IF_DST_BLACKLIST(default_core_cfg.blst_tls_imask=$3);
	}
	| DST_BLST_TLS_IMASK error { yyerror("number(flags) expected"); }
	| DST_BLST_SCTP_IMASK EQUAL NUMBER {
		IF_DST_BLACKLIST(default_core_cfg.blst_sctp_imask=$3);
	}
	| DST_BLST_SCTP_IMASK error { yyerror("number(flags) expected"); }
	| PORT EQUAL NUMBER   { port_no=$3; }
	| STAT EQUAL STRING {
		#ifdef STATS
				stat_file=$3;
		#endif
	}
	| MAXBUFFER EQUAL NUMBER { maxbuffer=$3; }
	| MAXBUFFER EQUAL error { yyerror("number expected"); }
    | SQL_BUFFER_SIZE EQUAL NUMBER { sql_buffer_size=$3; }
	| SQL_BUFFER_SIZE EQUAL error { yyerror("number expected"); }
	| PORT EQUAL error    { yyerror("number expected"); }
	| CHILDREN EQUAL NUMBER { children_no=$3; }
	| CHILDREN EQUAL error { yyerror("number expected"); }
	| SOCKET_WORKERS EQUAL NUMBER { socket_workers=$3; }
	| SOCKET_WORKERS EQUAL error { yyerror("number expected"); }
	| ASYNC_WORKERS EQUAL NUMBER { async_task_set_workers($3); }
	| ASYNC_WORKERS EQUAL error { yyerror("number expected"); }
	| CHECK_VIA EQUAL NUMBER { check_via=$3; }
	| CHECK_VIA EQUAL error { yyerror("boolean value expected"); }
	| PHONE2TEL EQUAL NUMBER { phone2tel=$3; }
	| PHONE2TEL EQUAL error { yyerror("boolean value expected"); }
	| MEMLOG EQUAL intno { default_core_cfg.memlog=$3; }
	| MEMLOG EQUAL error { yyerror("int value expected"); }
	| MEMDBG EQUAL intno { default_core_cfg.memdbg=$3; }
	| MEMDBG EQUAL error { yyerror("int value expected"); }
	| MEMSUM EQUAL intno { default_core_cfg.mem_summary=$3; }
	| MEMSUM EQUAL error { yyerror("int value expected"); }
	| MEMSAFETY EQUAL intno { default_core_cfg.mem_safety=$3; }
	| MEMSAFETY EQUAL error { yyerror("int value expected"); }
	| MEMJOIN EQUAL intno { default_core_cfg.mem_join=$3; }
	| MEMJOIN EQUAL error { yyerror("int value expected"); }
	| CORELOG EQUAL intno { default_core_cfg.corelog=$3; }
	| CORELOG EQUAL error { yyerror("int value expected"); }
	| SIP_WARNING EQUAL NUMBER { sip_warning=$3; }
	| SIP_WARNING EQUAL error { yyerror("boolean value expected"); }
	| VERSION_TABLE_CFG EQUAL STRING { version_table.s=$3;
			version_table.len=strlen(version_table.s);
	}
	| VERSION_TABLE_CFG EQUAL error { yyerror("string value expected"); }
	| USER EQUAL STRING     {
		if (shm_initialized())
			yyerror("user must be before any modparam or the"
					" route blocks");
		else if (user==0)
			user=$3; 
	}
	| USER EQUAL ID         {
		if (shm_initialized())
			yyerror("user must be before any modparam or the"
					" route blocks");
		else if (user==0)
			user=$3;
	}
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
	| RUNDIR EQUAL STRING     { runtime_dir=$3; }
	| RUNDIR EQUAL ID         { runtime_dir=$3; }
	| RUNDIR EQUAL error      { yyerror("string value expected"); }
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
	| TCP_ACCEPT_ALIASES EQUAL NUMBER {
		#ifdef USE_TCP
			tcp_default_cfg.accept_aliases=$3;
		#else
			warn("tcp support not compiled in");
		#endif
	}
	| TCP_ACCEPT_ALIASES EQUAL error { yyerror("boolean value expected"); }
	| TCP_CHILDREN EQUAL NUMBER {
		#ifdef USE_TCP
			tcp_cfg_children_no=$3;
		#else
			warn("tcp support not compiled in");
		#endif
	}
	| TCP_CHILDREN EQUAL error { yyerror("number expected"); }
	| TCP_CONNECT_TIMEOUT EQUAL intno {
		#ifdef USE_TCP
			tcp_default_cfg.connect_timeout_s=$3;
		#else
			warn("tcp support not compiled in");
		#endif
	}
	| TCP_CONNECT_TIMEOUT EQUAL error { yyerror("number expected"); }
	| TCP_SEND_TIMEOUT EQUAL intno {
		#ifdef USE_TCP
			tcp_default_cfg.send_timeout=S_TO_TICKS($3);
		#else
			warn("tcp support not compiled in");
		#endif
	}
	| TCP_SEND_TIMEOUT EQUAL error { yyerror("number expected"); }
	| TCP_CON_LIFETIME EQUAL intno {
		#ifdef USE_TCP
			if ($3<0)
				tcp_default_cfg.con_lifetime=-1;
			else
				tcp_default_cfg.con_lifetime=S_TO_TICKS($3);
		#else
			warn("tcp support not compiled in");
		#endif
	}
	| TCP_CON_LIFETIME EQUAL error { yyerror("number expected"); }
	| TCP_POLL_METHOD EQUAL ID {
		#ifdef USE_TCP
			tcp_poll_method=get_poll_type($3);
			if (tcp_poll_method==POLL_NONE) {
				LOG(L_CRIT, "bad poll method name:"
						" %s\n, try one of %s.\n",
						$3, poll_support);
				yyerror("bad tcp_poll_method "
						"value");
			}
		#else
			warn("tcp support not compiled in");
		#endif
	}
	| TCP_POLL_METHOD EQUAL STRING {
		#ifdef USE_TCP
			tcp_poll_method=get_poll_type($3);
			if (tcp_poll_method==POLL_NONE) {
				LOG(L_CRIT, "bad poll method name:"
						" %s\n, try one of %s.\n",
						$3, poll_support);
				yyerror("bad tcp_poll_method "
						"value");
			}
		#else
			warn("tcp support not compiled in");
		#endif
	}
	| TCP_POLL_METHOD EQUAL error { yyerror("poll method name expected"); }
	| TCP_MAX_CONNECTIONS EQUAL NUMBER {
		#ifdef USE_TCP
			tcp_max_connections=$3;
		#else
			warn("tcp support not compiled in");
		#endif
	}
	| TCP_MAX_CONNECTIONS EQUAL error { yyerror("number expected"); }
	| TLS_MAX_CONNECTIONS EQUAL NUMBER {
		#ifdef USE_TLS
			tls_max_connections=$3;
		#else
			warn("tls support not compiled in");
		#endif
	}
	| TLS_MAX_CONNECTIONS EQUAL error { yyerror("number expected"); }
	| TCP_NO_CONNECT EQUAL NUMBER {
		#ifdef USE_TCP
			tcp_default_cfg.no_connect=$3;
		#else
			warn("tcp support not compiled in");
		#endif
	}
	| TCP_NO_CONNECT EQUAL error { yyerror("boolean value expected"); }
	| TCP_SOURCE_IPV4 EQUAL ipv4 {
		#ifdef USE_TCP
			if (tcp_set_src_addr($3)<0)
				warn("tcp_source_ipv4 failed");
		#else
			warn("tcp support not compiled in");
		#endif
		pkg_free($3);
	}
	| TCP_SOURCE_IPV4 EQUAL error { yyerror("IPv4 address expected"); }
	| TCP_SOURCE_IPV6 EQUAL ipv6 {
		#ifdef USE_TCP
				if (tcp_set_src_addr($3)<0)
					warn("tcp_source_ipv6 failed");
		#else
			warn("tcp support not compiled in");
		#endif
		pkg_free($3);
	}
	| TCP_SOURCE_IPV6 EQUAL error { yyerror("IPv6 address expected"); }
	| TCP_OPT_FD_CACHE EQUAL NUMBER {
		#ifdef USE_TCP
			tcp_default_cfg.fd_cache=$3;
		#else
			warn("tcp support not compiled in");
		#endif
	}
	| TCP_OPT_FD_CACHE EQUAL error { yyerror("boolean value expected"); }
	| TCP_OPT_BUF_WRITE EQUAL NUMBER {
		#ifdef USE_TCP
			tcp_default_cfg.async=$3;
		#else
			warn("tcp support not compiled in");
		#endif
	}
	| TCP_OPT_BUF_WRITE EQUAL error { yyerror("boolean value expected"); }
	| TCP_OPT_CONN_WQ_MAX EQUAL NUMBER {
		#ifdef USE_TCP
			tcp_default_cfg.tcpconn_wq_max=$3;
		#else
			warn("tcp support not compiled in");
		#endif
	}
	| TCP_OPT_CONN_WQ_MAX error { yyerror("boolean value expected"); }
	| TCP_OPT_WQ_MAX EQUAL NUMBER {
		#ifdef USE_TCP
			tcp_default_cfg.tcp_wq_max=$3;
		#else
			warn("tcp support not compiled in");
		#endif
	}
	| TCP_OPT_WQ_MAX error { yyerror("number expected"); }
	| TCP_OPT_RD_BUF EQUAL NUMBER {
		#ifdef USE_TCP
			tcp_default_cfg.rd_buf_size=$3;
		#else
			warn("tcp support not compiled in");
		#endif
	}
	| TCP_OPT_RD_BUF error { yyerror("number expected"); }
	| TCP_OPT_WQ_BLK EQUAL NUMBER {
		#ifdef USE_TCP
			tcp_default_cfg.wq_blk_size=$3;
		#else
			warn("tcp support not compiled in");
		#endif
	}
	| TCP_OPT_WQ_BLK error { yyerror("number expected"); }
	| TCP_OPT_DEFER_ACCEPT EQUAL NUMBER {
		#ifdef USE_TCP
			tcp_default_cfg.defer_accept=$3;
		#else
			warn("tcp support not compiled in");
		#endif
	}
	| TCP_OPT_DEFER_ACCEPT EQUAL error { yyerror("boolean value expected"); }
	| TCP_OPT_DELAYED_ACK EQUAL NUMBER {
		#ifdef USE_TCP
			tcp_default_cfg.delayed_ack=$3;
		#else
			warn("tcp support not compiled in");
		#endif
	}
	| TCP_OPT_DELAYED_ACK EQUAL error { yyerror("boolean value expected"); }
	| TCP_OPT_SYNCNT EQUAL NUMBER {
		#ifdef USE_TCP
			tcp_default_cfg.syncnt=$3;
		#else
			warn("tcp support not compiled in");
		#endif
	}
	| TCP_OPT_SYNCNT EQUAL error { yyerror("number expected"); }
	| TCP_OPT_LINGER2 EQUAL NUMBER {
		#ifdef USE_TCP
			tcp_default_cfg.linger2=$3;
		#else
			warn("tcp support not compiled in");
		#endif
	}
	| TCP_OPT_LINGER2 EQUAL error { yyerror("number expected"); }
	| TCP_OPT_KEEPALIVE EQUAL NUMBER {
		#ifdef USE_TCP
			tcp_default_cfg.keepalive=$3;
		#else
			warn("tcp support not compiled in");
		#endif
	}
	| TCP_OPT_KEEPALIVE EQUAL error { yyerror("boolean value expected");}
	| TCP_OPT_KEEPIDLE EQUAL NUMBER {
		#ifdef USE_TCP
			tcp_default_cfg.keepidle=$3;
		#else
			warn("tcp support not compiled in");
		#endif
	}
	| TCP_OPT_KEEPIDLE EQUAL error { yyerror("number expected"); }
	| TCP_OPT_KEEPINTVL EQUAL NUMBER {
		#ifdef USE_TCP
			tcp_default_cfg.keepintvl=$3;
		#else
			warn("tcp support not compiled in");
		#endif
	}
	| TCP_OPT_KEEPINTVL EQUAL error { yyerror("number expected"); }
	| TCP_OPT_KEEPCNT EQUAL NUMBER {
		#ifdef USE_TCP
			tcp_default_cfg.keepcnt=$3;
		#else
			warn("tcp support not compiled in");
		#endif
	}
	| TCP_OPT_KEEPCNT EQUAL error { yyerror("number expected"); }
	| TCP_OPT_CRLF_PING EQUAL NUMBER {
		#ifdef USE_TCP
			tcp_default_cfg.crlf_ping=$3;
		#else
			warn("tcp support not compiled in");
		#endif
	}
	| TCP_OPT_CRLF_PING EQUAL error { yyerror("boolean value expected"); }
	| TCP_OPT_ACCEPT_NO_CL EQUAL NUMBER {
		#ifdef USE_TCP
			tcp_default_cfg.accept_no_cl=$3;
		#else
			warn("tcp support not compiled in");
		#endif
	}
	| TCP_OPT_ACCEPT_NO_CL EQUAL error { yyerror("boolean value expected"); }
	| TCP_CLONE_RCVBUF EQUAL NUMBER {
		#ifdef USE_TCP
			tcp_set_clone_rcvbuf($3);
		#else
			warn("tcp support not compiled in");
		#endif
	}
	| TCP_CLONE_RCVBUF EQUAL error { yyerror("number expected"); }
	| DISABLE_TLS EQUAL NUMBER {
		#ifdef USE_TLS
			tls_disable=$3;
		#else
			warn("tls support not compiled in");
		#endif
	}
	| DISABLE_TLS EQUAL error { yyerror("boolean value expected"); }
	| ENABLE_TLS EQUAL NUMBER {
		#ifdef USE_TLS
			tls_disable=!($3);
		#else
			warn("tls support not compiled in");
		#endif
	}
	| ENABLE_TLS EQUAL error { yyerror("boolean value expected"); }
	| TLSLOG EQUAL NUMBER {
		#ifdef CORE_TLS
			tls_log=$3;
		#else
			warn("tls-in-core support not compiled in");
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
		#ifdef CORE_TLS
			tls_method=TLS_USE_SSLv23;
		#else
			warn("tls-in-core support not compiled in");
		#endif
	}
	| TLS_METHOD EQUAL SSLv2 {
		#ifdef CORE_TLS
			tls_method=TLS_USE_SSLv2;
		#else
			warn("tls-in-core support not compiled in");
		#endif
	}
	| TLS_METHOD EQUAL SSLv3 {
		#ifdef CORE_TLS
			tls_method=TLS_USE_SSLv3;
		#else
			warn("tls-in-core support not compiled in");
		#endif
	}
	| TLS_METHOD EQUAL TLSv1 {
		#ifdef CORE_TLS
			tls_method=TLS_USE_TLSv1;
		#else
			warn("tls-in-core support not compiled in");
		#endif
	}
	| TLS_METHOD EQUAL error {
		#ifdef CORE_TLS
			yyerror("SSLv23, SSLv2, SSLv3 or TLSv1 expected");
		#else
			warn("tls-in-core support not compiled in");
		#endif
	}
	| TLS_VERIFY EQUAL NUMBER {
		#ifdef CORE_TLS
			tls_verify_cert=$3;
		#else
			warn("tls-in-core support not compiled in");
		#endif
	}
	| TLS_VERIFY EQUAL error { yyerror("boolean value expected"); }
	| TLS_REQUIRE_CERTIFICATE EQUAL NUMBER {
		#ifdef CORE_TLS
			tls_require_cert=$3;
		#else
			warn( "tls-in-core support not compiled in");
		#endif
	}
	| TLS_REQUIRE_CERTIFICATE EQUAL error { yyerror("boolean value expected"); }
	| TLS_CERTIFICATE EQUAL STRING {
		#ifdef CORE_TLS
			tls_cert_file=$3;
		#else
			warn("tls-in-core support not compiled in");
		#endif
	}
	| TLS_CERTIFICATE EQUAL error { yyerror("string value expected"); }
	| TLS_PRIVATE_KEY EQUAL STRING {
		#ifdef CORE_TLS
			tls_pkey_file=$3;
		#else
			warn("tls-in-core support not compiled in");
		#endif
	}
	| TLS_PRIVATE_KEY EQUAL error { yyerror("string value expected"); }
	| TLS_CA_LIST EQUAL STRING {
		#ifdef CORE_TLS
			tls_ca_file=$3;
		#else
			warn("tls-in-core support not compiled in");
		#endif
	}
	| TLS_CA_LIST EQUAL error { yyerror("string value expected"); }
	| TLS_HANDSHAKE_TIMEOUT EQUAL NUMBER {
		#ifdef CORE_TLS
			tls_handshake_timeout=$3;
		#else
			warn("tls-in-core support not compiled in");
		#endif
	}
	| TLS_HANDSHAKE_TIMEOUT EQUAL error { yyerror("number expected"); }
	| TLS_SEND_TIMEOUT EQUAL NUMBER {
		#ifdef CORE_TLS
			tls_send_timeout=$3;
		#else
			warn("tls-in-core support not compiled in");
		#endif
	}
	| TLS_SEND_TIMEOUT EQUAL error { yyerror("number expected"); }
	| DISABLE_SCTP EQUAL NUMBER {
		#ifdef USE_SCTP
			sctp_disable=$3;
		#else
			warn("sctp support not compiled in");
		#endif
	}
	| DISABLE_SCTP EQUAL error { yyerror("boolean value expected"); }
	| ENABLE_SCTP EQUAL NUMBER {
		#ifdef USE_SCTP
			sctp_disable=($3<=1)?!$3:$3;
		#else
			warn("sctp support not compiled in");
		#endif
	}
	| ENABLE_SCTP EQUAL error { yyerror("boolean or number expected"); }
	| SCTP_CHILDREN EQUAL NUMBER {
		#ifdef USE_SCTP
			sctp_children_no=$3;
		#else
			warn("sctp support not compiled in");
		#endif
	}
	| SCTP_CHILDREN EQUAL error { yyerror("number expected"); }
	| SERVER_SIGNATURE EQUAL NUMBER { server_signature=$3; }
	| SERVER_SIGNATURE EQUAL error { yyerror("boolean value expected"); }
	| SERVER_HEADER EQUAL STRING { server_hdr.s=$3;
			server_hdr.len=strlen(server_hdr.s);
	}
	| SERVER_HEADER EQUAL error { yyerror("string value expected"); }
	| USER_AGENT_HEADER EQUAL STRING { user_agent_hdr.s=$3;
			user_agent_hdr.len=strlen(user_agent_hdr.s);
	}
	| USER_AGENT_HEADER EQUAL error { yyerror("string value expected"); }
	| REPLY_TO_VIA EQUAL NUMBER { reply_to_via=$3; }
	| REPLY_TO_VIA EQUAL error { yyerror("boolean value expected"); }
	| LISTEN EQUAL id_lst {
		for(lst_tmp=$3; lst_tmp; lst_tmp=lst_tmp->next) {
			if (add_listen_iface(	lst_tmp->addr_lst->name,
									lst_tmp->addr_lst->next,
									lst_tmp->port, lst_tmp->proto,
									lst_tmp->flags)!=0) {
				LOG(L_CRIT,  "ERROR: cfg. parser: failed to add listen"
								" address\n");
				break;
			}
		}
		free_socket_id_lst($3);
	}
	| LISTEN EQUAL id_lst ADVERTISE listen_id COLON NUMBER {
		for(lst_tmp=$3; lst_tmp; lst_tmp=lst_tmp->next) {
			if (add_listen_advertise_iface(	lst_tmp->addr_lst->name,
									lst_tmp->addr_lst->next,
									lst_tmp->port, lst_tmp->proto,
									$5, $7,
									lst_tmp->flags)!=0) {
				LOG(L_CRIT,  "ERROR: cfg. parser: failed to add listen"
								" address\n");
				break;
			}
		}
		free_socket_id_lst($3);
	}
	| LISTEN EQUAL  error { yyerror("ip address, interface name or"
									" hostname expected"); }
	| ALIAS EQUAL  id_lst {
		for(lst_tmp=$3; lst_tmp; lst_tmp=lst_tmp->next){
			add_alias(	lst_tmp->addr_lst->name,
						strlen(lst_tmp->addr_lst->name),
						lst_tmp->port, lst_tmp->proto);
			for (nl_tmp=lst_tmp->addr_lst->next; nl_tmp; nl_tmp=nl_tmp->next)
				add_alias(nl_tmp->name, strlen(nl_tmp->name),
							lst_tmp->port, lst_tmp->proto);
		}
		free_socket_id_lst($3);
	}
	| ALIAS  EQUAL error  { yyerror(" hostname expected"); }
	| SR_AUTO_ALIASES EQUAL NUMBER { sr_auto_aliases=$3; }
	| SR_AUTO_ALIASES EQUAL error  { yyerror("boolean value expected"); }
	| ADVERTISED_ADDRESS EQUAL listen_id {
		if ($3){
			default_global_address.s=$3;
			default_global_address.len=strlen($3);
		}
	}
	| ADVERTISED_ADDRESS EQUAL error {yyerror("ip address or hostname expected"); }
	| ADVERTISED_PORT EQUAL NUMBER {
		tmp=int2str($3, &i_tmp);
		if ((default_global_port.s=pkg_malloc(i_tmp))==0) {
			LOG(L_CRIT, "ERROR: cfg. parser: out of memory.\n");
			default_global_port.len=0;
		} else {
			default_global_port.len=i_tmp;
			memcpy(default_global_port.s, tmp, default_global_port.len);
		};
	}
	|ADVERTISED_PORT EQUAL error {yyerror("ip address or hostname expected"); }
	| DISABLE_CORE EQUAL NUMBER { disable_core_dump=$3; }
	| DISABLE_CORE EQUAL error { yyerror("boolean value expected"); }
	| OPEN_FD_LIMIT EQUAL NUMBER { open_files_limit=$3; }
	| OPEN_FD_LIMIT EQUAL error { yyerror("number expected"); }
	| SHM_MEM_SZ EQUAL NUMBER {
		if (shm_initialized())
			yyerror("shm/shm_mem_size must be before any modparam or the"
					" route blocks");
		else if (shm_mem_size == 0)
			shm_mem_size=$3 * 1024 * 1024;
	}
	| SHM_MEM_SZ EQUAL error { yyerror("number expected"); }
	| SHM_FORCE_ALLOC EQUAL NUMBER {
		if (shm_initialized())
			yyerror("shm_force_alloc must be before any modparam or the"
					" route blocks");
		else
			shm_force_alloc=$3;
	}
	| SHM_FORCE_ALLOC EQUAL error { yyerror("boolean value expected"); }
	| MLOCK_PAGES EQUAL NUMBER { mlock_pages=$3; }
	| MLOCK_PAGES EQUAL error { yyerror("boolean value expected"); }
	| REAL_TIME EQUAL NUMBER { real_time=$3; }
	| REAL_TIME EQUAL error { yyerror("boolean value expected"); }
	| RT_PRIO EQUAL NUMBER { rt_prio=$3; }
	| RT_PRIO EQUAL error { yyerror("boolean value expected"); }
	| RT_POLICY EQUAL NUMBER { rt_policy=$3; }
	| RT_POLICY EQUAL error { yyerror("boolean value expected"); }
	| RT_TIMER1_PRIO EQUAL NUMBER { rt_timer1_prio=$3; }
	| RT_TIMER1_PRIO EQUAL error { yyerror("boolean value expected"); }
	| RT_TIMER1_POLICY EQUAL NUMBER { rt_timer1_policy=$3; }
	| RT_TIMER1_POLICY EQUAL error { yyerror("boolean value expected"); }
	| RT_TIMER2_PRIO EQUAL NUMBER { rt_timer2_prio=$3; }
	| RT_TIMER2_PRIO EQUAL error { yyerror("boolean value expected"); }
	| RT_TIMER2_POLICY EQUAL NUMBER { rt_timer2_policy=$3; }
	| RT_TIMER2_POLICY EQUAL error { yyerror("boolean value expected"); }
	| MCAST_LOOPBACK EQUAL NUMBER {
		#ifdef USE_MCAST
			mcast_loopback=$3;
		#else
			warn("no multicast support compiled in");
		#endif
	}
	| MCAST_LOOPBACK EQUAL error { yyerror("boolean value expected"); }
	| MCAST_TTL EQUAL NUMBER {
		#ifdef USE_MCAST
			mcast_ttl=$3;
		#else
			warn("no multicast support compiled in");
		#endif
	}
	| MCAST_TTL EQUAL error { yyerror("number expected"); }
	| TOS EQUAL NUMBER { tos=$3; }
	| TOS EQUAL ID { if (strcasecmp($3,"IPTOS_LOWDELAY")) {
			tos=IPTOS_LOWDELAY;
		} else if (strcasecmp($3,"IPTOS_THROUGHPUT")) {
			tos=IPTOS_THROUGHPUT;
		} else if (strcasecmp($3,"IPTOS_RELIABILITY")) {
			tos=IPTOS_RELIABILITY;
#if defined(IPTOS_MINCOST)
		} else if (strcasecmp($3,"IPTOS_MINCOST")) {
			tos=IPTOS_MINCOST;
#endif
#if defined(IPTOS_LOWCOST)
		} else if (strcasecmp($3,"IPTOS_LOWCOST")) {
			tos=IPTOS_LOWCOST;
#endif
		} else {
			yyerror("invalid tos value - allowed: "
				"IPTOS_LOWDELAY,IPTOS_THROUGHPUT,"
				"IPTOS_RELIABILITY"
#if defined(IPTOS_LOWCOST)
				",IPTOS_LOWCOST"
#endif
#if !defined(IPTOS_MINCOST)
				",IPTOS_MINCOST"
#endif
				"\n");
		}
	}
	| TOS EQUAL error { yyerror("number expected"); }
	| PMTU_DISCOVERY EQUAL NUMBER { pmtu_discovery=$3; }
	| PMTU_DISCOVERY error { yyerror("number expected"); }
	| KILL_TIMEOUT EQUAL NUMBER { ser_kill_timeout=$3; }
	| KILL_TIMEOUT EQUAL error { yyerror("number expected"); }
	| MAX_WLOOPS EQUAL NUMBER { default_core_cfg.max_while_loops=$3; }
	| MAX_WLOOPS EQUAL error { yyerror("number expected"); }
	| PVBUFSIZE EQUAL NUMBER { pv_set_buffer_size($3); }
	| PVBUFSIZE EQUAL error { yyerror("number expected"); }
	| PVBUFSLOTS EQUAL NUMBER { pv_set_buffer_slots($3); }
	| PVBUFSLOTS EQUAL error { yyerror("number expected"); }
	| HTTP_REPLY_PARSE EQUAL NUMBER { http_reply_parse=$3; }
	| HTTP_REPLY_PARSE EQUAL error { yyerror("boolean value expected"); }
    | SERVER_ID EQUAL NUMBER { server_id=$3; }
    | MAX_RECURSIVE_LEVEL EQUAL NUMBER { set_max_recursive_level($3); }
    | MAX_BRANCHES_PARAM EQUAL NUMBER { sr_dst_max_branches = $3; }
    | LATENCY_LOG EQUAL NUMBER { default_core_cfg.latency_log=$3; }
	| LATENCY_LOG EQUAL error  { yyerror("number  expected"); }
    | LATENCY_LIMIT_DB EQUAL NUMBER { default_core_cfg.latency_limit_db=$3; }
	| LATENCY_LIMIT_DB EQUAL error  { yyerror("number  expected"); }
    | LATENCY_LIMIT_ACTION EQUAL NUMBER { default_core_cfg.latency_limit_action=$3; }
	| LATENCY_LIMIT_ACTION EQUAL error  { yyerror("number  expected"); }
    | MSG_TIME EQUAL NUMBER { sr_msg_time=$3; }
	| MSG_TIME EQUAL error  { yyerror("number  expected"); }
	| ONSEND_RT_REPLY EQUAL NUMBER { onsend_route_reply=$3; }
	| ONSEND_RT_REPLY EQUAL error { yyerror("int value expected"); }
	| UDP_MTU EQUAL NUMBER { default_core_cfg.udp_mtu=$3; }
	| UDP_MTU EQUAL error { yyerror("number expected"); }
	| FORCE_RPORT EQUAL NUMBER 
		{ default_core_cfg.force_rport=$3; fix_global_req_flags(0, 0); }
	| FORCE_RPORT EQUAL error { yyerror("boolean value expected"); }
	| UDP_MTU_TRY_PROTO EQUAL proto
		{ default_core_cfg.udp_mtu_try_proto=$3; fix_global_req_flags(0, 0); }
	| UDP_MTU_TRY_PROTO EQUAL error
		{ yyerror("TCP, TLS, SCTP or UDP expected"); }
	| UDP4_RAW EQUAL intno { IF_RAW_SOCKS(default_core_cfg.udp4_raw=$3); }
	| UDP4_RAW EQUAL error { yyerror("number expected"); }
	| UDP4_RAW_MTU EQUAL NUMBER {
		IF_RAW_SOCKS(default_core_cfg.udp4_raw_mtu=$3);
	}
	| UDP4_RAW_MTU EQUAL error { yyerror("number expected"); }
	| UDP4_RAW_TTL EQUAL NUMBER {
		IF_RAW_SOCKS(default_core_cfg.udp4_raw_ttl=$3);
	}
	| UDP4_RAW_TTL EQUAL error { yyerror("number expected"); }
	| cfg_var
	| error EQUAL { yyerror("unknown config variable"); }
	;
	
cfg_var_id: ID 
	| DEFAULT { $$="default" ; } /*needed to allow default as cfg var. name*/
	;
	
cfg_var:
	cfg_var_id DOT cfg_var_id EQUAL NUMBER {
		if (cfg_declare_int($1, $3, $5, 0, 0, NULL)) {
			yyerror("variable cannot be declared");
		}
	}
	| cfg_var_id DOT cfg_var_id EQUAL STRING {
		if (cfg_declare_str($1, $3, $5, NULL)) {
			yyerror("variable cannot be declared");
		}
	}
	| cfg_var_id DOT cfg_var_id EQUAL NUMBER CFG_DESCRIPTION STRING {
		if (cfg_declare_int($1, $3, $5, 0, 0, $7)) {
			yyerror("variable cannot be declared");
		}
	}
	| cfg_var_id DOT cfg_var_id EQUAL STRING CFG_DESCRIPTION STRING {
		if (cfg_declare_str($1, $3, $5, $7)) {
			yyerror("variable cannot be declared");
		}
	}
	| cfg_var_id DOT cfg_var_id EQUAL error { 
		yyerror("number or string expected"); 
	}
	| cfg_var_id LBRACK NUMBER RBRACK DOT cfg_var_id EQUAL NUMBER {
		if (cfg_ginst_var_int($1, $3, $6, $8)) {
			yyerror("variable cannot be added to the group instance");
		}
	}
	| cfg_var_id LBRACK NUMBER RBRACK DOT cfg_var_id EQUAL STRING {
		if (cfg_ginst_var_string($1, $3, $6, $8)) {
			yyerror("variable cannot be added to the group instance");
		}
	}
	;

module_stm:
	LOADMODULE STRING {
		DBG("loading module %s\n", $2);
			if (load_module($2)!=0) {
				yyerror("failed to load module");
			}
	}
	| LOADMODULE error	{ yyerror("string expected"); }
	| LOADPATH STRING {
		if(mods_dir_cmd==0) {
			DBG("loading modules under %s\n", $2);
			printf("loading modules under config path: %s\n", $2);
			mods_dir = $2;
		} else {
			DBG("ignoring mod path given in config: %s\n", $2);
			printf("loading modules under command line path: %s\n", mods_dir);
		}
	}
	| LOADPATH error	{ yyerror("string expected"); }
	| LOADPATH EQUAL STRING {
		if(mods_dir_cmd==0) {
			DBG("loading modules under %s\n", $3);
			printf("loading modules under config path: %s\n", $3);
			mods_dir = $3;
		} else {
			DBG("ignoring mod path given in config: %s\n", $3);
			printf("loading modules under command line path: %s\n", mods_dir);
		}
	}
	| LOADPATH EQUAL error	{ yyerror("string expected"); }
	| MODPARAM LPAREN STRING COMMA STRING COMMA STRING RPAREN {
	#ifdef SHM_MEM
		if (!shm_initialized() && init_shm()<0) {
			yyerror("Can't initialize shared memory");
			YYABORT;
		}
	#endif /* SHM_MEM */
		if (set_mod_param_regex($3, $5, PARAM_STRING, $7) != 0) {
			 yyerror("Can't set module parameter");
		}
	}
	| MODPARAM LPAREN STRING COMMA STRING COMMA intno RPAREN {
	#ifdef SHM_MEM
		if (!shm_initialized() && init_shm()<0) {
			yyerror("Can't initialize shared memory");
			YYABORT;
		}
	#endif /* SHM_MEM */
		if (set_mod_param_regex($3, $5, PARAM_INT, (void*)$7) != 0) {
			 yyerror("Can't set module parameter");
		}
	}
	| MODPARAM error { yyerror("Invalid arguments"); }
	;
ip:
	ipv4  { $$=$1; }
	| ipv6  { $$=$1; }
	;
ipv4:
	NUMBER DOT NUMBER DOT NUMBER DOT NUMBER {
		$$=pkg_malloc(sizeof(struct ip_addr));
		if ($$==0) {
			LOG(L_CRIT, "ERROR: cfg. parser: out of memory.\n");
		} else {
			memset($$, 0, sizeof(struct ip_addr));
			$$->af=AF_INET;
			$$->len=4;
			if (($1>255) || ($1<0) ||
				($3>255) || ($3<0) ||
				($5>255) || ($5<0) ||
				($7>255) || ($7<0)) {
				yyerror("invalid ipv4 address");
				$$->u.addr32[0]=0;
				/* $$=0; */
			} else {
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
ipv6addr:
	IPV6ADDR {
		$$=pkg_malloc(sizeof(struct ip_addr));
		if ($$==0) {
			LOG(L_CRIT, "ERROR: cfg. parser: out of memory.\n");
		} else {
			memset($$, 0, sizeof(struct ip_addr));
			$$->af=AF_INET6;
			$$->len=16;
			if (inet_pton(AF_INET6, $1, $$->u.addr)<=0) {
				yyerror("bad ipv6 address");
			}
		}
	}
	;
ipv6:
	ipv6addr { $$=$1; }
	| LBRACK ipv6addr RBRACK {$$=$2; }
;


route_name:		NUMBER	{
					tmp=int2str($1, &i_tmp);
					if (($$=pkg_malloc(i_tmp+1))==0) {
						yyerror("out of  memory");
						YYABORT;
					} else {
						memcpy($$, tmp, i_tmp);
						$$[i_tmp]=0;
					}
					routename = tmp;
						}
			|	ID		{ routename = $1; $$=$1; }
			|	STRING	{ routename = $1; $$=$1; }
;


route_main:	ROUTE { ; }
		  | ROUTE_REQUEST { ; }
;

route_stm:
	route_main LBRACE actions RBRACE {
	#ifdef SHM_MEM
		if (!shm_initialized() && init_shm()<0) {
			yyerror("Can't initialize shared memory");
			YYABORT;
		}
	#endif /* SHM_MEM */
		push($3, &main_rt.rlist[DEFAULT_RT]);
	}
	| ROUTE LBRACK route_name RBRACK LBRACE actions RBRACE {
	#ifdef SHM_MEM
		if (!shm_initialized() && init_shm()<0) {
			yyerror("Can't initialize shared memory");
			YYABORT;
		}
	#endif /* SHM_MEM */
		i_tmp=route_get(&main_rt, $3);
		if (i_tmp==-1){
			yyerror("internal error");
			YYABORT;
		}
		if (main_rt.rlist[i_tmp]){
			yyerror("duplicate route");
			YYABORT;
		}
		push($6, &main_rt.rlist[i_tmp]);
	}
	| ROUTE error { yyerror("invalid  route  statement"); }
	| ROUTE_REQUEST error { yyerror("invalid  request_route  statement"); }
	;
failure_route_stm:
	ROUTE_FAILURE LBRACE actions RBRACE {
	#ifdef SHM_MEM
		if (!shm_initialized() && init_shm()<0) {
			yyerror("Can't initialize shared memory");
			YYABORT;
		}
	#endif /* SHM_MEM */
		push($3, &failure_rt.rlist[DEFAULT_RT]);
	}
	| ROUTE_FAILURE LBRACK route_name RBRACK LBRACE actions RBRACE {
	#ifdef SHM_MEM
		if (!shm_initialized() && init_shm()<0) {
			yyerror("Can't initialize shared memory");
			YYABORT;
		}
	#endif /* SHM_MEM */
		i_tmp=route_get(&failure_rt, $3);
		if (i_tmp==-1){
			yyerror("internal error");
			YYABORT;
		}
		if (failure_rt.rlist[i_tmp]){
			yyerror("duplicate route");
			YYABORT;
		}
		push($6, &failure_rt.rlist[i_tmp]);
	}
	| ROUTE_FAILURE error { yyerror("invalid failure_route statement"); }
	;


route_reply_main:	ROUTE_ONREPLY { ; }
		  | ROUTE_REPLY { ; }
;


onreply_route_stm:
	route_reply_main LBRACE {rt=CORE_ONREPLY_ROUTE;} actions RBRACE {
	#ifdef SHM_MEM
		if (!shm_initialized() && init_shm()<0) {
			yyerror("Can't initialize shared memory");
			YYABORT;
		}
	#endif /* SHM_MEM */
		push($4, &onreply_rt.rlist[DEFAULT_RT]);
	}
	| ROUTE_ONREPLY error { yyerror("invalid onreply_route statement"); }
	| ROUTE_REPLY error { yyerror("invalid onreply_route statement"); }
	| ROUTE_ONREPLY LBRACK route_name RBRACK 
		{rt=(*$3=='0' && $3[1]==0)?CORE_ONREPLY_ROUTE:TM_ONREPLY_ROUTE;}
		LBRACE actions RBRACE {
	#ifdef SHM_MEM
		if (!shm_initialized() && init_shm()<0) {
			yyerror("Can't initialize shared memory");
			YYABORT;
		}
	#endif /* SHM_MEM */
		if (*$3=='0' && $3[1]==0){
			/* onreply_route[0] {} is equivalent with onreply_route {}*/
			push($7, &onreply_rt.rlist[DEFAULT_RT]);
		}else{
			i_tmp=route_get(&onreply_rt, $3);
			if (i_tmp==-1){
				yyerror("internal error");
				YYABORT;
			}
			if (onreply_rt.rlist[i_tmp]){
				yyerror("duplicate route");
				YYABORT;
			}
			push($7, &onreply_rt.rlist[i_tmp]);
		}
	}
	| ROUTE_ONREPLY LBRACK route_name RBRACK error {
		yyerror("invalid onreply_route statement");
	}
	;
branch_route_stm:
	ROUTE_BRANCH LBRACE actions RBRACE {
	#ifdef SHM_MEM
		if (!shm_initialized() && init_shm()<0) {
			yyerror("Can't initialize shared memory");
			YYABORT;
		}
	#endif /* SHM_MEM */
		push($3, &branch_rt.rlist[DEFAULT_RT]);
	}
	| ROUTE_BRANCH LBRACK route_name RBRACK LBRACE actions RBRACE {
	#ifdef SHM_MEM
		if (!shm_initialized() && init_shm()<0) {
			yyerror("Can't initialize shared memory");
			YYABORT;
		}
	#endif /* SHM_MEM */
		i_tmp=route_get(&branch_rt, $3);
		if (i_tmp==-1){
			yyerror("internal error");
			YYABORT;
		}
		if (branch_rt.rlist[i_tmp]){
			yyerror("duplicate route");
			YYABORT;
		}
		push($6, &branch_rt.rlist[i_tmp]);
	}
	| ROUTE_BRANCH error { yyerror("invalid branch_route statement"); }
	;
send_route_stm: ROUTE_SEND LBRACE actions RBRACE {
	#ifdef SHM_MEM
		if (!shm_initialized() && init_shm()<0) {
			yyerror("Can't initialize shared memory");
			YYABORT;
		}
	#endif /* SHM_MEM */
		push($3, &onsend_rt.rlist[DEFAULT_RT]);
	}
	| ROUTE_SEND LBRACK route_name RBRACK LBRACE actions RBRACE {
	#ifdef SHM_MEM
		if (!shm_initialized() && init_shm()<0) {
			yyerror("Can't initialize shared memory");
			YYABORT;
		}
	#endif /* SHM_MEM */
		i_tmp=route_get(&onsend_rt, $3);
		if (i_tmp==-1){
			yyerror("internal error");
			YYABORT;
		}
		if (onsend_rt.rlist[i_tmp]){
			yyerror("duplicate route");
			YYABORT;
		}
		push($6, &onsend_rt.rlist[i_tmp]);
	}
	| ROUTE_SEND error { yyerror("invalid onsend_route statement"); }
	;
event_route_stm: ROUTE_EVENT LBRACK EVENT_RT_NAME RBRACK LBRACE actions RBRACE {
	#ifdef SHM_MEM
		if (!shm_initialized() && init_shm()<0) {
			yyerror("Can't initialize shared memory");
			YYABORT;
		}
	#endif /* SHM_MEM */
		i_tmp=route_get(&event_rt, $3);
		if (i_tmp==-1){
			yyerror("internal error");
			YYABORT;
		}
		if (event_rt.rlist[i_tmp]){
			yyerror("duplicate route");
			YYABORT;
		}
		push($6, &event_rt.rlist[i_tmp]);
	}

	| ROUTE_EVENT error { yyerror("invalid event_route statement"); }
	;
preprocess_stm:
	SUBST STRING { if(pp_subst_add($2)<0) YYERROR; }
	| SUBST error { yyerror("invalid subst preprocess statement"); }
	| SUBSTDEF STRING { if(pp_substdef_add($2, 0)<0) YYERROR; }
	| SUBSTDEF error { yyerror("invalid substdef preprocess statement"); }
	| SUBSTDEFS STRING { if(pp_substdef_add($2, 1)<0) YYERROR; }
	| SUBSTDEFS error { yyerror("invalid substdefs preprocess statement"); }
	;

/*exp:	rval_expr
		{
			if ($1==0){
				yyerror("invalid expression");
				$$=0;
			}else if (!rve_check_type((enum rval_type*)&i_tmp, $1, 0, 0 ,0)){
				yyerror("invalid expression");
				$$=0;
			}else if (i_tmp!=RV_INT && i_tmp!=RV_NONE){
				yyerror("invalid expression type, int expected\n");
				$$=0;
			}else
				$$=mk_elem(NO_OP, RVEXP_O, $1, 0, 0);
		}
	;
*/

/* exp elem operators */
equalop:
	EQUAL_T {$$=EQUAL_OP; }
	| DIFF	{$$=DIFF_OP; }
	| STREQ	{$$=EQUAL_OP; }  /* for expr. elems equiv. to EQUAL_T*/
	| STRDIFF {$$=DIFF_OP; } /* for expr. elems. equiv. to DIFF */
	;
cmpop:
	  GT	{$$=GT_OP; }
	| LT	{$$=LT_OP; }
	| GTE	{$$=GTE_OP; }
	| LTE	{$$=LTE_OP; }
	;
strop:
	equalop	{$$=$1; }
	| MATCH	{$$=MATCH_OP; }
	;


/* rve expr. operators */
rve_equalop:
	EQUAL_T {$$=RVE_EQ_OP; }
	| DIFF	{$$=RVE_DIFF_OP; }
	| INTEQ	{$$=RVE_IEQ_OP; }
	| INTDIFF {$$=RVE_IDIFF_OP; }
	| STREQ	{$$=RVE_STREQ_OP; }
	| STRDIFF {$$=RVE_STRDIFF_OP; }
	| MATCH	{$$=RVE_MATCH_OP; }
	;
rve_cmpop:
	  GT	{$$=RVE_GT_OP; }
	| LT	{$$=RVE_LT_OP; }
	| GTE	{$$=RVE_GTE_OP; }
	| LTE	{$$=RVE_LTE_OP; }
	;



/* boolean expression uri operands */
uri_type:
	URI		{$$=URI_O;}
	| FROM_URI	{$$=FROM_URI_O;}
	| TO_URI	{$$=TO_URI_O;}
	;


/* boolean expression integer operands, available only in the
  onsend route */
eint_op_onsend:
			SNDPORT		{ $$=SNDPORT_O; }
		|	TOPORT		{ $$=TOPORT_O; }
		|	SNDAF		{ $$=SNDAF_O; }
		;

/* boolean expression integer operands */
eint_op:	SRCPORT		{ $$=SRCPORT_O; }
		|	DSTPORT		{ $$=DSTPORT_O; }
		|	AF			{ $$=AF_O; }
		|	MSGLEN		{ $$=MSGLEN_O; }
		| eint_op_onsend
	;

/* boolean expression ip/ipnet operands */
eip_op_onsend:
			SNDIP		{ onsend_check("snd_ip"); $$=SNDIP_O; }
		|	TOIP		{ onsend_check("to_ip");  $$=TOIP_O; }
		;

eip_op:		SRCIP		{ $$=SRCIP_O; }
		|	DSTIP		{ $$=DSTIP_O; }
		| eip_op_onsend
		;



exp_elem:
	METHOD strop rval_expr %prec EQUAL_T
		{$$= mk_elem($2, METHOD_O, 0, RVE_ST, $3);}
	| METHOD strop ID %prec EQUAL_T
		{$$ = mk_elem($2, METHOD_O, 0, STRING_ST,$3); }
	| METHOD strop error { $$=0; yyerror("string expected"); }
	| METHOD error	
		{ $$=0; yyerror("invalid operator,== , !=, or =~ expected"); }
	| uri_type strop rval_expr %prec EQUAL_T
		{$$ = mk_elem($2, $1, 0, RVE_ST, $3); }
	| uri_type strop MYSELF %prec EQUAL_T
		{$$=mk_elem($2, $1, 0, MYSELF_ST, 0); }
	| uri_type strop error %prec EQUAL_T
		{ $$=0; yyerror("string or MYSELF expected"); }
	| uri_type error
		{ $$=0; yyerror("invalid operator, == , != or =~ expected"); }
	| eint_op cmpop rval_expr %prec GT { $$=mk_elem($2, $1, 0, RVE_ST, $3 ); }
	| eint_op equalop rval_expr %prec EQUAL_T
		{ $$=mk_elem($2, $1, 0, RVE_ST, $3 ); }
	| eint_op cmpop error   { $$=0; yyerror("number expected"); }
	| eint_op equalop error { $$=0; yyerror("number expected"); }
	| eint_op error { $$=0; yyerror("==, !=, <,>, >= or <=  expected"); }
	| PROTO equalop eqproto %prec EQUAL_T
		{ $$=mk_elem($2, PROTO_O, 0, NUMBER_ST, (void*)$3 ); }
	| PROTO equalop rval_expr %prec EQUAL_T
		{ $$=mk_elem($2, PROTO_O, 0, RVE_ST, $3 ); }
	| PROTO equalop error
		{ $$=0; yyerror("protocol expected (udp, tcp, tls, sctp, ws, or wss)"); }
	| SNDPROTO equalop eqproto %prec EQUAL_T
		{ $$=mk_elem($2, SNDPROTO_O, 0, NUMBER_ST, (void*)$3 ); }
	| SNDPROTO equalop rval_expr %prec EQUAL_T
		{ $$=mk_elem($2, SNDPROTO_O, 0, RVE_ST, $3 ); }
	| SNDPROTO equalop error
		{ $$=0; yyerror("protocol expected (udp, tcp, tls, sctp, ws, or wss)"); }
	| eip_op strop ipnet %prec EQUAL_T { $$=mk_elem($2, $1, 0, NET_ST, $3); }
	| eip_op strop rval_expr %prec EQUAL_T {
			s_tmp.s=0;
			$$=0;
			if (rve_is_constant($3)){
				i_tmp=rve_guess_type($3);
				if (i_tmp==RV_INT)
					yyerror("string expected");
				else if (i_tmp==RV_STR){
					if (((rval_tmp=rval_expr_eval(0, 0, $3))==0) ||
								(rval_get_str(0, 0, &s_tmp, rval_tmp, 0)<0)){
						rval_destroy(rval_tmp);
						yyerror("bad rvalue expression");
					}else{
						rval_destroy(rval_tmp);
					}
				}else{
					yyerror("BUG: unexpected dynamic type");
				}
			}else{
					/* warn("non constant rvalue in ip comparison") */;
			}
			if (s_tmp.s){
				ip_tmp=str2ip(&s_tmp);
				if (ip_tmp==0)
					ip_tmp=str2ip6(&s_tmp);
				pkg_free(s_tmp.s);
				if (ip_tmp) {
					$$=mk_elem($2, $1, 0, NET_ST, 
								mk_new_net_bitlen(ip_tmp, ip_tmp->len*8) );
				} else {
					$$=mk_elem($2, $1, 0, RVE_ST, $3);
				}
			}else{
				$$=mk_elem($2, $1, 0, RVE_ST, $3);
			}
		}
	| eip_op strop host %prec EQUAL_T
		{ $$=mk_elem($2, $1, 0, STRING_ST, $3); }
	| eip_op strop MYSELF %prec EQUAL_T
		{ $$=mk_elem($2, $1, 0, MYSELF_ST, 0); }
	| eip_op strop error %prec EQUAL_T
		{ $$=0; yyerror( "ip address or hostname expected" ); }
	| eip_op error
		{ $$=0; yyerror("invalid operator, ==, != or =~ expected");}
	
	| MYSELF equalop uri_type %prec EQUAL_T
		{ $$=mk_elem($2, $3, 0, MYSELF_ST, 0); }
	| MYSELF equalop eip_op %prec EQUAL_T
		{ $$=mk_elem($2, $3, 0, MYSELF_ST, 0); }
	| MYSELF equalop error %prec EQUAL_T
		{ $$=0; yyerror(" URI, SRCIP or DSTIP expected"); }
	| MYSELF error	{ $$=0; yyerror ("invalid operator, == or != expected"); }
	;

ipnet:
	ip SLASH ip	{ $$=mk_new_net($1, $3); }
	| ip SLASH NUMBER {
		if (($3<0) || ($3>$1->len*8)) {
			yyerror("invalid bit number in netmask");
			$$=0;
		} else {
			$$=mk_new_net_bitlen($1, $3);
		/*
			$$=mk_new_net($1, htonl( ($3)?~( (1<<(32-$3))-1 ):0 ) );
		*/
		}
	}
	| ip	{ $$=mk_new_net_bitlen($1, $1->len*8); }
	| ip SLASH error { $$=0; yyerror("netmask (eg:255.0.0.0 or 8) expected"); }
	;

host:
	ID { $$=$1; }
	| host DOT ID {
		if ($1){
			$$=(char*)pkg_malloc(strlen($1)+1+strlen($3)+1);
			if ($$==0) {
				LOG(L_CRIT, "ERROR: cfg. parser: memory allocation"
							" failure while parsing host\n");
			} else {
				memcpy($$, $1, strlen($1));
				$$[strlen($1)]='.';
				memcpy($$+strlen($1)+1, $3, strlen($3));
				$$[strlen($1)+1+strlen($3)]=0;
			}
			pkg_free($1);
		}
		if ($3) pkg_free($3);
	}
	| host MINUS ID {
		if ($1){
			$$=(char*)pkg_malloc(strlen($1)+1+strlen($3)+1);
			if ($$==0) {
				LOG(L_CRIT, "ERROR: cfg. parser: memory allocation"
							" failure while parsing host\n");
			} else {
				memcpy($$, $1, strlen($1));
				$$[strlen($1)]='-';
				memcpy($$+strlen($1)+1, $3, strlen($3));
				$$[strlen($1)+1+strlen($3)]=0;
			}
			pkg_free($1);
		}
		if ($3) pkg_free($3);
	}
	| host DOT error { $$=0; pkg_free($1); yyerror("invalid hostname"); }
	| host MINUS error { $$=0; pkg_free($1); yyerror("invalid hostname"); }
	;

host_if_id: ID
		| NUM_ID
		| NUMBER {
			/* get string version */
			$$=pkg_malloc(strlen(yy_number_str)+1);
			if ($$)
				strcpy($$, yy_number_str);
		}
		;

host_or_if:
	host_if_id { $$=$1; }
	| host_or_if DOT host_if_id {
		if ($1){
			$$=(char*)pkg_malloc(strlen($1)+1+strlen($3)+1);
			if ($$==0) {
				LOG(L_CRIT, "ERROR: cfg. parser: memory allocation"
							" failure while parsing host/interface name\n");
			} else {
				memcpy($$, $1, strlen($1));
				$$[strlen($1)]='.';
				memcpy($$+strlen($1)+1, $3, strlen($3));
				$$[strlen($1)+1+strlen($3)]=0;
			}
			pkg_free($1);
		}
		if ($3) pkg_free($3);
	}
	| host_or_if MINUS host_if_id {
		if ($1){
			$$=(char*)pkg_malloc(strlen($1)+1+strlen($3)+1);
			if ($$==0) {
				LOG(L_CRIT, "ERROR: cfg. parser: memory allocation"
							" failure while parsing host/interface name\n");
			} else {
				memcpy($$, $1, strlen($1));
				$$[strlen($1)]='-';
				memcpy($$+strlen($1)+1, $3, strlen($3));
				$$[strlen($1)+1+strlen($3)]=0;
			}
			pkg_free($1);
		}
		if ($3) pkg_free($3);
	}
	| host_or_if DOT error { $$=0; pkg_free($1);
								yyerror("invalid host or interface name"); }
	| host_or_if MINUS error { $$=0; pkg_free($1);
								yyerror("invalid host or interface name"); }
	;


/* filtered cmd */
fcmd:
	cmd {
		/* check if allowed */
		if ($1 && rt==ONSEND_ROUTE) {
			switch($1->type) {
				case DROP_T:
				case LOG_T:
				case SETFLAG_T:
				case RESETFLAG_T:
				case ISFLAGSET_T:
				case IF_T:
				case MODULE0_T:
				case MODULE1_T:
				case MODULE2_T:
				case MODULE3_T:
				case MODULE4_T:
				case MODULE5_T:
				case MODULE6_T:
				case MODULEX_T:
				case SET_FWD_NO_CONNECT_T:
				case SET_RPL_NO_CONNECT_T:
				case SET_FWD_CLOSE_T:
				case SET_RPL_CLOSE_T:
					$$=$1;
					break;
				default:
					$$=0;
					yyerror("command not allowed in onsend_route\n");
			}
		} else {
			$$=$1;
		}
	}
	;
/*
exp_stm:
	fcmd	{ $$=$1; }
	| if_cmd	{ $$=$1; }
	| assign_action { $$ = $1; }
	| LBRACE actions RBRACE	{ $$=$2; }
	;
*/
stm:
	action	{ $$=$1; }
	| LBRACE actions RBRACE	{ $$=$2; }
	;
actions:
	actions action	{$$=append_action($1, $2); }
	| action	{$$=$1;}
	| actions error { $$=0; yyerror("bad command"); }
	;
action:
	fcmd SEMICOLON {$$=$1;}
	| if_cmd {$$=$1;}
	| switch_cmd {$$=$1;}
	| while_cmd { $$=$1; }
	| ret_cmd SEMICOLON { $$=$1; }
	| assign_action SEMICOLON {$$=$1;}
	| SEMICOLON /* null action */ {$$=0;}
	| fcmd error { $$=0; yyerror("bad command: missing ';'?"); }
	;
if_cmd:
	IF rval_expr stm	{
		if ($2 && rval_expr_int_check($2)>=0){
			warn_ct_rve($2, "if");
			$$=mk_action( IF_T, 3, RVE_ST, $2, ACTIONS_ST, $3, NOSUBTYPE, 0);
			set_cfg_pos($$);
		}else
			YYERROR;
	}
	| IF rval_expr stm ELSE stm	{ 
		if ($2 && rval_expr_int_check($2)>=0){
			warn_ct_rve($2, "if");
			$$=mk_action( IF_T, 3, RVE_ST, $2, ACTIONS_ST, $3, ACTIONS_ST, $5);
			set_cfg_pos($$);
		}else
			YYERROR;
	}
	;

ct_rval: rval_expr {
			$$=0;
			if ($1 && !rve_is_constant($1)){
				yyerror("constant expected");
				YYERROR;
			/*
			} else if ($1 &&
						!rve_check_type((enum rval_type*)&i_tmp, $1, 0, 0 ,0)){
				yyerror("invalid expression (bad type)");
			}else if ($1 && i_tmp!=RV_INT){
				yyerror("invalid expression type, int expected\n");
			*/
			}else
				$$=$1;
		}
;
single_case:
	CASE ct_rval COLON actions {
		$$=0;
		if ($2==0) { yyerror ("bad case label"); YYERROR; }
		else if ((($$=mk_case_stm($2, 0, $4, &i_tmp))==0) && (i_tmp==-10)){
				YYABORT;
		}
	}
| CASE SLASH ct_rval COLON actions {
		$$=0;
		if ($3==0) { yyerror ("bad case label"); YYERROR; }
		else if ((($$=mk_case_stm($3, 1, $5, &i_tmp))==0) && (i_tmp==-10)){
				YYABORT;
		}
	}
	| CASE ct_rval COLON {
		$$=0;
		if ($2==0) { yyerror ("bad case label"); YYERROR; }
		else if ((($$=mk_case_stm($2, 0, 0, &i_tmp))==0) && (i_tmp==-10)){
				YYABORT;
		}
	}
	| CASE SLASH ct_rval COLON {
		$$=0;
		if ($3==0) { yyerror ("bad regex case label"); YYERROR; }
		else if ((($$=mk_case_stm($3, 1, 0, &i_tmp))==0) && (i_tmp==-10)){
				YYABORT;
		}
	}
	| DEFAULT COLON actions {
		if ((($$=mk_case_stm(0, 0, $3, &i_tmp))==0) && (i_tmp=-10)){
				YYABORT;
		}
	}
	| DEFAULT COLON {
		if ((($$=mk_case_stm(0, 0, 0, &i_tmp))==0) && (i_tmp==-10)){
				YYABORT;
		}
	}
	| CASE error COLON actions { $$=0; yyerror("bad case label"); }
	| CASE SLASH error COLON actions { $$=0; yyerror("bad case regex label"); }
	| CASE error COLON { $$=0; yyerror("bad case label"); }
	| CASE SLASH error COLON { $$=0; yyerror("bad case regex label"); }
	| CASE ct_rval COLON error { $$=0; yyerror ("bad case body"); }
;
case_stms:
	case_stms single_case {
		$$=$1;
		if ($2==0) yyerror ("bad case");
		if ($$){
			*($$->append)=$2;
			if (*($$->append)!=0)
				$$->append=&((*($$->append))->next);
		}
	}
	| single_case {
		$$=$1;
		if ($1==0) yyerror ("bad case");
		else $$->append=&($$->next);
	}
;
switch_cmd:
	  SWITCH rval_expr LBRACE case_stms RBRACE { 
		$$=0;
		if ($2==0){
			yyerror("bad expression in switch(...)");
			YYERROR;
		}else if ($4==0){
			yyerror ("bad switch body");
			YYERROR;
		}else if (case_check_default($4)!=0){
			yyerror_at(&$2->fpos, "bad switch(): too many "
							"\"default:\" labels\n");
			YYERROR;
		}else if (case_check_type($4)!=0){
			yyerror_at(&$2->fpos, "bad switch(): mixed integer and"
							" string/RE cases not allowed\n");
			YYERROR;
		}else{
			$$=mk_action(SWITCH_T, 2, RVE_ST, $2, CASE_ST, $4);
			if ($$==0) {
				yyerror("internal error");
				YYABORT;
			}
			set_cfg_pos($$);
		}
	}
	| SWITCH rval_expr LBRACE RBRACE {
		$$=0;
		warn("empty switch()");
		if ($2==0){
			yyerror("bad expression in switch(...)");
			YYERROR;
		}else{
			/* it might have sideffects, so leave it for the optimizer */
			$$=mk_action(SWITCH_T, 2, RVE_ST, $2, CASE_ST, 0);
			if ($$==0) {
				yyerror("internal error");
				YYABORT;
			}
			set_cfg_pos($$);
		}
	}
	| SWITCH error { $$=0; yyerror ("bad expression in switch(...)"); }
	| SWITCH rval_expr LBRACE error RBRACE 
		{$$=0; yyerror ("bad switch body"); }
;

while_cmd:
	WHILE rval_expr stm {
		if ($2 && rval_expr_int_check($2)>=0){
			warn_ct_rve($2, "while");
			$$=mk_action( WHILE_T, 2, RVE_ST, $2, ACTIONS_ST, $3);
			set_cfg_pos($$);
		}else{
			yyerror_at(&$2->fpos, "bad while(...) expression");
			YYERROR;
		}
	}
;

/* class_id:
	LBRACK ATTR_USER RBRACK { $$ = AVP_CLASS_USER; }
	| LBRACK ATTR_DOMAIN RBRACK { $$ = AVP_CLASS_DOMAIN; }
	| LBRACK ATTR_GLOBAL RBRACK { $$ = AVP_CLASS_GLOBAL; }
	;
*/
select_param:
	ID {
		if (sel.n >= MAX_SELECT_PARAMS-1) {
			yyerror("Select identifier too long\n");
		}
		sel.params[sel.n].type = SEL_PARAM_STR;
		sel.params[sel.n].v.s.s = $1;
		sel.params[sel.n].v.s.len = strlen($1);
		sel.n++;
	}
	| ID LBRACK intno RBRACK {
		if (sel.n >= MAX_SELECT_PARAMS-2) {
			yyerror("Select identifier too long\n");
		}
		sel.params[sel.n].type = SEL_PARAM_STR;
		sel.params[sel.n].v.s.s = $1;
		sel.params[sel.n].v.s.len = strlen($1);
		sel.n++;
		sel.params[sel.n].type = SEL_PARAM_INT;
		sel.params[sel.n].v.i = $3;
		sel.n++;
	}
	| ID LBRACK STRING RBRACK {
		if (sel.n >= MAX_SELECT_PARAMS-2) {
			yyerror("Select identifier too long\n");
		}
		sel.params[sel.n].type = SEL_PARAM_STR;
		sel.params[sel.n].v.s.s = $1;
		sel.params[sel.n].v.s.len = strlen($1);
		sel.n++;
		sel.params[sel.n].type = SEL_PARAM_STR;
		sel.params[sel.n].v.s.s = $3;
		sel.params[sel.n].v.s.len = strlen($3);
		sel.n++;
	}
	;
select_params:
	select_params DOT select_param
	| select_param
	;
select_id:
	SELECT_MARK { sel.n = 0; sel.f[0] = 0; } select_params {
		sel_ptr = (select_t*)pkg_malloc(sizeof(select_t));
		if (!sel_ptr) {
			yyerror("No memory left to allocate select structure\n");
		}
		memcpy(sel_ptr, &sel, sizeof(select_t));
		$$ = sel_ptr;
	}
	;
attr_class_spec:
	ATTR_FROM { s_attr->type |= AVP_TRACK_FROM; }
	| ATTR_TO { s_attr->type |= AVP_TRACK_TO; }
        | ATTR_FROMURI { s_attr->type |= AVP_TRACK_FROM | AVP_CLASS_URI; }
        | ATTR_TOURI { s_attr->type |= AVP_TRACK_TO | AVP_CLASS_URI; }
	| ATTR_FROMUSER { s_attr->type |= AVP_TRACK_FROM | AVP_CLASS_USER; }
	| ATTR_TOUSER { s_attr->type |= AVP_TRACK_TO | AVP_CLASS_USER; }
	| ATTR_FROMDOMAIN { s_attr->type |= AVP_TRACK_FROM | AVP_CLASS_DOMAIN; }
	| ATTR_TODOMAIN { s_attr->type |= AVP_TRACK_TO | AVP_CLASS_DOMAIN; }
	| ATTR_GLOBAL { s_attr->type |= AVP_TRACK_ALL | AVP_CLASS_GLOBAL; }
	;
attr_name_spec:
	ID { s_attr->type |= AVP_NAME_STR; s_attr->name.s.s = $1; s_attr->name.s.len = strlen ($1); }
	;
attr_spec:
	attr_name_spec
	| attr_class_spec DOT attr_name_spec
	;
attr_mark:
	ATTR_MARK {
		s_attr = (struct avp_spec*)pkg_malloc(sizeof(struct avp_spec));
		if (!s_attr) { yyerror("No memory left"); YYABORT; }
		else s_attr->type = 0;
	}
	;
attr_id:
	attr_mark attr_spec { $$ = s_attr; }
	;
attr_id_num_idx:
	attr_mark attr_spec LBRACK intno RBRACK {
		s_attr->type|= (AVP_NAME_STR | ($4<0?AVP_INDEX_BACKWARD:AVP_INDEX_FORWARD));
		s_attr->index = ($4<0?-$4:$4);
		$$ = s_attr;
	}
	;
attr_id_no_idx:
	attr_mark attr_spec LBRACK RBRACK {
		s_attr->type|= AVP_INDEX_ALL;
		$$ = s_attr;
	}
	;
attr_id_ass:
	attr_id
	| attr_id_no_idx
	;
/*
attr_id_val:
	attr_id
	| attr_id_num_idx
	;
*/
attr_id_any:
	attr_id
	| attr_id_no_idx
	| attr_id_num_idx
;
attr_id_any_str:
	attr_id
	| avp_pvar {
		if ($1->type==LV_AVP){
			s_attr = pkg_malloc(sizeof(struct avp_spec));
			if (!s_attr) { yyerror("No memory left"); YYABORT; }
			else{
				*s_attr=$1->lv.avps;
			}
			$$=s_attr;
		}else
			$$=0; /* not an avp, a pvar */
		pkg_free($1);
	}
	| STRING {
		avp_spec_t *avp_spec;
		str s;
		int type, idx;
		avp_spec = pkg_malloc(sizeof(*avp_spec));
		if (!avp_spec) {
			yyerror("Not enough memory");
			YYABORT;
		}
		s.s = $1;
		if (s.s[0] == '$')
			s.s++;
		s.len = strlen(s.s);
		if (parse_avp_name(&s, &type, &avp_spec->name, &idx)) {
			yyerror("error when parsing AVP");
			pkg_free(avp_spec);
			YYABORT;
		}
		avp_spec->type = type;
		avp_spec->index = idx;
		$$ = avp_spec;
	}
	;

pvar:	PVAR {
			s_tmp.s=$1; s_tmp.len=strlen($1);
			pv_spec=pv_cache_get(&s_tmp);
			if (!pv_spec) {
				yyerror("Can't get from cache: %s", $1);
				YYABORT;
			}
			$$=pv_spec;
		}
	;

avp_pvar:	AVP_OR_PVAR {
				lval_tmp=pkg_malloc(sizeof(*lval_tmp));
				if (!lval_tmp) {
					yyerror("Not enough memory");
					YYABORT;
				}
				memset(lval_tmp, 0, sizeof(*lval_tmp));
				s_tmp.s=$1; s_tmp.len=strlen(s_tmp.s);
				lval_tmp->lv.pvs = pv_cache_get(&s_tmp);
				if (lval_tmp->lv.pvs==NULL){
					lval_tmp->lv.avps.type|= AVP_NAME_STR;
					lval_tmp->lv.avps.name.s.s = s_tmp.s+1;
					lval_tmp->lv.avps.name.s.len = s_tmp.len-1;
					lval_tmp->type=LV_AVP;
				}else{
					lval_tmp->type=LV_PVAR;
				}
				$$ = lval_tmp;
				DBG("parsed ambigous avp/pvar \"%.*s\" to %d\n",
							s_tmp.len, s_tmp.s, lval_tmp->type);
			}
	;


/*
assign_op:
	ADDEQ { $$ = ADD_T; }
	| EQUAL { $$ = ASSIGN_T; }
	;
*/
assign_op:
	EQUAL { $$ = ASSIGN_T; }
	;


lval: attr_id_ass {
					lval_tmp=pkg_malloc(sizeof(*lval_tmp));
					if (!lval_tmp) {
						yyerror("Not enough memory");
						YYABORT;
					}
					lval_tmp->type=LV_AVP; lval_tmp->lv.avps=*$1;
					pkg_free($1); /* free the avp spec we just copied */
					$$=lval_tmp;
				}
	| pvar        {
					if (!pv_is_w($1))
						yyerror("read only pvar in assignment left side");
					if ($1->trans!=0)
						yyerror("pvar with transformations in assignment"
								" left side");
					lval_tmp=pkg_malloc(sizeof(*lval_tmp));
					if (!lval_tmp) {
						yyerror("Not enough memory");
						YYABORT;
					}
					lval_tmp->type=LV_PVAR; lval_tmp->lv.pvs=$1;
					$$=lval_tmp;
				}
	| avp_pvar    {
					if (($1)->type==LV_PVAR){
						if (!pv_is_w($1->lv.pvs))
							yyerror("read only pvar in assignment left side");
						if ($1->lv.pvs->trans!=0)
							yyerror("pvar with transformations in assignment"
									" left side");
					}
					$$=$1;
				}
	;

rval: intno			{$$=mk_rve_rval(RV_INT, (void*)$1); }
	| STRING			{	s_tmp.s=$1; s_tmp.len=strlen($1);
							$$=mk_rve_rval(RV_STR, &s_tmp); }
	| attr_id_any		{$$=mk_rve_rval(RV_AVP, $1); pkg_free($1); }
	| pvar				{$$=mk_rve_rval(RV_PVAR, $1); }
	| avp_pvar			{
							switch($1->type){
								case LV_AVP:
									$$=mk_rve_rval(RV_AVP, &$1->lv.avps);
									break;
								case LV_PVAR:
									$$=mk_rve_rval(RV_PVAR, $1->lv.pvs);
									break;
								default:
									yyerror("BUG: invalid lvalue type ");
									YYABORT;
							}
							pkg_free($1); /* not needed anymore */
						}
	| select_id			{$$=mk_rve_rval(RV_SEL, $1); pkg_free($1); }
	| fcmd				{$$=mk_rve_rval(RV_ACTION_ST, $1); }
	| exp_elem { $$=mk_rve_rval(RV_BEXPR, $1); }
	| LBRACE actions RBRACE	{$$=mk_rve_rval(RV_ACTION_ST, $2); }
	| LBRACE error RBRACE	{ $$=0; yyerror("bad command block"); }
	| LPAREN assign_action RPAREN	{$$=mk_rve_rval(RV_ACTION_ST, $2); }
	| LPAREN error RPAREN	{ $$=0; yyerror("bad expression"); }
	;


rve_un_op: NOT	{ $$=RVE_LNOT_OP; }
		|  BIN_NOT 	{ $$=RVE_BNOT_OP; }
		|  MINUS %prec UNARY	{ $$=RVE_UMINUS_OP; }
		/* TODO: RVE_BOOL_OP, RVE_NOT_OP? */
	;

/*
rve_op:		PLUS		{ $$=RVE_PLUS_OP; }
		|	MINUS		{ $$=RVE_MINUS_OP; }
		|	STAR		{ $$=RVE_MUL_OP; }
		|	SLASH		{ $$=RVE_DIV_OP; }
		|	MODULO		{ $$=RVE_MOD_OP; }
	;
*/

rval_expr: rval						{ $$=$1;
										if ($$==0){
											/*yyerror("out of memory\n");*/
											YYERROR;
										}
									}
		| rve_un_op rval_expr %prec UNARY	{$$=mk_rve1($1, $2); }
		| INTCAST rval_expr				{$$=mk_rve1(RVE_INT_OP, $2); }
		| STRCAST rval_expr				{$$=mk_rve1(RVE_STR_OP, $2); }
		| rval_expr PLUS rval_expr		{$$=mk_rve2(RVE_PLUS_OP, $1, $3); }
		| rval_expr MINUS rval_expr		{$$=mk_rve2(RVE_MINUS_OP, $1, $3); }
		| rval_expr STAR rval_expr		{$$=mk_rve2(RVE_MUL_OP, $1, $3); }
		| rval_expr SLASH rval_expr		{$$=mk_rve2(RVE_DIV_OP, $1, $3); }
		| rval_expr MODULO rval_expr	{$$=mk_rve2(RVE_MOD_OP, $1, $3); }
		| rval_expr BIN_OR rval_expr	{$$=mk_rve2(RVE_BOR_OP, $1,  $3); }
		| rval_expr BIN_AND rval_expr	{$$=mk_rve2(RVE_BAND_OP, $1,  $3);}
		| rval_expr BIN_XOR rval_expr	{$$=mk_rve2(RVE_BXOR_OP, $1,  $3);}
		| rval_expr BIN_LSHIFT rval_expr {$$=mk_rve2(RVE_BLSHIFT_OP, $1,  $3);}
		| rval_expr BIN_RSHIFT rval_expr {$$=mk_rve2(RVE_BRSHIFT_OP, $1,  $3);}
		| rval_expr rve_cmpop rval_expr %prec GT { $$=mk_rve2( $2, $1, $3);}
		| rval_expr rve_equalop rval_expr %prec EQUAL_T {
			/* comparing with $null => treat as defined or !defined */
			if($3->op==RVE_RVAL_OP && $3->left.rval.type==RV_PVAR
					&& $3->left.rval.v.pvs.type==PVT_NULL) {
				if($2==RVE_DIFF_OP || $2==RVE_IDIFF_OP
						|| $2==RVE_STRDIFF_OP) {
					DBG("comparison with $null switched to notdefined operator\n");
					$$=mk_rve1(RVE_DEFINED_OP, $1);
				} else {
					DBG("comparison with $null switched to defined operator\n");
					$$=mk_rve1(RVE_NOTDEFINED_OP, $1);
				}
				/* free rve struct for $null */
				rve_destroy($3);
			} else {
				$$=mk_rve2($2, $1, $3);
			}
		}
		| rval_expr LOG_AND rval_expr	{ $$=mk_rve2(RVE_LAND_OP, $1, $3);}
		| rval_expr LOG_OR rval_expr	{ $$=mk_rve2(RVE_LOR_OP, $1, $3);}
		| LPAREN rval_expr RPAREN		{ $$=$2;}
		| STRLEN LPAREN rval_expr RPAREN { $$=mk_rve1(RVE_STRLEN_OP, $3);}
		| STREMPTY LPAREN rval_expr RPAREN {$$=mk_rve1(RVE_STREMPTY_OP, $3);}
		| DEFINED rval_expr				{ $$=mk_rve1(RVE_DEFINED_OP, $2);}
		| rve_un_op error %prec UNARY 		{ $$=0; yyerror("bad expression"); }
		| INTCAST error					{ $$=0; yyerror("bad expression"); }
		| STRCAST error					{ $$=0; yyerror("bad expression"); }
		| rval_expr PLUS error			{ $$=0; yyerror("bad expression"); }
		| rval_expr MINUS error			{ $$=0; yyerror("bad expression"); }
		| rval_expr STAR error			{ $$=0; yyerror("bad expression"); }
		| rval_expr SLASH error			{ $$=0; yyerror("bad expression"); }
		| rval_expr MODULO error			{ $$=0; yyerror("bad expression"); }
		| rval_expr BIN_OR error		{ $$=0; yyerror("bad expression"); }
		| rval_expr BIN_AND error		{ $$=0; yyerror("bad expression"); }
		| rval_expr rve_cmpop error %prec GT
			{ $$=0; yyerror("bad expression"); }
		| rval_expr rve_equalop error %prec EQUAL_T
			{ $$=0; yyerror("bad expression"); }
		| rval_expr LOG_AND error		{ $$=0; yyerror("bad expression"); }
		| rval_expr LOG_OR error		{ $$=0; yyerror("bad expression"); }
		| STRLEN LPAREN error RPAREN	{ $$=0; yyerror("bad expression"); }
		| STREMPTY LPAREN error RPAREN	{ $$=0; yyerror("bad expression"); }
		| DEFINED error					{ $$=0; yyerror("bad expression"); }
		;

assign_action: lval assign_op  rval_expr	{ $$=mk_action($2, 2, LVAL_ST, $1, 
														 	  RVE_ST, $3);
											set_cfg_pos($$);
										}
	;

/*
assign_action:
	attr_id_ass assign_op STRING  { $$=mk_action($2, 2, AVP_ST, $1, STRING_ST, $3); }
	| attr_id_ass assign_op NUMBER  { $$=mk_action($2, 2, AVP_ST, $1, NUMBER_ST, (void*)$3); }
	| attr_id_ass assign_op fcmd    { $$=mk_action($2, 2, AVP_ST, $1, ACTION_ST, $3); }
	| attr_id_ass assign_op attr_id_any { $$=mk_action($2, 2, AVP_ST, $1, AVP_ST, $3); }
	| attr_id_ass assign_op select_id { $$=mk_action($2, 2, AVP_ST, (void*)$1, SELECT_ST, (void*)$3); }
	| attr_id_ass assign_op LPAREN exp RPAREN { $$ = mk_action($2, 2, AVP_ST, $1, EXPR_ST, $4); }
	;
*/

avpflag_oper:
	SETAVPFLAG { $$ = 1; }
	| RESETAVPFLAG { $$ = 0; }
	| ISAVPFLAGSET { $$ = -1; }
	;
cmd:
	FORWARD LPAREN RPAREN { $$=mk_action(FORWARD_T, 2, URIHOST_ST, 0, URIPORT_ST, 0); set_cfg_pos($$); }
	| FORWARD LPAREN host RPAREN	{ $$=mk_action(	FORWARD_T, 2, STRING_ST, $3, NUMBER_ST, 0); set_cfg_pos($$); }
	| FORWARD LPAREN STRING RPAREN	{ $$=mk_action(	FORWARD_T, 2, STRING_ST, $3, NUMBER_ST, 0); set_cfg_pos($$); }
	| FORWARD LPAREN ip RPAREN	{ $$=mk_action(	FORWARD_T, 2, IP_ST, (void*)$3, NUMBER_ST, 0); set_cfg_pos($$); }
	| FORWARD LPAREN host COMMA NUMBER RPAREN { $$=mk_action(FORWARD_T, 2, STRING_ST, $3, NUMBER_ST, (void*)$5); set_cfg_pos($$); }
	| FORWARD LPAREN STRING COMMA NUMBER RPAREN {$$=mk_action(FORWARD_T, 2, STRING_ST, $3, NUMBER_ST, (void*)$5); set_cfg_pos($$); }
	| FORWARD LPAREN ip COMMA NUMBER RPAREN { $$=mk_action(FORWARD_T, 2, IP_ST, (void*)$3, NUMBER_ST, (void*)$5); set_cfg_pos($$); }
	| FORWARD LPAREN URIHOST COMMA URIPORT RPAREN { $$=mk_action(FORWARD_T, 2, URIHOST_ST, 0, URIPORT_ST, 0); set_cfg_pos($$); }
	| FORWARD LPAREN URIHOST COMMA NUMBER RPAREN {$$=mk_action(FORWARD_T, 2, URIHOST_ST, 0, NUMBER_ST, (void*)$5); set_cfg_pos($$); }
	| FORWARD LPAREN URIHOST RPAREN { $$=mk_action(FORWARD_T, 2, URIHOST_ST, 0, NUMBER_ST, 0); set_cfg_pos($$); }
	| FORWARD error { $$=0; yyerror("missing '(' or ')' ?"); }
	| FORWARD LPAREN error RPAREN { $$=0; yyerror("bad forward argument"); }
	| FORWARD_UDP LPAREN host RPAREN	{ $$=mk_action(FORWARD_UDP_T, 2, STRING_ST, $3, NUMBER_ST, 0); set_cfg_pos($$); }
	| FORWARD_UDP LPAREN STRING RPAREN	{ $$=mk_action(FORWARD_UDP_T, 2, STRING_ST, $3, NUMBER_ST, 0); set_cfg_pos($$); }
	| FORWARD_UDP LPAREN ip RPAREN	{ $$=mk_action(FORWARD_UDP_T, 2, IP_ST, (void*)$3, NUMBER_ST, 0); set_cfg_pos($$); }
	| FORWARD_UDP LPAREN host COMMA NUMBER RPAREN { $$=mk_action(FORWARD_UDP_T, 2, STRING_ST, $3, NUMBER_ST, (void*)$5); set_cfg_pos($$); }
	| FORWARD_UDP LPAREN STRING COMMA NUMBER RPAREN {$$=mk_action(FORWARD_UDP_T, 2, STRING_ST, $3, NUMBER_ST, (void*)$5); set_cfg_pos($$); }
	| FORWARD_UDP LPAREN ip COMMA NUMBER RPAREN { $$=mk_action(FORWARD_UDP_T, 2, IP_ST, (void*)$3, NUMBER_ST, (void*)$5); set_cfg_pos($$); }
	| FORWARD_UDP LPAREN URIHOST COMMA URIPORT RPAREN {$$=mk_action(FORWARD_UDP_T, 2, URIHOST_ST, 0, URIPORT_ST, 0); set_cfg_pos($$); }
	| FORWARD_UDP LPAREN URIHOST COMMA NUMBER RPAREN { $$=mk_action(FORWARD_UDP_T, 2, URIHOST_ST, 0, NUMBER_ST, (void*)$5); set_cfg_pos($$); }
	| FORWARD_UDP LPAREN URIHOST RPAREN { $$=mk_action(FORWARD_UDP_T, 2, URIHOST_ST, 0, NUMBER_ST, 0); set_cfg_pos($$); }
	| FORWARD_UDP error { $$=0; yyerror("missing '(' or ')' ?"); }
	| FORWARD_UDP LPAREN error RPAREN { $$=0; yyerror("bad forward_udp argument"); }
	| FORWARD_TCP LPAREN host RPAREN	{ $$=mk_action(FORWARD_TCP_T, 2, STRING_ST, $3, NUMBER_ST, 0); set_cfg_pos($$); }
	| FORWARD_TCP LPAREN STRING RPAREN	{ $$=mk_action(FORWARD_TCP_T, 2, STRING_ST, $3, NUMBER_ST, 0); set_cfg_pos($$); }
	| FORWARD_TCP LPAREN ip RPAREN	{ $$=mk_action(FORWARD_TCP_T, 2, IP_ST, (void*)$3, NUMBER_ST, 0); set_cfg_pos($$); }
	| FORWARD_TCP LPAREN host COMMA NUMBER RPAREN { $$=mk_action(FORWARD_TCP_T, 2, STRING_ST, $3, NUMBER_ST, (void*)$5); set_cfg_pos($$); }
	| FORWARD_TCP LPAREN STRING COMMA NUMBER RPAREN {$$=mk_action(FORWARD_TCP_T, 2, STRING_ST, $3, NUMBER_ST, (void*)$5); set_cfg_pos($$); }
	| FORWARD_TCP LPAREN ip COMMA NUMBER RPAREN { $$=mk_action(FORWARD_TCP_T, 2, IP_ST, (void*)$3, NUMBER_ST, (void*)$5); set_cfg_pos($$); }
	| FORWARD_TCP LPAREN URIHOST COMMA URIPORT RPAREN {$$=mk_action(FORWARD_TCP_T, 2, URIHOST_ST, 0, URIPORT_ST, 0); set_cfg_pos($$); }
	| FORWARD_TCP LPAREN URIHOST COMMA NUMBER RPAREN { $$=mk_action(FORWARD_TCP_T, 2, URIHOST_ST, 0, NUMBER_ST, (void*)$5); set_cfg_pos($$); }
	| FORWARD_TCP LPAREN URIHOST RPAREN { $$=mk_action(FORWARD_TCP_T, 2, URIHOST_ST, 0, NUMBER_ST, 0); set_cfg_pos($$); }
	| FORWARD_TCP error { $$=0; yyerror("missing '(' or ')' ?"); }
	| FORWARD_TCP LPAREN error RPAREN { $$=0; yyerror("bad forward_tcp argument"); }
	| FORWARD_TLS LPAREN host RPAREN {
		#ifdef USE_TLS
			$$=mk_action(FORWARD_TLS_T, 2, STRING_ST, $3, NUMBER_ST, 0); set_cfg_pos($$);
		#else
			$$=0;
			yyerror("tls support not compiled in");
		#endif
	}
	| FORWARD_TLS LPAREN STRING RPAREN {
		#ifdef USE_TLS
			$$=mk_action(FORWARD_TLS_T, 2, STRING_ST, $3, NUMBER_ST, 0); set_cfg_pos($$);
		#else
			$$=0;
			yyerror("tls support not compiled in");
		#endif
	}
	| FORWARD_TLS LPAREN ip RPAREN	{
		#ifdef USE_TLS
			$$=mk_action(FORWARD_TLS_T, 2, IP_ST, (void*)$3, NUMBER_ST, 0); set_cfg_pos($$);
		#else
			$$=0;
			yyerror("tls support not compiled in");
		#endif
	}
	| FORWARD_TLS LPAREN host COMMA NUMBER RPAREN {
		#ifdef USE_TLS
			$$=mk_action(FORWARD_TLS_T, 2, STRING_ST, $3, NUMBER_ST, (void*)$5); set_cfg_pos($$);
		#else
			$$=0;
			yyerror("tls support not compiled in");
		#endif
	}
	| FORWARD_TLS LPAREN STRING COMMA NUMBER RPAREN {
		#ifdef USE_TLS
			$$=mk_action(FORWARD_TLS_T, 2, STRING_ST, $3, NUMBER_ST, (void*)$5); set_cfg_pos($$);
		#else
			$$=0;
			yyerror("tls support not compiled in");
		#endif
	}
	| FORWARD_TLS LPAREN ip COMMA NUMBER RPAREN {
		#ifdef USE_TLS
			$$=mk_action(FORWARD_TLS_T, 2, IP_ST, (void*)$3, NUMBER_ST, (void*)$5); set_cfg_pos($$);
		#else
			$$=0;
			yyerror("tls support not compiled in");
		#endif
					}
	| FORWARD_TLS LPAREN URIHOST COMMA URIPORT RPAREN {
		#ifdef USE_TLS
			$$=mk_action(FORWARD_TLS_T, 2, URIHOST_ST, 0, URIPORT_ST, 0); set_cfg_pos($$);
		#else
			$$=0;
			yyerror("tls support not compiled in");
		#endif
	}
	| FORWARD_TLS LPAREN URIHOST COMMA NUMBER RPAREN {
		#ifdef USE_TLS
			$$=mk_action(FORWARD_TLS_T, 2, URIHOST_ST, 0, NUMBER_ST, (void*)$5); set_cfg_pos($$);
		#else
			$$=0;
			yyerror("tls support not compiled in");
		#endif
	}
	| FORWARD_TLS LPAREN URIHOST RPAREN {
		#ifdef USE_TLS
			$$=mk_action(FORWARD_TLS_T, 2, URIHOST_ST, 0, NUMBER_ST, 0); set_cfg_pos($$);
		#else
			$$=0;
			yyerror("tls support not compiled in");
		#endif
	}
	| FORWARD_TLS error { $$=0; yyerror("missing '(' or ')' ?"); }
	| FORWARD_TLS LPAREN error RPAREN { $$=0; 
									yyerror("bad forward_tls argument"); }
	| FORWARD_SCTP LPAREN host RPAREN {
		#ifdef USE_SCTP
			$$=mk_action(FORWARD_SCTP_T, 2, STRING_ST, $3, NUMBER_ST, 0); set_cfg_pos($$);
		#else
			$$=0;
			yyerror("sctp support not compiled in");
		#endif
	}
	| FORWARD_SCTP LPAREN STRING RPAREN {
		#ifdef USE_SCTP
			$$=mk_action(FORWARD_SCTP_T, 2, STRING_ST, $3, NUMBER_ST, 0); set_cfg_pos($$);
		#else
			$$=0;
			yyerror("sctp support not compiled in");
		#endif
	}
	| FORWARD_SCTP LPAREN ip RPAREN	{
		#ifdef USE_SCTP
			$$=mk_action(FORWARD_SCTP_T, 2, IP_ST, (void*)$3, NUMBER_ST, 0); set_cfg_pos($$);
		#else
			$$=0;
			yyerror("sctp support not compiled in");
		#endif
	}
	| FORWARD_SCTP LPAREN host COMMA NUMBER RPAREN {
		#ifdef USE_SCTP
			$$=mk_action(FORWARD_SCTP_T, 2, STRING_ST, $3, NUMBER_ST,
							(void*)$5); set_cfg_pos($$);
		#else
			$$=0;
			yyerror("sctp support not compiled in");
		#endif
	}
	| FORWARD_SCTP LPAREN STRING COMMA NUMBER RPAREN {
		#ifdef USE_SCTP
			$$=mk_action(FORWARD_SCTP_T, 2, STRING_ST, $3, NUMBER_ST,
							(void*)$5); set_cfg_pos($$);
		#else
			$$=0;
			yyerror("sctp support not compiled in");
		#endif
	}
	| FORWARD_SCTP LPAREN ip COMMA NUMBER RPAREN {
		#ifdef USE_SCTP
			$$=mk_action(FORWARD_SCTP_T, 2, IP_ST, (void*)$3, NUMBER_ST, 
							(void*)$5); set_cfg_pos($$);
		#else
			$$=0;
			yyerror("sctp support not compiled in");
		#endif
					}
	| FORWARD_SCTP LPAREN URIHOST COMMA URIPORT RPAREN {
		#ifdef USE_SCTP
			$$=mk_action(FORWARD_SCTP_T, 2, URIHOST_ST, 0, URIPORT_ST, 0); set_cfg_pos($$);
		#else
			$$=0;
			yyerror("sctp support not compiled in");
		#endif
	}
	| FORWARD_SCTP LPAREN URIHOST COMMA NUMBER RPAREN {
		#ifdef USE_SCTP
			$$=mk_action(FORWARD_SCTP_T, 2, URIHOST_ST, 0, NUMBER_ST,
							(void*)$5); set_cfg_pos($$);
		#else
			$$=0;
			yyerror("sctp support not compiled in");
		#endif
	}
	| FORWARD_SCTP LPAREN URIHOST RPAREN {
		#ifdef USE_SCTP
			$$=mk_action(FORWARD_SCTP_T, 2, URIHOST_ST, 0, NUMBER_ST, 0); set_cfg_pos($$);
		#else
			$$=0;
			yyerror("tls support not compiled in");
		#endif
	}
	| FORWARD_SCTP error { $$=0; yyerror("missing '(' or ')' ?"); }
	| FORWARD_SCTP LPAREN error RPAREN { $$=0; 
									yyerror("bad forward_sctp argument"); }
	| LOG_TOK LPAREN STRING RPAREN	{$$=mk_action(LOG_T, 2, NUMBER_ST,
										(void*)(L_DBG+1), STRING_ST, $3);
									set_cfg_pos($$); }
	| LOG_TOK LPAREN NUMBER COMMA STRING RPAREN	{$$=mk_action(LOG_T, 2, NUMBER_ST, (void*)$3, STRING_ST, $5); set_cfg_pos($$); }
	| LOG_TOK error 		{ $$=0; yyerror("missing '(' or ')' ?"); }
	| LOG_TOK LPAREN error RPAREN	{ $$=0; yyerror("bad log argument"); }
	| SETFLAG LPAREN NUMBER RPAREN	{
							if (check_flag($3)==-1)
								yyerror("bad flag value");
							$$=mk_action(SETFLAG_T, 1, NUMBER_ST,
													(void*)$3);
							set_cfg_pos($$);
									}
	| SETFLAG LPAREN flag_name RPAREN	{
							i_tmp=get_flag_no($3, strlen($3));
							if (i_tmp<0) yyerror("flag not declared");
							$$=mk_action(SETFLAG_T, 1, NUMBER_ST,
										(void*)(long)i_tmp);
							set_cfg_pos($$);
									}
	| SETFLAG error			{ $$=0; yyerror("missing '(' or ')'?"); }
	| RESETFLAG LPAREN NUMBER RPAREN {
							if (check_flag($3)==-1)
								yyerror("bad flag value");
							$$=mk_action(RESETFLAG_T, 1, NUMBER_ST, (void*)$3);
							set_cfg_pos($$);
									}
	| RESETFLAG LPAREN flag_name RPAREN	{
							i_tmp=get_flag_no($3, strlen($3));
							if (i_tmp<0) yyerror("flag not declared");
							$$=mk_action(RESETFLAG_T, 1, NUMBER_ST,
										(void*)(long)i_tmp);
							set_cfg_pos($$);
									}
	| RESETFLAG error		{ $$=0; yyerror("missing '(' or ')'?"); }
	| ISFLAGSET LPAREN NUMBER RPAREN {
							if (check_flag($3)==-1)
								yyerror("bad flag value");
							$$=mk_action(ISFLAGSET_T, 1, NUMBER_ST, (void*)$3);
							set_cfg_pos($$);
									}
	| ISFLAGSET LPAREN flag_name RPAREN	{
							i_tmp=get_flag_no($3, strlen($3));
							if (i_tmp<0) yyerror("flag not declared");
							$$=mk_action(ISFLAGSET_T, 1, NUMBER_ST,
										(void*)(long)i_tmp);
							set_cfg_pos($$);
									}
	| ISFLAGSET error { $$=0; yyerror("missing '(' or ')'?"); }
	| avpflag_oper LPAREN attr_id_any_str COMMA flag_name RPAREN {
		i_tmp=get_avpflag_no($5);
		if (i_tmp==0) yyerror("avpflag not declared");
		$$=mk_action(AVPFLAG_OPER_T, 3, AVP_ST, $3, NUMBER_ST, (void*)(long)i_tmp, NUMBER_ST, (void*)$1);
		set_cfg_pos($$);
	}
	| avpflag_oper LPAREN attr_id_any_str COMMA error RPAREN {
		$$=0; yyerror("error parsing flag name");
	}
	| avpflag_oper LPAREN error COMMA flag_name RPAREN {
		$$=0; yyerror("error parsing first parameter (avp or string)");
	}
	| avpflag_oper LPAREN error RPAREN { $$=0; yyerror("bad parameters"); }
	| avpflag_oper error { $$=0; yyerror("missing '(' or ')'?"); }
	| ERROR LPAREN STRING COMMA STRING RPAREN {$$=mk_action(ERROR_T, 2, STRING_ST, $3, STRING_ST, $5);
			set_cfg_pos($$);
	}
	| ERROR error { $$=0; yyerror("missing '(' or ')' ?"); }
	| ERROR LPAREN error RPAREN { $$=0; yyerror("bad error argument"); }
	| ROUTE LPAREN rval_expr RPAREN	{
		if ($3) {
			$$ = mk_action(ROUTE_T, 1, RVE_ST, (void*)$3);
			set_cfg_pos($$);
		} else {
			$$ = 0;
			YYERROR;
		}
	}
	| ROUTE LPAREN ID RPAREN	{
		if ($3) {
			$$ = mk_action(ROUTE_T, 1, STRING_ST, (void*)$3);
			set_cfg_pos($$);
		} else {
			$$ = 0;
			YYERROR;
		}
	}
	| ROUTE error { $$=0; yyerror("missing '(' or ')' ?"); }
	| ROUTE LPAREN error RPAREN { $$=0; yyerror("bad route argument"); }
	| EXEC LPAREN STRING RPAREN	{ $$=mk_action(EXEC_T, 1, STRING_ST, $3); set_cfg_pos($$); }
	| SET_HOST LPAREN STRING RPAREN { $$=mk_action(SET_HOST_T, 1, STRING_ST, $3); set_cfg_pos($$); }
	| SET_HOST error { $$=0; yyerror("missing '(' or ')' ?"); }
	| SET_HOST LPAREN error RPAREN { $$=0; yyerror("bad argument, string expected"); }
	| PREFIX LPAREN STRING RPAREN { $$=mk_action(PREFIX_T, 1, STRING_ST,  $3); set_cfg_pos($$); }
	| PREFIX error { $$=0; yyerror("missing '(' or ')' ?"); }
	| PREFIX LPAREN error RPAREN { $$=0; yyerror("bad argument, string expected"); }
	| STRIP_TAIL LPAREN NUMBER RPAREN { $$=mk_action(STRIP_TAIL_T, 1, NUMBER_ST, (void*)$3); set_cfg_pos($$); }
	| STRIP_TAIL error { $$=0; yyerror("missing '(' or ')' ?"); }
	| STRIP_TAIL LPAREN error RPAREN { $$=0; yyerror("bad argument, number expected"); }
	| STRIP LPAREN NUMBER RPAREN { $$=mk_action(STRIP_T, 1, NUMBER_ST, (void*) $3); set_cfg_pos($$); }
	| STRIP error { $$=0; yyerror("missing '(' or ')' ?"); }
	| STRIP LPAREN error RPAREN { $$=0; yyerror("bad argument, number expected"); }
	| SET_USERPHONE LPAREN RPAREN { $$=mk_action(SET_USERPHONE_T, 0); set_cfg_pos($$); }
	| SET_USERPHONE error { $$=0; yyerror("missing '(' or ')' ?"); }
	| REMOVE_BRANCH LPAREN intno RPAREN {
			$$=mk_action(REMOVE_BRANCH_T, 1, NUMBER_ST, (void*)$3);
			set_cfg_pos($$);
	}
	| REMOVE_BRANCH LPAREN RPAREN {
			$$=mk_action(REMOVE_BRANCH_T, 0);
			set_cfg_pos($$);
	}
	| REMOVE_BRANCH error { $$=0; yyerror("missing '(' or ')' ?"); }
	| REMOVE_BRANCH LPAREN error RPAREN { $$=0; yyerror("bad argument, number expected"); }
	| CLEAR_BRANCHES LPAREN RPAREN { $$=mk_action(CLEAR_BRANCHES_T, 0); set_cfg_pos($$); }
	| SET_HOSTPORT LPAREN STRING RPAREN { $$=mk_action(SET_HOSTPORT_T, 1, STRING_ST, $3); set_cfg_pos($$); }
	| SET_HOSTPORT error { $$=0; yyerror("missing '(' or ')' ?"); }
	| SET_HOSTPORT LPAREN error RPAREN { $$=0; yyerror("bad argument, string expected"); }
	| SET_HOSTPORTTRANS LPAREN STRING RPAREN { $$=mk_action(SET_HOSTPORTTRANS_T, 1, STRING_ST, $3); set_cfg_pos($$); }
	| SET_HOSTPORTTRANS error { $$=0; yyerror("missing '(' or ')' ?"); }
	| SET_HOSTPORTTRANS LPAREN error RPAREN { $$=0; yyerror("bad argument, string expected"); }
	| SET_PORT LPAREN STRING RPAREN { $$=mk_action(SET_PORT_T, 1, STRING_ST, $3); set_cfg_pos($$); }
	| SET_PORT error { $$=0; yyerror("missing '(' or ')' ?"); }
	| SET_PORT LPAREN error RPAREN { $$=0; yyerror("bad argument, string expected"); }
	| SET_USER LPAREN STRING RPAREN { $$=mk_action(SET_USER_T, 1, STRING_ST, $3); set_cfg_pos($$); }
	| SET_USER error { $$=0; yyerror("missing '(' or ')' ?"); }
	| SET_USER LPAREN error RPAREN { $$=0; yyerror("bad argument, string expected"); }
	| SET_USERPASS LPAREN STRING RPAREN { $$=mk_action(SET_USERPASS_T, 1, STRING_ST, $3); set_cfg_pos($$); }
	| SET_USERPASS error { $$=0; yyerror("missing '(' or ')' ?"); }
	| SET_USERPASS LPAREN error RPAREN { $$=0; yyerror("bad argument, string expected"); }
	| SET_URI LPAREN STRING RPAREN { $$=mk_action(SET_URI_T, 1, STRING_ST,$3); set_cfg_pos($$); }
	| SET_URI error { $$=0; yyerror("missing '(' or ')' ?"); }
	| SET_URI LPAREN error RPAREN { $$=0; yyerror("bad argument, string expected"); }
	| REVERT_URI LPAREN RPAREN { $$=mk_action(REVERT_URI_T, 0); set_cfg_pos($$); }
	| REVERT_URI { $$=mk_action(REVERT_URI_T, 0); set_cfg_pos($$); }
	| FORCE_RPORT LPAREN RPAREN	{ $$=mk_action(FORCE_RPORT_T, 0); set_cfg_pos($$); }
	| FORCE_RPORT	{$$=mk_action(FORCE_RPORT_T, 0); set_cfg_pos($$); }
	| ADD_LOCAL_RPORT LPAREN RPAREN	{ $$=mk_action(ADD_LOCAL_RPORT_T, 0); set_cfg_pos($$); }
	| ADD_LOCAL_RPORT	{$$=mk_action(ADD_LOCAL_RPORT_T, 0); set_cfg_pos($$); }
	| FORCE_TCP_ALIAS LPAREN NUMBER RPAREN	{
		#ifdef USE_TCP
			$$=mk_action(FORCE_TCP_ALIAS_T, 1, NUMBER_ST, (void*)$3);
			set_cfg_pos($$);
		#else
			yyerror("tcp support not compiled in");
		#endif
	}
	| FORCE_TCP_ALIAS LPAREN RPAREN	{
		#ifdef USE_TCP
			$$=mk_action(FORCE_TCP_ALIAS_T, 0);
			set_cfg_pos($$);
		#else
			yyerror("tcp support not compiled in");
		#endif
	}
	| FORCE_TCP_ALIAS				{
		#ifdef USE_TCP
			$$=mk_action(FORCE_TCP_ALIAS_T, 0);
			set_cfg_pos($$);
		#else
			yyerror("tcp support not compiled in");
		#endif
	}
	| FORCE_TCP_ALIAS LPAREN error RPAREN	{$$=0; yyerror("bad argument, number expected"); }
	| UDP_MTU_TRY_PROTO LPAREN proto RPAREN
		{ $$=mk_action(UDP_MTU_TRY_PROTO_T, 1, NUMBER_ST, $3); set_cfg_pos($$); }
	| UDP_MTU_TRY_PROTO LPAREN error RPAREN
		{ $$=0; yyerror("bad argument, UDP, TCP, TLS or SCTP expected"); }
	| SET_ADV_ADDRESS LPAREN listen_id RPAREN {
		$$=0;
		if ((str_tmp=pkg_malloc(sizeof(str)))==0) {
			LOG(L_CRIT, "ERROR: cfg. parser: out of memory.\n");
		} else {
			str_tmp->s=$3;
			str_tmp->len=$3?strlen($3):0;
			$$=mk_action(SET_ADV_ADDR_T, 1, STR_ST, str_tmp);
			set_cfg_pos($$);
		}
	}
	| SET_ADV_ADDRESS LPAREN error RPAREN { $$=0; yyerror("bad argument, string expected"); }
	| SET_ADV_ADDRESS error {$$=0; yyerror("missing '(' or ')' ?"); }
	| SET_ADV_PORT LPAREN NUMBER RPAREN {
		$$=0;
		tmp=int2str($3, &i_tmp);
		if ((str_tmp=pkg_malloc(sizeof(str)))==0) {
			LOG(L_CRIT, "ERROR: cfg. parser: out of memory.\n");
		} else {
			if ((str_tmp->s=pkg_malloc(i_tmp))==0) {
				LOG(L_CRIT, "ERROR: cfg. parser: out of memory.\n");
			} else {
				memcpy(str_tmp->s, tmp, i_tmp);
				str_tmp->len=i_tmp;
				$$=mk_action(SET_ADV_PORT_T, 1, STR_ST, str_tmp);
				set_cfg_pos($$);
			}
		}
	}
	| SET_ADV_PORT LPAREN error RPAREN { $$=0; yyerror("bad argument, string expected"); }
	| SET_ADV_PORT  error {$$=0; yyerror("missing '(' or ')' ?"); }
	| FORCE_SEND_SOCKET LPAREN phostport RPAREN { 
		$$=mk_action(FORCE_SEND_SOCKET_T, 1, SOCKID_ST, $3);
		set_cfg_pos($$);
	}
	| FORCE_SEND_SOCKET LPAREN error RPAREN {
		$$=0; yyerror("bad argument, [proto:]host[:port] expected");
	}
	| FORCE_SEND_SOCKET error {$$=0; yyerror("missing '(' or ')' ?"); }
	| SET_FWD_NO_CONNECT LPAREN RPAREN	{
		$$=mk_action(SET_FWD_NO_CONNECT_T, 0); set_cfg_pos($$);
	}
	| SET_FWD_NO_CONNECT	{
		$$=mk_action(SET_FWD_NO_CONNECT_T, 0); set_cfg_pos($$);
	}
	| SET_RPL_NO_CONNECT LPAREN RPAREN	{
		$$=mk_action(SET_RPL_NO_CONNECT_T, 0); set_cfg_pos($$);
	}
	| SET_RPL_NO_CONNECT	{
		$$=mk_action(SET_RPL_NO_CONNECT_T, 0); set_cfg_pos($$);
	}
	| SET_FWD_CLOSE LPAREN RPAREN	{
		$$=mk_action(SET_FWD_CLOSE_T, 0); set_cfg_pos($$);
	}
	| SET_FWD_CLOSE	{
		$$=mk_action(SET_FWD_CLOSE_T, 0); set_cfg_pos($$);
	}
	| SET_RPL_CLOSE LPAREN RPAREN	{
		$$=mk_action(SET_RPL_CLOSE_T, 0); set_cfg_pos($$);
	}
	| SET_RPL_CLOSE	{
		$$=mk_action(SET_RPL_CLOSE_T, 0); set_cfg_pos($$);
	}
	| CFG_SELECT LPAREN STRING COMMA NUMBER RPAREN {
		$$=mk_action(CFG_SELECT_T, 2, STRING_ST, $3, NUMBER_ST, (void*)$5); set_cfg_pos($$);
	}
	| CFG_SELECT LPAREN STRING COMMA rval_expr RPAREN {
		$$=mk_action(CFG_SELECT_T, 2, STRING_ST, $3, RVE_ST, $5); set_cfg_pos($$);
	}
	| CFG_SELECT error { $$=0; yyerror("missing '(' or ')' ?"); }
	| CFG_SELECT LPAREN error RPAREN { $$=0; yyerror("bad arguments, string and number expected"); }
	| CFG_RESET LPAREN STRING RPAREN {
		$$=mk_action(CFG_RESET_T, 1, STRING_ST, $3); set_cfg_pos($$);
	}
	| CFG_RESET error { $$=0; yyerror("missing '(' or ')' ?"); }
	| CFG_RESET LPAREN error RPAREN { $$=0; yyerror("bad arguments, string expected"); }
	| ID {mod_func_action = mk_action(MODULE0_T, 2, MODEXP_ST, NULL, NUMBER_ST,
			0); } LPAREN func_params RPAREN	{
		mod_func_action->val[0].u.data =
			find_export_record($1, mod_func_action->val[1].u.number, rt,
								&u_tmp);
		if (mod_func_action->val[0].u.data == 0) {
			if (find_export_record($1, mod_func_action->val[1].u.number, 0,
									&u_tmp) ) {
					LOG(L_ERR, "misused command %s\n", $1);
					yyerror("Command cannot be used in the block\n");
			} else {
				LOG(L_ERR, "cfg. parser: failed to find command %s (params %ld)\n",
						$1, mod_func_action->val[1].u.number);
				yyerror("unknown command, missing loadmodule?\n");
			}
			free_mod_func_action(mod_func_action);
			mod_func_action=0;
		}else{
			if (mod_func_action && mod_f_params_pre_fixup(mod_func_action)<0) {
				/* error messages are printed inside the function */
				free_mod_func_action(mod_func_action);
				mod_func_action = 0;
				YYERROR;
			}
		}
		$$ = mod_func_action;
		set_cfg_pos($$);
	}
	| ID error					{ yyerror("'('')' expected (function call)");}
	;
func_params:
	/* empty */
	| func_params COMMA func_param { }
	| func_param {}
	;
func_param:
	rval_expr {
		if ($1 && mod_func_action->val[1].u.number < MAX_ACTIONS-2) {
			mod_func_action->val[mod_func_action->val[1].u.number+2].type =
				RVE_ST;
			mod_func_action->val[mod_func_action->val[1].u.number+2].u.data =
				$1;
			mod_func_action->val[1].u.number++;
		} else if ($1) {
			yyerror("Too many arguments\n");
			YYERROR;
		} else {
			YYERROR;
		}
	}
	;

ret_cmd:
	DROP LPAREN RPAREN		{
		$$=mk_action(DROP_T, 2, NUMBER_ST, 0, NUMBER_ST,
						(void*)(DROP_R_F|EXIT_R_F)); set_cfg_pos($$);
	}
	| DROP rval_expr	{
		$$=mk_action(DROP_T, 2, RVE_ST, $2, NUMBER_ST,
						(void*)(DROP_R_F|EXIT_R_F)); set_cfg_pos($$);
	}
	| DROP				{
		$$=mk_action(DROP_T, 2, NUMBER_ST, 0, NUMBER_ST, 
						(void*)(DROP_R_F|EXIT_R_F)); set_cfg_pos($$);
	}
	| EXIT LPAREN RPAREN		{
		$$=mk_action(DROP_T, 2, NUMBER_ST, (void*)1, NUMBER_ST,
						(void*)EXIT_R_F);
		set_cfg_pos($$);
	}
	| EXIT rval_expr	{
		$$=mk_action(DROP_T, 2, RVE_ST, $2, NUMBER_ST, (void*)EXIT_R_F);
		set_cfg_pos($$);
	}
	| EXIT				{
		$$=mk_action(DROP_T, 2, NUMBER_ST, (void*)1, NUMBER_ST,
						(void*)EXIT_R_F);
		set_cfg_pos($$);
	}
	| RETURN			{
		$$=mk_action(DROP_T, 2, NUMBER_ST, (void*)1, NUMBER_ST,
						(void*)RETURN_R_F); set_cfg_pos($$);
	}
	| RETURN  LPAREN RPAREN		{
		$$=mk_action(DROP_T, 2, NUMBER_ST, (void*)1, NUMBER_ST,
						(void*)RETURN_R_F); set_cfg_pos($$);
	}
	| RETURN rval_expr	{
		$$=mk_action(DROP_T, 2, RVE_ST, $2, NUMBER_ST, (void*)RETURN_R_F);
		set_cfg_pos($$);
	}
	| BREAK				{
		$$=mk_action(DROP_T, 2, NUMBER_ST, 0, NUMBER_ST, (void*)BREAK_R_F);
		set_cfg_pos($$);
	}
	;

%%

static void get_cpos(struct cfg_pos* pos)
{
	pos->s_line=startline;
	pos->e_line=line;
	pos->s_col=startcolumn;
	pos->e_col=column-1;
	if(finame==0)
		finame = (cfg_file!=0)?cfg_file:"default";
	pos->fname=finame;
	pos->rname=(routename!=0)?routename:default_routename;
}


static void warn_at(struct cfg_pos* p, char* format, ...)
{
	va_list ap;
	char s[256];
	
	va_start(ap, format);
	vsnprintf(s, sizeof(s), format, ap);
	va_end(ap);
	if (p->e_line!=p->s_line)
		LOG(L_WARN, "warning in config file %s, from line %d, column %d to"
					" line %d, column %d: %s\n",
					p->fname, p->s_line, p->s_col, p->e_line, p->e_col, s);
	else if (p->s_col!=p->e_col)
		LOG(L_WARN, "warning in config file %s, line %d, column %d-%d: %s\n",
					p->fname, p->s_line, p->s_col, p->e_col, s);
	else
		LOG(L_WARN, "warning in config file %s, line %d, column %d: %s\n",
				p->fname, p->s_line, p->s_col, s);
	cfg_warnings++;
}



static void yyerror_at(struct cfg_pos* p, char* format, ...)
{
	va_list ap;
	char s[256];
	
	va_start(ap, format);
	vsnprintf(s, sizeof(s), format, ap);
	va_end(ap);
	if (p->e_line!=p->s_line)
		LOG(L_CRIT, "parse error in config file %s, from line %d, column %d"
					" to line %d, column %d: %s\n",
					p->fname, p->s_line, p->s_col, p->e_line, p->e_col, s);
	else if (p->s_col!=p->e_col)
		LOG(L_CRIT,"parse error in config file %s, line %d, column %d-%d: %s\n",
					p->fname, p->s_line, p->s_col, p->e_col, s);
	else
		LOG(L_CRIT, "parse error in config file %s, line %d, column %d: %s\n",
					p->fname, p->s_line, p->s_col, s);
	cfg_errors++;
}



static void warn(char* format, ...)
{
	va_list ap;
	char s[256];
	struct cfg_pos pos;
	
	get_cpos(&pos);
	va_start(ap, format);
	vsnprintf(s, sizeof(s), format, ap);
	va_end(ap);
	warn_at(&pos, s);
}



static void yyerror(char* format, ...)
{
	va_list ap;
	char s[256];
	struct cfg_pos pos;
	
	get_cpos(&pos);
	va_start(ap, format);
	vsnprintf(s, sizeof(s), format, ap);
	va_end(ap);
	yyerror_at(&pos, s);
}



/** mk_rval_expr_v wrapper.
 *  checks mk_rval_expr_v return value and sets the cfg. pos
 *  (line and column numbers)
 *  @return rval_expr* on success, 0 on error (@see mk_rval_expr_v)
 */
static struct rval_expr* mk_rve_rval(enum rval_type type, void* v)
{
	struct rval_expr* ret;
	struct cfg_pos pos;

	get_cpos(&pos);
	ret=mk_rval_expr_v(type, v, &pos);
	if (ret==0){
		yyerror("internal error: failed to create rval expr");
		/* YYABORT; */
	}
	return ret;
}


/** mk_rval_expr1 wrapper.
 *  checks mk_rval_expr1 return value (!=0 and type checking)
 *  @return rval_expr* on success, 0 on error (@see mk_rval_expr1)
 */
static struct rval_expr* mk_rve1(enum rval_expr_op op, struct rval_expr* rve1)
{
	struct rval_expr* ret;
	struct rval_expr* bad_rve;
	enum rval_type type, bad_t, exp_t;
	
	if (rve1==0)
		return 0;
	ret=mk_rval_expr1(op, rve1, &rve1->fpos);
	if (ret && (rve_check_type(&type, ret, &bad_rve, &bad_t, &exp_t)!=1)){
		yyerror_at(&rve1->fpos, "bad expression: type mismatch"
					" (%s instead of %s)", rval_type_name(bad_t),
					rval_type_name(exp_t));
		rve_destroy(ret);
		ret=0;
	}
	return ret;
}


/** mk_rval_expr2 wrapper.
 *  checks mk_rval_expr2 return value (!=0 and type checking)
 *  @return rval_expr* on success, 0 on error (@see mk_rval_expr2)
 */
static struct rval_expr* mk_rve2(enum rval_expr_op op, struct rval_expr* rve1,
									struct rval_expr* rve2)
{
	struct rval_expr* ret;
	struct rval_expr* bad_rve;
	enum rval_type type, bad_t, exp_t;
	struct cfg_pos pos;
	
	if ((rve1==0) || (rve2==0))
		return 0;
	bad_rve=0;
	bad_t=0;
	exp_t=0;
	cfg_pos_join(&pos, &rve1->fpos, &rve2->fpos);
	ret=mk_rval_expr2(op, rve1, rve2, &pos);
	if (ret && (rve_check_type(&type, ret, &bad_rve, &bad_t, &exp_t)!=1)){
		if (bad_rve)
			yyerror_at(&pos, "bad expression: type mismatch:"
						" %s instead of %s at (%d,%d)",
						rval_type_name(bad_t), rval_type_name(exp_t),
						bad_rve->fpos.s_line, bad_rve->fpos.s_col);
		else
			yyerror("BUG: unexpected null \"bad\" expression\n");
		rve_destroy(ret);
		ret=0;
	}
	return ret;
}


/** check if the expression is an int.
 * if the expression does not evaluate to an int return -1 and
 * log an error.
 * @return 0 success, no warnings; 1 success but warnings; -1 on error */
static int rval_expr_int_check(struct rval_expr *rve)
{
	struct rval_expr* bad_rve;
	enum rval_type type, bad_t, exp_t;
	
	if (rve==0){
		yyerror("invalid expression");
		return -1;
	}else if (!rve_check_type(&type, rve, &bad_rve, &bad_t ,&exp_t)){
		if (bad_rve)
			yyerror_at(&rve->fpos, "bad expression: type mismatch:"
						" %s instead of %s at (%d,%d)",
						rval_type_name(bad_t), rval_type_name(exp_t),
						bad_rve->fpos.s_line, bad_rve->fpos.s_col);
		else
			yyerror("BUG: unexpected null \"bad\" expression\n");
		return -1;
	}else if (type!=RV_INT && type!=RV_NONE){
		warn_at(&rve->fpos, "non-int expression (you might want to use"
				" casts)\n");
		return 1;
	}
	return 0;
}


/** warn if the expression is constant.
 * @return 0 on success (no warning), 1 when warning */
static int warn_ct_rve(struct rval_expr *rve, char* name)
{
	if (rve && rve_is_constant(rve)){
		warn_at(&rve->fpos, "constant value in %s%s",
				name?name:"expression", name?"(...)":"");
		return 1;
	}
	return 0;
}


static struct name_lst* mk_name_lst(char* host, int flags)
{
	struct name_lst* l;
	if (host==0) return 0;
	l=pkg_malloc(sizeof(struct name_lst));
	if (l==0) {
		LOG(L_CRIT,"ERROR: cfg. parser: out of memory.\n");
	} else {
		l->name=host;
		l->flags=flags;
		l->next=0;
	}
	return l;
}


static struct socket_id* mk_listen_id(char* host, int proto, int port)
{
	struct socket_id* l;
	if (host==0) return 0;
	l=pkg_malloc(sizeof(struct socket_id));
	if (l==0) {
		LOG(L_CRIT,"ERROR: cfg. parser: out of memory.\n");
	} else {
		l->addr_lst=mk_name_lst(host, 0);
		if (l->addr_lst==0){
			pkg_free(l);
			return 0;
		}
		l->flags=0;
		l->port=port;
		l->proto=proto;
		l->next=0;
	}
	return l;
}


static void free_name_lst(struct name_lst* lst)
{
	struct name_lst* tmp;
	
	while(lst){
		tmp=lst;
		lst=lst->next;
		pkg_free(tmp);
	}
}


static struct socket_id* mk_listen_id2(struct name_lst* addr_l, int proto,
										int port)
{
	struct socket_id* l;
	if (addr_l==0) return 0;
	l=pkg_malloc(sizeof(struct socket_id));
	if (l==0) {
		LOG(L_CRIT,"ERROR: cfg. parser: out of memory.\n");
	} else {
		l->flags=addr_l->flags;
		l->port=port;
		l->proto=proto;
		l->addr_lst=addr_l;
		l->next=0;
	}
	return l;
}


static void free_socket_id(struct socket_id* i)
{
	free_name_lst(i->addr_lst);
	pkg_free(i);
}


static void free_socket_id_lst(struct socket_id* lst)
{
	struct socket_id* tmp;
	
	while(lst){
		tmp=lst;
		lst=lst->next;
		free_socket_id(tmp);
	}
}


/** create a temporary case statmenet structure.
 *  *err will be filled in case of error (return == 0):
 *   -1 - non constant expression
 *   -2 - expression error (bad type)
 *   -10 - memory allocation error
 */
static struct case_stms* mk_case_stm(struct rval_expr* ct, int is_re,
											struct action* a, int* err)
{
	struct case_stms* s;
	struct rval_expr* bad_rve;
	enum rval_type type, bad_t, exp_t;
	enum match_str_type t;
	
	t=MATCH_UNKNOWN;
	if (ct){
		/* if ct!=0 => case, else if ct==0 is a default */
		if (!rve_is_constant(ct)){
			yyerror_at(&ct->fpos, "non constant expression in case");
			*err=-1;
			return 0;
		}
		if (rve_check_type(&type, ct, &bad_rve, &bad_t, &exp_t)!=1){
			yyerror_at(&ct->fpos, "bad expression: type mismatch:"
							" %s instead of %s at (%d,%d)",
							rval_type_name(bad_t), rval_type_name(exp_t),
							bad_rve->fpos.s_line, bad_rve->fpos.s_col);
			*err=-2;
			return 0;
		}
		if (is_re)
			t=MATCH_RE;
		else if (type==RV_STR)
			t=MATCH_STR;
		else
			t=MATCH_INT;
	}

	s=pkg_malloc(sizeof(*s));
	if (s==0) {
		yyerror("internal error: memory allocation failure");
		*err=-10;
	} else {
		memset(s, 0, sizeof(*s));
		s->ct_rve=ct;
		s->type=t;
		s->actions=a;
		s->next=0;
		s->append=0;
	}
	return s;
}


/*
 * @return 0 on success, -1 on error.
 */
static int case_check_type(struct case_stms* stms)
{
	struct case_stms* c;
	struct case_stms* s;
	
	for(c=stms; c ; c=c->next){
		if (!c->ct_rve) continue;
		for (s=c->next; s; s=s->next){
			if (!s->ct_rve) continue;
			if ((s->type!=c->type) &&
				!(	(c->type==MATCH_STR || c->type==MATCH_RE) &&
					(s->type==MATCH_STR || s->type==MATCH_RE) ) ){
					yyerror_at(&s->ct_rve->fpos, "type mismatch in case");
					return -1;
			}
		}
	}
	return 0;
}


/*
 * @return 0 on success, -1 on error.
 */
static int case_check_default(struct case_stms* stms)
{
	struct case_stms* c;
	int default_no;
	
	default_no=0;
	for(c=stms; c ; c=c->next)
		if (c->ct_rve==0) default_no++;
	return (default_no<=1)?0:-1;
}



/** fixes the parameters and the type of a module function call.
 * It is done here instead of fix action, to have quicker feedback
 * on error cases (e.g. passing a non constant to a function with a 
 * declared fixup) 
 * The rest of the fixup is done inside do_action().
 * @param a - filled module function call (MODULE*_T) action structure
 *            complete with parameters, starting at val[2] and parameter
 *            number at val[1].
 * @return 0 on success, -1 on error (it will also print the error msg.).
 *
 */
static int mod_f_params_pre_fixup(struct action* a)
{
	sr31_cmd_export_t* cmd_exp;
	action_u_t* params;
	int param_no;
	struct rval_expr* rve;
	struct rvalue* rv;
	int r;
	str s;
	
	cmd_exp = a->val[0].u.data;
	param_no = a->val[1].u.number;
	params = &a->val[2];
	
	switch(cmd_exp->param_no) {
		case 0:
			a->type = MODULE0_T;
			break;
		case 1:
			a->type = MODULE1_T;
			break;
		case 2:
			a->type = MODULE2_T;
			break;
		case 3:
			a->type = MODULE3_T;
			break;
		case 4:
			a->type = MODULE4_T;
			break;
		case 5:
			a->type = MODULE5_T;
			break;
		case 6:
			a->type = MODULE6_T;
			break;
		case VAR_PARAM_NO:
			a->type = MODULEX_T;
			break;
		default:
			yyerror("function %s: bad definition"
					" (invalid number of parameters)", cmd_exp->name);
			return -1;
	}
	
	if ( cmd_exp->fixup) {
		if (is_fparam_rve_fixup(cmd_exp->fixup))
			/* mark known fparam rve safe fixups */
			cmd_exp->fixup_flags  |= FIXUP_F_FPARAM_RVE;
		else if (!(cmd_exp->fixup_flags & FIXUP_F_FPARAM_RVE) &&
				 cmd_exp->free_fixup == 0) {
			/* v0 or v1 functions that have fixups and no coresp. fixup_free
			   functions, need constant, string params.*/
			for (r=0; r < param_no; r++) {
				rve=params[r].u.data;
				if (!rve_is_constant(rve)) {
					yyerror_at(&rve->fpos, "function %s: parameter %d is not"
								" constant\n", cmd_exp->name, r+1);
					return -1;
				}
				if ((rv = rval_expr_eval(0, 0, rve)) == 0 ||
						rval_get_str(0, 0, &s, rv, 0) < 0 ) {
					/* out of mem or bug ? */
					rval_destroy(rv);
					yyerror_at(&rve->fpos, "function %s: bad parameter %d"
									" expression\n", cmd_exp->name, r+1);
					return -1;
				}
				rval_destroy(rv);
				rve_destroy(rve);
				params[r].type = STRING_ST; /* asciiz */
				params[r].u.string = s.s;
				params[r].u.str.len = s.len; /* not used right now */
			}
		}
	}/* else
		if no fixups are present, the RVEs can be transformed
		into strings at runtime, allowing seamless var. use
		even with old functions.
		Further optimizations -> in fix_actions()
		*/
	return 0;
}



/** frees a filled module function call action structure.
 * @param a - filled module function call action structure
 *            complete with parameters, starting at val[2] and parameter
 *            number at val[1].
 */
static void free_mod_func_action(struct action* a)
{
	action_u_t* params;
	int param_no;
	int r;
	
	param_no = a->val[1].u.number;
	params = &a->val[2];
	
	for (r=0; r < param_no; r++)
		if (params[r].u.data)
			rve_destroy(params[r].u.data);
	pkg_free(a);
}



/*
int main(int argc, char ** argv)
{
	if (yyparse()!=0)
		fprintf(stderr, "parsing error\n");
}
*/
