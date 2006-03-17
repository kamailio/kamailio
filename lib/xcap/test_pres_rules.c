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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libxml/parser.h>
#include <curl/curl.h>
#include <xcap/xcap_client.h>
#include <xcap/pres_rules.h>
#include <xcap/parse_pres_rules.h>
#include <xcap/common_policy.h>
#include <cds/sstr.h>
#include <cds/dstring.h>
#include <cds/memory.h>

void trace_conditions(cp_conditions_t *c)
{
	cp_sphere_t *s;
	cp_id_t *i;
	cp_domain_t *d;
	cp_except_t *e;
	
	printf(" - conditions: \n"); 
	if (!c) return;

	printf("   +- validity: "); 
	if (c->validity) 
		printf("from %s to %s", 
				ctime(&c->validity->from), 
				ctime(&c->validity->to));
	printf("\n");
	
	printf("   +- identity: \n"); 
	if (c->identity) {
		printf("      +- ids: "); 
		i = c->identity->ids;
		while (i) {
			if (i != c->identity->ids) printf(", ");
			printf("%.*s", FMT_STR(i->entity));
			i = i->next;
		}
		printf("\n");
		
		printf("      +- domains: "); 
		d = c->identity->domains;
		while (d) {
			if (d != c->identity->domains) printf(", ");
			printf("%.*s", FMT_STR(d->domain));
			d = d->next;
		}
		printf("\n");

		printf("      +- except: "); 
		e = c->identity->excepts;
		while (e) {
			if (e != c->identity->excepts) printf(", ");
			printf("%.*s", FMT_STR(e->entity));
			e = e->next;
		}
		printf("\n");
	}
	
	printf("   +- spheres: "); 
	s = c->spheres;
	while (s) {
		if (s != c->spheres) printf(", ");
		printf("%.*s", FMT_STR(s->value));
		s = s->next;
	}
	printf("\n");
}

void trace_actions(cp_actions_t *a)
{
	printf(" - actions: \n"); 
	if (!a) return;
	if (a->unknown) {
		printf("    sub-handling: ");
		sub_handling_t *sh = (sub_handling_t*)a->unknown->data;
		switch (*sh) {
			case sub_handling_block: printf("block"); break;
			case sub_handling_confirm: printf("confirm"); break;
			case sub_handling_polite_block: printf("polite block"); break;
			case sub_handling_allow: printf("allow"); break;
		}
		printf("\n");
	}
}

void trace_transformations(cp_transformations_t *c)
{
	printf(" - transformations: \n"); 
}

void trace_pres_rules(cp_ruleset_t *rules)
{
	cp_rule_t *r;
	
	if (!rules) {
		printf("null ruleset!\n");
		return;
	}

	r = rules->rules;
	while (r) {
		printf("rule \'%.*s\'\n", FMT_STR(r->id));
		trace_conditions(r->conditions);
		trace_actions(r->actions);
		trace_transformations(r->transformations);
		r = r->next;
	}
	
}
		
void test_rules(cp_ruleset_t *pres_rules, const char *uri)
{
	sub_handling_t sh;
	str_t s = zt2str((char *)uri);

	sh = sub_handling_confirm;
	get_pres_rules_action(pres_rules, &s, &sh);

	printf("rules for %s: ", uri);
	switch (sh) {
		case sub_handling_block: printf("block"); break;
		case sub_handling_confirm: printf("confirm"); break;
		case sub_handling_polite_block: printf("polite block"); break;
		case sub_handling_allow: printf("allow"); break;
	}
	printf("\n");
}

int pres_rules_test(const char *xcap_root, const char *uri)
{
	cp_ruleset_t *pres_rules = NULL;
	xcap_query_params_t xcap;
	int res;
	str_t u;
	
	u.s = (char *)uri;
	u.len = u.s ? strlen(u.s): 0;
	
	/* XCAP test */
	memset(&xcap, 0, sizeof(xcap));
	xcap.auth_user = "smith";
	xcap.auth_pass = "pass";
	xcap.enable_unverified_ssl_peer = 1;
	res = get_pres_rules(xcap_root, &u, &xcap, &pres_rules);
	if (res != 0) {
		printf("XCAP problems!\n");
		return -1;
	}

	if (pres_rules) {
		trace_pres_rules(pres_rules);
		test_rules(pres_rules, "pavel@iptel.org");
		test_rules(pres_rules, "nekdo@neco.cz");
		test_rules(pres_rules, "all:n@neco.cz");

		free_pres_rules(pres_rules);
	}
	
	return 0;
}

