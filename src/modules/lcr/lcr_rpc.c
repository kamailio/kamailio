/*
 * Various lcr related functions :: RPC API
 *
 * Copyright (C) 2009-2010 Juha Heinanen
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

/*!
 * \file
 * \brief Kamailio lcr :: rpc API interface
 * \ingroup lcr
 * Module: \ref lcr
 */

#include "lcr_rpc.h"

#include "lcr_mod.h"
#include "../../core/ip_addr.h"


static const char *reload_doc[2] = {"Reload lcr tables from database.", 0};


static void reload(rpc_t *rpc, void *c)
{
	lock_get(reload_lock);
	if(reload_tables() != 1)
		rpc->fault(c, 500, "LCR Module Reload Failed");
	lock_release(reload_lock);
}


static const char *dump_gws_doc[2] = {"Dump the contents of lcr_gws table.", 0};


static void dump_gws(rpc_t *rpc, void *c)
{
	void *st;
	unsigned int i, j;
	str scheme, gw_name, hostname, params, transport;
	str prefix, tag;
	struct gw_info *gws;
	char buf[INT2STR_MAX_LEN], *start;
	int len;

	for(j = 1; j <= lcr_count_param; j++) {

		gws = gw_pt[j];

		for(i = 1; i <= gws[0].ip_addr.u.addr32[0]; i++) {
			if(rpc->add(c, "{", &st) < 0)
				return;
			rpc->struct_add(st, "d", "lcr_id", j);
			rpc->struct_add(st, "d", "gw_id", gws[i].gw_id);
			rpc->struct_add(st, "d", "gw_index", i);
			gw_name.s = gws[i].gw_name;
			gw_name.len = gws[i].gw_name_len;
			rpc->struct_add(st, "S", "gw_name", &gw_name);
			scheme.s = gws[i].scheme;
			scheme.len = gws[i].scheme_len;
			rpc->struct_add(st, "S", "scheme", &scheme);
			switch(gws[i].ip_addr.af) {
				case AF_INET:
					rpc->struct_printf(st, "ip_addr", "%d.%d.%d.%d",
							gws[i].ip_addr.u.addr[0], gws[i].ip_addr.u.addr[1],
							gws[i].ip_addr.u.addr[2], gws[i].ip_addr.u.addr[3]);
					break;
				case AF_INET6:
					rpc->struct_printf(st, "ip_addr", "%x:%x:%x:%x:%x:%x:%x:%x",
							gws[i].ip_addr.u.addr16[0],
							gws[i].ip_addr.u.addr16[1],
							gws[i].ip_addr.u.addr16[2],
							gws[i].ip_addr.u.addr16[3],
							gws[i].ip_addr.u.addr16[4],
							gws[i].ip_addr.u.addr16[5],
							gws[i].ip_addr.u.addr16[6],
							gws[i].ip_addr.u.addr16[7]);
					break;
				case 0:
					rpc->struct_add(st, "s", "ip_addr", "0.0.0.0");
					break;
			}
			hostname.s = gws[i].hostname;
			hostname.len = gws[i].hostname_len;
			rpc->struct_add(st, "S", "hostname", &hostname);
			rpc->struct_add(st, "d", "port", gws[i].port);
			params.s = gws[i].params;
			params.len = gws[i].params_len;
			rpc->struct_add(st, "S", "params", &params);
			transport.s = gws[i].transport;
			transport.len = gws[i].transport_len;
			rpc->struct_add(st, "S", "transport", &transport);
			prefix.s = gws[i].prefix;
			prefix.len = gws[i].prefix_len;
			tag.s = gws[i].tag;
			tag.len = gws[i].tag_len;
			start = int2strbuf(
					gws[i].defunct_until, &(buf[0]), INT2STR_MAX_LEN, &len);
			rpc->struct_add(st, "dSSdds", "strip", gws[i].strip, "prefix",
					&prefix, "tag", &tag, "flags", gws[i].flags, "state",
					gws[i].state, "defunct_until", start);
		}
	}
}


