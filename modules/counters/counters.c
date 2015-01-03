/*
 * Copyright (C) 2010 iptelorg GmbH
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @brief counters/statistics rpcs and script functions
 * @file
 * @ingroup counters
 * Module: counters.
 * @author andrei
 */

/*!
 * \defgroup counters Counters/statistics
 * 
 */
 
#include "../../modparam.h"
#include "../../dprint.h"
#include "../../compiler_opt.h"
#include "../../counters.h"

MODULE_VERSION

/* default script counter group name */
static char* cnt_script_grp = "script";

static int add_script_counter(modparam_t type, void* val);
static int cnt_inc_f(struct sip_msg*, char*, char*);
static int cnt_add_f(struct sip_msg*, char*, char*);
static int cnt_reset_f(struct sip_msg*, char*, char*);
static int cnt_fixup1(void** param, int param_no);
static int cnt_int_fixup(void** param, int param_no);



static cmd_export_t cmds[] = {
	{"cnt_inc",    cnt_inc_f,   1,  cnt_fixup1,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|ONSEND_ROUTE},
	{"cnt_add",    cnt_add_f,   2,  cnt_int_fixup,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|ONSEND_ROUTE},
	{"cnt_reset", cnt_reset_f,  1, cnt_fixup1,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|ONSEND_ROUTE},
	{0,0,0,0,0}
};

static param_export_t params[] = {
	{"script_cnt_grp_name", PARAM_STRING, &cnt_script_grp},
	{"script_counter", PARAM_STRING|PARAM_USE_FUNC, add_script_counter},
	{0,0,0}
};


static void cnt_get_rpc(rpc_t* rpc, void* ctx);
static const char* cnt_get_doc[] = {
	"get counter value (takes group and counter name as parameters)", 0
};

static void cnt_reset_rpc(rpc_t* rpc, void* ctx);
static const char* cnt_reset_doc[] = {
	"reset counter (takes group and counter name as parameters)", 0
};

static void cnt_get_raw_rpc(rpc_t* rpc, void* ctx);
static const char* cnt_get_raw_doc[] = {
	"get raw counter value (debugging version)", 0
};

static void cnt_grps_list_rpc(rpc_t* rpc, void* ctx);
static const char* cnt_grps_list_doc[] = {
	"list all the counter group names", 0
};

static void cnt_var_list_rpc(rpc_t* rpc, void* ctx);
static const char* cnt_var_list_doc[] = {
	"list all the counters names in a specified group", 0
};

static void cnt_grp_get_all_rpc(rpc_t* rpc, void* ctx);
static const char* cnt_grp_get_all_doc[] = {
	"list all counter names and values in a specified group", 0
};

static void cnt_help_rpc(rpc_t* rpc, void* ctx);
static const char* cnt_help_doc[] = {
	"print the description of a counter (group and counter name required).", 0
};



static rpc_export_t counters_rpc[] = {
	{"cnt.get", cnt_get_rpc, cnt_get_doc, 0 },
	{"cnt.reset", cnt_reset_rpc, cnt_reset_doc, 0 },
	{"cnt.get_raw", cnt_get_raw_rpc, cnt_get_raw_doc, 0 },
	{"cnt.grps_list", cnt_grps_list_rpc, cnt_grps_list_doc, RET_ARRAY },
	{"cnt.var_list", cnt_var_list_rpc, cnt_var_list_doc, RET_ARRAY },
	{"cnt.grp_get_all", cnt_grp_get_all_rpc, cnt_grp_get_all_doc, 0 },
	{"cnt.help", cnt_help_rpc, cnt_help_doc, 0},
	{ 0, 0, 0, 0}
};



struct module_exports exports= {
	"counters",
	cmds,
	counters_rpc,        /* RPC methods */
	params,
	0, /* module initialization function */
	0, /* response function */
	0, /* destroy function */
	0, /* on_cancel function */
	0, /* per-child init function */
};



