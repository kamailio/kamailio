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
#include "../../core/mem/pkg.h"
#include "../../core/mem/shm.h"
#include "../../core/locking.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"

#include "mod_stats.h"


#define DBG_MOD_PKG_FLAG 1 /* 1<<0 - print pkg memory stats */
#define DBG_MOD_SHM_FLAG 2 /* 1<<1 - print shm memory stats */
#define DBG_MOD_ALL_FLAG 3 /* 1|2  - print pkg+shm (1+2) memory stats */
#define DBG_MOD_INF_FLAG 4 /* 1<<2 - print more info in the stats */


static gen_lock_t *kex_rpc_mod_mem_stats_lock = NULL;

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
static const char *rpc_mod_mem_stats_doc[2] = {
		"Per module memory usage statistics", 0};

/**
 *
 */
static const char *rpc_mod_mem_statsx_doc[2] = {
		"Per module memory use statistics with more details", 0};

/* test if the current mod info was already printed */
static int rpc_mod_is_printed_one(mem_counter *stats, mem_counter *current)
{
	mem_counter *iter;

	if(stats == NULL || current == NULL) {
		LM_ERR("invalid parameter\n");
		return 1;
	}
	iter = stats;

	while(iter && iter != current) {
		if(strcmp(iter->mname, current->mname) == 0) {
			return 1;
		}
		iter = iter->next;
	}

	return 0;
}

/* print memory info for a specific module in a specific stats list */
static int rpc_mod_print(
		rpc_t *rpc, void *ctx, const char *mname, mem_counter *stats, int flag)
{
	char nbuf[128];
	char vbuf[128];
	const char *total_str = "Total";
	void *stats_th = NULL;
	int total = 0;
	mem_counter *iter = stats;

	if(stats == NULL) {
		return -1;
	}

	if(rpc->add(ctx, "{", &stats_th) < 0) {
		rpc->fault(ctx, 500, "Internal error creating struct rpc");
		return -1;
	}

	while(iter) {
		if(strcmp(mname, iter->mname) == 0) {
			snprintf(nbuf, 128, "%s(%ld)", iter->func, iter->line);
			if(flag & DBG_MOD_INF_FLAG) {
				/* more info in the value */
				snprintf(vbuf, 128, "%lu (%d)", iter->size, iter->count);
				if(rpc->struct_add(stats_th, "s", nbuf, vbuf) < 0) {
					rpc->fault(ctx, 500, "Internal error adding to struct rpc");
					return -1;
				}
			} else {
				/* only allocated size in the value */
				if(rpc->struct_add(stats_th, "d", nbuf, (int)iter->size) < 0) {
					rpc->fault(ctx, 500, "Internal error adding to struct rpc");
					return -1;
				}
			}
			total += iter->size;
		}
		iter = iter->next;
	}

	if(rpc->struct_add(stats_th, "d", total_str, total) < 0) {
		rpc->fault(ctx, 500, "Internal error adding total to struct rpc");
		return -1;
	}

	return total;
}

/* print memory info for a specific module */
static int rpc_mod_print_one(rpc_t *rpc, void *ctx, const char *mname,
		mem_counter *pkg_stats, mem_counter *shm_stats, int flag)
{
	if(rpc->rpl_printf(ctx, "Module: %s", mname) < 0) {
		rpc->fault(ctx, 500, "Internal error adding module name to ctx");
		return -1;
	}

	if(flag & DBG_MOD_PKG_FLAG) {
		rpc_mod_print(rpc, ctx, mname, pkg_stats, flag);
	}
	if(flag & DBG_MOD_SHM_FLAG) {
		rpc_mod_print(rpc, ctx, mname, shm_stats, flag);
	}

	if(rpc->rpl_printf(ctx, "") < 0) {
		rpc->fault(ctx, 500, "Internal error adding module name to ctx");
		return -1;
	}

	return 0;
}

