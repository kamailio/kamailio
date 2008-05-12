/*
 * LDAP module - Configuration file parser
 *
 * Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2004,2005 Free Software Foundation, Inc.
 * Copyright (C) 2005,2006 iptelorg GmbH
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
 *
 */

#include "ld_config.h"
#include "ld_mod.h"

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../trim.h"
#include "../../ut.h"
#include "../../resolve.h"

#include <ldap.h>
#include <malloc.h>
#include <stdio.h>
#include <libgen.h>

#define MAX_TOKEN_LEN 256


struct ld_config* ld_cfg_root = NULL;


/*
 * Parser state
 */
static struct {
	FILE* f;
	char* file;
	int line;
	int col;
	struct ld_config* cfg; /* Current configuration data */
} pstate;


/*
 * Structure representing lexical token
 */
typedef struct token {
	char buf [MAX_TOKEN_LEN];
	int type;  /* Token type */
	str val;   /* Token value */

	struct {   /* Position of first and last character of
		    * token in file
		    */
		int line;
		int col;
	} start, end;
} token_t;


typedef int (*parser_func_f)(token_t* token);


struct parser_tab {
	str token;
	union {
		int ival;
		str sval;
		parser_func_f fval;
	} u;
};

static struct parser_tab token_scope[];
static struct parser_tab option_name[];


/*
 * The states of the lexical scanner
 */
enum st {
	ST_S,  /* Begin */
	ST_A,  /* Alphanumeric */
	ST_AE, /* Alphanumeric escaped */
	ST_Q,  /* Quoted */
	ST_QE, /* Quoted escaped */
	ST_C,  /* Comment */
	ST_CE, /* Comment escaped */
	ST_E,  /* Escaped */
};


/* Extended tokens can contain also delimiters,
 * in addition to alpha-numeric characters,
 * this is used on the righ side of assignments
 * where no quotes are used
 */
#define EXTENDED_ALPHA_TOKEN (1 << 0)


/*
 * Test for alphanumeric characters
 */
#define IS_ALPHA(c) \
    (((c) >= 'a' && (c) <= 'z') || \
     ((c) >= 'A' && (c) <= 'Z') || \
     ((c) >= '0' && (c) <= '9') || \
     (c) == '_')


/*
 * Test for delimiter characters
 */
#define IS_DELIM(c) \
    ((c) == '=' || \
     (c) == ':' || \
     (c) == ';' || \
     (c) == '.' || \
     (c) == ',' || \
     (c) == '?' || \
     (c) == '[' || \
     (c) == ']' || \
     (c) == '/' || \
     (c) == '@' || \
     (c) == '!' || \
     (c) == '$' || \
     (c) == '%' || \
     (c) == '&' || \
     (c) == '*' || \
     (c) == '(' || \
     (c) == ')' || \
     (c) == '-' || \
     (c) == '+' || \
     (c) == '|' || \
     (c) == '\'')

/* Whitespace characters */
#define IS_WHITESPACE(c) ((c) == ' ' || (c) == '\t' || (c) == '\r') 
#define IS_QUOTE(c)      ((c) == '\"')  /* Quote characters */
#define IS_COMMENT(c)    ((c) == '#')   /* Characters that start comments */
#define IS_ESCAPE(c)     ((c) == '\\')  /* Escape characters */
#define IS_EOL(c)        ((c) == '\n')  /* End of line */


/*
 * Append character to the value of current
 * token
 */
#define PUSH(c)                                    \
    if (token->val.len >= MAX_TOKEN_LEN) {         \
        ERR("%s:%d:%d: Token too long\n",          \
        pstate.file, pstate.line, pstate.col);     \
        return -1;                                 \
    }                                              \
    if (token->val.len == 0) {                     \
         token->start.line = pstate.line;          \
         token->start.col = pstate.col;            \
    }                                              \
    token->val.s[token->val.len++] = (c);


/*
 * Return current token from the lexical analyzer
 */
#define RETURN(c)                  \
    token->end.line = pstate.line; \
    token->end.col = pstate.col;   \
    token->type = (c);             \
    print_token(token);            \
    return 1;


