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

#ifndef __COMMON_POLICY
#define __COMMON_POLICY

#include <cds/sstr.h>
#include <time.h>

typedef struct {
	time_t from;
	time_t to;
} cp_validity_t;

typedef struct _cp_domain_t {
	struct _cp_domain_t *next;
	str_t domain;
} cp_domain_t;

typedef struct _cp_except_domain_t {
	struct _cp_except_domain_t *next;
	str_t domain;
} cp_except_domain_t;

typedef struct _cp_except_t {
	struct _cp_except_t *next;
	str_t entity;
} cp_except_t;

typedef struct _cp_unknown_t {
	struct _cp_unknown_t *next;

	char data[1]; /* elements from external schemes */
} cp_unknown_t;

typedef struct {
	cp_unknown_t *unknown; /* elements from external schemes */
} cp_actions_t;

typedef struct {
	cp_unknown_t *unknown; /* elements from external schemes */
} cp_transformations_t;

typedef struct _cp_id_t {
	struct _cp_id_t *next;
	str_t entity;
} cp_id_t;

typedef struct {
	cp_domain_t *domains;
	cp_except_domain_t *except_domains;
} cp_any_identity_t;

typedef struct {
	cp_id_t *ids;
	cp_domain_t *domains;
	cp_except_t *excepts;
	cp_any_identity_t *any_identity;
} cp_identity_t;

typedef struct _cp_sphere_t {
	struct _cp_sphere_t *next;
	str_t value;
} cp_sphere_t;

typedef struct {
	cp_validity_t *validity;
	cp_identity_t *identity;
	cp_sphere_t *spheres;
} cp_conditions_t;

typedef struct _cp_rule_t {
	struct _cp_rule_t *next;
		
	cp_conditions_t *conditions;
	cp_actions_t *actions;
	cp_transformations_t *transformations;
	str_t id;
} cp_rule_t;

typedef struct {
	cp_rule_t *rules;
} cp_ruleset_t;

cp_unknown_t *create_unknown(int data_size);

typedef void (cp_free_actions_func)(cp_actions_t *a);

void free_cp_rule(cp_rule_t *r, cp_free_actions_func free_actions);
void free_common_rules(cp_ruleset_t *r, cp_free_actions_func free_actions);
int is_rule_for_uri(cp_rule_t *rule, const str_t *uri);

#endif
