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


static void dump_gw(rpc_t *rpc, void *st, struct gw_info *gw, unsigned int gw_index, unsigned int lcr_id)
{
	str scheme, gw_name, hostname, params, transport;
	str prefix, tag;
	char buf[INT2STR_MAX_LEN], *start;
	int len;

	rpc->struct_add(st, "d", "lcr_id", lcr_id);
	rpc->struct_add(st, "d", "gw_index", gw_index);
	rpc->struct_add(st, "d", "gw_id", gw->gw_id);
	gw_name.s = gw->gw_name;
	gw_name.len = gw->gw_name_len;
	rpc->struct_add(st, "S", "gw_name", &gw_name);
	scheme.s = gw->scheme;
	scheme.len = gw->scheme_len;
	rpc->struct_add(st, "S", "scheme", &scheme);
	switch(gw->ip_addr.af) {
		case AF_INET:
			rpc->struct_printf(st, "ip_addr", "%d.%d.%d.%d",
					gw->ip_addr.u.addr[0], gw->ip_addr.u.addr[1],
					gw->ip_addr.u.addr[2], gw->ip_addr.u.addr[3]);
			break;
		case AF_INET6:
			rpc->struct_printf(st, "ip_addr", "%x:%x:%x:%x:%x:%x:%x:%x",
					gw->ip_addr.u.addr16[0],
					gw->ip_addr.u.addr16[1],
					gw->ip_addr.u.addr16[2],
					gw->ip_addr.u.addr16[3],
					gw->ip_addr.u.addr16[4],
					gw->ip_addr.u.addr16[5],
					gw->ip_addr.u.addr16[6],
					gw->ip_addr.u.addr16[7]);
			break;
		case 0:
			rpc->struct_add(st, "s", "ip_addr", "0.0.0.0");
			break;
	}
	hostname.s = gw->hostname;
	hostname.len = gw->hostname_len;
	rpc->struct_add(st, "S", "hostname", &hostname);
	rpc->struct_add(st, "d", "port", gw->port);
	params.s = gw->params;
	params.len = gw->params_len;
	rpc->struct_add(st, "S", "params", &params);
	transport.s = gw->transport;
	transport.len = gw->transport_len;
	rpc->struct_add(st, "S", "transport", &transport);
	prefix.s = gw->prefix;
	prefix.len = gw->prefix_len;
	tag.s = gw->tag;
	tag.len = gw->tag_len;
	start = int2strbuf(
			gw->defunct_until, &(buf[0]), INT2STR_MAX_LEN, &len);
	rpc->struct_add(st, "dSSdds", "strip", gw->strip, "prefix",
			&prefix, "tag", &tag, "flags", gw->flags, "state",
			gw->state, "defunct_until", start);
}

static void dump_gws(rpc_t *rpc, void *c)
{
	void *st;
	void *rec = NULL;
	void *srec = NULL;
	unsigned int i, j;
	struct gw_info *gws;

	for(j = 1; j <= lcr_count_param; j++) {

		gws = gw_pt[j];

		for(i = 1; i <= gws[0].ip_addr.u.addr32[0]; i++) {
			if (srec==NULL) {
				/* We create one array per lcr_id */
				if(rpc->add(c, "{", &rec) < 0)
					return;
				if(rpc->struct_add(rec, "[", "gw", &srec) < 0)
					return;
			}
			if(rpc->array_add(srec, "{", &st) < 0)
				return;
			dump_gw(rpc, st, &gws[i], i, j);
		}
	}
}


static const char *dump_rules_doc[2] = {
		"Dump the contents of the lcr_rules table.", 0};


static void dump_rules(rpc_t *rpc, void *c)
{
	int i, j;
	int _filter_by_prefix = 0;
	int _lcr_id = 0;
	str _prefix = {NULL,0};
	struct rule_info **rules, *rule;
	struct target *t;
	void *rec = NULL;
	void *srec = NULL;
	void *st, *sst, *ssst;
	str prefix, from_uri, request_uri;

	if (rpc->scan(c, "d", &_lcr_id)>0) {
		if (rpc->scan(c, ".S", &_prefix)>0) {
			_filter_by_prefix = 1;
		}
	}

	for(j = 1; j <= lcr_count_param; j++) {

		if (_lcr_id && _lcr_id!=j) continue;

		rules = rule_pt[j];

		for(i = 0; i < lcr_rule_hash_size_param; i++) {
			rule = rules[i];
			while(rule) {
				if (_filter_by_prefix && _prefix.len && _prefix.s) {
					if (_prefix.len < rule->prefix_len ||
						strncmp(_prefix.s, rule->prefix,  rule->prefix_len)!=0) {
						rule = rule->next;
						continue;
					}
				}
				if (srec==NULL) {
					/* We create one array per lcr_id */
					if(rpc->add(c, "{", &rec) < 0)
						return;
					if(rpc->struct_add(rec, "[", "rule", &srec) < 0)
						return;
				}
				if(rpc->array_add(srec, "{", &st) < 0)
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
				if (t) {
					if (rpc->struct_add(st, "[", "gw", &sst) < 0)
						return;
					while(t) {
						if (rpc->array_add(sst, "{", &ssst) < 0)
							return;
						rpc->struct_add(ssst, "ddd", "gw_index", t->gw_index,
								"priority", t->priority, "weight", t->weight);
						t = t->next;
					}
				}
				rule = rule->next;
			}
		}

		/* Mark the end of rule array */
		srec = NULL;

		if (_filter_by_prefix)
			continue;
		rule = rules[lcr_rule_hash_size_param];
		if (rule) {
			if(rpc->struct_add(rec, "[", "prefix_len", &st) < 0)
				return;
			while(rule) {
				rpc->array_add(st, "d", rule->prefix_len);
				rule = rule->next;
			}
		}
	}
	if (rec==NULL) rpc->fault(c, 404, "Empty reply");
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
		"Load matching gateways and prints their info in priority order.  "
		"Mandatory parameters are lcr_id and uri_user followed by optional "
		"parameters caller_uri and request_uri.  Error is reported if an "
		"lcr_rule with matching prefix and from_uri has non-null request_uri "
		"and request_uri parameter has not been given.",
		0};


static void load_gws(rpc_t *rpc, void *c)
{
	unsigned int lcr_id, i, j;
	int gw_count, ret;
	str uri_user;
	str caller_uri;
	str request_uri;
	unsigned int gw_indexes[MAX_NO_OF_GWS];
	struct gw_info *gws;
	void *rec = NULL;
	void *st = NULL;

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
			lcr_id, &uri_user, &caller_uri, &request_uri, &(gw_indexes[0]));

	if(gw_count < 0) {
		rpc->fault(c, 400, "load_gws excution error (see syslog)");
		return;
	}

	gws = gw_pt[lcr_id];
	for(j = 0; j < gw_count; j++) {
		if (rec==NULL) {
			if(rpc->add(c, "[", &rec) < 0)
				return;
		}
		if(rpc->array_add(rec, "{", &st) < 0)
			return;
		i = gw_indexes[j];
		dump_gw(rpc, st, &gws[i], i, lcr_id);
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
