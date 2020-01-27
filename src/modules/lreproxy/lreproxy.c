/*
 * Copyright (C) 2019-2020 Mojtaba Esfandiari.S, Nasim-Telecom
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

#include "../../core/flags.h"
#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/data_lump.h"
#include "../../core/data_lump_rpl.h"
#include "../../core/error.h"
#include "../../core/forward.h"
#include "../../core/mem/mem.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_to.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/parser_f.h"
#include "../../core/parser/sdp/sdp.h"
#include "../../core/resolve.h"
#include "../../core/timer.h"
#include "../../core/trim.h"
#include "../../core/ut.h"
#include "../../core/pt.h"
#include "../../core/timer_proc.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include "../../core/pvar.h"
#include "../../core/lvalue.h"
#include "../../core/msg_translator.h"
#include "../../core/usr_avp.h"
#include "../../core/socket_info.h"
#include "../../core/mod_fix.h"
#include "../../core/dset.h"
#include "../../core/route.h"
#include "../../core/kemi.h"
#include "../../modules/tm/tm_load.h"
#include "lreproxy.h"
#include "lreproxy_hash.h"
#include "lreproxy_funcs.h"
//#include "rtpproxy_stream.h"

MODULE_VERSION


#if !defined(AF_LOCAL)
#define	AF_LOCAL AF_UNIX
#endif
#if !defined(PF_LOCAL)
#define	PF_LOCAL PF_UNIX
#endif

///* NAT UAC test constants */
//#define	NAT_UAC_TEST_C_1918	0x01
//#define	NAT_UAC_TEST_RCVD	0x02
//#define	NAT_UAC_TEST_V_1918	0x04
//#define	NAT_UAC_TEST_S_1918	0x08
//#define	NAT_UAC_TEST_RPORT	0x10

#define DEFAULT_LREP_SET_ID		0
static str DEFAULT_LREP_SET_ID_STR = str_init("0");

//#define RPC_DEFAULT_NATPING_STATE	1

#define RPC_MIN_RECHECK_TICKS		0
#define RPC_MAX_RECHECK_TICKS		(unsigned int)-1


/* Supported version of the RTP proxy command protocol */
#define	SUP_CPROTOVER	"20190708"
/* Required additional version of the RTP proxy command protocol */
#define	REQ_CPROTOVER	"20190709"
/* Additional version necessary for re-packetization support */
#define	REP_CPROTOVER	"20190708"
#define	PTL_CPROTOVER	"20190708"

#define	CPORT		"22333"
#define HASH_SIZE   128
//static int extract_mediaip(str *, str *, int *, char *);
//static int alter_mediaip(struct sip_msg *, str *, str *, int, str *, int, int);
//static int alter_mediaport(struct sip_msg *, str *, str *, str *, int);
//static int alter_rtcp(struct sip_msg *msg, str *body, str *oldport, str *newport);
static char *gencookie();
static int lrep_test(struct lrep_node*);
static int lrep_get_config(struct lrep_node *node);
static int lrep_set_conntrack_rule(struct lreproxy_hash_entry *e);

//static int unforce_rtp_proxy1_f(struct sip_msg *, char *, char *);
//static int unforce_rtp_proxy(struct sip_msg *, char *);
//static int force_rtp_proxy(struct sip_msg *, char *, char *, int, int);
//static int start_recording_f(struct sip_msg *, char *, char *);
//static int lreproxy_answer1_f(struct sip_msg *, char *, char *);
//static int lreproxy_answer2_f(struct sip_msg *, char *, char *);
//static int lreproxy_offer1_f(struct sip_msg *, char *, char *);
//static int lreproxy_offer2_f(struct sip_msg *, char *, char *);
static int lreproxy_force(struct sip_msg *msg, const char *flags, enum lre_operation op, int more);
static int lreproxy_unforce(struct sip_msg *msg, const char *flags, enum lre_operation op, int more);

static int lreproxy_manage0(struct sip_msg *msg, char *flags, char *ip);
static int lreproxy_manage1(struct sip_msg *msg, char *flags, char *ip);
static int lreproxy_manage2(struct sip_msg *msg, char *flags, char *ip);
//static int w_lre_manage(sip_msg_t *msg, char *flags);
//static int lre_get_sdp_info(struct sip_msg *msg, lre_sdp_info_t *lre_sdp_info);
//static int replace_body_total(sip_msg_t *msg, int port, char *flags, int type);
//static int replace_body_total(sip_msg_t *msg, struct lrep_node *n, const char *flags, int type);
//static int change_media_sdp(sip_msg_t *msg, struct lrep_node *n, const char *flags, int type);
static int change_media_sdp(sip_msg_t *msg, struct lreproxy_hash_entry *e, const char *flags, enum lre_operation op);





static int add_lreproxy_socks(struct lrep_set * lrep_list, char * rtpproxy);
static int fixup_set_id(void ** param, int param_no);
static int set_lre_proxy_set_f(struct sip_msg * msg, char * str1, char * str2);

static struct lrep_set * select_lrep_set(int id_set);

static int rtpproxy_set_store(modparam_t type, void * val);
static int lreproxy_add_lreproxy_set( char * lre_proxies);

static int mod_init(void);
static int child_init(int);
static void mod_destroy(void);

/* Pseudo-Variables */
//static int pv_get_rtpstat_f(struct sip_msg *, pv_param_t *, pv_value_t *);

static int lreproxy_disable_tout = 60;
static int lreproxy_retr = 5;
static int lreproxy_tout = 1;
static pid_t mypid;
static unsigned int myseqn = 0;
//static str nortpproxy_str = str_init("a=nortpproxy:yes");
//static str extra_id_pv_param = {NULL, 0};

static char ** rtpp_strings=0;
static int lrep_sets=0; /*used in rtpproxy_set_store()*/
static int lrep_set_count = 0;
static unsigned int current_msg_id = (unsigned int)-1;
/* RTP proxy balancing list */
struct lrep_set_head * lrep_set_list =0;
struct lrep_set * selected_lrep_set =0;
struct lrep_set * default_lrep_set=0;
struct lrep_node *selected_lrep_node = 0;
int lrep_algorithm = LRE_LINER;
static int hash_table_size = 0;
static int hash_table_tout = 3600;



//static char *ice_candidate_priority_avp_param = NULL;
//static int ice_candidate_priority_avp_type;
//static int_str ice_candidate_priority_avp;
//static str rtp_inst_pv_param = {NULL, 0};
//static pv_spec_t *rtp_inst_pvar = NULL;

/* array with the sockets used by rtpproxy (per process)*/
static unsigned int rtpp_no = 0;
static int *rtpp_socks = 0;


typedef struct lrep_set_link {
	struct lrep_set *rset;
	pv_spec_t *rpv;
} lrep_set_link_t;

/* tm */
static struct tm_binds tmb;

/*0-> disabled, 1 ->enabled*/
//unsigned int *natping_state=0;

static str timeout_socket_str = {0, 0};
//static pv_elem_t *extra_id_pv = NULL;
int start_port = 10000;
int end_port = 20000;
str internal_ip;
str external_ip;

