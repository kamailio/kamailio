/*
 * Prefix Route Module
 *
 * Copyright (C) 2007 Alfred E. Heggestad
 * Copyright (C) 2008 Telio Telecom AS
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

/*! \file
 * \brief Prefix Route Module - module interface
 * \ingroup prefix_route
 */

/*! \defgroup prefix_route Prefix routing module
 * \ingroup modules
 * The prefix_route module does routing based on a set of prefixes from the database.
 * @{
 */


#include <stdio.h>
#include <stdlib.h>
#include "../../lib/srdb2/db.h"
#include "../../core/rpc.h"
#include "../../core/sr_module.h"
#include "../../core/mod_fix.h"
#include "../../core/mem/mem.h"
#include "../../core/data_lump_rpl.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/locking.h"
#include "../../core/action.h"
#include "../../core/route.h"
#include "../../core/kemi.h"
#include "tree.h"
#include "pr.h"


MODULE_VERSION


/* Modules parameters */
static char *db_url = DEFAULT_DB_URL;
static char *db_table = "prefix_route";
static int prefix_route_exit = 1;

static int add_route(
		struct tree_item *root, const char *prefix, const char *route)
{
	int ix, err;

	/* We cache the route index here so we can avoid route_lookup()
	 * in the prefix_route() routing function.
	 */
	ix = route_lookup(&main_rt, (char *)route);
	if(ix < 0) {
		LM_CRIT("route name '%s' is not defined\n", route);
		return -1;
	}

	if(ix >= main_rt.entries) {
		LM_CRIT("route %d > n_entries (%d)\n", ix, main_rt.entries);
		return -1;
	}

	err = tree_item_add(root, prefix, route, ix);
	if(0 != err) {
		LM_CRIT("tree_item_add() failed (%d)\n", err);
		return err;
	}

	return 0;
}


/**
 * Load prefix rules from database and build tree in memory
 */
int pr_db_load(void)
{
	db_ctx_t *ctx;
	db_cmd_t *cmd = NULL;
	db_res_t *res = NULL;
	db_rec_t *rec;
	struct tree_item *root;
	int err = -1;
	int count = 0;
	db_fld_t match[] = {{.name = "prefix", .type = DB_CSTR},
			{.name = "route", .type = DB_CSTR},
			{.name = "comment", .type = DB_CSTR},
			{.name = NULL, .type = DB_NONE}};

	ctx = db_ctx("prefix_route");
	if(!ctx) {
		LM_ERR("db_ctx() failed\n");
		return -1;
	}
	if(db_add_db(ctx, db_url) < 0) {
		LM_ERR(" could not add db\n");
		goto out;
	}
	if(db_connect(ctx) < 0) {
		LM_ERR("could not connect\n");
		goto out;
	}

	cmd = db_cmd(DB_GET, ctx, db_table, match, NULL, NULL);
	if(!cmd) {
		LM_ERR("db_cmd() failed\n");
		goto out;
	}

	if(db_exec(&res, cmd) < 0) {
		LM_ERR("db_exec() failed\n");
		goto out;
	}

	root = tree_item_alloc();
	if(NULL == root) {
		LM_ERR("tree alloc failed\n");
		err = -1;
		goto out;
	}

	/* Assume Success */
	err = 0;

	/* Read from DB and build tree */
	for(rec = db_first(res); rec != NULL && !err; rec = db_next(res)) {
		const char *prefix, *route, *comment;

		++count;

		if(rec->fld[0].flags & DB_NULL) {
			LM_CRIT("bad 'prefix' record in table %s, skipping...\n", db_table);
			continue;
		}
		if(rec->fld[1].flags & DB_NULL) {
			LM_CRIT("bad 'route' record in table %s, skipping...\n", db_table);
			continue;
		}

		prefix = rec->fld[0].v.cstr;
		route = rec->fld[1].v.cstr;
		comment = rec->fld[2].v.cstr;

		LM_DBG("  %d: prefix=%-10s  route=%-15s  comment=%s\n", count, prefix,
				route, comment);

		err = add_route(root, prefix, route);
	}

	LM_NOTICE("Total prefix routes loaded: %d\n", count);

	/* Error */
	if(0 != err) {
		LM_ERR("error flushing tree\n");
		tree_item_free(root);
		goto out;
	}

	/* Swap trees */
	err = tree_swap(root);
	if(0 != err)
		goto out;

out:
	/* Free database results */
	if(res)
		db_res_free(res);
	if(cmd)
		db_cmd_free(cmd);

	/* Close database connection */
	if(ctx)
		db_ctx_free(ctx);

	return err;
}


