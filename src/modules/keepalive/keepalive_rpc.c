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
static const char *keepalive_rpc_add_doc[2];
static const char *keepalive_rpc_del_doc[2];
static const char *keepalive_rpc_get_doc[2];
static const char *keepalive_rpc_flush_doc[2];

static void keepalive_rpc_list(rpc_t *rpc, void *ctx);
static void keepalive_rpc_add(rpc_t *rpc, void *ctx);
static void keepalive_rpc_del(rpc_t *rpc, void *ctx);
static void keepalive_rpc_get(rpc_t *rpc, void *ctx);
static void keepalive_rpc_flush(rpc_t *rpc, void *ctx);

rpc_export_t keepalive_rpc_cmds[] = {
		{"keepalive.list", keepalive_rpc_list, keepalive_rpc_list_doc, 0},
		{"keepalive.add", keepalive_rpc_add, keepalive_rpc_add_doc, 0},
		{"keepalive.del", keepalive_rpc_del, keepalive_rpc_del_doc, 0},
		{"keepalive.get", keepalive_rpc_get, keepalive_rpc_get_doc, 0},
		{"keepalive.flush", keepalive_rpc_flush, keepalive_rpc_flush_doc, 0},
		{0, 0, 0, 0}};

int ka_init_rpc(void)
{
	if(rpc_register_array(keepalive_rpc_cmds) != 0) {
		LM_ERR("failed to register RPC commands\n");
	}

	return 0;
}

static const char *keepalive_rpc_list_doc[2] = {
		"Return the content of keepalive destination groups", 0};

static void keepalive_rpc_list(rpc_t *rpc, void *ctx)
{
	void *sub;
	ka_dest_t *dest;
	char t_buf[26] = {0};

	for(dest = ka_destinations_list->first; dest != NULL; dest = dest->next) {
		rpc->add(ctx, "{", &sub);

		rpc->struct_add(sub, "SS", "uri", &dest->uri, "owner", &dest->owner);

		ctime_r(&dest->last_checked, t_buf);
		rpc->struct_add(sub, "s", "last checked", t_buf);
		ctime_r(&dest->last_up, t_buf);
		rpc->struct_add(sub, "s", "last up", t_buf);
		ctime_r(&dest->last_down, t_buf);
		rpc->struct_add(sub, "s", "last down", t_buf);
		rpc->struct_add(sub, "d", "state", (int)dest->state);
	}

	return;
}

static void keepalive_rpc_add(rpc_t *rpc, void *ctx)
{
	str sip_address = {0, 0};
	str table_name = {0, 0};
	int ret = 0;

	ret = rpc->scan(ctx, "SS", &sip_address, &table_name);

	if(ret < 2) {
		LM_ERR("not enough parameters - read so far: %d\n", ret);
		rpc->fault(ctx, 500, "Not enough parameters or wrong format");
		return;
	}

	LM_DBG("keepalive add [%.*s]\n", sip_address.len, sip_address.s);
	if(sip_address.len < 1 || table_name.len < 1) {
		LM_ERR("parameter is len less than 1  \n");
		rpc->fault(ctx, 500, "parameter is len less than 1");
		return;
	}

	if(ka_add_dest(&sip_address, &table_name, 0, ka_ping_interval, 0, 0, 0)
			< 0) {
		LM_ERR("couldn't add data to list \n");
		rpc->fault(ctx, 500, "couldn't add data to list");
		return;
	}
	rpc->rpl_printf(ctx, "Ok. Destination added.");
	return;
}
static const char *keepalive_rpc_add_doc[2] = {
		"add new destination to keepalive memory. Usage: keepalive.add "
		"sip:user@domain listname",
		0};

static void keepalive_rpc_del(rpc_t *rpc, void *ctx)
{
	str sip_address = {0, 0};
	str table_name = {0, 0};
	int ret = 0;

	ret = rpc->scan(ctx, "SS", &sip_address, &table_name);

	if(ret < 2) {
		LM_ERR("not enough parameters - read so far: %d\n", ret);
		rpc->fault(ctx, 500, "Not enough parameters or wrong format");
		return;
	}

	LM_DBG("keepalive delete [%.*s]\n", sip_address.len, sip_address.s);

	if(sip_address.len < 1 || table_name.len < 1) {
		LM_ERR("parameter is len less than 1  \n");
		rpc->fault(ctx, 500, "parameter is len less than 1");
		return;
	}

	if(ka_del_destination(&sip_address, &table_name) < 0) {
		LM_ERR("couldn't delete data from list \n");
		rpc->fault(ctx, 500, "couldn't delete data from list");
		return;
	}
	rpc->rpl_printf(ctx, "Ok. Destination removed.");
	return;
}
static const char *keepalive_rpc_del_doc[2] = {
		"delete destination from keepalive memory. Usage: keepalive.del "
		"sip:user@domain listname",
		0};

static void keepalive_rpc_get(rpc_t *rpc, void *ctx)
{
	str sip_address = {0, 0};
	str table_name = {0, 0};
	int ret = 0;
	ka_dest_t *target = 0, *head = 0;
	void *sub;

	ret = rpc->scan(ctx, "SS", &sip_address, &table_name);

	if(ret < 2) {
		LM_ERR("not enough parameters - read so far: %d\n", ret);
		rpc->fault(ctx, 500, "Not enough parameters or wrong format");
		return;
	}

	LM_DBG("keepalive get [%.*s]\n", sip_address.len, sip_address.s);

	if(sip_address.len < 1 || table_name.len < 1) {
		LM_ERR("parameter is len less than 1  \n");
		rpc->fault(ctx, 500, "parameter is len less than 1");
		return;
	}
	ka_lock_destination_list();

	if(ka_find_destination(&sip_address, &table_name, &target, &head) < 0) {
		LM_ERR("couldn't get data from list \n");
		rpc->fault(ctx, 500, "couldn't get data from list");
		ka_unlock_destination_list();

		return;
	}

	if(!target) {
		LM_ERR("Target is empty \n");
		rpc->fault(ctx, 500, "couldn't get data from list");
		ka_unlock_destination_list();
		return;
	}

	rpc->add(ctx, "{", &sub);

	rpc->struct_add(sub, "SSd", "uri", &target->uri, "owner", &target->owner,
			"state", target->state);

	ka_unlock_destination_list();

	return;
}
static const char *keepalive_rpc_get_doc[2] = {
		"get destination details from keepalive memory. Usage: keepalive.get "
		"sip:user@domain listname",
		0};


static void keepalive_rpc_flush(rpc_t *rpc, void *ctx)
{
	ka_dest_t *dest;
	LM_DBG("keepalive flush\n");
	ka_lock_destination_list();

	for(dest = ka_destinations_list->first; dest != NULL; dest = dest->next) {
		free_destination(dest);
	}
	ka_destinations_list->first = 0;
	ka_unlock_destination_list();
	rpc->rpl_printf(ctx, "Ok. Destination list flushed.");
	return;
}
static const char *keepalive_rpc_flush_doc[2] = {
		"Flush data from keepalive memory. Usage: keepalive.flush", 0};
