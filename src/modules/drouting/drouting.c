/*
 * Copyright (C) 2005-2009 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "stdlib.h"
#include "stdio.h"
#include "assert.h"
#include <unistd.h>

#include "../../core/sr_module.h"
#include "../../core/str.h"
#include "../../core/dprint.h"
#include "../../core/usr_avp.h"
#include "../../lib/srdb1/db.h"
#include "../../core/mem/mem.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/locking.h"
#include "../../core/action.h"
#include "../../core/error.h"
#include "../../core/ut.h"
#include "../../core/resolve.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/dset.h"
#include "../../core/mod_fix.h"
#include "../../core/kemi.h"
#include "../../core/rpc_lookup.h"
#include "../../core/rand/kam_rand.h"

#include "../keepalive/api.h"

#include "dr_load.h"
#include "prefix_tree.h"
#include "routing.h"


/*** DB relatede stuff ***/
/* parameters  */
static str db_url           = str_init(DEFAULT_RODB_URL);
static str drg_table = str_init("dr_groups");
static str drd_table = str_init("dr_gateways");
static str drr_table = str_init("dr_rules");
static str drl_table = str_init("dr_gw_lists");
/* DRG use domain */
static int use_domain = 1;
/**
 * - 0 - normal order
 * - 1 - random order, full set
 * - 2 - random order, one per set
 */
static int sort_order = 0;
int dr_fetch_rows = 1000;
int dr_force_dns = 1;
/* enable destinations keepalive (through keepalive module */
int dr_enable_keepalive = 0;
keepalive_api_t keepalive_api;

/* DRG table columns */
static str drg_user_col = str_init("username");
static str drg_domain_col = str_init("domain");
static str drg_grpid_col = str_init("groupid");
/* variables */
static db1_con_t *db_hdl = 0; /* DB handler */
static db_func_t dr_dbf;	  /* DB functions */

/* current dr data - pointer to a pointer in shm */
static rt_data_t **rdata = 0;

/* AVP used to store serial RURIs */
static struct _ruri_avp
{
	unsigned short type; /* AVP ID */
	int_str name;		 /* AVP name*/
} ruri_avp = {0, {.n = (int)0xad346b2f}};
static str ruri_avp_spec = {0, 0};

/* AVP used to store serial ATTRs */
static struct _attrs_avp
{
	unsigned short type; /* AVP ID */
	int_str name;		 /* AVP name*/
} attrs_avp = {0, {.n = (int)0xad346b30}};
static str attrs_avp_spec = {0, 0};

/* statistic data */
int tree_size = 0;
int inode = 0;
int unode = 0;

/* lock, ref counter and flag used for reloading the date */
static gen_lock_t *ref_lock = 0;
static int *data_refcnt = 0;
static int *reload_flag = 0;

static int dr_init(void);
static int dr_child_init(int rank);
static void dr_exit(void);

static int do_routing(struct sip_msg *msg, int grp_id);
static int do_routing_0(struct sip_msg *msg, char *str1, char *str2);
static int do_routing_1(struct sip_msg *msg, char *str1, char *str2);
static int use_next_gw(struct sip_msg *msg, char *p1, char *p2);
static int is_from_gw_0(struct sip_msg *msg, char *str1, char *str2);
static int is_from_gw_1(struct sip_msg *msg, char *str1, char *str2);
static int is_from_gw_2(struct sip_msg *msg, char *str1, char *str2);
static int goes_to_gw_0(struct sip_msg *msg, char *f1, char *f2);
static int goes_to_gw_1(struct sip_msg *msg, char *f1, char *f2);

MODULE_VERSION

