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
	start = int2strbuf(gw->defunct_until, &(buf[0]), INT2STR_MAX_LEN, &len);
	rpc->struct_add(st, "dSSdds", "strip", gw->strip, "prefix", &prefix, "tag",
			&tag, "flags", gw->flags, "state", gw->state, "defunct_until",
			start);
}

static void print_gw_stat(rpc_t *rpc, void *st, struct gw_info *gw)
{
	str gw_name;

	rpc->struct_add(st, "d", "gw_id", gw->gw_id);
	gw_name.s = gw->gw_name;
	gw_name.len = gw->gw_name_len;
	rpc->struct_add(st, "S", "gw_name", &gw_name);

	rpc->struct_add(st, "d", "requests", gw->rcv_gw_reqs);
	rpc->struct_add(st, "d", "requests_invite", gw->rcv_gw_reqs_invite);
	rpc->struct_add(st, "d", "requests_cancel", gw->rcv_gw_reqs_cancel);
	rpc->struct_add(st, "d", "requests_ack", gw->rcv_gw_reqs_ack);
	rpc->struct_add(st, "d", "requests_bye", gw->rcv_gw_reqs_bye);
	rpc->struct_add(st, "d", "requests_info", gw->rcv_gw_reqs_info);
	rpc->struct_add(st, "d", "requests_register", gw->rcv_gw_reqs_register);
	rpc->struct_add(st, "d", "requests_subscribe", gw->rcv_gw_reqs_subscribe);
	rpc->struct_add(st, "d", "requests_notify", gw->rcv_gw_reqs_notify);
	rpc->struct_add(st, "d", "requests_message", gw->rcv_gw_reqs_message);
	rpc->struct_add(st, "d", "requests_options", gw->rcv_gw_reqs_options);
	rpc->struct_add(st, "d", "requests_prack", gw->rcv_gw_reqs_prack);
	rpc->struct_add(st, "d", "requests_update", gw->rcv_gw_reqs_update);
	rpc->struct_add(st, "d", "requests_refer", gw->rcv_gw_reqs_refer);
	rpc->struct_add(st, "d", "requests_publish", gw->rcv_gw_reqs_publish);
	rpc->struct_add(st, "d", "requests_other", gw->rcv_gw_reqs_other);

	rpc->struct_add(st, "d", "replies", gw->rcv_gw_rpl);
	rpc->struct_add(st, "d", "replies_invite", gw->rcv_gw_rpl_invite);
	rpc->struct_add(
			st, "d", "replies_1xx_invite", gw->rcv_gw_rpl_invite_by_method[0]);
	rpc->struct_add(
			st, "d", "replies_2xx_invite", gw->rcv_gw_rpl_invite_by_method[1]);
	rpc->struct_add(
			st, "d", "replies_3xx_invite", gw->rcv_gw_rpl_invite_by_method[2]);
	rpc->struct_add(
			st, "d", "replies_4xx_invite", gw->rcv_gw_rpl_invite_by_method[3]);
	rpc->struct_add(
			st, "d", "replies_5xx_invite", gw->rcv_gw_rpl_invite_by_method[4]);
	rpc->struct_add(
			st, "d", "replies_6xx_invite", gw->rcv_gw_rpl_invite_by_method[5]);

	rpc->struct_add(st, "d", "replies_cancel", gw->rcv_gw_rpl_cancel);
	rpc->struct_add(
			st, "d", "replies_1xx_cancel", gw->rcv_gw_rpl_cancel_by_method[0]);
	rpc->struct_add(
			st, "d", "replies_2xx_cancel", gw->rcv_gw_rpl_cancel_by_method[1]);
	rpc->struct_add(
			st, "d", "replies_3xx_cancel", gw->rcv_gw_rpl_cancel_by_method[2]);
	rpc->struct_add(
			st, "d", "replies_4xx_cancel", gw->rcv_gw_rpl_cancel_by_method[3]);
	rpc->struct_add(
			st, "d", "replies_5xx_cancel", gw->rcv_gw_rpl_cancel_by_method[4]);
	rpc->struct_add(
			st, "d", "replies_6xx_cancel", gw->rcv_gw_rpl_cancel_by_method[5]);

	rpc->struct_add(st, "d", "replies_bye", gw->rcv_gw_rpl_bye);
	rpc->struct_add(
			st, "d", "replies_1xx_bye", gw->rcv_gw_rpl_bye_by_method[0]);
	rpc->struct_add(
			st, "d", "replies_2xx_bye", gw->rcv_gw_rpl_bye_by_method[1]);
	rpc->struct_add(
			st, "d", "replies_3xx_bye", gw->rcv_gw_rpl_bye_by_method[2]);
	rpc->struct_add(
			st, "d", "replies_4xx_bye", gw->rcv_gw_rpl_bye_by_method[3]);
	rpc->struct_add(
			st, "d", "replies_5xx_bye", gw->rcv_gw_rpl_bye_by_method[4]);
	rpc->struct_add(
			st, "d", "replies_6xx_bye", gw->rcv_gw_rpl_bye_by_method[5]);

	rpc->struct_add(st, "d", "replies_register", gw->rcv_gw_rpl_register);
	rpc->struct_add(st, "d", "replies_1xx_register",
			gw->rcv_gw_rpl_register_by_method[0]);
	rpc->struct_add(st, "d", "replies_2xx_register",
			gw->rcv_gw_rpl_register_by_method[1]);
	rpc->struct_add(st, "d", "replies_3xx_register",
			gw->rcv_gw_rpl_register_by_method[2]);
	rpc->struct_add(st, "d", "replies_4xx_register",
			gw->rcv_gw_rpl_register_by_method[3]);
	rpc->struct_add(st, "d", "replies_5xx_register",
			gw->rcv_gw_rpl_register_by_method[4]);
	rpc->struct_add(st, "d", "replies_6xx_register",
			gw->rcv_gw_rpl_register_by_method[5]);

	rpc->struct_add(st, "d", "replies_message", gw->rcv_gw_rpl_message);
	rpc->struct_add(st, "d", "replies_1xx_message",
			gw->rcv_gw_rpl_message_by_method[0]);
	rpc->struct_add(st, "d", "replies_2xx_message",
			gw->rcv_gw_rpl_message_by_method[1]);
	rpc->struct_add(st, "d", "replies_3xx_message",
			gw->rcv_gw_rpl_message_by_method[2]);
	rpc->struct_add(st, "d", "replies_4xx_message",
			gw->rcv_gw_rpl_message_by_method[3]);
	rpc->struct_add(st, "d", "replies_5xx_message",
			gw->rcv_gw_rpl_message_by_method[4]);
	rpc->struct_add(st, "d", "replies_6xx_message",
			gw->rcv_gw_rpl_message_by_method[5]);

	rpc->struct_add(st, "d", "replies_prack", gw->rcv_gw_rpl_prack);
	rpc->struct_add(
			st, "d", "replies_1xx_prack", gw->rcv_gw_rpl_prack_by_method[0]);
	rpc->struct_add(
			st, "d", "replies_2xx_prack", gw->rcv_gw_rpl_prack_by_method[1]);
	rpc->struct_add(
			st, "d", "replies_3xx_prack", gw->rcv_gw_rpl_prack_by_method[2]);
	rpc->struct_add(
			st, "d", "replies_4xx_prack", gw->rcv_gw_rpl_prack_by_method[3]);
	rpc->struct_add(
			st, "d", "replies_5xx_prack", gw->rcv_gw_rpl_prack_by_method[4]);
	rpc->struct_add(
			st, "d", "replies_6xx_prack", gw->rcv_gw_rpl_prack_by_method[5]);

	rpc->struct_add(st, "d", "replies_update", gw->rcv_gw_rpl_update);
	rpc->struct_add(
			st, "d", "replies_1xx_update", gw->rcv_gw_rpl_update_by_method[0]);
	rpc->struct_add(
			st, "d", "replies_2xx_update", gw->rcv_gw_rpl_update_by_method[1]);
	rpc->struct_add(
			st, "d", "replies_3xx_update", gw->rcv_gw_rpl_update_by_method[2]);
	rpc->struct_add(
			st, "d", "replies_4xx_update", gw->rcv_gw_rpl_update_by_method[3]);
	rpc->struct_add(
			st, "d", "replies_5xx_update", gw->rcv_gw_rpl_update_by_method[4]);
	rpc->struct_add(
			st, "d", "replies_6xx_update", gw->rcv_gw_rpl_update_by_method[5]);

	rpc->struct_add(st, "d", "replies_refer", gw->rcv_gw_rpl_refer);
	rpc->struct_add(
			st, "d", "replies_1xx_refer", gw->rcv_gw_rpl_refer_by_method[0]);
	rpc->struct_add(
			st, "d", "replies_2xx_refer", gw->rcv_gw_rpl_refer_by_method[1]);
	rpc->struct_add(
			st, "d", "replies_3xx_refer", gw->rcv_gw_rpl_refer_by_method[2]);
	rpc->struct_add(
			st, "d", "replies_4xx_refer", gw->rcv_gw_rpl_refer_by_method[3]);
	rpc->struct_add(
			st, "d", "replies_5xx_refer", gw->rcv_gw_rpl_refer_by_method[4]);
	rpc->struct_add(
			st, "d", "replies_6xx_refer", gw->rcv_gw_rpl_refer_by_method[5]);

	rpc->struct_add(st, "d", "replies_1xx", gw->rcv_gw_rpls_1xx);
	rpc->struct_add(st, "d", "replies_18x", gw->rcv_gw_rpls_18x);
	rpc->struct_add(st, "d", "replies_2xx", gw->rcv_gw_rpls_2xx);
	rpc->struct_add(st, "d", "replies_3xx", gw->rcv_gw_rpls_3xx);
	rpc->struct_add(st, "d", "replies_4xx", gw->rcv_gw_rpls_4xx);
	rpc->struct_add(st, "d", "replies_401", gw->rcv_gw_rpls_401);
	rpc->struct_add(st, "d", "replies_404", gw->rcv_gw_rpls_404);
	rpc->struct_add(st, "d", "replies_407", gw->rcv_gw_rpls_407);
	rpc->struct_add(st, "d", "replies_480", gw->rcv_gw_rpls_480);
	rpc->struct_add(st, "d", "replies_486", gw->rcv_gw_rpls_486);
	rpc->struct_add(st, "d", "replies_5xx", gw->rcv_gw_rpls_5xx);
	rpc->struct_add(st, "d", "replies_6xx", gw->rcv_gw_rpls_6xx);
}

