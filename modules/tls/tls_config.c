/*
 * $Id$
 *
 * TLS module - Configuration file parser
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

#include <stdio.h>
#include <libgen.h>
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../trim.h"
#include "../../ut.h"
#include "../../resolve.h"
#include "tls_config.h"
#include "tls_util.h"
#include "tls_domain.h"

#define MAX_TOKEN_LEN 256


/*
 * Parser state
 */
static struct {
	FILE* f;
	char* file;
	int line;
	int col;

	tls_cfg_t* cfg;       /* Current configuration data */
	tls_domain_t* domain; /* Current domain in the configuration data */
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

static struct parser_tab option_name[];
static struct parser_tab token_method[];
static struct parser_tab token_bool[];
static struct parser_tab token_type[];
static struct parser_tab token_default[];

/*
 * States of lexical scanner
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
	    pstate.file, pstate.line, pstate.col); \
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


int tls_lex(token_t* token, unsigned int flags)
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

	ret = tls_lex(&t, 0);
	if (ret < 0) return -1;
	if (ret == 0) {
		LOG(L_ERR, "ERROR:%s:%d:%d: Option value missing\n", 
		    pstate.file, token->start.line, token->start.col);
		return -1;
	}

	if (t.type != '=') {
		LOG(L_ERR, "ERROR:%s:%d:%d: Syntax error, '=' expected\n", 
		    pstate.file, t.start.line, t.start.col);
		return -1;
	}

	ret = tls_lex(&t, EXTENDED_ALPHA_TOKEN);
	if (ret < 0) return -1;
	if (ret == 0) {
		LOG(L_ERR, "ERROR:%s:%d:%d: Option value missing\n",
		    pstate.file, t.start.line, t.start.col);
		return -1;
	}	

	if (t.type != TOKEN_ALPHA && t.type != TOKEN_STRING) {
		LOG(L_ERR, "ERROR:%s:%d:%d: Invalid option value '%.*s'\n", 
		    pstate.file, t.start.line, t.start.col,
		    t.val.len, ZSW(t.val.s));
		return -1;
	}

	*res = t.val;;
	return 0;
}


/*
 * Parse method option
 */
static int parse_method_opt(token_t* token)
{
	int ret;
	token_t t;
	struct parser_tab* r;

	ret = tls_lex(&t, 0);
	if (ret < 0) return -1;
	if (ret == 0) {
		LOG(L_ERR, "ERROR:%s:%d:%d: Option value missing\n", 
		    pstate.file, token->start.line, token->start.col);
		return -1;
	}

	if (t.type != '=') {
		LOG(L_ERR, "ERROR:%s:%d:%d: Syntax error, '=' expected\n", 
		    pstate.file, t.start.line, t.start.col);
		return -1;
	}

	ret = tls_lex(&t, EXTENDED_ALPHA_TOKEN);
	if (ret < 0) return -1;
	if (ret == 0) {
		LOG(L_ERR, "ERROR:%s:%d:%d: Option value missing\n",
		    pstate.file, t.start.line, t.start.col);
		return -1;
	}	

	if (t.type != TOKEN_ALPHA && t.type != TOKEN_STRING) {
		LOG(L_ERR, "ERROR:%s:%d:%d: Invalid option value '%.*s'\n", 
		    pstate.file, t.start.line, t.start.col,
		    t.val.len, ZSW(t.val.s));
		return -1;
	}

	r = lookup_token(token_method, &t.val);
	if (!r) {
		LOG(L_ERR, "ERROR:%s:%d:%d: Invalid option value '%.*s'\n", 
		    pstate.file, t.start.line, t.start.col,
		    t.val.len, ZSW(t.val.s));
		return -1;
	}

	pstate.domain->method = r->u.ival;
	return 0;
}


/*
 * Parse boolean option value
 */
static int parse_bool_val(int* res, token_t* token)
{
	int ret;
	token_t t;
	struct parser_tab* r;
	
	ret = tls_lex(&t, 0);
	if (ret < 0) return -1;

	     /* No token or EOL means that the option did not have value, and
	      * since we are parsing a boolean option we would assume true
	      */
	if (ret == 0 || t.type == '\n') {
		*res = 1;
		return 0;
	}
	
	if (t.type != '=') {
		LOG(L_ERR, "ERROR:%s:%d:%d: Syntax error, '=' expected\n", 
		    pstate.file, t.start.line, t.start.col);
		return -1;
	}

	ret = tls_lex(&t, EXTENDED_ALPHA_TOKEN);
	if (ret < 0) return -1;
	if (ret == 0) {
		LOG(L_ERR, "ERROR:%s:%d:%d: Option value missing\n", 
		    pstate.file, t.start.line, t.start.col);
		return -1;
	}

	if (t.type != TOKEN_ALPHA && t.type != TOKEN_STRING) {
		LOG(L_ERR, "ERROR:%s:%d:%d: Invalid option value '%.*s', boolean expected\n", 
		    pstate.file, t.start.line, t.start.col,
		    t.val.len, ZSW(t.val.s));
		return -1;
	}

	r = lookup_token(token_bool, &t.val);
	if (!r) {
		LOG(L_ERR, "ERROR:%s:%d:%d: Invalid option value '%.*s', boolean expected\n", 
		    pstate.file, t.start.line, t.start.col,
		    t.val.len, ZSW(t.val.s));
		return -1;
	}
	
	*res = r->u.ival;
	return 0;

}


static int parse_verify_cert_opt(token_t* token)
{
	int ret;
	if (parse_bool_val(&ret, token) < 0) return -1;
	pstate.domain->verify_cert  = ret;
	return 0;
}


static int parse_verify_depth_opt(token_t* token)
{
	unsigned int val;
	int ret;
	token_t t;

	ret = tls_lex(&t, 0);
	if (ret < 0) return -1;
	if (ret == 0) {
		LOG(L_ERR, "ERROR:%s:%d:%d: Option value missing\n", 
		    pstate.file, token->start.line, token->start.col);
		return -1;
	}

	if (t.type != '=') {
		LOG(L_ERR, "ERROR:%s:%d:%d: Syntax error, '=' expected\n", 
		    pstate.file, t.start.line, t.start.col);
		return -1;
	}

	ret = tls_lex(&t, EXTENDED_ALPHA_TOKEN);
	if (ret < 0) return -1;
	if (ret == 0) {
		LOG(L_ERR, "ERROR:%s:%d:%d: Option value missing\n", 
		    pstate.file, t.start.line, t.start.col);
		return -1;
	}	

	if (t.type != TOKEN_ALPHA) {
		LOG(L_ERR, "ERROR:%s:%d:%d: Invalid option value '%.*s', number expected\n", 
		    pstate.file, t.start.line, t.start.col,
		    t.val.len, ZSW(t.val.s));
		return -1;
	}

	if (str2int(&t.val, &val) < 0) {
		LOG(L_ERR, "ERROR:%s:%d:%d: Invalid number '%.*s'\n", 
		    pstate.file, t.start.line, t.start.col,
		    t.val.len, ZSW(t.val.s));

		return -1;
	}
	pstate.domain->verify_depth = val;
	return 0;
}


static int parse_req_cert_opt(token_t* token)
{
	int ret;
	if (parse_bool_val(&ret, token) < 0) return -1;
	pstate.domain->require_cert  = ret;
	return 0;
}


/*
 * Parse filename value, the function would add CFG_DIR prefix
 * if the filename does not start with /. The result is allocated
 * using shm_malloc and must be freed using shm_free
 */
static char* parse_file_val(token_t* token)
{
	char* file, *res;
	str val;
	if (parse_string_val(&val, token) < 0) return 0;
	file = get_pathname(&val);
	if (!file) return 0;
	if (shm_asciiz_dup(&res, file) < 0) {
		pkg_free(file);
		return 0;
	}
	pkg_free(file);
	return res;
}


static int parse_pkey_opt(token_t* token)
{
	char* file;
	file = parse_file_val(token);
	if (!file) return -1;
	pstate.domain->pkey_file = file;
	return 0;
}

static int parse_ca_list_opt(token_t* token)
{
	char* file;
	file = parse_file_val(token);
	if (!file) return -1;
	pstate.domain->ca_file = file;
	return 0;
}

static int parse_cert_opt(token_t* token)
{
	char* file;
	file = parse_file_val(token);
	if (!file) return -1;
	pstate.domain->cert_file = file;
	return 0;
}

static int parse_cipher_list_opt(token_t* token)
{
	str val;
	if (parse_string_val(&val, token) < 0) return -1;
	if (shm_str_dup(&pstate.domain->cipher_list, &val) < 0) return -1;
	return 0;
}

static int parse_ipv6(struct ip_addr* ip, token_t* token)
{
	int ret;
	token_t t;
	struct ip_addr* ipv6;
	str ip6_str;

	ip6_str.s=t.val.s;
	while(1) {
		ret = tls_lex(&t, 0);
		if (ret <= 0) goto err;
		if (t.type == ']') break;
		if (t.type != TOKEN_ALPHA && t.type != ':') goto err;
	}
	ip6_str.len=(int)(long)(t.val.s-ip6_str.s);

	ipv6=str2ip6(&ip6_str);
	if (ipv6==0)  goto err;
	*ip=*ipv6;
	return 0;
 err:
	LOG(L_ERR, "ERROR:%s:%d:%d: Invalid IPv6 address\n", 
	    pstate.file, token->start.line, token->start.col);
	return -1;
}

static int parse_ipv4(struct ip_addr* ip, token_t* token)
{
	int ret, i;
	token_t  t;
	unsigned int v;

	ip->af = AF_INET;
	ip->len = 4;

	if (str2int(&token->val, &v) < 0) goto err;
	if (v < 0 || v > 255) goto err;

	ip->u.addr[0] = v;

	for(i = 1; i < 4; i++) {
		ret = tls_lex(&t, 0);
		if (ret < 0) return -1;
		if (ret == 0 || t.type != '.')  goto err;
		
		ret = tls_lex(&t, 0);
		if (ret < 0) return -1;
		if (ret == 0 || t.type != TOKEN_ALPHA) goto err;
		if (str2int(&t.val, &v) < 0)  goto err;
		if (v < 0 || v > 255) goto err;
		ip->u.addr[i] = v;
	}

	return 0;
 err:
	LOG(L_ERR, "ERROR:%s:%d:%d: Invalid IPv4 address\n", 
	    pstate.file, token->start.line, token->start.col);
	return -1;
}

static int parse_hostport(int* type, struct ip_addr* ip, unsigned int* port, token_t* token)
{
	int ret;
	token_t t;
	struct parser_tab* r;

	ret = tls_lex(&t, 0);
	if (ret < 0) return -1;
	if (ret == 0) {
		LOG(L_ERR, "ERROR:%s:%d:%d: Missing IP address\n", 
		    pstate.file, token->start.line, token->start.col);
		return -1;
	}

	if (t.type == '[') {
		if (parse_ipv6(ip, &t) < 0) return -1;
	} else if (t.type == TOKEN_ALPHA) {
		r = lookup_token(token_default, &t.val);
		if (r) {
			*type = TLS_DOMAIN_DEF;
			     /* Default domain */
			return 0;
		} else {
			if (parse_ipv4(ip, &t) < 0) return -1;
		}
	} else {
		LOG(L_ERR, "ERROR:%s:%d:%d: Syntax error, IP address expected\n", 
		    pstate.file, t.start.line, t.start.col);
		return -1;
	}
	*type = 0;

	     /* Parse port */
	ret = tls_lex(&t, 0);
	if (ret < 0) return -1;
	if (ret == 0) {
		LOG(L_ERR, "ERROR:%s:%d:%d: Syntax error, ':' expected\n", 
		    pstate.file, pstate.line, pstate.col);
		return -1;
	}

	if (t.type != ':') {
		LOG(L_ERR, "ERROR:%s:%d:%d: Syntax error, ':' expected\n", 
		    pstate.file, t.start.line, t.start.col);
		return -1;
	}	

	ret = tls_lex(&t, 0);
	if (ret < 0) return -1;
	if (ret == 0) {
		LOG(L_ERR, "ERROR:%s:%d:%d: Premature end of file, port number missing\n", 
		    pstate.file, t.start.line, t.start.col);
		return -1;
	}

	if (t.type != TOKEN_ALPHA || (str2int(&t.val, port) < 0)) {
		LOG(L_ERR, "ERROR:%s:%d:%d: Invalid port number '%.*s'\n", 
		    pstate.file, t.start.line, t.start.col,
		    t.val.len, ZSW(t.val.s));
		return -1;
	}		
	return 0;
}

static int parse_domain(token_t* token)
{
	token_t t;
	int ret;
	struct parser_tab* r;

	int type;
	struct ip_addr ip;
	unsigned int port;

	memset(&ip, 0, sizeof(struct ip_addr));

	ret = tls_lex(&t, 0);
	if (ret < 0) return -1;
	if (ret == 0) {
		LOG(L_ERR, "ERROR:%s:%d:%d: TLS domain type missing\n", 
		    pstate.file, token->start.line, token->start.col);
		return -1;
	}

	if (t.type != TOKEN_ALPHA || 
	    ((r = lookup_token(token_type, &t.val)) == NULL)) {
		LOG(L_ERR, "ERROR:%s:%d:%d: Invalid TLS domain type %d:'%.*s'\n", 
		    pstate.file, t.start.line, t.start.col,
		    t.type, t.val.len, ZSW(t.val.s));
		return -1;
	}
	
	ret = tls_lex(&t, 0);
	if (ret < 0) return -1;
	if (ret == 0) {
		LOG(L_ERR, "ERROR:%s:%d:%d: TLS domain IP address missing\n", 
		    pstate.file, t.start.line, t.start.col);
		return -1;
	}
	if (t.type != ':') {
		LOG(L_ERR, "ERROR:%s:%d:%d: Syntax error, ':' expected\n", 
		    pstate.file, t.start.line, t.start.col);
		return -1;
	}	

	port=0;
	if (parse_hostport(&type, &ip, &port, &t) < 0) return -1;

	ret = tls_lex(&t, 0);
	if (ret < 0) return -1;
	if (ret == 0) {
		LOG(L_ERR, "ERROR:%s:%d:%d: Closing ']' missing\n", 
		    pstate.file, t.start.line, t.start.col);
		return -1;
	}
	if (t.type != ']') {
		LOG(L_ERR, "ERROR:%s:%d:%d: Syntax error, ']' expected\n", 
		    pstate.file, t.start.line, t.start.col);
		return -1;
	}	

	pstate.domain = tls_new_domain(r->u.ival | type, &ip, port);
	if (!pstate.domain) {
		LOG(L_ERR, "ERROR:%s:%d: Cannot create TLS domain structure\n", 
		    pstate.file, token->start.line);
		return -1;
	}

	ret = tls_add_domain(pstate.cfg, pstate.domain);
	if (ret < 0) {
		LOG(L_ERR, "ERROR:%s:%d: Error while creating TLS domain structure\n", 
		    pstate.file, token->start.line);
		tls_free_domain(pstate.domain);
		return -1;
	} else if (ret == 1) {
		LOG(L_ERR, "ERROR:%s:%d: Duplicate TLS domain (appears earlier in the config file)\n", 
		    pstate.file, token->start.line);
		tls_free_domain(pstate.domain);
		return -1;
	}

	return 0;
}

static int parse_config(void)
{
	int ret;
	token_t token;
	struct parser_tab* r;

	while(1) {
		     /* Get option name */
		ret = tls_lex(&token, 0);
		if (ret <= 0) return ret;

		switch(token.type) {
		case TOKEN_ALPHA:
			if (!pstate.domain) {
				LOG(L_ERR, "ERROR:%s:%d: You need to specify TLS domain first\n", 
				    pstate.file, token.start.line);
				return -1;
			}

			     /* Lookup the option name */
			r = lookup_token(option_name, &token.val);
			if (!r) {
				LOG(L_ERR, "ERROR:%s:%d: Unsupported option '%.*s'\n", 
				    pstate.file, token.start.line, 
				    token.val.len, ZSW(token.val.s));
				return -1;
			}
			     /* Parse the option value */
			if (r->u.fval(&token) < 0) return -1;
			break;

		case '[': 
			if (parse_domain(&token) < 0) return -1;
			break;

			     /* Empty line */
		case '\n': continue;

		default:
			LOG(L_ERR, "ERROR:%s:%d:%d: Syntax error\n", 
			    pstate.file, token.start.line, token.start.col);
			return -1;
		}

		     /* Skip EOL */
		ret = tls_lex(&token, 0);
		if (ret <= 0) return ret;
		if (token.type != '\n') {
			LOG(L_ERR, "ERROR:%s:%d:%d: End of line expected\n", 
			    pstate.file, token.start.line, token.start.col);
			return -1;
		}
	}
	return 0;
}


/*
 * Create configuration structures from configuration file
 */
tls_cfg_t* tls_load_config(str* filename)
{
	char* file;

	file = get_pathname(filename);
	if (!file) return 0;

	pstate.f = fopen(file, "r");
	if (pstate.f == NULL) {
		ERR("Unable to open TLS config file '%s'\n", file);
		pkg_free(file);
		return 0;
	}
	pstate.file = basename(file);
	pstate.line = 1;
	pstate.col = 0;
	pstate.domain = 0;

	pstate.cfg = tls_new_cfg();
	if (!pstate.cfg) goto error;
	
	if (parse_config() < 0) goto error;

	fclose(pstate.f);
	pkg_free(file);
	return pstate.cfg;

 error:
	pkg_free(file);
	if (pstate.cfg) tls_free_cfg(pstate.cfg);
	fclose(pstate.f);
	return 0;
}

/*
 * Convert TLS method string to integer
 */
int tls_parse_method(str* method)
{
	struct parser_tab* r;

	if (!method) {
		ERR("BUG: Invalid parameter value\n");
		return -1;
	}

	r = lookup_token(token_method, method);
	if (!r) return -1;

	return r->u.ival;
}


static struct parser_tab token_type[] = {
	{STR_STATIC_INIT("server"), {.ival = TLS_DOMAIN_SRV}},
	{STR_STATIC_INIT("srv"),    {.ival = TLS_DOMAIN_SRV}},
	{STR_STATIC_INIT("s"),      {.ival = TLS_DOMAIN_SRV}},
	{STR_STATIC_INIT("client"), {.ival = TLS_DOMAIN_CLI}},
	{STR_STATIC_INIT("cli"),    {.ival = TLS_DOMAIN_CLI}},
	{STR_STATIC_INIT("c"),      {.ival = TLS_DOMAIN_CLI}}, 
	{STR_NULL}
};


static struct parser_tab token_bool[] = { 
	{STR_STATIC_INIT("yes"),      {.ival = 1}},
	{STR_STATIC_INIT("true"),     {.ival = 1}},
	{STR_STATIC_INIT("enable"),   {.ival = 1}},
	{STR_STATIC_INIT("enabled"),  {.ival = 1}},
	{STR_STATIC_INIT("1"),        {.ival = 1}},
	{STR_STATIC_INIT("on"),       {.ival = 1}},
	{STR_STATIC_INIT("no"),       {.ival = 0}},
	{STR_STATIC_INIT("false"),    {.ival = 0}},
	{STR_STATIC_INIT("disable"),  {.ival = 0}},
	{STR_STATIC_INIT("disabled"), {.ival = 0}},
	{STR_STATIC_INIT("0"),        {.ival = 0}},
	{STR_STATIC_INIT("off"),      {.ival = 0}},
	{STR_NULL}
};

static struct parser_tab token_default[] = { 
	{STR_STATIC_INIT("default")},
	{STR_STATIC_INIT("def")},
	{STR_STATIC_INIT("*")},
	{STR_NULL}
};

static struct parser_tab token_method[] = { 
	{STR_STATIC_INIT("SSLv2"),  {.ival = TLS_USE_SSLv2}},
	{STR_STATIC_INIT("SSLv3"),  {.ival = TLS_USE_SSLv3}},
	{STR_STATIC_INIT("SSLv23"), {.ival = TLS_USE_SSLv23}},
	{STR_STATIC_INIT("TLSv1"),  {.ival = TLS_USE_TLSv1}},
	{STR_NULL}
};

static struct parser_tab option_name[] = {
	{STR_STATIC_INIT("method"),              {.fval = parse_method_opt}},
	{STR_STATIC_INIT("tls_method"),          {.fval = parse_method_opt}},
	{STR_STATIC_INIT("verify_certificate"),  {.fval = parse_verify_cert_opt}},
	{STR_STATIC_INIT("verify_cert"),         {.fval = parse_verify_cert_opt}},
	{STR_STATIC_INIT("verify_depth"),        {.fval = parse_verify_depth_opt}},
	{STR_STATIC_INIT("require_certificate"), {.fval = parse_req_cert_opt}},
	{STR_STATIC_INIT("require_cert"),        {.fval = parse_req_cert_opt}},
	{STR_STATIC_INIT("private_key"),         {.fval = parse_pkey_opt}},
	{STR_STATIC_INIT("pkey_file"),           {.fval = parse_pkey_opt}},
	{STR_STATIC_INIT("ca_list"),             {.fval = parse_ca_list_opt}},
	{STR_STATIC_INIT("calist_file"),         {.fval = parse_ca_list_opt}},
	{STR_STATIC_INIT("certificate"),         {.fval = parse_cert_opt}},
	{STR_STATIC_INIT("cert_file"),           {.fval = parse_cert_opt}},
	{STR_STATIC_INIT("cipher_list"),         {.fval = parse_cipher_list_opt}},
	{STR_NULL}
};
