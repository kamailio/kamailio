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

#include <xcap/pres_rules.h>
#include <xcap/parse_pres_rules.h>
#include <xcap/xcap_result_codes.h>
#include <cds/dstring.h>
#include <cds/memory.h>
#include <cds/logger.h>
#include <cds/list.h>
#include <cds/sstr.h>
#include <string.h>

char *xcap_uri_for_pres_rules(const char *xcap_root, const str_t *uri)
{
	dstring_t s;
	int l;
	char *dst = NULL;

	if (!xcap_root) return NULL;
	l = strlen(xcap_root);
	dstr_init(&s, 2 * l + 32);
	dstr_append(&s, xcap_root, l);
	if (xcap_root[l - 1] != '/') dstr_append(&s, "/", 1);
	dstr_append_zt(&s, "pres-rules/users/");
	dstr_append_str(&s, uri);
	dstr_append_zt(&s, "/presence-rules.xml");
	
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

int get_pres_rules(const char *xcap_root, const str_t *uri, xcap_query_t *xcap_params, cp_ruleset_t **dst)
{
	char *data = NULL;
	int dsize = 0;
	xcap_query_t xcap;
	int res = RES_OK;
	
	if (xcap_params) {
		xcap = *xcap_params;
	}
	else memset(&xcap, 0, sizeof(xcap));
	
	xcap.uri = xcap_uri_for_pres_rules(xcap_root, uri);
	res = xcap_query(&xcap, &data, &dsize);
	if (res != RES_OK) {
		DEBUG_LOG("XCAP problems for uri \'%s\'\n", xcap.uri ? xcap.uri: "???");
		if (data) cds_free(data);
		return RES_XCAP_QUERY_ERR;
	}
	if (xcap.uri) cds_free(xcap.uri);
	
	/* parse input data */
	res = parse_pres_rules(data, dsize, dst);
	if (res != RES_OK) {
		ERROR_LOG("Error occured during document parsing!\n");
	}

	if (data) cds_free(data);
	return res;
}

static void parse_uri(const str_t *uri, str_t *user, str_t *domain)
{
	char *a;
	char *d;
	str_t s;
	
	str_clear(user);
	str_clear(domain);
	if (uri->len > 0) {
		d = str_strchr(uri, ':');
		if (d) {
			s.s = d + 1;
			s.len = uri->len - (s.s - uri->s);
		}
		else s = *uri;
		a = str_strchr(&s, '@');
		if (a) {
			user->s = s.s;
			user->len = a - s.s;
		}
		domain->s = s.s + user->len;
		if (a) domain->s++;
		domain->len = uri->len - (domain->s - uri->s);
		
/*		TRACE_LOG("parse uri \'%.*s\': user=\'%.*s\' domain=\'%.*s\'\n",
				FMT_STR(*uri), FMT_STR(*user), FMT_STR(*domain));*/
	}
}

/* returns 1 if rule is used for uri */
int is_rule_for_uri(cp_rule_t *rule, const str_t *uri)
{	
	cp_identity_t *id;
	int ok = 0;
	str_t domain, user;
	str_t d_, u_;
	cp_domain_t *d;
	cp_id_t *i;
	cp_except_t *e;
	cp_except_domain_t *ed;
	
	if (!rule) return 0;
	if (!rule->conditions) return 1; /* FIXME: ??? */
	id = rule->conditions->identity;
	if (!id) return 0;
	
	parse_uri(uri, &user, &domain);
	
	i = id->ids;
	while (i) {
		parse_uri(&i->entity, &u_, &d_);
/*		TRACE_LOG("comparing uris \'%.*s\' \'%.*s\' "
				"domains \'%.*s\' \'%.*s\'\n", 
				FMT_STR(user), FMT_STR(u_),
				FMT_STR(domain), FMT_STR(d_));*/
		if (str_case_equals(&user, &u_) == 0) {
			if (str_nocase_equals(&domain, &d_) == 0) {
/*				TRACE_LOG("id found\n");*/
				return 1;
			}
		}
		i = i->next;
	}
	
	d = id->domains;
	while (d) {
/*		TRACE_LOG("comparing domains \'%.*s\' \'%.*s\'\n",
				FMT_STR(domain), FMT_STR(d->domain));*/
		if (str_nocase_equals(&domain, &d->domain) == 0) ok = 1;
		d = d->next;
	}
	if (ok) {
		e = id->excepts;
		while (e) {
			if (str_case_equals(&user, &e->entity) == 0)
				return 0; /* excepts matched */
			e = e->next;
		}
/*		TRACE_LOG("domain found and excepts not matched\n");*/
		return 1;
	}

	if (id->any_identity) {
		d = id->any_identity->domains;
		while (d) {
			if (str_nocase_equals(&domain, &d->domain) == 0) {
/*				TRACE_LOG("domain matches for anonymous\n");*/
				return 1;
			}
			d = d->next;
		}
		
		ed = id->any_identity->except_domains;
		while (ed) {
			if (str_nocase_equals(&domain, &d->domain) == 0) return 0;
			ed = ed->next;
		}
	}
	return 0;
}

int get_pres_rules_action(cp_ruleset_t *r, const str_t *wuri, 
		sub_handling_t *dst_action)
{
	int res = 1; /* rule not found */
	cp_rule_t *rule;
	sub_handling_t a = sub_handling_block;
	sub_handling_t aa;
	
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
			aa = *(sub_handling_t*)(rule->actions->unknown->data);
			if (aa > a) a = aa;
			res = 0;
		}
		rule = rule->next;
	}
	if (dst_action && (res == 0)) *dst_action = a;
	
	return res;
}

