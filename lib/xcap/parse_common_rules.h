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

#ifndef __PARSE_COMMON_RULES_H
#define __PARSE_COMMON_RULES_H

#include <xcap/pres_rules.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

typedef int(cp_read_actions_func)(xmlNode *an, cp_actions_t **dst);

int parse_common_rules(const char *data, int dsize, cp_ruleset_t **dst,
	cp_read_actions_func read_actions, cp_free_actions_func free_actions);

/* extern char *common_policy_ns; */

#endif
