/*
 * Copyright (C) 2003-2008 Sippy Software, Inc., http://www.sippysoft.com
 * Copyright (C) 2014-2015 Sipwise GmbH, http://www.sipwise.com
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
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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
#include "rtpengine.h"
#include "rtpengine_funcs.h"
#include "bencode.h"

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
#define MAX_RTPP_TRIED_NODES    50
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
#define MI_DISABLED_PERMANENT        "1 (permanent)"
#define MI_DISABLED_PERMANENT_LEN    (sizeof(MI_DISABLED_PERMANENT)-1)
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



#define	CPORT		"22222"

enum rtpe_operation {
	OP_OFFER = 1,
	OP_ANSWER,
	OP_DELETE,
	OP_START_RECORDING,
	OP_QUERY,
};

struct ng_flags_parse {
	int via, to, packetize, transport;
	bencode_item_t *dict, *flags, *direction, *replace, *rtcp_mux;
};

static const char *command_strings[] = {
	[OP_OFFER]		= "offer",
	[OP_ANSWER]		= "answer",
	[OP_DELETE]		= "delete",
	[OP_START_RECORDING]	= "start recording",
	[OP_QUERY]		= "query",
};

static char *gencookie();
static int rtpp_test(struct rtpp_node*, int, int);
static int start_recording_f(struct sip_msg *, char *, char *);
static int rtpengine_answer1_f(struct sip_msg *, char *, char *);
static int rtpengine_offer1_f(struct sip_msg *, char *, char *);
static int rtpengine_delete1_f(struct sip_msg *, char *, char *);
static int rtpengine_manage1_f(struct sip_msg *, char *, char *);

static int parse_flags(struct ng_flags_parse *, struct sip_msg *, enum rtpe_operation *, const char *);

static int rtpengine_offer_answer(struct sip_msg *msg, const char *flags, int op, int more);
static int fixup_set_id(void ** param, int param_no);
static int set_rtpengine_set_f(struct sip_msg * msg, char * str1, char * str2);
static struct rtpp_set * select_rtpp_set(int id_set);
static struct rtpp_node *select_rtpp_node(str, int);
static char *send_rtpp_command(struct rtpp_node *, bencode_item_t *, int *);
static int get_extra_id(struct sip_msg* msg, str *id_str);

static int rtpengine_set_store(modparam_t type, void * val);
static int rtpengine_add_rtpengine_set( char * rtp_proxies);

static int mod_init(void);
static int child_init(int);
static void mod_destroy(void);

static int get_ip_type(char *str_addr);
static int get_ip_scope(char *str_addr); // useful for link-local ipv6
static int bind_force_send_ip(int sock_idx);

/* Pseudo-Variables */
static int pv_get_rtpstat_f(struct sip_msg *, pv_param_t *, pv_value_t *);

/*mi commands*/
static struct mi_root* mi_enable_rtp_proxy(struct mi_root* cmd_tree,
		void* param );
static struct mi_root* mi_show_rtpproxies(struct mi_root* cmd_tree,
		void* param);


static int rtpengine_disable_tout = 60;
static int rtpengine_retr = 5;
static int rtpengine_tout_ms = 1000;
static int queried_nodes_limit = MAX_RTPP_TRIED_NODES;
static pid_t mypid;
static unsigned int myseqn = 0;
static str extra_id_pv_param = {NULL, 0};
static char *setid_avp_param = NULL;

static char ** rtpp_strings=0;
static int rtpp_sets=0; /*used in rtpengine_set_store()*/
static int rtpp_set_count = 0;
static unsigned int current_msg_id = (unsigned int)-1;
/* RTP proxy balancing list */
static struct rtpp_set_head * rtpp_set_list =0;
static struct rtpp_set * active_rtpp_set =0;
static struct rtpp_set * selected_rtpp_set_1 =0;
static struct rtpp_set * selected_rtpp_set_2 =0;
static struct rtpp_set * default_rtpp_set=0;

static str body_intermediate;

static str rtp_inst_pv_param = {NULL, 0};
static pv_spec_t *rtp_inst_pvar = NULL;

/* array with the sockets used by rtpporxy (per process)*/
static unsigned int rtpp_no = 0;
static int *rtpp_socks = 0;

static int     setid_avp_type;
static int_str setid_avp;

static str            write_sdp_pvar_str = {NULL, 0};
static pv_spec_t*     write_sdp_pvar = NULL;


char* force_send_ip_str="";
int force_send_ip_af = AF_UNSPEC;

typedef struct rtpp_set_link {
	struct rtpp_set *rset;
	pv_spec_t *rpv;
} rtpp_set_link_t;

/* tm */
static struct tm_binds tmb;

/*0-> disabled, 1 ->enabled*/
unsigned int *natping_state=0;

static pv_elem_t *extra_id_pv = NULL;

