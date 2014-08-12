/* $Id$
 *
 * Copyright (C) 2003-2008 Sippy Software, Inc., http://www.sippysoft.com
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * ---------
 * 2003-10-09	nat_uac_test introduced (jiri)
 *
 * 2003-11-06   nat_uac_test permitted from onreply_route (jiri)
 *
 * 2003-12-01   unforce_rtp_proxy introduced (sobomax)
 *
 * 2004-01-07	RTP proxy support updated to support new version of the
 *		RTP proxy (20040107).
 *
 *		force_rtp_proxy() now inserts a special flag
 *		into the SDP body to indicate that this session already
 *		proxied and ignores sessions with such flag.
 *
 *		Added run-time check for version of command protocol
 *		supported by the RTP proxy.
 *
 * 2004-01-16   Integrated slightly modified patch from Tristan Colgate,
 *		force_rtp_proxy function with IP as a parameter (janakj)
 *
 * 2004-01-28	nat_uac_test extended to allow testing SDP body (sobomax)
 *
 *		nat_uac_test extended to allow testing top Via (sobomax)
 *
 * 2004-02-21	force_rtp_proxy now accepts option argument, which
 *		consists of string of chars, each of them turns "on"
 *		some feature, currently supported ones are:
 *
 *		 `a' - flags that UA from which message is received
 *		       doesn't support symmetric RTP;
 *		 `l' - force "lookup", that is, only rewrite SDP when
 *		       corresponding session is already exists in the
 *		       RTP proxy. Only makes sense for SIP requests,
 *		       replies are always processed in "lookup" mode;
 *		 `i' - flags that message is received from UA in the
 *		       LAN. Only makes sense when RTP proxy is running
 *		       in the bridge mode.
 *
 *		force_rtp_proxy can now be invoked without any arguments,
 *		as previously, with one argument - in this case argument
 *		is treated as option string and with two arguments, in
 *		which case 1st argument is option string and the 2nd
 *		one is IP address which have to be inserted into
 *		SDP (IP address on which RTP proxy listens).
 *
 * 2004-03-12	Added support for IPv6 addresses in SDPs. Particularly,
 *		force_rtp_proxy now can work with IPv6-aware RTP proxy,
 *		replacing IPv4 address in SDP with IPv6 one and vice versa.
 *		This allows creating full-fledged IPv4<->IPv6 gateway.
 *		See 4to6.cfg file for example.
 *
 *		Two new options added into force_rtp_proxy:
 *
 *		 `f' - instructs nathelper to ignore marks inserted
 *		       by another nathelper in transit to indicate
 *		       that the session is already goes through another
 *		       proxy. Allows creating chain of proxies.
 *		 `r' - flags that IP address in SDP should be trusted.
 *		       Without this flag, nathelper ignores address in the
 *		       SDP and uses source address of the SIP message
 *		       as media address which is passed to the RTP proxy.
 *
 *		Protocol between nathelper and RTP proxy in bridge
 *		mode has been slightly changed. Now RTP proxy expects SER
 *		to provide 2 flags when creating or updating session
 *		to indicate direction of this session. Each of those
 *		flags can be either `e' or `i'. For example `ei' means
 *		that we received INVITE from UA on the "external" network
 *		network and will send it to the UA on "internal" one.
 *		Also possible `ie' (internal->external), `ii'
 *		(internal->internal) and `ee' (external->external). See
 *		example file alg.cfg for details.
 *
 * 2004-03-15	If the rtp proxy test failed (wrong version or not started)
 *		retry test from time to time, when some *rtpproxy* function
 *		is invoked. Minimum interval between retries can be
 *		configured via rtpproxy_disable_tout module parameter (default
 *		is 60 seconds). Setting it to -1 will disable periodic
 *		rechecks completely, setting it to 0 will force checks
 *		for each *rtpproxy* function call. (andrei)
 *
 * 2004-03-22	Fix assignment of rtpproxy_retr and rtpproxy_tout module
 *		parameters.
 *
 * 2004-03-22	Fix get_body position (should be called before get_callid)
 * 				(andrei)
 *
 * 2004-03-24	Fix newport for null ip address case (e.g onhold re-INVITE)
 * 				(andrei)
 *
 * 2004-09-30	added received port != via port test (andrei)
 *
 * 2004-10-10   force_socket option introduced (jiri)
 *
 * 2005-02-24	Added support for using more than one rtp proxy, in which
 *		case traffic will be distributed evenly among them. In addition,
 *		each such proxy can be assigned a weight, which will specify
 *		which share of the traffic should be placed to this particular
 *		proxy.
 *
 *		Introduce failover mechanism, so that if SER detects that one
 *		of many proxies is no longer available it temporarily decreases
 *		its weight to 0, so that no traffic will be assigned to it.
 *		Such "disabled" proxies are periodically checked to see if they
 *		are back to normal in which case respective weight is restored
 *		resulting in traffic being sent to that proxy again.
 *
 *		Those features can be enabled by specifying more than one "URI"
 *		in the rtpproxy_sock parameter, optionally followed by the weight,
 *		which if absent is assumed to be 1, for example:
 *
 *		rtpproxy_sock="unix:/foo/bar=4 udp:1.2.3.4:3456=3 udp:5.6.7.8:5432=1"
 *
 * 2005-02-25	Force for pinging the socket returned by USRLOC (bogdan)
 *
 * 2005-03-22	support for multiple media streams added (netch)
 *
 * 2005-07-11  SIP ping support added (bogdan)
 *
 * 2005-07-14  SDP origin (o=) IP may be also changed (bogdan)
 *
 * 2006-03-08  fix_nated_sdp() may take one more param to force a specific IP;
 *             force_rtp_proxy() accepts a new flag 's' to swap creation/
 *              confirmation between requests/replies; 
 *             add_rcv_param() may take as parameter a flag telling if the
 *              parameter should go to the contact URI or contact header;
 *             (bogdan)
 * 2006-03-28 Support for changing session-level SDP connection (c=) IP when
 *            media-description also includes connection information (bayan)
 * 2007-04-13 Support multiple sets of rtpproxies and set selection added
 *            (ancuta)
 * 2007-04-26 Added some MI commands:
 *             nh_enable_ping used to enable or disable natping
 *             nh_enable_rtpp used to enable or disable a specific rtp proxy
 *             nh_show_rtpp   used to display information for all rtp proxies 
 *             (ancuta)
 * 2007-05-09 New function start_recording() allowing to start recording RTP 
 *             session in the RTP proxy (Carsten Bock - ported from SER)
 * 2007-09-11 Separate timer process and support for multiple timer processes
 *             (bogdan)
 * 2008-12-12 Support for RTCP attribute in the SDP
 *              (Min Wang/BASIS AudioNet - ported from SER)
 * 2010-08-05 Core SDP parser integrated into nathelper (osas)
 * 2010-10-08 Removal of deprecated force_rtp_proxy and swap flag (osas)
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#ifndef __USE_BSD
#define  __USE_BSD
#endif
#include <netinet/ip.h>
#ifndef __FAVOR_BSD
#define __FAVOR_BSD
#endif
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../flags.h"
#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../data_lump.h"
#include "../../data_lump_rpl.h"
#include "../../error.h"
#include "../../forward.h"
#include "../../mem/mem.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parser_f.h"
#include "../../parser/sdp/sdp.h"
#include "../../resolve.h"
#include "../../timer.h"
#include "../../trim.h"
#include "../../ut.h"
#include "../../pt.h"
#include "../../timer_proc.h"
#include "../../lib/kmi/mi.h"
#include "../../pvar.h"
#include "../../lvalue.h"
#include "../../msg_translator.h"
#include "../../usr_avp.h"
#include "../../socket_info.h"
#include "../../mod_fix.h"
#include "../../dset.h"
#include "../../route.h"
#include "../../modules/tm/tm_load.h"
#include "rtpproxy.h"
#include "rtpproxy_funcs.h"
#include "rtpproxy_stream.h"
 
MODULE_VERSION

#if !defined(AF_LOCAL)
#define	AF_LOCAL AF_UNIX
#endif
#if !defined(PF_LOCAL)
#define	PF_LOCAL PF_UNIX
#endif

/* NAT UAC test constants */
#define	NAT_UAC_TEST_C_1918	0x01
#define	NAT_UAC_TEST_RCVD	0x02
#define	NAT_UAC_TEST_V_1918	0x04
#define	NAT_UAC_TEST_S_1918	0x08
#define	NAT_UAC_TEST_RPORT	0x10


#define DEFAULT_RTPP_SET_ID		0
static str DEFAULT_RTPP_SET_ID_STR = str_init("0");

#define MI_SET_NATPING_STATE		"nh_enable_ping"
#define MI_DEFAULT_NATPING_STATE	1

#define MI_ENABLE_RTP_PROXY			"nh_enable_rtpp"
#define MI_MIN_RECHECK_TICKS		0
#define MI_MAX_RECHECK_TICKS		(unsigned int)-1

#define MI_SHOW_RTP_PROXIES			"nh_show_rtpp"

#define MI_RTP_PROXY_NOT_FOUND		"RTP proxy not found"
#define MI_RTP_PROXY_NOT_FOUND_LEN	(sizeof(MI_RTP_PROXY_NOT_FOUND)-1)
#define MI_PING_DISABLED			"NATping disabled from script"
#define MI_PING_DISABLED_LEN		(sizeof(MI_PING_DISABLED)-1)
#define MI_SET						"set"
#define MI_SET_LEN					(sizeof(MI_SET)-1)
#define MI_INDEX					"index"
#define MI_INDEX_LEN				(sizeof(MI_INDEX)-1)
#define MI_DISABLED					"disabled"
#define MI_DISABLED_LEN				(sizeof(MI_DISABLED)-1)
#define MI_WEIGHT					"weight"
#define MI_WEIGHT_LEN				(sizeof(MI_WEIGHT)-1)
#define MI_RECHECK_TICKS			"recheck_ticks"
#define MI_RECHECK_T_LEN			(sizeof(MI_RECHECK_TICKS)-1)



/* Supported version of the RTP proxy command protocol */
#define	SUP_CPROTOVER	20040107
/* Required additional version of the RTP proxy command protocol */
#define	REQ_CPROTOVER	"20050322"
/* Additional version necessary for re-packetization support */
#define	REP_CPROTOVER	"20071116"
#define	PTL_CPROTOVER	"20081102"

#define	CPORT		"22222"
static int extract_mediaip(str *, str *, int *, char *);
static int alter_mediaip(struct sip_msg *, str *, str *, int, str *, int, int);
static int alter_mediaport(struct sip_msg *, str *, str *, str *, int);
static int alter_rtcp(struct sip_msg *msg, str *body, str *oldport, str *newport);
static char *gencookie();
static int rtpp_test(struct rtpp_node*, int, int);
static int unforce_rtp_proxy1_f(struct sip_msg *, char *, char *);
static int unforce_rtp_proxy(struct sip_msg *, char *);
static int force_rtp_proxy(struct sip_msg *, char *, char *, int, int);
static int start_recording_f(struct sip_msg *, char *, char *);
static int rtpproxy_answer1_f(struct sip_msg *, char *, char *);
static int rtpproxy_answer2_f(struct sip_msg *, char *, char *);
static int rtpproxy_offer1_f(struct sip_msg *, char *, char *);
static int rtpproxy_offer2_f(struct sip_msg *, char *, char *);
static int rtpproxy_manage0(struct sip_msg *msg, char *flags, char *ip);
static int rtpproxy_manage1(struct sip_msg *msg, char *flags, char *ip);
static int rtpproxy_manage2(struct sip_msg *msg, char *flags, char *ip);

static int add_rtpproxy_socks(struct rtpp_set * rtpp_list, char * rtpproxy);
static int fixup_set_id(void ** param, int param_no);
static int set_rtp_proxy_set_f(struct sip_msg * msg, char * str1, char * str2);
static struct rtpp_set * select_rtpp_set(int id_set);

static int rtpproxy_set_store(modparam_t type, void * val);
static int rtpproxy_add_rtpproxy_set( char * rtp_proxies);

static int mod_init(void);
static int child_init(int);
static void mod_destroy(void);

/* Pseudo-Variables */
static int pv_get_rtpstat_f(struct sip_msg *, pv_param_t *, pv_value_t *);

/*mi commands*/
static struct mi_root* mi_enable_rtp_proxy(struct mi_root* cmd_tree,
		void* param );
static struct mi_root* mi_show_rtpproxies(struct mi_root* cmd_tree,
		void* param);


static int rtpproxy_disable_tout = 60;
static int rtpproxy_retr = 5;
static int rtpproxy_tout = 1;
static pid_t mypid;
static unsigned int myseqn = 0;
static str nortpproxy_str = str_init("a=nortpproxy:yes");
static str extra_id_pv_param = {NULL, 0};