/**
 * Initialize module
 */
static int mod_init(void)
{
	/* Initialise tree */
	if(0 != tree_init()) {
		LM_CRIT("tree init failed\n\n");
		return -1;
	}

	/* Populate database */
	if(0 != pr_db_load()) {
		LM_CRIT("db load failed\n\n");
		return -1;
	}

	return 0;
}


/**
 * Destroy module and free all resources
 */
static void mod_destroy(void)
{
	tree_close();
}


/**
 * Extract username from the Request URI
 * First try to look at the original Request URI and if there
 * is no username use the new Request URI
 */
static int get_username(struct sip_msg *msg, str *user)
{
	if(!msg || !user)
		return -1;

	if(parse_sip_msg_uri(msg) < 0) {
		LM_ERR("bad sip msg uri\n");
		return -1; /* error, bad uri */
	}

	if(msg->parsed_uri.user.s == 0) {
		/* no user in uri */
		LM_ERR("no user in uri\n");
		return -2;
	}

	*user = msg->parsed_uri.user;

	return 0;
}


/**
 * ki command "prefix_route"
 */
static int ki_prefix_route(sip_msg_t *msg, str *ruser)
{
	struct run_act_ctx ra_ctx;
	int err;
	int route;

	route = tree_route_get(ruser);
	if(route <= 0)
		return -1;

	/* If match send to route[x] */
	init_run_actions_ctx(&ra_ctx);

	err = run_actions(&ra_ctx, main_rt.rlist[route], msg);
	if(err < 0) {
		LM_ERR("run_actions failed (%d)\n", err);
		return -1;
	}

	/* Success */
	return (prefix_route_exit) ? 0 : 1;
}

/**
 * ki command "prefix_route"
 */
static int ki_prefix_route_uri(sip_msg_t *msg)
{
	str user;
	int err;

	err = get_username(msg, &user);
	if(0 != err) {
		LM_ERR("could not get username in Request URI (%d)\n", err);
		return err;
	}

	return ki_prefix_route(msg, &user);
}

/**
 * cfg command "prefix_route"
 */
static int prefix_route(struct sip_msg *msg, char *p1, char *p2)
{
	str user;
	int err;

	/* Unused */
	(void)p2;

	/* Get request URI */
	if(p1 == NULL) {
		err = get_username(msg, &user);
		if(0 != err) {
			LM_ERR("could not get username in Request URI (%d)\n", err);
			return err;
		}
	} else {
		if(fixup_get_svalue(msg, (gparam_t *)p1, &user) < 0) {
			LM_ERR("could not get username in parameter\n");
			return -1;
		}
	}

	return ki_prefix_route(msg, &user);
}
/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
		{"prefix_route", prefix_route, 0, 0, 0,
				REQUEST_ROUTE | BRANCH_ROUTE | FAILURE_ROUTE},
		{"prefix_route", prefix_route, 1, fixup_spve_null, 0, ANY_ROUTE},
		{0, 0, 0, 0, 0, 0}};

/*
 * Exported parameters
 */
static param_export_t params[] = {{"db_url", PARAM_STRING, &db_url},
		{"db_table", PARAM_STRING, &db_table},
		{"exit", INT_PARAM, &prefix_route_exit}, {0, 0, 0}};

/*
 * Module description
 */
struct module_exports exports = {
		"prefix_route",	 /* Module name              */
		DEFAULT_DLFLAGS, /* dlopen flags             */
		cmds,			 /* exported functions       */
		params,			 /* exported parameters      */
		pr_rpc,			 /* RPC methods              */
		0,				 /* pseudo-variables exports */
		0,				 /* response function        */
		mod_init,		 /* initialization function  */
		0,				 /* per-child init function  */
		mod_destroy		 /* module destroy function  */
};

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_prefix_route_exports[] = {
	{ str_init("prefix_route"), str_init("prefix_route"),
		SR_KEMIP_INT, ki_prefix_route,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("prefix_route"), str_init("prefix_route_uri"),
		SR_KEMIP_INT, ki_prefix_route_uri,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_prefix_route_exports);
	return 0;
}

/** @} */
