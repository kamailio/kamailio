/*
 * $Id$
 *
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


 * History:
 * ---------
 *  2005-02-20  first version (cristian)
 *  2005-02-27  ported to 0.9.0 (bogdan)
 */

#include "stdlib.h"
#include "stdio.h"
#include "assert.h"
#include <unistd.h>

#include "../../sr_module.h"
#include "../../str.h"
#include "../../dprint.h"
#include "../../usr_avp.h"
#include "../../lib/srdb1/db.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../locking.h"
#include "../../action.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../resolve.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_uri.h"
#include "../../dset.h"
#include "../../rpc_lookup.h"

#include "dr_load.h"
#include "prefix_tree.h"
#include "routing.h"


/*** DB relatede stuff ***/
/* parameters  */
static str db_url = {NULL,0};
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

/* DRG table columns */
static str drg_user_col = str_init("username");
static str drg_domain_col = str_init("domain");
static str drg_grpid_col = str_init("groupid");
/* variables */
static db1_con_t  *db_hdl=0;     /* DB handler */
static db_func_t dr_dbf;        /* DB functions */

/* current dr data - pointer to a pointer in shm */
static rt_data_t **rdata = 0;

/* AVP used to store serial RURIs */
static struct _ruri_avp{
	unsigned short type; /* AVP ID */
	int_str name; /* AVP name*/
}ruri_avp = { 0, {.n=(int)0xad346b2f} };
static str ruri_avp_spec = {0,0};

/* AVP used to store serial ATTRs */
static struct _attrs_avp{
	unsigned short type; /* AVP ID */
	int_str name; /* AVP name*/
}attrs_avp = { 0, {.n=(int)0xad346b30} };
static str attrs_avp_spec = {0,0};

/* statistic data */
int tree_size = 0;
int inode = 0;
int unode = 0;

/* lock, ref counter and flag used for reloading the date */
static gen_lock_t *ref_lock = 0;
static int* data_refcnt = 0;
static int* reload_flag = 0;

static int dr_init(void);
static int dr_child_init(int rank);
static int dr_exit(void);

static int fixup_do_routing(void** param, int param_no);
static int fixup_from_gw(void** param, int param_no);

static int do_routing(struct sip_msg* msg, dr_group_t *drg);
static int do_routing_0(struct sip_msg* msg, char* str1, char* str2);
static int do_routing_1(struct sip_msg* msg, char* str1, char* str2);
static int use_next_gw(struct sip_msg* msg);
static int is_from_gw_0(struct sip_msg* msg, char* str1, char* str2);
static int is_from_gw_1(struct sip_msg* msg, char* str1, char* str2);
static int is_from_gw_2(struct sip_msg* msg, char* str1, char* str2);
static int goes_to_gw_0(struct sip_msg* msg, char* f1, char* f2);
static int goes_to_gw_1(struct sip_msg* msg, char* f1, char* f2);

MODULE_VERSION

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"do_routing",  (cmd_function)do_routing_0,   0,  0, 0,
		REQUEST_ROUTE|FAILURE_ROUTE},
	{"do_routing",  (cmd_function)do_routing_1,   1,  fixup_do_routing, 0,
		REQUEST_ROUTE|FAILURE_ROUTE},
	{"use_next_gw",  (cmd_function)use_next_gw,   0,  0, 0,
		REQUEST_ROUTE|FAILURE_ROUTE},
	{"next_routing",  (cmd_function)use_next_gw, 0,  0, 0,
		FAILURE_ROUTE},
	{"is_from_gw",  (cmd_function)is_from_gw_0,   0,  0, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE},
	{"is_from_gw",  (cmd_function)is_from_gw_1,   1,  fixup_from_gw, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE},
	{"is_from_gw",  (cmd_function)is_from_gw_2,   2,  fixup_from_gw, 0,
		REQUEST_ROUTE},
	{"goes_to_gw",  (cmd_function)goes_to_gw_0,   0,  0, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE},
	{"goes_to_gw",  (cmd_function)goes_to_gw_1,   1,  fixup_from_gw, 0,
		REQUEST_ROUTE|FAILURE_ROUTE|ONREPLY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"db_url",          PARAM_STR, &db_url        },
	{"drd_table",       PARAM_STR, &drd_table     },
	{"drr_table",       PARAM_STR, &drr_table     },
	{"drg_table",       PARAM_STR, &drg_table     },
	{"drl_table",       PARAM_STR, &drl_table     },
	{"use_domain",      INT_PARAM, &use_domain      },
	{"drg_user_col",    PARAM_STR, &drg_user_col  },
	{"drg_domain_col",  PARAM_STR, &drg_domain_col},
	{"drg_grpid_col",   PARAM_STR, &drg_grpid_col },
	{"ruri_avp",        PARAM_STR, &ruri_avp_spec },
	{"attrs_avp",       PARAM_STR, &attrs_avp_spec},
	{"sort_order",      INT_PARAM, &sort_order      },
	{"fetch_rows",      INT_PARAM, &dr_fetch_rows   },
	{"force_dns",       INT_PARAM, &dr_force_dns    },
	{0, 0, 0}
};

