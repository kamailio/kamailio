/*
 * Copyright (C) 2003-2008 Sippy Software, Inc., http://www.sippysoft.com
 * Copyright (C) 2020 Mojtaba Esfandiari.S, Nasim-Telecom
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
#include "lrkproxy.h"
#include "lrkproxy_hash.h"
#include "lrkproxy_funcs.h"

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

#define DEFAULT_LRKP_SET_ID		0
static str DEFAULT_LRKP_SET_ID_STR = str_init("0");

//#define RPC_DEFAULT_NATPING_STATE	1

#define RPC_MIN_RECHECK_TICKS		0
#define RPC_MAX_RECHECK_TICKS		(unsigned int)-1


/* Supported version of the LRK proxy command protocol */
#define	SUP_CPROTOVER	"20190708"
/* Required additional version of the LRK proxy command protocol */
#define	REQ_CPROTOVER	"20190709"
/* Additional version necessary for re-packetization support */
#define	REP_CPROTOVER	"20190708"
#define	PTL_CPROTOVER	"20190708"

#define	CPORT		"22333"
#define HASH_SIZE   128

static char *gencookie();
static int lrkp_test(struct lrkp_node*);
static int lrkp_get_config(struct lrkp_node *node);
static int lrkp_set_conntrack_rule(struct lrkproxy_hash_entry *e);


static int lrkproxy_force(struct sip_msg *msg, const char *flags, enum lrk_operation op, int more);
static int lrkproxy_unforce(struct sip_msg *msg, const char *flags, enum lrk_operation op, int more);

static int lrkproxy_manage0(struct sip_msg *msg, char *flags, char *ip);
static int lrkproxy_manage1(struct sip_msg *msg, char *flags, char *ip);
static int lrkproxy_manage2(struct sip_msg *msg, char *flags, char *ip);

static int change_media_sdp(sip_msg_t *msg, struct lrkproxy_hash_entry *e, const char *flags, enum lrk_operation op);

static int add_lrkproxy_socks(struct lrkp_set * lrkp_list, char * lrkproxy);
static int fixup_set_id(void ** param, int param_no);
static int set_lrkproxy_set_f(struct sip_msg * msg, char * str1, char * str2);

static struct lrkp_set * select_lrkp_set(int id_set);

static int lrkproxy_set_store(modparam_t type, void * val);
static int lrkproxy_add_lrkproxy_set( char * lrk_proxies);

static int mod_init(void);
static int child_init(int);
static void mod_destroy(void);

/* Pseudo-Variables */
//static int pv_get_lrkstat_f(struct sip_msg *, pv_param_t *, pv_value_t *);

static int lrkproxy_disable_tout = 60;
static int lrkproxy_retr = 5;
static int lrkproxy_tout = 1;
static pid_t mypid;
static unsigned int myseqn = 0;
//static str nolrkproxy_str = str_init("a=nolrkproxy:yes");
//static str extra_id_pv_param = {NULL, 0};

static char ** lrkp_strings=0;
static int lrkp_sets=0; /*used in lrkproxy_set_store()*/
static int lrkp_set_count = 0;
static unsigned int current_msg_id = (unsigned int)-1;
/* LRK proxy balancing list */
struct lrkp_set_head * lrkp_set_list =0;
struct lrkp_set * selected_lrkp_set =0;
struct lrkp_set * default_lrkp_set=0;
struct lrkp_node *selected_lrkp_node = 0;
int lrkp_algorithm = LRK_LINER;
static int hash_table_size = 0;
static int hash_table_tout = 3600;

/*!< The gt is game-theory variable, It could be set 0:disable and 1:enable
 * default is 0.
 */
int gt = 0;

/*
 * the custom_sdp_ip_spec variable is used for specific SDP information based $si (source address)
 * */
static str custom_sdp_ip_spec = {NULL, 0};
pv_spec_t custom_sdp_ip_avp;




//static char *ice_candidate_priority_avp_param = NULL;
//static int ice_candidate_priority_avp_type;
//static int_str ice_candidate_priority_avp;
//static str lrk_inst_pv_param = {NULL, 0};
//static pv_spec_t *lrk_inst_pvar = NULL;

/* array with the sockets used by lrkproxy (per process)*/
static unsigned int lrkp_no = 0;
static int *lrkp_socks = 0;


typedef struct lrkp_set_link {
	struct lrkp_set *rset;
	pv_spec_t *rpv;
} lrkp_set_link_t;

/* tm */
static struct tm_binds tmb;

/*0-> disabled, 1 ->enabled*/
//unsigned int *natping_state=0;