static cmd_export_t cmds[] = {
	{"set_rtpengine_set",  (cmd_function)set_rtpengine_set_f,    1,
		fixup_set_id, 0,
		ANY_ROUTE},
	{"set_rtpengine_set",  (cmd_function)set_rtpengine_set_f,    2,
		fixup_set_id, 0,
		ANY_ROUTE},
	{"start_recording",    (cmd_function)start_recording_f,      0,
		0, 0,
		ANY_ROUTE },
	{"rtpengine_offer",	(cmd_function)rtpengine_offer1_f,     0,
		0, 0,
		ANY_ROUTE},
	{"rtpengine_offer",	(cmd_function)rtpengine_offer1_f,     1,
		fixup_spve_null, 0,
		ANY_ROUTE},
	{"rtpengine_answer",	(cmd_function)rtpengine_answer1_f,    0,
		0, 0,
		ANY_ROUTE},
	{"rtpengine_answer",	(cmd_function)rtpengine_answer1_f,    1,
		fixup_spve_null, 0,
		ANY_ROUTE},
	{"rtpengine_manage",	(cmd_function)rtpengine_manage1_f,     0,
		0, 0,
		ANY_ROUTE},
	{"rtpengine_manage",	(cmd_function)rtpengine_manage1_f,     1,
		fixup_spve_null, 0,
		ANY_ROUTE},
	{"rtpengine_delete",  (cmd_function)rtpengine_delete1_f,    0,
		0, 0,
		ANY_ROUTE},
	{"rtpengine_delete",  (cmd_function)rtpengine_delete1_f,    1,
		fixup_spve_null, 0,
		ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static pv_export_t mod_pvs[] = {
    {{"rtpstat", (sizeof("rtpstat")-1)}, /* RTP-Statistics */
     PVT_OTHER, pv_get_rtpstat_f, 0, 0, 0, 0, 0},
    {{0, 0}, 0, 0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
	{"rtpengine_sock",        PARAM_STRING|USE_FUNC_PARAM,
	                         (void*)rtpengine_set_store          },
	{"rtpengine_disable_tout",INT_PARAM, &rtpengine_disable_tout },
	{"rtpengine_retr",        INT_PARAM, &rtpengine_retr         },
	{"rtpengine_tout_ms",     INT_PARAM, &rtpengine_tout_ms      },
	{"queried_nodes_limit",   INT_PARAM, &queried_nodes_limit    },
	{"db_url",                PARAM_STR, &rtpp_db_url },
	{"table_name",            PARAM_STR, &rtpp_table_name },
	{"url_col",               PARAM_STR, &rtpp_url_col },
	{"extra_id_pv",           PARAM_STR, &extra_id_pv_param },
	{"setid_avp",             PARAM_STRING, &setid_avp_param },
	{"force_send_interface",  PARAM_STRING, &force_send_ip_str	},
	{"rtp_inst_pvar",         PARAM_STR, &rtp_inst_pv_param },
	{"write_sdp_pv",          PARAM_STR, &write_sdp_pvar_str          },
	{0, 0, 0}
};

static mi_export_t mi_cmds[] = {
	{MI_ENABLE_RTP_PROXY,     mi_enable_rtp_proxy,  0,                0, 0},
	{MI_SHOW_RTP_PROXIES,     mi_show_rtpproxies,   MI_NO_INPUT_FLAG, 0, 0},
	{ 0, 0, 0, 0, 0}
};


struct module_exports exports = {
	"rtpengine",
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


static int get_ip_type(char *str_addr)
{
	struct addrinfo hint, *info = NULL;
	int ret;

	memset(&hint, '\0', sizeof hint);
	hint.ai_family = PF_UNSPEC;
	hint.ai_flags = AI_NUMERICHOST;

	ret = getaddrinfo(str_addr, NULL, &hint, &info);
	if (ret) {
		/* Invalid ip addinfos */
		return -1;
	}

	if(info->ai_family == AF_INET) {
		LM_DBG("%s is an ipv4 addinfos\n", str_addr);
	} else if (info->ai_family == AF_INET6) {
		LM_DBG("%s is an ipv6 addinfos\n", str_addr);
	} else {
		LM_DBG("%s is an unknown addinfos format AF=%d\n",str_addr, info->ai_family);
		return -1;
	}

	ret = info->ai_family;

	freeaddrinfo(info);

	return ret;
}


static int get_ip_scope(char *str_addr)
{
	struct ifaddrs *ifaddr, *ifa;
	struct sockaddr_in6 *in6;
	char str_if_ip[NI_MAXHOST];
	int ret = -1;

	if (getifaddrs(&ifaddr) == -1) {
		LM_ERR("getifaddrs() failed: %s\n", gai_strerror(ret));
		return -1;
	}

	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		in6 = (struct sockaddr_in6 *)ifa->ifa_addr;

		if (ifa->ifa_addr == NULL)
			continue;

		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;

		ret = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in6),
		str_if_ip, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
		if (ret != 0) {
			LM_ERR("getnameinfo() failed: %s\n", gai_strerror(ret));
			return -1;
		}

		if (strstr(str_if_ip, str_addr)) {
			LM_INFO("dev: %-8s address: <%s> scope %d\n",
			ifa->ifa_name, str_if_ip, in6->sin6_scope_id);
			ret = in6->sin6_scope_id;
			break;
		}
	}

	freeifaddrs(ifaddr);

	return ret;
}


static int bind_force_send_ip(int sock_idx)
{
	struct sockaddr_in tmp, ip4addr;
	struct sockaddr_in6 tmp6, ip6addr;
	char str_addr[INET_ADDRSTRLEN];
	char str_addr6[INET6_ADDRSTRLEN];
	socklen_t sock_len = sizeof(struct sockaddr);
	int ret, scope;

	switch (force_send_ip_af) {
		case AF_INET:
			memset(&ip4addr, 0, sizeof(ip4addr));
			ip4addr.sin_family = AF_INET;
			ip4addr.sin_port = htons(0);
			inet_pton(AF_INET, force_send_ip_str, &ip4addr.sin_addr);

			if (bind(rtpp_socks[sock_idx], (struct sockaddr*)&ip4addr, sizeof(ip4addr)) < 0) {
				LM_ERR("can't bind socket to required ipv4 interface\n");
				return -1;
			}

			memset(&tmp, 0, sizeof(tmp));
			getsockname(rtpp_socks[sock_idx], (struct sockaddr *) &tmp, &sock_len);
			inet_ntop(AF_INET, &tmp.sin_addr, str_addr, INET_ADDRSTRLEN);
			LM_INFO("Binding on %s:%d\n", str_addr, ntohs(tmp.sin_port));

			break;

		case AF_INET6:
			if ((scope = get_ip_scope(force_send_ip_str)) < 0) {
				LM_ERR("can't get the ipv6 interface scope\n");
				return -1;
			}
			memset(&ip6addr, 0, sizeof(ip6addr));
			ip6addr.sin6_family = AF_INET6;
			ip6addr.sin6_port = htons(0);
			ip6addr.sin6_scope_id = scope;
			inet_pton(AF_INET6, force_send_ip_str, &ip6addr.sin6_addr);

			if ((ret = bind(rtpp_socks[sock_idx], (struct sockaddr*)&ip6addr, sizeof(ip6addr))) < 0) {
				LM_ERR("can't bind socket to required ipv6 interface\n");
				LM_ERR("ret=%d errno=%d\n", ret, errno);
				return -1;
			}

			memset(&tmp6, 0, sizeof(tmp6));
			getsockname(rtpp_socks[sock_idx], (struct sockaddr *) &tmp6, &sock_len);
			inet_ntop(AF_INET6, &tmp6.sin6_addr, str_addr6, INET6_ADDRSTRLEN);
			LM_INFO("Binding on ipv6 %s:%d\n", str_addr6, ntohs(tmp6.sin6_port));

			break;

		default:
			LM_DBG("force_send_ip_str not specified in .cfg file!\n");
			break;
	}

	return 0;
}


static inline int str_eq(const str *p, const char *q) {
	int l = strlen(q);
	if (p->len != l)
		return 0;
	if (memcmp(p->s, q, l))
		return 0;
	return 1;
}
static inline str str_prefix(const str *p, const char *q) {
	str ret;
	ret.s = NULL;
	int l = strlen(q);
	if (p->len < l)
		return ret;
	if (memcmp(p->s, q, l))
		return ret;
	ret = *p;
	ret.s += l;
	ret.len -= l;
	return ret;
}


static int rtpengine_set_store(modparam_t type, void * val){

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

struct rtpp_set *get_rtpp_set(int set_id)
{
	struct rtpp_set * rtpp_list;
	unsigned int my_current_id = 0;
	int new_list;

	if (set_id < DEFAULT_RTPP_SET_ID )
	{
		LM_ERR(" invalid rtpproxy set value [%d]\n",
		       set_id);
		return NULL;
	}

	my_current_id = set_id;
	/*search for the current_id*/
	rtpp_list = rtpp_set_list ? rtpp_set_list->rset_first : 0;
	while( rtpp_list != 0 && rtpp_list->id_set!=my_current_id)
		rtpp_list = rtpp_list->rset_next;

	if (rtpp_list==NULL)
	{	/*if a new id_set : add a new set of rtpp*/
		rtpp_list = shm_malloc(sizeof(struct rtpp_set));
		if(!rtpp_list)
		{
			LM_ERR("no shm memory left to create new rtpproxy set %d\n", my_current_id);
			return NULL;
		}
		memset(rtpp_list, 0, sizeof(struct rtpp_set));
		rtpp_list->id_set = my_current_id;
		new_list = 1;
	}
	else {
		new_list = 0;
	}

	if (new_list)
	{
		if(!rtpp_set_list){/*initialize the list of set*/
			rtpp_set_list = shm_malloc(sizeof(struct rtpp_set_head));
			if(!rtpp_set_list){
				LM_ERR("no shm memory left to create list of proxysets\n");
				return NULL;
			}
			memset(rtpp_set_list, 0, sizeof(struct rtpp_set_head));
		}

		/*update the list of set info*/
		if (!rtpp_set_list->rset_first)
		{
			rtpp_set_list->rset_first = rtpp_list;
		}
		else
		{
			rtpp_set_list->rset_last->rset_next = rtpp_list;
		}

		rtpp_set_list->rset_last = rtpp_list;
		rtpp_set_count++;

		if(my_current_id == DEFAULT_RTPP_SET_ID){
			default_rtpp_set = rtpp_list;
		}
	}
	return rtpp_list;
}


int add_rtpengine_socks(struct rtpp_set * rtpp_list, char * rtpproxy)
{
	/* Make rtp proxies list. */
	char *p, *p1, *p2, *plim;
	struct rtpp_node *pnode;
	int weight;

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
		pnode = shm_malloc(sizeof(struct rtpp_node));
		if (pnode == NULL) {
			LM_ERR("no shm memory left\n");
			return -1;
		}
		memset(pnode, 0, sizeof(*pnode));
		pnode->idx = rtpp_no++;
		pnode->rn_recheck_ticks = 0;
		pnode->rn_weight = weight;
		pnode->rn_umode = 0;
		pnode->rn_disabled = 0;
		pnode->rn_url.s = shm_malloc(p2 - p1 + 1);
		if (pnode->rn_url.s == NULL) {
			shm_free(pnode);
			LM_ERR("no shm memory left\n");
			return -1;
		}
		memmove(pnode->rn_url.s, p1, p2 - p1);
		pnode->rn_url.s[p2 - p1] 	= 0;
		pnode->rn_url.len 			= p2-p1;

		/* Leave only address in rn_address */
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

		if (rtpp_list->rn_first == NULL) {
			rtpp_list->rn_first = pnode;
		} else {
			rtpp_list->rn_last->rn_next = pnode;
		}

		rtpp_list->rn_last = pnode;
		rtpp_list->rtpp_node_count++;
	}
	return 0;
}


/*	0-succes
 *  -1 - erorr
 * */
static int rtpengine_add_rtpengine_set( char * rtp_proxies)
{
	char *p,*p2;
	struct rtpp_set * rtpp_list;
	unsigned int my_current_id;
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

		if(id_set.len <= 0 ||str2int(&id_set, &my_current_id)<0 ){
		LM_ERR("script error -invalid set_id value!\n");
			return -1;
		}

		rtp_proxies+=2;
	}else{
		rtp_proxies = p;
		my_current_id = DEFAULT_RTPP_SET_ID;
	}

	for(;*rtp_proxies && isspace(*rtp_proxies);rtp_proxies++);

	if(!(*rtp_proxies)){
		LM_ERR("script error -empty rtp_proxy list\n");
		return -1;;
	}

	/*search for the current_id*/
	rtpp_list = get_rtpp_set(my_current_id);

	if (rtpp_list != NULL)
	{
		if (add_rtpengine_socks(rtpp_list, rtp_proxies) != 0)
			goto error;
		else
			return 0;
	}

error:
	return -1;
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
			
			if (( 1 == crt_rtpp->rn_disabled ) && ( crt_rtpp->rn_recheck_ticks == MI_MAX_RECHECK_TICKS)) {
				if( !(child = add_mi_node_child(crt_node, MI_DUP_VALUE, MI_DISABLED, MI_DISABLED_LEN,
								MI_DISABLED_PERMANENT, MI_DISABLED_PERMANENT_LEN))) {
					LM_ERR("cannot add disabled (permanent) message\n");
					goto error;
				}
			}
			else {
				add_rtpp_node_int_info(crt_node, MI_DISABLED, MI_DISABLED_LEN,
					crt_rtpp->rn_disabled, child, len,string,error);
			}

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
	pv_spec_t *avp_spec;
	unsigned short avp_flags;
	str s;

	if(register_mi_mod(exports.name, mi_cmds)!=0)
	{
		LM_ERR("failed to register MI commands\n");
		return -1;
	}

	/* any rtpproxy configured? */
	if(rtpp_set_list)
		default_rtpp_set = select_rtpp_set(DEFAULT_RTPP_SET_ID);

	if (rtpp_db_url.s == NULL)
	{
		/* storing the list of rtp proxy sets in shared memory*/
		for(i=0;i<rtpp_sets;i++){
			if(rtpengine_add_rtpengine_set(rtpp_strings[i]) !=0){
				for(;i<rtpp_sets;i++)
					if(rtpp_strings[i])
						pkg_free(rtpp_strings[i]);
				pkg_free(rtpp_strings);
				return -1;
			}
			if(rtpp_strings[i])
				pkg_free(rtpp_strings[i]);
		}
	}
	else
	{
		LM_INFO("Loading rtp proxy definitions from DB\n");
		if ( init_rtpproxy_db() < 0)
		{
			LM_ERR("error while loading rtp proxies from database\n");
			return -1;
		}
	}

	if (rtp_inst_pv_param.s) {
		rtp_inst_pv_param.len = strlen(rtp_inst_pv_param.s);
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
		extra_id_pv_param.len = strlen(extra_id_pv_param.s);
		if(pv_parse_format(&extra_id_pv_param, &extra_id_pv) < 0) {
			LM_ERR("malformed PV string: %s\n", extra_id_pv_param.s);
			return -1;
		}
	} else {
		extra_id_pv = NULL;
	}

	if (setid_avp_param) {
	    s.s = setid_avp_param; s.len = strlen(s.s);
	    avp_spec = pv_cache_get(&s);
	    if (avp_spec==NULL || (avp_spec->type != PVT_AVP)) {
		LM_ERR("malformed or non AVP definition <%s>\n",
		       setid_avp_param);
		return -1;
	    }
	    if (pv_get_avp_name(0, &(avp_spec->pvp), &setid_avp,
				&avp_flags) != 0) {
		LM_ERR("invalid AVP definition <%s>\n", setid_avp_param);
		return -1;
	    }
	    setid_avp_type = avp_flags;
	}

	if (write_sdp_pvar_str.len > 0) {
		write_sdp_pvar = pv_cache_get(&write_sdp_pvar_str);
		if (write_sdp_pvar == NULL
		    || (write_sdp_pvar->type != PVT_AVP &&  write_sdp_pvar->type != PVT_SCRIPTVAR) ) {
			LM_ERR("write_sdp_pv: not a valid AVP or VAR definition <%.*s>\n",
		       write_sdp_pvar_str.len, write_sdp_pvar_str.s);
			return -1;
		}
	}

	if (rtpp_strings)
		pkg_free(rtpp_strings);

	if ((queried_nodes_limit < 1) || (queried_nodes_limit > MAX_RTPP_TRIED_NODES)) {
		LM_ERR("queried_nodes_limit must be a number in the range 1..50 \n");
		return -1;
	}

	if (load_tm_api( &tmb ) < 0)
	{
		LM_DBG("could not load the TM-functions - answer-offer model"
				" auto-detection is disabled\n");
		memset(&tmb, 0, sizeof(struct tm_binds));
	}

	/* Determine IP addr type (IPv4 or IPv6 allowed) */
	force_send_ip_af = get_ip_type(force_send_ip_str);
	if (force_send_ip_af != AF_INET && force_send_ip_af != AF_INET6 &&
			strlen(force_send_ip_str) > 0) {
		LM_ERR("%s is an unknown address\n", force_send_ip_str);
		return -1;
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
#ifdef IP_MTU_DISCOVER
	int ip_mtu_discover = IP_PMTUDISC_DONT;
#endif

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
			if (rtpp_socks[pnode->idx] == -1) {
				LM_ERR("can't create socket\n");
				freeaddrinfo(res);
				return -1;
			}

#ifdef IP_MTU_DISCOVER
			setsockopt(rtpp_socks[pnode->idx], IPPROTO_IP,
				   IP_MTU_DISCOVER, &ip_mtu_discover,
				   sizeof(ip_mtu_discover));
#endif

			if (bind_force_send_ip(pnode->idx) == -1) {
				LM_ERR("can't bind socket\n");
				close(rtpp_socks[pnode->idx]);
				rtpp_socks[pnode->idx] = -1;
				freeaddrinfo(res);
				return -1;
			}

			if (connect(rtpp_socks[pnode->idx], res->ai_addr, res->ai_addrlen) == -1) {
				LM_ERR("can't connect to a RTP proxy\n");
				close(rtpp_socks[pnode->idx]);
				rtpp_socks[pnode->idx] = -1;
				freeaddrinfo(res);
				return -1;
			}

			freeaddrinfo(res);
rptest:
			pnode->rn_disabled = rtpp_test(pnode, 0, 1);
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

			if(crt_rtpp->rn_url.s)
				shm_free(crt_rtpp->rn_url.s);

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



static char * gencookie(void)
{
	static char cook[34];

	sprintf(cook, "%d_%u ", (int)mypid, myseqn);
	myseqn++;
	return cook;
}



static const char *transports[] = {
	[0x00]	= "RTP/AVP",
	[0x01]	= "RTP/SAVP",
	[0x02]	= "RTP/AVPF",
	[0x03]	= "RTP/SAVPF",
};

static int parse_flags(struct ng_flags_parse *ng_flags, struct sip_msg *msg, enum rtpe_operation *op,
		const char *flags_str)
{
	char *e;
	const char *err;
	str key, val, s;

	if (!flags_str)
		return 0;

	while (1) {
		while (*flags_str == ' ')
			flags_str++;

		key.s = (void *) flags_str;
		val.len = key.len = -1;
		val.s = NULL;

		e = strpbrk(key.s, " =");
		if (!e)
			e = key.s + strlen(key.s);
		else if (*e == '=') {
			key.len = e - key.s;
			val.s = e + 1;
			e = strchr(val.s, ' ');
			if (!e)
				e = val.s + strlen(val.s);
			val.len = e - val.s;
		}

		if (key.len == -1)
			key.len = e - key.s;
		if (!key.len)
			break;

		/* check for items which have their own sub-list */
		s = str_prefix(&key, "replace-");
		if (s.s) {
			bencode_list_add_str(ng_flags->replace, &s);
			goto next;
		}

		s = str_prefix(&key, "rtcp-mux-");
		if (s.s) {
			bencode_list_add_str(ng_flags->rtcp_mux, &s);
			goto next;
		}

		/* check for specially handled items */
		switch (key.len) {
			case 3:
				if (str_eq(&key, "RTP")) {
					ng_flags->transport |= 0x100;
					ng_flags->transport &= ~0x001;
				}
				else if (str_eq(&key, "AVP")) {
					ng_flags->transport |= 0x100;
					ng_flags->transport &= ~0x002;
				}
				else if (str_eq(&key, "TOS") && val.s)
					bencode_dictionary_add_integer(ng_flags->dict, "TOS", atoi(val.s));
				else
					goto generic;
				goto next;
				break;

			case 4:
				if (str_eq(&key, "SRTP"))
					ng_flags->transport |= 0x101;
				else if (str_eq(&key, "AVPF"))
					ng_flags->transport |= 0x102;
				else
					goto generic;
				goto next;
				break;

			case 6:
				if (str_eq(&key, "to-tag")) {
					ng_flags->to = 1;
					goto next;
				}
				break;

			case 7:
				if (str_eq(&key, "RTP/AVP")) {
					ng_flags->transport = 0x100;
					goto next;
				}
				break;

			case 8:
				if (str_eq(&key, "internal") || str_eq(&key, "external"))
					bencode_list_add_str(ng_flags->direction, &key);
				else if (str_eq(&key, "RTP/AVPF"))
					ng_flags->transport = 0x102;
				else if (str_eq(&key, "RTP/SAVP"))
					ng_flags->transport = 0x101;
				else
					goto generic;
				goto next;
				break;

			case 9:
				if (str_eq(&key, "RTP/SAVPF"))
					ng_flags->transport = 0x103;
				else if (str_eq(&key, "direction"))
					bencode_list_add_str(ng_flags->direction, &val);
				else
					goto generic;
				goto next;
				break;

			case 10:
				if (str_eq(&key, "via-branch")) {
					err = "missing value";
					if (!val.s)
						goto error;
					err = "invalid value";
					if (*val.s == '1' || *val.s == '2')
						ng_flags->via = *val.s - '0';
					else if (str_eq(&val, "auto"))
						ng_flags->via = 3;
					else if (str_eq(&val, "extra"))
						ng_flags->via = -1;
					else
						goto error;
					goto next;
				}
				break;

			case 11:
				if (str_eq(&key, "repacketize")) {
					err = "missing value";
					if (!val.s)
						goto error;
					ng_flags->packetize = 0;
					while (isdigit(*val.s)) {
						ng_flags->packetize *= 10;
						ng_flags->packetize += *val.s - '0';
						val.s++;
					}
					err = "invalid value";
					if (!ng_flags->packetize)
						goto error;
					bencode_dictionary_add_integer(ng_flags->dict, "repacketize", ng_flags->packetize);
					goto next;
				}
				break;

			case 12:
				if (str_eq(&key, "force-answer")) {
					err = "cannot force answer in non-offer command";
					if (*op != OP_OFFER)
						goto error;
					*op = OP_ANSWER;
					goto next;
				}
				break;
		}

generic:
		if (!val.s)
			bencode_list_add_str(ng_flags->flags, &key);
		else
			bencode_dictionary_str_add_str(ng_flags->dict, &key, &val);
		goto next;

next:
		flags_str = e;
	}

	return 0;

error:
	if (val.s)
		LM_ERR("error processing flag `%.*s' (value '%.*s'): %s\n", key.len, key.s,
				val.len, val.s, err);
	else
		LM_ERR("error processing flag `%.*s': %s\n", key.len, key.s, err);
	return -1;
}

static bencode_item_t *rtpp_function_call(bencode_buffer_t *bencbuf, struct sip_msg *msg,
	enum rtpe_operation op, const char *flags_str, str *body_out)
{
	struct ng_flags_parse ng_flags;
	bencode_item_t *item, *resp;
	str callid, from_tag, to_tag, body, viabranch, error;
	int ret, queried_nodes;
	struct rtpp_node *node;
	char *cp;

	/*** get & init basic stuff needed ***/

	memset(&ng_flags, 0, sizeof(ng_flags));

	if (get_callid(msg, &callid) == -1 || callid.len == 0) {
		LM_ERR("can't get Call-Id field\n");
		return NULL;
	}
	if (get_to_tag(msg, &to_tag) == -1) {
		LM_ERR("can't get To tag\n");
		return NULL;
	}
	if (get_from_tag(msg, &from_tag) == -1 || from_tag.len == 0) {
		LM_ERR("can't get From tag\n");
		return NULL;
	}
	if (bencode_buffer_init(bencbuf)) {
		LM_ERR("could not initialize bencode_buffer_t\n");
		return NULL;
	}
	ng_flags.dict = bencode_dictionary(bencbuf);

	body.s = NULL;
	if (op == OP_OFFER || op == OP_ANSWER) {
		ng_flags.flags = bencode_list(bencbuf);
		ng_flags.direction = bencode_list(bencbuf);
		ng_flags.replace = bencode_list(bencbuf);
		ng_flags.rtcp_mux = bencode_list(bencbuf);

		if (extract_body(msg, &body) == -1) {
			LM_ERR("can't extract body from the message\n");
			goto error;
		}
		if (body_intermediate.s)
			bencode_dictionary_add_str(ng_flags.dict, "sdp", &body_intermediate);
		else
			bencode_dictionary_add_str(ng_flags.dict, "sdp", &body);
	}

	/*** parse flags & build dictionary ***/

	ng_flags.to = (op == OP_DELETE) ? 0 : 1;

	if (parse_flags(&ng_flags, msg, &op, flags_str))
		goto error;

	/* only add those if any flags were given at all */
	if (ng_flags.direction && ng_flags.direction->child)
		bencode_dictionary_add(ng_flags.dict, "direction", ng_flags.direction);
	if (ng_flags.flags && ng_flags.flags->child)
		bencode_dictionary_add(ng_flags.dict, "flags", ng_flags.flags);
	if (ng_flags.replace && ng_flags.replace->child)
		bencode_dictionary_add(ng_flags.dict, "replace", ng_flags.replace);
	if ((ng_flags.transport & 0x100))
		bencode_dictionary_add_string(ng_flags.dict, "transport-protocol",
				transports[ng_flags.transport & 0x003]);
	if (ng_flags.rtcp_mux && ng_flags.rtcp_mux->child)
		bencode_dictionary_add(ng_flags.dict, "rtcp-mux", ng_flags.rtcp_mux);

	bencode_dictionary_add_str(ng_flags.dict, "call-id", &callid);

	if (ng_flags.via) {
		if (ng_flags.via == 1 || ng_flags.via == 2)
			ret = get_via_branch(msg, ng_flags.via, &viabranch);
		else if (ng_flags.via == -1 && extra_id_pv)
			ret = get_extra_id(msg, &viabranch);
		else
			ret = -1;
		if (ret == -1 || viabranch.len == 0) {
			LM_ERR("can't get Via branch/extra ID\n");
			goto error;
		}
		bencode_dictionary_add_str(ng_flags.dict, "via-branch", &viabranch);
	}

	item = bencode_list(bencbuf);
	bencode_dictionary_add(ng_flags.dict, "received-from", item);
	bencode_list_add_string(item, (msg->rcv.src_ip.af == AF_INET) ? "IP4" : (
		(msg->rcv.src_ip.af == AF_INET6) ? "IP6" :
		"?"
	) );
	bencode_list_add_string(item, ip_addr2a(&msg->rcv.src_ip));

	if ((msg->first_line.type == SIP_REQUEST && op != OP_ANSWER)
		|| (msg->first_line.type == SIP_REPLY && op == OP_ANSWER))
	{
		bencode_dictionary_add_str(ng_flags.dict, "from-tag", &from_tag);
		if (ng_flags.to && to_tag.s && to_tag.len)
			bencode_dictionary_add_str(ng_flags.dict, "to-tag", &to_tag);
	}
	else {
		if (!to_tag.s || !to_tag.len) {
			LM_ERR("No to-tag present\n");
			goto error;
		}
		bencode_dictionary_add_str(ng_flags.dict, "from-tag", &to_tag);
		bencode_dictionary_add_str(ng_flags.dict, "to-tag", &from_tag);
	}

	bencode_dictionary_add_string(ng_flags.dict, "command", command_strings[op]);

	/*** send it out ***/

	if (bencbuf->error) {
		LM_ERR("out of memory - bencode failed\n");
		goto error;
	}

	if(msg->id != current_msg_id)
		active_rtpp_set = default_rtpp_set;

	queried_nodes = 0;
	do {
		if (++queried_nodes > queried_nodes_limit) {
			LM_ERR("queried nodes limit reached\n");
			goto error;
		}
		node = select_rtpp_node(callid, 1);
		if (!node) {
			LM_ERR("no available proxies\n");
			goto error;
		}
		cp = send_rtpp_command(node, ng_flags.dict, &ret);
	} while (cp == NULL);
	LM_DBG("proxy reply: %.*s\n", ret, cp);

	set_rtp_inst_pvar(msg, &node->rn_url);
	/*** process reply ***/

	resp = bencode_decode_expect(bencbuf, cp, ret, BENCODE_DICTIONARY);
	if (!resp) {
		LM_ERR("failed to decode bencoded reply from proxy: %.*s\n", ret, cp);
		goto error;
	}
	if (!bencode_dictionary_get_strcmp(resp, "result", "error")) {
		if (!bencode_dictionary_get_str(resp, "error-reason", &error))
			LM_ERR("proxy return error but didn't give an error reason: %.*s\n", ret, cp);
		else
			LM_ERR("proxy replied with error: %.*s\n", error.len, error.s);
		goto error;
	}

	if (body_out)
		*body_out = body;

	return resp;

error:
	bencode_buffer_free(bencbuf);
	return NULL;
}

static int rtpp_function_call_simple(struct sip_msg *msg, enum rtpe_operation op, const char *flags_str)
{
	bencode_buffer_t bencbuf;

	if (!rtpp_function_call(&bencbuf, msg, op, flags_str, NULL))
		return -1;

	bencode_buffer_free(&bencbuf);
	return 1;
}

static bencode_item_t *rtpp_function_call_ok(bencode_buffer_t *bencbuf, struct sip_msg *msg,
		enum rtpe_operation op, const char *flags_str, str *body)
{
	bencode_item_t *ret;

	ret = rtpp_function_call(bencbuf, msg, op, flags_str, body);
	if (!ret)
		return NULL;

	if (bencode_dictionary_get_strcmp(ret, "result", "ok")) {
		LM_ERR("proxy didn't return \"ok\" result\n");
		bencode_buffer_free(bencbuf);
		return NULL;
	}

	return ret;
}



static int
rtpp_test(struct rtpp_node *node, int isdisabled, int force)
{
	bencode_buffer_t bencbuf;
	bencode_item_t *dict;
	char *cp;
	int ret;

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

	if (bencode_buffer_init(&bencbuf)) {
		LM_ERR("could not initialized bencode_buffer_t\n");
		return 1;
	}
	dict = bencode_dictionary(&bencbuf);
	bencode_dictionary_add_string(dict, "command", "ping");
	if (bencbuf.error)
		goto benc_error;

	cp = send_rtpp_command(node, dict, &ret);
	if (!cp) {
		LM_ERR("proxy did not respond to ping\n");
		goto error;
	}

	dict = bencode_decode_expect(&bencbuf, cp, ret, BENCODE_DICTIONARY);
	if (!dict || bencode_dictionary_get_strcmp(dict, "result", "pong")) {
		LM_ERR("proxy responded with invalid response\n");
		goto error;
	}

	LM_INFO("rtp proxy <%s> found, support for it %senabled\n",
	    node->rn_url.s, force == 0 ? "re-" : "");

	bencode_buffer_free(&bencbuf);
	return 0;

benc_error:
        LM_ERR("out of memory - bencode failed\n");
error:
	bencode_buffer_free(&bencbuf);
	return 1;
}

static char *
send_rtpp_command(struct rtpp_node *node, bencode_item_t *dict, int *outlen)
{
	struct sockaddr_un addr;
	int fd, len, i, vcnt;
	char *cp;
	static char buf[0x10000];
	struct pollfd fds[1];
	struct iovec *v;

	v = bencode_iovec(dict, &vcnt, 1, 0);
	if (!v) {
		LM_ERR("error converting bencode to iovec\n");
		return NULL;
	}

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
			len = writev(fd, v + 1, vcnt);
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
		for (i = 0; i < rtpengine_retr; i++) {
			do {
				len = writev(rtpp_socks[node->idx], v, vcnt + 1);
			} while (len == -1 && (errno == EINTR || errno == ENOBUFS));
			if (len <= 0) {
				LM_ERR("can't send command to a RTP proxy\n");
				goto badproxy;
			}
			while ((poll(fds, 1, rtpengine_tout_ms) == 1) &&
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
		if (i == rtpengine_retr) {
			LM_ERR("timeout waiting reply from a RTP proxy\n");
			goto badproxy;
		}
	}

out:
	cp[len] = '\0';
	*outlen = len;
	return cp;
badproxy:
	LM_ERR("proxy <%s> does not respond, disable it\n", node->rn_url.s);
	node->rn_disabled = 1;
	node->rn_recheck_ticks = get_ticks() + rtpengine_disable_tout;

	return NULL;
}

/*
 * select the set with the id_set id
 */

static struct rtpp_set * select_rtpp_set(int id_set ){

	struct rtpp_set * rtpp_list;
	/*is it a valid set_id?*/

	if(!rtpp_set_list || !rtpp_set_list->rset_first){
		LM_ERR("no rtp_proxy configured\n");
		return 0;
	}

	for(rtpp_list=rtpp_set_list->rset_first; rtpp_list!=0 &&
		rtpp_list->id_set!=id_set; rtpp_list=rtpp_list->rset_next);
	if(!rtpp_list){
		LM_ERR(" script error-invalid id_set to be selected\n");
	}

	return rtpp_list;
}
/*
 * Main balancing routine. This does not try to keep the same proxy for
 * the call if some proxies were disabled or enabled; proxy death considered
 * too rare. Otherwise we should implement "mature" HA clustering, which is
 * too expensive here.
 */
static struct rtpp_node *
select_rtpp_node(str callid, int do_test)
{
	unsigned sum, sumcut, weight_sum;
	struct rtpp_node* node;
	int was_forced;

	if(!active_rtpp_set){
		LM_ERR("script error -no valid set selected\n");
		return NULL;
	}
	/* Most popular case: 1 proxy, nothing to calculate */
	if (active_rtpp_set->rtpp_node_count == 1) {
		node = active_rtpp_set->rn_first;
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
	for (node=active_rtpp_set->rn_first; node!=NULL; node=node->rn_next) {

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
		for(node=active_rtpp_set->rn_first; node!=NULL; node=node->rn_next) {
			node->rn_disabled = rtpp_test(node, 1, 1);
		}
		goto retry;
	}
	sumcut = sum % weight_sum;
	/*
	 * sumcut here lays from 0 to weight_sum-1.
	 * Scan proxy list and decrease until appropriate proxy is found.
	 */
	for (node=active_rtpp_set->rn_first; node!=NULL; node=node->rn_next) {
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
		return -1;
	}
	if (pv_printf_s(msg, extra_id_pv, id_str)<0) {
		LM_ERR("cannot print the additional id\n");
		return -1;
	}

	return 1;

}

static int
set_rtpengine_set_from_avp(struct sip_msg *msg, int direction)
{
    struct usr_avp *avp;
    int_str setid_val;

    if ((setid_avp_param == NULL) ||
	(avp = search_first_avp(setid_avp_type, setid_avp, &setid_val, 0))
	== NULL)
    {
	    if (direction == 1 || !selected_rtpp_set_2)
		    active_rtpp_set = selected_rtpp_set_1;
	    else
		    active_rtpp_set = selected_rtpp_set_2;
	    return 1;
    }

    if (avp->flags&AVP_VAL_STR) {
	LM_ERR("setid_avp must hold an integer value\n");
	return -1;
    }

    active_rtpp_set = select_rtpp_set(setid_val.n);
    if(active_rtpp_set == NULL) {
	LM_ERR("could not locate rtpproxy set %d\n", setid_val.n);
	return -1;
    }

    LM_DBG("using rtpengine set %d\n", setid_val.n);

    current_msg_id = msg->id;

    return 1;
}

static int rtpengine_delete(struct sip_msg *msg, const char *flags) {
	return rtpp_function_call_simple(msg, OP_DELETE, flags);
}

static int rtpengine_rtpp_set_wrap(struct sip_msg *msg, int (*func)(struct sip_msg *msg, void *, int),
		void *data, int direction)
{
	int ret, more;

	body_intermediate.s = NULL;

	if (set_rtpengine_set_from_avp(msg, direction) == -1)
	    return -1;

	more = 1;
	if (!selected_rtpp_set_2 || selected_rtpp_set_2 == selected_rtpp_set_1)
		more = 0;

	ret = func(msg, data, more);
	if (ret < 0)
		return ret;

	if (!more)
		return ret;

	direction = (direction == 1) ? 2 : 1;
	if (set_rtpengine_set_from_avp(msg, direction) == -1)
	    return -1;

	ret = func(msg, data, 0);
	body_intermediate.s = NULL;
	return ret;
}

static int rtpengine_delete_wrap(struct sip_msg *msg, void *d, int more) {
	return rtpengine_delete(msg, d);
}

static int
rtpengine_delete1_f(struct sip_msg* msg, char* str1, char* str2)
{
	str flags;

	flags.s = NULL;
	if (str1)
		get_str_fparam(&flags, msg, (fparam_t *) str1);

	return rtpengine_rtpp_set_wrap(msg, rtpengine_delete_wrap, flags.s, 1);
}

/* This function assumes p points to a line of requested type. */

static int
set_rtpengine_set_n(struct sip_msg *msg, rtpp_set_link_t *rtpl, struct rtpp_set **out)
{
	pv_value_t val;
	struct rtpp_node *node;
	int nb_active_nodes = 0;

	if(rtpl->rset != NULL) {
		current_msg_id = msg->id;
		*out = rtpl->rset;
		return 1;
	}

	if(pv_get_spec_value(msg, rtpl->rpv, &val)<0) {
		LM_ERR("cannot evaluate pv param\n");
		return -1;
	}
	if(!(val.flags & PV_VAL_INT)) {
		LM_ERR("pv param must hold an integer value\n");
		return -1;
	}
	*out = select_rtpp_set(val.ri);
	if(*out==NULL) {
		LM_ERR("could not locate rtpengine set %d\n", val.ri);
		return -1;
	}
	current_msg_id = msg->id;

	node = (*out)->rn_first;
	while (node != NULL)
	{
	    if (node->rn_disabled == 0) nb_active_nodes++;
	    node = node->rn_next;
	}

	if ( nb_active_nodes > 0 )
	{
		LM_DBG("rtpp: selected proxy set ID %d with %d active nodes.\n",
		       current_msg_id, nb_active_nodes);
		return nb_active_nodes;
	}
	else
	{
		LM_WARN("rtpp: selected proxy set ID %d but it has no active node.\n",
			 current_msg_id);
		return -2;
	}
}

static int
set_rtpengine_set_f(struct sip_msg * msg, char * str1, char * str2)
{
	rtpp_set_link_t *rtpl1, *rtpl2;
	int ret;

	rtpl1 = (rtpp_set_link_t*)str1;
	rtpl2 = (rtpp_set_link_t*)str2;

	current_msg_id = 0;
	active_rtpp_set = 0;
	selected_rtpp_set_1 = 0;
	selected_rtpp_set_2 = 0;

	ret = set_rtpengine_set_n(msg, rtpl1, &selected_rtpp_set_1);
	if (ret < 0)
		return ret;

	if (rtpl2) {
		ret = set_rtpengine_set_n(msg, rtpl2, &selected_rtpp_set_2);
		if (ret < 0)
			return ret;
	}

	return 1;
}

static int
rtpengine_manage(struct sip_msg *msg, const char *flags)
{
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
				|| method==METHOD_BYE || method==METHOD_UPDATE))
		return -1;

	if(method==METHOD_CANCEL || method==METHOD_BYE)
		return rtpengine_delete(msg, flags);

	if(msg->msg_flags & FL_SDP_BODY)
		nosdp = 0;
	else
		nosdp = parse_sdp(msg);

	if(msg->first_line.type == SIP_REQUEST) {
		if(method==METHOD_ACK && nosdp==0)
			return rtpengine_offer_answer(msg, flags, OP_ANSWER, 0);
		if(method==METHOD_UPDATE && nosdp==0)
			return rtpengine_offer_answer(msg, flags, OP_OFFER, 0);
		if(method==METHOD_INVITE && nosdp==0) {
			msg->msg_flags |= FL_SDP_BODY;
			if(tmb.t_gett!=NULL && tmb.t_gett()!=NULL
					&& tmb.t_gett()!=T_UNDEFINED)
				tmb.t_gett()->uas.request->msg_flags |= FL_SDP_BODY;
			if(route_type==FAILURE_ROUTE)
				return rtpengine_delete(msg, flags);
			return rtpengine_offer_answer(msg, flags, OP_OFFER, 0);
		}
	} else if(msg->first_line.type == SIP_REPLY) {
		if(msg->first_line.u.reply.statuscode>=300)
			return rtpengine_delete(msg, flags);
		if(nosdp==0) {
			if(method==METHOD_UPDATE)
				return rtpengine_offer_answer(msg, flags, OP_ANSWER, 0);
			if(tmb.t_gett==NULL || tmb.t_gett()==NULL
					|| tmb.t_gett()==T_UNDEFINED)
				return rtpengine_offer_answer(msg, flags, OP_ANSWER, 0);
			if(tmb.t_gett()->uas.request->msg_flags & FL_SDP_BODY)
				return rtpengine_offer_answer(msg, flags, OP_ANSWER, 0);
			return rtpengine_offer_answer(msg, flags, OP_OFFER, 0);
		}
	}
	return -1;
}

static int rtpengine_manage_wrap(struct sip_msg *msg, void *d, int more) {
	return rtpengine_manage(msg, d);
}

static int
rtpengine_manage1_f(struct sip_msg *msg, char *str1, char *str2)
{
	str flags;

	flags.s = NULL;
	if (str1)
		get_str_fparam(&flags, msg, (fparam_t *) str1);

	return rtpengine_rtpp_set_wrap(msg, rtpengine_manage_wrap, flags.s, 1);
}

static int rtpengine_offer_wrap(struct sip_msg *msg, void *d, int more) {
	return rtpengine_offer_answer(msg, d, OP_OFFER, more);
}

static int
rtpengine_offer1_f(struct sip_msg *msg, char *str1, char *str2)
{
	str flags;

	flags.s = NULL;
	if (str1)
		get_str_fparam(&flags, msg, (fparam_t *) str1);

	return rtpengine_rtpp_set_wrap(msg, rtpengine_offer_wrap, flags.s, 1);
}

static int rtpengine_answer_wrap(struct sip_msg *msg, void *d, int more) {
	return rtpengine_offer_answer(msg, d, OP_ANSWER, more);
}

static int
rtpengine_answer1_f(struct sip_msg *msg, char *str1, char *str2)
{
	str flags;

	if (msg->first_line.type == SIP_REQUEST)
		if (msg->first_line.u.request.method_value != METHOD_ACK)
			return -1;

	flags.s = NULL;
	if (str1)
		get_str_fparam(&flags, msg, (fparam_t *) str1);

	return rtpengine_rtpp_set_wrap(msg, rtpengine_answer_wrap, flags.s, 2);
}

static int
rtpengine_offer_answer(struct sip_msg *msg, const char *flags, int op, int more)
{
	bencode_buffer_t bencbuf;
	bencode_item_t *dict;
	str body, newbody;
	struct lump *anchor;
	pv_value_t pv_val;

	dict = rtpp_function_call_ok(&bencbuf, msg, op, flags, &body);
	if (!dict)
		return -1;

	if (!bencode_dictionary_get_str_dup(dict, "sdp", &newbody)) {
		LM_ERR("failed to extract sdp body from proxy reply\n");
		goto error;
	}

	if (body_intermediate.s)
		pkg_free(body_intermediate.s);

	if (more)
		body_intermediate = newbody;
	else {
		if (write_sdp_pvar!= NULL) {
			pv_val.rs = newbody;
			pv_val.flags = PV_VAL_STR;

			if (write_sdp_pvar->setf(msg,&write_sdp_pvar->pvp, (int)EQ_T, &pv_val) < 0)
			{
				LM_ERR("error setting pvar <%.*s>\n", write_sdp_pvar_str.len, write_sdp_pvar_str.s);
				goto error_free;
			}

			pkg_free(newbody.s);

		} else {
			anchor = del_lump(msg, body.s - msg->buf, body.len, 0);
			if (!anchor) {
				LM_ERR("del_lump failed\n");
				goto error_free;
			}
			if (!insert_new_lump_after(anchor, newbody.s, newbody.len, 0)) {
				LM_ERR("insert_new_lump_after failed\n");
				goto error_free;
			}
		}
	}

	bencode_buffer_free(&bencbuf);
	return 1;

error_free:
	pkg_free(newbody.s);
error:
	bencode_buffer_free(&bencbuf);
	return -1;
}


static int rtpengine_start_recording_wrap(struct sip_msg *msg, void *d, int more) {
	return rtpp_function_call_simple(msg, OP_START_RECORDING, NULL);
}

static int
start_recording_f(struct sip_msg* msg, char *foo, char *bar)
{
	return rtpengine_rtpp_set_wrap(msg, rtpengine_start_recording_wrap, NULL, 1);
}

static int rtpengine_rtpstat_wrap(struct sip_msg *msg, void *d, int more) {
	void **parms;
	pv_param_t *param;
	pv_value_t *res;
	bencode_buffer_t bencbuf;
	bencode_item_t *dict, *tot, *rtp, *rtcp;
	static char buf[256];
	str ret;

	parms = d;
	param = parms[0];
	res = parms[1];

	dict = rtpp_function_call_ok(&bencbuf, msg, OP_QUERY, NULL, NULL);
	if (!dict)
		return -1;

	tot = bencode_dictionary_get_expect(dict, "totals", BENCODE_DICTIONARY);
	rtp = bencode_dictionary_get_expect(tot, "RTP", BENCODE_DICTIONARY);
	rtcp = bencode_dictionary_get_expect(tot, "RTCP", BENCODE_DICTIONARY);

	if (!rtp || !rtcp)
		goto error;

	ret.s = buf;
	ret.len = snprintf(buf, sizeof(buf),
			"RTP: %lli bytes, %lli packets, %lli errors; "
			"RTCP: %lli bytes, %lli packets, %lli errors",
			bencode_dictionary_get_integer(rtp, "bytes", -1),
			bencode_dictionary_get_integer(rtp, "packets", -1),
			bencode_dictionary_get_integer(rtp, "errors", -1),
			bencode_dictionary_get_integer(rtcp, "bytes", -1),
			bencode_dictionary_get_integer(rtcp, "packets", -1),
			bencode_dictionary_get_integer(rtcp, "errors", -1));

	bencode_buffer_free(&bencbuf);
	return pv_get_strval(msg, param, res, &ret);

error:
	bencode_buffer_free(&bencbuf);
	return -1;
}

/*
 * Returns the current RTP-Statistics from the RTP-Proxy
 */
static int
pv_get_rtpstat_f(struct sip_msg *msg, pv_param_t *param,
		  pv_value_t *res)
{
	void *parms[2];

	parms[0] = param;
	parms[1] = res;

	return rtpengine_rtpp_set_wrap(msg, rtpengine_rtpstat_wrap, parms, 1);
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
		LM_ERR("Failed to add RTP Engine URI to pvar\n");
		return -1;
	}
	return 0;
}