static cmd_export_t cmds[] = {
//        {"lreproxy_offer",	(cmd_function)lreproxy_offer1_f,     0,
//                0, 0,
//                ANY_ROUTE},
//        {"lreproxy_offer",	(cmd_function)lreproxy_offer1_f,     1,
//                fixup_spve_null, 0,
//                ANY_ROUTE},
//        {"lreproxy_offer",	(cmd_function)lreproxy_offer2_f,     2,
//                fixup_spve_spve, 0,
//                ANY_ROUTE},
//        {"lreproxy_answer",	(cmd_function)lreproxy_answer1_f,    0,
//                0, 0,
//                ANY_ROUTE},
//        {"lreproxy_answer",	(cmd_function)lreproxy_answer1_f,    1,
//                fixup_spve_null, 0,
//                ANY_ROUTE},
//        {"lreproxy_answer",	(cmd_function)lreproxy_answer2_f,    2,
//                fixup_spve_spve, 0,
//                ANY_ROUTE},
        {"set_lre_proxy_set",  (cmd_function)set_lre_proxy_set_f,    1,
                fixup_set_id, 0,
                ANY_ROUTE},
        {"lreproxy_manage",	(cmd_function)lreproxy_manage0,     0,
                                                       0, 0,
                   ANY_ROUTE},
        {"lreproxy_manage",	(cmd_function)lreproxy_manage1,     1,
                                                       fixup_spve_null, fixup_free_spve_null,
                   ANY_ROUTE},
        {"lreproxy_manage",	(cmd_function)lreproxy_manage2,     2,
                                                       fixup_spve_spve, fixup_free_spve_spve,
                   ANY_ROUTE},
//        {"lre_manage", (cmd_function) w_lre_manage, 0, 0,
//                0, ANY_ROUTE},
//        {"lre_manage", (cmd_function) w_lre_manage, 1, 0, 0,
//                   ANY_ROUTE},
        {0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
        {"lreproxy_sock",         PARAM_STRING|USE_FUNC_PARAM,
                (void*)rtpproxy_set_store          },
        {"lreproxy_disable_tout", INT_PARAM, &lreproxy_disable_tout },
        {"lreproxy_retr",         INT_PARAM, &lreproxy_retr         },
        {"lreproxy_tout",         INT_PARAM, &lreproxy_tout         },
        {"timeout_socket",    	  PARAM_STR, &timeout_socket_str  },
        {"lrep_alg",         INT_PARAM, &lrep_algorithm         },
        {"hash_table_tout",       INT_PARAM, &hash_table_tout        },
        {"hash_table_size",       INT_PARAM, &hash_table_size        },
//    {"ht_name", STR_PARAM, &ht_name.s},
//        {"start_port",             INT_PARAM, &start_port },
//        {"end_port",             INT_PARAM, &end_port },
//        {"internal_ip",         STR_PARAM, &internal_ip},
//        {"external_ip",         STR_PARAM, &external_ip },

        {0, 0, 0}
};

struct module_exports exports = {
        "lreproxy",
        DEFAULT_DLFLAGS, /* dlopen flags */
        cmds,
        params,
        0,           /* exported statistics */
        0,           /* exported MI functions */
        0,     /* exported pseudo-variables */
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

    if(lrep_sets==0){
        rtpp_strings = (char**)pkg_malloc(sizeof(char*));
        if(!rtpp_strings){
                    LM_ERR("no pkg memory left\n");
            return -1;
        }
    } else {/*realloc to make room for the current set*/
        rtpp_strings = (char**)pkg_reallocxf(rtpp_strings,
                                             (lrep_sets+1)* sizeof(char*));
        if(!rtpp_strings){
                    LM_ERR("no pkg memory left\n");
            return -1;
        }
    }

    /*allocate for the current set of urls*/
    len = strlen(p);
    rtpp_strings[lrep_sets] = (char*)pkg_malloc((len+1)*sizeof(char));

    if(!rtpp_strings[lrep_sets]){
                LM_ERR("no pkg memory left\n");
        return -1;
    }

    memcpy(rtpp_strings[lrep_sets], p, len);
    rtpp_strings[lrep_sets][len] = '\0';
    lrep_sets++;

    return 0;
}

struct lrep_set *get_lrep_set(str *const set_name)
{
    unsigned int this_set_id;
    struct lrep_set *lrep_list;
    if (lrep_set_list == NULL)
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

    lrep_list = select_lrep_set(this_set_id);

    if(lrep_list==NULL){	/*if a new id_set : add a new set of rtpp*/
        lrep_list = shm_malloc(sizeof(struct lrep_set));
        if(!lrep_list){
                    LM_ERR("no shm memory left\n");
            return NULL;
        }
        memset(lrep_list, 0, sizeof(struct lrep_set));
        lrep_list->id_set = this_set_id;
        if (lrep_set_list->lset_first == NULL)
        {
            lrep_set_list->lset_first = lrep_list;
        } else {
            lrep_set_list->lset_last->lset_next = lrep_list;
        }
        lrep_set_list->lset_last = lrep_list;
        lrep_set_count++;

        if (this_set_id == DEFAULT_LREP_SET_ID)
        {
            default_lrep_set = lrep_list;
        }
    }
    return lrep_list;
}

int insert_lrep_node(struct lrep_set *const lrep_list, const str *const url, const int weight, const int enable)
{
    struct lrep_node *pnode;

    if ((pnode = shm_malloc(sizeof(struct lrep_node) + url->len + 1)) == NULL)
    {
                LM_ERR("out of shm memory\n");
        return -1;
    }

    memset(pnode, 0, sizeof(struct lrep_node) + url->len + 1);


    struct lrep_node_conf *node_conf;
    node_conf = shm_malloc(sizeof(struct lrep_node_conf));
    if (!node_conf)
    {
                LM_ERR("out of shm memory\n");
        return -1;
    }

    memset(node_conf, 0, sizeof(struct lrep_node_conf));
    pnode->lrep_n_c = node_conf;

    pnode->idx = rtpp_no++;
    pnode->ln_weight = weight;
    pnode->ln_umode = 0;
    pnode->ln_enable = enable;
    /* Permanently disable if marked as disabled */
//    pnode->ln_recheck_ticks = disabled ? RPC_MAX_RECHECK_TICKS : 0;
    pnode->ln_url.s = (char*)(pnode + 1);
    memcpy(pnode->ln_url.s, url->s, url->len);
    pnode->ln_url.len = url->len;

            LM_DBG("url is '%.*s'\n", pnode->ln_url.len, pnode->ln_url.s);

    /* Find protocol and store address */
    pnode->ln_address = pnode->ln_url.s;
    if (strncasecmp(pnode->ln_address, "udp:", 4) == 0) {
        pnode->ln_umode = 1;
        pnode->ln_address += 4;
    } else if (strncasecmp(pnode->ln_address, "udp6:", 5) == 0) {
        pnode->ln_umode = 6;
        pnode->ln_address += 5;
    } else if (strncasecmp(pnode->ln_address, "unix:", 5) == 0) {
        pnode->ln_umode = 0;
        pnode->ln_address += 5;
    }

    if (lrep_list->ln_first == NULL)
    {
        lrep_list->ln_first = pnode;
    } else {
        lrep_list->ln_last->ln_next = pnode;
    }
    lrep_list->ln_last = pnode;
    lrep_list->lrep_node_count++;

    return 0;
}

static int add_lreproxy_socks(struct lrep_set * lrep_list,
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
        insert_lrep_node(lrep_list, &url, weight, 0);
    }
    return 0;
}

/*	0-succes
 *  -1 - erorr
 * */
static int lreproxy_add_lreproxy_set( char * lre_proxies)
{
    char *p,*p2;
    struct lrep_set * lrep_list;
    str id_set;

    /* empty definition? */
    p= lre_proxies;
    if(!p || *p=='\0'){
        return 0;
    }

    for(;*p && isspace(*p);p++);
    if(*p=='\0'){
        return 0;
    }

    lre_proxies = strstr(p, "==");
    if(lre_proxies){
        if(*(lre_proxies +2)=='\0'){
                    LM_ERR("script error -invalid rtp proxy list!\n");
            return -1;
        }

        *lre_proxies = '\0';
        p2 = lre_proxies-1;
        for(;isspace(*p2); *p2 = '\0',p2--);
        id_set.s = p;	id_set.len = p2 - p+1;

        if(id_set.len <= 0){
                    LM_ERR("script error -invalid set_id value!\n");
            return -1;
        }

        lre_proxies+=2;
    }else{
        lre_proxies = p;
        id_set = DEFAULT_LREP_SET_ID_STR;
    }

    for(;*lre_proxies && isspace(*lre_proxies);lre_proxies++);

    if(!(*lre_proxies)){
                LM_ERR("script error -empty rtp_proxy list\n");
        return -1;;
    }

    lrep_list = get_lrep_set(&id_set);
    if (lrep_list == NULL)
    {
                LM_ERR("Failed to get or create lrep_list for '%.*s'\n", id_set.len, id_set.s);
        return -1;
    }

    if(add_lreproxy_socks(lrep_list, lre_proxies)!= 0){
        return -1;
    }

    return 0;
}


