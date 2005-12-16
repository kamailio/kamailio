/*
 * $Id$
 *
 * Domain module
 *
 * Copyright (C) 2002-2003 Juha Heinanen
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "../../dprint.h"
#include "../../db/db.h"
#include "../../ut.h"
#include "../../mem/mem.h"
#include "hash.h"
#include "domain_mod.h"
#include "domain_rpc.h"


static void dump_domain(rpc_t* rpc, void* ctx, domain_t* d)
{
	char* buf, *p;
	void* st;
	int i;
	int len;
	
	if (rpc->add(ctx, "{", &st) < 0) return;
	if (rpc->struct_add(st, "S", "did", &d->did) < 0) return;
	if (rpc->struct_add(st, "d", "flags", d->flags[0]) < 0) return;

	len = 0;
	for(i = 0; i < d->n; i++) {
		len += d->domain[i].len + 1;
	}

	buf = (char*)pkg_malloc(len);
	if (!buf) {
		rpc->fault(ctx, 500, "Internal Server Error (No memory left)");
		return;
	}
	p = buf;

	for(i = 0; i < d->n; i++) {
		memcpy(p, d->domain[i].s, d->domain[i].len);
		p += d->domain[i].len;
		if (i == d->n-1) *p++ = '\0'; 
		else*p++ = ' ';
	}
	rpc->struct_add(st, "s", "domains", buf);
	pkg_free(buf);
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
	if (*active_hash == hash_1) list = *domains_1;
	else list = *domains_2;
	dump_domain_list(rpc, ctx, list);
}


rpc_export_t domain_rpc[] = {
	{"domain.reload", domain_reload, domain_reload_doc, 0},
	{"domain.dump",   domain_dump,   domain_dump_doc,   0},
	{0, 0, 0, 0}
};