/* ------- freeing used memory for pres-rules ------- */

static void free_identity(cp_identity_t *id)
{
	cp_id_t *i, *ni;
	cp_domain_t *d, *nd;
	cp_except_t *e, *ne;
	cp_except_domain_t *ed, *ned;
	
	if (!id) return;
	
	i = id->ids;
	while (i) {
		ni = i->next;
		str_free_content(&i->entity);
		cds_free(i);
		i = ni;
	}
	
	d = id->domains;
	while (d) {
		nd = d->next;
		str_free_content(&d->domain);
		cds_free(d);
		d = nd;
	}
	
	e = id->excepts;
	while (e) {
		ne = e->next;
		str_free_content(&e->entity);
		cds_free(e);
		e = ne;
	}

	if (id->any_identity) {
		d = id->any_identity->domains;
		while (d) {
			nd = d->next;
			str_free_content(&d->domain);
			cds_free(d);
			d = nd;
		}
		
		ed = id->any_identity->except_domains;
		while (ed) {
			ned = ed->next;
			str_free_content(&ed->domain);
			cds_free(ed);
			ed = ned;
		}
	}
	
	cds_free(id);
}

static void free_conditions(cp_conditions_t *c)
{
	cp_sphere_t *s, *n;
	if (!c) return;
	if (c->validity) cds_free(c->validity);
	if (c->identity) free_identity(c->identity);
	s = c->spheres;
	while (s) {
		n = s->next;
		str_free_content(&s->value);
		cds_free(s);
		s = n;
	}
	cds_free(c);
}

static void free_actions(cp_actions_t *a)
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

static void free_transformations(cp_transformations_t *t)
{
	cp_unknown_t *u, *nu;
	
	if (!t) return;
	
	u = t->unknown;
	while (u) {
		nu = u->next;
		cds_free(u);
		u = nu;
	}
	cds_free(t);
}

static void free_rule(cp_rule_t *r)
{
	if (!r) return;
	if (r->conditions) free_conditions(r->conditions);
	if (r->actions) free_actions(r->actions);
	if (r->transformations) free_transformations(r->transformations);
	str_free_content(&r->id);
	cds_free(r);
}

void free_pres_rules(cp_ruleset_t *r)
{
	cp_rule_t *rule, *n;
	
	if (!r) return;
	rule = r->rules;
	while (rule) {
		n = rule->next;
		free_rule(rule);
		rule = n;
	}
	cds_free(r);
}

