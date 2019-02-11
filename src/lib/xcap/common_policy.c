#include <xcap/common_policy.h>

cp_unknown_t *create_unknown(int data_size)
{
	cp_unknown_t *u = cds_malloc(sizeof(cp_unknown_t) + data_size);
	u->next = NULL;
	return u;
}

/* ------- freeing used memory for common-rules ------- */

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

void free_cp_rule(cp_rule_t *r, cp_free_actions_func free_actions)
{
	if (!r) return;
	if (r->conditions) free_conditions(r->conditions);
	if (r->actions) free_actions(r->actions);
	if (r->transformations) free_transformations(r->transformations);
	str_free_content(&r->id);
	cds_free(r);
}

void free_common_rules(cp_ruleset_t *r, cp_free_actions_func free_actions)
{
	cp_rule_t *rule, *n;
	
	if (!r) return;
	rule = r->rules;
	while (rule) {
		n = rule->next;
		free_cp_rule(rule, free_actions);
		rule = n;
	}
	cds_free(r);
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