/*
 * Get next character and update counters
 */
#define READ_CHAR         \
     c = fgetc(pstate.f); \
     if (IS_EOL(c)) {     \
         pstate.line++;   \
         pstate.col = 0;  \
     } else {             \
         pstate.col++;    \
     }


enum {
	TOKEN_EOF = -1,
	TOKEN_ALPHA = -2,
	TOKEN_STRING = -3
};


static void print_token(struct token* token)
{
	DBG("token(%d, '%.*s', <%d,%d>-<%d,%d>)\n", 
	    token->type, token->val.len, ZSW(token->val.s),
	    token->start.line, token->start.col, 
	    token->end.line, token->end.col);
}


static int lex(token_t* token, unsigned int flags)
{
	static int look_ahead = EOF;
	int c;
	enum st state;

	state = ST_S;
	
	token->val.s = token->buf;
	token->val.len = 0;

	if (look_ahead != EOF) {
		c = look_ahead;
		look_ahead = EOF;
	} else {
		READ_CHAR;
	}

	while(c != EOF) {
		switch(state) {
		case ST_S:
			if (flags & EXTENDED_ALPHA_TOKEN) {
				if (IS_WHITESPACE(c)) {
					     /* Do nothing */
				} else if (IS_ALPHA(c) ||
					   IS_ESCAPE(c) ||
					   IS_DELIM(c)) {
					PUSH(c);
					state = ST_A;
				} else if (IS_QUOTE(c)) {
					state = ST_Q;
				} else if (IS_COMMENT(c)) {
					state = ST_C;
				} else if (IS_EOL(c)) {
					PUSH(c);
					RETURN(c);
				} else {
					ERR("%s:%d:%d: Invalid character 0x%x\n", 
					    pstate.file, pstate.line, pstate.col, c);
					return -1;
				}
			} else {
				if (IS_WHITESPACE(c)) {
					     /* Do nothing */
				} else if (IS_ALPHA(c)) {
					PUSH(c);
					state = ST_A;
				} else if (IS_QUOTE(c)) {
					state = ST_Q;
				} else if (IS_COMMENT(c)) {
					state = ST_C;
				} else if (IS_ESCAPE(c)) {
					state = ST_E;
				} else if (IS_DELIM(c) || IS_EOL(c)) {
					PUSH(c);
					RETURN(c);
				} else {
					ERR("%s:%d:%d: Invalid character 0x%x\n", 
					    pstate.file, pstate.line, pstate.col, c);
					return -1;
				}
			}
			break;

		case ST_A:
			if (flags & EXTENDED_ALPHA_TOKEN) {
				if (IS_ALPHA(c) ||
				    IS_DELIM(c) ||
				    IS_QUOTE(c)) {
					PUSH(c);
				} else if (IS_ESCAPE(c)) {
					state = ST_AE;
				} else if (IS_COMMENT(c) || IS_EOL(c) || IS_WHITESPACE(c)) {
					look_ahead = c;
					RETURN(TOKEN_ALPHA);
				} else {
					ERR("%s:%d:%d: Invalid character 0x%x\n", 
					    pstate.file, pstate.line, pstate.col, c);
					return -1;
				}
			} else {
				if (IS_ALPHA(c)) {
					PUSH(c);
				} else if (IS_ESCAPE(c)) {
					state = ST_AE;
				} else if (IS_WHITESPACE(c) ||
					   IS_DELIM(c) ||
					   IS_QUOTE(c) ||
					   IS_COMMENT(c) ||
					   IS_EOL(c)) {
					look_ahead = c;
					RETURN(TOKEN_ALPHA);
				} else {
					ERR("%s:%d:%d: Invalid character 0x%x\n", 
					    pstate.file, pstate.line, pstate.col, c);
					return -1;
				}
			}
			break;

		case ST_AE:
			if (IS_COMMENT(c) ||
			    IS_QUOTE(c) ||
			    IS_ESCAPE(c)) {
				PUSH(c);
			} else if (c == 'r') {
				PUSH('\r');
			} else if (c == 'n') {
				PUSH('\n');
			} else if (c == 't') {
				PUSH('\t');
			} else if (c == ' ') {
				PUSH(' ');
			} else if (IS_EOL(c)) {
				     /* Do nothing */
			} else {
				ERR("%s:%d:%d: Unsupported escape character 0x%x\n", 
				    pstate.file, pstate.line, pstate.col, c);
				return -1;
			}
			state = ST_A;
			break;

		case ST_Q:
			if (IS_QUOTE(c)) {
				RETURN(TOKEN_STRING);
			} else if (IS_ESCAPE(c)) {
				state = ST_QE;
				break;
			} else {
				PUSH(c);
			}
			break;

		case ST_QE:
			if (IS_ESCAPE(c) ||
			    IS_QUOTE(c)) {
				PUSH(c);
			} else if (c == 'n') {
				PUSH('\n');
			} else if (c == 'r') {
				PUSH('\r');
			} else if (c == 't') {
				PUSH('\t');
			} else if (IS_EOL(c)) {
				     /* Do nothing */
			} else {
				ERR("%s:%d:%d: Unsupported escape character 0x%x\n", 
				    pstate.file, pstate.line, pstate.col, c);
				return -1;
			}
			state = ST_Q;
			break;

		case ST_C:
			if (IS_ESCAPE(c)) {
				state = ST_CE;
			} else if (IS_EOL(c)) {
				state = ST_S;
				continue; /* Do not read a new char, return EOL */
			} else {
				     /* Do nothing */
			}
			break;

		case ST_CE:
			state = ST_C;
			break;

		case ST_E:
			if (IS_COMMENT(c) ||
			    IS_QUOTE(c) ||
			    IS_ESCAPE(c)) {
				PUSH(c);
				RETURN(c);
			} else if (c == 'r') {
				PUSH('\r');
				RETURN('\r');
			} else if (c == 'n') {
				PUSH('\n');
				RETURN('\n');
			} else if (c == 't') {
				PUSH('\t');
				RETURN('\t');
			} else if (c == ' ') {
				PUSH(' ');
				RETURN(' ');
			} else if (IS_EOL(c)) {
				     /* Escped eol means no eol */
				state = ST_S;
			} else {
				ERR("%s:%d:%d: Unsupported escape character 0x%x\n", 
				    pstate.file, pstate.line, pstate.col, c);
				return -1;
			}
			break;
		}

		READ_CHAR;
	};

	switch(state) {
	case ST_S: 
	case ST_C:
	case ST_CE:
		return 0;

	case ST_A:
		RETURN(TOKEN_ALPHA);

	case ST_Q:
		ERR("%s:%d:%d: Premature end of file, missing closing quote in"
				" string constant\n", pstate.file, pstate.line, pstate.col);
		return -1;

	case ST_QE:
	case ST_E:
	case ST_AE:
		ERR("%s:%d:%d: Premature end of file, missing escaped character\n", 
		    pstate.file, pstate.line, pstate.col);
		return -1;
	}
	BUG("%s:%d:%d: invalid state %d\n",
			pstate.file, pstate.line, pstate.col, state);
		return -1;
}


