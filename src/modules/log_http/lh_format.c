/**
 * Copyright (C) 2019-2026 Alexey Volokitin (didww.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/**
 * Output format registry and the parser of the "format" modparam.
 * The formats themselves are implemented in the formats/ subdirectory.
 * The format and its options are set with the "format" modparam:
 * name[=>opt=val[;opt=val...]]
 */

#include <string.h>
#include <strings.h>

#include "../../core/dprint.h"
#include "../../core/mem/mem.h"
#include "../../core/str.h"
#include "../../core/ut.h"
#include "../../core/trim.h"
#include "../../core/parser/parse_param.h"

#include "lh_format.h"
#include "formats/elasticsearch.h"
#include "formats/ndjson.h"

const lh_format_t *lh_format = NULL;

/* clang-format off */
static const lh_format_t *lh_formats[] = {
	&lh_format_elasticsearch,
	&lh_format_ndjson,
	NULL
};
/* clang-format on */

static const lh_format_t *lh_format_lookup(const str *name)
{
	const lh_format_t **f;

	for(f = lh_formats; *f; f++) {
		if(name->len == (int)strlen((*f)->name)
				&& strncasecmp(name->s, (*f)->name, name->len) == 0)
			return *f;
	}
	return NULL;
}

static int lh_format_set_opt(
		const lh_format_t *f, const str *key, const str *val)
{
	const lh_format_opt_t *o;
	str *sdst;
	str tmp;
	int ival;

	for(o = f->opts; o && o->name; o++) {
		if(key->len == (int)strlen(o->name)
				&& strncasecmp(key->s, o->name, key->len) == 0)
			break;
	}
	if(!o || !o->name) {
		LM_ERR("unknown option [%.*s] for format %s\n", key->len, key->s,
				f->name);
		return -1;
	}

	switch(o->type) {
		case LH_OPT_STR:
			sdst = (str *)o->dst;
			if(sdst->s)
				pkg_free(sdst->s);
			sdst->s = pkg_malloc(val->len + 1);
			if(!sdst->s) {
				PKG_MEM_ERROR;
				sdst->len = 0;
				return -1;
			}
			memcpy(sdst->s, val->s, val->len);
			sdst->s[val->len] = '\0';
			sdst->len = val->len;
			break;
		case LH_OPT_INT:
			tmp = *val;
			if(str2sint(&tmp, &ival) < 0) {
				LM_ERR("option [%.*s] of format %s must be a number, got "
					   "[%.*s]\n",
						key->len, key->s, f->name, val->len, val->s);
				return -1;
			}
			*(int *)o->dst = ival;
			break;
		default:
			return -1;
	}
	return 0;
}

int lh_format_parse(char *val)
{
	const lh_format_t *f;
	str name, opts, rest;
	param_t *params = NULL, *pit;
	param_hooks_t phooks;
	char *p;

	if(lh_format) {
		LM_ERR("format parameter set more than once\n");
		return -1;
	}

	/* name[=>opt=val[;opt=val...]] */
	name.s = val;
	p = strstr(val, "=>");
	if(p) {
		name.len = p - val;
		opts.s = p + 2;
		opts.len = strlen(opts.s);
	} else {
		name.len = strlen(val);
		opts.s = NULL;
		opts.len = 0;
	}
	trim(&name);
	trim(&opts);

	f = lh_format_lookup(&name);
	if(!f) {
		LM_ERR("unknown format [%.*s]\n", name.len, name.s);
		return -1;
	}

	if(opts.len) {
		if(opts.s[opts.len - 1] == ';')
			opts.len--;
		rest = opts;
		if(parse_params(&rest, CLASS_ANY, &phooks, &params) < 0) {
			LM_ERR("failed to parse options [%.*s] of format %s\n", opts.len,
					opts.s, f->name);
			return -1;
		}
		/* the core parser stops at a comma, values containing commas
		 * must be quoted */
		if(rest.len) {
			LM_ERR("unexpected [%.*s] in options of format %s, quote the "
				   "values containing commas\n",
					rest.len, rest.s, f->name);
			free_params(params);
			return -1;
		}
		for(pit = params; pit; pit = pit->next) {
			if(!pit->body.len) {
				LM_ERR("option [%.*s] of format %s has no value\n",
						pit->name.len, pit->name.s, f->name);
				free_params(params);
				return -1;
			}
			if(lh_format_set_opt(f, &pit->name, &pit->body) < 0) {
				free_params(params);
				return -1;
			}
		}
		free_params(params);
	}

	lh_format = f;
	return 0;
}

int lh_format_set_default(void)
{
	if(lh_format)
		return 0;
	lh_format = lh_formats[0];
	return 0;
}

void lh_format_destroy(void)
{
	const lh_format_t **f;
	const lh_format_opt_t *o;

	for(f = lh_formats; *f; f++) {
		for(o = (*f)->opts; o && o->name; o++) {
			if(o->type == LH_OPT_STR && ((str *)o->dst)->s) {
				pkg_free(((str *)o->dst)->s);
				((str *)o->dst)->s = NULL;
				((str *)o->dst)->len = 0;
			}
		}
	}
}
