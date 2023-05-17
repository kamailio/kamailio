/*
 * Copyright (C) 2007-2020 1&1 Internet AG
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

#include "cr_rpc.h"
#include "cr_data.h"
#include "cr_carrier.h"
#include "cr_domain.h"
#include "carrierroute.h"
#include "db_carrierroute.h"


/**
 * Defines the option set for the different fifo commands
 * Every line is for a command,
 * The first field defines the required options, the second field defines the
 * optional options and the third field defines the invalid options.
 */
static unsigned int opt_settings[5][3] = {
		{O_PREFIX | O_DOMAIN | O_HOST | O_PROB,
				O_R_PREFIX | O_R_SUFFIX | O_H_INDEX, O_NEW_TARGET},
		{O_HOST | O_DOMAIN | O_PREFIX, O_PROB,
				O_R_PREFIX | O_R_SUFFIX | O_NEW_TARGET | O_H_INDEX},
		{O_HOST | O_NEW_TARGET, O_PREFIX | O_DOMAIN | O_PROB,
				O_R_PREFIX | O_R_SUFFIX | O_H_INDEX},
		{O_HOST | O_DOMAIN | O_PREFIX, O_PROB | O_NEW_TARGET,
				O_R_PREFIX | O_R_SUFFIX | O_H_INDEX},
		{O_HOST | O_DOMAIN | O_PREFIX, O_PROB,
				O_R_PREFIX | O_R_SUFFIX | O_NEW_TARGET | O_H_INDEX}};


static void cr_rpc_reload_routes(rpc_t *rpc, void *c)
{

	if(mode == CARRIERROUTE_MODE_DB) {
		if(carrierroute_dbh == NULL) {
			carrierroute_dbh = carrierroute_dbf.init(&carrierroute_db_url);
			if(carrierroute_dbh == 0) {
				rpc->fault(c, 500,
						"Internal error -- cannot initialize database "
						"connection");
				LM_ERR("cannot initialize database connection\n");
				return;
			}
		}
	}

	if((reload_route_data()) != 0) {
		rpc->fault(c, 500, "Internal error -- failed to load routing data\n");
		LM_ERR("failed to load routing data\n");
		return;
	}
}

static const char *cr_rpc_reload_routes_doc[2] = {
		"Reload carrierroute routes", 0};


/**
 * prints the routing data
 *
 * @param rpc - RPC API structure
 * @param ctx - RPC context
 */
void cr_rpc_dump_routes(rpc_t *rpc, void *ctx)
{
	struct route_data_t *rd;
	str *tmp_str;
	str empty_str = str_init("<empty>");
	void *th;
	void *ih;
	void *dh;
	void *eh;
	void *fh;
	void *gh;
	int i, j;

	if((rd = get_data()) == NULL) {
		LM_ERR("error during retrieve data\n");
		rpc->fault(ctx, 500, "Internal error - cr data");
		return;
	}

	/* add root node */
	if(rpc->add(ctx, "{", &th) < 0) {
		rpc->fault(ctx, 500, "Internal error root reply");
		goto error;
	}
	if(rpc->struct_add(th, "[", "routes", &ih) < 0) {
		rpc->fault(ctx, 500, "Internal error - routes structure");
		goto error;
	}

	LM_DBG("start processing of data\n");
	for(i = 0; i < rd->carrier_num; i++) {
		if(rd->carriers[i]) {
			if(rpc->array_add(ih, "{", &dh) < 0) {
				LM_ERR("add carrier data failure at count %d\n", i);
				rpc->fault(ctx, 500, "Response failure - carrier data");
				goto error;
			}
			tmp_str = (rd->carriers[i] ? rd->carriers[i]->name : &empty_str);
			if(rpc->struct_add(dh, "Sd[", "carrier", tmp_str, "id",
					   (rd->carriers[i] ? rd->carriers[i]->id : 0), "domains",
					   &eh)
					< 0) {
				LM_ERR("add carrier structure failure at count %d"
					   " (carrier: %d/%.*s)\n",
						i, tmp_str->len, tmp_str->len, tmp_str->s);
				rpc->fault(ctx, 500, "Internal error - carrier structure");
				goto error;
			}

			for(j = 0; j < rd->carriers[i]->domain_num; j++) {
				if(rd->carriers[i]->domains[j]
						&& rd->carriers[i]->domains[j]->tree) {
					if(rpc->array_add(eh, "{", &fh) < 0) {
						LM_ERR("add domain data failure at count %d/%d\n", i,
								j);
						rpc->fault(ctx, 500, "Response failure - domain data");
						goto error;
					}
					tmp_str = (rd->carriers[i]->domains[j]
									   ? rd->carriers[i]->domains[j]->name
									   : &empty_str);
					if(rpc->struct_add(fh, "Sd[", "domain", tmp_str, "id",
							   rd->carriers[i]->domains[j]->id, "data", &gh)
							< 0) {
						LM_ERR("add domain structure failure at count %d/%d\n",
								i, j);
						rpc->fault(
								ctx, 500, "Internal error - domain structure");
						goto error;
					}
					if(dump_tree_recursor(rpc, ctx, gh,
							   rd->carriers[i]->domains[j]->tree, "")
							< 0) {
						LM_ERR("dump tree recursor failure at count %d/%d\n", i,
								j);
						goto error;
					}
				}
			}
		}
	}
	release_data(rd);
	return;

error:
	release_data(rd);
	return;
}

static const char *cr_rpc_dump_routes_doc[2] = {"Dump carrierroute routes", 0};


