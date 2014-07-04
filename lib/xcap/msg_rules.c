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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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

int get_msg_rules(const str_t *username, const str_t *filename,
		xcap_query_params_t *xcap_params, msg_rules_t **dst)
{
	char *data = NULL;
	int dsize = 0;
	char *uri = NULL;
	int res = RES_OK;
	
	uri = xcap_uri_for_users_document(xcap_doc_im_rules,
				username, filename,
				xcap_params);
	if (!uri) {
		/* can't create XCAP uri */
		ERROR_LOG("can't build XCAP uri\n");
		return RES_XCAP_QUERY_ERR;
	}
	
	res = xcap_query(uri, xcap_params, &data, &dsize);
	if (res != RES_OK) {
		DEBUG_LOG("XCAP problems for uri \'%s\'\n", uri);
		if (data) cds_free(data);
		cds_free(uri);
		return RES_XCAP_QUERY_ERR;
	}
	cds_free(uri);
	
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

