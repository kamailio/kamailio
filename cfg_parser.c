/*
 * Standalone Configuration File Parser
 *
 * Copyright (C) 2008 iptelorg GmbH
 * Written by Jan Janak <jan@iptel.org>
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * Kamailio is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
/*!
 * \file
 * \brief Kamailio core :: Standalone Configuration File Parser
 * \author Jan Janak <jan@iptel.org>
 * \ingroup core
 *
 * Module: \ref core
 *
 * See \ref ConfigEngine
 *
 * \page ConfigEngine
 * In file \ref cfg_parser.c 
 * Configuration examples
 * - \ref ConfigExample1
 * - \ref ConfigExample2
 * - \ref ConfigExample3
 * - \ref ConfigExample4
 * - \ref ConfigExample5
 * - \ref ConfigExample6
 * - \ref ConfigExample7
 * - \ref ConfigExample8
 *
 * <b>Run-time Allocated Destination Variables</b>
 * - the destination variable pointers in arrays are assigned at compile time
 * - The address of variables allocated on the heap (typically in dynamically allocated
 *   structures) is not know and thus we need to assign NULL initially and change the pointer
 *   at runtime.
 *   (provide an example).
 *
 * <b>Built-in parsing functions</b>
 *
 * *_val functions parse the whole option body (including =)
 */


/*! \page ConfigExample1  Configuration engine Example 1: Options without values
 *
\verbatim
 *	str file = STR_STATIC_INIT("test.cfg");
 *	cfg_parser_t* parser;
 *	int feature_a = 0, feature_b = 0;
 *
 *	cfg_option_t options[] = {
 *		{"enable_feature_a",  .param = &feature_a, .val = 1},
 *		{"disable_feature_a", .param = &feature_a, .val = 0},
 *		{"feature_b",         .param = &feature_b, .val = 1},
 *      {0}
 *	};
 *
 *	if ((parser = cfg_parser_init(&file)) == NULL) {
 *		ERR("Error while creating parser\n");
 *		return -1;
 *	}
 *
 *	cfg_set_options(parser, options);
 *
 *	if (sr_cfg_parse(parser) < 0) {
 *		ERR("Error while parsing configuration file\n");
 *      cfg_parser_close(parser);
 *		return -1;
 *	}
 *
 *	cfg_parser_close(parser);
\endverbatim
 */

/*! \page ConfigExample2  Configuration engine Example 2: Options with integer values
\verbatim
 * 	cfg_option_t options[] = {
 *		{"max_number",   .param = &max_number,   .f = cfg_parse_int_val },
 *		{"extra_checks", .param = &extra_checks, .f = cfg_parse_bool_val},
 *		{0}
 *	};
\endverbatim
 */

/*! \page ConfigExample3 Configuration engine Example 3: Enum options
\verbatim
 * int scope;
 *
 *	cfg_option_t scopes[] = {
 *	    {"base",     .param = &scope, .val = 1},
 *	    {"onelevel", .param = &scope, .val = 2},
 *	    {"one",      .param = &scope, .val = 3},
 *	    {"subtree",  .param = &scope, .val = 4},
 *	    {"sub",      .param = &scope, .val = 5},
 *	    {"children", .param = &scope, .val = 6},
 *	    {0}
 *  };
 *
 *	cfg_option_t options[] = {
 *		{"scope", .param = scopes, .f = cfg_parse_enum_val},
 *		{0}
 *	};
\endverbatim
 */

/*! \page ConfigExample4 Configuration engine Example 4: Options with string values
\verbatim
* 	str filename = STR_NULL;
 *
 *	cfg_option_t options[] = {
 *		{"filename", .param = &filename, .f = cfg_parse_str_val},
 *		{0}
 *	};
 *
 * - By default the function returns a pointer to an internal buffer which will be destroyed
 *   by a subsequent call
 * - There are flags to tell the function to copy the resuting string in a pkg, shm, glibc,
 *   or static buffers
\endverbatim
 */

/*! \page ConfigExample5 Configuration engine Example 5: Custom value parsing
 * TBD
 */

/*! \page ConfigExample6 Configuration engine Example 6: Parsing Sections
 * TBD
 */

/*! \page ConfigExample7 Configuration engine Example 7: Default Options
 * TBD
 */

