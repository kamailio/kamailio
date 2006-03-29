/* 
 * Copyright (C) 2005 iptelorg GmbH
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

#include <xcap/msg_rules.h>
#include <xcap/parse_msg_rules.h>
#include <xcap/xcap_result_codes.h>
#include <cds/dstring.h>
#include <cds/memory.h>
#include <cds/logger.h>
#include <cds/list.h>
#include <cds/sstr.h>
#include <string.h>

char *xcap_uri_for_msg_rules(const char *xcap_root, const str_t *uri)
{
	dstring_t s;
	int l;
	char *dst = NULL;

	if (!xcap_root) return NULL;
	l = strlen(xcap_root);
	dstr_init(&s, 2 * l + 32);
	dstr_append(&s, xcap_root, l);
	if (xcap_root[l - 1] != '/') dstr_append(&s, "/", 1);
	dstr_append_zt(&s, "im-rules/users/");
	dstr_append_str(&s, uri);
	dstr_append_zt(&s, "/im-rules.xml");
	
	l = dstr_get_data_length(&s);
	if (l > 0) {
		dst = (char *)cds_malloc(l + 1);
		if (dst) {
			dstr_get_data(&s, dst);
			dst[l] = 0;
		}
	}
	dstr_destroy(&s);
	return dst;
}

int get_msg_rules(const char *xcap_root, const str_t *uri, xcap_query_params_t *xcap_params, msg_rules_t **dst)
{
	char *data = NULL;
	int dsize = 0;
	char *xcap_uri;
	int res = RES_OK;
	
	xcap_uri = xcap_uri_for_msg_rules(xcap_root, uri);
	res = xcap_query(xcap_uri, xcap_params, &data, &dsize);
	if (res != RES_OK) {
		TRACE_LOG("XCAP problems for uri \'%s\'\n", xcap_uri ? xcap_uri: "???");
		if (data) cds_free(data);
		if (xcap_uri) cds_free(xcap_uri);
		return RES_XCAP_QUERY_ERR;
	}
	if (xcap_uri) cds_free(xcap_uri);
	
	/* parse input data */
	res = parse_msg_rules(data, dsize, dst);
	if (res != RES_OK) {
		ERROR_LOG("Error occured during document parsing!\n");
	}

	if (data) cds_free(data);
	return res;
}

int get_msg_rules_action(cp_ruleset_t *r, const str_t *wuri, 
		msg_handling_t *dst_action)
{
	int res = 1; /* rule not found */
	cp_rule_t *rule;
	msg_handling_t a = msg_handling_block;
	msg_handling_t aa;
	
	if (!r) return -1;
	
	rule = r->rules;
	while (rule) {
		DEBUG_LOG("TRYING rule %.*s for uri %.*s\n", 
					FMT_STR(rule->id), FMT_STR(*wuri));
		if (is_rule_for_uri(rule, wuri)) {
			DEBUG_LOG("rule %.*s matches for uri %.*s\n", 
					FMT_STR(rule->id), FMT_STR(*wuri));

			if (!rule->actions) continue;
			if (!rule->actions->unknown) continue;
			aa = *(msg_handling_t*)(rule->actions->unknown->data);
			if (aa > a) a = aa;
			res = 0;
		}
		rule = rule->next;
	}
	if (dst_action && (res == 0)) *dst_action = a;
	
	return res;
}

void free_msg_actions(cp_actions_t *a)
{
	cp_unknown_t *u, *nu;
	
	if (!a) return;
	
	u = a->unknown;
	while (u) {
		nu = u->next;
		cds_free(u);
		u = nu;
	}
	cds_free(a);
}

void free_msg_rules(cp_ruleset_t *r)
{
	free_common_rules(r, free_msg_actions);
}