/** parse the the script_counter modparam.
 *  Format:   [grp.]name[( |:)desc]
 *  E.g.:
 *           "name" => new counter: *cnt_script_grp."name"
 *           "grp.name" => new counter: "grp"."name"
 *           "name desc" => new counter "name", desc = "desc"
 *           "grp.name desc" => "grp"."name", desc = "desc".
 */
static int add_script_counter(modparam_t type, void* val)
{
	char* name;
	counter_handle_t h;
	int ret;
	char* grp;
	char* desc;
	char* p;

	if ((type & PARAM_STRING) == 0) {
		BUG("bad parameter type %d\n", type);
		goto error;
	}
	name = (char*) val;
	grp = cnt_script_grp; /* default group */
	desc = "custom script counter."; /* default desc. */
	if ((p = strchr(name, ':')) != 0 ||
			(p = strchr(name, ' ')) != 0) {
		/* found desc. */
		*p = 0;
		for(p = p+1; *p && (*p == ' ' || *p == '\t'); p++);
		if (*p)
			desc = p;
	}
	if ((p = strchr(name, '.')) != 0) {
		/* found group */
		grp = name;
		*p = 0;
		name = p+1;
	}
	ret = counter_register(&h, grp, name, 0, 0, 0, desc, 0);
	if (ret < 0) {
		if (ret == -2) {
			ERR("counter %s.%s already registered\n", grp, name);
			return 0;
		}
		ERR("failed to register counter %s.%s\n", grp, name);
		goto error;
	}
	return 0;
error:
	return -1;
}



static int cnt_fixup1(void** param, int param_no)
{
	char* name;
	char* grp;
	char* p;
	counter_handle_t h;

	name = (char*)*param;
	grp = cnt_script_grp; /* default group */
	if ((p = strchr(name, '.')) != 0) {
		/* found group */
		grp = name;
		name = p+1;
		*p = 0;
	}
	if (counter_lookup(&h, grp, name) < 0) {
		ERR("counter %s.%s does not exist (forgot to define it?)\n",
				grp, name);
		return -1;
	}
	*param = (void*)(long)h.id;
	return 0;
}



static int cnt_int_fixup(void** param, int param_no)
{
	char* name;
	char* grp;
	char* p;
	counter_handle_t h;

	if (param_no == 1) {
		name = (char*)*param;
		grp = cnt_script_grp; /* default group */
		if ((p = strchr(name, '.')) != 0) {
			/* found group */
			grp = name;
			name = p+1;
			*p = 0;
		}
		if (counter_lookup(&h, grp, name) < 0) {
			ERR("counter %s.%s does not exist (forgot to define it?)\n",
					grp, name);
			return -1;
		}
		*param = (void*)(long)h.id;
	} else
		return fixup_var_int_2(param, param_no);
	return 0;
}



static int cnt_inc_f(struct sip_msg* msg, char* handle, char* bar)
{
	counter_handle_t h;
	
	h.id = (long)(void*)handle;
	counter_inc(h);
	return 1;
}



static int cnt_add_f(struct sip_msg* msg, char* handle, char* val)
{
	counter_handle_t h;
	int v;
	
	h.id = (long)(void*)handle;
	if (unlikely(get_int_fparam(&v, msg, (fparam_t*)val) < 0)) {
		ERR("non integer parameter\n");
		return -1;
	}
	counter_add(h, v);
	return 1;
}



static int cnt_reset_f(struct sip_msg* msg, char* handle, char* bar)
{
	counter_handle_t h;
	
	h.id = (long)(void*)handle;
	counter_reset(h);
	return 1;
}



static void cnt_grp_get_all(rpc_t* rpc, void* c, char* group);



static void cnt_get_rpc(rpc_t* rpc, void* c)
{
	char* group;
	char* name;
	counter_val_t v;
	counter_handle_t h;
	
	if (rpc->scan(c, "s", &group) < 1)
		return;
	if (rpc->scan(c, "*s", &name) < 1)
		return cnt_grp_get_all(rpc, c, group);
	/* group & name read */
	if (counter_lookup(&h, group, name) < 0) {
		rpc->fault(c, 400, "non-existent counter %s.%s\n", group, name);
		return;
	}
	v = counter_get_val(h);
	rpc->add(c, "d", (int)v);
	return;
}