static const char *dump_rules_doc[2] = {
		"Dump the contents of the lcr_rules table.", 0};


static void dump_rules(rpc_t *rpc, void *c)
{
	int i, j;
	struct rule_info **rules, *rule;
	struct target *t;
	void *st;
	str prefix, from_uri, request_uri;

	for(j = 1; j <= lcr_count_param; j++) {

		rules = rule_pt[j];

		for(i = 0; i < lcr_rule_hash_size_param; i++) {
			rule = rules[i];
			while(rule) {
				if(rpc->add(c, "{", &st) < 0)
					return;
				prefix.s = rule->prefix;
				prefix.len = rule->prefix_len;
				from_uri.s = rule->from_uri;
				from_uri.len = rule->from_uri_len;
				request_uri.s = rule->request_uri;
				request_uri.len = rule->request_uri_len;
				rpc->struct_add(st, "ddSSSd", "lcr_id", j, "rule_id",
						rule->rule_id, "prefix", &prefix, "from_uri", &from_uri,
						"request_uri", &request_uri, "stopper", rule->stopper);
				t = rule->targets;
				while(t) {
					if(rpc->add(c, "{", &st) < 0)
						return;
					rpc->struct_add(st, "ddd", "gw_index", t->gw_index,
							"priority", t->priority, "weight", t->weight);
					t = t->next;
				}
				rule = rule->next;
			}
		}
		rule = rules[lcr_rule_hash_size_param];
		while(rule) {
			rpc->add(c, "d", rule->prefix_len);
			rule = rule->next;
		}
	}
}


static const char *defunct_gw_doc[2] = {
		"Defunct gateway until specified time (Unix timestamp).", 0};


static void defunct_gw(rpc_t *rpc, void *c)
{
	unsigned int lcr_id, gw_id, until;

	if(rpc->scan(c, "ddd", &lcr_id, &gw_id, &until) < 3) {
		rpc->fault(c, 400, "lcr_id, gw_id, and timestamp parameters required");
		return;
	}

	if(rpc_defunct_gw(lcr_id, gw_id, until) == 0) {
		rpc->fault(c, 400, "parameter value error (see syslog)");
	}

	return;
}

static const char *load_gws_doc[2] = {
		"Load matching gateways and prints their ids in priority order.  "
		"Mandatory parameters are lcr_id and uri_user followed by optional "
		"parameters caller_uri and request_uri.  Error is reported if an "
		"lcr_rule with matching prefix and from_uri has non-null request_uri "
		"and request_uri parameter has not been given.",
		0};


static void load_gws(rpc_t *rpc, void *c)
{
	unsigned int lcr_id, i;
	int gw_count, ret;
	str uri_user;
	str caller_uri;
	str request_uri;
	unsigned int gw_ids[MAX_NO_OF_GWS];

	ret = rpc->scan(c, "dS*SS", &lcr_id, &uri_user, &caller_uri, &request_uri);
	if(ret == -1) {
		rpc->fault(c, 400, "parameter error; if using cli, remember to prefix "
						   "numeric uri_user param value with 's:'");
		return;
	}

	if(ret < 4)
		request_uri.len = 0;
	if(ret < 3)
		caller_uri.len = 0;

	gw_count = load_gws_dummy(
			lcr_id, &uri_user, &caller_uri, &request_uri, &(gw_ids[0]));

	if(gw_count < 0) {
		rpc->fault(c, 400, "load_gws excution error (see syslog)");
		return;
	}

	for(i = 0; i < gw_count; i++) {
		rpc->add(c, "d", gw_ids[i]);
	}

	return;
}

/* clang-format off */
rpc_export_t lcr_rpc[] = {
    {"lcr.reload", reload, reload_doc, 0},
    {"lcr.dump_gws", dump_gws, dump_gws_doc, 0},
    {"lcr.dump_rules", dump_rules, dump_rules_doc, 0},
    {"lcr.defunct_gw", defunct_gw, defunct_gw_doc, 0},
    {"lcr.load_gws", load_gws, load_gws_doc, 0},
    {0, 0, 0, 0}
};
/* clang-format on */