/* clang-format off */
/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"do_routing", (cmd_function)do_routing_0, 0, 0,
			  0, REQUEST_ROUTE | FAILURE_ROUTE},
	{"do_routing", (cmd_function)do_routing_1, 1, fixup_igp_null, 0,
			REQUEST_ROUTE | FAILURE_ROUTE},
	{"use_next_gw", (cmd_function)use_next_gw, 0, 0, 0,
			REQUEST_ROUTE | FAILURE_ROUTE},
	{"next_routing", (cmd_function)use_next_gw, 0, 0, 0, FAILURE_ROUTE},
	{"is_from_gw", (cmd_function)is_from_gw_0, 0, 0, 0,
			REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
	{"is_from_gw", (cmd_function)is_from_gw_1, 1, fixup_igp_null, 0,
			REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
	{"is_from_gw", (cmd_function)is_from_gw_2, 2, fixup_igp_igp, 0,
			REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
	{"goes_to_gw", (cmd_function)goes_to_gw_0, 0, 0, 0,
			REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
	{"goes_to_gw", (cmd_function)goes_to_gw_1, 1, fixup_igp_null, 0,
			REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"db_url", PARAM_STR, &db_url},
	{"drd_table", PARAM_STR, &drd_table},
	{"drr_table", PARAM_STR, &drr_table},
	{"drg_table", PARAM_STR, &drg_table},
	{"drl_table", PARAM_STR, &drl_table},
	{"use_domain", INT_PARAM, &use_domain},
	{"drg_user_col", PARAM_STR, &drg_user_col},
	{"drg_domain_col", PARAM_STR, &drg_domain_col},
	{"drg_grpid_col", PARAM_STR, &drg_grpid_col},
	{"ruri_avp", PARAM_STR, &ruri_avp_spec},
	{"attrs_avp", PARAM_STR, &attrs_avp_spec},
	{"sort_order", INT_PARAM, &sort_order},
	{"fetch_rows", INT_PARAM, &dr_fetch_rows},
	{"force_dns", INT_PARAM, &dr_force_dns},
	{"enable_keepalive", INT_PARAM, &dr_enable_keepalive},
	{0, 0, 0}
};

static rpc_export_t rpc_methods[];

struct module_exports exports = {
	"drouting",			/* module name */
	DEFAULT_DLFLAGS,	/* dlopen flags */
	cmds,				/* exported functions */
	params,				/* exported parameters */
	0,					/* RPC method exports */
	0,					/* exported pseudo-variables */
	0,					/* response handling function */
	dr_init,			/* module initialization function */
	dr_child_init,		/* per-child init function */
	dr_exit				/* module destroy function */
};
/* clang-format on */

/**
 * Rewrite Request-URI
 */
static inline int rewrite_ruri(struct sip_msg *_m, char *_s)
{
	struct action act;
	struct run_act_ctx ra_ctx;

	memset(&act, '\0', sizeof(act));
	act.type = SET_URI_T;
	act.val[0].type = STRING_ST;
	act.val[0].u.string = _s;
	init_run_actions_ctx(&ra_ctx);
	if(do_action(&ra_ctx, &act, _m) < 0) {
		LM_ERR("do_action failed\n");
		return -1;
	}
	return 0;
}


void dr_keepalive_statechanged(str *uri, ka_state state, void *user_attr)
{

	((pgw_t *)user_attr)->state = state;
}

static int dr_update_keepalive(pgw_t *addrs)
{
	pgw_t *cur;
	str owner = str_init("drouting");

	for(cur = addrs; cur != NULL; cur = cur->next) {
		LM_DBG("uri: %.*s\n", cur->ip.len, cur->ip.s);
		keepalive_api.add_destination(
				&cur->ip, &owner, 0, dr_keepalive_statechanged, cur);
	}

	return 0;
}

static inline int dr_reload_data(void)
{
	rt_data_t *new_data;
	rt_data_t *old_data;

	new_data = dr_load_routing_info(
			&dr_dbf, db_hdl, &drd_table, &drl_table, &drr_table);
	if(new_data == 0) {
		LM_CRIT("failed to load routing info\n");
		return -1;
	}

	/* block access to data for all readers */
	lock_get(ref_lock);
	*reload_flag = 1;
	lock_release(ref_lock);

	/* wait for all readers to finish - it's a kind of busy waitting but
	 * it's not critical;
	 * at this point, data_refcnt can only be decremented */
	while(*data_refcnt) {
		usleep(10);
	}

	/* no more activ readers -> do the swapping */
	old_data = *rdata;
	*rdata = new_data;

	/* release the readers */
	*reload_flag = 0;

	/* destroy old data */
	if(old_data)
		free_rt_data(old_data, 1);

	if(dr_enable_keepalive) {
		dr_update_keepalive((*rdata)->pgw_l);
	}

	return 0;
}


static int dr_init(void)
{
	pv_spec_t avp_spec;

	LM_INFO("DRouting - initializing\n");

	if(rpc_register_array(rpc_methods) != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	/* check the module params */
	if(db_url.s == NULL || db_url.len <= 0) {
		LM_CRIT("mandatory parameter \"DB_URL\" found empty\n");
		goto error;
	}

	if(drd_table.len <= 0) {
		LM_CRIT("mandatory parameter \"DRD_TABLE\" found empty\n");
		goto error;
	}

	if(drr_table.len <= 0) {
		LM_CRIT("mandatory parameter \"DRR_TABLE\" found empty\n");
		goto error;
	}

	if(drg_table.len <= 0) {
		LM_CRIT("mandatory parameter \"DRG_TABLE\"  found empty\n");
		goto error;
	}

	if(drl_table.len <= 0) {
		LM_CRIT("mandatory parameter \"DRL_TABLE\"  found empty\n");
		goto error;
	}

	/* fix AVP spec */
	if(ruri_avp_spec.s) {
		if(pv_parse_spec(&ruri_avp_spec, &avp_spec) == 0
				|| avp_spec.type != PVT_AVP) {
			LM_ERR("malformed or non AVP [%.*s] for RURI AVP definition\n",
					ruri_avp_spec.len, ruri_avp_spec.s);
			return E_CFG;
		}

		if(pv_get_avp_name(
				   0, &(avp_spec.pvp), &(ruri_avp.name), &(ruri_avp.type))
				!= 0) {
			LM_ERR("[%.*s]- invalid AVP definition for RURI AVP\n",
					ruri_avp_spec.len, ruri_avp_spec.s);
			return E_CFG;
		}
	}
	if(attrs_avp_spec.s) {
		if(pv_parse_spec(&attrs_avp_spec, &avp_spec) == 0
				|| avp_spec.type != PVT_AVP) {
			LM_ERR("malformed or non AVP [%.*s] for ATTRS AVP definition\n",
					attrs_avp_spec.len, attrs_avp_spec.s);
			return E_CFG;
		}

		if(pv_get_avp_name(
				   0, &(avp_spec.pvp), &(attrs_avp.name), &(attrs_avp.type))
				!= 0) {
			LM_ERR("[%.*s]- invalid AVP definition for ATTRS AVP\n",
					attrs_avp_spec.len, attrs_avp_spec.s);
			return E_CFG;
		}
	}

	/* data pointer in shm */
	rdata = (rt_data_t **)shm_malloc(sizeof(rt_data_t *));
	if(rdata == 0) {
		LM_CRIT("failed to get shm mem for data ptr\n");
		goto error;
	}
	*rdata = 0;

	/* create & init lock */
	if((ref_lock = lock_alloc()) == 0) {
		LM_CRIT("failed to alloc ref_lock\n");
		goto error;
	}
	if(lock_init(ref_lock) == 0) {
		LM_CRIT("failed to init ref_lock\n");
		goto error;
	}
	data_refcnt = (int *)shm_malloc(sizeof(int));
	reload_flag = (int *)shm_malloc(sizeof(int));
	if(!data_refcnt || !reload_flag) {
		LM_ERR("no more shared memory\n");
		goto error;
	}
	*data_refcnt = 0;
	*reload_flag = 0;

	/* bind to the mysql module */
	if(db_bind_mod(&db_url, &dr_dbf)) {
		LM_CRIT("cannot bind to database module! "
				"Did you forget to load a database module ?\n");
		goto error;
	}

	if(!DB_CAPABILITY(dr_dbf, DB_CAP_QUERY)) {
		LM_CRIT("database modules does not "
				"provide QUERY functions needed by DRounting module\n");
		return -1;
	}

	if(dr_enable_keepalive) {
		LM_DBG("keepalive enabled - try loading keepalive module API\n");

		if(keepalive_load_api(&keepalive_api) < 0) {
			LM_ERR("failed to load keepalive API\n");
			goto error;
		}
	}

	return 0;
error:
	if(ref_lock) {
		lock_destroy(ref_lock);
		lock_dealloc(ref_lock);
		ref_lock = 0;
	}
	if(db_hdl) {
		dr_dbf.close(db_hdl);
		db_hdl = 0;
	}
	if(rdata) {
		shm_free(rdata);
		rdata = 0;
	}
	return -1;
}


static int dr_child_init(int rank)
{
	/* only workers needs DB connection */
	if(rank == PROC_MAIN || rank == PROC_TCP_MAIN || rank == PROC_INIT)
		return 0;

	/* init DB connection */
	if((db_hdl = dr_dbf.init(&db_url)) == 0) {
		LM_CRIT("cannot initialize database connection\n");
		return -1;
	}

	/* child 1 load the routing info */
	if((rank == 1) && dr_reload_data() != 0) {
		LM_CRIT("failed to load routing data\n");
		return -1;
	}

	/* set GROUP table for workers */
	if(dr_dbf.use_table(db_hdl, &drg_table) < 0) {
		LM_ERR("cannot select table \"%.*s\"\n", drg_table.len, drg_table.s);
		return -1;
	}
	return 0;
}


static void dr_exit(void)
{
	/* close DB connection */
	if(db_hdl) {
		dr_dbf.close(db_hdl);
		db_hdl = 0;
	}

	/* destroy data */
	if(rdata) {
		if(*rdata)
			free_rt_data(*rdata, 1);
		shm_free(rdata);
		rdata = 0;
	}

	/* destroy lock */
	if(ref_lock) {
		lock_destroy(ref_lock);
		lock_dealloc(ref_lock);
		ref_lock = 0;
	}

	if(reload_flag)
		shm_free(reload_flag);
	if(data_refcnt)
		shm_free(data_refcnt);

	return;
}


/* rpc function documentation */
static const char *rpc_reload_doc[2] = {
		"Write back to disk modified tables", 0};

/* rpc function implementations */
static void rpc_reload(rpc_t *rpc, void *c)
{
	int n;

	LM_INFO("RPC command received!\n");

	/* init DB connection if needed */
	if(db_hdl == NULL) {
		db_hdl = dr_dbf.init(&db_url);
		if(db_hdl == 0) {
			rpc->rpl_printf(c, "cannot initialize database connection");
			return;
		}
	}

	if((n = dr_reload_data()) != 0) {
		rpc->rpl_printf(c, "failed to load routing data");
		return;
	}

	rpc->rpl_printf(c, "reload ok");
	return;
}

static rpc_export_t rpc_methods[] = {
		{"drouting.reload", rpc_reload, rpc_reload_doc, 0}, {0, 0, 0, 0}};


static inline int get_group_id(struct sip_uri *uri)
{
	db_key_t keys_ret[1];
	db_key_t keys_cmp[2];
	db_val_t vals_cmp[2];
	db1_res_t *res;
	int n;


	/* user */
	keys_cmp[0] = &drg_user_col;
	vals_cmp[0].type = DB1_STR;
	vals_cmp[0].nul = 0;
	vals_cmp[0].val.str_val = uri->user;
	n = 1;

	if(use_domain) {
		keys_cmp[1] = &drg_domain_col;
		vals_cmp[1].type = DB1_STR;
		vals_cmp[1].nul = 0;
		vals_cmp[1].val.str_val = uri->host;
		n++;
	}

	keys_ret[0] = &drg_grpid_col;
	res = 0;

	if(dr_dbf.query(db_hdl, keys_cmp, 0, vals_cmp, keys_ret, n, 1, 0, &res)
			< 0) {
		LM_ERR("DB query failed\n");
		goto error;
	}

	if(RES_ROW_N(res) == 0) {
		LM_ERR("no group for user "
			   "\"%.*s\"@\"%.*s\"\n",
				uri->user.len, uri->user.s, uri->host.len, uri->host.s);
		goto error;
	}
	if(res->rows[0].values[0].nul || res->rows[0].values[0].type != DB1_INT) {
		LM_ERR("null or non-integer group_id\n");
		goto error;
	}
	n = res->rows[0].values[0].val.int_val;

	dr_dbf.free_result(db_hdl, res);
	return n;
error:
	if(res)
		dr_dbf.free_result(db_hdl, res);
	return -1;
}


static inline str *build_ruri(
		struct sip_uri *uri, int strip, str *pri, str *hostport)
{
	static str uri_str;
	char *p;

	if(uri->user.len <= strip) {
		LM_ERR("stripping %d makes "
			   "username <%.*s> null\n",
				strip, uri->user.len, uri->user.s);
		return 0;
	}

	uri_str.len = 4 /*sip:*/ + uri->user.len - strip + pri->len
				  + (uri->passwd.s ? (uri->passwd.len + 1) : 0)
				  + 1 /*@*/ + hostport->len
				  + (uri->params.s ? (uri->params.len + 1) : 0)
				  + (uri->headers.s ? (uri->headers.len + 1) : 0);

	if((uri_str.s = (char *)pkg_malloc(uri_str.len + 1)) == 0) {
		LM_ERR("no more pkg mem\n");
		return 0;
	}

	p = uri_str.s;
	*(p++) = 's';
	*(p++) = 'i';
	*(p++) = 'p';
	*(p++) = ':';
	if(pri->len) {
		memcpy(p, pri->s, pri->len);
		p += pri->len;
	}
	memcpy(p, uri->user.s + strip, uri->user.len - strip);
	p += uri->user.len - strip;
	if(uri->passwd.s && uri->passwd.len) {
		*(p++) = ':';
		memcpy(p, uri->passwd.s, uri->passwd.len);
		p += uri->passwd.len;
	}
	*(p++) = '@';
	memcpy(p, hostport->s, hostport->len);
	p += hostport->len;
	if(uri->params.s && uri->params.len) {
		*(p++) = ';';
		memcpy(p, uri->params.s, uri->params.len);
		p += uri->params.len;
	}
	if(uri->headers.s && uri->headers.len) {
		*(p++) = '?';
		memcpy(p, uri->headers.s, uri->headers.len);
		p += uri->headers.len;
	}
	*p = 0;

	if(p - uri_str.s != uri_str.len) {
		LM_CRIT("difference between allocated(%d)"
				" and written(%d)\n",
				uri_str.len, (int)(long)(p - uri_str.s));
		return 0;
	}
	return &uri_str;
}


static int ki_do_routing_furi(sip_msg_t *msg)
{
	int grp_id;
	struct to_body *from;
	struct sip_uri uri;

	/* get the username from FROM_HDR */
	if(parse_from_header(msg) != 0) {
		LM_ERR("unable to parse from hdr\n");
		return -1;
	}
	from = (struct to_body *)msg->from->parsed;
	/* parse uri */
	if(parse_uri(from->uri.s, from->uri.len, &uri) != 0) {
		LM_ERR("unable to parse from uri\n");
		return -1;
	}

	grp_id = get_group_id(&uri);
	if(grp_id < 0) {
		LM_ERR("failed to get group id\n");
		return -1;;
	}

	return do_routing(msg, grp_id);
}

static int do_routing_0(struct sip_msg *msg, char *str1, char *str2)
{
	return ki_do_routing_furi(msg);
}

static int do_routing_1(struct sip_msg *msg, char *str1, char *str2)
{
	int grp_id;
	if(fixup_get_ivalue(msg, (gparam_t*)str1, &grp_id)<0) {
		LM_ERR("failed to get group id parameter\n");
		return -1;
	}
	return do_routing(msg, grp_id);
}


static int ki_next_routing(sip_msg_t *msg)
{
	struct usr_avp *avp;
	int_str val;

	/* search for the first RURI AVP containing a string */
	do {
		avp = search_first_avp(ruri_avp.type, ruri_avp.name, &val, 0);
	} while(avp && (avp->flags & AVP_VAL_STR) == 0);

	if(!avp)
		return -1;

	if(rewrite_ruri(msg, val.s.s) == -1) {
		LM_ERR("failed to rewite RURI\n");
		return -1;
	}
	destroy_avp(avp);
	LM_DBG("new RURI set to <%.*s>\n", val.s.len, val.s.s);

	/* remove the old attrs */
	avp = NULL;
	do {
		if(avp)
			destroy_avp(avp);
		avp = search_first_avp(attrs_avp.type, attrs_avp.name, NULL, 0);
	} while(avp && (avp->flags & AVP_VAL_STR) == 0);
	if(avp)
		destroy_avp(avp);

	return 1;
}

static int use_next_gw(struct sip_msg *msg, char *p1, char *p2)
{
	return ki_next_routing(msg);
}

int dr_already_choosen(rt_info_t *rt_info, int *active_gwlist,
		int *local_gwlist, int lgw_size, int check)
{
	int l;

	for(l = 0; l < lgw_size; l++) {
		if(rt_info->pgwl[active_gwlist[local_gwlist[l]]].pgw
				== rt_info->pgwl[check].pgw) {
			LM_INFO("Gateway already chosen %.*s, local_gwlist[%d]=%d, %d\n",
					rt_info->pgwl[check].pgw->ip.len,
					rt_info->pgwl[check].pgw->ip.s, l, local_gwlist[l], check);
			return 1;
		}
	}

	return 0;
}

static int do_routing(struct sip_msg *msg, int grp_id)
{
	rt_info_t *rt_info;
	int i, j, l, t;
	struct sip_uri uri;
	str *ruri;
	int_str val;
#define DR_MAX_GWLIST 32
	static int active_gwlist[DR_MAX_GWLIST];
	static int local_gwlist[DR_MAX_GWLIST];
	int gwlist_size;
	int ret;
	pgw_t *dest;

	ret = -1;

	if((*rdata) == 0 || (*rdata)->pgw_l == 0) {
		LM_DBG("empty ruting table\n");
		goto error1;
	}

	LM_DBG("using dr group %d\n", grp_id);

	/* get the number */
	ruri = GET_RURI(msg);
	/* parse ruri */
	if(parse_uri(ruri->s, ruri->len, &uri) != 0) {
		LM_ERR("unable to parse RURI\n");
		goto error1;
	}

/* ref the data for reading */
again:
	lock_get(ref_lock);
	/* if reload must be done, do un ugly busy waiting 
	 * until reload is finished */
	if(*reload_flag) {
		lock_release(ref_lock);
		usleep(5);
		goto again;
	}
	*data_refcnt = *data_refcnt + 1;
	lock_release(ref_lock);

	/* search a prefix */
	rt_info = get_prefix((*rdata)->pt, &uri.user, (unsigned int)grp_id);
	if(rt_info == 0) {
		LM_DBG("no matching for prefix \"%.*s\"\n", uri.user.len, uri.user.s);
		/* try prefixless rules */
		rt_info = check_rt(&(*rdata)->noprefix, (unsigned int)grp_id);
		if(rt_info == 0) {
			LM_DBG("no prefixless matching for "
				   "grp %d\n",
					grp_id);
			goto error2;
		}
	}

	if(rt_info->route_idx > 0 && main_rt.rlist[rt_info->route_idx] != NULL) {
		ret = run_top_route(main_rt.rlist[rt_info->route_idx], msg, 0);
		if(ret < 1) {
			/* drop the action */
			LM_DBG("script route %d drops routing "
				   "by %d\n",
					rt_info->route_idx, ret);
			goto error2;
		}
		ret = -1;
	}

	gwlist_size = (rt_info->pgwa_len > DR_MAX_GWLIST) ? DR_MAX_GWLIST
													  : rt_info->pgwa_len;

	// we filter out inactive gateways
	for(i = 0, j = 0; i < gwlist_size; i++) {
		dest = rt_info->pgwl[i].pgw;

		if(dest->state != KA_STATE_DOWN) {
			active_gwlist[j++] = i;
		}
	}
	// updating gwlist_size value
	gwlist_size = j;

	if(gwlist_size == 0) {
		LM_WARN("no gateways available (all in state down)\n");
		ret = -1;
		goto error2;
	}


	/* set gw order */
	if(sort_order >= 1 && gwlist_size > 1) {
		j = 0;
		t = 0;
		while(j < gwlist_size) {
			/* identify the group: [j..i) */
			for(i = j + 1; i < gwlist_size; i++)
				if(rt_info->pgwl[active_gwlist[j]].grpid
						!= rt_info->pgwl[active_gwlist[i]].grpid)
					break;
			if(i - j == 1) {
				local_gwlist[t++] = j;
				/*LM_DBG("selected gw[%d]=%d\n",
					j, local_gwlist[j]);*/
			} else {
				if(i - j == 2) {
					local_gwlist[t++] = j + kam_rand() % 2;
					if(sort_order == 1) {
						local_gwlist[t++] = j + (local_gwlist[j] - j + 1) % 2;
						/*LM_DBG("selected gw[%d]=%d"
						 *  " gw[%d]=%d\n", j, local_gwlist[j], j+1,
						 *  local_gwlist[j+1]);*/
					}
				} else {
					local_gwlist[t++] = j + kam_rand() % (i - j);
					if(sort_order == 1) {
						do {
							local_gwlist[t] = j + kam_rand() % (i - j);
						} while(local_gwlist[t] == local_gwlist[t - 1]);
						t++;

						/*
						LM_DBG("selected gw[%d]=%d"
							" gw[%d]=%d.\n",
							j, local_gwlist[j], j+1, local_gwlist[j+1]); */
						/* add the rest in this group */
						for(l = j; l < i; l++) {
							if(l == local_gwlist[j] || l == local_gwlist[j + 1])
								continue;
							local_gwlist[t++] = l;
							/* LM_DBG("selected gw[%d]=%d.\n",
								j+k, local_gwlist[t]); */
						}
					}
				}
			}

			if(sort_order == 2) {
				/* check not to use the same gateway as before */
				if(t > 1) {
					/* check if all in the current set were already chosen */
					if(i - j <= t - 1) {
						for(l = j; l < i; l++) {
							if(!dr_already_choosen(rt_info, active_gwlist,
									   local_gwlist, t - 1, active_gwlist[l]))
								break;
						}
						if(l == i) {
							LM_INFO("All gateways in group from %d - %d were "
									"already used\n",
									j, i);
							t--; /* jump over this group, nothing to choose here */
							j = i;
							continue;
						}
					}
					while(dr_already_choosen(rt_info, active_gwlist,
							local_gwlist, t - 1,
							active_gwlist[local_gwlist[t - 1]])) {
						local_gwlist[t - 1] = j + kam_rand() % (i - j);
					}
				}
				LM_DBG("The %d gateway is %.*s [%d]\n", t,
						rt_info->pgwl[active_gwlist[local_gwlist[t - 1]]]
								.pgw->ip.len,
						rt_info->pgwl[active_gwlist[local_gwlist[t - 1]]]
								.pgw->ip.s,
						local_gwlist[t - 1]);
			}

			/* next group starts from i */
			j = i;
		}

		// sort order 0
	} else {
		LM_DBG("sort order 0\n");

		for(i = 0; i < gwlist_size; i++) {
			local_gwlist[i] = i;
		}

		t = j;
	}

	/* do some cleanup first */
	destroy_avps(ruri_avp.type, ruri_avp.name, 1);

	if(j == 0) {
		LM_WARN("no destinations available\n");
		ret = -1;
		goto error2;
	}

	/* push gwlist into avps in reverse order */
	for(j = t - 1; j >= 1; j--) {
		/* build uri */
		ruri = build_ruri(&uri,
				rt_info->pgwl[active_gwlist[local_gwlist[j]]].pgw->strip,
				&rt_info->pgwl[active_gwlist[local_gwlist[j]]].pgw->pri,
				&rt_info->pgwl[active_gwlist[local_gwlist[j]]].pgw->ip);
		if(ruri == 0) {
			LM_ERR("failed to build avp ruri\n");
			goto error2;
		}
		LM_DBG("adding gw [%d] as avp \"%.*s\"\n", local_gwlist[j], ruri->len,
				ruri->s);
		/* add ruri avp */
		val.s = *ruri;
		if(add_avp(AVP_VAL_STR | (ruri_avp.type), ruri_avp.name, val) != 0) {
			LM_ERR("failed to insert ruri avp\n");
			pkg_free(ruri->s);
			goto error2;
		}
		pkg_free(ruri->s);
		/* add attrs avp */
		val.s = rt_info->pgwl[active_gwlist[local_gwlist[j]]].pgw->attrs;
		LM_DBG("setting attr [%.*s] as avp\n", val.s.len, val.s.s);
		if(add_avp(AVP_VAL_STR | (attrs_avp.type), attrs_avp.name, val) != 0) {
			LM_ERR("failed to insert attrs avp\n");
			goto error2;
		}
	}

	/* use first GW in RURI */
	ruri = build_ruri(&uri,
			rt_info->pgwl[active_gwlist[local_gwlist[0]]].pgw->strip,
			&rt_info->pgwl[active_gwlist[local_gwlist[0]]].pgw->pri,
			&rt_info->pgwl[active_gwlist[local_gwlist[0]]].pgw->ip);

	/* add attrs avp */
	val.s = rt_info->pgwl[active_gwlist[local_gwlist[0]]].pgw->attrs;
	LM_DBG("setting attr [%.*s] as for ruri\n", val.s.len, val.s.s);
	if(add_avp(AVP_VAL_STR | (attrs_avp.type), attrs_avp.name, val) != 0) {
		LM_ERR("failed to insert attrs avp\n");
		goto error2;
	}

	/* we are done reading -> unref the data */
	lock_get(ref_lock);
	*data_refcnt = *data_refcnt - 1;
	lock_release(ref_lock);

	/* what hev we get here?? */
	if(ruri == 0) {
		LM_ERR("failed to build ruri\n");
		goto error1;
	}
	LM_DBG("setting the gw [%d] as ruri \"%.*s\"\n",
			active_gwlist[local_gwlist[0]], ruri->len, ruri->s);
	if(msg->new_uri.s)
		pkg_free(msg->new_uri.s);
	msg->new_uri = *ruri;
	msg->parsed_uri_ok = 0;
	ruri_mark_new();

	return 1;
error2:
	/* we are done reading -> unref the data */
	lock_get(ref_lock);
	*data_refcnt = *data_refcnt - 1;
	lock_release(ref_lock);
error1:
	return ret;
}



static int strip_username(struct sip_msg *msg, int strip)
{
	struct action act;
	struct run_act_ctx ra_ctx;

	act.type = STRIP_T;
	act.val[0].type = NUMBER_ST;
	act.val[0].u.number = strip;
	act.next = 0;

	init_run_actions_ctx(&ra_ctx);
	if(do_action(&ra_ctx, &act, msg) < 0) {
		LM_ERR("Error in do_action\n");
		return -1;
	}
	return 0;
}


static int ki_is_from_gw(sip_msg_t *msg)
{
	pgw_addr_t *pgwa = NULL;

	if(rdata == NULL || *rdata == NULL || msg == NULL)
		return -1;

	pgwa = (*rdata)->pgw_addr_l;
	while(pgwa) {
		if((pgwa->port == 0 || pgwa->port == msg->rcv.src_port)
				&& ip_addr_cmp(&pgwa->ip, &msg->rcv.src_ip))
			return 1;
		pgwa = pgwa->next;
	}
	return -1;
}

static int is_from_gw_0(struct sip_msg *msg, char *str, char *str2)
{
	return ki_is_from_gw(msg);
}

static int ki_is_from_gw_type(sip_msg_t *msg, int type)
{
	pgw_addr_t *pgwa = NULL;

	if(rdata == NULL || *rdata == NULL || msg == NULL)
		return -1;

	pgwa = (*rdata)->pgw_addr_l;
	while(pgwa) {
		if(type == pgwa->type
				&& (pgwa->port == 0 || pgwa->port == msg->rcv.src_port)
				&& ip_addr_cmp(&pgwa->ip, &msg->rcv.src_ip))
			return 1;
		pgwa = pgwa->next;
	}
	return -1;
}

static int is_from_gw_1(struct sip_msg *msg, char *str1, char *str2)
{
	int type;

	if(fixup_get_ivalue(msg, (gparam_t*)str1, &type)<0) {
		LM_ERR("failed to get parameter value\n");
		return -1;
	}

	return ki_is_from_gw_type(msg, type);
}

static int ki_is_from_gw_type_flags(sip_msg_t *msg, int type, int flags)
{
	pgw_addr_t *pgwa = NULL;

	if(rdata == NULL || *rdata == NULL || msg == NULL)
		return -1;

	pgwa = (*rdata)->pgw_addr_l;
	while(pgwa) {
		if(type == pgwa->type
				&& (pgwa->port == 0 || pgwa->port == msg->rcv.src_port)
				&& ip_addr_cmp(&pgwa->ip, &msg->rcv.src_ip)) {
			if(flags != 0 && pgwa->strip > 0)
				strip_username(msg, pgwa->strip);
			return 1;
		}
		pgwa = pgwa->next;
	}
	return -1;
}

static int is_from_gw_2(struct sip_msg *msg, char *str1, char *str2)
{
	int type;
	int flags;

	if(fixup_get_ivalue(msg, (gparam_t*)str1, &type)<0) {
		LM_ERR("failed to get type parameter value\n");
		return -1;
	}
	if(fixup_get_ivalue(msg, (gparam_t*)str2, &flags)<0) {
		LM_ERR("failed to get flags parameter value\n");
		return -1;
	}

	return ki_is_from_gw_type_flags(msg, type, flags);
}

static int ki_goes_to_gw_type(struct sip_msg *msg, int type)
{
	pgw_addr_t *pgwa = NULL;
	struct sip_uri puri;
	struct ip_addr *ip;
	str *uri;

	if(rdata == NULL || *rdata == NULL || msg == NULL)
		return -1;

	uri = GET_NEXT_HOP(msg);

	if(parse_uri(uri->s, uri->len, &puri) < 0) {
		LM_ERR("bad uri <%.*s>\n", uri->len, uri->s);
		return -1;
	}

	if(((ip = str2ip(&puri.host)) != 0) || ((ip = str2ip6(&puri.host)) != 0)) {
		pgwa = (*rdata)->pgw_addr_l;
		while(pgwa) {
			if((type < 0 || type == pgwa->type) && ip_addr_cmp(&pgwa->ip, ip))
				return 1;
			pgwa = pgwa->next;
		}
	}

	return -1;
}

static int goes_to_gw_1(struct sip_msg *msg, char *_type, char *_f2)
{
	int type;
	if(fixup_get_ivalue(msg, (gparam_t*)_type, &type)<0) {
		LM_ERR("failed to get parameter value\n");
		return -1;
	}
	return ki_goes_to_gw_type(msg, type);
}

static int goes_to_gw_0(struct sip_msg *msg, char *_type, char *_f2)
{
	return ki_goes_to_gw_type(msg, -1);
}

static int ki_goes_to_gw(sip_msg_t *msg)
{
	return ki_goes_to_gw_type(msg, -1);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_drouting_exports[] = {
	{ str_init("drouting"), str_init("do_routing_furi"),
		SR_KEMIP_INT, ki_do_routing_furi,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("drouting"), str_init("do_routing"),
		SR_KEMIP_INT, do_routing,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("drouting"), str_init("next_routing"),
		SR_KEMIP_INT, ki_next_routing,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("drouting"), str_init("use_next_gw"),
		SR_KEMIP_INT, ki_next_routing,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("drouting"), str_init("is_from_gw"),
		SR_KEMIP_INT, ki_is_from_gw,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("drouting"), str_init("is_from_gw_type"),
		SR_KEMIP_INT, ki_is_from_gw_type,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("drouting"), str_init("is_from_gw_type_flags"),
		SR_KEMIP_INT, ki_is_from_gw_type_flags,
		{ SR_KEMIP_INT, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("drouting"), str_init("goes_to_gw"),
		SR_KEMIP_INT, ki_goes_to_gw,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("drouting"), str_init("goes_to_gw_type"),
		SR_KEMIP_INT, ki_goes_to_gw_type,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_drouting_exports);
	return 0;
}