static struct parser_tab* lookup_token(struct parser_tab* table, str* token)
{
	struct parser_tab* ptr;

	ptr = table;
	while(ptr->token.s && ptr->token.len) {
		if (token->len == ptr->token.len && 
		    !strncasecmp(token->s, ptr->token.s, token->len)) {
			return ptr;
		}
		ptr++;
	}
	return 0;
}

static int parse_string_val(str* res, token_t* token)
{
	int ret;
	static token_t t;

	ret = lex(&t, 0);
	if (ret < 0) return -1;
	if (ret == 0) {
		ERR("ldap:%s:%d:%d: Option value missing\n", 
		    pstate.file, token->start.line, token->start.col);
		return -1;
	}

	if (t.type != '=') {
		ERR("ldap:%s:%d:%d: Syntax error, '=' expected\n", 
		    pstate.file, t.start.line, t.start.col);
		return -1;
	}

	ret = lex(&t, EXTENDED_ALPHA_TOKEN);
	if (ret < 0) return -1;
	if (ret == 0) {
		ERR("ldap:%s:%d:%d: Option value missing\n",
		    pstate.file, t.start.line, t.start.col);
		return -1;
	}	

	if (t.type != TOKEN_ALPHA && t.type != TOKEN_STRING) {
		ERR("ldap:%s:%d:%d: Invalid option value '%.*s'\n", 
		    pstate.file, t.start.line, t.start.col,
		    t.val.len, ZSW(t.val.s));
		return -1;
	}

	*res = t.val;;
	return 0;
}


