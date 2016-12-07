/**
 * $Id$
 *
 * Copyright (C) 2011 Daniel-Constantin Mierla (asipto.com)
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

#include "../../dprint.h"
#include "../../ut.h"
#include "../../pt.h"
#include "../../cfg/cfg.h"
#include "../../cfg/cfg_ctx.h"
#include "../../socket_info.h"
#include "../../name_alias.h"
#include "../../mem/shm_mem.h"
#include "../../rpc.h"
#include "../../rpc_lookup.h"


static const char* corex_rpc_list_sockets_doc[2] = {
	"List listening sockets",
	0
};


/*
 * RPC command to list the listening sockets
 */
static void corex_rpc_list_sockets(rpc_t* rpc, void* ctx)
{
	void* th;
	void* ih;

	struct socket_info *si;
	struct socket_info** list;
	struct addr_info* ai;
	unsigned short proto;
	
	proto=PROTO_UDP;
	do {
		list=get_sock_info_list(proto);
		for(si=list?*list:0; si; si=si->next)
		{
			/* add structure node */
			if (rpc->add(ctx, "{", &th) < 0)
			{
				rpc->fault(ctx, 500, "Internal error socket structure");
				return;
			}

			if(rpc->struct_add(th, "ss{",
				"PROTO", 	get_valid_proto_name(proto),
				"NAME", 	si->name.s,
				"ADDRLIST",  &ih)<0)
			{
				rpc->fault(ctx, 500, "Internal error address list structure");
				return;
			}

			if(rpc->struct_add(ih, "s", "ADDR", si->address_str.s)<0)
			{
				rpc->fault(ctx, 500, "Internal error address structure");
				return;
			}
	
			if (si->addr_info_lst)
			{

				for (ai=si->addr_info_lst; ai; ai=ai->next)
				{
					if(rpc->struct_add(ih, "s", "ADDR", ai->address_str.s)<0)
					{
						rpc->fault(ctx, 500,
								"Internal error extra address structure");
						return;
					}

				}
			}

			if(rpc->struct_add(th, "ssss",
					"PORT", si->port_no_str.s,
					"MCAST", si->flags & SI_IS_MCAST ? "yes" : "no",
					"MHOMED", si->flags & SI_IS_MHOMED? "yes" : "no",
					"ADVERTISE", si->useinfo.name.s?si->useinfo.name.s:"-")<0)
			{
				rpc->fault(ctx, 500, "Internal error attrs structure");
				return;
			}
		}
	} while((proto=next_proto(proto)));

	return;
}


static const char* corex_rpc_list_aliases_doc[2] = {
	"List socket aliases",
	0
};


/*
 * RPC command to list the socket aliases
 */
static void corex_rpc_list_aliases(rpc_t* rpc, void* ctx)
{
	void* th;

	struct host_alias* a;

	for(a=aliases; a; a=a->next) 
	{
		/* add structure node */
		if (rpc->add(ctx, "{", &th) < 0)
		{
			rpc->fault(ctx, 500, "Internal error alias structure");
			return;
		}
		if(rpc->struct_add(th, "sSd",
				"PROTO", get_valid_proto_name(a->proto),
				"ADDR",  &a->alias,
				"PORT",  a->port)<0)
		{
			rpc->fault(ctx, 500, "Internal error alias attributes");
			return;
		}
	}

	return;
}

static const char* corex_rpc_shm_status_doc[2] = {
	"Trigger shm status dump to syslog",
	0
};

/*
 * RPC command to dump shm status to syslog
 */
static void corex_rpc_shm_status(rpc_t* rpc, void* ctx)
{
	LM_DBG("printing shared memory status report\n");
	shm_status();
}

static const char* corex_rpc_shm_summary_doc[2] = {
	"Trigger shm summary dump to syslog",
	0
};

/*
 * RPC command to dump shm summary to syslog
 */
static void corex_rpc_shm_summary(rpc_t* rpc, void* ctx)
{
	LM_DBG("printing shared memory summary report\n");
	shm_sums();
}


static const char* corex_rpc_pkg_summary_doc[2] = {
	"Trigger shm summary dump to syslog",
	0
};


static cfg_ctx_t	*_cfg_corex_ctx = NULL;

/*
 * RPC command to dump pkg summary to syslog
 */
static void corex_rpc_pkg_summary(rpc_t* rpc, void* c)
{
	str	group = str_init("core");
	str var = str_init("mem_dump_pkg");
	str sel = {0, 0};
	int	i;

	if (rpc->scan(c, "Sd", &sel, &i) < 2) {
		rpc->fault(c, 400, "Selector and value not provided");
		return;
	}

	if(sel.len!=3) {
		rpc->fault(c, 500, "Unsupported selector");
		return;
	}

	if(strncasecmp(sel.s, "idx", 3)==0) {
		if(i<0 || i>=*process_count) {
			rpc->fault(c, 500, "Index value out of range");
			return;
		}
		i = pt[i].pid;
	} else if(strncasecmp(sel.s, "pid", 3)!=0) {
		rpc->fault(c, 500, "Unsupported selector type");
		return;
	}

	if(cfg_set_now(_cfg_corex_ctx, &group, NULL, &var,
				(void *)(long)i, CFG_VAR_INT)!=0) {
		rpc->fault(c, 500, "Operation failed");
		return;
	}
}

rpc_export_t corex_rpc_cmds[] = {
	{"corex.list_sockets", corex_rpc_list_sockets,
		corex_rpc_list_sockets_doc, RET_ARRAY},
	{"corex.list_aliases", corex_rpc_list_aliases,
		corex_rpc_list_aliases_doc, RET_ARRAY},
	{"corex.shm_status", corex_rpc_shm_status,
		corex_rpc_shm_status_doc, 0},
	{"corex.shm_summary", corex_rpc_shm_summary,
		corex_rpc_shm_summary_doc, 0},
	{"corex.pkg_summary", corex_rpc_pkg_summary,
		corex_rpc_pkg_summary_doc, 0},
	{0, 0, 0, 0}
};

/**
 * register RPC commands
 */
int corex_init_rpc(void)
{
	if (cfg_register_ctx(&_cfg_corex_ctx, NULL)) {
		LOG(L_ERR, "failed to register cfg context\n");
		return -1;
	}

	if (rpc_register_array(corex_rpc_cmds)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}