/*! \page ConfigExample8 Configuration engine Example 8: Memory management of strings
 *
 * Data types with fixed size are easy, they can be copied into a pre-allocated memory
 * buffer, strings cannot because their length is unknown.
 */

#include "cfg_parser.h"

#include "mem/mem.h"
#include "mem/shm_mem.h"
#include "dprint.h"
#include "trim.h"
#include "ut.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>


/*! \brief The states of the lexical scanner */
enum st {
	ST_S,  /*!< Begin */
	ST_A,  /*!< Alphanumeric */
	ST_AE, /*!< Alphanumeric escaped */
	ST_Q,  /*!< Quoted */
	ST_QE, /*!< Quoted escaped */
	ST_C,  /*!< Comment */
	ST_CE, /*!< Comment escaped */
	ST_E,  /*!< Escaped */
};


/*! \brief Test for alphanumeric characters */
#define IS_ALPHA(c) \
    (((c) >= 'a' && (c) <= 'z') || \
     ((c) >= 'A' && (c) <= 'Z') || \
     ((c) >= '0' && (c) <= '9') || \
     (c) == '_')


/*! \brief Test for delimiter characters */
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


/*! \brief Whitespace characters */
#define IS_WHITESPACE(c) ((c) == ' ' || (c) == '\t' || (c) == '\r') 
#define IS_QUOTE(c)      ((c) == '\"')  /* Quote characters */
#define IS_COMMENT(c)    ((c) == '#')   /* Characters that start comments */
#define IS_ESCAPE(c)     ((c) == '\\')  /* Escape characters */
#define IS_EOL(c)        ((c) == '\n')  /* End of line */


/*! \brief
 * Append character to the value of current token
 */
#define PUSH(c)                            \
    if (token->val.len >= MAX_TOKEN_LEN) { \
        ERR("%s:%d:%d: Token too long\n",  \
        st->file, st->line, st->col);      \
        return -1;                         \
    }                                      \
    if (token->val.len == 0) {             \
         token->start.line = st->line;     \
         token->start.col = st->col;       \
    }                                      \
    token->val.s[token->val.len++] = (c);


/*! \brief
 * Return current token from the lexical analyzer
 */
#define RETURN(c)               \
    token->end.line = st->line; \
    token->end.col = st->col;   \
    token->type = (c);          \
    print_token(token);         \
    return 0;


/*! \brief
 * Get next character and update counters
 */
#define READ_CHAR      \
     c = fgetc(st->f); \
     if (IS_EOL(c)) {  \
         st->line++;   \
         st->col = 0;  \
     } else {          \
         st->col++;    \
     }


cfg_option_t cfg_bool_values[] = {
	{"yes",      .val = 1},
	{"true",     .val = 1},
	{"enable",   .val = 1},
	{"enabled",  .val = 1},
	{"1",        .val = 1},
	{"on",       .val = 1},
	{"no",       .val = 0},
	{"false",    .val = 0},
	{"disable",  .val = 0},
	{"disabled", .val = 0},
	{"0",        .val = 0},
	{"off",      .val = 0},
	{0}
};


static void print_token(cfg_token_t* token)
{
#ifdef EXTRA_DEBUG
	int i, j;
	char* buf;

	if ((buf = pkg_malloc(token->val.len * 2)) == NULL) {
		LM_DBG("token(%d, '%.*s', <%d,%d>-<%d,%d>)\n", 
			token->type, STR_FMT(&token->val),
			token->start.line, token->start.col, 
			token->end.line, token->end.col);
	} else {
		for(i = 0, j = 0; i < token->val.len; i++) {
			switch(token->val.s[i]) {
			case '\n': buf[j++] = '\\'; buf[j++] = 'n'; break;
			case '\r': buf[j++] = '\\'; buf[j++] = 'r'; break;
			case '\t': buf[j++] = '\\'; buf[j++] = 't'; break;
			case '\0': buf[j++] = '\\'; buf[j++] = '0'; break;
			case '\\': buf[j++] = '\\'; buf[j++] = '\\'; break;
			default: buf[j++] = token->val.s[i];
			}
		}
		LM_DBG("token(%d, '%.*s', <%d,%d>-<%d,%d>)\n", 
			token->type, j, buf,
			token->start.line, token->start.col, 
			token->end.line, token->end.col);
		pkg_free(buf);
	}
#endif /* EXTRA_DEBUG */
}