static int parse_search_scope(token_t* token)
{
	int ret;
	token_t t;
	struct parser_tab* r;

	ret = lex(&t, 0);
	if (ret < 0) return -1;
	if (ret == 0) {
		ERR("ldap:%s:%d:%d: Option value missing\n", 
		    pstate.file, token->start.line, token->start.col);
		return -1;
	}

	if (t.type != '=') {
		ERR("ldap:%s:%d:%d: Syntax error, '=' expected\n", 
		    pstate.file, t.start.line, t.start.col);
		return -1;
	}

	ret = lex(&t, EXTENDED_ALPHA_TOKEN);
	if (ret < 0) return -1;
	if (ret == 0) {
		ERR("ldap:%s:%d:%d: Option value missing\n",
		    pstate.file, t.start.line, t.start.col);
		return -1;
	}	

	if (t.type != TOKEN_ALPHA && t.type != TOKEN_STRING) {
		ERR("ldap:%s:%d:%d: Invalid option value '%.*s'\n", 
		    pstate.file, t.start.line, t.start.col,
		    t.val.len, ZSW(t.val.s));
		return -1;
	}

	r = lookup_token(token_scope, &t.val);
	if (!r) {
		ERR("ldap:%s:%d:%d: Invalid option value '%.*s'\n", 
		    pstate.file, t.start.line, t.start.col,
		    t.val.len, ZSW(t.val.s));
		return -1;
	}

	pstate.cfg->scope = r->u.ival;
	return 0;	
}

static int parse_search_base(token_t* token)
{
	str val;
	if (parse_string_val(&val, token) < 0) return -1;
	pstate.cfg->base = as_asciiz(&val);
	if (pstate.cfg->base == NULL) {
		ERR("ldap:%s:%d:%d: Out of memory while processing token %d:'%.*s'\n", 
		    pstate.file, token->start.line, token->start.col,
		    token->type, token->val.len, ZSW(token->val.s));
		return -1;
	}
	return 0;
}


static int parse_search_filter(token_t* token)
{
	str val;
	if (parse_string_val(&val, token) < 0) return -1;
	pstate.cfg->filter = as_asciiz(&val);
	if (pstate.cfg->filter == NULL) {
		ERR("ldap:%s:%d:%d: Out of memory while processing token %d:'%.*s'\n", 
		    pstate.file, token->start.line, token->start.col,
		    token->type, token->val.len, ZSW(token->val.s));
		return -1;
	}
	return 0;
}


