/**
 * $Id$
 *
 * Copyright (C) 2008 Elena-Ramona Modroiu (asipto.com)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <fcntl.h>

#include "../../sr_module.h"
#include "../../timer.h"
#include "../../route.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../rpc.h"
#include "../../rpc_lookup.h"
#include "../../lib/kmi/mi.h"
#include "../../lib/kcore/faked_msg.h"

#include "../../pvar.h"
#include "htable.h"

#include "ht_api.h"
#include "ht_db.h"
#include "ht_var.h"
#include "api.h"


MODULE_VERSION

/* dmq API structure */
dmq_api_t ht_dmq_bind;
dmq_peer_t* ht_dmq_peer;
dmq_resp_cback_t ht_dmq_resp_cback;

int dmq_htable_callback(struct sip_msg* msg, peer_reponse_t* resp) {
	int content_length;
	str body;
	pv_value_t val;
	str key, htname;
	if(parse_headers(msg, HDR_EOH_F, 0) < 0) {
		LM_ERR("error parsing message headers\n");
		goto error;
	}
	if(!msg->content_length) {
		LM_ERR("no content length header found\n");
		goto error;
	}
	content_length = get_content_length(msg);
	if(!content_length) {
		LM_ERR("content length is 0\n");
		goto error;
	}
	body.s = get_body(msg);
	body.len = content_length;
	
	htname.s = pkg_malloc(MAX_HT_SERIALIZE_BUF);
	htname.len = MAX_HT_SERIALIZE_BUF;
	key.s = pkg_malloc(MAX_HT_SERIALIZE_BUF);
	key.len = MAX_HT_SERIALIZE_BUF;
	deserialize_ht_pair(&key, &val, &htname, &body);
	if(dmq_ht_set_cell(&key, &val, &htname) < 0) {
		LM_ERR("error setting cell\n");
		pkg_free(htname.s);
		pkg_free(key.s);
		return -1;
	}
	pkg_free(htname.s);
	pkg_free(key.s);
	LM_ERR("it worked - dmq module triggered the htable callback [%ld %d]\n", time(0), my_pid());
	
	str ct = str_init("text/xml");
	str reason = str_init("200 OK");
	resp->content_type = ct;
	resp->reason = reason;
	resp->body.s = 0;
	resp->resp_code = 200;
	return 0;
error:
	return -1;
}

static void ht_add_dmq_peer() {
	dmq_peer_t htable_peer;
	memset(&ht_dmq_resp_cback, 0, sizeof(ht_dmq_resp_cback));
	htable_peer.peer_id.s = "htable";
	htable_peer.peer_id.len = 6;
	htable_peer.description.s = "ditributed htable implementation using dmq";
	htable_peer.description.len = 42;
	htable_peer.callback = dmq_htable_callback;
	ht_dmq_peer = ht_dmq_bind.register_dmq_peer(&htable_peer);
}

int ht_timer_interval = 20;

static int htable_init_rpc(void);

/** module functions */
static int ht_print(struct sip_msg*, char*, char*);
static int mod_init(void);
static int child_init(int rank);
static void destroy(void);

static int fixup_ht_rm(void** param, int param_no);
static int ht_rm_name_re(struct sip_msg* msg, char* key, char* foo);
static int ht_rm_value_re(struct sip_msg* msg, char* key, char* foo);

int ht_param(modparam_t type, void* val);

static struct mi_root* ht_mi_reload(struct mi_root* cmd_tree, void* param);
static struct mi_root* ht_mi_dump(struct mi_root* cmd_tree, void* param);