static char ** rtpp_strings=0;
static int rtpp_sets=0; /*used in rtpproxy_set_store()*/
static int rtpp_set_count = 0;
static unsigned int current_msg_id = (unsigned int)-1;
/* RTP proxy balancing list */
struct rtpp_set_head * rtpp_set_list =0;
struct rtpp_set * selected_rtpp_set =0;
struct rtpp_set * default_rtpp_set=0;
static char *ice_candidate_priority_avp_param = NULL;
static int ice_candidate_priority_avp_type;
static int_str ice_candidate_priority_avp;
static str rtp_inst_pv_param = {NULL, 0};
static pv_spec_t *rtp_inst_pvar = NULL;

/* array with the sockets used by rtpporxy (per process)*/
static unsigned int rtpp_no = 0;
static int *rtpp_socks = 0;


typedef struct rtpp_set_link {
	struct rtpp_set *rset;
	pv_spec_t *rpv;
} rtpp_set_link_t;

/* tm */
static struct tm_binds tmb;

/*0-> disabled, 1 ->enabled*/
unsigned int *natping_state=0;

static str timeout_socket_str = {0, 0};
static pv_elem_t *extra_id_pv = NULL;

static cmd_export_t cmds[] = {
	{"set_rtp_proxy_set",  (cmd_function)set_rtp_proxy_set_f,    1,
		fixup_set_id, 0,
		ANY_ROUTE},
	{"unforce_rtp_proxy",  (cmd_function)unforce_rtp_proxy1_f,   0,
		0, 0,
		ANY_ROUTE},
	{"rtpproxy_destroy",   (cmd_function)unforce_rtp_proxy1_f,   0,
		0, 0,
		ANY_ROUTE},
	{"unforce_rtp_proxy",  (cmd_function)unforce_rtp_proxy1_f,   1,
		fixup_spve_null, 0,
		ANY_ROUTE},
	{"rtpproxy_destroy",   (cmd_function)unforce_rtp_proxy1_f,   1,
		fixup_spve_null, 0,
		ANY_ROUTE},
	{"start_recording",    (cmd_function)start_recording_f,      0,
		0, 0,
		ANY_ROUTE },
	{"rtpproxy_offer",	(cmd_function)rtpproxy_offer1_f,     0,
		0, 0,
		ANY_ROUTE},
	{"rtpproxy_offer",	(cmd_function)rtpproxy_offer1_f,     1,
		fixup_spve_null, 0,
		ANY_ROUTE},
	{"rtpproxy_offer",	(cmd_function)rtpproxy_offer2_f,     2,
		fixup_spve_spve, 0,
		ANY_ROUTE},
	{"rtpproxy_answer",	(cmd_function)rtpproxy_answer1_f,    0,
		0, 0,
		ANY_ROUTE},
	{"rtpproxy_answer",	(cmd_function)rtpproxy_answer1_f,    1,
		fixup_spve_null, 0,
		ANY_ROUTE},
	{"rtpproxy_answer",	(cmd_function)rtpproxy_answer2_f,    2,
		fixup_spve_spve, 0,
		ANY_ROUTE},
	{"rtpproxy_stream2uac",(cmd_function)rtpproxy_stream2uac2_f, 2,
		fixup_var_str_int, 0,
		ANY_ROUTE },
	{"rtpproxy_stream2uas",(cmd_function)rtpproxy_stream2uas2_f, 2,
		fixup_var_str_int, 0,
		ANY_ROUTE },
	{"rtpproxy_stop_stream2uac",(cmd_function)rtpproxy_stop_stream2uac2_f,0,
		NULL, 0,
		ANY_ROUTE },
	{"rtpproxy_stop_stream2uas",(cmd_function)rtpproxy_stop_stream2uas2_f,0,
		NULL, 0,
		ANY_ROUTE },
	{"rtpproxy_manage",	(cmd_function)rtpproxy_manage0,     0,
		0, 0,
		ANY_ROUTE},
	{"rtpproxy_manage",	(cmd_function)rtpproxy_manage1,     1,
		fixup_spve_null, fixup_free_spve_null,
		ANY_ROUTE},
	{"rtpproxy_manage",	(cmd_function)rtpproxy_manage2,     2,
		fixup_spve_spve, fixup_free_spve_spve,
		ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static pv_export_t mod_pvs[] = {
    {{"rtpstat", (sizeof("rtpstat")-1)}, /* RTP-Statistics */
     PVT_OTHER, pv_get_rtpstat_f, 0, 0, 0, 0, 0},
    {{0, 0}, 0, 0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
	{"nortpproxy_str",        PARAM_STR, &nortpproxy_str      },
	{"rtpproxy_sock",         PARAM_STRING|USE_FUNC_PARAM,
	                         (void*)rtpproxy_set_store          },
	{"rtpproxy_disable_tout", INT_PARAM, &rtpproxy_disable_tout },
	{"rtpproxy_retr",         INT_PARAM, &rtpproxy_retr         },
	{"rtpproxy_tout",         INT_PARAM, &rtpproxy_tout         },
	{"timeout_socket",    	  PARAM_STR, &timeout_socket_str  },
	{"ice_candidate_priority_avp", PARAM_STRING,
	 &ice_candidate_priority_avp_param},
	{"extra_id_pv",           PARAM_STR, &extra_id_pv_param },
	{"db_url",                PARAM_STR, &rtpp_db_url },
	{"table_name",            PARAM_STR, &rtpp_table_name },
	{"rtp_inst_pvar",         PARAM_STR, &rtp_inst_pv_param },
	{0, 0, 0}
};

static mi_export_t mi_cmds[] = {
	{MI_ENABLE_RTP_PROXY,     mi_enable_rtp_proxy,  0,                0, 0},
	{MI_SHOW_RTP_PROXIES,     mi_show_rtpproxies,   MI_NO_INPUT_FLAG, 0, 0},
	{ 0, 0, 0, 0, 0}
};


struct module_exports exports = {
	"rtpproxy",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,           /* exported statistics */
	mi_cmds,     /* exported MI functions */
	mod_pvs,     /* exported pseudo-variables */
	0,           /* extra processes */
	mod_init,
	0,           /* reply processing */
	mod_destroy, /* destroy function */
	child_init
};


static int rtpproxy_set_store(modparam_t type, void * val){

	char * p;
	int len;

	p = (char* )val;

	if(p==0 || *p=='\0'){
		return 0;
	}

	if(rtpp_sets==0){
		rtpp_strings = (char**)pkg_malloc(sizeof(char*));
		if(!rtpp_strings){
			LM_ERR("no pkg memory left\n");
			return -1;
		}
	} else {/*realloc to make room for the current set*/
		rtpp_strings = (char**)pkg_realloc(rtpp_strings, 
										  (rtpp_sets+1)* sizeof(char*));
		if(!rtpp_strings){
			LM_ERR("no pkg memory left\n");
			return -1;
		}
	}
	
	/*allocate for the current set of urls*/
	len = strlen(p);
	rtpp_strings[rtpp_sets] = (char*)pkg_malloc((len+1)*sizeof(char));

	if(!rtpp_strings[rtpp_sets]){
		LM_ERR("no pkg memory left\n");
		return -1;
	}
	
	memcpy(rtpp_strings[rtpp_sets], p, len);
	rtpp_strings[rtpp_sets][len] = '\0';
	rtpp_sets++;

	return 0;
}

struct rtpp_set *get_rtpp_set(str *const set_name)
{
	unsigned int this_set_id;
	struct rtpp_set *rtpp_list;
	if (rtpp_set_list == NULL)
	{
		LM_ERR("rtpp set list not configured\n");
		return NULL;
	}
	/* Only integer set_names are valid at the moment */
	if ((set_name->s == NULL) || (set_name->len == 0))
	{
		LM_ERR("Invalid set name '%.*s'\n", set_name->len, set_name->s);
		return NULL;
	}
	if (str2int(set_name, &this_set_id) < 0)
	{
		LM_ERR("Invalid set name '%.*s' - must be integer\n", set_name->len, set_name->s);
		return NULL;
	}

	rtpp_list = select_rtpp_set(this_set_id);

	if(rtpp_list==NULL){	/*if a new id_set : add a new set of rtpp*/
		rtpp_list = shm_malloc(sizeof(struct rtpp_set));
		if(!rtpp_list){
			LM_ERR("no shm memory left\n");
			return NULL;
		}
		memset(rtpp_list, 0, sizeof(struct rtpp_set));
		rtpp_list->id_set = this_set_id;
		if (rtpp_set_list->rset_first == NULL)
		{
			rtpp_set_list->rset_first = rtpp_list;
		} else {
			rtpp_set_list->rset_last->rset_next = rtpp_list;
		}
		rtpp_set_list->rset_last = rtpp_list;
		rtpp_set_count++;

		if (this_set_id == DEFAULT_RTPP_SET_ID)
		{
			default_rtpp_set = rtpp_list;
		}
	}
	return rtpp_list;
}

int insert_rtpp_node(struct rtpp_set *const rtpp_list, const str *const url, const int weight, const int disabled)
{
	struct rtpp_node *pnode;

	if ((pnode = shm_malloc(sizeof(struct rtpp_node) + url->len + 1)) == NULL)
	{
		LM_ERR("out of shm memory\n");
		return -1;
	}
	memset(pnode, 0, sizeof(struct rtpp_node) + url->len + 1);
	pnode->idx = rtpp_no++;
	pnode->rn_weight = weight;
	pnode->rn_umode = 0;
	pnode->rn_disabled = disabled;
	/* Permanently disable if marked as disabled */
	pnode->rn_recheck_ticks = disabled ? MI_MAX_RECHECK_TICKS : 0;
	pnode->rn_url.s = (char*)(pnode + 1);
	memcpy(pnode->rn_url.s, url->s, url->len);
	pnode->rn_url.len = url->len;

	LM_DBG("url is '%.*s'\n", pnode->rn_url.len, pnode->rn_url.s);

	/* Find protocol and store address */
	pnode->rn_address = pnode->rn_url.s;
	if (strncasecmp(pnode->rn_address, "udp:", 4) == 0) {
		pnode->rn_umode = 1;
		pnode->rn_address += 4;
	} else if (strncasecmp(pnode->rn_address, "udp6:", 5) == 0) {
		pnode->rn_umode = 6;
		pnode->rn_address += 5;
	} else if (strncasecmp(pnode->rn_address, "unix:", 5) == 0) {
		pnode->rn_umode = 0;
		pnode->rn_address += 5;
	}

	if (rtpp_list->rn_first == NULL)
	{
		rtpp_list->rn_first = pnode;
	} else {
		rtpp_list->rn_last->rn_next = pnode;
	}
	rtpp_list->rn_last = pnode;
	rtpp_list->rtpp_node_count++;

	return 0;
}

static int add_rtpproxy_socks(struct rtpp_set * rtpp_list, 
										char * rtpproxy){
	/* Make rtp proxies list. */
	char *p, *p1, *p2, *plim;
	int weight;
	str url;

	p = rtpproxy;
	plim = p + strlen(p);

	for(;;) {
			weight = 1;
		while (*p && isspace((int)*p))
			++p;
		if (p >= plim)
			break;
		p1 = p;
		while (*p && !isspace((int)*p))
			++p;
		if (p <= p1)
			break; /* may happen??? */
		/* Have weight specified? If yes, scan it */
		p2 = memchr(p1, '=', p - p1);
		if (p2 != NULL) {
			weight = strtoul(p2 + 1, NULL, 10);
		} else {
			p2 = p;
		}

		url.s = p1;
		url.len = (p2-p1);
		insert_rtpp_node(rtpp_list, &url, weight, 0);
	}
	return 0;
}


/*	0-succes
 *  -1 - erorr
 * */
static int rtpproxy_add_rtpproxy_set( char * rtp_proxies)
{
	char *p,*p2;
	struct rtpp_set * rtpp_list;
	str id_set;

	/* empty definition? */
	p= rtp_proxies;
	if(!p || *p=='\0'){
		return 0;
	}

	for(;*p && isspace(*p);p++);
	if(*p=='\0'){
		return 0;
	}

	rtp_proxies = strstr(p, "==");
	if(rtp_proxies){
		if(*(rtp_proxies +2)=='\0'){
			LM_ERR("script error -invalid rtp proxy list!\n");
			return -1;
		}
			
		*rtp_proxies = '\0';
		p2 = rtp_proxies-1;
		for(;isspace(*p2); *p2 = '\0',p2--);
		id_set.s = p;	id_set.len = p2 - p+1;
			
		if(id_set.len <= 0){
		LM_ERR("script error -invalid set_id value!\n");
			return -1;
		}
			
		rtp_proxies+=2;
	}else{
		rtp_proxies = p;
		id_set = DEFAULT_RTPP_SET_ID_STR;
	}

	for(;*rtp_proxies && isspace(*rtp_proxies);rtp_proxies++);

	if(!(*rtp_proxies)){
		LM_ERR("script error -empty rtp_proxy list\n");
		return -1;;
	}

	rtpp_list = get_rtpp_set(&id_set);
	if (rtpp_list == NULL)
	{
		LM_ERR("Failed to get or create rtpp_list for '%.*s'\n", id_set.len, id_set.s);
		return -1;
	}

	if(add_rtpproxy_socks(rtpp_list, rtp_proxies)!= 0){
		return -1;
	}

	return 0;
}


static int fixup_set_id(void ** param, int param_no)
{
	int int_val, err;
	struct rtpp_set* rtpp_list;
	rtpp_set_link_t *rtpl = NULL;
	str s;

	rtpl = (rtpp_set_link_t*)pkg_malloc(sizeof(rtpp_set_link_t));
	if(rtpl==NULL) {
		LM_ERR("no more pkg memory\n");
		return -1;
	}
	memset(rtpl, 0, sizeof(rtpp_set_link_t));
	s.s = (char*)*param;
	s.len = strlen(s.s);

	if(s.s[0] == PV_MARKER) {
		int_val = pv_locate_name(&s);
		if(int_val<0 || int_val!=s.len) {
			LM_ERR("invalid parameter %s\n", s.s);
			return -1;
		}
		rtpl->rpv = pv_cache_get(&s);
		if(rtpl->rpv == NULL) {
			LM_ERR("invalid pv parameter %s\n", s.s);
			return -1;
		}
	} else {
		int_val = str2s(*param, strlen(*param), &err);
		if (err == 0) {
			pkg_free(*param);
			if((rtpp_list = select_rtpp_set(int_val)) ==0){
				LM_ERR("rtpp_proxy set %i not configured\n", int_val);
				return E_CFG;
			}
			rtpl->rset = rtpp_list;
		} else {
			LM_ERR("bad number <%s>\n",	(char *)(*param));
			return E_CFG;
		}
	}
	*param = (void*)rtpl;
	return 0;
}

static struct mi_root* mi_enable_rtp_proxy(struct mi_root* cmd_tree, 
												void* param )
{	struct mi_node* node;
	str rtpp_url;
	unsigned int enable;
	struct rtpp_set * rtpp_list;
	struct rtpp_node * crt_rtpp;
	int found;

	found = 0;

	if(rtpp_set_list ==NULL)
		goto end;

	node = cmd_tree->node.kids;
	if(node == NULL)
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	if(node->value.s == NULL || node->value.len ==0)
		return init_mi_tree( 400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);

	rtpp_url = node->value;

	node = node->next;
	if(node == NULL)
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	enable = 0;
	if( strno2int( &node->value, &enable) <0)
		goto error;

	for(rtpp_list = rtpp_set_list->rset_first; rtpp_list != NULL; 
					rtpp_list = rtpp_list->rset_next){

		for(crt_rtpp = rtpp_list->rn_first; crt_rtpp != NULL;
						crt_rtpp = crt_rtpp->rn_next){
			/*found a matching rtpp*/
			
			if(crt_rtpp->rn_url.len == rtpp_url.len){
				
				if(strncmp(crt_rtpp->rn_url.s, rtpp_url.s, rtpp_url.len) == 0){
					/*set the enabled/disabled status*/
					found = 1;
					crt_rtpp->rn_recheck_ticks = 
						enable? MI_MIN_RECHECK_TICKS : MI_MAX_RECHECK_TICKS;
					crt_rtpp->rn_disabled = enable?0:1;
				}
			}
		}
	}

end:
	if(found)
		return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	return init_mi_tree(404,MI_RTP_PROXY_NOT_FOUND,MI_RTP_PROXY_NOT_FOUND_LEN);
error:
	return init_mi_tree( 400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);
}



#define add_rtpp_node_int_info(_parent, _name, _name_len, _value, _child,\
								_len, _string, _error)\
	do {\
		(_string) = int2str((_value), &(_len));\
		if((_string) == 0){\
			LM_ERR("cannot convert int value\n");\
				goto _error;\
		}\
		if(((_child) = add_mi_node_child((_parent), MI_DUP_VALUE, (_name), \
				(_name_len), (_string), (_len))   ) == 0)\
			goto _error;\
	}while(0);

static struct mi_root* mi_show_rtpproxies(struct mi_root* cmd_tree, 
												void* param)
{
	struct mi_node* node, *crt_node, *child;
	struct mi_root* root;
	struct mi_attr * attr;
	struct rtpp_set * rtpp_list;
	struct rtpp_node * crt_rtpp;
	char * string, *id;
	int id_len, len;

	string = id = 0;

	root = init_mi_tree(200, MI_OK_S, MI_OK_LEN);
	if (!root) {
		LM_ERR("the MI tree cannot be initialized!\n");
		return 0;
	}

	if(rtpp_set_list ==NULL)
		return root;

	node = &root->node;

	for(rtpp_list = rtpp_set_list->rset_first; rtpp_list != NULL; 
					rtpp_list = rtpp_list->rset_next){

		for(crt_rtpp = rtpp_list->rn_first; crt_rtpp != NULL;
						crt_rtpp = crt_rtpp->rn_next){

			id =  int2str(rtpp_list->id_set, &id_len);
			if(!id){
				LM_ERR("cannot convert set id\n");
				goto error;
			}

			if(!(crt_node = add_mi_node_child(node, 0, crt_rtpp->rn_url.s, 
					crt_rtpp->rn_url.len, 0,0)) ) {
				LM_ERR("cannot add the child node to the tree\n");
				goto error;
			}

			LM_DBG("adding node name %s \n",crt_rtpp->rn_url.s );

			if((attr = add_mi_attr(crt_node, MI_DUP_VALUE, MI_SET, MI_SET_LEN, 
									id, id_len))== 0){
				LM_ERR("cannot add attributes to the node\n");
				goto error;
			}

			add_rtpp_node_int_info(crt_node, MI_INDEX, MI_INDEX_LEN,
				crt_rtpp->idx, child, len,string,error);
			add_rtpp_node_int_info(crt_node, MI_DISABLED, MI_DISABLED_LEN,
				crt_rtpp->rn_disabled, child, len,string,error);
			add_rtpp_node_int_info(crt_node, MI_WEIGHT, MI_WEIGHT_LEN,
				crt_rtpp->rn_weight,  child, len, string,error);
			add_rtpp_node_int_info(crt_node, MI_RECHECK_TICKS,MI_RECHECK_T_LEN,
				crt_rtpp->rn_recheck_ticks, child, len, string, error);
		}
	}

	return root;
error:
	if (root)
		free_mi_tree(root);
	return 0;
}


static int
mod_init(void)
{
	int i;
	pv_spec_t avp_spec;
	str s;
	unsigned short avp_flags;

	if(register_mi_mod(exports.name, mi_cmds)!=0)
	{
		LM_ERR("failed to register MI commands\n");
		return -1;
	}

	/* Configure the head of the rtpp_set_list */
	rtpp_set_list = shm_malloc(sizeof(struct rtpp_set_head));
	if (rtpp_set_list == NULL)
	{
		LM_ERR("no shm memory for rtpp_set_list\n");
		return -1;
	}
	memset(rtpp_set_list, 0, sizeof(struct rtpp_set_head));

	if (nortpproxy_str.s==NULL || nortpproxy_str.len<=0) {
		nortpproxy_str.len = 0;
	} else {
		while (nortpproxy_str.len > 0 && (nortpproxy_str.s[nortpproxy_str.len - 1] == '\r' ||
		    nortpproxy_str.s[nortpproxy_str.len - 1] == '\n'))
			nortpproxy_str.len--;
	}

	if (rtpp_db_url.s != NULL)
	{
		init_rtpproxy_db();
		if (rtpp_sets > 0)
		{
			LM_WARN("rtpproxy db url configured - ignoring modparam sets\n");
		}
	}
	/* storing the list of rtp proxy sets in shared memory*/
	for(i=0;i<rtpp_sets;i++){
		LM_DBG("Adding RTP-Proxy set %d/%d: %s\n", i, rtpp_sets, rtpp_strings[i]);
		if ((rtpp_db_url.s == NULL) &&
		    (rtpproxy_add_rtpproxy_set(rtpp_strings[i]) != 0)) {
			for(;i<rtpp_sets;i++)
				if(rtpp_strings[i])
					pkg_free(rtpp_strings[i]);
			pkg_free(rtpp_strings);
			LM_ERR("Failed to add RTP-Proxy from Config!\n");
			return -1;
		}
		if(rtpp_strings[i])
			pkg_free(rtpp_strings[i]);
	}

	if (ice_candidate_priority_avp_param) {
	    s.s = ice_candidate_priority_avp_param; s.len = strlen(s.s);
	    if (pv_parse_spec(&s, &avp_spec) == 0 || avp_spec.type != PVT_AVP) {
		LM_ERR("malformed or non AVP definition <%s>\n", ice_candidate_priority_avp_param);
		return -1;
	    }
	    if (pv_get_avp_name(0, &(avp_spec.pvp), &ice_candidate_priority_avp, &avp_flags) != 0) {
		LM_ERR("invalid AVP definition <%s>\n", ice_candidate_priority_avp_param);
		return -1;
	    }
	    ice_candidate_priority_avp_type = avp_flags;
	}

	if (rtp_inst_pv_param.s) {
	    rtp_inst_pvar = pv_cache_get(&rtp_inst_pv_param);
	    if ((rtp_inst_pvar == NULL) ||
	    	((rtp_inst_pvar->type != PVT_AVP) &&
	    	 (rtp_inst_pvar->type != PVT_XAVP) &&
	    	 (rtp_inst_pvar->type != PVT_SCRIPTVAR))) {
		LM_ERR("Invalid pvar name <%.*s>\n", rtp_inst_pv_param.len, rtp_inst_pv_param.s);
		return -1;
	    }
	}

	if (extra_id_pv_param.s && *extra_id_pv_param.s) {
		if(pv_parse_format(&extra_id_pv_param, &extra_id_pv) < 0) {
			LM_ERR("malformed PV string: %s\n", extra_id_pv_param.s);
			return -1;
		}
	} else {
		extra_id_pv = NULL;
	}

	if (rtpp_strings)
		pkg_free(rtpp_strings);

	if (load_tm_api( &tmb ) < 0)
	{
		LM_DBG("could not load the TM-functions - answer-offer model"
				" auto-detection is disabled\n");
		memset(&tmb, 0, sizeof(struct tm_binds));
	}

	return 0;
}


static int
child_init(int rank)
{
	int n;
	char *cp;
	struct addrinfo hints, *res;
	struct rtpp_set  *rtpp_list;
	struct rtpp_node *pnode;

	if(rtpp_set_list==NULL )
		return 0;

	/* Iterate known RTP proxies - create sockets */
	mypid = getpid();

	rtpp_socks = (int*)pkg_malloc( sizeof(int)*rtpp_no );
	if (rtpp_socks==NULL) {
		LM_ERR("no more pkg memory\n");
		return -1;
	}

	for(rtpp_list = rtpp_set_list->rset_first; rtpp_list != 0;
		rtpp_list = rtpp_list->rset_next){

		for (pnode=rtpp_list->rn_first; pnode!=0; pnode = pnode->rn_next){
			char *hostname;

			if (pnode->rn_umode == 0) {
				rtpp_socks[pnode->idx] = -1;
				goto rptest;
			}

			/*
			 * This is UDP or UDP6. Detect host and port; lookup host;
			 * do connect() in order to specify peer address
			 */
			hostname = (char*)pkg_malloc(sizeof(char) * (strlen(pnode->rn_address) + 1));
			if (hostname==NULL) {
				LM_ERR("no more pkg memory\n");
				return -1;
			}
			strcpy(hostname, pnode->rn_address);

			cp = strrchr(hostname, ':');
			if (cp != NULL) {
				*cp = '\0';
				cp++;
			}
			if (cp == NULL || *cp == '\0')
				cp = CPORT;

			memset(&hints, 0, sizeof(hints));
			hints.ai_flags = 0;
			hints.ai_family = (pnode->rn_umode == 6) ? AF_INET6 : AF_INET;
			hints.ai_socktype = SOCK_DGRAM;
			if ((n = getaddrinfo(hostname, cp, &hints, &res)) != 0) {
				LM_ERR("%s\n", gai_strerror(n));
				pkg_free(hostname);
				return -1;
			}
			pkg_free(hostname);

			rtpp_socks[pnode->idx] = socket((pnode->rn_umode == 6)
			    ? AF_INET6 : AF_INET, SOCK_DGRAM, 0);
			if ( rtpp_socks[pnode->idx] == -1) {
				LM_ERR("can't create socket\n");
				freeaddrinfo(res);
				return -1;
			}

			if (connect( rtpp_socks[pnode->idx], res->ai_addr, res->ai_addrlen) == -1) {
				LM_ERR("can't connect to a RTP proxy\n");
				close( rtpp_socks[pnode->idx] );
				rtpp_socks[pnode->idx] = -1;
				freeaddrinfo(res);
				return -1;
			}
			freeaddrinfo(res);
rptest:
			pnode->rn_disabled = rtpp_test(pnode, pnode->rn_disabled, 1);
		}
	}

	return 0;
}


static void mod_destroy(void)
{
	struct rtpp_set * crt_list, * last_list;
	struct rtpp_node * crt_rtpp, *last_rtpp;

	/*free the shared memory*/
	if (natping_state)
		shm_free(natping_state);

	if(rtpp_set_list == NULL)
		return;

	for(crt_list = rtpp_set_list->rset_first; crt_list != NULL; ){

		for(crt_rtpp = crt_list->rn_first; crt_rtpp != NULL;  ){

			last_rtpp = crt_rtpp;
			crt_rtpp = last_rtpp->rn_next;
			shm_free(last_rtpp);
		}

		last_list = crt_list;
		crt_list = last_list->rset_next;
		shm_free(last_list);
	}

	shm_free(rtpp_set_list);
}



static int
isnulladdr(str *sx, int pf)
{
	char *cp;

	if (pf == AF_INET6) {
		for(cp = sx->s; cp < sx->s + sx->len; cp++)
			if (*cp != '0' && *cp != ':')
				return 0;
		return 1;
	}
	return (sx->len == 7 && memcmp("0.0.0.0", sx->s, 7) == 0);
}

#define	ADD_ADIRECTION	0x01
#define	FIX_MEDIP	0x02
#define	ADD_ANORTPPROXY	0x04
#define	FIX_ORGIP	0x08

#define	ADIRECTION	"a=direction:active"
#define	ADIRECTION_LEN	(sizeof(ADIRECTION) - 1)

#define	AOLDMEDIP	"a=oldmediaip:"
#define	AOLDMEDIP_LEN	(sizeof(AOLDMEDIP) - 1)

#define	AOLDMEDIP6	"a=oldmediaip6:"
#define	AOLDMEDIP6_LEN	(sizeof(AOLDMEDIP6) - 1)

#define	AOLDMEDPRT	"a=oldmediaport:"
#define	AOLDMEDPRT_LEN	(sizeof(AOLDMEDPRT) - 1)


static inline int 
replace_sdp_ip(struct sip_msg* msg, str *org_body, char *line, str *ip)
{
	str body1, oldip, newip;
	str body = *org_body;
	unsigned hasreplaced = 0;
	int pf, pf1 = 0;
	str body2;
	char *bodylimit = body.s + body.len;

	/* Iterate all lines and replace ips in them. */
	if (!ip) {
		newip.s = ip_addr2a(&msg->rcv.src_ip);
		newip.len = strlen(newip.s);
	} else {
		newip = *ip;
	}
	body1 = body;
	for(;;) {
		if (extract_mediaip(&body1, &oldip, &pf,line) == -1)
			break;
		if (pf != AF_INET) {
			LM_ERR("not an IPv4 address in '%s' SDP\n",line);
				return -1;
			}
		if (!pf1)
			pf1 = pf;
		else if (pf != pf1) {
			LM_ERR("mismatching address families in '%s' SDP\n",line);
			return -1;
		}
		body2.s = oldip.s + oldip.len;
		body2.len = bodylimit - body2.s;
		if (alter_mediaip(msg, &body1, &oldip, pf, &newip, pf,1) == -1) {
			LM_ERR("can't alter '%s' IP\n",line);
			return -1;
		}
		hasreplaced = 1;
		body1 = body2;
	}
	if (!hasreplaced) {
		LM_ERR("can't extract '%s' IP from the SDP\n",line);
		return -1;
	}

	return 0;
}

static int
extract_mediaip(str *body, str *mediaip, int *pf, char *line)
{
	char *cp, *cp1;
	int len, nextisip;

	cp1 = NULL;
	for (cp = body->s; (len = body->s + body->len - cp) > 0;) {
		cp1 = ser_memmem(cp, line, len, 2);
		if (cp1 == NULL || cp1[-1] == '\n' || cp1[-1] == '\r')
			break;
		cp = cp1 + 2;
	}
	if (cp1 == NULL)
		return -1;

	mediaip->s = cp1 + 2;
	mediaip->len = eat_line(mediaip->s, body->s + body->len - mediaip->s) - mediaip->s;
	trim_len(mediaip->len, mediaip->s, *mediaip);

	nextisip = 0;
	for (cp = mediaip->s; cp < mediaip->s + mediaip->len;) {
		len = eat_token_end(cp, mediaip->s + mediaip->len) - cp;
		if (nextisip == 1) {
			mediaip->s = cp;
			mediaip->len = len;
			nextisip++;
			break;
		}
		if (len == 3 && memcmp(cp, "IP", 2) == 0) {
			switch (cp[2]) {
			case '4':
				nextisip = 1;
				*pf = AF_INET;
				break;

			case '6':
				nextisip = 1;
				*pf = AF_INET6;
				break;

			default:
				break;
			}
		}
		cp = eat_space_end(cp + len, mediaip->s + mediaip->len);
	}
	if (nextisip != 2 || mediaip->len == 0) {
		LM_ERR("no `IP[4|6]' in `%s' field\n",line);
		return -1;
	}
	return 1;
}

static int
alter_mediaip(struct sip_msg *msg, str *body, str *oldip, int oldpf,
  str *newip, int newpf, int preserve)
{
	char *buf;
	int offset;
	struct lump* anchor;
	str omip, nip, oip;

	/* check that updating mediaip is really necessary */
	if (oldpf == newpf && isnulladdr(oldip, oldpf))
		return 0;
	if (newip->len == oldip->len &&
	    memcmp(newip->s, oldip->s, newip->len) == 0)
		return 0;

	if (preserve != 0) {
		anchor = anchor_lump(msg, body->s + body->len - msg->buf, 0, 0);
		if (anchor == NULL) {
			LM_ERR("anchor_lump failed\n");
			return -1;
		}
		if (oldpf == AF_INET6) {
			omip.s = AOLDMEDIP6;
			omip.len = AOLDMEDIP6_LEN;
		} else {
			omip.s = AOLDMEDIP;
			omip.len = AOLDMEDIP_LEN;
		}
		buf = pkg_malloc(omip.len + oldip->len + CRLF_LEN);
		if (buf == NULL) {
			LM_ERR("out of pkg memory\n");
			return -1;
		}
		memcpy(buf, CRLF, CRLF_LEN);
		memcpy(buf + CRLF_LEN, omip.s, omip.len);
		memcpy(buf + CRLF_LEN + omip.len, oldip->s, oldip->len);
		if (insert_new_lump_after(anchor, buf,
		    omip.len + oldip->len + CRLF_LEN, 0) == NULL) {
			LM_ERR("insert_new_lump_after failed\n");
			pkg_free(buf);
			return -1;
		}
	}

	if (oldpf == newpf) {
		nip.len = newip->len;
		nip.s = pkg_malloc(nip.len);
		if (nip.s == NULL) {
			LM_ERR("out of pkg memory\n");
			return -1;
		}
		memcpy(nip.s, newip->s, newip->len);
	} else {
		nip.len = newip->len + 2;
		nip.s = pkg_malloc(nip.len);
		if (nip.s == NULL) {
			LM_ERR("out of pkg memory\n");
			return -1;
		}
		memcpy(nip.s + 2, newip->s, newip->len);
		nip.s[0] = (newpf == AF_INET6) ? '6' : '4';
		nip.s[1] = ' ';
	}

	oip = *oldip;
	if (oldpf != newpf) {
		do {
			oip.s--;
			oip.len++;
		} while (*oip.s != '6' && *oip.s != '4');
	}
	offset = oip.s - msg->buf;
	anchor = del_lump(msg, offset, oip.len, 0);
	if (anchor == NULL) {
		LM_ERR("del_lump failed\n");
		pkg_free(nip.s);
		return -1;
	}

	if (insert_new_lump_after(anchor, nip.s, nip.len, 0) == 0) {
		LM_ERR("insert_new_lump_after failed\n");
		pkg_free(nip.s);
		return -1;
	}
	return 0;
}

static int
alter_mediaport(struct sip_msg *msg, str *body, str *oldport, str *newport,
  int preserve)
{
	char *buf;
	int offset;
	struct lump* anchor;

	/* check that updating mediaport is really necessary */
	if (newport->len == oldport->len &&
	    memcmp(newport->s, oldport->s, newport->len) == 0)
		return 0;

	/*
	 * Since rewriting the same info twice will mess SDP up,
	 * apply simple anti foot shooting measure - put flag on
	 * messages that have been altered and check it when
	 * another request comes.
	 */
#if 0
	/* disabled: - it propagates to the reply and we don't want this
	 *  -- andrei */
	if (msg->msg_flags & FL_SDP_PORT_AFS) {
		LM_ERR("you can't rewrite the same SDP twice, check your config!\n");
		return -1;
	}
#endif

	if (preserve != 0) {
		anchor = anchor_lump(msg, body->s + body->len - msg->buf, 0, 0);
		if (anchor == NULL) {
			LM_ERR("anchor_lump failed\n");
			return -1;
		}
		buf = pkg_malloc(AOLDMEDPRT_LEN + oldport->len + CRLF_LEN);
		if (buf == NULL) {
			LM_ERR("out of pkg memory\n");
			return -1;
		}
		memcpy(buf, CRLF, CRLF_LEN);
		memcpy(buf + CRLF_LEN, AOLDMEDPRT, AOLDMEDPRT_LEN);
		memcpy(buf + CRLF_LEN + AOLDMEDPRT_LEN, oldport->s, oldport->len);
		if (insert_new_lump_after(anchor, buf,
		    AOLDMEDPRT_LEN + oldport->len + CRLF_LEN, 0) == NULL) {
			LM_ERR("insert_new_lump_after failed\n");
			pkg_free(buf);
			return -1;
		}
	}

	buf = pkg_malloc(newport->len);
	if (buf == NULL) {
		LM_ERR("out of pkg memory\n");
		return -1;
	}
	offset = oldport->s - msg->buf;
	anchor = del_lump(msg, offset, oldport->len, 0);
	if (anchor == NULL) {
		LM_ERR("del_lump failed\n");
		pkg_free(buf);
		return -1;
	}
	memcpy(buf, newport->s, newport->len);
	if (insert_new_lump_after(anchor, buf, newport->len, 0) == 0) {
		LM_ERR("insert_new_lump_after failed\n");
		pkg_free(buf);
		return -1;
	}

#if 0
	msg->msg_flags |= FL_SDP_PORT_AFS;
#endif
	return 0;
}

/*
 * this function is ported from SER 
 */
static int
alter_rtcp(struct sip_msg *msg, str *body, str *oldport, str *newport)
{
	char *buf;
	int offset;
	struct lump* anchor;

	/* check that updating rtcpport is really necessary */
	if (newport->len == oldport->len &&
	    memcmp(newport->s, oldport->s, newport->len) == 0)
		return 0;

	buf = pkg_malloc(newport->len);
	if (buf == NULL) {
		LM_ERR("alter_rtcp: out of memory\n");
		return -1;
	}
	offset = oldport->s - msg->buf;
	anchor = del_lump(msg, offset, oldport->len, 0);
	if (anchor == NULL) {
		LM_ERR("alter_rtcp: del_lump failed\n");
		pkg_free(buf);
		return -1;
	}
	memcpy(buf, newport->s, newport->len);
	if (insert_new_lump_after(anchor, buf, newport->len, 0) == 0) {
		LM_ERR("alter_rtcp: insert_new_lump_after failed\n");
		pkg_free(buf);
		return -1;
	}

	return 0;
}


static char *
append_filtered_ip(char *at, str *ip)
{
    int i;
    for (i = 0; i < ip->len; i++) {
	if (isdigit(ip->s[i])) {
	    append_chr(at, ip->s[i]);
	}
    }
    return at;
}

		
static int
insert_candidates(struct sip_msg *msg, char *where, str *ip, unsigned int port,
		  int priority)
{
    char *buf, *at;
    struct lump* anchor;
    str rtp_port;
    str rtcp_port;

    rtcp_port.s = int2str(port+1, &rtcp_port.len); /* beware static buffer */
    buf = pkg_malloc(24 + 78 + 14 + 24 + 2*ip->len + 2 + 2*rtcp_port.len + 24);
    if (buf == NULL) {
	LM_ERR("insert_candidates: out of memory\n");
	return -1;
    }

    at = buf;

	append_str(at, "a=candidate:", 12);
	at = append_filtered_ip(at, ip);
	append_str(at, " 2 UDP ", 7);
	if (priority == 2) {
	    append_str(at, "16777214 ", 9);
	} else {
	    append_str(at, "2197815294 ", 11);
	}
	append_str(at, ip->s, ip->len);
	append_chr(at, ' ');
	append_str(at, rtcp_port.s, rtcp_port.len);
	append_str(at, " typ relay\r\n", 12);

    rtp_port.s = int2str(port, &rtp_port.len); /* beware static buffer */
    append_str(at, "a=candidate:", 12);
    at = append_filtered_ip(at, ip);
    append_str(at, " 1 UDP ", 7);
    if (priority == 2) {
	append_str(at, "16777215 ", 9);
    } else {
	append_str(at, "2197815295 ", 11);
    }
    append_str(at, ip->s, ip->len);
    append_chr(at, ' ');
    append_str(at, rtp_port.s, rtp_port.len);
    append_str(at, " typ relay\r\n", 12);

    LM_DBG("inserting '%.*s'\n", (int)(at - buf), buf);

    anchor = anchor_lump(msg, where - msg->buf, 0, 0);
    if (anchor == 0) {
	LOG(L_ERR, "insert_candidates: can't get anchor\n");
	pkg_free(buf);
	return -1;
    }
    if (insert_new_lump_before(anchor, buf, at - buf, 0) == 0) {
	LM_ERR("insert_candidates: insert_new_lump_before failed\n");
	pkg_free(buf);
	return -1;
    }

    return 0;
}
    

static char * gencookie(void)
{
	static char cook[34];

	sprintf(cook, "%d_%u ", (int)mypid, myseqn);
	myseqn++;
	return cook;
}

static int
rtpp_checkcap(struct rtpp_node *node, char *cap, int caplen)
{
	char *cp;
	struct iovec vf[4] = {{NULL, 0}, {"VF", 2}, {" ", 1}, {NULL, 0}};

	vf[3].iov_base = cap;
	vf[3].iov_len = caplen;

	cp = send_rtpp_command(node, vf, 4);
	if (cp == NULL)
		return -1;
	if (cp[0] == 'E' || atoi(cp) != 1)
		return 0;
	return 1;
}

static int
rtpp_test(struct rtpp_node *node, int isdisabled, int force)
{
	int rtpp_ver, rval;
	char *cp;
	struct iovec v[2] = {{NULL, 0}, {"V", 1}};

	if(node->rn_recheck_ticks == MI_MAX_RECHECK_TICKS){
	    LM_DBG("rtpp %s disabled for ever\n", node->rn_url.s);
		return 1;
	}

	if (force == 0) {
		if (isdisabled == 0)
			return 0;
		if (node->rn_recheck_ticks > get_ticks())
			return 1;
	}
	cp = send_rtpp_command(node, v, 2);
	if (cp == NULL) {
		LM_WARN("can't get version of the RTP proxy\n");
		goto error;
	}
	rtpp_ver = atoi(cp);
	if (rtpp_ver != SUP_CPROTOVER) {
		LM_WARN("unsupported version of RTP proxy <%s> found: %d supported,"
				"%d present\n", node->rn_url.s, SUP_CPROTOVER, rtpp_ver);
		goto error;
	}
	rval = rtpp_checkcap(node, REQ_CPROTOVER, sizeof(REQ_CPROTOVER) - 1);
	if (rval == -1) {
		LM_WARN("RTP proxy went down during version query\n");
		goto error;
	}
	if (rval == 0) {
		LM_WARN("of RTP proxy <%s> doesn't support required protocol version"
				"%s\n", node->rn_url.s, REQ_CPROTOVER);
		goto error;
	}
	LM_INFO("rtp proxy <%s> found, support for it %senabled\n",
	    node->rn_url.s, force == 0 ? "re-" : "");
	/* Check for optional capabilities */
	rval = rtpp_checkcap(node, REP_CPROTOVER, sizeof(REP_CPROTOVER) - 1);
	if (rval != -1) {
		node->rn_rep_supported = rval;
	} else {
		node->rn_rep_supported = 0;
	}
	rval = rtpp_checkcap(node, PTL_CPROTOVER, sizeof(PTL_CPROTOVER) - 1);
	if (rval != -1) {
		node->rn_ptl_supported = rval;
	} else {
		node->rn_ptl_supported = 0;
	}
	return 0;
error:
	LM_WARN("support for RTP proxy <%s> has been disabled%s\n", node->rn_url.s,
	    rtpproxy_disable_tout < 0 ? "" : " temporarily");
	if (rtpproxy_disable_tout >= 0)
		node->rn_recheck_ticks = get_ticks() + rtpproxy_disable_tout;

	return 1;
}

char *
send_rtpp_command(struct rtpp_node *node, struct iovec *v, int vcnt)
{
	struct sockaddr_un addr;
	int fd, len, i;
	char *cp;
	static char buf[256];
	struct pollfd fds[1];

	len = 0;
	cp = buf;
	if (node->rn_umode == 0) {
		memset(&addr, 0, sizeof(addr));
		addr.sun_family = AF_LOCAL;
		strncpy(addr.sun_path, node->rn_address,
		    sizeof(addr.sun_path) - 1);
#ifdef HAVE_SOCKADDR_SA_LEN
		addr.sun_len = strlen(addr.sun_path);
#endif

		fd = socket(AF_LOCAL, SOCK_STREAM, 0);
		if (fd < 0) {
			LM_ERR("can't create socket\n");
			goto badproxy;
		}
		if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
			close(fd);
			LM_ERR("can't connect to RTP proxy\n");
			goto badproxy;
		}

		do {
			len = writev(fd, v + 1, vcnt - 1);
		} while (len == -1 && errno == EINTR);
		if (len <= 0) {
			close(fd);
			LM_ERR("can't send command to a RTP proxy\n");
			goto badproxy;
		}
		do {
			len = read(fd, buf, sizeof(buf) - 1);
		} while (len == -1 && errno == EINTR);
		close(fd);
		if (len <= 0) {
			LM_ERR("can't read reply from a RTP proxy\n");
			goto badproxy;
		}
	} else {
		fds[0].fd = rtpp_socks[node->idx];
		fds[0].events = POLLIN;
		fds[0].revents = 0;
		/* Drain input buffer */
		while ((poll(fds, 1, 0) == 1) &&
		    ((fds[0].revents & POLLIN) != 0)) {
			recv(rtpp_socks[node->idx], buf, sizeof(buf) - 1, 0);
			fds[0].revents = 0;
		}
		v[0].iov_base = gencookie();
		v[0].iov_len = strlen(v[0].iov_base);
		for (i = 0; i < rtpproxy_retr; i++) {
			do {
				len = writev(rtpp_socks[node->idx], v, vcnt);
			} while (len == -1 && (errno == EINTR || errno == ENOBUFS));
			if (len <= 0) {
				LM_ERR("can't send command to a RTP proxy\n");
				goto badproxy;
			}
			while ((poll(fds, 1, rtpproxy_tout * 1000) == 1) &&
			    (fds[0].revents & POLLIN) != 0) {
				do {
					len = recv(rtpp_socks[node->idx], buf, sizeof(buf)-1, 0);
				} while (len == -1 && errno == EINTR);
				if (len <= 0) {
					LM_ERR("can't read reply from a RTP proxy\n");
					goto badproxy;
				}
				if (len >= (v[0].iov_len - 1) &&
				    memcmp(buf, v[0].iov_base, (v[0].iov_len - 1)) == 0) {
					len -= (v[0].iov_len - 1);
					cp += (v[0].iov_len - 1);
					if (len != 0) {
						len--;
						cp++;
					}
					goto out;
				}
				fds[0].revents = 0;
			}
		}
		if (i == rtpproxy_retr) {
			LM_ERR("timeout waiting reply from a RTP proxy\n");
			goto badproxy;
		}
	}

out:
	cp[len] = '\0';
	return cp;
badproxy:
	LM_ERR("proxy <%s> does not respond, disable it\n", node->rn_url.s);
	node->rn_disabled = 1;
	node->rn_recheck_ticks = get_ticks() + rtpproxy_disable_tout;
	
	return NULL;
}

/*
 * select the set with the id_set id
 */

static struct rtpp_set * select_rtpp_set(int id_set ){

	struct rtpp_set * rtpp_list;
	/*is it a valid set_id?*/
	
	if(!rtpp_set_list)
	{
		LM_ERR("rtpproxy set list not initialised\n");
		return NULL;
	}

	for(rtpp_list=rtpp_set_list->rset_first; rtpp_list!=NULL && 
		rtpp_list->id_set!=id_set; rtpp_list=rtpp_list->rset_next);

	return rtpp_list;
}
/*
 * Main balancing routine. This does not try to keep the same proxy for
 * the call if some proxies were disabled or enabled; proxy death considered
 * too rare. Otherwise we should implement "mature" HA clustering, which is
 * too expensive here.
 */
struct rtpp_node *
select_rtpp_node(str callid, int do_test)
{
	unsigned sum, sumcut, weight_sum;
	struct rtpp_node* node;
	int was_forced;

	if(!selected_rtpp_set){
		LM_ERR("script error -no valid set selected\n");
		return NULL;
	}
	/* Most popular case: 1 proxy, nothing to calculate */
	if (selected_rtpp_set->rtpp_node_count == 1) {
		node = selected_rtpp_set->rn_first;
		if (node->rn_disabled && node->rn_recheck_ticks <= get_ticks())
			node->rn_disabled = rtpp_test(node, 1, 0);
		return node->rn_disabled ? NULL : node;
	}

	/* XXX Use quick-and-dirty hashing algo */
	for(sum = 0; callid.len > 0; callid.len--)
		sum += callid.s[callid.len - 1];
	sum &= 0xff;

	was_forced = 0;
retry:
	weight_sum = 0;
	for (node=selected_rtpp_set->rn_first; node!=NULL; node=node->rn_next) {

		if (node->rn_disabled && node->rn_recheck_ticks <= get_ticks()){
			/* Try to enable if it's time to try. */
			node->rn_disabled = rtpp_test(node, 1, 0);
		}
		if (!node->rn_disabled)
			weight_sum += node->rn_weight;
	}
	if (weight_sum == 0) {
		/* No proxies? Force all to be redetected, if not yet */
		if (was_forced)
			return NULL;
		was_forced = 1;
		for(node=selected_rtpp_set->rn_first; node!=NULL; node=node->rn_next) {
			node->rn_disabled = rtpp_test(node, 1, 1);
		}
		goto retry;
	}
	sumcut = sum % weight_sum;
	/*
	 * sumcut here lays from 0 to weight_sum-1.
	 * Scan proxy list and decrease until appropriate proxy is found.
	 */
	for (node=selected_rtpp_set->rn_first; node!=NULL; node=node->rn_next) {
		if (node->rn_disabled)
			continue;
		if (sumcut < node->rn_weight)
			goto found;
		sumcut -= node->rn_weight;
	}
	/* No node list */
	return NULL;
found:
	if (do_test) {
		node->rn_disabled = rtpp_test(node, node->rn_disabled, 0);
		if (node->rn_disabled)
			goto retry;
	}
	return node;
}


static int
get_extra_id(struct sip_msg* msg, str *id_str) {
	if(msg==NULL || extra_id_pv==NULL || id_str==NULL) {
		LM_ERR("bad parameters\n");
		return 0;
	}
	if (pv_printf_s(msg, extra_id_pv, id_str)<0) {
		LM_ERR("cannot print the additional id\n");
		return 0;
	}

	return 1;
}


static int
unforce_rtp_proxy1_f(struct sip_msg* msg, char* str1, char* str2)
{
	str flags;

	if (str1)
		get_str_fparam(&flags, msg, (fparam_t *) str1);
	else
		flags.s = NULL;

	return unforce_rtp_proxy(msg, flags.s);
}


static int
unforce_rtp_proxy(struct sip_msg* msg, char* flags)
{
	str callid, from_tag, to_tag, viabranch;
	char *cp;
	int via = 0;
	int to = 1;
	int extra = 0;
	str extra_id;
	int ret;
	struct rtpp_node *node;
	struct iovec v[1 + 4 + 3 + 2] = {{NULL, 0}, {"D", 1}, {" ", 1}, {NULL, 0}, {NULL, 0}, {NULL, 0}, {" ", 1}, {NULL, 0}, {" ", 1}, {NULL, 0}};
	                                            /* 1 */   /* 2 */   /* 3 */    /* 4 */    /* 5 */    /* 6 */   /* 7 */    /* 8 */   /* 9 */


	for (cp = flags; cp && *cp; cp++) {
		switch (*cp) {
			case '1':
				via = 1;
				break;

			case '2':
				via = 2;
				break;

			case '3':
				if(msg && msg->first_line.type == SIP_REPLY)
					via = 2;
				else
					via = 1;
				break;

 		        case 't':
		        case 'T':
			    to = 0;
			    break;
			case 'b':
				extra = 1;
				break;
			case 'a':
			case 'A':
			case 'i':
			case 'I':
			case 'e':
			case 'E':
			case 'l':
			case 'L':
			case 'f':
			case 'F':
			case 'r':
			case 'R':
			case 'c':
			case 'C':
			case 'o':
			case 'O':
			case 'x':
			case 'X':
			case 'w':
			case 'W':
			case 'z':
			case 'Z':
				/* ignore them - they can be sent by rtpproxy_manage() */
				break;

			default:
				LM_ERR("unknown option `%c'\n", *cp);
				return -1;
		}
	}

	if (get_callid(msg, &callid) == -1 || callid.len == 0) {
		LM_ERR("can't get Call-Id field\n");
		return -1;
	}
	to_tag.s = 0;
	to_tag.len = 0;
	if ((to == 1) && get_to_tag(msg, &to_tag) == -1) {
		LM_ERR("can't get To tag\n");
		return -1;
	}
	if (get_from_tag(msg, &from_tag) == -1 || from_tag.len == 0) {
		LM_ERR("can't get From tag\n");
		return -1;
	}
	if (via) {
		if (via == 1)
			ret = get_via_branch(msg, 1, &viabranch);
		else /* (via == 2) */
			ret = get_via_branch(msg, 2, &viabranch);
		if (ret == -1 || viabranch.len == 0) {
			LM_ERR("can't get Via branch\n");
			return -1;
		}
		v[4].iov_base = ";";
		v[4].iov_len = 1;
		STR2IOVEC(viabranch, v[5]);
	} else
	/* Append extra id to call-id */
	if (extra && extra_id_pv && get_extra_id(msg, &extra_id)) {
		v[4].iov_base = ";";
		v[4].iov_len = 1;
		STR2IOVEC(extra_id, v[5]);
	}
	STR2IOVEC(callid, v[3]);
	STR2IOVEC(from_tag, v[7]);
	STR2IOVEC(to_tag, v[9]);
	
	if(msg->id != current_msg_id){
		selected_rtpp_set = default_rtpp_set;
	}
	
	node = select_rtpp_node(callid, 1);
	if (!node) {
		LM_ERR("no available proxies\n");
		return -1;
	}
    	set_rtp_inst_pvar(msg, &node->rn_url);
	send_rtpp_command(node, v, (to_tag.len > 0) ? 10 : 8);

	return 1;
}

/* This function assumes p points to a line of requested type. */

static int
set_rtp_proxy_set_f(struct sip_msg * msg, char * str1, char * str2)
{
	rtpp_set_link_t *rtpl;
	pv_value_t val;

	rtpl = (rtpp_set_link_t*)str1;

	current_msg_id = 0;
	selected_rtpp_set = 0;

	if(rtpl->rset != NULL) {
		current_msg_id = msg->id;
		selected_rtpp_set = rtpl->rset;
	} else {
		if(pv_get_spec_value(msg, rtpl->rpv, &val)<0) {
			LM_ERR("cannot evaluate pv param\n");
			return -1;
		}
		if(!(val.flags & PV_VAL_INT)) {
			LM_ERR("pv param must hold an integer value\n");
			return -1;
		}
		selected_rtpp_set = select_rtpp_set(val.ri);
		if(selected_rtpp_set==NULL) {
			LM_ERR("could not locate rtpproxy set %d\n", val.ri);
			return -1;
		}
		current_msg_id = msg->id;
	}
	return 1;
}

static int
rtpproxy_manage(struct sip_msg *msg, char *flags, char *ip)
{
	char *cp = NULL;
	char newip[IP_ADDR_MAX_STR_SIZE];
	int method;
	int nosdp;

	if(msg->cseq==NULL && ((parse_headers(msg, HDR_CSEQ_F, 0)==-1)
				|| (msg->cseq==NULL)))
	{
		LM_ERR("no CSEQ header\n");
		return -1;
	}

	method = get_cseq(msg)->method_id;

	if(!(method==METHOD_INVITE || method==METHOD_ACK || method==METHOD_CANCEL
				|| method==METHOD_BYE || method==METHOD_UPDATE || method==METHOD_PRACK))
		return -1;

	if(method==METHOD_CANCEL || method==METHOD_BYE)
		return unforce_rtp_proxy(msg, flags);

	if(ip==NULL)
	{
		cp = ip_addr2a(&msg->rcv.dst_ip);
		strcpy(newip, cp);
	}

	if(msg->msg_flags & FL_SDP_BODY)
		nosdp = 0;
	else
		nosdp = parse_sdp(msg);

	if(msg->first_line.type == SIP_REQUEST) {
		if(method==METHOD_ACK && nosdp==0)
			return force_rtp_proxy(msg, flags, (cp!=NULL)?newip:ip, 0,
					(ip!=NULL)?1:0);
		if(method==METHOD_PRACK && nosdp==0)
			return force_rtp_proxy(msg, flags, (cp!=NULL)?newip:ip, 1,
					(ip!=NULL)?1:0);
		if(method==METHOD_UPDATE && nosdp==0)
			return force_rtp_proxy(msg, flags, (cp!=NULL)?newip:ip, 1,
					(ip!=NULL)?1:0);
		if(method==METHOD_INVITE && nosdp==0) {
			msg->msg_flags |= FL_SDP_BODY;
			if(tmb.t_gett!=NULL && tmb.t_gett()!=NULL
					&& tmb.t_gett()!=T_UNDEFINED)
				tmb.t_gett()->uas.request->msg_flags |= FL_SDP_BODY;
			if(route_type==FAILURE_ROUTE)
				return unforce_rtp_proxy(msg, flags);
			return force_rtp_proxy(msg, flags, (cp!=NULL)?newip:ip, 1,
					(ip!=NULL)?1:0);
		}
	} else if(msg->first_line.type == SIP_REPLY) {
		if(msg->first_line.u.reply.statuscode>=300)
			return unforce_rtp_proxy(msg, flags);
		if(nosdp==0) {
			if(method==METHOD_PRACK)
				return force_rtp_proxy(msg, flags, (cp!=NULL)?newip:ip, 0,
					(ip!=NULL)?1:0);
			if(method==METHOD_UPDATE)
				return force_rtp_proxy(msg, flags, (cp!=NULL)?newip:ip, 0,
					(ip!=NULL)?1:0);
			if(tmb.t_gett==NULL || tmb.t_gett()==NULL
					|| tmb.t_gett()==T_UNDEFINED)
				return force_rtp_proxy(msg, flags, (cp!=NULL)?newip:ip, 0,
					(ip!=NULL)?1:0);
			if(tmb.t_gett()->uas.request->msg_flags & FL_SDP_BODY)
				return force_rtp_proxy(msg, flags, (cp!=NULL)?newip:ip, 0,
					(ip!=NULL)?1:0);
			return force_rtp_proxy(msg, flags, (cp!=NULL)?newip:ip, 1,
					(ip!=NULL)?1:0);
		}
	}
	return -1;
}

static int
rtpproxy_manage0(struct sip_msg *msg, char *flags, char *ip)
{
	return rtpproxy_manage(msg, 0, 0);
}

static int
rtpproxy_manage1(struct sip_msg *msg, char *flags, char *ip)
{
	str flag_str;
	if(fixup_get_svalue(msg, (gparam_p)flags, &flag_str)<0)
	{
		LM_ERR("invalid flags parameter\n");
		return -1;
	}
	return rtpproxy_manage(msg, flag_str.s, 0);
}

static int
rtpproxy_manage2(struct sip_msg *msg, char *flags, char *ip)
{
	str flag_str;
	str ip_str;
	if(fixup_get_svalue(msg, (gparam_p)flags, &flag_str)<0)
	{
		LM_ERR("invalid flags parameter\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_p)ip, &ip_str)<0)
	{
		LM_ERR("invalid IP parameter\n");
		return -1;
	}
	return rtpproxy_manage(msg, flag_str.s, ip_str.s);
}

static int
rtpproxy_offer1_f(struct sip_msg *msg, char *str1, char *str2)
{
        char *cp;
        char newip[IP_ADDR_MAX_STR_SIZE];
	str flags;

        cp = ip_addr2a(&msg->rcv.dst_ip);
        strcpy(newip, cp);

	if (str1)
		get_str_fparam(&flags, msg, (fparam_t *) str1);
	else
		flags.s = NULL;
	return force_rtp_proxy(msg, flags.s, newip, 1, 0);
}

static int
rtpproxy_offer2_f(struct sip_msg *msg, char *param1, char *param2)
{
	str flags, new_ip;

	get_str_fparam(&flags, msg, (fparam_t *) param1);
	get_str_fparam(&new_ip, msg, (fparam_t *) param2);
	return force_rtp_proxy(msg, flags.s, new_ip.s, 1, 1);
}

static int
rtpproxy_answer1_f(struct sip_msg *msg, char *str1, char *str2)
{
        char *cp;
        char newip[IP_ADDR_MAX_STR_SIZE];
	str flags;

	if (msg->first_line.type == SIP_REQUEST)
		if (msg->first_line.u.request.method_value != METHOD_ACK)
			return -1;

        cp = ip_addr2a(&msg->rcv.dst_ip);
        strcpy(newip, cp);

	if (str1)
		get_str_fparam(&flags, msg, (fparam_t *) str1);
	else
		flags.s = NULL;
	return force_rtp_proxy(msg, flags.s, newip, 0, 0);
}

static int
rtpproxy_answer2_f(struct sip_msg *msg, char *param1, char *param2)
{

	str flags, new_ip;

	if (msg->first_line.type == SIP_REQUEST)
		if (msg->first_line.u.request.method_value != METHOD_ACK)
			return -1;

	get_str_fparam(&flags, msg, (fparam_t *) param1);
	get_str_fparam(&new_ip, msg, (fparam_t *) param2);
	return force_rtp_proxy(msg, flags.s, new_ip.s, 0, 1);
}


struct options {
	str s;
	int oidx;
};

static int
append_opts(struct options *op, char ch)
{
	void *p;

	if (op->s.len <= op->oidx) {
		p = pkg_realloc(op->s.s, op->oidx + 32);
		if (p == NULL) {
			return (-1);
		}
		op->s.s = p;
		op->s.len = op->oidx + 32;
	}
	op->s.s[op->oidx++] = ch;
	return (0);
}

static void
free_opts(struct options *op1, struct options *op2, struct options *op3)
{

	if (op1->s.len > 0 && op1->s.s != NULL) {
		pkg_free(op1->s.s);
		op1->s.len = 0;
	}
	if (op2->s.len > 0 && op2->s.s != NULL) {
		pkg_free(op2->s.s);
		op2->s.len = 0;
	}
	if (op3->s.len > 0 && op3->s.s != NULL) {
		pkg_free(op3->s.s);
		op3->s.len = 0;
	}
}

#define FORCE_RTP_PROXY_RET(e) \
    do { \
	free_opts(&opts, &rep_opts, &pt_opts); \
	return (e); \
    } while (0);

static int
force_rtp_proxy(struct sip_msg* msg, char* str1, char* str2, int offer, int forcedIP)
{
	str body, body1, oldport, oldip, newport, newip;
	str callid, from_tag, to_tag, tmp, payload_types;
	str newrtcp = {0, 0};
	str viabranch;
	int create, port, len, flookup, argc, proxied, real, via, ret;
	int orgip, commip;
	int pf, pf1, force;
	struct options opts, rep_opts, pt_opts;
	char *cp, *cp1;
	char  *cpend, *next;
	char **ap, *argv[10];
	struct lump* anchor;
	struct rtpp_node *node;
	struct iovec v[] = {
		{NULL, 0},	/* reserved (cookie) */
		{NULL, 0},	/* command & common options */
		{NULL, 0},	/* per-media/per-node options 1 */
		{NULL, 0},	/* per-media/per-node options 2 */
		{" ", 1},	/* separator */
		{NULL, 0},	/* callid */
		{NULL, 0},	/* via-branch separator ";" */
		{NULL, 0},	/* via-branch */
		{" ", 1},	/* separator */
		{NULL, 7},	/* newip */
		{" ", 1},	/* separator */
		{NULL, 1},	/* oldport */
		{" ", 1},	/* separator */
		{NULL, 0},	/* from_tag */
		{";", 1},	/* separator */
		{NULL, 0},	/* medianum */
		{" ", 1},	/* separator */
		{NULL, 0},	/* to_tag */
		{";", 1},	/* separator */
		{NULL, 0},	/* medianum */
		{" ", 1},	/* separator */
		{NULL, 0},	/* Timeout-Socket */
	};
	int iovec_param_count;
	int autobridge_ipv4v6;
	int extra;
	str extra_id;

	char *c1p, *c2p, *bodylimit, *o1p;
	char itoabuf_buf[20];
	int medianum, media_multi;
	str itoabuf_str;
	int c1p_altered;

	int sdp_session_num, sdp_stream_num;
	sdp_session_cell_t* sdp_session;
	sdp_stream_cell_t* sdp_stream;

	int_str ice_candidate_priority_val;

	memset(&opts, '\0', sizeof(opts));
	memset(&rep_opts, '\0', sizeof(rep_opts));
	memset(&pt_opts, '\0', sizeof(pt_opts));
	/* Leave space for U/L prefix TBD later */
	if (append_opts(&opts, '?') == -1) {
		LM_ERR("out of pkg memory\n");
		FORCE_RTP_PROXY_RET (-1);
	}
	flookup = force = real = orgip = commip = via = autobridge_ipv4v6 = extra = 0;
	for (cp = str1; cp != NULL && *cp != '\0'; cp++) {
		switch (*cp) {
		case '1':
			via = 1;
			break;

		case '2':
			via = 2;
			break;

		case '3':
			if(msg && msg->first_line.type == SIP_REPLY)
				via = 2;
			else
				via = 1;
			break;

		case 'a':
		case 'A':
			if (append_opts(&opts, 'A') == -1) {
				LM_ERR("out of pkg memory\n");
				FORCE_RTP_PROXY_RET (-1);
			}
			real = 1;
			break;

		case 'b':
			extra = 1;
			break;

		case 'i':
		case 'I':
			if (append_opts(&opts, 'I') == -1) {
				LM_ERR("out of pkg memory\n");
				FORCE_RTP_PROXY_RET (-1);
			}
			break;

		case 'e':
		case 'E':
			if (append_opts(&opts, 'E') == -1) {
				LM_ERR("out of pkg memory\n");
				FORCE_RTP_PROXY_RET (-1);
			}
			break;

		case 'l':
		case 'L':
			if (offer == 0) {
				FORCE_RTP_PROXY_RET (-1);
			}
			flookup = 1;
			break;

		case 'f':
		case 'F':
			force = 1;
			break;

		case 'r':
		case 'R':
			real = 1;
			break;

		case 'c':
		case 'C':
			commip = 1;
			break;

		case 'o':
		case 'O':
			orgip = 1;
			break;

		case 'x':
		case 'X':
			autobridge_ipv4v6 = 1;
			break;

		case 'w':
		case 'W':
			if (append_opts(&opts, 'S') == -1) {
				LM_ERR("out of pkg memory\n");
				FORCE_RTP_PROXY_RET (-1);
			}
			break;

		case 'z':
		case 'Z':
			if (append_opts(&rep_opts, 'Z') == -1) {
				LM_ERR("out of pkg memory\n");
				FORCE_RTP_PROXY_RET (-1);
			}
			/* If there are any digits following Z copy them into the command */
			for (; cp[1] != '\0' && isdigit(cp[1]); cp++) {
				if (append_opts(&rep_opts, cp[1]) == -1) {
					LM_ERR("out of pkg memory\n");
					FORCE_RTP_PROXY_RET (-1);
				}
			}
			break;

		case 't':
		case 'T':
		        /* Only used in rtpproxy_destroy */
		        break;

		default:
			LM_ERR("unknown option `%c'\n", *cp);
			FORCE_RTP_PROXY_RET (-1);
		}
	}

	if (offer != 0) {
		create = 1;
	} else {
		create = 0;
	}
	/* extract_body will also parse all the headers in the message as
	 * a side effect => don't move get_callid/get_to_tag in front of it
	 * -- andrei */
	if (extract_body(msg, &body) == -1) {
		LM_ERR("can't extract body from the message\n");
		FORCE_RTP_PROXY_RET (-1);
	}
	if (get_callid(msg, &callid) == -1 || callid.len == 0) {
		LM_ERR("can't get Call-Id field\n");
		FORCE_RTP_PROXY_RET (-1);
	}
	to_tag.s = 0;
	if (get_to_tag(msg, &to_tag) == -1) {
		LM_ERR("can't get To tag\n");
		FORCE_RTP_PROXY_RET (-1);
	}
	if (get_from_tag(msg, &from_tag) == -1 || from_tag.len == 0) {
		LM_ERR("can't get From tag\n");
		FORCE_RTP_PROXY_RET (-1);
	}
	if (via) {
		if (via == 1)
			ret = get_via_branch(msg, 1, &viabranch);
		else /* (via == 2) */
			ret = get_via_branch(msg, 2, &viabranch);
		if (ret == -1 || viabranch.len == 0) {
			LM_ERR("can't get Via branch\n");
			FORCE_RTP_PROXY_RET (-1);
		}
		v[6].iov_base = ";";
		v[6].iov_len = 1;
		STR2IOVEC(viabranch, v[7]);
	} else
	/* Append extra id to call-id */
	if (extra && extra_id_pv && get_extra_id(msg, &extra_id)) {
		v[6].iov_base = ";";
		v[6].iov_len = 1;
		STR2IOVEC(extra_id, v[7]);
	}
	if (flookup != 0) {
		if (to_tag.len == 0) {
			FORCE_RTP_PROXY_RET (-1);
		}
		if (msg->first_line.type == SIP_REQUEST) {
			tmp = from_tag;
			from_tag = to_tag;
			to_tag = tmp;
		}
		create = 0;
	} else if ((msg->first_line.type == SIP_REPLY && offer != 0)
			|| (msg->first_line.type == SIP_REQUEST && offer == 0)) {
		if (to_tag.len == 0) {
			FORCE_RTP_PROXY_RET (-1);
		}
		tmp = from_tag;
		from_tag = to_tag;
		to_tag = tmp;
	}
	proxied = 0;
	if (nortpproxy_str.len) {
		for ( cp=body.s ; (len=body.s+body.len-cp) >= nortpproxy_str.len ; ) {
			cp1 = ser_memmem(cp, nortpproxy_str.s, len, nortpproxy_str.len);
			if (cp1 == NULL)
				break;
			if (cp1[-1] == '\n' || cp1[-1] == '\r') {
				proxied = 1;
				break;
			}
			cp = cp1 + nortpproxy_str.len;
		}
	}
	if (proxied != 0 && force == 0) {
		FORCE_RTP_PROXY_RET (-2);
	}
	/*
	 * Parsing of SDP body.
	 * It can contain a few session descriptions (each starts with
	 * v-line), and each session may contain a few media descriptions
	 * (each starts with m-line).
	 * We have to change ports in m-lines, and also change IP addresses in
	 * c-lines which can be placed either in session header (fallback for
	 * all medias) or media description.
	 * Ports should be allocated for any media. IPs all should be changed
	 * to the same value (RTP proxy IP), so we can change all c-lines
	 * unconditionally.
	 */
	if(0 != parse_sdp(msg)) {
		LM_ERR("Unable to parse sdp\n");
		FORCE_RTP_PROXY_RET (-1);
	}
#ifdef EXTRA_DEBUG
	print_sdp((sdp_info_t*)msg->body, L_DBG);
#endif

	bodylimit = body.s + body.len;

	if(msg->id != current_msg_id){
		selected_rtpp_set = default_rtpp_set;
	}

	opts.s.s[0] = (create == 0) ? 'L' : 'U';
	v[1].iov_base = opts.s.s;
	v[1].iov_len = opts.oidx;
	STR2IOVEC(callid, v[5]);
	STR2IOVEC(from_tag, v[13]);
	STR2IOVEC(to_tag, v[17]);

	if (ice_candidate_priority_avp_param) {
	    if (search_first_avp(ice_candidate_priority_avp_type,
				 ice_candidate_priority_avp,
				 &ice_candidate_priority_val, 0)
		== NULL) {
		ice_candidate_priority_val.n = 2;
	    } else if ((ice_candidate_priority_val.n < 1) ||
		       (ice_candidate_priority_val.n > 2)) {
		LM_ERR("invalid ice candidate priority value %d\n",
		       ice_candidate_priority_val.n);
		FORCE_RTP_PROXY_RET (-1);
	    }
	} else {
	    ice_candidate_priority_val.n = 0;
	}

	/* check if this is a single or a multi stream SDP offer/answer */
	sdp_stream_num = get_sdp_stream_num(msg);
	switch (sdp_stream_num) {
	case 0:
		LM_ERR("sdp w/o streams\n");
		FORCE_RTP_PROXY_RET (-1);
		break;
	case 1:
		media_multi = 0;
		break;
	default:
		media_multi = 1;
	}
#ifdef EXTRA_DEBUG
	LM_DBG("my new media_multi=%d\n", media_multi);
#endif
	medianum = 0; 
	sdp_session_num = 0;
	for(;;) {
		sdp_session = get_sdp_session(msg, sdp_session_num);
		if(!sdp_session) break;
		sdp_stream_num = 0;
		c1p_altered = 0;
		o1p = sdp_session->o_ip_addr.s;
		for(;;) {
			sdp_stream = get_sdp_stream(msg, sdp_session_num, sdp_stream_num);
			if (!sdp_stream ||
			    (ice_candidate_priority_val.n && sdp_stream->remote_candidates.len)) break;

			if (sdp_stream->ip_addr.s && sdp_stream->ip_addr.len>0) {
				oldip = sdp_stream->ip_addr;
				pf = sdp_stream->pf;
			} else {
				oldip = sdp_session->ip_addr;
				pf = sdp_session->pf;
			}
			oldport = sdp_stream->port;
			payload_types = sdp_stream->payloads;
			medianum++;

			if (real != 0) {
				newip = oldip;
			} else {
				newip.s = ip_addr2a(&msg->rcv.src_ip);
				newip.len = strlen(newip.s);
			}
			/* XXX must compare address families in all addresses */
			if (pf == AF_INET6) {
				if (autobridge_ipv4v6 != 0) {
					if ((append_opts(&opts, 'E') == -1) || (append_opts(&opts, 'I') == -1))  {
						LM_ERR("out of pkg memory\n");
						FORCE_RTP_PROXY_RET (-1);
					}
					/* Only execute once */
					autobridge_ipv4v6 = 0;
				}
				if (append_opts(&opts, '6') == -1) {
					LM_ERR("out of pkg memory\n");
					FORCE_RTP_PROXY_RET (-1);
				}
				/* We need to update the pointers and the length here, it has changed. */
				v[1].iov_base = opts.s.s;
				v[1].iov_len = opts.oidx;
			} else {
				if (autobridge_ipv4v6 != 0) {
					if ((append_opts(&opts, 'I') == -1) || (append_opts(&opts, 'E') == -1))  {
						LM_ERR("out of pkg memory\n");
						FORCE_RTP_PROXY_RET (-1);
					}
					/* We need to update the pointers and the length here, it has changed. */
					v[1].iov_base = opts.s.s;
					v[1].iov_len = opts.oidx;
					/* Only execute once */
					autobridge_ipv4v6 = 0;
				}
			}

			STR2IOVEC(newip, v[9]);
			STR2IOVEC(oldport, v[11]);
#ifdef EXTRA_DEBUG
			LM_DBG("STR2IOVEC(newip[%.*s], v[9])", newip.len, newip.s);
			LM_DBG("STR2IOVEC(oldport[%.*s], v[11])", oldport.len, oldport.s);
#endif
			if (1 || media_multi) /* XXX netch: can't choose now*/
			{
				snprintf(itoabuf_buf, sizeof itoabuf_buf, "%d", medianum);
				itoabuf_str.s = itoabuf_buf;
				itoabuf_str.len = strlen(itoabuf_buf);
				STR2IOVEC(itoabuf_str, v[15]);
				STR2IOVEC(itoabuf_str, v[19]);
#ifdef EXTRA_DEBUG
				LM_DBG("STR2IOVEC(itoabuf_str, v[15])\n");
				LM_DBG("STR2IOVEC(itoabuf_str, v[19])\n");
#endif
			} else {
				v[14].iov_len = v[15].iov_len = 0;
				v[18].iov_len = v[19].iov_len = 0;
			}
			do {
				node = select_rtpp_node(callid, 1);
				if (!node) {
					LM_ERR("no available proxies\n");
					FORCE_RTP_PROXY_RET (-3);
				}
				set_rtp_inst_pvar(msg, &node->rn_url);
				if (rep_opts.oidx > 0) {
					if (node->rn_rep_supported == 0) {
						LM_WARN("re-packetization is requested but is not "
						    "supported by the selected RTP proxy node\n");
						v[2].iov_len = 0;
					} else {
						v[2].iov_base = rep_opts.s.s;
						v[2].iov_len += rep_opts.oidx;
					}
				}
#ifdef EXTRA_DEBUG
				LM_DBG("payload_types='%.*s'\n", payload_types.len, payload_types.s);
#endif
				if (sdp_stream->is_rtp && payload_types.len > 0 && node->rn_ptl_supported != 0) {
					pt_opts.oidx = 0;
					if (append_opts(&pt_opts, 'c') == -1) {
						LM_ERR("out of pkg memory\n");
						FORCE_RTP_PROXY_RET (-1);
					}
					/*
					 * Convert space-separated payload types list into
					 * a comma-separated list.
					 */
					for (cp = payload_types.s;
					    cp < payload_types.s + payload_types.len; cp++) {
						if (isdigit(*cp)) {
							if (append_opts(&pt_opts, *cp) == -1) {
								LM_ERR("out of pkg memory\n");
								FORCE_RTP_PROXY_RET (-1);
							}
							continue;
						}
						do {
							cp++;
						} while (!isdigit(*cp) &&
						    cp < payload_types.s + payload_types.len);
						/* Check EOL */
						if (cp >= payload_types.s + payload_types.len)
							break;
						if (append_opts(&pt_opts, ',') == -1) {
							LM_ERR("out of pkg memory\n");
							FORCE_RTP_PROXY_RET (-1);
						}
						cp--;
					}
					v[3].iov_base = pt_opts.s.s;
					v[3].iov_len = pt_opts.oidx;
				} else {
					v[3].iov_len = 0;
				}
				if (to_tag.len > 0) {
					iovec_param_count = 20;
					if (opts.s.s[0] == 'U' && timeout_socket_str.len > 0) {
						iovec_param_count = 22;
						STR2IOVEC(timeout_socket_str, v[21]);
					}
				} else {
					iovec_param_count = 16;
				}

				cp = send_rtpp_command(node, v, iovec_param_count);
			} while (cp == NULL);
			LM_DBG("proxy reply: %s\n", cp);
			/* Parse proxy reply to <argc,argv> */
			argc = 0;
			memset(argv, 0, sizeof(argv));
			cpend=cp+strlen(cp);
			next=eat_token_end(cp, cpend);
			for (ap=argv; cp<cpend; cp=next+1, next=eat_token_end(cp, cpend)){
				*next=0;
				if (*cp != '\0') {
					*ap=cp;
					argc++;
					if ((char*)++ap >= ((char*)argv+sizeof(argv)))
						break;
				}
			}
			if (argc < 1) {
				LM_ERR("no reply from rtp proxy\n");
				FORCE_RTP_PROXY_RET (-1);
			}
			port = atoi(argv[0]);
			if (port <= 0 || port > 65535) {
				if (port != 0 || flookup == 0)
					LM_ERR("incorrect port %i in reply "
						"from rtp proxy\n",port);
				FORCE_RTP_PROXY_RET (-1);
			}

			pf1 = (argc >= 3 && argv[2][0] == '6') ? AF_INET6 : AF_INET;

			if (isnulladdr(&oldip, pf)) {
				if (pf1 == AF_INET6) {
					newip.s = "::";
					newip.len = 2;
				} else {
					newip.s = "0.0.0.0";
					newip.len = 7;
				}
			} else {
				if (forcedIP) {
					newip.s = str2;
					newip.len = strlen(newip.s);
#ifdef EXTRA_DEBUG
					LM_DBG("forcing IP='%.*s'\n", newip.len, newip.s);
#endif
				} else {
					newip.s = (argc < 2) ? str2 : argv[1];
					newip.len = strlen(newip.s);
				}
			}
			/* marker to double check : newport goes: str -> int -> str ?!?! */
			newport.s = int2str(port, &newport.len); /* beware static buffer */
			/* Alter port. */
			body1.s = sdp_stream->media.s;
			body1.len = bodylimit - body1.s;
#ifdef EXTRA_DEBUG
			LM_DBG("alter port body1='%.*s'\n", body1.len, body1.s);
#endif
			/* do not do it if old port was 0 (means media disable)
			 * - check if actually should be better done in rtpptoxy,
			 *   by returning also 0
			 * - or by not sending to rtpproxy the old port if 0
			 */
			if(oldport.len!=1 || oldport.s[0]!='0')
			{
				if (alter_mediaport(msg, &body1, &oldport, &newport, 0) == -1) {
					FORCE_RTP_PROXY_RET (-1);
				}
			}
			
			/*
			 * Alter RTCP attribute if present. Inserting RTP port + 1 (as allocated
			 * by RTP proxy). No IP-address is needed in the new RTCP attribute as the
			 * 'c' attribute (altered below) will contain the RTP proxy IP address.
			 * See RFC 3605 for definition of RTCP attribute.
			 * ported from ser
			 */

			if (sdp_stream->rtcp_port.s && sdp_stream->rtcp_port.len) {
				newrtcp.s = int2str(port+1, &newrtcp.len); /* beware static buffer */
				/* Alter port. */
				body1.s = sdp_stream->rtcp_port.s;
				body1.len = bodylimit - body1.s;
#ifdef EXTRA_DEBUG
				LM_DBG("alter rtcp body1='%.*s'\n", body1.len, body1.s);
#endif
				if (alter_rtcp(msg, &body1, &sdp_stream->rtcp_port, &newrtcp) == -1) {
					FORCE_RTP_PROXY_RET (-1);
				}
			}

			/* Add ice relay candidates */
			if (ice_candidate_priority_val.n && sdp_stream->ice_attrs_num > 0) {
				body1.s = sdp_stream->ice_attr->foundation.s - 12;
				body1.len = bodylimit - body1.s;
				if (insert_candidates(msg, sdp_stream->ice_attr->foundation.s - 12,
						&newip, port, ice_candidate_priority_val.n) == -1) {
					FORCE_RTP_PROXY_RET (-1);
				}
			}

			c1p = sdp_session->ip_addr.s;
			c2p = sdp_stream->ip_addr.s;
			/*
			 * Alter IP. Don't alter IP common for the session
			 * more than once.
			 */
			if (c2p != NULL || !c1p_altered) {
				body1.s = c2p ? c2p : c1p;
				body1.len = bodylimit - body1.s;
#ifdef EXTRA_DEBUG
				LM_DBG("alter ip body1='%.*s'\n", body1.len, body1.s);
#endif
				if (alter_mediaip(msg, &body1, &oldip, pf, &newip, pf1, 0)==-1) {
					FORCE_RTP_PROXY_RET (-1);
				}
				if (!c2p)
					c1p_altered = 1;
			}
			/*
			 * Alter common IP if required, but don't do it more than once.
			 */
			if (commip && c1p && !c1p_altered) {
				body1.s = c1p;
				body1.len = bodylimit - body1.s;
#ifdef EXTRA_DEBUG
				LM_DBG("alter common ip body1='%.*s'\n", body1.len, body1.s);
#endif
				if (alter_mediaip(msg, &body1, &sdp_session->ip_addr, sdp_session->pf, &newip, pf1, 0)==-1) {
					FORCE_RTP_PROXY_RET (-1);
				}
				c1p_altered = 1;
			}
			/*
			 * Alter the IP in "o=", but only once per session
			 */
			if (o1p) {
				body1.s = o1p;
				body1.len = bodylimit - body1.s;
#ifdef EXTRA_DEBUG
				LM_DBG("alter media ip body1='%.*s'\n", body1.len, body1.s);
#endif
				if (alter_mediaip(msg, &body1, &sdp_session->o_ip_addr, sdp_session->o_pf, &newip, pf1, 0)==-1) {
					FORCE_RTP_PROXY_RET (-1);
				}
				o1p = 0;
			}
			sdp_stream_num++;
		}
		sdp_session_num++;
	}



	free_opts(&opts, &rep_opts, &pt_opts);

	if (proxied == 0 && nortpproxy_str.len) {
		cp = pkg_malloc((nortpproxy_str.len + CRLF_LEN) * sizeof(char));
		if (cp == NULL) {
			LM_ERR("out of pkg memory\n");
			return -1;
		}
		anchor = anchor_lump(msg, body.s + body.len - msg->buf, 0, 0);
		if (anchor == NULL) {
			LM_ERR("anchor_lump failed\n");
			pkg_free(cp);
			return -1;
		}
		memcpy(cp, CRLF, CRLF_LEN);
		memcpy(cp + CRLF_LEN, nortpproxy_str.s, nortpproxy_str.len);
		if (insert_new_lump_after(anchor, cp, nortpproxy_str.len + CRLF_LEN, 0) == NULL) {
			LM_ERR("insert_new_lump_after failed\n");
			pkg_free(cp);
			return -1;
		}
	}

	return 1;
}


static int start_recording_f(struct sip_msg* msg, char *foo, char *bar)
{
	int nitems;
	str callid = {0, 0};
	str from_tag = {0, 0};
	str to_tag = {0, 0};
	struct rtpp_node *node;
	struct iovec v[1 + 4 + 3] = {{NULL, 0}, {"R", 1}, {" ", 1}, {NULL, 0}, {" ", 1}, {NULL, 0}, {" ", 1}, {NULL, 0}};
	                             /* 1 */   /* 2 */   /* 3 */    /* 4 */   /* 5 */    /* 6 */   /* 1 */

	if (get_callid(msg, &callid) == -1 || callid.len == 0) {
		LM_ERR("can't get Call-Id field\n");
		return -1;
	}

	if (get_to_tag(msg, &to_tag) == -1) {
		LM_ERR("can't get To tag\n");
		return -1;
	}

	if (get_from_tag(msg, &from_tag) == -1 || from_tag.len == 0) {
		LM_ERR("can't get From tag\n");
		return -1;
	}

	if(msg->id != current_msg_id){
		selected_rtpp_set = default_rtpp_set;
	}

	STR2IOVEC(callid, v[3]);
	STR2IOVEC(from_tag, v[5]);
	STR2IOVEC(to_tag, v[7]);
	node = select_rtpp_node(callid, 1);
	if (!node) {
		LM_ERR("no available proxies\n");
		return -1;
	}
	set_rtp_inst_pvar(msg, &node->rn_url);

	nitems = 8;
	if (msg->first_line.type == SIP_REPLY) {
		if (to_tag.len == 0)
			return -1;
		STR2IOVEC(to_tag, v[5]);
		STR2IOVEC(from_tag, v[7]);
	} else {
		STR2IOVEC(from_tag, v[5]);
		STR2IOVEC(to_tag, v[7]);
		if (to_tag.len <= 0)
			nitems = 6;
	}
	send_rtpp_command(node, v, nitems);

	return 1;
}

/*
 * Returns the current RTP-Statistics from the RTP-Proxy
 */
static int
pv_get_rtpstat_f(struct sip_msg *msg, pv_param_t *param,
		  pv_value_t *res)
{
    str ret_val = {0, 0};
    int nitems;
    str callid = {0, 0};
    str from_tag = {0, 0};
    str to_tag = {0, 0};
    struct rtpp_node *node;
    struct iovec v[1 + 4 + 3 + 1] = {{NULL, 0}, {"Q", 1}, {" ", 1}, {NULL, 0},
		{" ", 1}, {NULL, 0}, {";1 ", 3}, {";1", }, {NULL, 0}};

    if (get_callid(msg, &callid) == -1 || callid.len == 0) {
        LM_ERR("can't get Call-Id field\n");
		return pv_get_null(msg, param, res);
    }
    if (get_to_tag(msg, &to_tag) == -1) {
        LM_ERR("can't get To tag\n");
		return pv_get_null(msg, param, res);
    }
    if (get_from_tag(msg, &from_tag) == -1 || from_tag.len == 0) {
        LM_ERR("can't get From tag\n");
		return pv_get_null(msg, param, res);
    }
    if(msg->id != current_msg_id){
        selected_rtpp_set = default_rtpp_set;
    }

    STR2IOVEC(callid, v[3]);
    STR2IOVEC(from_tag, v[5]);
    STR2IOVEC(to_tag, v[7]);
    node = select_rtpp_node(callid, 1);
    if (!node) {
        LM_ERR("no available proxies\n");
        return -1;
    }
    set_rtp_inst_pvar(msg, &node->rn_url);
    nitems = 8;
    if (msg->first_line.type == SIP_REPLY) {
        if (to_tag.len == 0)
            return -1;
        STR2IOVEC(to_tag, v[5]);
        STR2IOVEC(from_tag, v[7]);
    } else {
        STR2IOVEC(from_tag, v[5]);
        STR2IOVEC(to_tag, v[7]);
        if (to_tag.len <= 0)
            nitems = 6;
    }
    ret_val.s = send_rtpp_command(node, v, nitems);
	if(ret_val.s==NULL)
		return pv_get_null(msg, param, res);
    ret_val.len = strlen(ret_val.s);
    return pv_get_strval(msg, param, res, &ret_val);
}

int set_rtp_inst_pvar(struct sip_msg *msg, const str * const uri) {
	pv_value_t val;

	if (rtp_inst_pvar == NULL)
		return 0;

	memset(&val, 0, sizeof(pv_value_t));
	val.flags = PV_VAL_STR;
	val.rs = *uri;

	if (rtp_inst_pvar->setf(msg, &rtp_inst_pvar->pvp, (int)EQ_T, &val) < 0)
	{
		LM_ERR("Failed to add RTPProxy URI to pvar\n");
		return -1;
	}
	return 0;
}