static cmd_export_t cmds[] = {

        {"set_lrkproxy_set",  (cmd_function)set_lrkproxy_set_f,    1,
                fixup_set_id, 0,
                ANY_ROUTE},
        {"lrkproxy_manage",	(cmd_function)lrkproxy_manage0,     0,
                                                       0, 0,
                   ANY_ROUTE},
        {"lrkproxy_manage",	(cmd_function)lrkproxy_manage1,     1,
                                                       fixup_spve_null, fixup_free_spve_null,
                   ANY_ROUTE},
        {"lrkproxy_manage",	(cmd_function)lrkproxy_manage2,     2,
                                                       fixup_spve_spve, fixup_free_spve_spve,
                   ANY_ROUTE},

        {0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
        {"lrkproxy_sock",         PARAM_STRING|USE_FUNC_PARAM,
                (void*)lrkproxy_set_store          },
        {"lrkproxy_disable_tout", INT_PARAM, &lrkproxy_disable_tout },
        {"lrkproxy_retr",         INT_PARAM, &lrkproxy_retr         },
        {"lrkproxy_tout",         INT_PARAM, &lrkproxy_tout         },
        {"lrkp_alg",         INT_PARAM, &lrkp_algorithm         },
        {"hash_table_tout",       INT_PARAM, &hash_table_tout        },
        {"hash_table_size",       INT_PARAM, &hash_table_size        },
        {"custom_sdp_ip_avp",     PARAM_STR, &custom_sdp_ip_spec},
        {"gt",   INT_PARAM  , &gt},

        {0, 0, 0}
};

/** module exports */
struct module_exports exports= {
        "lrkproxy",        /* module name */
        DEFAULT_DLFLAGS, /* dlopen flags */
        cmds,            /* cmd exports */
        params,          /* param exports */
        0,               /* RPC method exports */
        0,         /* exported pseudo-variables */
        0,               /* response handling function */
        mod_init,        /* module initialization function */
        child_init,               /* per-child init function */
        mod_destroy                /* module destroy function */
};


static int lrkproxy_set_store(modparam_t type, void * val){

    char * p;
    int len;

    p = (char* )val;

    if(p==0 || *p=='\0'){
        return 0;
    }

    if(lrkp_sets==0){
        lrkp_strings = (char**)pkg_malloc(sizeof(char*));
        if(!lrkp_strings){
                    LM_ERR("no pkg memory left\n");
            return -1;
        }
    } else {/*realloc to make room for the current set*/
        lrkp_strings = (char**)pkg_reallocxf(lrkp_strings,
                                             (lrkp_sets+1)* sizeof(char*));
        if(!lrkp_strings){
                    LM_ERR("no pkg memory left\n");
            return -1;
        }
    }

    /*allocate for the current set of urls*/
    len = strlen(p);
    lrkp_strings[lrkp_sets] = (char*)pkg_malloc((len+1)*sizeof(char));

    if(!lrkp_strings[lrkp_sets]){
                LM_ERR("no pkg memory left\n");
        return -1;
    }

    memcpy(lrkp_strings[lrkp_sets], p, len);
    lrkp_strings[lrkp_sets][len] = '\0';
    lrkp_sets++;

    return 0;
}

struct lrkp_set *get_lrkp_set(str *const set_name)
{
    unsigned int this_set_id;
    struct lrkp_set *lrkp_list;
    if (lrkp_set_list == NULL)
    {
                LM_ERR("lrkp set list not configured\n");
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

    lrkp_list = select_lrkp_set(this_set_id);

    if(lrkp_list==NULL){	/*if a new id_set : add a new set of lrkp*/
        lrkp_list = shm_malloc(sizeof(struct lrkp_set));
        if(!lrkp_list){
                    LM_ERR("no shm memory left\n");
            return NULL;
        }
        memset(lrkp_list, 0, sizeof(struct lrkp_set));
        lrkp_list->id_set = this_set_id;
        if (lrkp_set_list->lset_first == NULL)
        {
            lrkp_set_list->lset_first = lrkp_list;
        } else {
            lrkp_set_list->lset_last->lset_next = lrkp_list;
        }
        lrkp_set_list->lset_last = lrkp_list;
        lrkp_set_count++;

        if (this_set_id == DEFAULT_LRKP_SET_ID)
        {
            default_lrkp_set = lrkp_list;
        }
    }
    return lrkp_list;
}

int insert_lrkp_node(struct lrkp_set *const lrkp_list, const str *const url, const int weight, const int enable)
{
    struct lrkp_node *pnode;

    if ((pnode = shm_malloc(sizeof(struct lrkp_node) + url->len + 1)) == NULL)
    {
                LM_ERR("out of shm memory\n");
        return -1;
    }

    memset(pnode, 0, sizeof(struct lrkp_node) + url->len + 1);


    struct lrkp_node_conf *node_conf;
    node_conf = shm_malloc(sizeof(struct lrkp_node_conf));
    if (!node_conf)
    {
                LM_ERR("out of shm memory\n");
        return -1;
    }

    memset(node_conf, 0, sizeof(struct lrkp_node_conf));
    pnode->lrkp_n_c = node_conf;

    pnode->idx = lrkp_no++;
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

    if (lrkp_list->ln_first == NULL)
    {
        lrkp_list->ln_first = pnode;
    } else {
        lrkp_list->ln_last->ln_next = pnode;
    }
    lrkp_list->ln_last = pnode;
    lrkp_list->lrkp_node_count++;

    return 0;
}

static int add_lrkproxy_socks(struct lrkp_set * lrkp_list,
                              char * lrkproxy){
    /* Make lrk proxies list. */
    char *p, *p1, *p2, *plim;
    int weight;
    str url;

    p = lrkproxy;
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
        insert_lrkp_node(lrkp_list, &url, weight, 0);
    }
    return 0;
}

/*	0-succes
 *  -1 - erorr
 * */
static int lrkproxy_add_lrkproxy_set( char * lrk_proxies)
{
    char *p,*p2;
    struct lrkp_set * lrkp_list;
    str id_set;

    /* empty definition? */
    p= lrk_proxies;
    if(!p || *p=='\0'){
        return 0;
    }

    for(;*p && isspace(*p);p++);
    if(*p=='\0'){
        return 0;
    }

    lrk_proxies = strstr(p, "==");
    if(lrk_proxies){
        if(*(lrk_proxies +2)=='\0'){
                    LM_ERR("script error -invalid lrk proxy list!\n");
            return -1;
        }

        *lrk_proxies = '\0';
        p2 = lrk_proxies-1;
        for(;isspace(*p2); *p2 = '\0',p2--);
        id_set.s = p;	id_set.len = p2 - p+1;

        if(id_set.len <= 0){
                    LM_ERR("script error -invalid set_id value!\n");
            return -1;
        }

        lrk_proxies+=2;
    }else{
        lrk_proxies = p;
        id_set = DEFAULT_LRKP_SET_ID_STR;
    }

    for(;*lrk_proxies && isspace(*lrk_proxies);lrk_proxies++);

    if(!(*lrk_proxies)){
                LM_ERR("script error -empty lrkproxy list\n");
        return -1;;
    }

    lrkp_list = get_lrkp_set(&id_set);
    if (lrkp_list == NULL)
    {
                LM_ERR("Failed to get or create lrkp_list for '%.*s'\n", id_set.len, id_set.s);
        return -1;
    }

    if(add_lrkproxy_socks(lrkp_list, lrk_proxies)!= 0){
        return -1;
    }

    return 0;
}


static int fixup_set_id(void ** param, int param_no)
{
	int int_val, err;
	struct lrkp_set* lrkp_list;
	lrkp_set_link_t *lrkl = NULL;
	str s;

	lrkl = (lrkp_set_link_t*)pkg_malloc(sizeof(lrkp_set_link_t));
	if(lrkl==NULL) {
		LM_ERR("no more pkg memory\n");
		return -1;
	}
	memset(lrkl, 0, sizeof(lrkp_set_link_t));
	s.s = (char*)*param;
	s.len = strlen(s.s);

	if(s.s[0] == PV_MARKER) {
		int_val = pv_locate_name(&s);
		if(int_val<0 || int_val!=s.len) {
			LM_ERR("invalid parameter %s\n", s.s);
			pkg_free(lrkl);
			return -1;
		}
		lrkl->rpv = pv_cache_get(&s);
		if(lrkl->rpv == NULL) {
			LM_ERR("invalid pv parameter %s\n", s.s);
			pkg_free(lrkl);
			return -1;
		}
	} else {
		int_val = str2s(*param, strlen(*param), &err);
		if (err == 0) {
			pkg_free(*param);
			if((lrkp_list = select_lrkp_set(int_val)) ==0){
				LM_ERR("lrkp_proxy set %i not configured\n", int_val);
				pkg_free(lrkl);
				return E_CFG;
			}
			lrkl->rset = lrkp_list;
		} else {
			LM_ERR("bad number <%s>\n",	(char *)(*param));
			pkg_free(lrkl);
			return E_CFG;
		}
	}
	*param = (void*)lrkl;
	return 0;
}


static int
mod_init(void)
{
    int i;
//	pv_spec_t avp_spec;
//	str s;
//	unsigned short avp_flags;

//	if(lrkproxy_rpc_init()<0)
//	{
//		LM_ERR("failed to register RPC commands\n");
//		return -1;
//	}

    /* Configure the head of the lrkp_set_list */
    lrkp_set_list = shm_malloc(sizeof(struct lrkp_set_head));
    if (lrkp_set_list == NULL)
    {
                LM_ERR("no shm memory for lrkp_set_list\n");
        return -1;
    }
    memset(lrkp_set_list, 0, sizeof(struct lrkp_set_head));


    /* storing the list of lrk proxy sets in shared memory*/
    for(i=0;i<lrkp_sets;i++){
                LM_DBG("Adding LRK-Proxy set %d/%d: %s\n", i, lrkp_sets, lrkp_strings[i]);
//        if ((lrkp_db_url.s == NULL) &&
        if (lrkproxy_add_lrkproxy_set(lrkp_strings[i]) != 0) {
            for(;i<lrkp_sets;i++)
                if(lrkp_strings[i])
                    pkg_free(lrkp_strings[i]);
            pkg_free(lrkp_strings);
                    LM_ERR("Failed to add LRK-Proxy from Config!\n");
            return -1;
        }
        if(lrkp_strings[i])
            pkg_free(lrkp_strings[i]);
    }


    if (lrkp_strings)
        pkg_free(lrkp_strings);


    /* init the hastable which keeps the all media address for both party and also the elected_node <--> callid& via-branch relation */
    if (hash_table_size < 1){
        hash_table_size = HASH_SIZE;    //the default size 128 entry.
    }

    if (!lrkproxy_hash_table_init(hash_table_size)) {
                LM_ERR("lrkproxy_hash_table_init(%d) failed!\n", hash_table_size);
        return -1;
    } else {
//                LM_DBG("lrkproxy_hash_table_init(%d) success!\n", hash_table_size);
                LM_INFO(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>lrkproxy_hash_table_init(%d) success!\n", hash_table_size);
    }



    /* load tm module*/
    if (load_tm_api( &tmb ) < 0)
    {
                LM_DBG("could not load the TM-functions - answer-offer model"
                               " auto-detection is disabled\n");
        memset(&tmb, 0, sizeof(struct tm_binds));
    }

    if (custom_sdp_ip_spec.s) {
        if (pv_parse_spec(&custom_sdp_ip_spec, &custom_sdp_ip_avp) == 0
            && (custom_sdp_ip_avp.type != PVT_AVP)) {
                    LM_ERR("malformed or non AVP custom_sdp_ip "
                                   "AVP definition in '%.*s'\n", custom_sdp_ip_spec.len,custom_sdp_ip_spec.s);
            return -1;
        }
    }

    init_custom_sdp_ip(custom_sdp_ip_spec.s ? &custom_sdp_ip_avp : 0);


    return 0;
}


static int
child_init(int rank)
{
    int n;
    char *cp;
    struct addrinfo hints, *res;
    struct lrkp_set  *lrkp_list;
    struct lrkp_node *pnode;

    if(lrkp_set_list==NULL )
        return 0;

    /* do not init sockets for PROC_INIT and main process when fork=yes */
    if(rank==PROC_INIT || (rank==PROC_MAIN && dont_fork==0)) {
        return 0;
    }

    /* Iterate known LRK proxies - create sockets */
    mypid = getpid();

    lrkp_socks = (int*)pkg_malloc( sizeof(int)*lrkp_no );
    if (lrkp_socks==NULL) {
                LM_ERR("no more pkg memory\n");
        return -1;
    }
    memset(lrkp_socks, -1, sizeof(int)*lrkp_no);

    for(lrkp_list = lrkp_set_list->lset_first; lrkp_list != 0;
        lrkp_list = lrkp_list->lset_next){

        for (pnode=lrkp_list->ln_first; pnode!=0; pnode = pnode->ln_next){
            char *hostname;

            if (pnode->ln_umode == 0) {
                lrkp_socks[pnode->idx] = -1;
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

            lrkp_socks[pnode->idx] = socket((pnode->ln_umode == 6)
                                            ? AF_INET6 : AF_INET, SOCK_DGRAM, 0);
            if ( lrkp_socks[pnode->idx] == -1) {
                        LM_ERR("can't create socket\n");
                freeaddrinfo(res);
                return -1;
            }

            if (connect( lrkp_socks[pnode->idx], res->ai_addr, res->ai_addrlen) == -1) {
                        LM_ERR("can't connect to a LRK proxy\n");
                close( lrkp_socks[pnode->idx] );
                lrkp_socks[pnode->idx] = -1;
                freeaddrinfo(res);
                return -1;
            }
            freeaddrinfo(res);
rptest:
            pnode->ln_enable = lrkp_test(pnode);
            if (pnode->ln_enable) {       //get lrk proxy config if it is enable.
//                LM_INFO("lrkp_test test is calling here\n"); //enable next line.
                lrkp_get_config(pnode);
            }
        }
    }

    return 0;
}


static void mod_destroy(void)
{
    struct lrkp_set * crt_list, * last_list;
    struct lrkp_node * crt_lrkp, *last_lrkp;

    /*free the shared memory*/
//	if (natping_state)
//		shm_free(natping_state);

    if(lrkp_set_list == NULL)
        return;

    for(crt_list = lrkp_set_list->lset_first; crt_list != NULL; ){

        for(crt_lrkp = crt_list->ln_first; crt_lrkp != NULL;  ){

            last_lrkp = crt_lrkp;
            crt_lrkp = last_lrkp->ln_next;
            shm_free(last_lrkp);
        }

        last_list = crt_list;
        crt_list = last_list->lset_next;
        shm_free(last_list);
    }

    shm_free(lrkp_set_list);

    /* destroy the hash table */
    if (!lrkproxy_hash_table_destroy()) {
                LM_ERR("lrkproxy_hash_table_destroy() failed!\n");
    } else {
                LM_DBG("lrkproxy_hash_table_destroy() success!\n");
    }

}


static char * gencookie(void)
{
    static char cook[34];

    sprintf(cook, "%d_%u ", (int)mypid, myseqn);
    myseqn++;
    return cook;
}

static int lrkp_test(struct lrkp_node *node)
{
    int buflen = 256;
    char buf[buflen];
    struct iovec v[2] = {{NULL, 0}, {"P", 1}};

    memset(buf, 0, buflen);
    memcpy(buf, send_lrkp_command(node, v, 2, 0), buflen);

//    if (buf == NULL) {
    if (!buf[0]) {
        LM_ERR("can't ping the lrk proxy %s, Disable it right now.\n", node->ln_url.s);
        return 0;
    }

    char *resp = buf + v[0].iov_len + v[1].iov_len + 1;
    if (memcmp(resp, "PONG", 4) == 0)
//                LM_DBG("Recieve PONG response from lrk proxy server %s, Enable it right now.\n", node->ln_url.s);
            LM_INFO("Recieve PONG response from lrk proxy server %s, Enable it right now.\n", node->ln_url.s);

    return 1;

}

static int lrkp_get_config(struct lrkp_node *node){

    int buflen = 256;
    char buf[buflen];
    struct iovec v[2] = {{NULL, 0}, {"G", 1}};
    struct lrkp_node_conf *lnconf = NULL;

    memset(buf, 0, buflen);
    memcpy(buf, send_lrkp_command(node, v, 2, 0), buflen);

//    if (buf == NULL) {
    if (!buf[0]) {
        LM_ERR("can't get config of the lrk proxy %s, Disable it right now.\n", node->ln_url.s);
        return 0;
    }

    lnconf = (struct lrkp_node_conf *)(buf + v[0].iov_len + v[1].iov_len + 1);

    if (lnconf == NULL){
        LM_ERR("can't get config of the lrk proxy %s, Disable it right now.\n", node->ln_url.s);
        return 0;
    }


    memcpy(node->lrkp_n_c, lnconf, sizeof(struct lrkp_node_conf));

//    node->lrkp_n_c = lnconf;
    LM_INFO("the lrk proxy %s is configured successfully right now.\n", node->ln_url.s);
    LM_INFO("buffer internal:%s\n", node->lrkp_n_c->internal_ip);
    LM_INFO("buffer external:%s\n", node->lrkp_n_c->external_ip);
    LM_INFO("buffer start_port:%d\n", node->lrkp_n_c->start_port);
    LM_INFO("buffer end_port:%d\n", node->lrkp_n_c->end_port);
    LM_INFO("buffer current_port:%d\n", node->lrkp_n_c->current_port);

    return 1;


}

static int lrkp_set_conntrack_rule(struct lrkproxy_hash_entry *e) {
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
    char callid[50];

    struct iovec v[] = {
            {NULL, 0},  /* reserved (cookie) */
            {"S",  1},   /* command & common options */
            {NULL, 0},  /* src_ipv4 */
            {NULL, 0},  /* dst_ipnv4 */
            {NULL, 0},  /* snat_ipv4 */
            {NULL, 0},  /* dnat_ipv4 */
            {NULL, 0},  /* src_port */
            {NULL, 0},  /* dst_port*/
            {NULL, 0},  /* snat_port */
            {NULL, 0},  /* dnat_port*/
            {NULL, 0},  /* timeout to clear conntrack entry*/
            {NULL, 0},  /* callid of session */
    };

    v_len += v[1].iov_len;

    //set src_ipv4 to buffer.
    sprintf(src_ipv4, " %.*s ", e->src_ipv4.len, e->src_ipv4.s);
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

    //set callid to buffer.
    sprintf(callid, "%.*s ", e->callid.len, e->callid.s);
    v[11].iov_base = callid;
    v[11].iov_len = strlen(v[11].iov_base);
    v_len += v[11].iov_len;
//    LM_ERR("e->callid.len is:%d right now.\n\n", e->callid.len);

    memset(buf, 0, buflen);
    memcpy(buf, send_lrkp_command(e->node, v, 12, v_len), buflen);
//

//    if (buf == NULL) {
    if (!buf[0]) {
                LM_ERR("can't ping the lrk proxy %s, Disable it right now.\n", e->node->ln_url.s);
        return 0;
    }

    v_len += v[0].iov_len;


//    char *resp = buf + v[0].iov_len + v[1].iov_len + v[2].iov_len;
    char *resp = buf + v_len;
    if (memcmp(resp, "OK", 2) == 0) {
                LM_INFO("Recieve OK response from lrk proxy server %s, Rule set successfully.\n", e->node->ln_url.s);
                LM_DBG("Recieve OK response from lrk proxy server %s, Rule set successfully.\n", e->node->ln_url.s);
    }
    return 1;

}


char *send_lrkp_command(struct lrkp_node *node, struct iovec *v, int vcnt, int more)
{
    struct sockaddr_un addr;
    int fd, len, i;
    int ret;
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
                    LM_ERR("can't connect to lrk proxy\n");
            goto badproxy;
        }

        do {
            len = writev(fd, v + 1, vcnt - 1);
        } while (len == -1 && errno == EINTR);
        if (len <= 0) {
            close(fd);
                    LM_ERR("can't send command to a lrk proxy %s\n", node->ln_url.s);
            goto badproxy;
        }
        do {
            len = read(fd, buf, sizeof(buf) - 1);
        } while (len == -1 && errno == EINTR);
        close(fd);
        if (len <= 0) {
                    LM_ERR("can't read reply from a lrk proxy %s\n", node->ln_url.s);
            goto badproxy;
        }
    } else {
        fds[0].fd = lrkp_socks[node->idx];
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        /* Drain input buffer */
        ret = -1;
        while ((poll(fds, 1, 0) == 1) &&
               ((fds[0].revents & POLLIN) != 0) && ret != 0) {
            ret = recv(lrkp_socks[node->idx], buf, sizeof(buf) - 1, 0);
            fds[0].revents = 0;
        }
        v[0].iov_base = gencookie();
        v[0].iov_len = strlen(v[0].iov_base);
        for (i = 0; i < lrkproxy_retr; i++) {
            do {
                len = writev(lrkp_socks[node->idx], v, vcnt);
            } while (len == -1 && (errno == EINTR || errno == ENOBUFS));
            if (len <= 0) {
                        LM_ERR("can't send command to a lrk proxy %s\n", node->ln_url.s);
                goto badproxy;
            }
            while ((poll(fds, 1, lrkproxy_tout * 1000) == 1) &&
                   (fds[0].revents & POLLIN) != 0) {
                do {
                    len = recv(lrkp_socks[node->idx], buf, sizeof(buf) - 1, 0);
                } while (len == -1 && errno == EINTR);
                if (len <= 0) {
                            LM_ERR("can't read reply from a lrk proxy %s\n", node->ln_url.s);
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
                            if (len == v[0].iov_len + v[1].iov_len + sizeof(struct lrkp_node_conf) + 1)
                                goto out;
//                            break;
                        case 'S':
                            if (len == more + v[0].iov_len + 2)
                                goto out;
//                            break;
                    }

                }
                fds[0].revents = 0;
            }
        }
        if (i == lrkproxy_tout) {
                    LM_ERR("timeout waiting reply from a lrk proxy server %s\n", node->ln_url.s);
            goto badproxy;

        }
    }
    out:
    return buf;
    badproxy:
            LM_ERR("lrk proxy <%s> does not respond, disable it\n", node->ln_url.s);
    node->ln_enable = 0;
//    node->ln_recheck_ticks = get_ticks() + lrkproxy_disable_tout;
    return buf;
}