static void cnt_get_raw_rpc(rpc_t* rpc, void* c)
{
	char* group;
	char* name;
	counter_val_t v;
	counter_handle_t h;
	
	if (rpc->scan(c, "ss", &group, &name) < 2) {
		/* rpc->fault(c, 400, "group and counter name required"); */
		return;
	}
	if (counter_lookup(&h, group, name) < 0) {
		rpc->fault(c, 400, "non-existent counter %s.%s\n", group, name);
		return;
	}
	v = counter_get_raw_val(h);
	rpc->add(c, "d", (int)v);
	return;
}



static void cnt_reset_rpc(rpc_t* rpc, void* c)
{
	char* group;
	char* name;
	counter_handle_t h;
	
	if (rpc->scan(c, "ss", &group, &name) < 2) {
		/* rpc->fault(c, 400, "group and counter name required"); */
		return;
	}
	if (counter_lookup(&h, group, name) < 0) {
		rpc->fault(c, 400, "non-existent counter %s.%s\n", group, name);
		return;
	}
	counter_reset(h);
	return;
}



struct rpc_list_params {
	rpc_t* rpc;
	void* ctx;
};


/* helper callback for iterating groups or names */
static  void rpc_print_name(void* param, str* n)
{
	struct rpc_list_params* p;
	rpc_t* rpc;
	void* ctx;

	p = param;
	rpc = p->rpc;
	ctx = p->ctx;
	rpc->add(ctx, "S", n);
}


/* helper callback for iterating on variable names & values*/
static  void rpc_print_name_val(void* param, str* g, str* n,
								counter_handle_t h)
{
	struct rpc_list_params* p;
	rpc_t* rpc;
	void* s;

	p = param;
	rpc = p->rpc;
	s = p->ctx;
	rpc->struct_add(s, "d", n->s, (int)counter_get_val(h));
}



static void cnt_grps_list_rpc(rpc_t* rpc, void* c)
{
	struct rpc_list_params packed_params;
	
	packed_params.rpc = rpc;
	packed_params.ctx = c;
	counter_iterate_grp_names(rpc_print_name, &packed_params);
}



static void cnt_var_list_rpc(rpc_t* rpc, void* c)
{
	char* group;
	struct rpc_list_params packed_params;
	
	if (rpc->scan(c, "s", &group) < 1) {
		/* rpc->fault(c, 400, "group name required"); */
		return;
	}
	packed_params.rpc = rpc;
	packed_params.ctx = c;
	counter_iterate_grp_var_names(group, rpc_print_name, &packed_params);
}



static void cnt_grp_get_all(rpc_t* rpc, void* c, char* group)
{
	void* s;
	struct rpc_list_params packed_params;
	
	if (rpc->add(c, "{", &s) < 0) return;
	packed_params.rpc = rpc;
	packed_params.ctx = s;
	counter_iterate_grp_vars(group, rpc_print_name_val, &packed_params);
}



static void cnt_grp_get_all_rpc(rpc_t* rpc, void* c)
{
	char* group;
	
	if (rpc->scan(c, "s", &group) < 1) {
		/* rpc->fault(c, 400, "group name required"); */
		return;
	}
	return cnt_grp_get_all(rpc, c, group);
}



static void cnt_help_rpc(rpc_t* rpc, void* ctx)
{
	char* group;
	char* name;
	char* desc;
	counter_handle_t h;
	
	if (rpc->scan(ctx, "ss", &group, &name) < 2) {
		/* rpc->fault(c, 400, "group and counter name required"); */
		return;
	}
	if (counter_lookup(&h, group, name) < 0) {
		rpc->fault(ctx, 400, "non-existent counter %s.%s\n", group, name);
		return;
	}
	desc = counter_get_doc(h);
	if (desc)
		rpc->add(ctx, "s", desc);
	else
		rpc->fault(ctx, 400, "no description for counter %s.%s\n",
					group, name);
	return;
}

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