int cfg_get_token(cfg_token_t* token, cfg_parser_t* st, unsigned int flags)
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
			if (flags & CFG_EXTENDED_ALPHA) {
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
					    st->file, st->line, st->col, c);
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
					    st->file, st->line, st->col, c);
					return -1;
				}
			}
			break;

		case ST_A:
			if (flags & CFG_EXTENDED_ALPHA) {
				if (IS_ALPHA(c) ||
				    IS_DELIM(c) ||
				    IS_QUOTE(c)) {
					PUSH(c);
				} else if (IS_ESCAPE(c)) {
					state = ST_AE;
				} else if (IS_COMMENT(c) || IS_EOL(c) || IS_WHITESPACE(c)) {
					look_ahead = c;
					RETURN(CFG_TOKEN_ALPHA);
				} else {
					ERR("%s:%d:%d: Invalid character 0x%x\n", 
					    st->file, st->line, st->col, c);
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
					RETURN(CFG_TOKEN_ALPHA);
				} else {
					ERR("%s:%d:%d: Invalid character 0x%x\n", 
					    st->file, st->line, st->col, c);
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
				    st->file, st->line, st->col, c);
				return -1;
			}
			state = ST_A;
			break;

		case ST_Q:
			if (IS_QUOTE(c)) {
				RETURN(CFG_TOKEN_STRING);
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
				    st->file, st->line, st->col, c);
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
				    st->file, st->line, st->col, c);
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
		return 1;

	case ST_A:
		RETURN(CFG_TOKEN_ALPHA);

	case ST_Q:
		ERR("%s:%d:%d: Premature end of file, missing closing quote in"
			" string constant\n", st->file, st->line, st->col);
		return -1;

	case ST_QE:
	case ST_E:
	case ST_AE:
		ERR("%s:%d:%d: Premature end of file, missing escaped character\n", 
		    st->file, st->line, st->col);
		return -1;
	}
	BUG("%s:%d:%d: Invalid state %d\n",
		st->file, st->line, st->col, state);
	return -1;
}


int cfg_parse_section(void* param, cfg_parser_t* st, unsigned int flags)
{
	cfg_token_t t;
	int ret;

	ret = cfg_parse_str(param, st, flags);
	if (ret < 0) return ret;
	if (ret > 0) {
		ERR("%s:%d:%d: Section name missing.\n",
			st->file, st->line, st->col);
		return ret;
	}

	ret = cfg_get_token(&t, st, flags);
	if (ret < 0) goto error;
	if (ret > 0) {
		ERR("%s:%d:%d: Closing ']' missing\n", st->file, st->line, st->col);
		goto error;
	}
	if (t.type != ']') {
		ERR("%s:%d:%d: Syntax error, ']' expected\n", 
		    st->file, t.start.line, t.start.col);
		goto error;
	}

	if (cfg_eat_eol(st, flags)) goto error;
	return 0;

 error:
	if (param && ((str*)param)->s) {
		if (flags & CFG_STR_PKGMEM) {
			pkg_free(((str*)param)->s);
			((str*)param)->s = NULL;
		} else if (flags & CFG_STR_SHMMEM) {
			shm_free(((str*)param)->s);
			((str*)param)->s = NULL;
		} else if (flags & CFG_STR_MALLOC) {
			free(((str*)param)->s);
			((str*)param)->s = NULL;
		}		
	}
	return -1;
}


static char* get_base_name(str* filename)
{
	char* tmp1, *tmp2, *res;
	int len;

	res = NULL;
	if ((tmp1 = as_asciiz(filename)) == NULL) {
		ERR("cfg_parser: No memory left\n");
		goto error;
	}
	
	if ((tmp2 = basename(tmp1)) == NULL) {
		ERR("cfg_parser: Error in basename\n");
		goto error;
	}
	len = strlen(tmp2);

	if ((res = pkg_malloc(len + 1)) == NULL) {
		ERR("cfg_parser: No memory left");
		goto error;
	}
	memcpy(res, tmp2, len + 1);
	pkg_free(tmp1);
	return res;

 error:
	if (tmp1) pkg_free(tmp1);
	return NULL;
}



