/*
 * Prefix Route Module - RPC Commands
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
 * \brief Prefix Route Module - RPC Commands
 * \ingroup prefix_route
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../str.h"
#include "../../dprint.h"
#include "../../rpc.h"
#include "tree.h"
#include "pr.h"


static const char *rpc_dump_doc[2]   = {"Dump the prefix route tree",   NULL};
static const char *rpc_reload_doc[2] = {"Reload prefix routes from DB", NULL};


/**
 * RPC command - dump prefix route tree
 */
static void rpc_dump(rpc_t *rpc, void *c)
{
	char buf[1024];
	FILE *f;

	f = tmpfile();
	if (!f) {
		rpc->fault(c, 500, "failed to open temp file");
		return;
	}

	tree_print(f);

	rewind(f);

	while (!feof(f)) {

		if (!fgets(buf, sizeof(buf), f))
			break;

		buf[strlen(buf)-1] = '\0';

		rpc->rpl_printf(c, "%s", buf);
	}

	fclose(f);
}


/**
 * RPC command - reload prefix tree from database
 */
static void rpc_reload(rpc_t *rpc, void *c)
{
	LOG(L_NOTICE, "prefix_route: Reloading prefix route tree from DB\n");

	if (0 != pr_db_load()) {
		LOG(L_ERR, "prefix_route: rpc_reload(): db_load() failed\n");
		rpc->fault(c, 400, "failed to reload prefix routes");
	}
	else {
		rpc->rpl_printf(c, "Prefix routes reloaded successfully");
	}
}


rpc_export_t pr_rpc[] = {
	{"prefix_route.reload", rpc_reload, rpc_reload_doc, 0},
	{"prefix_route.dump",   rpc_dump,   rpc_dump_doc,   0},
	{0, 0, 0, 0}
};
