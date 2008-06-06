/*
 * LDAP module - Configuration file parser
 *
 * Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2004,2005 Free Software Foundation, Inc.
 * Copyright (C) 2005,2006 iptelorg GmbH
 *
 * This file is part of ser, a free SIP server.
 *
 * SER is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * SER is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "ld_cfg.h"
#include "ld_mod.h"

#include "../../cfg_parser.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../trim.h"
#include "../../ut.h"
#include "../../resolve.h"

#include <ldap.h>
#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>


static struct ld_cfg* cfg;


void ld_cfg_free(void)
{
	struct ld_cfg* ptr;
	int i;

	while (cfg) {
		ptr = cfg;
		cfg = cfg->next;

		if (ptr->table.s) pkg_free(ptr->table.s);
		if (ptr->base.s) pkg_free(ptr->base.s);
		if (ptr->filter.s) pkg_free(ptr->filter.s);

		for(i = 0; i < ptr->n; i++) {
			if (ptr->field[i].s) pkg_free(ptr->field[i].s);
			if (ptr->attr[i].s) pkg_free(ptr->attr[i].s);
		}
		if (ptr->field) pkg_free(ptr->field);
		if (ptr->attr) pkg_free(ptr->attr);
		if (ptr->syntax) pkg_free(ptr->syntax);
	}
}


static int parse_field_map(void* param, cfg_parser_t* st, unsigned int flags)
{
	int ret;
	cfg_token_t t;
	void* ptr;
	static cfg_option_t syntaxes[] = {
		{"GeneralizedTime", .val = LD_SYNTAX_GENTIME},
		{"Integer",         .val = LD_SYNTAX_INT    },
		{"BitString",       .val = LD_SYNTAX_BIT    },
		{"Boolean",         .val = LD_SYNTAX_BOOL   },
		{"String",          .val = LD_SYNTAX_STRING },
		{"Binary",          .val = LD_SYNTAX_BIN    },
		{"Float",           .val = LD_SYNTAX_FLOAT  },
		{0}
	};

	cfg_option_t* syntax;

	if (cfg_eat_equal(st)) return -1;

	if (!(ptr = pkg_realloc(cfg->field, sizeof(str) * (cfg->n + 1)))) {
		ERR("ldap:%s:%d:%d Out of memory\n", st->file, st->line, st->col);
		return -1;
	}
	cfg->field = (str*)ptr;
	cfg->field[cfg->n].s = NULL;

	if (!(ptr = pkg_realloc(cfg->attr, sizeof(str) * (cfg->n + 1)))) {
		ERR("ldap:%s:%d:%d: Out of memory\n", st->file, st->line, st->col);
		return -1;
	}
	cfg->attr = (str*)ptr;
	cfg->attr[cfg->n].s = NULL;

	if (!(ptr = pkg_realloc(cfg->syntax, sizeof(enum ld_syntax)*(cfg->n+1)))) {
		ERR("ldap:%s:%d:%d: Out of memory\n", st->file, st->line, st->col);
		return -1;
	}
	cfg->syntax = (enum ld_syntax*)ptr;
	cfg->syntax[cfg->n] = LD_SYNTAX_STRING;
		
	cfg->n++;

	ret = cfg_get_token(&t, st, 0);
	if (ret < 0) return -1;
	if (ret == 0) {
		ERR("ldap:%s:%d:%d: Database field name expected\n", 
		    st->file, st->line, st->col);
		return -1;
	}

	if (t.type != CFG_TOKEN_ALPHA) {
		ERR("ldap:%s:%d:%d: Invalid field name format %d:'%.*s'\n", 
		    st->file, t.start.line, t.start.col,
		    t.type, STR_FMT(&t.val));
		return -1;
	}
	
	if ((cfg->field[cfg->n - 1].s = as_asciiz(&t.val)) == NULL) {
		ERR("ldap:%s:%d:%d: Out of memory\n", st->file, 
			t.start.line, t.start.col);
		return -1;
	}
	cfg->field[cfg->n - 1].len = t.val.len;

	ret = cfg_get_token(&t, st, 0);
	if (ret < 0) return -1;
	if (ret == 0) {
		ERR("ldap:%s:%d:%d: Delimiter ':' missing\n", 
		    st->file, st->line, st->col);
		return -1;
	}
	if (t.type != ':') {
		ERR("ldap:%s:%d:%d: Syntax error, ':' expected\n", 
		    st->file, t.start.line, t.start.col);
		return -1;
	}

	ret = cfg_get_token(&t, st, 0);
	if (ret < 0) return -1;
	if (ret == 0) {
		ERR("ldap:%s:%d:%d: LDAP Attribute syntax or name expected\n", 
		    st->file, st->line, st->col);
		return -1;
	}

	if (t.type == '(') {
		ret = cfg_get_token(&t, st, 0);
		if (ret < 0) return -1;
		if (ret == 0) {
			ERR("ldap:%s:%d:%d: LDAP Attribute Syntax expected\n", 
				st->file, st->line, st->col);
			return -1;
		}
		if (t.type != CFG_TOKEN_ALPHA) {
			ERR("ldap:%s:%d:%d: Invalid LDAP attribute syntax format %d:'%.*s'\n", 
				st->file, t.start.line, t.start.col,
				t.type, STR_FMT(&t.val));
			return -1;
		}
		
		if ((syntax = cfg_lookup_token(syntaxes, &t.val)) == NULL) {
			ERR("ldap:%s:%d:%d: Invalid syntaxt value '%.*s'\n", 
				st->file, t.start.line, t.start.col, STR_FMT(&t.val));
			return -1;
		}
		cfg->syntax[cfg->n - 1] = syntax->val;

		ret = cfg_get_token(&t, st, 0);
		if (ret < 0) return -1;
		if (ret == 0) {
			ERR("ldap:%s:%d:%d: Closing ')' missing in attribute syntax\n", 
				st->file, st->line, st->col);
			return -1;
		}

		if (t.type != ')') {
			ERR("ldap:%s:%d:%d: Syntax error, ')' expected\n", 
				st->file, st->line, st->col);
			return -1;
		}

		ret = cfg_get_token(&t, st, 0);
		if (ret < 0) return -1;
		if (ret == 0) {
			ERR("ldap:%s:%d:%d: LDAP Attribute name expected\n", 
				st->file, st->line, st->col);
			return -1;
		}
	}
	
	if (t.type != CFG_TOKEN_ALPHA) {
		ERR("ldap:%s:%d:%d: Invalid LDAP attribute name format %d:'%.*s'\n", 
		    st->file, t.start.line, t.start.col, t.type, STR_FMT(&t.val));
		return -1;
	}

	if ((cfg->attr[cfg->n - 1].s = as_asciiz(&t.val)) == NULL) {
		ERR("ldap:%s:%d:%d: Out of memory\n", 
		    st->file, t.start.line, t.start.col);
		return -1;
	}
	cfg->attr[cfg->n - 1].len = t.val.len;
	
	return 0;
}


static cfg_option_t scope_values[] = {
	{"base",     .val = LDAP_SCOPE_BASE    },
	{"onelevel", .val = LDAP_SCOPE_ONELEVEL},
	{"one",      .val = LDAP_SCOPE_ONELEVEL},
	{"subtree",  .val = LDAP_SCOPE_SUBTREE },
	{"sub",      .val = LDAP_SCOPE_SUBTREE },
#if defined HAVE_SCOPE_CHILDREN
	{"children", .val = LDAP_SCOPE_CHILDREN},
#endif
	{0}
};


static cfg_option_t ldap_options[] = {
	{"scope",     .param = scope_values, .f = cfg_parse_enum_val},
	{"field_map", .f = parse_field_map},
	{"filter",    .f = cfg_parse_str_val, .flags = CFG_STR_PKGMEM},
	{"base",      .f = cfg_parse_str_val, .flags = CFG_STR_PKGMEM},
	{0}
};


static int parse_table(void* param, cfg_parser_t* st, unsigned int flags)
{
	int i;
	str name, tab;
	struct ld_cfg* ptr;

	if (cfg_parse_section(&name, st, 0)) return -1;

	if ((tab.s = as_asciiz(&name)) == NULL) {
		ERR("ldap:%s:%d: Out of memory\n", st->file, st->line);
		return -1;
	}
	tab.len = name.len;;

	if ((ptr = (struct ld_cfg*)pkg_malloc(sizeof(*ptr))) == NULL) {
		ERR("ldap:%s:%d: Out of memory\n", st->file, st->line);
		goto error;
	}
	memset(ptr, '\0', sizeof(*ptr));
	ptr->table = tab;
	ptr->next = cfg;
	cfg = ptr;

	cfg_set_options(st, ldap_options);
	ldap_options[2].param = &cfg->filter;
	ldap_options[3].param = &cfg->base;
	for(i = 0; scope_values[i].name; i++) {
		scope_values[i].param = &cfg->scope;
	}
	return 0;

 error:
	if (tab.s) pkg_free(tab.s);
	return -1;
}


struct ld_cfg* ld_find_cfg(str* table)
{
	struct ld_cfg* ptr;

	ptr = cfg;
	while(ptr) {
		if (ptr->table.len == table->len &&
			!strncmp(ptr->table.s, table->s, table->len))
			return ptr;
		ptr = ptr->next;
	}
	return NULL;
}


char* ld_find_attr_name(enum ld_syntax* syntax, struct ld_cfg* cfg, char* fld_name)
{
	int i;

	for(i = 0; i < cfg->n; i++) {
		if (!strcmp(fld_name, cfg->field[i].s)) {
			*syntax = cfg->syntax[i];
			return cfg->attr[i].s;
		}
	}
	return NULL;
}



int ld_load_cfg(str* filename)
{
	cfg_parser_t* parser;
	cfg = NULL;

	if ((parser = cfg_parser_init(filename)) == NULL) {
		ERR("ldap: Error while initializing configuration file parser.\n");
		return -1;
	}

	cfg_section_parser(parser, parse_table, NULL);

	if (cfg_parse(parser)) {
		if (cfg == NULL) {
			ERR("ldap: A table name (i.e. [table_name]) is missing in the "
				"configuration file.\n");
		}
		cfg_parser_close(parser);
		ld_cfg_free();
		return -1;
	}
	cfg_parser_close(parser);
	return 0;
}
