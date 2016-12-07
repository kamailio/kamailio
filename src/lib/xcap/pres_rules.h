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

#ifndef __PRES_RULES_H
#define __PRES_RULES_H

#include <cds/sstr.h>
#include <xcap/xcap_client.h>
#include <xcap/common_policy.h>

typedef cp_ruleset_t presence_rules_t;

/* Type defining action for pres_rules */
typedef enum {
	sub_handling_block,
	sub_handling_confirm,
	sub_handling_polite_block,
	sub_handling_allow
} sub_handling_t;

int get_pres_rules(const str_t *username, const str_t *filename, xcap_query_params_t *xcap_params, cp_ruleset_t **dst);
void free_pres_rules(cp_ruleset_t *r);
void free_pres_actions(cp_actions_t *a);

/* returns 0 if rule found, 1 if not found and -1 on error */
int get_pres_rules_action(cp_ruleset_t *r, const str_t *wuri, sub_handling_t *dst_action);

#endif