/*
 * select the set with the id_set id
 */
static struct lrkp_set * select_lrkp_set(int id_set ){

    struct lrkp_set * lrkp_list;
    /*is it a valid set_id?*/

    if(!lrkp_set_list)
    {
                LM_ERR("lrkproxy set list not initialised\n");
        return NULL;
    }

    for(lrkp_list=lrkp_set_list->lset_first; lrkp_list!=NULL &&
                                             lrkp_list->id_set!=id_set; lrkp_list=lrkp_list->lset_next);

    return lrkp_list;
}


struct lrkp_node *select_lrkp_node(int do_test)
{
    struct lrkp_node* node;
    int was_forced;
    int was_forced2;

    if(!selected_lrkp_set){
        LM_ERR("script error -no valid set selected\n");
        return NULL;
    }
    /* Most popular case: 1 proxy, nothing to calculate */
    if (selected_lrkp_set->lrkp_node_count == 1) {
        node = selected_lrkp_set->ln_first;
//        if (node->rn_disabled && node->rn_recheck_ticks <= get_ticks())
        if (!node->ln_enable) {
            node->ln_enable = lrkp_test(node);
            if (node->ln_enable) {       //get lrk proxy config if it is enable.
                lrkp_get_config(node);
                return node;
            }
        }
        return node->ln_enable ? node : NULL;
//        return NULL;
    }