static void cr_rpc_activate_host(rpc_t *rpc, void *ctx)
{
	int ret;
	str argument;
	rpc_opt_t options;

	if(mode != CARRIERROUTE_MODE_FILE) {
		rpc->fault(ctx, 500,
				"Not running in config file mode, cannot modify route from "
				"command line");
		return;
	}


	if(rpc->scan(ctx, "S", &argument) < 1) {
		rpc->fault(ctx, 500, "Get argument failed");
		return;
	}

	if((ret = get_rpc_opts(&argument, &options, opt_settings[OPT_ACTIVATE]))
			< 0) {
		rpc->fault(ctx, 500, "Get options failed");
		return;
	}

	options.status = 1;
	options.cmd = OPT_ACTIVATE;

	if(update_route_data(&options) < 0) {
		rpc->fault(ctx, 500, "Update options failed");
		return;
	}

	rpc->add(ctx, "s", "200 ok");
	return;
}

static const char *cr_rpc_activate_host_doc[2] = {
		"Activate carrierroute host", 0};


static void cr_rpc_deactivate_host(rpc_t *rpc, void *ctx)
{
	int ret;
	str argument;
	rpc_opt_t options;

	if(mode != CARRIERROUTE_MODE_FILE) {
		rpc->fault(ctx, 500,
				"Not running in config file mode, cannot modify route from "
				"command line");
		return;
	}


	if(rpc->scan(ctx, "S", &argument) < 1) {
		rpc->fault(ctx, 500, "Get argument failed");
		return;
	}

	if((ret = get_rpc_opts(&argument, &options, opt_settings[OPT_DEACTIVATE]))
			< 0) {
		rpc->fault(ctx, 500, "Get options failed");
		return;
	}

	options.status = 0;
	options.cmd = OPT_DEACTIVATE;

	if(update_route_data(&options) < 0) {
		rpc->fault(ctx, 500, "Update options failed");
		return;
	}

	rpc->add(ctx, "s", "200 ok");
	return;
}

static const char *cr_rpc_deactivate_host_doc[2] = {
		"Deactivate carrierroute host", 0};


static void cr_rpc_add_host(rpc_t *rpc, void *ctx)
{
	int ret;
	str argument;
	rpc_opt_t options;

	if(mode != CARRIERROUTE_MODE_FILE) {
		rpc->fault(ctx, 500,
				"Not running in config file mode, cannot modify route from "
				"command line");
		return;
	}

	if(rpc->scan(ctx, "S", &argument) < 1) {
		rpc->fault(ctx, 500, "Get argument failed");
		return;
	}

	if((ret = get_rpc_opts(&argument, &options, opt_settings[OPT_ADD])) < 0) {
		rpc->fault(ctx, 500, "Get options failed");
		return;
	}

	options.status = 1;
	options.cmd = OPT_ADD;

	if(update_route_data(&options) < 0) {
		rpc->fault(ctx, 500, "Update options failed");
		return;
	}

	rpc->add(ctx, "s", "200 ok");
	return;
}

static const char *cr_rpc_add_host_doc[2] = {"Add carrierroute host", 0};


static void cr_rpc_delete_host(rpc_t *rpc, void *ctx)
{
	int ret;
	str argument;
	rpc_opt_t options;

	if(mode != CARRIERROUTE_MODE_FILE) {
		rpc->fault(ctx, 500,
				"Not running in config file mode, cannot modify route from "
				"command line");
		return;
	}

	if(rpc->scan(ctx, "S", &argument) < 1) {
		rpc->fault(ctx, 500, "Get argument failed");
		return;
	}

	if((ret = get_rpc_opts(&argument, &options, opt_settings[OPT_REMOVE]))
			< 0) {
		rpc->fault(ctx, 500, "Get options failed");
		return;
	}

	options.cmd = OPT_REMOVE;

	if(update_route_data(&options) < 0) {
		rpc->fault(ctx, 500, "Update options failed");
		return;
	}

	rpc->add(ctx, "s", "200 ok");
	return;
}

static const char *cr_rpc_delete_host_doc[2] = {"Remove carrierroute host", 0};


static void cr_rpc_replace_host(rpc_t *rpc, void *ctx)
{
	int ret;
	str argument;
	rpc_opt_t options;

	if(mode != CARRIERROUTE_MODE_FILE) {
		rpc->fault(ctx, 500,
				"Not running in config file mode, cannot modify route from "
				"command line");
		return;
	}


	if(rpc->scan(ctx, "S", &argument) < 1) {
		rpc->fault(ctx, 500, "Get argument failed");
		return;
	}

	if((ret = get_rpc_opts(&argument, &options, opt_settings[OPT_REPLACE]))
			< 0) {
		rpc->fault(ctx, 500, "Get options failed");
		return;
	}

	options.status = 1;
	options.cmd = OPT_REPLACE;

	if(update_route_data(&options) < 0) {
		rpc->fault(ctx, 500, "Update options failed");
		return;
	}

	rpc->add(ctx, "s", "200 ok");
	return;
}

static const char *cr_rpc_replace_host_doc[2] = {
		"Deactivate carrierroute host", 0};


rpc_export_t cr_rpc_methods[] = {
		{"cr.reload_routes", cr_rpc_reload_routes, cr_rpc_reload_routes_doc, 0},
		{"cr.dump_routes", cr_rpc_dump_routes, cr_rpc_dump_routes_doc, 0},
		{"cr.activate_host", cr_rpc_activate_host, cr_rpc_activate_host_doc, 0},
		{"cr.deactivate_host", cr_rpc_deactivate_host,
				cr_rpc_deactivate_host_doc, 0},
		{"cr.add_host", cr_rpc_add_host, cr_rpc_add_host_doc, 0},
		{"cr.delete_host", cr_rpc_delete_host, cr_rpc_delete_host_doc, 0},
		{"cr.replace_host", cr_rpc_replace_host, cr_rpc_replace_host_doc, 0},
		{0, 0, 0, 0}};
