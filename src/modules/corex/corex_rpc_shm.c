/**
 * Copyright (C) 2020 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
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
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/mem/shm.h"
#include "../../core/globals.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"

static const char* corex_rpc_shm_info_doc[2] = {
	"Return details of the shared memory manager",
	0
};


/*
 * RPC command to list the listening sockets
 */
static void corex_rpc_shm_info(rpc_t* rpc, void* ctx)
{
	void* th;

	if (rpc->add(ctx, "{", &th) < 0) {
		rpc->fault(ctx, 500, "Internal error creating rpc");
		return;
	}
	if(rpc->struct_add(th, "sj",
			"name", (_shm_root.mname)?_shm_root.mname:"unknown",
			"size", shm_mem_size) <0) {
		rpc->fault(ctx, 500, "Internal error adding fields");
		return;
	}
}

static const char* corex_rpc_shm_stats_doc[2] = {
	"Return shared memory stats",
	0
};

/*
 * RPC command to return shm stats
 */
static void corex_rpc_shm_stats(rpc_t* rpc, void* c)
{
	struct mem_info mi;
	void *th;

	shm_info(&mi);
	rpc->add(c, "{", &th);
	rpc->struct_add(th, "jjjjjj",
			"total", mi.total_size,
			"free", mi.free_size,
			"used", mi.used_size,
			"real_used", mi.real_used,
			"max_used", mi.max_used,
			"fragments", mi.total_frags
		);
}

static const char* corex_rpc_shm_report_doc[2] = {
	"Return shared memory report",
	0
};

/*
 * RPC command to return shm report
 */
static void corex_rpc_shm_report(rpc_t* rpc, void* ctx)
{
	mem_report_t mrep;
	void *th;

	if(_shm_root.xreport==NULL) {
		rpc->fault(ctx, 500, "No report callback function");
		return;
	}
	shm_report(&mrep);
	rpc->add(ctx, "{", &th);
	rpc->struct_add(th, "jjjjjjjjjjjj",
			"total_size", mrep.total_size,
			"free_size_s", mrep.free_size_s,
			"used_size_s", mrep.used_size_s,
			"real_used_s", mrep.real_used_s,
			"max_used_s", mrep.max_used_s,
			"free_frags", mrep.free_frags,
			"used_frags", mrep.used_frags,
			"total_frags", mrep.total_frags,
			"max_free_frag_size", mrep.max_free_frag_size,
			"max_used_frag_size", mrep.max_used_frag_size,
			"min_free_frag_size", mrep.min_free_frag_size,
			"min_used_frag_size", mrep.min_used_frag_size
		);
}

rpc_export_t corex_rpc_shm_cmds[] = {
	{"shm.info",   corex_rpc_shm_info,   corex_rpc_shm_info_doc,   0},
	{"shm.report", corex_rpc_shm_report, corex_rpc_shm_report_doc, 0},
	{"shm.stats",  corex_rpc_shm_stats,  corex_rpc_shm_stats_doc,  0},
	{0, 0, 0, 0}
};

/**
 * register RPC shm commands
 */
int corex_init_rpc_shm(void)
{
	if (rpc_register_array(corex_rpc_shm_cmds)!=0) {
		LM_ERR("failed to register RPC shm commands\n");
		return -1;
	}
	return 0;
}