    /* Check node is enable and test it again*/
retry:
    for (node=selected_lrkp_set->ln_first; node!=NULL; node=node->ln_next) {

        if (!node->ln_enable) {
            /* Try to enable if it's time to try. */
            node->ln_enable = lrkp_test(node);
            if (node->ln_enable)       //get lrk proxy config if it is enable.
                lrkp_get_config(node);
        }
    }

    if (lrkp_algorithm == LRK_LINER) {
        was_forced = 0;
        retry2:
        for (node = selected_lrkp_set->ln_first; node != NULL; node = node->ln_next)
            if (node->ln_enable)
                goto found;
        if (was_forced)
            return NULL;

        was_forced = 1;
        //trying to enable all lrkproxy and check again.
        for (node = selected_lrkp_set->ln_first; node != NULL; node = node->ln_next) {
            /* Try to enable if it's time to try. */
            node->ln_enable = lrkp_test(node);
            if (node->ln_enable)       //get lrk proxy config if it is enable.
                lrkp_get_config(node);
        }


        goto retry2;
    }
    else if(lrkp_algorithm == LRK_RR) {
        was_forced2 = 0;
retry3:
        if (!selected_lrkp_node) {
            selected_lrkp_node = selected_lrkp_set->ln_first;
            node = selected_lrkp_set->ln_first;
            if(node->ln_enable)
                    goto found;
//            was_forced2 = 1;
        }
        for (node = selected_lrkp_node->ln_next; node != NULL; node = node->ln_next)
//        for (node = selected_lrkp_node; node != NULL; node = node->ln_next) {
            if (node->ln_enable) {
                selected_lrkp_node = node;
                goto found;
            }
//            selected_lrkp_node = node->ln_next;
//        if (sumcut < node->ln_weight)
//        sumcut -= node->ln_weight;

        if (was_forced2)
            return NULL;

        was_forced2 = 1;
        selected_lrkp_node = NULL;

        goto retry3;
    }