/** intialize the config parser.
 * @param basedir - path to the config file name. If 0 the path
 *               (base directory) of the main ser.cfg file will be used, else
 *               basedir will be concatenated to the filename. It will be
 *               used only if filename is not an absolute path.
 * @param filename - config filename (can include path elements).
 * @return 0 on error, !=0 on success.
 */
cfg_parser_t* cfg_parser_init(str* basedir, str* filename)
{
	cfg_parser_t* st;
	char* pathname, *base, *abs_pathname;

	abs_pathname = NULL;
	pathname = filename->s;
	st = NULL;
	base = NULL;
	
	/* if basedir == 0 or != "" get_abs_pathname */
	if (basedir == 0  || basedir->len != 0) {
		if ((abs_pathname = get_abs_pathname(basedir, filename)) == NULL) {
			ERR("cfg_parser: Error while converting %.*s to absolute"
					" pathname\n", STR_FMT(filename));
			goto error;
		}
		pathname = abs_pathname;
	}

	if ((base = get_base_name(filename)) == NULL) goto error;

	if ((st = (cfg_parser_t*)pkg_malloc(sizeof(*st))) == NULL) {
		ERR("cfg_parser: No memory left\n");
		goto error;
	}
	memset(st, '\0', sizeof(*st));

	if ((st->f = fopen(pathname, "r")) == NULL) {
		ERR("cfg_parser: Unable to open file '%s'\n", pathname);
		goto error;
	}

	if (abs_pathname) pkg_free(abs_pathname);

	st->file = base;
	st->line = 1;
	st->col = 0;
	return st;

 error:
	if (st) {
		if (st->f) fclose(st->f);
		pkg_free(st);
	}
	if (base) pkg_free(base);
	if (abs_pathname) pkg_free(abs_pathname);
	return NULL;
}


void cfg_parser_close(cfg_parser_t* st)
{
	if (!st) return;
	if (st->f) fclose(st->f);
	if (st->file) pkg_free(st->file);
	pkg_free(st);
}


void cfg_section_parser(cfg_parser_t* st, cfg_func_f parser, void* param)
{
	if (st == NULL) return;
	st->section.parser = parser;
	st->section.param = param;
}


void cfg_set_options(cfg_parser_t* st, cfg_option_t* options)
{
	if (st) st->options = options;
}


static int process_option(cfg_parser_t* st, cfg_option_t* opt)
{
	if (opt->f) {
		/* We have a function so store it and pass opt->dst to it */
		if (opt->f(opt->param, st, opt->flags) < 0) return -1;
	} else {
		/* We have no function, so if we have a pointer to some
		 * variable in opt->param then store the value of opt->i
		 * there, the variable is assumed to be of type i
		 */
		if (opt->param) *(int*)opt->param = opt->val;
	}
	return 0;
}


int sr_cfg_parse(cfg_parser_t* st)
{
	int ret;
	cfg_token_t t;
	cfg_option_t* opt;

	while(1) {
		ret = cfg_get_token(&t, st, 0);
		if (ret < 0) return ret;
		if (ret > 0) break;

		switch(t.type) {
		case CFG_TOKEN_ALPHA:
			/* Lookup the option name */
			if ((opt = cfg_lookup_token(st->options, &t.val)) == NULL) {
				ERR("%s:%d:%d: Unsupported option '%.*s'\n", 
				    st->file, t.start.line, t.start.col, STR_FMT(&t.val));
				return -1;
			}

			st->cur_opt = &t;
			if (process_option(st, opt) < 0) { st->cur_opt = 0; return -1; }
			st->cur_opt = 0;
			break;

		case '[': 
			if (st->section.parser == NULL) {
				ERR("%s:%d:%d: Syntax error\n", st->file,
					t.start.line, t.start.col);
				return -1;
			}
			if (st->section.parser(st->section.param, st, 0) < 0) return -1;
			break;

			     /* Empty line */
		case '\n': continue;

		default:
			ERR("%s:%d:%d: Syntax error\n", 
			    st->file, t.start.line, t.start.col);
			return -1;
		}
	}
	return 0;
}