static int parse_field_map(token_t* token)
{
	int ret;
	token_t t;
	void* ptr;

	ret = lex(&t, 0);
	if (ret < 0) return -1;
	if (ret == 0) {
		ERR("ldap:%s:%d:%d: Field map value missing\n", 
		    pstate.file, token->start.line, token->start.col);
		return -1;
	}

	if (t.type != '=') {
		ERR("ldap:%s:%d:%d: Syntax error, '=' expected\n", 
		    pstate.file, t.start.line, t.start.col);
		return -1;
	}

	ptr = pkg_realloc(pstate.cfg->fields, sizeof(char*) * (pstate.cfg->n + 1));
	if (ptr == NULL) {
		ERR("ldap:%s:%d: Out of memory\n", 
		    pstate.file, token->start.line);
		return -1;
	}
	pstate.cfg->fields = (char**)ptr;
	pstate.cfg->fields[pstate.cfg->n] = NULL;

	ptr = pkg_realloc(pstate.cfg->attrs, sizeof(char*) * (pstate.cfg->n + 1));
	if (ptr == NULL) {
		ERR("ldap:%s:%d: Out of memory\n", 
		    pstate.file, token->start.line);
		return -1;
	}
	pstate.cfg->attrs = (char**)ptr;
	pstate.cfg->attrs[pstate.cfg->n] = NULL;

	pstate.cfg->n++;

	ret = lex(&t, 0);
	if (ret < 0) return -1;
	if (ret == 0) {
		ERR("ldap:%s:%d:%d: Database field name expected\n", 
		    pstate.file, token->start.line, token->start.col);
		return -1;
	}

	if (t.type != TOKEN_ALPHA) {
		ERR("ldap:%s:%d:%d: Invalid field name format %d:'%.*s'\n", 
		    pstate.file, t.start.line, t.start.col,
		    t.type, t.val.len, ZSW(t.val.s));
		return -1;
	}
	
	pstate.cfg->fields[pstate.cfg->n - 1] = as_asciiz(&t.val);
	if (pstate.cfg->fields[pstate.cfg->n - 1] == NULL) {
		ERR("ldap:%s:%d: Out of memory\n", 
		    pstate.file, token->start.line);
		return -1;
	}

	ret = lex(&t, 0);
	if (ret < 0) return -1;
	if (ret == 0) {
		ERR("ldap:%s:%d:%d: Delimiter ':' missing\n", 
		    pstate.file, t.start.line, t.start.col);
		return -1;
	}
	if (t.type != ':') {
		ERR("ldap:%s:%d:%d: Syntax error, ':' expected\n", 
		    pstate.file, t.start.line, t.start.col);
		return -1;
	}

	ret = lex(&t, 0);
	if (ret < 0) return -1;
	if (ret == 0) {
		ERR("ldap:%s:%d:%d: LDAP Attribute name expected\n", 
		    pstate.file, token->start.line, token->start.col);
		return -1;
	}

	if (t.type != TOKEN_ALPHA) {
		ERR("ldap:%s:%d:%d: Invalid LDAP attribute name format %d:'%.*s'\n", 
		    pstate.file, t.start.line, t.start.col,
		    t.type, t.val.len, ZSW(t.val.s));
		return -1;
	}

	pstate.cfg->attrs[pstate.cfg->n - 1] = as_asciiz(&t.val);
	if (pstate.cfg->attrs[pstate.cfg->n - 1] == NULL) {
		ERR("ldap:%s:%d: Out of memory\n", 
		    pstate.file, token->start.line);
		return -1;
	}
	
	return 0;
}



static int parse_table(token_t* token)
{
	token_t t;
	int ret;
	struct ld_config* ptr;
	str tab;

	tab.s = NULL;
	ret = lex(&t, 0);
	if (ret < 0) return -1;
	if (ret == 0) {
		ERR("ldap:%s:%d:%d: Table name missing\n", 
		    pstate.file, token->start.line, token->start.col);
		return -1;
	}

	if (t.type != TOKEN_ALPHA) {
		ERR("ldap:%s:%d:%d: Invalid table name %d:'%.*s'\n", 
		    pstate.file, t.start.line, t.start.col,
		    t.type, t.val.len, ZSW(t.val.s));
		return -1;
	}
	
	tab.s = as_asciiz(&t.val);
	if (tab.s == NULL) {
		ERR("ldap:%s:%d: Out of memory\n", 
		    pstate.file, token->start.line);
		return -1;
	}
	tab.len = t.val.len;

	ret = lex(&t, 0);
	if (ret < 0) goto error;
	if (ret == 0) {
		ERR("ldap:%s:%d:%d: Closing ']' missing\n", 
		    pstate.file, t.start.line, t.start.col);
		goto error;
	}
	if (t.type != ']') {
		ERR("ldap:%s:%d:%d: Syntax error, ']' expected\n", 
		    pstate.file, t.start.line, t.start.col);
		goto error;
	}
	
	ptr = (struct ld_config*)pkg_malloc(sizeof(struct ld_config));
	if (ptr == NULL) {
		ERR("ldap:%s:%d: Out of memory\n", 
		    pstate.file, token->start.line);
		goto error;
	}
	memset(ptr, '\0', sizeof(struct ld_config));
	ptr->table = tab;
	ptr->next = ld_cfg_root;
	ld_cfg_root = ptr;
	pstate.cfg = ptr;
	return 0;

 error:
	if (tab.s) pkg_free(tab.s);
	return -1;
}

