/*
 * Domain module
 *
 * Copyright (C) 2002-2003 Juha Heinanen
 *
 * This file is part of sip-router, a free SIP server.
 *
 * sip-router is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version
 *
 * sip-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "../../dprint.h"
#include "../../lib/srdb2/db.h"
#include "../../ut.h"
#include "../../mem/mem.h"
#include "hash.h"
#include "uid_domain_mod.h"
#include "domain_rpc.h"


static void dump_domain(rpc_t* rpc, void* ctx, domain_t* d)
{
	avp_t* a;
	void* st;
	int i;
	str* name;
	int_str val;

	if (rpc->add(ctx, "{", &st) < 0) return;
	if (rpc->struct_add(st, "S", "did", &d->did) < 0) return;

	for(i = 0; i < d->n; i++) {
		if (rpc->struct_add(st, "S", "domain", &d->domain[i]) < 0) return;
		if (rpc->struct_add(st, "d", "flags", d->flags[i]) < 0) return;
	}

	a = d->attrs;
	while(a) {
		name = get_avp_name(a);
		get_avp_val(a, &val);
		if (a->flags & AVP_VAL_STR) {
			if (rpc->struct_printf(st, "attr", "%.*s=%.*s",
								   STR_FMT(name), STR_FMT(&val.s)) < 0) return;
		} else {
			if (rpc->struct_printf(st, "attr", "%.*s=%d",
								   STR_FMT(name), val.n) < 0) return;
		}
		a = a->next;
	}
}


void dump_domain_list(rpc_t* rpc, void* ctx, domain_t* list)
{
	while(list) {
		dump_domain(rpc, ctx, list);
		list = list->next;
	}
}


static const char* domain_reload_doc[2] = {
	"Reload domain table from database",
	0
};


/*
 * Fifo function to reload domain table
 */
static void domain_reload(rpc_t* rpc, void* ctx)
{
	if (!db_mode) {
		rpc->fault(ctx, 200, "Server Domain Cache Disabled");
		return;
	}

	if (reload_domain_list() < 0) {
		rpc->fault(ctx, 400, "Domain Table Reload Failed");
	}
}



static const char* domain_dump_doc[2] = {
	"Return the contents of domain table",
	0
};


/*
 * Fifo function to print domains from current hash table
 */
static void domain_dump(rpc_t* rpc, void* ctx)
{
	domain_t* list;

	if (!db_mode) {
		rpc->fault(ctx, 400, "Server Domain Cache Disabled");
		return;
	}

	if (*active_hash == hash_1) list = *domains_1;
	else list = *domains_2;
	dump_domain_list(rpc, ctx, list);
}


rpc_export_t domain_rpc[] = {
	{"domain.reload", domain_reload, domain_reload_doc, 0},
	{"domain.dump",   domain_dump,   domain_dump_doc,   0},
	{0, 0, 0, 0}
};
