/**
 * keepalive module - remote destinations probing
 *
 * Copyright (C) 2017 Guillaume Bour <guillaume@bour.cc>
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
 * \ingroup keepalive
 * \brief Keepalive :: Send keepalives
 */

/*! \defgroup keepalive Keepalive :: Probing remote gateways by sending keepalives
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"

#include "keepalive.h"
#include "api.h"

static const char *keepalive_rpc_list_doc[2];
static void keepalive_rpc_list(rpc_t *rpc, void *ctx);

rpc_export_t keepalive_rpc_cmds[] = {
	{"keepalive.list", keepalive_rpc_list, keepalive_rpc_list_doc, 0},
	{0, 0, 0, 0}
};

int ka_init_rpc(void)
{
	if(rpc_register_array(keepalive_rpc_cmds) != 0) {
		LM_ERR("failed to register RPC commands\n");
	}

	return 0;
}

static const char *keepalive_rpc_list_doc[2] = {
		"Return the content of dispatcher sets", 0};

static void keepalive_rpc_list(rpc_t *rpc, void *ctx)
{
	void *sub;
	ka_dest_t *dest;
	char *_ctime;
	char *_utime;
	char *_dtime;

	for(dest = ka_destinations_list->first; dest != NULL; dest = dest->next) {
		rpc->add(ctx, "{", &sub);

		rpc->struct_add(sub, "SS", "uri", &dest->uri, "owner", &dest->owner);

		_ctime = ctime(&dest->last_checked);
		_ctime[strlen(_ctime) - 1] = '\0';
		rpc->struct_add(sub, "s", "last checked", _ctime);
		_utime = ctime(&dest->last_up);
		_utime[strlen(_utime) - 1] = '\0';
		rpc->struct_add(sub, "s", "last up", _utime);
		_dtime = ctime(&dest->last_down);
		_dtime[strlen(_dtime) - 1] = '\0';
		rpc->struct_add(sub, "s", "last down", _dtime);
	}

	return;
}