/* print memory info for all modules */
static int rpc_mod_print_all(rpc_t *rpc, void *ctx, mem_counter *pkg_stats,
		mem_counter *shm_stats, int flag)
{
	mem_counter *pkg_iter = pkg_stats;
	mem_counter *shm_iter = shm_stats;

	/* print unique module info found in pkg_stats */
	while(pkg_iter) {
		if(!rpc_mod_is_printed_one(pkg_stats, pkg_iter)) {
			rpc_mod_print_one(
					rpc, ctx, pkg_iter->mname, pkg_stats, shm_stats, flag);
		}
		pkg_iter = pkg_iter->next;
	}

	/* print unique module info found in shm_stats and not found in pkg_stats */
	while(shm_iter) {
		if(!rpc_mod_is_printed_one(shm_stats, shm_iter)
				&& !rpc_mod_is_printed_one(pkg_stats, shm_iter)) {
			rpc_mod_print_one(
					rpc, ctx, shm_iter->mname, pkg_stats, shm_stats, flag);
		}
		shm_iter = shm_iter->next;
	}
	return 0;
}

/**
 *
 */
static void rpc_mod_mem_stats_mode(rpc_t *rpc, void *ctx, int fmode)
{
	int flag = 0;
	str mname = STR_NULL;
	str mtype = STR_NULL;

	mem_counter *pkg_mod_stats_list = NULL;
	mem_counter *shm_mod_stats_list = NULL;

	if(rpc->scan(ctx, "*S", &mname) != 1) {
		rpc->fault(ctx, 500, "Module name or \"all\" needed");
		return;
	}

	if(rpc->scan(ctx, "*S", &mtype) != 1) {
		rpc->fault(ctx, 500, "\"pkg\" or \"shm\" or \"all\" needed");
		return;
	}

	flag |= fmode;

	if(strcmp(mtype.s, "pkg") == 0) {
		flag |= DBG_MOD_PKG_FLAG;
	} else if(strcmp(mtype.s, "shm") == 0) {
		flag |= DBG_MOD_SHM_FLAG;
	} else if(strcmp(mtype.s, "all") == 0) {
		flag |= DBG_MOD_ALL_FLAG;
	}

	pkg_mod_get_stats((void **)&pkg_mod_stats_list);
	shm_mod_get_stats((void **)&shm_mod_stats_list);

	/* print info about all modules */
	if(strcmp(mname.s, "all") == 0) {
		rpc_mod_print_all(
				rpc, ctx, pkg_mod_stats_list, shm_mod_stats_list, flag);

		/* print info about a particular module */
	} else {
		rpc_mod_print_one(rpc, ctx, mname.s, pkg_mod_stats_list,
				shm_mod_stats_list, flag);
	}

	pkg_mod_free_stats(pkg_mod_stats_list);
	shm_mod_free_stats(shm_mod_stats_list);
}

/**
 *
 */
static void rpc_mod_mem_stats(rpc_t *rpc, void *ctx)
{
	lock_get(kex_rpc_mod_mem_stats_lock);
	rpc_mod_mem_stats_mode(rpc, ctx, 0);
	lock_release(kex_rpc_mod_mem_stats_lock);
}

/**
 *
 */
static void rpc_mod_mem_statsx(rpc_t *rpc, void *ctx)
{
	lock_get(kex_rpc_mod_mem_stats_lock);
	rpc_mod_mem_stats_mode(rpc, ctx, DBG_MOD_INF_FLAG);
	lock_release(kex_rpc_mod_mem_stats_lock);
}


/**
 *
 */
rpc_export_t kex_mod_rpc[] = {
		{"mod.stats", rpc_mod_mem_stats, rpc_mod_mem_stats_doc, RET_ARRAY},
		{"mod.mem_stats", rpc_mod_mem_stats, rpc_mod_mem_stats_doc, RET_ARRAY},
		{"mod.mem_statsx", rpc_mod_mem_statsx, rpc_mod_mem_statsx_doc,
				RET_ARRAY},
		{0, 0, 0, 0}};

/**
 *
 */
int mod_stats_init_rpc(void)
{
	kex_rpc_mod_mem_stats_lock = lock_alloc();
	if(kex_rpc_mod_mem_stats_lock == NULL) {
		LM_ERR("failed to allocate the lock\n");
		return -1;
	}
	if(lock_init(kex_rpc_mod_mem_stats_lock) == NULL) {
		LM_ERR("failed to init the lock\n");
		return -1;
	}
	if(rpc_register_array(kex_mod_rpc) != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}
