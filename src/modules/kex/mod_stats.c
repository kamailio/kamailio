/*
 * Copyright (C) 2010 Daniel-Constantin Mierla (asipto.com)
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
 *
 */

/*!
 * \file
 * \brief KEX :: Kamailio private memory pool statistics
 * \ingroup kex
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/pt.h"
#include "../../core/sr_module.h"
#include "../../core/events.h"
#include "../../core/mem/f_malloc.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"

#include "mod_stats.h"


#define DBG_MOD_PKG_FLAG		0
#define DBG_MOD_SHM_FLAG		1
#define DBG_MOD_ALL_FLAG		2

/**
 *
 */
int mod_stats_init(void)
{
	return 0;
}

/**
 *
 */
int mod_stats_destroy(void)
{
	return 0;
}

/**
 *
 */
static const char* rpc_mod_stats_doc[2] = {
	"Per module memory statistics",
	0
};

/* test if the current mod info was already printed */
static int rpc_mod_is_printed_one(mem_counter *stats, mem_counter *current) {
	mem_counter *iter = stats;

	while (iter && iter != current) {
		if (strcmp(iter->mname, current->mname) == 0) {
			return 1;
		}
		iter = iter->next;
	}

	return 0;
}

/* print memory info for a specific module in a specific stats list */
static int rpc_mod_print(rpc_t *rpc, void *ctx, const char *mname,
	mem_counter *stats)
{
	char buff[128];
	const char *total_str= "Total";
	void *stats_th = NULL;
	int total = 0;
	mem_counter *iter = stats;

	if (stats == NULL) {
		return -1;
	}

	if (rpc->add(ctx, "{", &stats_th) < 0) {
		rpc->fault(ctx, 500, "Internal error creating struct rpc");
		return -1;
	}

	while (iter) {
		if (strcmp(mname, iter->mname) == 0) {
			sprintf(buff, "%s(%ld)", iter->func, iter->line);
			if (rpc->struct_add(stats_th, "d", buff, iter->size) < 0) {
				rpc->fault(ctx, 500, "Internal error adding to struct rpc");
				return -1;
			}
			total += iter->size;
		}
		iter = iter->next;
	}

	if (rpc->struct_add(stats_th, "d", total_str, total) < 0) {
		rpc->fault(ctx, 500, "Internal error adding total to struct rpc");
		return -1;
	}

	return total;
}

/* print memory info for a specific module */
static int rpc_mod_print_one(rpc_t *rpc, void *ctx, const char *mname,
	mem_counter *pkg_stats, mem_counter *shm_stats, int flag)
{
	if (rpc->rpl_printf(ctx, "Module: %s", mname) < 0) {
		rpc->fault(ctx, 500, "Internal error adding module name to ctx");
		return -1;
	}

	switch (flag){
		case DBG_MOD_PKG_FLAG:
			rpc_mod_print(rpc, ctx, mname, pkg_stats);
			break;
		case DBG_MOD_SHM_FLAG:
			rpc_mod_print(rpc, ctx, mname, shm_stats);
			break;
		case DBG_MOD_ALL_FLAG:
			rpc_mod_print(rpc, ctx, mname, pkg_stats);
			rpc_mod_print(rpc, ctx, mname, shm_stats);
			break;
		default:
			rpc_mod_print(rpc, ctx, mname, pkg_stats);
			rpc_mod_print(rpc, ctx, mname, shm_stats);
			break;
	}

	if (rpc->rpl_printf(ctx, "") < 0) {
		rpc->fault(ctx, 500, "Internal error adding module name to ctx");
		return -1;
	}

	return 0;
}

/* print memory info for all modules */
static int rpc_mod_print_all(rpc_t *rpc, void *ctx,
		mem_counter *pkg_stats, mem_counter *shm_stats, int flag)
{
	mem_counter *pkg_iter = pkg_stats;
	mem_counter *shm_iter = shm_stats;

	/* print unique module info found in pkg_stats */
	while (pkg_iter) {
		if (!rpc_mod_is_printed_one(pkg_stats, pkg_iter)) {
			rpc_mod_print_one(rpc, ctx,
				pkg_iter->mname, pkg_stats, shm_stats, flag);
		}
		pkg_iter = pkg_iter->next;
	}

	/* print unique module info found in shm_stats and not found in pkg_stats */
	while (shm_iter) {
		if (!rpc_mod_is_printed_one(shm_stats, shm_iter)
				&& !rpc_mod_is_printed_one(pkg_stats, shm_iter)) {
			rpc_mod_print_one(rpc, ctx,
				shm_iter->mname, pkg_stats, shm_stats, flag);
		}
		shm_iter = shm_iter->next;
	}
	return 0;
}

/**
 *
 */
static void rpc_mod_stats(rpc_t *rpc, void *ctx)
{
	int flag = DBG_MOD_ALL_FLAG;
	str mname = STR_NULL;
	str mtype = STR_NULL;

	mem_counter *pkg_mod_stats_list = NULL;
	mem_counter *shm_mod_stats_list = NULL;

	if (rpc->scan(ctx, "*S", &mname) != 1) {
		rpc->fault(ctx, 500, "Module name or \"all\" needed");
		return;
	}

	if (rpc->scan(ctx, "*S", &mtype) != 1) {
		rpc->fault(ctx, 500, "\"pkg\" or \"shm\" or \"all\" needed");
		return;
	}

	if (strcmp(mtype.s, "pkg") == 0) {
		flag = DBG_MOD_PKG_FLAG;
	} else if (strcmp(mtype.s, "shm") == 0) {
		flag = DBG_MOD_SHM_FLAG;
	} else if (strcmp(mtype.s, "all") == 0) {
		flag = DBG_MOD_ALL_FLAG;
	}

	pkg_mod_get_stats((void **)&pkg_mod_stats_list);
	shm_mod_get_stats((void **)&shm_mod_stats_list);

	/* print info about all modules */
	if (strcmp(mname.s, "all") == 0) {
		rpc_mod_print_all(rpc, ctx, pkg_mod_stats_list, shm_mod_stats_list, flag);

	/* print info about a particular module */
	} else {
		rpc_mod_print_one(rpc, ctx, mname.s, pkg_mod_stats_list, shm_mod_stats_list, flag);
	}

	pkg_mod_free_stats(pkg_mod_stats_list);
	shm_mod_free_stats(shm_mod_stats_list);
}

/**
 *
 */
rpc_export_t kex_mod_rpc[] = {
	{"mod.stats", rpc_mod_stats,  rpc_mod_stats_doc,	   RET_ARRAY},
	{0, 0, 0, 0}
};

/**
 *
 */
int mod_stats_init_rpc(void)
{
	if (rpc_register_array(kex_mod_rpc)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}
