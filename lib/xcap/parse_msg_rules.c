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

#include <xcap/parse_common_rules.h>
#include <xcap/parse_msg_rules.h>
#include <xcap/xcap_result_codes.h>

#include <cds/dstring.h>
#include <cds/memory.h>
#include <cds/logger.h>
#include <cds/list.h>
#include <string.h>

#include <xcap/xml_utils.h>

char *msg_rules_ns = NULL;

static int str2msg_handling(const char *s, msg_handling_t *dst)
{
	if (!s) return RES_INTERNAL_ERR;
	
	if (strcmp(s, "allow") == 0) {
		*dst = msg_handling_allow;
		return 0;
	}
	if (strcmp(s, "block") == 0) {
		*dst = msg_handling_block;
		return 0;
	}
/*	if (strcmp(s, "polite-block") == 0) {
		*dst = msg_handling_polite_block;
		return 0;
	}
	if (strcmp(s, "confirm") == 0) {
		*dst = msg_handling_confirm;
		return 0;
	}*/
	ERROR_LOG("invalid im-handling value: \'%s\'\n", s);
	return RES_INTERNAL_ERR;
}

static int read_msg_actions(xmlNode *an, cp_actions_t **dst)
{
	xmlNode *n;
	const char *s;
	int res = RES_OK;
	if ((!an) || (!dst)) return RES_INTERNAL_ERR;
	
	*dst = (cp_actions_t*)cds_malloc(sizeof(cp_actions_t));
	if (!(*dst)) return RES_MEMORY_ERR;
	memset(*dst, 0, sizeof(cp_actions_t));

	n = find_node(an, "im-handling", msg_rules_ns);
	if (n) {
		/* may be only one sub-handling node? */
		s = get_node_value(n);
		(*dst)->unknown = create_unknown(sizeof(msg_handling_t));
		if (!(*dst)->unknown) return RES_MEMORY_ERR;
		res = str2msg_handling(s, (msg_handling_t*)(*dst)->unknown->data);
	}

	return res;
}

int parse_msg_rules(const char *data, int dsize, cp_ruleset_t **dst)
{
	return parse_common_rules(data, dsize, dst, 
			read_msg_actions, free_msg_actions);
}