static void dump_gw_counters(rpc_t *rpc, void *st, struct gw_info *gw,
		unsigned int gw_index, unsigned int lcr_id)
{
	print_gw_stat(rpc, st, gw);
}

static void reset_gw_counters(rpc_t *rpc, void *st, struct gw_info *gw,
		unsigned int gw_index, unsigned int lcr_id)
{
	reset_gw_stats(gw);
	print_gw_stat(rpc, st, gw);
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

static const char *lcr_reset_stats_doc[2] = {"Reset stats for gws.", 0};

static void lcr_reset_stats(rpc_t *rpc, void *ctx)
{
	int input_gw = 0;

	if(!lcr_stats_flag) {
		rpc->fault(ctx, 500, "lcr stat module not enabled");
		return;
	}

	if(rpc->scan(ctx, "*d", &input_gw) == -1) {
		rpc->fault(ctx, 500, "invalid parameters");
		return;
	}

	void *st;
	void *rec = NULL;
	void *srec = NULL;
	unsigned int i, j;
	struct gw_info *gws;

	for(j = 1; j <= lcr_count_param; j++) {

		gws = gw_pt[j];

		for(i = 1; i <= gws[0].ip_addr.u.addr32[0]; i++) {
			if(!input_gw || input_gw == gws[i].gw_id) {
				if(srec == NULL) {
					/* We create one array per lcr_id */
					if(rpc->add(ctx, "{", &rec) < 0)
						return;
					if(rpc->struct_add(rec, "[", "gw", &srec) < 0)
						return;
				}
				if(rpc->array_add(srec, "{", &st) < 0)
					return;
				reset_gw_counters(rpc, st, &gws[i], i, j);
			}
		}
	}

	return;
}

static const char *lcr_print_stats_doc[2] = {"Print stats for gws.", 0};

static void lcr_print_stats(rpc_t *rpc, void *ctx)
{
	int input_gw = 0;
	void *st;
	void *rec = NULL;
	void *srec = NULL;
	unsigned int i, j;
	struct gw_info *gws;

	if(!lcr_stats_flag) {
		rpc->fault(ctx, 500, "lcr stat module not enabled");
		return;
	}

	rpc->scan(ctx, "*d", &input_gw);

	for(j = 1; j <= lcr_count_param; j++) {

		gws = gw_pt[j];

		for(i = 1; i <= gws[0].ip_addr.u.addr32[0]; i++) {
			if(!input_gw || input_gw == gws[i].gw_id) {
				if(srec == NULL) {
					/* We create one array per lcr_id */
					if(rpc->add(ctx, "{", &rec) < 0)
						return;
					if(rpc->struct_add(rec, "[", "gw", &srec) < 0)
						return;
				}
				if(rpc->array_add(srec, "{", &st) < 0)
					return;
				dump_gw_counters(rpc, st, &gws[i], i, j);
			}
		}
	}

	// if no gw return empty array
	if(srec == NULL) {
		rpc->add(ctx, "{", &rec);
	}
	return;
}

#define CREATE_RPC_ENTRY(var1, var2)                                       \
	snprintf(param, sizeof(param), "%s%u:%s = %lu", "gw", gw->gw_id, var1, \
			var2);                                                         \
	paramstr.s = param;                                                    \
	paramstr.len = strlen(param);                                          \
	if(rpc->array_add(rec, "S", &paramstr) < 0) {                          \
		rpc->fault(ctx, 500, "cant create array element");                 \
		return;                                                            \
	}

static const char *lcr_get_statistics_doc[2] = {
		"Print stats for gws as string value.", 0};

static void lcr_get_statistics(rpc_t *rpc, void *ctx)
{
	int input_gw = 0;
	unsigned int i, j;
	struct gw_info *gws;
	struct gw_info *gw;
	void *rec = NULL;
	char param[100];
	str paramstr;

	if(!lcr_stats_flag) {
		rpc->fault(ctx, 500, "lcr stat module not enabled");
		return;
	}

	rpc->scan(ctx, "*d", &input_gw);

	if(rpc->add(ctx, "[", &rec) < 0) {
		rpc->fault(ctx, 500, "cant create rec");
		return;
	}

	for(j = 1; j <= lcr_count_param; j++) {

		gws = gw_pt[j];

		for(i = 1; i <= gws[0].ip_addr.u.addr32[0]; i++) {
			if(!input_gw || input_gw == gws[i].gw_id) {
				gw = &gws[i];

				CREATE_RPC_ENTRY("request", gw->rcv_gw_reqs);
				CREATE_RPC_ENTRY("requests_invite", gw->rcv_gw_reqs_invite);
				CREATE_RPC_ENTRY("requests_cancel", gw->rcv_gw_reqs_cancel);
				CREATE_RPC_ENTRY("requests_ack", gw->rcv_gw_reqs_ack);
				CREATE_RPC_ENTRY("requests_bye", gw->rcv_gw_reqs_bye);
				CREATE_RPC_ENTRY("requests_info", gw->rcv_gw_reqs_info);
				CREATE_RPC_ENTRY("requests_register", gw->rcv_gw_reqs_register);
				CREATE_RPC_ENTRY(
						"requests_subscribe", gw->rcv_gw_reqs_subscribe);
				CREATE_RPC_ENTRY("requests_notify", gw->rcv_gw_reqs_notify);
				CREATE_RPC_ENTRY("requests_message", gw->rcv_gw_reqs_message);
				CREATE_RPC_ENTRY("requests_options", gw->rcv_gw_reqs_options);
				CREATE_RPC_ENTRY("requests_prack", gw->rcv_gw_reqs_prack);
				CREATE_RPC_ENTRY("requests_update", gw->rcv_gw_reqs_update);
				CREATE_RPC_ENTRY("requests_refer", gw->rcv_gw_reqs_refer);
				CREATE_RPC_ENTRY("requests_publish", gw->rcv_gw_reqs_publish);
				CREATE_RPC_ENTRY("requests_other", gw->rcv_gw_reqs_other);

				CREATE_RPC_ENTRY("replies", gw->rcv_gw_rpl);
				CREATE_RPC_ENTRY("replies_invite", gw->rcv_gw_rpl_invite);
				CREATE_RPC_ENTRY("replies_1XX_invite",
						gw->rcv_gw_rpl_invite_by_method[0]);
				CREATE_RPC_ENTRY("replies_2XX_invite",
						gw->rcv_gw_rpl_invite_by_method[1]);
				CREATE_RPC_ENTRY("replies_3XX_invite",
						gw->rcv_gw_rpl_invite_by_method[2]);
				CREATE_RPC_ENTRY("replies_4XX_invite",
						gw->rcv_gw_rpl_invite_by_method[3]);
				CREATE_RPC_ENTRY("replies_5XX_invite",
						gw->rcv_gw_rpl_invite_by_method[4]);
				CREATE_RPC_ENTRY("replies_6XX_invite",
						gw->rcv_gw_rpl_invite_by_method[5]);

				CREATE_RPC_ENTRY("replies_cancel", gw->rcv_gw_rpl_cancel);
				CREATE_RPC_ENTRY("replies_1XX_cancel",
						gw->rcv_gw_rpl_cancel_by_method[0]);
				CREATE_RPC_ENTRY("replies_2XX_cancel",
						gw->rcv_gw_rpl_cancel_by_method[1]);
				CREATE_RPC_ENTRY("replies_3XX_cancel",
						gw->rcv_gw_rpl_cancel_by_method[2]);
				CREATE_RPC_ENTRY("replies_4XX_cancel",
						gw->rcv_gw_rpl_cancel_by_method[3]);
				CREATE_RPC_ENTRY("replies_5XX_cancel",
						gw->rcv_gw_rpl_cancel_by_method[4]);
				CREATE_RPC_ENTRY("replies_6XX_cancel",
						gw->rcv_gw_rpl_cancel_by_method[5]);

				CREATE_RPC_ENTRY("replies_bye", gw->rcv_gw_rpl_bye);
				CREATE_RPC_ENTRY(
						"replies_1XX_bye", gw->rcv_gw_rpl_bye_by_method[0]);
				CREATE_RPC_ENTRY(
						"replies_2XX_bye", gw->rcv_gw_rpl_bye_by_method[1]);
				CREATE_RPC_ENTRY(
						"replies_3XX_bye", gw->rcv_gw_rpl_bye_by_method[2]);
				CREATE_RPC_ENTRY(
						"replies_4XX_bye", gw->rcv_gw_rpl_bye_by_method[3]);
				CREATE_RPC_ENTRY(
						"replies_5XX_bye", gw->rcv_gw_rpl_bye_by_method[4]);
				CREATE_RPC_ENTRY(
						"replies_6XX_bye", gw->rcv_gw_rpl_bye_by_method[5]);

				CREATE_RPC_ENTRY("replies_register", gw->rcv_gw_rpl_register);
				CREATE_RPC_ENTRY("replies_1XX_register",
						gw->rcv_gw_rpl_register_by_method[0]);
				CREATE_RPC_ENTRY("replies_2XX_register",
						gw->rcv_gw_rpl_register_by_method[1]);
				CREATE_RPC_ENTRY("replies_3XX_register",
						gw->rcv_gw_rpl_register_by_method[2]);
				CREATE_RPC_ENTRY("replies_4XX_register",
						gw->rcv_gw_rpl_register_by_method[3]);
				CREATE_RPC_ENTRY("replies_5XX_register",
						gw->rcv_gw_rpl_register_by_method[4]);
				CREATE_RPC_ENTRY("replies_6XX_register",
						gw->rcv_gw_rpl_register_by_method[5]);

				CREATE_RPC_ENTRY("replies_message", gw->rcv_gw_rpl_message);
				CREATE_RPC_ENTRY("replies_1XX_message",
						gw->rcv_gw_rpl_message_by_method[0]);
				CREATE_RPC_ENTRY("replies_2XX_message",
						gw->rcv_gw_rpl_message_by_method[1]);
				CREATE_RPC_ENTRY("replies_3XX_message",
						gw->rcv_gw_rpl_message_by_method[2]);
				CREATE_RPC_ENTRY("replies_4XX_message",
						gw->rcv_gw_rpl_message_by_method[3]);
				CREATE_RPC_ENTRY("replies_5XX_message",
						gw->rcv_gw_rpl_message_by_method[4]);
				CREATE_RPC_ENTRY("replies_6XX_message",
						gw->rcv_gw_rpl_message_by_method[5]);

				CREATE_RPC_ENTRY("replies_prack", gw->rcv_gw_rpl_prack);
				CREATE_RPC_ENTRY(
						"replies_1XX_prack", gw->rcv_gw_rpl_prack_by_method[0]);
				CREATE_RPC_ENTRY(
						"replies_2XX_prack", gw->rcv_gw_rpl_prack_by_method[1]);
				CREATE_RPC_ENTRY(
						"replies_3XX_prack", gw->rcv_gw_rpl_prack_by_method[2]);
				CREATE_RPC_ENTRY(
						"replies_4XX_prack", gw->rcv_gw_rpl_prack_by_method[3]);
				CREATE_RPC_ENTRY(
						"replies_5XX_prack", gw->rcv_gw_rpl_prack_by_method[4]);
				CREATE_RPC_ENTRY(
						"replies_6XX_prack", gw->rcv_gw_rpl_prack_by_method[5]);

				CREATE_RPC_ENTRY("replies_update", gw->rcv_gw_rpl_update);
				CREATE_RPC_ENTRY("replies_1XX_update",
						gw->rcv_gw_rpl_update_by_method[0]);
				CREATE_RPC_ENTRY("replies_2XX_update",
						gw->rcv_gw_rpl_update_by_method[1]);
				CREATE_RPC_ENTRY("replies_3XX_update",
						gw->rcv_gw_rpl_update_by_method[2]);
				CREATE_RPC_ENTRY("replies_4XX_update",
						gw->rcv_gw_rpl_update_by_method[3]);
				CREATE_RPC_ENTRY("replies_5XX_update",
						gw->rcv_gw_rpl_update_by_method[4]);
				CREATE_RPC_ENTRY("replies_6XX_update",
						gw->rcv_gw_rpl_update_by_method[5]);

				CREATE_RPC_ENTRY("replies_refer", gw->rcv_gw_rpl_refer);
				CREATE_RPC_ENTRY(
						"replies_1XX_refer", gw->rcv_gw_rpl_refer_by_method[0]);
				CREATE_RPC_ENTRY(
						"replies_2XX_refer", gw->rcv_gw_rpl_refer_by_method[1]);
				CREATE_RPC_ENTRY(
						"replies_3XX_refer", gw->rcv_gw_rpl_refer_by_method[2]);
				CREATE_RPC_ENTRY(
						"replies_4XX_refer", gw->rcv_gw_rpl_refer_by_method[3]);
				CREATE_RPC_ENTRY(
						"replies_5XX_refer", gw->rcv_gw_rpl_refer_by_method[4]);
				CREATE_RPC_ENTRY(
						"replies_6XX_refer", gw->rcv_gw_rpl_refer_by_method[5]);

				CREATE_RPC_ENTRY("replies_1xx", gw->rcv_gw_rpls_1xx);
				CREATE_RPC_ENTRY("replies_18x", gw->rcv_gw_rpls_18x);
				CREATE_RPC_ENTRY("replies_2xx", gw->rcv_gw_rpls_2xx);
				CREATE_RPC_ENTRY("replies_3xx", gw->rcv_gw_rpls_3xx);
				CREATE_RPC_ENTRY("replies_4xx", gw->rcv_gw_rpls_4xx);
				CREATE_RPC_ENTRY("replies_401", gw->rcv_gw_rpls_401);
				CREATE_RPC_ENTRY("replies_404", gw->rcv_gw_rpls_404);
				CREATE_RPC_ENTRY("replies_407", gw->rcv_gw_rpls_407);
				CREATE_RPC_ENTRY("replies_480", gw->rcv_gw_rpls_480);
				CREATE_RPC_ENTRY("replies_486", gw->rcv_gw_rpls_486);
				CREATE_RPC_ENTRY("replies_5xx", gw->rcv_gw_rpls_5xx);
				CREATE_RPC_ENTRY("replies_6xx", gw->rcv_gw_rpls_6xx);
			}
		}
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
    {"lcr.stats", lcr_print_stats, lcr_print_stats_doc, 0},
    {"lcr.reset_stats", lcr_reset_stats, lcr_reset_stats_doc, 0},
    {"lcr.get_statistics", lcr_get_statistics, lcr_get_statistics_doc, 0},
    {0, 0, 0, 0}
};
/* clang-format on */
