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
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "ld_cfg.h"
#include "ld_mod.h"
#include "ld_uri.h"

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


enum section_type {
	LDAP_CON_SECTION = 0,
	LDAP_TABLE_SECTION
};

static struct ld_cfg* cfg = NULL;

static struct ld_con_info* con = NULL;


void ld_cfg_free(void)
{
	struct ld_con_info* c;
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

	while (con) {
		c = con;
		con = con->next;

		if (c->id.s) pkg_free(c->id.s);
		if (c->host.s) pkg_free(c->host.s);
		if (c->username.s) pkg_free(c->username.s);
		if (c->password.s) pkg_free(c->password.s);

		pkg_free(c);
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

	if (cfg_eat_equal(st, flags)) return -1;

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
	if (ret > 0) {
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
	if (ret > 0) {
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
	if (ret > 0) {
		ERR("ldap:%s:%d:%d: LDAP Attribute syntax or name expected\n",
		    st->file, st->line, st->col);
		return -1;
	}

	if (t.type == '(') {
		ret = cfg_get_token(&t, st, 0);
		if (ret < 0) return -1;
		if (ret > 0) {
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
		if (ret > 0) {
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
		if (ret > 0) {
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

	if (cfg_eat_eol(st, flags)) return -1;
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


static cfg_option_t deref_values[] = {
	{"never",     .val = LDAP_DEREF_NEVER },   /* default, 0x00 */
	{"searching", .val = LDAP_DEREF_SEARCHING},
	{"finding",   .val = LDAP_DEREF_FINDING },
	{"always",    .val = LDAP_DEREF_ALWAYS },
	{0}
};

static cfg_option_t ldap_tab_options[] = {
	{"scope",     .param = scope_values, .f = cfg_parse_enum_opt},
	{"field_map", .f = parse_field_map},
	{"filter",    .f = cfg_parse_str_opt, .flags = CFG_STR_PKGMEM},
	{"base",      .f = cfg_parse_str_opt, .flags = CFG_STR_PKGMEM},
	{"timelimit", .f = cfg_parse_int_opt},
	{"sizelimit", .f = cfg_parse_int_opt},
	{"chase_references",  .param = deref_values, .f = cfg_parse_enum_opt},
	{"chase_referrals",   .f = cfg_parse_bool_opt},
	{0}
};


static cfg_option_t auth_values[] = {
	{"none",       .val = LDAP_AUTHMECH_NONE},
	{"simple",     .val = LDAP_AUTHMECH_SIMPLE},
	{"digest-md5", .val = LDAP_AUTHMECH_DIGESTMD5},
	{"external",   .val = LDAP_AUTHMECH_EXTERNAL},
	{0}
};


static cfg_option_t ldap_con_options[] = {
	{"host",     		    .f = cfg_parse_str_opt, .flags = CFG_STR_PKGMEM},
	{"port",     		    .f = cfg_parse_int_opt},
	{"username", 		    .f = cfg_parse_str_opt, .flags = CFG_STR_PKGMEM},
	{"password", 		    .f = cfg_parse_str_opt, .flags = CFG_STR_PKGMEM},
	{"authtype", 		    .param = auth_values, .f = cfg_parse_enum_opt},
	{"tls",			    .f = cfg_parse_bool_opt},
	{"ca_list",  		    .f = cfg_parse_str_opt, .flags = CFG_STR_PKGMEM},
	{"require_certificate",     .f = cfg_parse_str_opt, .flags = CFG_STR_PKGMEM},
	{0}
};


static cfg_option_t section_types[] = {
	{"connection", .val = LDAP_CON_SECTION},
	{"con",        .val = LDAP_CON_SECTION},
	{"table",      .val = LDAP_TABLE_SECTION},
	{0}
};


static int parse_section(void* param, cfg_parser_t* st, unsigned int flags)
{
	cfg_token_t t;
	int ret, type, i;
	cfg_option_t* opt;
	str* id = NULL;
	struct ld_cfg* tab;
	struct ld_con_info* cinfo;

	ret = cfg_get_token(&t, st, 0);
	if (ret < 0) return -1;
	if (ret > 0) {
		ERR("%s:%d:%d: Section type missing\n",
		    st->file, st->line, st->col);
		return -1;
	}

	if (t.type != CFG_TOKEN_ALPHA ||
	    ((opt = cfg_lookup_token(section_types, &t.val)) == NULL)) {
		ERR("%s:%d:%d: Invalid section type %d:'%.*s'\n",
		    st->file, t.start.line, t.start.col, t.type, STR_FMT(&t.val));
		return -1;
	}
	type = opt->val;

	if (type == LDAP_TABLE_SECTION) {
		if ((tab = pkg_malloc(sizeof(*tab))) == NULL) {
			ERR("ldap:%s:%d: Out of memory\n", st->file, st->line);
			return -1;
		}
		memset(tab, '\0', sizeof(*tab));
		tab->next = cfg;
		cfg = tab;

		cfg_set_options(st, ldap_tab_options);
		ldap_tab_options[2].param = &cfg->filter;
		ldap_tab_options[3].param = &cfg->base;
		for(i = 0; scope_values[i].name; i++) {
			scope_values[i].param = &cfg->scope;
		}
		ldap_tab_options[4].param = &cfg->timelimit;
		ldap_tab_options[5].param = &cfg->sizelimit;
		for(i = 0; deref_values[i].name; i++) {
			deref_values[i].param = &cfg->chase_references;
		}
		ldap_tab_options[7].param = &cfg->chase_referrals;
	} else if (type == LDAP_CON_SECTION) {
		if ((cinfo = pkg_malloc(sizeof(*cinfo))) == NULL) {
			ERR("ldap:%s:%d: Out of memory\n", st->file, st->line);
			return -1;
		}
		memset(cinfo, '\0', sizeof(*cinfo));
		cinfo->next = con;
		con = cinfo;

		cfg_set_options(st, ldap_con_options);
		ldap_con_options[0].param = &con->host;
		ldap_con_options[1].param = &con->port;
		ldap_con_options[2].param = &con->username;
		ldap_con_options[3].param = &con->password;
		for(i = 0; auth_values[i].name; i++) {
			auth_values[i].param = &con->authmech;
		}
		ldap_con_options[5].param = &con->tls;
		ldap_con_options[6].param = &con->ca_list;
		ldap_con_options[7].param = &con->req_cert;
	} else {
		BUG("%s:%d:%d: Unsupported section type %c\n",
			st->file, t.start.line, t.start.col, t.type);
		return -1;
	}

	ret = cfg_get_token(&t, st, 0);
	if (ret < 0) return -1;
	if (ret > 0) {
		ERR("%s:%d:%d: Delimiter ':' expected.\n",
		    st->file, st->line, st->col);
		return -1;
	}

	if (type == LDAP_TABLE_SECTION) {
		id = &cfg->table;
	} else if (type == LDAP_CON_SECTION) {
		id = &con->id;
	} else {
		BUG("%s:%d:%d: Invalid section type %d\n", st->file,
			st->line, st->col, type);
	}

	ret = cfg_parse_str(id, st, CFG_STR_PKGMEM);
	if (ret < 0) return -1;
	if (ret > 0) {
		ERR("%s:%d:%d: Section identifier expected\n",
			st->file, st->line, st->col);
		return -1;
	}

	ret = cfg_get_token(&t, st, 0);
	if (ret < 0) return ret;
	if (ret > 0) {
		ERR("%s:%d:%d: Missing closing ']'.\n",
			st->file, st->line, st->col);
		return -1;
	}
	if (t. type != ']') {
		ERR("%s:%d:%d: Syntax error, ']' expected.\n",
			st->file, t.start.line, t.start.col);
		return -1;
	}

	if (cfg_eat_eol(st, flags)) return -1;
	return 0;
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


struct ld_con_info* ld_find_conn_info(str* conn_id)
{
	struct ld_con_info* ptr;

	ptr = con;
	while(ptr) {
		if (ptr->id.len == conn_id->len &&
			!memcmp(ptr->id.s, conn_id->s, conn_id->len)) {
			return ptr;
		}
		ptr = ptr->next;
	}
	return NULL;
}


static int ld_cfg_validity_check(struct ld_cfg *cfg)
{
	struct ld_cfg *pcfg;

	for (pcfg = cfg; pcfg; pcfg = pcfg->next) {
		if (pcfg->sizelimit < 0 || pcfg->sizelimit > LD_MAXINT) {
			ERR("ldap: invalid sizelimit (%d) specified\n", pcfg->sizelimit);
			return -1;
		}
		if (pcfg->timelimit < 0 || pcfg->timelimit > LD_MAXINT) {
			ERR("ldap: invalid timelimit (%d) specified\n", pcfg->timelimit);
			return -1;
		}
	}

	return 0;
}


int ld_load_cfg(str* filename)
{
	cfg_parser_t* parser;
	cfg = NULL;

	if ((parser = cfg_parser_init(0, filename)) == NULL) {
		ERR("ldap: Error while initializing configuration file parser.\n");
		return -1;
	}

	cfg_section_parser(parser, parse_section, NULL);

	if (sr_cfg_parse(parser)) {
		if (cfg == NULL) {
			ERR("ldap: A table name (i.e. [table_name]) is missing in the "
				"configuration file.\n");
		}
		cfg_parser_close(parser);
		ld_cfg_free();
		return -1;
	}
	cfg_parser_close(parser);

	if (ld_cfg_validity_check(cfg)) {
		ld_cfg_free();
		return -1;
	}

	return 0;
}
