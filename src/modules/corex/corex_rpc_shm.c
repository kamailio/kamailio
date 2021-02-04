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
	if(rpc->struct_add(th, "su",
			"name", (_shm_root.mname)?_shm_root.mname:"unknown",
			"size", (unsigned int)shm_mem_size) <0) {
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
	rpc->struct_add(th, "uuuuuu",
			"total", (unsigned int)(mi.total_size),
			"free", (unsigned int)(mi.free),
			"used", (unsigned int)(mi.used),
			"real_used",(unsigned int)(mi.real_used),
			"max_used", (unsigned int)(mi.max_used),
			"fragments", (unsigned int)mi.total_frags
		);
}

rpc_export_t corex_rpc_shm_cmds[] = {
	{"shm.info",  corex_rpc_shm_info,  corex_rpc_shm_info_doc,  0},
	{"shm.stats", corex_rpc_shm_stats, corex_rpc_shm_stats_doc, 0},
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