static rpc_export_t rpc_methods[];

struct module_exports exports = {
	"drouting",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* Exported functions */
	params,          /* Exported parameters */
	NULL,            /* exported statistics */
	NULL,            /* exported MI functions */
	NULL,            /* exported pseudo-variables */
	0,               /* additional processes */
	dr_init,         /* Module initialization function */
	(response_function) NULL,
	(destroy_function) dr_exit,
	(child_init_function) dr_child_init /* per-child init function */
};


/**
 * Rewrite Request-URI
 */
static inline int rewrite_ruri(struct sip_msg* _m, char* _s)
{
   struct action act;
   struct run_act_ctx ra_ctx;

   memset(&act, '\0', sizeof(act));
   act.type = SET_URI_T;
   act.val[0].type = STRING_ST;
   act.val[0].u.string = _s;
   init_run_actions_ctx(&ra_ctx);
   if (do_action(&ra_ctx, &act, _m) < 0)
   {
      LM_ERR("do_action failed\n");
      return -1;
   }
   return 0;
}

static inline int dr_reload_data( void )
{
	rt_data_t *new_data;
	rt_data_t *old_data;

	new_data = dr_load_routing_info( &dr_dbf, db_hdl,
		&drd_table, &drl_table, &drr_table);
	if ( new_data==0 ) {
		LM_CRIT("failed to load routing info\n");
		return -1;
	}

	/* block access to data for all readers */
	lock_get( ref_lock );
	*reload_flag = 1;
	lock_release( ref_lock );

	/* wait for all readers to finish - it's a kind of busy waitting but
	 * it's not critical;
	 * at this point, data_refcnt can only be decremented */
	while (*data_refcnt) {
		usleep(10);
	}

	/* no more activ readers -> do the swapping */
	old_data = *rdata;
	*rdata = new_data;

	/* release the readers */
	*reload_flag = 0;

	/* destroy old data */
	if (old_data)
		free_rt_data( old_data, 1 );

	return 0;
}