static int fixup_set_id(void ** param, int param_no)
{
	int int_val, err;
	struct lrep_set* lrep_list;
	lrep_set_link_t *rtpl = NULL;
	str s;

	rtpl = (lrep_set_link_t*)pkg_malloc(sizeof(lrep_set_link_t));
	if(rtpl==NULL) {
		LM_ERR("no more pkg memory\n");
		return -1;
	}
	memset(rtpl, 0, sizeof(lrep_set_link_t));
	s.s = (char*)*param;
	s.len = strlen(s.s);

	if(s.s[0] == PV_MARKER) {
		int_val = pv_locate_name(&s);
		if(int_val<0 || int_val!=s.len) {
			LM_ERR("invalid parameter %s\n", s.s);
			pkg_free(rtpl);
			return -1;
		}
		rtpl->rpv = pv_cache_get(&s);
		if(rtpl->rpv == NULL) {
			LM_ERR("invalid pv parameter %s\n", s.s);
			pkg_free(rtpl);
			return -1;
		}
	} else {
		int_val = str2s(*param, strlen(*param), &err);
		if (err == 0) {
			pkg_free(*param);
			if((lrep_list = select_lrep_set(int_val)) ==0){
				LM_ERR("lrep_proxy set %i not configured\n", int_val);
				pkg_free(rtpl);
				return E_CFG;
			}
			rtpl->rset = lrep_list;
		} else {
			LM_ERR("bad number <%s>\n",	(char *)(*param));
			pkg_free(rtpl);
			return E_CFG;
		}
	}
	*param = (void*)rtpl;
	return 0;
}

//static void  rtpproxy_rpc_enable(rpc_t* rpc, void* ctx)
//{
//	str rtpp_url;
//	int enable;
//	struct lrep_set *lrep_list;
//	struct lrep_node *crt_rtpp;
//	int found;
//
//	found = 0;
//	enable = 0;
//
//	if(lrep_set_list ==NULL)
//		goto end;
//
//	if (rpc->scan(ctx, "Sd", &rtpp_url, &enable) < 2) {
//		rpc->fault(ctx, 500, "Not enough parameters");
//		return;
//	}
//
//	for(lrep_list = lrep_set_list->lset_first; lrep_list != NULL;
//			lrep_list = lrep_list->lset_next) {
//
//		for(crt_rtpp = lrep_list->ln_first; crt_rtpp != NULL;
//				crt_rtpp = crt_rtpp->ln_next) {
//			/*found a matching rtpp*/
//			if(crt_rtpp->ln_url.len == rtpp_url.len) {
//
//				if(strncmp(crt_rtpp->ln_url.s, rtpp_url.s, rtpp_url.len) == 0) {
//					/*set the enabled/disabled status*/
//					found = 1;
//					crt_rtpp->ln_recheck_ticks =
//						enable? RPC_MIN_RECHECK_TICKS : RPC_MAX_RECHECK_TICKS;
//					crt_rtpp->ln_disabled = enable?0:1;
//				}
//			}
//		}
//	}
//
//end:
//	if(!found) {
//		rpc->fault(ctx, 404, "RTPProxy not found");
//		return;
//	}
//}


//static void  rtpproxy_rpc_list(rpc_t* rpc, void* ctx)
//{
//	struct lrep_set *lrep_list;
//	struct lrep_node *crt_rtpp;
//	void *vh;
//
//	if(lrep_set_list ==NULL)
//		return;
//
//	for(lrep_list = lrep_set_list->lset_first; lrep_list != NULL;
//			lrep_list = lrep_list->lset_next) {
//
//		for(crt_rtpp = lrep_list->ln_first; crt_rtpp != NULL;
//				crt_rtpp = crt_rtpp->ln_next) {
//
//			if (rpc->add(ctx, "{", &vh) < 0) {
//				rpc->fault(ctx, 500, "Server error");
//				return;
//			}
//			rpc->struct_add(vh, "dSdddd",
//				"setid", lrep_list->id_set,
//				"url", &crt_rtpp->ln_url,
//				"index", crt_rtpp->idx,
//				"disabled", crt_rtpp->ln_disabled,
//				"weight", crt_rtpp->ln_weight,
//				"recheck", crt_rtpp->ln_recheck_ticks);
//		}
//	}
//}
//
//static const char* rtpproxy_rpc_enable_doc[2] = {
//	"Set state (enable/disable) for a rtp proxy.",
//	0
//};
//
//static const char* rtpproxy_rpc_list_doc[2] = {
//	"List rtp proxies.",
//	0
//};
//
//rpc_export_t rtpproxy_rpc[] = {
//	{"rtpproxy.list", rtpproxy_rpc_list, rtpproxy_rpc_list_doc, RET_ARRAY},
//	{"rtpproxy.enable", rtpproxy_rpc_enable, rtpproxy_rpc_enable_doc, 0},
//	{0, 0, 0, 0}
//};

//static int rtpproxy_rpc_init(void)
//{
//	if (rpc_register_array(rtpproxy_rpc)!=0)
//	{
//		LM_ERR("failed to register RPC commands\n");
//		return -1;
//	}
//	return 0;
//}