    found:
    if (do_test) {
//    //todo...
        node->ln_enable = lrkp_test(node);
        if (!node->ln_enable)
            goto retry;
    }
    return node;
}

//static int change_media_sdp(sip_msg_t *msg, struct lrkp_node *n, const char *flags, int type) {
static int change_media_sdp(sip_msg_t *msg, struct lrkproxy_hash_entry *e, const char *flags, enum lrk_operation op) {
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

    //The external_ip should be set in config file for relaying RTP media between NIC.
//    if (e->node->lrkp_n_c->external_ip && flags) {
    if(flags) {
        if (strstr(flags, "ei")) {
            ip_selected = e->node->lrkp_n_c->internal_ip;// lrk_node->internal_ip;
        } else if (strstr(flags, "ie")) {
            ip_selected = e->node->lrkp_n_c->external_ip; //lrk_node->external_ip;
        } else {
            LM_INFO("unknown flags, use internal_ip\n");
            ip_selected = e->node->lrkp_n_c->internal_ip;
        }
    }
    else {
        LM_INFO("no flags set, use internal_ip\n");
        ip_selected = e->node->lrkp_n_c->internal_ip;
    }

    if (op == OP_OFFER) {
        e->dst_ipv4.s = ip_selected;
        e->dst_ipv4.len = strlen(e->dst_ipv4.s);

        str current_port;
        current_port.s = int2str(e->node->lrkp_n_c->current_port, &current_port.len);

        if (shm_str_dup(&e->dst_port, &current_port) < 0) {
                    LM_ERR("lrkproxy fail to insert dst_port, calllen=%d dst_port=%.*s\n",
                           e->callid.len, current_port.len, current_port.s);
            lrkproxy_hash_table_free_entry(e);
            return 0;
        }

//        e->dst_port = e->node->lrkp_n_c->current_port;
    }
    else if (op == OP_ANSWER){
        e->snat_ipv4.s = ip_selected;
        e->snat_ipv4.len = strlen(e->snat_ipv4.s);

        str current_port;
        unsigned int snat_port = 0;

        str2int(&e->dst_port, &snat_port);

        /*check if gt is enable or not*/
        if (!gt)
            snat_port += 2;

        current_port.s = int2str(snat_port, &current_port.len);

        if (shm_str_dup(&e->snat_port, &current_port) < 0) {
                    LM_ERR("lrkproxy fail to insert snat_port, calllen=%d snat_port=%.*s\n",
                           e->callid.len, current_port.len, current_port.s);
            lrkproxy_hash_table_free_entry(e);
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
            snprintf(sdp_new_o, 128, "o=lrkproxy %s %s IN IP4 %s\r", SUP_CPROTOVER, REQ_CPROTOVER, ip_selected);
            strncat(newbody.s, sdp_new_o, strlen(sdp_new_o));
            off += len + 1;
            continue;
        }
        if ((int) (start_sdp_s - off) == 0) {
            memset(sdp_new_s, 0, 128);
            snprintf(sdp_new_s, 128, "s=lrkproxy Support only Audio Call\r");
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
            if (op == OP_OFFER)
                snprintf(sdp_new_m, 128, "m=audio %.*s %.*s\r",e->dst_port.len, e->dst_port.s, (int)(len - (avp_flags-off)), avp_flags);
//                snprintf(sdp_new_m, 128, "m=audio %d %.*s\r",e->node->lrkp_n_c->current_port, (int)(len - (avp_flags-off)), avp_flags);
            else if (op == OP_ANSWER)
                snprintf(sdp_new_m, 128, "m=audio %.*s %.*s\r",e->snat_port.len, e->snat_port.s, (int)(len - (avp_flags-off)), avp_flags);
//               snprintf(sdp_new_m, 128, "m=audio %d %.*s\r",e->node->lrkp_n_c->current_port, (int)(len - (avp_flags-off)), avp_flags);
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

/* This function assumes p points to a line of requested type. */

	static int
set_lrkproxy_set_f(struct sip_msg * msg, char * str1, char * str2)
{
	lrkp_set_link_t *lrkl;
	pv_value_t val;

	lrkl = (lrkp_set_link_t*)str1;

	current_msg_id = 0;
	selected_lrkp_set = 0;

	if(lrkl->rset != NULL) {
		current_msg_id = msg->id;
		selected_lrkp_set = lrkl->rset;
	} else {
		if(pv_get_spec_value(msg, lrkl->rpv, &val)<0) {
			LM_ERR("cannot evaluate pv param\n");
			return -1;
		}
		if(!(val.flags & PV_VAL_INT)) {
			LM_ERR("pv param must hold an integer value\n");
			return -1;
		}
		selected_lrkp_set = select_lrkp_set(val.ri);
		if(selected_lrkp_set==NULL) {
			LM_ERR("could not locate lrkproxy set %d\n", val.ri);
			return -1;
		}
		current_msg_id = msg->id;
	}
	return 1;
}

static int
lrkproxy_manage(struct sip_msg *msg, char *flags, char *ip)
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
        return lrkproxy_unforce(msg, flags, OP_DELETE, 1);

    if (msg->msg_flags & FL_SDP_BODY)
        nosdp = 0;
    else
        nosdp = parse_sdp(msg);

    if (msg->first_line.type == SIP_REQUEST) {
        if(method==METHOD_ACK && nosdp==0)
            return lrkproxy_force(msg, flags, OP_ANSWER, 1);
        if(method==METHOD_UPDATE && nosdp==0)
            return lrkproxy_force(msg, flags, OP_OFFER, 1);
        if(method==METHOD_INVITE && nosdp==0) {
            msg->msg_flags |= FL_SDP_BODY;
            if(tmb.t_gett!=NULL) {
                t = tmb.t_gett();
                if(t!=NULL && t!=T_UNDEFINED && t->uas.request!=NULL) {
                    t->uas.request->msg_flags |= FL_SDP_BODY;
                }
            }
            if(route_type==FAILURE_ROUTE)
                return lrkproxy_unforce(msg, flags, OP_DELETE, 1);
            return lrkproxy_force(msg, flags, OP_OFFER, 1);
        }
    } else if (msg->first_line.type == SIP_REPLY) {
        if (msg->first_line.u.reply.statuscode>=300)
            return lrkproxy_unforce(msg, flags, OP_DELETE, 2);
        if (nosdp==0) {
            if (method==METHOD_UPDATE)
                return lrkproxy_force(msg, flags, OP_ANSWER, 2);
            if (tmb.t_gett==NULL || tmb.t_gett()==NULL
                || tmb.t_gett()==T_UNDEFINED)
                return lrkproxy_force(msg, flags, OP_ANSWER, 2);
            if (tmb.t_gett()->uas.request->msg_flags & FL_SDP_BODY)
                return lrkproxy_force(msg, flags, OP_ANSWER, 2);
            return lrkproxy_force(msg, flags, OP_OFFER, 2);
        }
    }

    return -1;
}

static int
lrkproxy_manage0(struct sip_msg *msg, char *flags, char *ip)
{
    return lrkproxy_manage(msg, 0, 0);
}

static int
lrkproxy_manage1(struct sip_msg *msg, char *flags, char *ip)
{
    str flag_str;
    if(fixup_get_svalue(msg, (gparam_p)flags, &flag_str)<0)
    {
                LM_ERR("invalid flags parameter\n");
        return -1;
    }
    return lrkproxy_manage(msg, flag_str.s, 0);
}

static int
lrkproxy_manage2(struct sip_msg *msg, char *flags, char *ip)
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
    return lrkproxy_manage(msg, flag_str.s, ip_str.s);
}


static int lrkproxy_force(struct sip_msg *msg, const char *flags, enum lrk_operation op, int more) {

//    lrk_sdp_info_t lrk_sdp_info;
    struct lrkproxy_hash_entry *entry = NULL;
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

        //select new node based on lrkp_algorithm param.
        struct lrkp_node *node = select_lrkp_node(more_test);
        if (!node) {
                    LM_ERR("can't ping any lrk proxy right now.\n");
            return -1;
        }

                LM_DBG("selected lrk proxy node: %s\n", node->ln_url.s);

        //check if entry not exist.
        entry = lrkproxy_hash_table_lookup(call_id, viabranch);
        if (entry)
            return -1;

//        lrk_get_sdp_info(msg, &lrk_sdp_info);

        //build new entry for hash table.
//            struct lrkproxy_hash_entry *entry = shm_malloc(sizeof(struct lrkproxy_hash_entry));
        entry = shm_malloc(sizeof(struct lrkproxy_hash_entry));
        if (!entry) {
                    LM_ERR("lrkproxy hash table fail to create entry for calllen=%d callid=%.*s viabranch=%.*s\n",
                           call_id.len, call_id.len, call_id.s,
                           viabranch.len, viabranch.s);
            return 0;
        }
        memset(entry, 0, sizeof(struct lrkproxy_hash_entry));

        // fill the entry
        if (call_id.s && call_id.len > 0) {
            if (shm_str_dup(&entry->callid, &call_id) < 0) {
                        LM_ERR("lrkproxy hash table fail to instert call_id, calllen=%d callid=%.*s\n",
                               call_id.len, call_id.len, call_id.s);
                lrkproxy_hash_table_free_entry(entry);
                return 0;
            }
        }

        if (viabranch.s && viabranch.len > 0) {
            if (shm_str_dup(&entry->viabranch, &viabranch) < 0) {
                        LM_ERR("lrkproxy hash table fail to insert viabranch, calllen=%d viabranch=%.*s\n",
                               call_id.len, viabranch.len, viabranch.s);
                lrkproxy_hash_table_free_entry(entry);
                return 0;
            }
        }

        //fill src_ipv4 and src_port for entry.
        str src_ipv4;

        if (get_sdp_ipaddr_media(msg, &src_ipv4) == -1) {
                    LM_ERR("can't get media src_ipv4 from sdp field\n");
            return -1;
        }

        if (src_ipv4.s && src_ipv4.len > 0) {
                    LM_DBG("src_ipv4 from sdp:%.*s\n", src_ipv4.len, src_ipv4.s);
            if (shm_str_dup(&entry->src_ipv4, &src_ipv4) < 0) {
                        LM_ERR("lrkproxy hash table fail to insert src_ipv4, calllen=%d src_ipv4=%.*s\n",
                               call_id.len, src_ipv4.len, src_ipv4.s);
                lrkproxy_hash_table_free_entry(entry);
                return 0;
            }
        }

        str src_port;
        if (get_sdp_port_media(msg, &src_port) == -1) {
                    LM_ERR("can't get media src_port from sdp field\n");
            return -1;
        }


        if (src_port.s && src_port.len > 0) {
                    LM_DBG("src_port from sdp:%.*s\n", src_port.len, src_port.s);
            if (shm_str_dup(&entry->src_port, &src_port) < 0) {
                        LM_ERR("lrkproxy hash table fail to insert src_port, calllen=%d src_port=%.*s\n",
                               call_id.len, src_port.len, src_port.s);
                lrkproxy_hash_table_free_entry(entry);
                return 0;
            }
        }

//            entry->
        entry->node = node;
        entry->next = NULL;
        entry->tout = get_ticks() + hash_table_tout;

        // insert the key<->entry from the hashtable
        if (!lrkproxy_hash_table_insert(call_id, viabranch, entry)) {
                    LM_ERR(
                    "lrkproxy hash table fail to insert node=%.*s for calllen=%d callid=%.*s viabranch=%.*s\n",
                    node->ln_url.len, node->ln_url.s, call_id.len,
                    call_id.len, call_id.s, viabranch.len, viabranch.s);
            lrkproxy_hash_table_free_entry(entry);
            return 0;
        } else {
                    LM_INFO("lrkproxy hash table insert node=%.*s for calllen=%d callid=%.*s viabranch=%.*s\n",
                            node->ln_url.len, node->ln_url.s, call_id.len,
                            call_id.len, call_id.s, viabranch.len, viabranch.s);

                    LM_DBG("lrkproxy hash table insert node=%.*s for calllen=%d callid=%.*s viabranch=%.*s\n",
                           node->ln_url.len, node->ln_url.s, call_id.len,
                           call_id.len, call_id.s, viabranch.len, viabranch.s);
        }


        if (flags)
            change_media_sdp(msg, entry, flags, op);
        else
            change_media_sdp(msg, entry, NULL, op);

        if (node->lrkp_n_c->current_port >= node->lrkp_n_c->end_port)
            node->lrkp_n_c->current_port = node->lrkp_n_c->start_port;
        else {
            if (gt)
                node->lrkp_n_c->current_port += 2;
            else
                node->lrkp_n_c->current_port += 4;
        }
    } else if (op == OP_ANSWER) {
                LM_INFO ("Here is SIP_REPLY of METHOD_INVITE\n");


        entry = lrkproxy_hash_table_lookup(call_id, viabranch);
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
                        LM_ERR("lrkproxy hash table fail to insert dnat_ipv4, calllen=%d dnat_ipv4=%.*s\n",
                               call_id.len, dnat_ipv4.len, dnat_ipv4.s);
                lrkproxy_hash_table_free_entry(entry);
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
                        LM_ERR("lrkproxy hash table fail to insert dnat_port, calllen=%d dnat_port=%.*s\n",
                               call_id.len, dnat_port.len, dnat_port.s);
                lrkproxy_hash_table_free_entry(entry);
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


        lrkp_set_conntrack_rule(entry);

    }
    return 1;
}

static int lrkproxy_unforce(struct sip_msg *msg, const char *flags, enum lrk_operation op, int more){
//            LM_INFO ("Here is lrkproxy_unforce\n");
//    struct lrkproxy_hash_entry *entry = NULL;
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
        if (!lrkproxy_hash_table_remove(call_id, viabranch, op)) {
                    LM_ERR("lrkproxy hash table failed to remove entry for callen=%d callid=%.*s viabranch=%.*s\n",
                           call_id.len, call_id.len, call_id.s,
                           viabranch.len, viabranch.s);
        } else {
                    LM_DBG("lrkproxy hash table remove entry for callen=%d callid=%.*s viabranch=%.*s\n",
                           call_id.len, call_id.len, call_id.s,
                           viabranch.len, viabranch.s);
        }
    }
    LM_INFO("lrkproxy hash table remove entry for callen=%d callid=%.*s viabranch=%.*s successfully\n",
            call_id.len, call_id.len, call_id.s,
            viabranch.len, viabranch.s);
    return 1;
}