static int dr_init(void)
{
	pv_spec_t avp_spec;

	LM_INFO("DRouting - initializing\n");

	if (rpc_register_array(rpc_methods)!=0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	/* check the module params */
	if (db_url.s==NULL || db_url.len<=0) {
		LM_CRIT("mandatory parameter \"DB_URL\" found empty\n");
		goto error;
	}

	if (drd_table.len<=0) {
		LM_CRIT("mandatory parameter \"DRD_TABLE\" found empty\n");
		goto error;
	}

	if (drr_table.len<=0) {
		LM_CRIT("mandatory parameter \"DRR_TABLE\" found empty\n");
		goto error;
	}

	if (drg_table.len<=0) {
		LM_CRIT("mandatory parameter \"DRG_TABLE\"  found empty\n");
		goto error;
	}

	if (drl_table.len<=0) {
		LM_CRIT("mandatory parameter \"DRL_TABLE\"  found empty\n");
		goto error;
	}

	/* fix AVP spec */
	if (ruri_avp_spec.s) {
		if (pv_parse_spec( &ruri_avp_spec, &avp_spec)==0
		|| avp_spec.type!=PVT_AVP) {
			LM_ERR("malformed or non AVP [%.*s] for RURI AVP definition\n",
				ruri_avp_spec.len, ruri_avp_spec.s);
			return E_CFG;
		}

		if( pv_get_avp_name(0, &(avp_spec.pvp), &(ruri_avp.name),
		&(ruri_avp.type) )!=0) {
			LM_ERR("[%.*s]- invalid AVP definition for RURI AVP\n",
				ruri_avp_spec.len, ruri_avp_spec.s);
			return E_CFG;
		}
	}
	if (attrs_avp_spec.s) {
		if (pv_parse_spec( &attrs_avp_spec, &avp_spec)==0
		|| avp_spec.type!=PVT_AVP) {
			LM_ERR("malformed or non AVP [%.*s] for ATTRS AVP definition\n",
				attrs_avp_spec.len, attrs_avp_spec.s);
			return E_CFG;
		}

		if( pv_get_avp_name(0, &(avp_spec.pvp), &(attrs_avp.name),
		&(attrs_avp.type) )!=0) {
			LM_ERR("[%.*s]- invalid AVP definition for ATTRS AVP\n",
				attrs_avp_spec.len, attrs_avp_spec.s);
			return E_CFG;
		}
	}

	/* data pointer in shm */
	rdata = (rt_data_t**)shm_malloc( sizeof(rt_data_t*) );
	if (rdata==0) {
		LM_CRIT("failed to get shm mem for data ptr\n");
		goto error;
	}
	*rdata = 0;

	/* create & init lock */
	if ( (ref_lock=lock_alloc())==0) {
		LM_CRIT("failed to alloc ref_lock\n");
		goto error;
	}
	if (lock_init(ref_lock)==0 ) {
		LM_CRIT("failed to init ref_lock\n");
		goto error;
	}
	data_refcnt = (int*)shm_malloc(sizeof(int));
	reload_flag = (int*)shm_malloc(sizeof(int));
	if(!data_refcnt || !reload_flag)
	{
		LM_ERR("no more shared memory\n");
		goto error;
	}
	*data_refcnt = 0;
	*reload_flag = 0;

	/* bind to the mysql module */
	if (db_bind_mod( &db_url, &dr_dbf  )) {
		LM_CRIT("cannot bind to database module! "
			"Did you forget to load a database module ?\n");
		goto error;
	}

	if (!DB_CAPABILITY( dr_dbf, DB_CAP_QUERY)) {
		LM_CRIT( "database modules does not "
			"provide QUERY functions needed by DRounting module\n");
		return -1;
	}

	return 0;
error:
	if (ref_lock) {
		lock_destroy( ref_lock );
		lock_dealloc( ref_lock );
		ref_lock = 0;
	}
	if (db_hdl) {
		dr_dbf.close(db_hdl);
		db_hdl = 0;
	}
	if (rdata) {
		shm_free(rdata);
		rdata = 0;
	}
	return -1;
}



static int dr_child_init(int rank)
{
	/* only workers needs DB connection */
	if (rank==PROC_MAIN || rank==PROC_TCP_MAIN || rank==PROC_INIT)
		return 0;

	/* init DB connection */
	if ( (db_hdl=dr_dbf.init(&db_url))==0 ) {
		LM_CRIT("cannot initialize database connection\n");
		return -1;
	}

	/* child 1 load the routing info */
	if ( (rank==1) && dr_reload_data()!=0 ) {
		LM_CRIT("failed to load routing data\n");
		return -1;
	}

	/* set GROUP table for workers */
	if (dr_dbf.use_table( db_hdl, &drg_table) < 0) {
		LM_ERR("cannot select table \"%.*s\"\n", drg_table.len, drg_table.s);
		return -1;
	}
	srand(getpid()+time(0)+rank);
	return 0;
}


static int dr_exit(void)
{
	/* close DB connection */
	if (db_hdl) {
		dr_dbf.close(db_hdl);
		db_hdl = 0;
	}

	/* destroy data */
	if ( rdata) {
		if (*rdata)
			free_rt_data( *rdata, 1 );
		shm_free( rdata );
		rdata = 0;
	}

	/* destroy lock */
	if (ref_lock) {
		lock_destroy( ref_lock );
		lock_dealloc( ref_lock );
		ref_lock = 0;
	}
	
	if(reload_flag)
		shm_free(reload_flag);
	if(data_refcnt)
		shm_free(data_refcnt);

	return 0;
}


/* rpc function documentation */
static const char *rpc_reload_doc[2] = {
    "Write back to disk modified tables", 0
};

/* rpc function implementations */
static void rpc_reload(rpc_t *rpc, void *c)
{
	int n;

	LM_INFO("RPC command received!\n");

	/* init DB connection if needed */
	if (db_hdl==NULL) {
		db_hdl=dr_dbf.init(&db_url);
		if(db_hdl==0 ) {
			rpc->rpl_printf(c, "cannot initialize database connection");
			return;
		}
	}

	if ( (n=dr_reload_data())!=0 ) {
		rpc->rpl_printf(c, "failed to load routing data");
		return;
	}

	rpc->rpl_printf(c, "reload ok");
	return;
}

static rpc_export_t rpc_methods[] = {
	{"drouting.reload", rpc_reload, rpc_reload_doc, 0},
	{0, 0, 0, 0}
};


static inline int get_group_id(struct sip_uri *uri)
{
	db_key_t keys_ret[1];
	db_key_t keys_cmp[2];
	db_val_t vals_cmp[2];
	db1_res_t* res;
	int n;


	/* user */
	keys_cmp[0] = &drg_user_col;
	vals_cmp[0].type = DB1_STR;
	vals_cmp[0].nul  = 0;
	vals_cmp[0].val.str_val = uri->user;
	n = 1;

	if (use_domain) {
		keys_cmp[1] = &drg_domain_col;
		vals_cmp[1].type = DB1_STR;
		vals_cmp[1].nul  = 0;
		vals_cmp[1].val.str_val = uri->host;
		n++;
	}

	keys_ret[0] = &drg_grpid_col;
	res = 0;

	if ( dr_dbf.query(db_hdl,keys_cmp,0,vals_cmp,keys_ret,n,1,0,&res)<0 ) {
		LM_ERR("DB query failed\n");
		goto error;
	}

	if (RES_ROW_N(res) == 0) {
		LM_ERR("no group for user "
			"\"%.*s\"@\"%.*s\"\n", uri->user.len, uri->user.s,
			uri->host.len, uri->host.s);
		goto error;
	}
	if (res->rows[0].values[0].nul || res->rows[0].values[0].type!=DB1_INT) {
		LM_ERR("null or non-integer group_id\n");
		goto error;
	}
	n = res->rows[0].values[0].val.int_val;

	dr_dbf.free_result(db_hdl, res);
	return n;
error:
	if (res)
		dr_dbf.free_result(db_hdl, res);
	return -1;
}



static inline str* build_ruri(struct sip_uri *uri, int strip, str *pri,
																str *hostport)
{
	static str uri_str;
	char *p;

	if (uri->user.len<=strip) {
		LM_ERR("stripping %d makes "
			"username <%.*s> null\n",strip,uri->user.len,uri->user.s);
		return 0;
	}

	uri_str.len = 4 /*sip:*/ + uri->user.len - strip +pri->len +
		(uri->passwd.s?(uri->passwd.len+1):0) + 1/*@*/ + hostport->len +
		(uri->params.s?(uri->params.len+1):0) +
		(uri->headers.s?(uri->headers.len+1):0);

	if ( (uri_str.s=(char*)pkg_malloc( uri_str.len + 1))==0) {
		LM_ERR("no more pkg mem\n");
		return 0;
	}

	p = uri_str.s;
	*(p++)='s';
	*(p++)='i';
	*(p++)='p';
	*(p++)=':';
	if (pri->len) {
		memcpy(p, pri->s, pri->len);
		p += pri->len;
	}
	memcpy(p, uri->user.s+strip, uri->user.len-strip);
	p += uri->user.len-strip;
	if (uri->passwd.s && uri->passwd.len) {
		*(p++)=':';
		memcpy(p, uri->passwd.s, uri->passwd.len);
		p += uri->passwd.len;
	}
	*(p++)='@';
	memcpy(p, hostport->s, hostport->len);
	p += hostport->len;
	if (uri->params.s && uri->params.len) {
		*(p++)=';';
		memcpy(p, uri->params.s, uri->params.len);
		p += uri->params.len;
	}
	if (uri->headers.s && uri->headers.len) {
		*(p++)='?';
		memcpy(p, uri->headers.s, uri->headers.len);
		p += uri->headers.len;
	}
	*p = 0;

	if (p-uri_str.s!=uri_str.len) {
		LM_CRIT("difference between allocated(%d)"
			" and written(%d)\n",uri_str.len,(int)(long)(p-uri_str.s));
		return 0;
	}
	return &uri_str;
}


static int do_routing_0(struct sip_msg* msg, char* str1, char* str2)
{
	return do_routing(msg, NULL);
}

static int do_routing_1(struct sip_msg* msg, char* str1, char* str2)
{
	return do_routing(msg, (dr_group_t*)str1);
}


static int use_next_gw(struct sip_msg* msg)
{
	struct usr_avp *avp;
	int_str val;

	/* search for the first RURI AVP containing a string */
	do {
		avp = search_first_avp(ruri_avp.type, ruri_avp.name, &val, 0);
	}while (avp && (avp->flags&AVP_VAL_STR)==0 );

	if (!avp) return -1;

	if (rewrite_ruri(msg, val.s.s)==-1) {
		LM_ERR("failed to rewite RURI\n");
		return -1;
	}
	destroy_avp(avp);
	LM_DBG("new RURI set to <%.*s>\n", val.s.len,val.s.s);

	/* remove the old attrs */
	avp = NULL;
	do {
		if (avp) destroy_avp(avp);
		avp = search_first_avp(attrs_avp.type, attrs_avp.name, NULL, 0);
	}while (avp && (avp->flags&AVP_VAL_STR)==0 );
	if (avp) destroy_avp(avp);

	return 1;
}

int dr_already_choosen(rt_info_t* rt_info, int* local_gwlist, int lgw_size, int check)
{
	int l;

	for ( l = 0; l<lgw_size; l++ ) {
		if ( rt_info->pgwl[local_gwlist[l]].pgw == rt_info->pgwl[check].pgw ) {
			LM_INFO("Gateway already chosen %.*s, local_gwlist[%d]=%d, %d\n",
					rt_info->pgwl[check].pgw->ip.len, rt_info->pgwl[check].pgw->ip.s, l, local_gwlist[l], check);
			return 1;
		}
	}

	return 0;
}

static int do_routing(struct sip_msg* msg, dr_group_t *drg)
{
	struct to_body  *from;
	struct sip_uri  uri;
	rt_info_t      *rt_info;
	int    grp_id;
	int    i, j, l, t;
	str    *ruri;
	int_str val;
	struct usr_avp *avp;
#define DR_MAX_GWLIST	32
	static int local_gwlist[DR_MAX_GWLIST];
	int gwlist_size;
	int ret;

	ret = -1;

	if ( (*rdata)==0 || (*rdata)->pgw_l==0 ) {
		LM_DBG("empty ruting table\n");
		goto error1;
	}

	/* get the username from FROM_HDR */
	if (parse_from_header(msg)!=0) {
		LM_ERR("unable to parse from hdr\n");
		goto error1;
	}
	from = (struct to_body*)msg->from->parsed;
	/* parse uri */
	if (parse_uri( from->uri.s, from->uri.len, &uri)!=0) {
		LM_ERR("unable to parse from uri\n");
		goto error1;
	}

	/* get user's routing group */
	if(drg==NULL)
	{
		grp_id = get_group_id( &uri );
		if (grp_id<0) {
			LM_ERR("failed to get group id\n");
			goto error1;
		}
	} else {
		if(drg->type==0)
			grp_id = (int)drg->u.grp_id;
		else if(drg->type==1) {
			grp_id = 0; /* call get avp here */
			if((avp=search_first_avp( drg->u.avp_id.type,
			drg->u.avp_id.name, &val, 0))==NULL||(avp->flags&AVP_VAL_STR)) {
				LM_ERR( "failed to get group id\n");
				goto error1;
			}
			grp_id = val.n;
		} else
			grp_id = 0; 
	}
	LM_DBG("using dr group %d\n",grp_id);

	/* get the number */
	ruri = GET_RURI(msg);
	/* parse ruri */
	if (parse_uri( ruri->s, ruri->len, &uri)!=0) {
		LM_ERR("unable to parse RURI\n");
		goto error1;
	}

	/* ref the data for reading */
again:
	lock_get( ref_lock );
	/* if reload must be done, do un ugly busy waiting 
	 * until reload is finished */
	if (*reload_flag) {
		lock_release( ref_lock );
		usleep(5);
		goto again;
	}
	*data_refcnt = *data_refcnt + 1;
	lock_release( ref_lock );

	/* search a prefix */
	rt_info = get_prefix( (*rdata)->pt, &uri.user , (unsigned int)grp_id);
	if (rt_info==0) {
		LM_DBG("no matching for prefix \"%.*s\"\n",
			uri.user.len, uri.user.s);
		/* try prefixless rules */
		rt_info = check_rt( &(*rdata)->noprefix, (unsigned int)grp_id);
		if (rt_info==0) {
			LM_DBG("no prefixless matching for "
				"grp %d\n", grp_id);
			goto error2;
		}
	}

	if (rt_info->route_idx>0 && main_rt.rlist[rt_info->route_idx]!=NULL) {
		ret = run_top_route(main_rt.rlist[rt_info->route_idx], msg, 0);
		if (ret<1) {
			/* drop the action */
			LM_DBG("script route %d drops routing "
				"by %d\n", rt_info->route_idx, ret);
			goto error2;
		}
		ret = -1;
	}

	gwlist_size
		= (rt_info->pgwa_len>DR_MAX_GWLIST)?DR_MAX_GWLIST:rt_info->pgwa_len;
	
	/* set gw order */
	if(sort_order>=1&&gwlist_size>1)
	{
		j = 0;
		t = 0;
		while(j<gwlist_size)
		{
			/* identify the group: [j..i) */
			for(i=j+1; i<gwlist_size; i++)
				if(rt_info->pgwl[j].grpid!=rt_info->pgwl[i].grpid)
					break;
			if(i-j==1)
			{
				local_gwlist[t++] = j;
				/*LM_DBG("selected gw[%d]=%d\n",
					j, local_gwlist[j]);*/
			} else {
				if(i-j==2)
				{
					local_gwlist[t++]   = j + rand()%2;
					if(sort_order==1)
					{
						local_gwlist[t++] = j + (local_gwlist[j]-j+1)%2;
						/*LM_DBG("selected gw[%d]=%d"
						 *  " gw[%d]=%d\n", j, local_gwlist[j], j+1,
						 *  local_gwlist[j+1]);*/
					}
				} else {
					local_gwlist[t++]   = j + rand()%(i-j);
					if(sort_order==1)
					{
						do{
							local_gwlist[t] = j + rand()%(i-j);
						}while(local_gwlist[t]==local_gwlist[t-1]);
						t++;

						/*
						LM_DBG("selected gw[%d]=%d"
							" gw[%d]=%d.\n",
							j, local_gwlist[j], j+1, local_gwlist[j+1]); */
						/* add the rest in this group */
						for(l=j; l<i; l++)
						{
							if(l==local_gwlist[j] || l==local_gwlist[j+1])
								continue;
							local_gwlist[t++] = l;
							/* LM_DBG("selected gw[%d]=%d.\n",
								j+k, local_gwlist[t]); */
						}
					}
				}
			}

			if ( sort_order == 2 ) {
				/* check not to use the same gateway as before */
				if ( t>1 ) {
					/* check if all in the current set were already chosen */
					if (i-j <= t-1) {
						for( l = j; l< i; l++) {
							if ( ! dr_already_choosen(rt_info, local_gwlist, t-1, l) )
								break;
						}
						if ( l == i ) {
							LM_INFO("All gateways in group from %d - %d were already used\n", j, i);
							t--; /* jump over this group, nothing to choose here */
							j=i; continue;
						}
					}
					while ( dr_already_choosen(rt_info, local_gwlist, t-1, local_gwlist[t-1]) ) {
						local_gwlist[t-1]   = j + rand()%(i-j);
					}
				}
				LM_DBG("The %d gateway is %.*s [%d]\n", t, rt_info->pgwl[local_gwlist[t-1]].pgw->ip.len,
						rt_info->pgwl[local_gwlist[t-1]].pgw->ip.s, local_gwlist[t-1]);
			}

			/* next group starts from i */
			j=i;
		}
	} else {
		for(i=0; i<gwlist_size; i++)
			local_gwlist[i] = i;
		t = i;
	}

	/* do some cleanup first */
	destroy_avps( ruri_avp.type, ruri_avp.name, 1);

	/* push gwlist into avps in reverse order */
	for( j=t-1 ; j>=1 ; j-- ) {
		/* build uri*/
		ruri = build_ruri(&uri, rt_info->pgwl[local_gwlist[j]].pgw->strip,
				&rt_info->pgwl[local_gwlist[j]].pgw->pri,
				&rt_info->pgwl[local_gwlist[j]].pgw->ip);
		if (ruri==0) {
			LM_ERR("failed to build avp ruri\n");
			goto error2;
		}
		LM_DBG("adding gw [%d] as avp \"%.*s\"\n",
			local_gwlist[j], ruri->len, ruri->s);
		/* add ruri avp */
		val.s = *ruri;
		if (add_avp( AVP_VAL_STR|(ruri_avp.type),ruri_avp.name, val)!=0 ) {
			LM_ERR("failed to insert ruri avp\n");
			pkg_free(ruri->s);
			goto error2;
		}
		pkg_free(ruri->s);
		/* add attrs avp */
		val.s = rt_info->pgwl[local_gwlist[j]].pgw->attrs;
		LM_DBG("setting attr [%.*s] as avp\n",val.s.len,val.s.s);
		if (add_avp( AVP_VAL_STR|(attrs_avp.type),attrs_avp.name, val)!=0 ) {
			LM_ERR("failed to insert attrs avp\n");
			goto error2;
		}
	}

	/* use first GW in RURI */
	ruri = build_ruri(&uri, rt_info->pgwl[local_gwlist[0]].pgw->strip,
			&rt_info->pgwl[local_gwlist[0]].pgw->pri,
			&rt_info->pgwl[local_gwlist[0]].pgw->ip);

	/* add attrs avp */
	val.s = rt_info->pgwl[local_gwlist[0]].pgw->attrs;
	LM_DBG("setting attr [%.*s] as for ruri\n",val.s.len,val.s.s);
	if (add_avp( AVP_VAL_STR|(attrs_avp.type),attrs_avp.name, val)!=0 ) {
		LM_ERR("failed to insert attrs avp\n");
		goto error2;
	}

	/* we are done reading -> unref the data */
	lock_get( ref_lock );
	*data_refcnt = *data_refcnt - 1;
	lock_release( ref_lock );

	/* what hev we get here?? */
	if (ruri==0) {
		LM_ERR("failed to build ruri\n");
		goto error1;
	}
	LM_DBG("setting the gw [%d] as ruri \"%.*s\"\n",
			local_gwlist[0], ruri->len, ruri->s);
	if (msg->new_uri.s)
		pkg_free(msg->new_uri.s);
	msg->new_uri = *ruri;
	msg->parsed_uri_ok = 0;
	ruri_mark_new();

	return 1;
error2:
	/* we are done reading -> unref the data */
	lock_get( ref_lock );
	*data_refcnt = *data_refcnt - 1;
	lock_release( ref_lock );
error1:
	return ret;
}


static int fixup_do_routing(void** param, int param_no)
{
	char *s;
	dr_group_t *drg;
	pv_spec_t avp_spec;
	str r;

	s = (char*)*param;

	if (param_no==1)
	{
		drg = (dr_group_t*)pkg_malloc(sizeof(dr_group_t));
		if(drg==NULL)
		{
			LM_ERR( "no more memory\n");
			return E_OUT_OF_MEM;
		}
		memset(drg, 0, sizeof(dr_group_t));

		if ( s==NULL || s[0]==0 ) {
			LM_CRIT("empty group id definition");
			return E_CFG;
		}

		if (s[0]=='$') {
			/* param is a PV (AVP only supported) */
			r.s = s;
			r.len = strlen(s);
			if (pv_parse_spec( &r, &avp_spec)==0
			|| avp_spec.type!=PVT_AVP) {
				LM_ERR("malformed or non AVP %s AVP definition\n", s);
				return E_CFG;
			}

			if( pv_get_avp_name(0, &(avp_spec.pvp), &(drg->u.avp_id.name),
			&(drg->u.avp_id.type) )!=0) {
				LM_ERR("[%s]- invalid AVP definition\n", s);
				return E_CFG;
			}
			drg->type = 1;
			/* do not free the param as the AVP spec may point inside 
			   this string*/
		} else {
			while(s && *s) {
				if(*s<'0' || *s>'9') {
					LM_ERR( "bad number\n");
					return E_UNSPEC;
				}
				drg->u.grp_id = (drg->u.grp_id)*10+(*s-'0');
				s++;
			}
			pkg_free(*param);
		}
		*param = (void*)drg;
	}

	return 0;
}


static int fixup_from_gw( void** param, int param_no)
{
	unsigned long type;
	int err;

	if (param_no == 1 || param_no == 2) {
		type = str2s(*param, strlen(*param), &err);
		if (err == 0) {
			pkg_free(*param);
			*param = (void *)type;
			return 0;
		} else {
			LM_ERR( "bad number <%s>\n",
				(char *)(*param));
			return E_CFG;
		}
	}
	return 0;
}

static int strip_username(struct sip_msg* msg, int strip)
{
	struct action act;
   struct run_act_ctx ra_ctx;
 
	act.type = STRIP_T;
	act.val[0].type = NUMBER_ST;
	act.val[0].u.number = strip;
	act.next = 0;

   init_run_actions_ctx(&ra_ctx);
   if (do_action(&ra_ctx, &act, msg) < 0)
	{
		LM_ERR( "Error in do_action\n");
		return -1;
	}
	return 0;
}


static int is_from_gw_0(struct sip_msg* msg, char* str, char* str2)
{
	pgw_addr_t *pgwa = NULL;

	if(rdata==NULL || *rdata==NULL || msg==NULL)
		return -1;
	
	pgwa = (*rdata)->pgw_addr_l;
	while(pgwa) {
		if( (pgwa->port==0 || pgwa->port==msg->rcv.src_port) &&
		ip_addr_cmp(&pgwa->ip, &msg->rcv.src_ip))
			return 1;
		pgwa = pgwa->next;
	}
	return -1;
}


static int is_from_gw_1(struct sip_msg* msg, char* str, char* str2)
{
	pgw_addr_t *pgwa = NULL;
	int type = (int)(long)str;

	if(rdata==NULL || *rdata==NULL || msg==NULL)
		return -1;
	
	pgwa = (*rdata)->pgw_addr_l;
	while(pgwa) {
		if( type==pgwa->type && 
		(pgwa->port==0 || pgwa->port==msg->rcv.src_port) &&
		ip_addr_cmp(&pgwa->ip, &msg->rcv.src_ip) )
			return 1;
		pgwa = pgwa->next;
	}
	return -1;
}

static int is_from_gw_2(struct sip_msg* msg, char* str1, char* str2)
{
	pgw_addr_t *pgwa = NULL;
	int type = (int)(long)str1;
	int flags = (int)(long)str2;

	if(rdata==NULL || *rdata==NULL || msg==NULL)
		return -1;
	
	pgwa = (*rdata)->pgw_addr_l;
	while(pgwa) {
		if( type==pgwa->type &&
		(pgwa->port==0 || pgwa->port==msg->rcv.src_port) &&
		ip_addr_cmp(&pgwa->ip, &msg->rcv.src_ip) ) {
			if (flags!=0 && pgwa->strip>0)
				strip_username(msg, pgwa->strip);
			return 1;
		}
		pgwa = pgwa->next;
	}
	return -1;
}


static int goes_to_gw_1(struct sip_msg* msg, char* _type, char* _f2)
{
	pgw_addr_t *pgwa = NULL;
	struct sip_uri puri;
	struct ip_addr *ip;
	str *uri;
	int type;

	if(rdata==NULL || *rdata==NULL || msg==NULL)
		return -1;

	uri = GET_NEXT_HOP(msg);
	type = (int)(long)_type;

	if (parse_uri(uri->s, uri->len, &puri)<0){
		LM_ERR("bad uri <%.*s>\n", uri->len, uri->s);
		return -1;
	}

	if ( ((ip=str2ip(&puri.host))!=0)
	|| ((ip=str2ip6(&puri.host))!=0)
	){
		pgwa = (*rdata)->pgw_addr_l;
		while(pgwa) {
			if( (type<0 || type==pgwa->type) && ip_addr_cmp(&pgwa->ip, ip))
				return 1;
			pgwa = pgwa->next;
		}
	}

	return -1;
}


static int goes_to_gw_0(struct sip_msg* msg, char* _type, char* _f2)
{
	return goes_to_gw_1(msg, (char*)(long)-1, _f2);
}