static int
mod_init(void)
{
    int i;
//	pv_spec_t avp_spec;
//	str s;
//	unsigned short avp_flags;

//	if(rtpproxy_rpc_init()<0)
//	{
//		LM_ERR("failed to register RPC commands\n");
//		return -1;
//	}

    /* Configure the head of the lrep_set_list */
    lrep_set_list = shm_malloc(sizeof(struct lrep_set_head));
    if (lrep_set_list == NULL)
    {
                LM_ERR("no shm memory for lrep_set_list\n");
        return -1;
    }
    memset(lrep_set_list, 0, sizeof(struct lrep_set_head));

//	if (nortpproxy_str.s==NULL || nortpproxy_str.len<=0) {
//		nortpproxy_str.len = 0;
//	} else {
//		while (nortpproxy_str.len > 0
//				&& (nortpproxy_str.s[nortpproxy_str.len - 1] == '\r' ||
//					nortpproxy_str.s[nortpproxy_str.len - 1] == '\n'))
//			nortpproxy_str.len--;
//	}

//	if (rtpp_db_url.s != NULL)
//	{
//		init_rtpproxy_db();
//		if (lrep_sets > 0)
//		{
//			LM_WARN("rtpproxy db url configured - ignoring modparam sets\n");
//		}
//	}
    /* storing the list of rtp proxy sets in shared memory*/
    for(i=0;i<lrep_sets;i++){
                LM_DBG("Adding RTP-Proxy set %d/%d: %s\n", i, lrep_sets, rtpp_strings[i]);
//        if ((rtpp_db_url.s == NULL) &&
        if (lreproxy_add_lreproxy_set(rtpp_strings[i]) != 0) {
            for(;i<lrep_sets;i++)
                if(rtpp_strings[i])
                    pkg_free(rtpp_strings[i]);
            pkg_free(rtpp_strings);
                    LM_ERR("Failed to add RTP-Proxy from Config!\n");
            return -1;
        }
        if(rtpp_strings[i])
            pkg_free(rtpp_strings[i]);
    }

//	if (ice_candidate_priority_avp_param) {
//		s.s = ice_candidate_priority_avp_param; s.len = strlen(s.s);
//		if (pv_parse_spec(&s, &avp_spec) == 0 || avp_spec.type != PVT_AVP) {
//			LM_ERR("malformed or non AVP definition <%s>\n",
//					ice_candidate_priority_avp_param);
//			return -1;
//		}
//		if (pv_get_avp_name(0, &(avp_spec.pvp), &ice_candidate_priority_avp,
//					&avp_flags) != 0) {
//			LM_ERR("invalid AVP definition <%s>\n",
//					ice_candidate_priority_avp_param);
//			return -1;
//		}
//		ice_candidate_priority_avp_type = avp_flags;
//	}

//	if (rtp_inst_pv_param.s) {
//		rtp_inst_pvar = pv_cache_get(&rtp_inst_pv_param);
//		if ((rtp_inst_pvar == NULL) ||
//				((rtp_inst_pvar->type != PVT_AVP) &&
//					(rtp_inst_pvar->type != PVT_XAVP) &&
//					(rtp_inst_pvar->type != PVT_SCRIPTVAR))) {
//			LM_ERR("Invalid pvar name <%.*s>\n", rtp_inst_pv_param.len,
//					rtp_inst_pv_param.s);
//			return -1;
//		}
//	}

//	if (extra_id_pv_param.s && *extra_id_pv_param.s) {
//		if(pv_parse_format(&extra_id_pv_param, &extra_id_pv) < 0) {
//			LM_ERR("malformed PV string: %s\n", extra_id_pv_param.s);
//			return -1;
//		}
//	} else {
//		extra_id_pv = NULL;
//	}

    if (rtpp_strings)
        pkg_free(rtpp_strings);


    /* init the hastable which keeps the all media address for both party and also the elected_node <--> callid& via-branch relation */
    if (hash_table_size < 1){
        hash_table_size = HASH_SIZE;    //the default size 128 entry.
    }

    if (!lreproxy_hash_table_init(hash_table_size)) {
                LM_ERR("lreproxy_hash_table_init(%d) failed!\n", hash_table_size);
        return -1;
    } else {
                LM_INFO(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>lreproxy_hash_table_init(%d) success!\n", hash_table_size);
    }



    /* load tm module*/
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
    struct lrep_set  *lrep_list;
    struct lrep_node *pnode;

    if(lrep_set_list==NULL )
        return 0;

    /* do not init sockets for PROC_INIT and main process when fork=yes */
    if(rank==PROC_INIT || (rank==PROC_MAIN && dont_fork==0)) {
        return 0;
    }

    /* Iterate known RTP proxies - create sockets */
    mypid = getpid();

    rtpp_socks = (int*)pkg_malloc( sizeof(int)*rtpp_no );
    if (rtpp_socks==NULL) {
                LM_ERR("no more pkg memory\n");
        return -1;
    }
    memset(rtpp_socks, -1, sizeof(int)*rtpp_no);

    for(lrep_list = lrep_set_list->lset_first; lrep_list != 0;
        lrep_list = lrep_list->lset_next){

        for (pnode=lrep_list->ln_first; pnode!=0; pnode = pnode->ln_next){
            char *hostname;

            if (pnode->ln_umode == 0) {
                rtpp_socks[pnode->idx] = -1;
                goto rptest;
            }

            /*
             * This is UDP or UDP6. Detect host and port; lookup host;
             * do connect() in order to specify peer address
             */
            hostname = (char*)pkg_malloc(sizeof(char) * (strlen(pnode->ln_address) + 1));
            if (hostname==NULL) {
                        LM_ERR("no more pkg memory\n");
                return -1;
            }
            strcpy(hostname, pnode->ln_address);

            cp = strrchr(hostname, ':');
            if (cp != NULL) {
                *cp = '\0';
                cp++;
            }
            if (cp == NULL || *cp == '\0')
                cp = CPORT;

            memset(&hints, 0, sizeof(hints));
            hints.ai_flags = 0;
            hints.ai_family = (pnode->ln_umode == 6) ? AF_INET6 : AF_INET;
            hints.ai_socktype = SOCK_DGRAM;
            if ((n = getaddrinfo(hostname, cp, &hints, &res)) != 0) {
                        LM_ERR("%s\n", gai_strerror(n));
                pkg_free(hostname);
                return -1;
            }
            pkg_free(hostname);

            rtpp_socks[pnode->idx] = socket((pnode->ln_umode == 6)
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
            pnode->ln_enable = lrep_test(pnode);
            if (pnode->ln_enable) {       //get lre proxy config if it is enable.
//                LM_INFO("lrep_test test is calling here\n"); //enable next line.
                lrep_get_config(pnode);
            }
        }
    }

    return 0;
}


static void mod_destroy(void)
{
    struct lrep_set * crt_list, * last_list;
    struct lrep_node * crt_rtpp, *last_rtpp;

    /*free the shared memory*/
//	if (natping_state)
//		shm_free(natping_state);

    if(lrep_set_list == NULL)
        return;

    for(crt_list = lrep_set_list->lset_first; crt_list != NULL; ){

        for(crt_rtpp = crt_list->ln_first; crt_rtpp != NULL;  ){

            last_rtpp = crt_rtpp;
            crt_rtpp = last_rtpp->ln_next;
            shm_free(last_rtpp);
        }

        last_list = crt_list;
        crt_list = last_list->lset_next;
        shm_free(last_list);
    }

    shm_free(lrep_set_list);

    /* destroy the hash table */
    if (!lreproxy_hash_table_destroy()) {
                LM_ERR("lreproxy_hash_table_destroy() failed!\n");
    } else {
                LM_DBG("lreproxy_hash_table_destroy() success!\n");
    }

}



static char * gencookie(void)
{
    static char cook[34];

    sprintf(cook, "%d_%u ", (int)mypid, myseqn);
    myseqn++;
    return cook;
}

static int lrep_test(struct lrep_node *node)
{
    int buflen = 256;
    char buf[buflen];
    struct iovec v[2] = {{NULL, 0}, {"P", 1}};

    memset(buf, 0, buflen);
    memcpy(buf, send_lrep_command(node, v, 2, 0), buflen);

//    if (buf == NULL) {
    if (!buf[0]) {
        LM_ERR("can't ping the lre proxy %s, Disable it right now.\n", node->ln_url.s);
        return 0;
    }

    char *resp = buf + v[0].iov_len + v[1].iov_len + 1;
    if (memcmp(resp, "PONG", 4) == 0)
//                LM_DBG("Recieve PONG response from lre proxy server %s, Enable it right now.\n", node->ln_url.s);
            LM_INFO("Recieve PONG response from lre proxy server %s, Enable it right now.\n", node->ln_url.s);

    return 1;

}

static int lrep_get_config(struct lrep_node *node){

    int buflen = 256;
    char buf[buflen];
    struct iovec v[2] = {{NULL, 0}, {"G", 1}};
    struct lrep_node_conf *lnconf = NULL;

    memset(buf, 0, buflen);
    memcpy(buf, send_lrep_command(node, v, 2, 0), buflen);

//    if (buf == NULL) {
    if (!buf[0]) {
        LM_ERR("can't get config of the lre proxy %s, Disable it right now.\n", node->ln_url.s);
        return 0;
    }

    lnconf = (struct lrep_node_conf *)(buf + v[0].iov_len + v[1].iov_len + 1);

    if (lnconf == NULL){
        LM_ERR("can't get config of the lre proxy %s, Disable it right now.\n", node->ln_url.s);
        return 0;
    }


    memcpy(node->lrep_n_c, lnconf, sizeof(struct lrep_node_conf));

//    node->lrep_n_c = lnconf;
    LM_INFO("the lre proxy %s is configured successfully right now.\n", node->ln_url.s);
    LM_INFO("buffer internal:%s\n", node->lrep_n_c->internal_ip);
    LM_INFO("buffer external:%s\n", node->lrep_n_c->external_ip);
    LM_INFO("buffer start_port:%d\n", node->lrep_n_c->start_port);
    LM_INFO("buffer end_port:%d\n", node->lrep_n_c->end_port);
    LM_INFO("buffer current_port:%d\n", node->lrep_n_c->current_port);

    return 1;



}

static int lrep_set_conntrack_rule(struct lreproxy_hash_entry *e) {
    int buflen = 254;
    char buf[buflen];
    int v_len = 0;

    char src_ipv4[20];
    char src_port[20];
    char dst_ipv4[20];
    char dst_port[20];
    char snat_ipv4[20];
    char snat_port[20];
    char dnat_ipv4[20];
    char dnat_port[20];
    char timeout[20];

    struct iovec v[] = {
            {NULL, 0},  /* reserved (cookie) */
            {"S",  1},   /* command & common options */
            {NULL, 0},  /* src_ipv4 */
            {NULL, 0},  /* dst_ipv4 */
            {NULL, 0},  /* snat_ipv4 */
            {NULL, 0},  /* dnat_ipv4 */
            {NULL, 0},  /* src_port */
            {NULL, 0},  /* dst_port*/
            {NULL, 0},  /* snat_port */
            {NULL, 0},  /* dnat_port*/
            {NULL, 0},  /* timeout to clear conntrack entry*/
    };

    v_len += v[1].iov_len;

    //set src_ipv4 to buffer.
    sprintf(src_ipv4, "%.*s ", e->src_ipv4.len, e->src_ipv4.s);
    v[2].iov_base = src_ipv4;
    v[2].iov_len = strlen(v[2].iov_base);
    v_len += v[2].iov_len;

    //set dst_ipv4 to buffer.
    sprintf(dst_ipv4, "%.*s ", e->dst_ipv4.len, e->dst_ipv4.s);
    v[3].iov_base = dst_ipv4;
    v[3].iov_len = strlen(v[3].iov_base);
    v_len += v[3].iov_len;

    //set snat_ipv4 to buffer.
    sprintf(snat_ipv4, "%.*s ", e->snat_ipv4.len, e->snat_ipv4.s);
    v[4].iov_base = snat_ipv4;
    v[4].iov_len = strlen(v[4].iov_base);
    v_len += v[4].iov_len;

    //set dnat_ipv4 to buffer.
    sprintf(dnat_ipv4, "%.*s ", e->dnat_ipv4.len, e->dnat_ipv4.s);
    v[5].iov_base = dnat_ipv4;
    v[5].iov_len = strlen(v[5].iov_base);
    v_len += v[5].iov_len;

    //set src_port to buffer.
    sprintf(src_port, "%.*s ", e->src_port.len, e->src_port.s);
    v[6].iov_base = src_port;
    v[6].iov_len = strlen(v[6].iov_base);
    v_len += v[6].iov_len;

    //set dst_port to buffer.
    sprintf(dst_port, "%.*s ", e->dst_port.len, e->dst_port.s);
    v[7].iov_base = dst_port;
    v[7].iov_len = strlen(v[7].iov_base);
    v_len += v[7].iov_len;

    //set snat_port to buffer.
    sprintf(snat_port, "%.*s ", e->snat_port.len, e->snat_port.s);
    v[8].iov_base = snat_port;
    v[8].iov_len = strlen(v[8].iov_base);
    v_len += v[8].iov_len;

    //set dnat_port to buffer.
    sprintf(dnat_port, "%.*s ", e->dnat_port.len, e->dnat_port.s);
    v[9].iov_base = dnat_port;
    v[9].iov_len = strlen(v[9].iov_base);
    v_len += v[9].iov_len;

    //set timeout to buffer. Set to 60 sec for default.
    sprintf(timeout, "%d ", 60);
    v[10].iov_base = timeout;
    v[10].iov_len = strlen(v[10].iov_base);
    v_len += v[10].iov_len;

    memset(buf, 0, buflen);
    memcpy(buf, send_lrep_command(e->node, v, 11, v_len), buflen);
//

//    if (buf == NULL) {
    if (!buf[0]) {
                LM_ERR("can't ping the lre proxy %s, Disable it right now.\n", e->node->ln_url.s);
        return 0;
    }

    v_len += v[0].iov_len;


//    char *resp = buf + v[0].iov_len + v[1].iov_len + v[2].iov_len;
    char *resp = buf + v_len;
    if (memcmp(resp, "OK", 2) == 0) {
                LM_INFO("Recieve OK response from lre proxy server %s, Rule set successfully.\n", e->node->ln_url.s);
                LM_DBG("Recieve OK response from lre proxy server %s, Rule set successfully.\n", e->node->ln_url.s);
    }
    return 1;

}


char *send_lrep_command(struct lrep_node *node, struct iovec *v, int vcnt, int more)
{
    struct sockaddr_un addr;
    int fd, len, i;
//    char *cp;
    static char buf[256];
    struct pollfd fds[1];

    memset(buf, 0, 256);
    len = 0;
//    cp = buf;
    if (node->ln_umode == 0) {
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_LOCAL;
        strncpy(addr.sun_path, node->ln_address,
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
                    LM_ERR("can't connect to lre proxy\n");
            goto badproxy;
        }

        do {
            len = writev(fd, v + 1, vcnt - 1);
        } while (len == -1 && errno == EINTR);
        if (len <= 0) {
            close(fd);
                    LM_ERR("can't send command to a lre proxy %s\n", node->ln_url.s);
            goto badproxy;
        }
        do {
            len = read(fd, buf, sizeof(buf) - 1);
        } while (len == -1 && errno == EINTR);
        close(fd);
        if (len <= 0) {
                    LM_ERR("can't read reply from a lre proxy %s\n", node->ln_url.s);
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
        for (i = 0; i < lreproxy_retr; i++) {
            do {
                len = writev(rtpp_socks[node->idx], v, vcnt);
            } while (len == -1 && (errno == EINTR || errno == ENOBUFS));
            if (len <= 0) {
                        LM_ERR("can't send command to a lre proxy %s\n", node->ln_url.s);
                goto badproxy;
            }
            while ((poll(fds, 1, lreproxy_tout * 1000) == 1) &&
                   (fds[0].revents & POLLIN) != 0) {
                do {
                    len = recv(rtpp_socks[node->idx], buf, sizeof(buf) - 1, 0);
                } while (len == -1 && errno == EINTR);
                if (len <= 0) {
                            LM_ERR("can't read reply from a lre proxy %s\n", node->ln_url.s);
                    goto badproxy;
                }
                if (len >= (v[0].iov_len - 1) &&
                    memcmp(buf, v[0].iov_base, (v[0].iov_len - 1)) == 0) {      //check coocke validation.
                    char *command = buf + v[0].iov_len;
                    switch (*command) {
                        case 'P':
                            if (len == v[0].iov_len + v[1].iov_len + 4 + 1)
                                goto out;
//                            break;
                        case 'G':
                            if (len == v[0].iov_len + v[1].iov_len + sizeof(struct lrep_node_conf) + 1)
                                goto out;
//                            break;
                        case 'S':
                            if (len == more + v[0].iov_len + 2)
                                goto out;
//                            break;
                    }
//                    len -= (v[0].iov_len - 1);
//                    cp += (v[0].iov_len - 1);
//                    if (len != 0) {
//                        len--;
//                        cp++;
//                    }
//                    goto out;
//                    }
                }
                fds[0].revents = 0;
            }
        }
        if (i == lreproxy_tout) {
                    LM_ERR("timeout waiting reply from a lre proxy server %s\n", node->ln_url.s);
            goto badproxy;

        }
    }
    out:
    return buf;
    badproxy:
            LM_ERR("lre proxy <%s> does not respond, disable it\n", node->ln_url.s);
    node->ln_enable = 0;
//    node->ln_recheck_ticks = get_ticks() + lreproxy_disable_tout;
    return buf;
}

/*
 * select the set with the id_set id
 */
static struct lrep_set * select_lrep_set(int id_set ){

    struct lrep_set * lrep_list;
    /*is it a valid set_id?*/

    if(!lrep_set_list)
    {
                LM_ERR("rtpproxy set list not initialised\n");
        return NULL;
    }

    for(lrep_list=lrep_set_list->lset_first; lrep_list!=NULL &&
                                             lrep_list->id_set!=id_set; lrep_list=lrep_list->lset_next);

    return lrep_list;
}


struct lrep_node *select_lrep_node(int do_test)
{
//    unsigned sum, sumcut, weight_sum;
    unsigned weight_sum;
    struct lrep_node* node;
    int was_forced;
    int was_forced2;
    int was_forced3;

    if(!selected_lrep_set){
        LM_ERR("script error -no valid set selected\n");
        return NULL;
    }
    /* Most popular case: 1 proxy, nothing to calculate */
    if (selected_lrep_set->lrep_node_count == 1) {
        node = selected_lrep_set->ln_first;
//        if (node->rn_disabled && node->rn_recheck_ticks <= get_ticks())
        if (!node->ln_enable) {
            node->ln_enable = lrep_test(node);
            if (node->ln_enable) {       //get lre proxy config if it is enable.
                lrep_get_config(node);
                return node;
            }
        }
//        return node->ln_enable ? node : NULL;
        return NULL;
    }

//    /* XXX Use quick-and-dirty hashing algo */
//    for(sum = 0; callid.len > 0; callid.len--)
//        sum += callid.s[callid.len - 1];
//    sum &= 0xff;


    /* Check node is enable and test it again*/
    was_forced = 0;
retry:
    weight_sum = 0;
    for (node=selected_lrep_set->ln_first; node!=NULL; node=node->ln_next) {

        if (!node->ln_enable) {
            /* Try to enable if it's time to try. */
            node->ln_enable = lrep_test(node);
            if (node->ln_enable)       //get lre proxy config if it is enable.
                lrep_get_config(node);
        }

//        if (!node->rn_disabled)
//            weight_sum += node->rn_weight;
        if (node->ln_enable)
            weight_sum += node->ln_weight;
    }

    if (weight_sum == 0) {
        /* No proxies? Force all to be redetected, if not yet */
        if (was_forced)
            return NULL;
        was_forced = 1;
//        for(node=selected_lrep_set->ln_first; node!=NULL; node=node->ln_next) {
//            node->ln_enable = lrep_test(node);
//        }
        goto retry;
    }

    if (lrep_algorithm == LRE_LINER) {
        was_forced2 = 0;
retry2:
        for (node=selected_lrep_set->ln_first; node != NULL; node = node->ln_next)
            if (node->ln_enable)
                goto found;
        was_forced2 = 1;
        if (was_forced2)
            return NULL;

        goto retry2;
    }
    else if(lrep_algorithm == LRE_RR) {
        was_forced3 = 0;
retry3:
        if (!selected_lrep_node) {
            selected_lrep_node = selected_lrep_set->ln_first;
            was_forced3 = 1;
        }
        for (node = selected_lrep_node; node != NULL; node = node->ln_next) {
            if (!node->ln_enable)
                continue;
            selected_lrep_node = node->ln_next;
//        if (sumcut < node->ln_weight)
            goto found;
//        sumcut -= node->ln_weight;
        }

        if (was_forced3)
            return NULL;

        selected_lrep_node = NULL;
        goto retry3;
    }

//            LM_INFO("STEP-4>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");


//    sumcut = sum % weight_sum;
//    /*
//     * sumcut here lays from 0 to weight_sum-1.
//     * Scan proxy list and decrease until appropriate proxy is found.
//     */
//    for (node=selected_rtpp_set->ln_first; node!=NULL; node=node->ln_next) {
//        if (!node->ln_enable)
//            continue;
//        if (sumcut < node->ln_weight)
//            goto found;
//        sumcut -= node->ln_weight;
//    }
    /* No node list */
//    return NULL;
    found:
    if (do_test) {
//    //todo...
        node->ln_enable = lrep_test(node);
        if (!node->ln_enable)
            goto retry;
    }
    return node;
}



//static int change_media_sdp(sip_msg_t *msg, struct lrep_node *n, const char *flags, int type) {
static int change_media_sdp(sip_msg_t *msg, struct lreproxy_hash_entry *e, const char *flags, enum lre_operation op) {
    struct lump *l;
    str body;
    str newbody;

    int len;
    char *start_sdp_o = NULL; //"o=";
    char *start_sdp_s = NULL; //"s=";
    char *start_sdp_c = NULL; //"c=IN IP4";
    char *start_sdp_m = NULL; //"m=audio";
    char *ip_selected = NULL;
    char *sdp_param_start = NULL;
    char *sdp_param_end = NULL;
    char *off=NULL;
    char sdp_new_o[128];
    char sdp_new_s[128];
    char sdp_new_c[128];
    char sdp_new_m[128];

    body.s = get_body(msg);
    if (body.s == 0) {
                LM_ERR("failed to get the message body\n");
        return -1;
    }

    body.len = msg->len - (int) (body.s - msg->buf);
    if (body.len == 0) {
                LM_DBG("message body has zero length\n");
        return -1;
    }
//            LM_INFO("body:<%.*s>\n", body.len, body.s);

    //allocate new buffer to new sdp buffer.
    newbody.len = 1024;
    newbody.s = pkg_malloc(newbody.len);
    if (newbody.s == NULL) {
                LM_ERR("out of pkg memory\n");
        return -1;
    }
    memset(newbody.s, 0, 1024);

    off = body.s;
    start_sdp_o = strstr(off, "o=");
    start_sdp_s = strstr(off, "s=");
    start_sdp_c = strstr(off, "c=IN IP4");
    start_sdp_m = strstr(off, "m=audio");


    //if enabled then set direction,
    if (e->node->lrep_n_c->internal_ip && flags) {
        if (strstr(flags, "ei")) {
            ip_selected = e->node->lrep_n_c->internal_ip;// lre_node->internal_ip;
        } else if (strstr(flags, "ie")) {
            ip_selected = e->node->lrep_n_c->external_ip; //lre_node->external_ip;
        } else {
                    LM_INFO("no flags found\n");
            return 0;
        }
    } else {
        ip_selected = e->node->lrep_n_c->external_ip; //lre_node->external_ip;
    }

    if (op == OP_OFFER) {
        e->dst_ipv4.s = ip_selected;
        e->dst_ipv4.len = strlen(e->dst_ipv4.s);

        str current_port;
        current_port.s = int2str(e->node->lrep_n_c->current_port, &current_port.len);

        if (shm_str_dup(&e->dst_port, &current_port) < 0) {
                    LM_ERR("lreproxy fail to insert dst_port, calllen=%d dst_port=%.*s\n",
                           e->callid.len, current_port.len, current_port.s);
            lreproxy_hash_table_free_entry(e);
            return 0;
        }

//        e->dst_port = e->node->lrep_n_c->current_port;
    }
    else if (op == OP_ANSWER){
        e->snat_ipv4.s = ip_selected;
        e->snat_ipv4.len = strlen(e->snat_ipv4.s);

        str current_port;
        unsigned int snat_port;

        str2int(&e->dst_port, &snat_port);
        snat_port += 2;

        current_port.s = int2str(snat_port, &current_port.len);

        if (shm_str_dup(&e->snat_port, &current_port) < 0) {
                    LM_ERR("lreproxy fail to insert snat_port, calllen=%d snat_port=%.*s\n",
                           e->callid.len, current_port.len, current_port.s);
            lreproxy_hash_table_free_entry(e);
            return 0;
        }

//        e->snat_port = e->dst_port + 2;
    }


    while (*off != EOB)    //while end of body.
    {
        sdp_param_start = off;
        sdp_param_end = sdp_param_start;
        while (*sdp_param_end != CR && *sdp_param_end != LF && *sdp_param_end != EOB) sdp_param_end++;
        len = (int) (sdp_param_end - sdp_param_start);
        if ((int) (start_sdp_o - off) == 0) {
            memset(sdp_new_o, 0, 128);
            snprintf(sdp_new_o, 128, "o=lreproxy %s %s IN IP4 %s\r", SUP_CPROTOVER, REQ_CPROTOVER, ip_selected);
            strncat(newbody.s, sdp_new_o, strlen(sdp_new_o));
            off += len + 1;
            continue;
        }
        if ((int) (start_sdp_s - off) == 0) {
            memset(sdp_new_s, 0, 128);
            snprintf(sdp_new_s, 128, "s=lreproxy Support only Audio Call\r");
            strncat(newbody.s, sdp_new_s, strlen(sdp_new_s));
            off += len + 1;
            continue;
        }
        if ((int) (start_sdp_c - off) == 0) {
            memset(sdp_new_c, 0, 128);
            snprintf(sdp_new_c, 128, "c=IN IP4 %s\r", ip_selected);
            strncat(newbody.s, sdp_new_c, strlen(sdp_new_c));
            off += len + 1;
            continue;
        }
        if ((int)(start_sdp_m - off) == 0){
            memset(sdp_new_m, 0, 128);
            char *avp_flags = off;
//            int occure = 0;
            for (;*avp_flags && !isspace(*avp_flags); avp_flags++);
            for (avp_flags++;*avp_flags && !isspace(*avp_flags); avp_flags++);
            avp_flags++;
//            while(occure <2) {
//                if (isspace(*avp_flags))
//                    occure++;
//                avp_flags++;
//            }
//            snprintf(sdp_new_m, 128, "m=audio %d \r\n",port);
//            snprintf(sdp_new_m, 128, "m=audio %d %.*s\r",port, (int)(len - (avp_flags-off)), avp_flags);
//            snprintf(sdp_new_m, 128, "m=audio %d %.*s\r",n->lrep_n_c->current_port, (int)(len - (avp_flags-off)), avp_flags);
            if (op == OP_OFFER)
                snprintf(sdp_new_m, 128, "m=audio %.*s %.*s\r",e->dst_port.len, e->dst_port.s, (int)(len - (avp_flags-off)), avp_flags);
//                snprintf(sdp_new_m, 128, "m=audio %d %.*s\r",e->node->lrep_n_c->current_port, (int)(len - (avp_flags-off)), avp_flags);
            else if (op == OP_ANSWER)
                snprintf(sdp_new_m, 128, "m=audio %.*s %.*s\r",e->snat_port.len, e->snat_port.s, (int)(len - (avp_flags-off)), avp_flags);
//               snprintf(sdp_new_m, 128, "m=audio %d %.*s\r",e->node->lrep_n_c->current_port, (int)(len - (avp_flags-off)), avp_flags);
//            printf("%.*s\n\n", len - (avp_flags-off), avp_flags);
            strncat(newbody.s,sdp_new_m, strlen(sdp_new_m));
            off += len+1;
            continue;
        }

        strncat(newbody.s, off, len + 1);
        off += len + 1;
    }


//    LM_INFO("%.*s", (int)strlen(newbody.s), newbody.s);
    l = del_lump(msg, body.s - msg->buf, body.len, 0);
    if (!l) {
                LM_ERR("del_lump failed\n");
        return -1;
    }


    if (insert_new_lump_after(l, newbody.s, strlen(newbody.s), 0) == 0) {
                LM_ERR("could not insert new lump\n");
        pkg_free(newbody.s);
        return -1;
    }

            LM_BUG("Insert_new_lump successfully\n");

    return 1;
}


static int
set_lre_proxy_set_f(struct sip_msg * msg, char * str1, char * str2)
{
	lrep_set_link_t *rtpl;
	pv_value_t val;

	rtpl = (lrep_set_link_t*)str1;

	current_msg_id = 0;
	selected_lrep_set = 0;

	if(rtpl->rset != NULL) {
		current_msg_id = msg->id;
		selected_lrep_set = rtpl->rset;
	} else {
		if(pv_get_spec_value(msg, rtpl->rpv, &val)<0) {
			LM_ERR("cannot evaluate pv param\n");
			return -1;
		}
		if(!(val.flags & PV_VAL_INT)) {
			LM_ERR("pv param must hold an integer value\n");
			return -1;
		}
		selected_lrep_set = select_lrep_set(val.ri);
		if(selected_lrep_set==NULL) {
			LM_ERR("could not locate rtpproxy set %d\n", val.ri);
			return -1;
		}
		current_msg_id = msg->id;
	}
	return 1;
}

static int
lreproxy_manage(struct sip_msg *msg, char *flags, char *ip)
{
    int method;
    int nosdp;
    tm_cell_t *t = NULL;

    if (msg->cseq==NULL && ((parse_headers(msg, HDR_CSEQ_F, 0)==-1) ||
                            (msg->cseq==NULL)))
    {
                LM_ERR("no CSEQ header\n");
        return -1;
    }

    method = get_cseq(msg)->method_id;

    if (!(method==METHOD_INVITE || method==METHOD_ACK || method==METHOD_CANCEL
          || method==METHOD_BYE || method==METHOD_UPDATE))
        return -1;

    if (method==METHOD_CANCEL || method==METHOD_BYE)
        return lreproxy_unforce(msg, flags, OP_DELETE, 1);

    if (msg->msg_flags & FL_SDP_BODY)
        nosdp = 0;
    else
        nosdp = parse_sdp(msg);

    if (msg->first_line.type == SIP_REQUEST) {
        if(method==METHOD_ACK && nosdp==0)
            return lreproxy_force(msg, flags, OP_ANSWER, 1);
        if(method==METHOD_UPDATE && nosdp==0)
            return lreproxy_force(msg, flags, OP_OFFER, 1);
        if(method==METHOD_INVITE && nosdp==0) {
            msg->msg_flags |= FL_SDP_BODY;
            if(tmb.t_gett!=NULL) {
                t = tmb.t_gett();
                if(t!=NULL && t!=T_UNDEFINED && t->uas.request!=NULL) {
                    t->uas.request->msg_flags |= FL_SDP_BODY;
                }
            }
            if(route_type==FAILURE_ROUTE)
                return lreproxy_unforce(msg, flags, OP_DELETE, 1);
            return lreproxy_force(msg, flags, OP_OFFER, 1);
        }
    } else if (msg->first_line.type == SIP_REPLY) {
        if (msg->first_line.u.reply.statuscode>=300)
            return lreproxy_unforce(msg, flags, OP_DELETE, 2);
        if (nosdp==0) {
            if (method==METHOD_UPDATE)
                return lreproxy_force(msg, flags, OP_ANSWER, 2);
            if (tmb.t_gett==NULL || tmb.t_gett()==NULL
                || tmb.t_gett()==T_UNDEFINED)
                return lreproxy_force(msg, flags, OP_ANSWER, 2);
            if (tmb.t_gett()->uas.request->msg_flags & FL_SDP_BODY)
                return lreproxy_force(msg, flags, OP_ANSWER, 2);
            return lreproxy_force(msg, flags, OP_OFFER, 2);
        }
    }

    return -1;
}

static int
lreproxy_manage0(struct sip_msg *msg, char *flags, char *ip)
{
    return lreproxy_manage(msg, 0, 0);
}

static int
lreproxy_manage1(struct sip_msg *msg, char *flags, char *ip)
{
    str flag_str;
    if(fixup_get_svalue(msg, (gparam_p)flags, &flag_str)<0)
    {
                LM_ERR("invalid flags parameter\n");
        return -1;
    }
    return lreproxy_manage(msg, flag_str.s, 0);
}

static int
lreproxy_manage2(struct sip_msg *msg, char *flags, char *ip)
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
    return lreproxy_manage(msg, flag_str.s, ip_str.s);
}


static int lreproxy_force(struct sip_msg *msg, const char *flags, enum lre_operation op, int more) {

//    lre_sdp_info_t lre_sdp_info;
    struct lreproxy_hash_entry *entry = NULL;
    str viabranch = STR_NULL;
    str call_id;
    int via_id;

    if (get_callid(msg, &call_id) == -1) {
                LM_ERR("can't get Call-Id field\n");
        return -1;
    }

    /*We have to choice VIA id,
     * for SIP_REQUEST we use VIA1 and for SIP_REPLY we use VIA2 */
    via_id = more;

    if (get_via_branch(msg, via_id, &viabranch) == -1) {
                LM_ERR("can't get Call-Id field\n");
        return -1;
    }

    if (op == OP_OFFER) {
                LM_INFO ("Here is SIP_REQUEST &  METHOD_INVITE\n");
        int more_test = 1;

        //select new node based on lrep_algorithm param.
        struct lrep_node *node = select_lrep_node(more_test);
        if (!node) {
                    LM_ERR("can't ping any lre proxy right now.\n");
            return -1;
        }

        LM_DBG("selected lre proxy node: %s\n", node->ln_url.s);

        //check if entry not exist.
        if (!lreproxy_hash_table_lookup(call_id, viabranch)) {

//        lre_get_sdp_info(msg, &lre_sdp_info);

            //build new entry for hash table.
//            struct lreproxy_hash_entry *entry = shm_malloc(sizeof(struct lreproxy_hash_entry));
            entry = shm_malloc(sizeof(struct lreproxy_hash_entry));
            if (!entry) {
                        LM_ERR("lreproxy hash table fail to create entry for calllen=%d callid=%.*s viabranch=%.*s\n",
                               call_id.len, call_id.len, call_id.s,
                               viabranch.len, viabranch.s);
                return 0;
            }
            memset(entry, 0, sizeof(struct lreproxy_hash_entry));

            // fill the entry
            if (call_id.s && call_id.len > 0) {
                if (shm_str_dup(&entry->callid, &call_id) < 0) {
                            LM_ERR("lreproxy hash table fail to instert call_id, calllen=%d callid=%.*s\n",
                                   call_id.len, call_id.len, call_id.s);
                    lreproxy_hash_table_free_entry(entry);
                    return 0;
                }
            }

            if (viabranch.s && viabranch.len > 0) {
                if (shm_str_dup(&entry->viabranch, &viabranch) < 0) {
                            LM_ERR("lreproxy hash table fail to insert viabranch, calllen=%d viabranch=%.*s\n",
                                   call_id.len, viabranch.len, viabranch.s);
                    lreproxy_hash_table_free_entry(entry);
                    return 0;
                }
            }

            //fill src_ipv4 and src_port for entry.
            str src_ipv4;
            if (get_sdp_ipaddr_media(msg, &src_ipv4) == -1) {
                        LM_ERR("can't get media src_ipv4 from sdp field\n");
                return -1;
            }

            if(src_ipv4.s && src_ipv4.len > 0) {
                        LM_DBG("src_ipv4 from sdp:%.*s\n", src_ipv4.len, src_ipv4.s);
                if (shm_str_dup(&entry->src_ipv4, &src_ipv4) < 0) {
                            LM_ERR("lreproxy hash table fail to insert src_ipv4, calllen=%d src_ipv4=%.*s\n",
                                   call_id.len, src_ipv4.len, src_ipv4.s);
                    lreproxy_hash_table_free_entry(entry);
                    return 0;
                }
            }

            str src_port;
            if (get_sdp_port_media(msg, &src_port) == -1) {
                        LM_ERR("can't get media src_port from sdp field\n");
                return -1;
            }


            if(src_port.s && src_port.len > 0) {
                        LM_DBG("src_port from sdp:%.*s\n", src_port.len, src_port.s);
                if (shm_str_dup(&entry->src_port, &src_port) < 0) {
                            LM_ERR("lreproxy hash table fail to insert src_port, calllen=%d src_port=%.*s\n",
                                   call_id.len, src_port.len, src_port.s);
                    lreproxy_hash_table_free_entry(entry);
                    return 0;
                }
            }

//            entry->
            entry->node = node;
            entry->next = NULL;
            entry->tout = get_ticks() + hash_table_tout;

            // insert the key<->entry from the hashtable
            if (!lreproxy_hash_table_insert(call_id, viabranch, entry)) {
                        LM_ERR(
                        "lreproxy hash table fail to insert node=%.*s for calllen=%d callid=%.*s viabranch=%.*s\n",
                        node->ln_url.len, node->ln_url.s, call_id.len,
                        call_id.len, call_id.s, viabranch.len, viabranch.s);
                lreproxy_hash_table_free_entry(entry);
                return 0;
            } else {
                        LM_INFO("lreproxy hash table insert node=%.*s for calllen=%d callid=%.*s viabranch=%.*s\n",
                                node->ln_url.len, node->ln_url.s, call_id.len,
                                call_id.len, call_id.s, viabranch.len, viabranch.s);

                        LM_DBG("lreproxy hash table insert node=%.*s for calllen=%d callid=%.*s viabranch=%.*s\n",
                               node->ln_url.len, node->ln_url.s, call_id.len,
                               call_id.len, call_id.s, viabranch.len, viabranch.s);
            }
        }

        if (flags)
            change_media_sdp(msg, entry, flags, op);
        else
            change_media_sdp(msg, entry, NULL, op);

        if (node->lrep_n_c->current_port >= node->lrep_n_c->end_port)
            node->lrep_n_c->current_port = node->lrep_n_c->start_port;
        else
            node->lrep_n_c->current_port += 4;

    } else if (op == OP_ANSWER) {
                LM_INFO ("Here is SIP_REPLY of METHOD_INVITE\n");


        entry = lreproxy_hash_table_lookup(call_id, viabranch);
        if (!entry){
                    LM_ERR("No found entry in hash table\n");
                    //todo...
            return 0;
        }

        //fill other data for entry
        str dnat_ipv4;
        if (get_sdp_ipaddr_media(msg, &dnat_ipv4) == -1) {
                    LM_ERR("can't get media dnat_ipv4 from sdp field\n");
            return -1;
        }

        if(dnat_ipv4.s && dnat_ipv4.len > 0) {
                    LM_DBG("dnat_ipv4 from sdp:%.*s\n", dnat_ipv4.len, dnat_ipv4.s);
            if (shm_str_dup(&entry->dnat_ipv4, &dnat_ipv4) < 0) {
                        LM_ERR("lreproxy hash table fail to insert dnat_ipv4, calllen=%d dnat_ipv4=%.*s\n",
                               call_id.len, dnat_ipv4.len, dnat_ipv4.s);
                lreproxy_hash_table_free_entry(entry);
                return 0;
            }
        }

        str dnat_port;
        if (get_sdp_port_media(msg, &dnat_port) == -1) {
                    LM_ERR("can't get media port from sdp field\n");
            return -1;
        }


        if(dnat_port.s && dnat_port.len > 0) {
                    LM_DBG("port from sdp:%.*s\n", dnat_port.len, dnat_port.s);
            if (shm_str_dup(&entry->dnat_port, &dnat_port) < 0) {
                        LM_ERR("lreproxy hash table fail to insert dnat_port, calllen=%d dnat_port=%.*s\n",
                               call_id.len, dnat_port.len, dnat_port.s);
                lreproxy_hash_table_free_entry(entry);
                return 0;
            }
        }


        if (flags)
            change_media_sdp(msg, entry, flags, op);
        else
            change_media_sdp(msg, entry, NULL, op);


        LM_INFO("selected node: %s\n",entry->node->ln_url.s);
        LM_INFO("call_is: %.*s\n",entry->callid.len, entry->callid.s);
        LM_INFO("viabranch: %.*s\n",entry->viabranch.len, entry->viabranch.s);
        LM_INFO("src_ipv4: %.*s\n",entry->src_ipv4.len, entry->src_ipv4.s);
        LM_INFO("src_port: %.*s\n",entry->src_port.len, entry->src_port.s);
        LM_INFO("dst_ipv4: %.*s\n",entry->dst_ipv4.len, entry->dst_ipv4.s);
        LM_INFO("dst_port: %.*s\n",entry->dst_port.len, entry->dst_port.s);

        LM_INFO("dnat_ipv4: %.*s\n",entry->dnat_ipv4.len, entry->dnat_ipv4.s);
        LM_INFO("dnat_port: %.*s\n",entry->dnat_port.len, entry->dnat_port.s);
        LM_INFO("snat_ipv4: %.*s\n",entry->snat_ipv4.len, entry->snat_ipv4.s);
        LM_INFO("snat_port: %.*s\n",entry->snat_port.len, entry->snat_port.s);


        lrep_set_conntrack_rule(entry);


//        lre_get_sdp_info(msg, &lre_sdp_info);

        //todo...
//        change_media_sdp(msg, n, flags, op);
//        else
//        change_media_sdp(msg, n, NULL, op);

    }
    return 1;
}

static int lreproxy_unforce(struct sip_msg *msg, const char *flags, enum lre_operation op, int more){
//            LM_INFO ("Here is lreproxy_unforce\n");
//    struct lreproxy_hash_entry *entry = NULL;
    str viabranch = STR_NULL;
    str call_id;
    int via_id;

    if (get_callid(msg, &call_id) == -1) {
                LM_ERR("can't get Call-Id field\n");
        return -1;
    }

    /*We have to choice VIA id,
     * for SIP_REQUEST we use VIA1 and for SIP_REPLY we use VIA2 */
    via_id = more;

    if (get_via_branch(msg, via_id, &viabranch) == -1) {
                LM_ERR("can't get Call-Id field\n");
        return -1;
    }

    if (op == OP_DELETE) {
        /* Delete the key<->value from the hashtable */
        if (!lreproxy_hash_table_remove(call_id, viabranch, op)) {
                    LM_ERR("lreproxy hash table failed to remove entry for callen=%d callid=%.*s viabranch=%.*s\n",
                           call_id.len, call_id.len, call_id.s,
                           viabranch.len, viabranch.s);
        } else {
                    LM_DBG("lreproxy hash table remove entry for callen=%d callid=%.*s viabranch=%.*s\n",
                           call_id.len, call_id.len, call_id.s,
                           viabranch.len, viabranch.s);
        }
    }

    return 1;
}