cfg_option_t* cfg_lookup_token(cfg_option_t* table, str* token)
{
	int len, i;
	int (*cmp)(const char* s1, const char* s2, size_t n) = NULL;


	if (table == NULL) return NULL;

	for(i = 0; table[i].name; i++) {
		len = strlen(table[i].name);

		if (table[i].flags & CFG_PREFIX) {
			if (token->len < len) continue;
		} else {
			if (token->len != len) continue;
		}

		if (table[i].flags & CFG_CASE_SENSITIVE) cmp = strncmp;
		else cmp = strncasecmp;

		if (cmp(token->s, table[i].name, len)) continue;
		return table + i;
	}
	if (table[i].flags & CFG_DEFAULT) {
		return table + i;
	}
	return NULL;
}


int cfg_eat_equal(cfg_parser_t* st, unsigned int flags)
{
	cfg_token_t t;
	int ret;

	ret = cfg_get_token(&t, st, flags);
	if (ret < 0) return ret;
	if (ret > 0) {
		ERR("%s:%d:%d: Delimiter '=' missing\n", 
		    st->file, st->line, st->col);
		return ret;
	}

	if (t.type != '=') {
		ERR("%s:%d:%d: Syntax error, '=' expected\n", 
		    st->file, t.start.line, t.start.col);
		return -1;
	}
	return 0;
}


int cfg_eat_eol(cfg_parser_t* st, unsigned int flags)
{
	cfg_token_t t;
	int ret;

	/* Skip EOL */
	ret = cfg_get_token(&t, st, 0);
	if (ret < 0) return ret;
	if (ret > 0) return 0;
	if (t.type != '\n') {
		ERR("%s:%d:%d: End of line expected\n", 
			st->file, t.start.line, t.start.col);
		return -1;
	}
	return 0;
}


int cfg_parse_enum(void* param, cfg_parser_t* st, unsigned int flags)
{
	int ret;
    cfg_token_t t;
	cfg_option_t* values, *val;
	
	values = (cfg_option_t*)param;

	ret = cfg_get_token(&t, st, flags);
	if (ret != 0) return ret;

	if (t.type != CFG_TOKEN_ALPHA && t.type != CFG_TOKEN_STRING) {
		ERR("%s:%d:%d: Invalid enum value '%.*s'\n",
		    st->file, t.start.line, t.start.col, STR_FMT(&t.val));
		return -1;
	}

	if (values) {
		if ((val = cfg_lookup_token(values, &t.val)) == NULL) {
			ERR("%s:%d:%d Unsupported enum value '%.*s'\n", 
				st->file, t.start.line, t.start.col, STR_FMT(&t.val));
			return -1;
		}
		return process_option(st, val);
	} else {
		return 0;
	}
}


int cfg_parse_enum_opt(void* param, cfg_parser_t* st, unsigned int flags)
{
	int ret;

	if (cfg_eat_equal(st, flags)) return -1;

	ret = cfg_parse_enum(param, st, CFG_EXTENDED_ALPHA | flags);
	if (ret > 0) {
		ERR("%s:%d:%d: Option value missing\n",
		    st->file, st->line, st->col);
		return ret;
	} else if (ret < 0) return ret;

	if (cfg_eat_eol(st, flags)) return -1;
	return 0;
}


int cfg_parse_str(void* param, cfg_parser_t* st, unsigned int flags)
{
	str* val;
	int ret;
	char* buf;
    cfg_token_t t;
	
	ret = cfg_get_token(&t, st, flags);
	if (ret != 0) return ret;
	
	if (t.type != CFG_TOKEN_ALPHA && t.type != CFG_TOKEN_STRING) {
		ERR("%s:%d:%d: Invalid string value '%.*s', a string expected.\n",
		    st->file, t.start.line, t.start.col, STR_FMT(&t.val));
		return -1;
	}

	if (!param) return 0;
	val = (str*)param;

	if (flags & CFG_STR_STATIC) {
		if (!val->s || val->len <= t.val.len) {
			ERR("%s:%d:%d: Destination string buffer too small\n",
				st->file, t.start.line, t.start.col);
			return -1;
		}
		buf = val->s;
	} else if (flags & CFG_STR_SHMMEM) {
		if ((buf = shm_malloc(t.val.len + 1)) == NULL) {
			ERR("%s:%d:%d: Out of shared memory\n", st->file,
				t.start.line, t.start.col);
			return -1;
		}
		if (val->s) shm_free(val->s);
	} else if (flags & CFG_STR_MALLOC) {
		if ((buf = malloc(t.val.len + 1)) == NULL) {
			ERR("%s:%d:%d: Out of malloc memory\n", st->file,
				t.start.line, t.start.col);
			return -1;
		}
		if (val->s) free(val->s);
	} else if (flags & CFG_STR_PKGMEM) {
		if ((buf = pkg_malloc(t.val.len + 1)) == NULL) {
			ERR("%s:%d:%d: Out of private memory\n", st->file,
				t.start.line, t.start.col);
			return -1;
		}
		if (val->s) pkg_free(val->s);
	} else {
		*val = t.val;
		return 0;
	}
	
	memcpy(buf, t.val.s, t.val.len);
	buf[t.val.len] = '\0';
	val->s = buf;
	val->len = t.val.len;
	return 0;
}