static int parse_config(void)
{
	int ret;
	token_t token;
	struct parser_tab* r;

	while(1) {
		     /* Get option name */
		ret = lex(&token, 0);
		if (ret <= 0) return ret;

		switch(token.type) {
		case TOKEN_ALPHA:
			if (!pstate.cfg) {
				ERR("ldap:%s:%d: You need to specify database table first, for example [table]\n", 
				    pstate.file, token.start.line);
				return -1;
			}

			     /* Lookup the option name */
			r = lookup_token(option_name, &token.val);
			if (!r) {
				ERR("ldap:%s:%d: Unsupported option '%.*s'\n", 
				    pstate.file, token.start.line, 
				    token.val.len, ZSW(token.val.s));
				return -1;
			}
			     /* Parse the option value */
			if (r->u.fval(&token) < 0) return -1;
			break;

		case '[': 
			if (parse_table(&token) < 0) return -1;
			break;

			     /* Empty line */
		case '\n': continue;

		default:
			ERR("ldap:%s:%d:%d: Syntax error\n", 
			    pstate.file, token.start.line, token.start.col);
			return -1;
		}

		     /* Skip EOL */
		ret = lex(&token, 0);
		if (ret <= 0) return ret;
		if (token.type != '\n') {
			ERR("ldap:%s:%d:%d: End of line expected\n", 
			    pstate.file, token.start.line, token.start.col);
			return -1;
		}
	}
	return 0;
}


/*
 * Create configuration structures from configuration file
 */
int ld_load_config(str* filename)
{
	char* file;

	file = get_abs_pathname(NULL, filename);
	if (!file) return -1;

	pstate.f = fopen(file, "r");
	if (pstate.f == NULL) {
		ERR("ldap: Unable to open configuration file'%s'\n", file);
		free(file);
		return -1;
	}
	pstate.file = basename(file);
	pstate.line = 1;
	pstate.col = 0;
	pstate.cfg = 0;

	if (parse_config() < 0) goto error;

	fclose(pstate.f);
	free(file);
	return 0;

 error:
	free(file);
	fclose(pstate.f);
	return -1;
}


struct ld_config* ld_find_config(str* table)
{
	struct ld_config* ptr;

	ptr = ld_cfg_root;
	while(ptr) {
		if (ptr->table.len == table->len &&
			!strncmp(ptr->table.s, table->s, table->len))
			return ptr;
		ptr = ptr->next;
	}
	return NULL;
}


char* ld_find_attr_name(struct ld_config* cfg, char* fld_name)
{
	int i;

	for(i = 0; i < cfg->n; i++) {
		if (!strcmp(fld_name, cfg->fields[i]))
			return cfg->attrs[i];
	}
	return NULL;
}


static struct parser_tab token_scope[] = {
	{STR_STATIC_INIT("base"),     {.ival = LDAP_SCOPE_BASE}},
	{STR_STATIC_INIT("onelevel"), {.ival = LDAP_SCOPE_ONELEVEL}},
	{STR_STATIC_INIT("subtree"),  {.ival = LDAP_SCOPE_SUBTREE}},
	{STR_STATIC_INIT("children"), {.ival = LDAP_SCOPE_CHILDREN}},
	{STR_NULL}
};


static struct parser_tab option_name[] = {
	{STR_STATIC_INIT("base"),      {.fval = parse_search_base}},
	{STR_STATIC_INIT("scope"),     {.fval = parse_search_scope}},
	{STR_STATIC_INIT("filter"),    {.fval = parse_search_filter}},
	{STR_STATIC_INIT("field_map"), {.fval = parse_field_map}},
	{STR_NULL}
};