static pv_export_t mod_pvs[] = {
	{ {"sht", sizeof("sht")-1}, PVT_OTHER, pv_get_ht_cell, pv_set_ht_cell,
		pv_parse_ht_name, 0, 0, 0 },
	{ {"shtex", sizeof("shtex")-1}, PVT_OTHER, pv_get_ht_cell_expire,
		pv_set_ht_cell_expire,
		pv_parse_ht_name, 0, 0, 0 },
	{ {"shtcn", sizeof("shtcn")-1}, PVT_OTHER, pv_get_ht_cn, 0,
		pv_parse_ht_name, 0, 0, 0 },
	{ {"shtcv", sizeof("shtcv")-1}, PVT_OTHER, pv_get_ht_cv, 0,
		pv_parse_ht_name, 0, 0, 0 },
	{ {"shtinc", sizeof("shtinc")-1}, PVT_OTHER, pv_get_ht_inc, 0,
		pv_parse_ht_name, 0, 0, 0 },
	{ {"shtdec", sizeof("shtdec")-1}, PVT_OTHER, pv_get_ht_dec, 0,
		pv_parse_ht_name, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

static mi_export_t mi_cmds[] = {
	{ "sht_reload",     ht_mi_reload,  0,  0,  0},
	{ "sht_dump",       ht_mi_dump,    0,  0,  0},
	{ 0, 0, 0, 0, 0}
};


static cmd_export_t cmds[]={
	{"sht_print",       (cmd_function)ht_print,        0, 0, 0,
		ANY_ROUTE},
	{"sht_rm_name_re",  (cmd_function)ht_rm_name_re,   1, fixup_ht_rm, 0,
		ANY_ROUTE},
	{"sht_rm_value_re", (cmd_function)ht_rm_value_re,  1, fixup_ht_rm, 0,
		ANY_ROUTE},
	{"bind_htable",     (cmd_function)bind_htable,     0, 0, 0,
		ANY_ROUTE},
	{0,0,0,0,0,0}
};

static param_export_t params[]={
	{"htable",             STR_PARAM|USE_FUNC_PARAM, (void*)ht_param},
	{"db_url",             STR_PARAM, &ht_db_url.s},
	{"key_name_column",    STR_PARAM, &ht_db_name_column.s},
	{"key_type_column",    STR_PARAM, &ht_db_ktype_column.s},
	{"value_type_column",  STR_PARAM, &ht_db_vtype_column.s},
	{"key_value_column",   STR_PARAM, &ht_db_value_column.s},
	{"array_size_suffix",  STR_PARAM, &ht_array_size_suffix.s},
	{"fetch_rows",         INT_PARAM, &ht_fetch_rows},
	{"timer_interval",     INT_PARAM, &ht_timer_interval},
	{0,0,0}
};


/** module exports */
struct module_exports exports= {
	"htable",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,          /* exported statistics */
	mi_cmds,    /* exported MI functions */
	mod_pvs,    /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,
	(destroy_function) destroy,
	child_init  /* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	if(register_mi_mod(exports.name, mi_cmds)!=0)
	{
		LM_ERR("failed to register MI commands\n");
		return -1;
	}
	if(htable_init_rpc()!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if(ht_init_tables()!=0)
		return -1;
	ht_db_init_params();

	if(ht_db_url.len>0)
	{
		if(ht_db_init_con()!=0)
			return -1;
		if(ht_db_open_con()!=0)
			return -1;
		if(ht_db_load_tables()!=0)
		{
			ht_db_close_con();
			return -1;
		}
		ht_db_close_con();
	}
	if(ht_has_autoexpire())
	{
		LM_DBG("starting auto-expire timer\n");
		if(ht_timer_interval<=0)
			ht_timer_interval = 20;
		if(register_timer(ht_timer, 0, ht_timer_interval)<0)
		{
			LM_ERR("failed to register timer function\n");
			return -1;
		}
	}
	
	if(dmq_load_api(&ht_dmq_bind) < 0) {
		LM_ERR("cannot load dmq api\n");
		return -1;
	} else {
		ht_add_dmq_peer();
		LM_DBG("presence-dmq loaded\n");
	}
	return 0;
}


static int child_init(int rank)
{
	struct sip_msg *fmsg;
	struct run_act_ctx ctx;
	int rtb, rt;

	LM_DBG("rank is (%d)\n", rank);
	if (rank!=PROC_INIT)
		return 0;
	
	rt = route_get(&event_rt, "htable:mod-init");
	if(rt>=0 && event_rt.rlist[rt]!=NULL) {
		LM_DBG("executing event_route[htable:mod-init] (%d)\n", rt);
		if(faked_msg_init()<0)
			return -1;
		fmsg = faked_msg_next();
		rtb = get_route_type();
		set_route_type(REQUEST_ROUTE);
		init_run_actions_ctx(&ctx);
		run_top_route(event_rt.rlist[rt], fmsg, &ctx);
		if(ctx.run_flags&DROP_R_F)
		{
			LM_ERR("exit due to 'drop' in event route\n");
			return -1;
		}
		set_route_type(rtb);
	}

	return 0;
}

/**
 * destroy function
 */
static void destroy(void)
{
	/* sync back to db */
	if(ht_db_url.len>0)
	{
		if(ht_db_init_con()==0)
		{
			if(ht_db_open_con()==0)
			{
				ht_db_sync_tables();
				ht_db_close_con();
			}
		}
	}
	ht_destroy();
}

/**
 * print hash table content
 */
static int ht_print(struct sip_msg *msg, char *s1, char *s2)
{
	ht_dbg();
	return 1;
}

static int fixup_ht_rm(void** param, int param_no)
{
	pv_spec_t *sp;
	str s;

	sp = (pv_spec_t*)pkg_malloc(sizeof(pv_spec_t));
	if(param_no != 1)
	{
		LM_ERR("invalid parameter number %d\n", param_no);
		return -1;
	}
	if (sp == 0)
	{
		LM_ERR("no pkg memory left\n");
		return -1;
	}
	memset(sp, 0, sizeof(pv_spec_t));
	s.s = (char*)*param; s.len = strlen(s.s);
	if(pv_parse_ht_name(sp, &s)<0)
	{
		pkg_free(sp);
		LM_ERR("invalid parameter %d\n", param_no);
		return -1;
	}
	*param = (void*)sp;
	return 0;
}

static int ht_rm_name_re(struct sip_msg* msg, char* key, char* foo)
{
	ht_pv_t *hpv;
	str sre;
	pv_spec_t *sp;
	sp = (pv_spec_t*)key;

	hpv = (ht_pv_t*)sp->pvp.pvn.u.dname;

	if(hpv->ht==NULL)
	{
		hpv->ht = ht_get_table(&hpv->htname);
		if(hpv->ht==NULL)
			return 1;
	}
	if(pv_printf_s(msg, hpv->pve, &sre)!=0)
	{
		LM_ERR("cannot get $ht expression\n");
		return -1;
	}
	if(ht_rm_cell_re(&sre, hpv->ht, 0)<0)
		return -1;
	return 1;
}

static int ht_rm_value_re(struct sip_msg* msg, char* key, char* foo)
{
	ht_pv_t *hpv;
	str sre;
	pv_spec_t *sp;
	sp = (pv_spec_t*)key;

	hpv = (ht_pv_t*)sp->pvp.pvn.u.dname;

	if(hpv->ht==NULL)
	{
		hpv->ht = ht_get_table(&hpv->htname);
		if(hpv->ht==NULL)
			return 1;
	}
	if(pv_printf_s(msg, hpv->pve, &sre)!=0)
	{
		LM_ERR("cannot get $ht expression\n");
		return -1;
	}

	if(ht_rm_cell_re(&sre, hpv->ht, 1)<0)
		return -1;
	return 1;
}


int ht_param(modparam_t type, void *val)
{
	if(val==NULL)
		goto error;

	return ht_table_spec((char*)val);
error:
	return -1;

}

#define MI_ERR_RELOAD 			"ERROR Reloading data"
#define MI_ERR_RELOAD_LEN 		(sizeof(MI_ERR_RELOAD)-1)
static struct mi_root* ht_mi_reload(struct mi_root* cmd_tree, void* param)
{
	struct mi_node* node;
	str htname;
	ht_t *ht;
	ht_t nht;
	ht_cell_t *first;
	ht_cell_t *it;
	int i;

	if(ht_db_url.len<=0)
		return init_mi_tree(500, MI_ERR_RELOAD, MI_ERR_RELOAD_LEN);
	
	if(ht_db_init_con()!=0)
		return init_mi_tree(500, MI_ERR_RELOAD, MI_ERR_RELOAD_LEN);
	if(ht_db_open_con()!=0)
		return init_mi_tree(500, MI_ERR_RELOAD, MI_ERR_RELOAD_LEN);

	node = cmd_tree->node.kids;
	if(node == NULL)
	{
		ht_db_close_con();
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);
	}
	htname = node->value;
	if(htname.len<=0 || htname.s==NULL)
	{
		LM_ERR("bad hash table name\n");
		ht_db_close_con();
		return init_mi_tree( 500, "bad hash table name", 19);
	}
	ht = ht_get_table(&htname);
	if(ht==NULL || ht->dbtable.len<=0)
	{
		LM_ERR("bad hash table name\n");
		ht_db_close_con();
		return init_mi_tree( 500, "no such hash table", 18);
	}
	memcpy(&nht, ht, sizeof(ht_t));
	nht.entries = (ht_entry_t*)shm_malloc(nht.htsize*sizeof(ht_entry_t));
	if(nht.entries == NULL)
	{
		ht_db_close_con();
		return init_mi_tree(500, MI_ERR_RELOAD, MI_ERR_RELOAD_LEN);
	}
	memset(nht.entries, 0, nht.htsize*sizeof(ht_entry_t));

	if(ht_db_load_table(&nht, &ht->dbtable, 0)<0)
	{
		ht_db_close_con();
		return init_mi_tree(500, MI_ERR_RELOAD, MI_ERR_RELOAD_LEN);
	}

	/* replace old entries */
	for(i=0; i<nht.htsize; i++)
	{
		lock_get(&ht->entries[i].lock);
		first = ht->entries[i].first;
		ht->entries[i].first = nht.entries[i].first;
		ht->entries[i].esize = nht.entries[i].esize;
		lock_release(&ht->entries[i].lock);
		nht.entries[i].first = first;
	}
	/* free old entries */
	for(i=0; i<nht.htsize; i++)
	{
		first = nht.entries[i].first;
		while(first)
		{
			it = first;
			first = first->next;
			ht_cell_free(it);
		}
	}
	ht_db_close_con();
	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
}

static struct mi_root* ht_mi_dump(struct mi_root* cmd_tree, void* param)
{
	struct mi_node* node;
	struct mi_node* node2;
	struct mi_root *rpl_tree;
	struct mi_node *rpl;
	str htname;
	ht_t *ht;
	ht_cell_t *it;
	int i;
	int len;
	char *p;

	node = cmd_tree->node.kids;
	if(node == NULL)
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);
	htname = node->value;
	if(htname.len<=0 || htname.s==NULL)
	{
		LM_ERR("bad hash table name\n");
		return init_mi_tree( 500, "bad hash table name", 19);
	}
	ht = ht_get_table(&htname);
	if(ht==NULL)
	{
		LM_ERR("bad hash table name\n");
		return init_mi_tree( 500, "no such hash table", 18);
	}

	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	if (rpl_tree==NULL)
		return 0;
	rpl = &rpl_tree->node;

	for(i=0; i<ht->htsize; i++)
	{
		lock_get(&ht->entries[i].lock);
		it = ht->entries[i].first;
		if(it)
		{
			/* add entry node */
			p = int2str((unsigned long)i, &len);
			node = add_mi_node_child(rpl, MI_DUP_VALUE, "Entry", 5, p, len);
			if (node==0)
				goto error;
			while(it)
			{
				if(it->flags&AVP_VAL_STR) {
					node2 = add_mi_node_child(node, MI_DUP_VALUE, it->name.s, it->name.len,
							it->value.s.s, it->value.s.len);
				} else {
					p = sint2str((long)it->value.n, &len);
					node2 = add_mi_node_child(node, MI_DUP_VALUE, it->name.s, it->name.len,
							p, len);
				}
				if (node2==0)
					goto error;
				it = it->next;
			}
		}
		lock_release(&ht->entries[i].lock);
	}

	return rpl_tree;
error:
	free_mi_tree(rpl_tree);
	return 0;
}

static const char* htable_dump_doc[2] = {
	"Dump the contents of hash table.",
	0
};

static void  htable_rpc_dump(rpc_t* rpc, void* c)
{
	str htname;
	ht_t *ht;
	ht_cell_t *it;
	int i;
	void* th;
	void* ih;
	void* vh;

	if (rpc->scan(c, "S", &htname) < 1)
	{
		rpc->fault(c, 500, "No htable name given");
		return;
	}
	ht = ht_get_table(&htname);
	if(ht==NULL)
	{
		rpc->fault(c, 500, "No such htable");
		return;
	}
	for(i=0; i<ht->htsize; i++)
	{
		lock_get(&ht->entries[i].lock);
		it = ht->entries[i].first;
		if(it)
		{
			/* add entry node */
			if (rpc->add(c, "{", &th) < 0)
			{
				rpc->fault(c, 500, "Internal error creating rpc");
				return;
			}
			if(rpc->struct_add(th, "dd{",
							"entry", i,
							"size",  (int)ht->entries[i].esize,
							"slot",  &ih)<0)
			{
				rpc->fault(c, 500, "Internal error creating rpc");
				return;
			}
			while(it)
			{
				if(rpc->struct_add(ih, "{",
							"item", &vh)<0)
				{
					rpc->fault(c, 500, "Internal error creating rpc");
					return;
				}
				if(it->flags&AVP_VAL_STR) {
					if(rpc->struct_add(vh, "SS",
							"name",  &it->name.s,
							"value", &it->value.s)<0)
					{
						rpc->fault(c, 500, "Internal error adding item");
						return;
					}
				} else {
					if(rpc->struct_add(vh, "Sd",
							"name",  &it->name.s,
							"value", (int)it->value.n))
					{
						rpc->fault(c, 500, "Internal error adding item");
						return;
					}
				}
				it = it->next;
			}
		}
		lock_release(&ht->entries[i].lock);
	}
}

rpc_export_t htable_rpc[] = {
	{"htable.dump", htable_rpc_dump, htable_dump_doc, 0},
	{0, 0, 0, 0}
};

static int htable_init_rpc(void)
{
	if (rpc_register_array(htable_rpc)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}