int cfg_parse_str_opt(void* param, cfg_parser_t* st, unsigned int flags)
{
	int ret;

	if (cfg_eat_equal(st, flags)) return -1;

	ret = cfg_parse_str(param, st, flags | CFG_EXTENDED_ALPHA);
	if (ret > 0) {
		ERR("%s:%d:%d: Option value missing\n",
		    st->file, st->line, st->col);
	} else if (ret < 0) return ret;

	if (cfg_eat_eol(st, flags)) return -1;
	return 0;
}


int cfg_parse_int(void* param, cfg_parser_t* st, unsigned int flags)
{
	int* val;
	int ret, tmp;
	cfg_token_t t;

	val = (int*)param;

	ret = cfg_get_token(&t, st, flags);
	if (ret != 0) return ret;

	if (t.type != CFG_TOKEN_ALPHA && t.type != CFG_TOKEN_STRING) {
		ERR("%s:%d:%d: Invalid integer value '%.*s'\n", 
		    st->file, t.start.line, t.start.col, STR_FMT(&t.val));
		return -1;
	}

	if (str2sint(&t.val, &tmp) < 0) {
		ERR("%s:%d:%d: Invalid integer value '%.*s'\n",
			st->file, t.start.line, t.start.col, STR_FMT(&t.val));
		return -1;
	}

	if (val) *val = tmp;
	return 0;
}


int cfg_parse_int_opt(void* param, cfg_parser_t* st, unsigned int flags)
{
	int ret;

	if (cfg_eat_equal(st, flags)) return -1;

	ret = cfg_parse_int(param, st, flags);
	if (ret > 0) {
		ERR("%s:%d:%d: Option value missing\n", 
		    st->file, st->line, st->col);
	} else if (ret < 0) return ret;

	if (cfg_eat_eol(st, flags)) return -1;
	return 0;
}


int cfg_parse_bool(void* param, cfg_parser_t* st, unsigned int flags)
{
	int ret, *val;
	cfg_token_t t;
	cfg_option_t* map;
	
	val = (int*)param;

	ret = cfg_get_token(&t, st, flags);
	if (ret != 0) return ret;

	if (t.type != CFG_TOKEN_ALPHA && t.type != CFG_TOKEN_STRING) {
		ERR("%s:%d:%d: Invalid option value '%.*s', boolean expected\n", 
		    st->file, t.start.line, t.start.col, STR_FMT(&t.val));
		return -1;
	}

	if ((map = cfg_lookup_token(cfg_bool_values, &t.val)) == NULL) {
		ERR("%s:%d:%d: Invalid option value '%.*s', boolean expected\n", 
		    st->file, t.start.line, t.start.col, STR_FMT(&t.val));
		return -1;
	}

	if (val) *val = map->val;
	return 0;
}


int cfg_parse_bool_opt(void* param, cfg_parser_t* st, unsigned int flags)
{
	int ret;
	if (cfg_eat_equal(st, flags)) return -1;

    ret = cfg_parse_bool(param, st, CFG_EXTENDED_ALPHA | flags);
	if (ret > 0) {
		ERR("%s:%d:%d: Option value missing\n", 
		    st->file, st->line, st->col);
	} else if (ret < 0) return ret;

	if (cfg_eat_eol(st, flags)) return -1;
	return 0;
}
