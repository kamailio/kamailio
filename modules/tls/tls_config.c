/*
 * $Id$
 *
 * TLS module - Configuration file parser
 *
 * Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2004,2005 Free Software Foundation, Inc.
 * Copyright (C) 2005,2006 iptelorg GmbH
 *
 * This file is part of SIP-router, a free SIP server.
 *
 * SIP-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
/*!
 * \file
 * \brief SIP-router TLS support :: Configuration file parser
 * \ingroup tls
 * Module: \ref tls
 */


#include "tls_config.h"
#include "tls_domain.h"
#include "tls_mod.h"
#include "tls_util.h"

#include "../../cfg_parser.h"

#include "../../resolve.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../trim.h"
#include "../../ut.h"
#include "../../cfg/cfg.h"

static tls_domains_cfg_t* cfg = NULL;
static tls_domain_t* domain = NULL;

#ifdef USE_IPV6
static int parse_ipv6(struct ip_addr* ip, cfg_token_t* token, 
					  cfg_parser_t* st)
{
	int ret;
	cfg_token_t t;
	struct ip_addr* ipv6;
	str ip6_str;

	while(1) {
		ret = cfg_get_token(&t, st, 0);
		if (ret != 0) goto err;
		if (t.type == ']') break;
		if (t.type != CFG_TOKEN_ALPHA && t.type != ':') goto err;
	}
	ip6_str.s = t.val.s;
	ip6_str.len = (int)(long)(t.val.s - ip6_str.s);

	ipv6 = str2ip6(&ip6_str);
	if (ipv6 == 0) goto err;
	*ip = *ipv6;
	return 0;

 err:
	ERR("%s:%d:%d: Invalid IPv6 address\n", 
	    st->file, token->start.line, token->start.col);
	return -1;
}
#endif /* USE_IPV6 */


static int parse_ipv4(struct ip_addr* ip, cfg_token_t* token, 
					  cfg_parser_t* st)
{
	int ret, i;
	cfg_token_t  t;
	unsigned int v;

	ip->af = AF_INET;
	ip->len = 4;

	if (str2int(&token->val, &v) < 0) goto err;
	if (v > 255) goto err;

	ip->u.addr[0] = v;

	for(i = 1; i < 4; i++) {
		ret = cfg_get_token(&t, st, 0);
		if (ret < 0) return -1;
		if (ret > 0 || t.type != '.')  goto err;
		
		ret = cfg_get_token(&t, st, 0);
		if (ret < 0) return -1;
		if (ret > 0 || t.type != CFG_TOKEN_ALPHA) goto err;
		if (str2int(&t.val, &v) < 0)  goto err;
		if (v > 255) goto err;
		ip->u.addr[i] = v;
	}

	return 0;
 err:
	ERR("%s:%d:%d: Invalid IPv4 address\n", 
	    st->file, token->start.line, token->start.col);
	return -1;
}


static cfg_option_t methods[] = { 
	{"SSLv2",  .val = TLS_USE_SSLv2},
	{"SSLv3",  .val = TLS_USE_SSLv3},
	{"SSLv23", .val = TLS_USE_SSLv23},
	{"TLSv1",  .val = TLS_USE_TLSv1},
	{0}
};


static cfg_option_t domain_types[] = {
	{"server", .val = TLS_DOMAIN_SRV},
	{"srv",    .val = TLS_DOMAIN_SRV},
	{"s",      .val = TLS_DOMAIN_SRV},
	{"client", .val = TLS_DOMAIN_CLI},
	{"cli",    .val = TLS_DOMAIN_CLI},
	{"c",      .val = TLS_DOMAIN_CLI}, 
	{0}
};


static cfg_option_t token_default[] = { 
	{"default"},
	{"def"},
	{"*"},
	{0}
};


static cfg_option_t options[] = {
	{"method",              .param = methods, .f = cfg_parse_enum_opt},
	{"tls_method",          .param = methods, .f = cfg_parse_enum_opt},
	{"verify_certificate",  .f = cfg_parse_bool_opt},
	{"verify_cert",         .f = cfg_parse_bool_opt},
	{"verify_depth",        .f = cfg_parse_int_opt},
	{"require_certificate", .f = cfg_parse_bool_opt},
	{"require_cert",        .f = cfg_parse_bool_opt},
	{"private_key",         .f = cfg_parse_str_opt, .flags = CFG_STR_SHMMEM},
	{"pkey_file",           .f = cfg_parse_str_opt, .flags = CFG_STR_SHMMEM},
	{"calist_file",         .f = cfg_parse_str_opt, .flags = CFG_STR_SHMMEM},
	{"certificate",         .f = cfg_parse_str_opt, .flags = CFG_STR_SHMMEM},
	{"cert_file",           .f = cfg_parse_str_opt, .flags = CFG_STR_SHMMEM},
	{"cipher_list",         .f = cfg_parse_str_opt, .flags = CFG_STR_SHMMEM},
	{"ca_list",             .f = cfg_parse_str_opt, .flags = CFG_STR_SHMMEM},
	{"crl",                 .f = cfg_parse_str_opt, .flags = CFG_STR_SHMMEM},
	{0}
};


static void update_opt_variables(void)
{
	int i;
	for(i = 0; methods[i].name; i++) {
		methods[i].param = &domain->method;
	}
	options[2].param = &domain->verify_cert;
	options[3].param = &domain->verify_cert;
	options[4].param = &domain->verify_depth;
	options[5].param = &domain->require_cert;
	options[6].param = &domain->require_cert;
	options[7].param = &domain->pkey_file;
	options[8].param = &domain->pkey_file;
	options[9].param = &domain->ca_file;
	options[10].param = &domain->cert_file;
	options[11].param = &domain->cert_file;
	options[12].param = &domain->cipher_list;
	options[13].param = &domain->ca_file;
	options[14].param = &domain->crl_file;
}


static int parse_hostport(int* type, struct ip_addr* ip, unsigned int* port, 
						  cfg_token_t* token, cfg_parser_t* st)
{
	int ret;
	cfg_token_t t;
    cfg_option_t* opt;

	ret = cfg_get_token(&t, st, 0);
	if (ret < 0) return -1;
	if (ret > 0) {
		ERR("%s:%d:%d: Missing IP address\n", st->file, 
			token->start.line, token->start.col);
		return -1;
	}

	if (t.type == '[') {
#ifdef USE_IPV6
		if (parse_ipv6(ip, &t, st) < 0) return -1;
#else
		ERR("%s:%d:%d: IPv6 address  not supported (compiled without IPv6"
				" support)\n", 
		    st->file, t.start.line, t.start.col);
		return -1;
#endif /* USE_IPV6 */
	} else if (t.type == CFG_TOKEN_ALPHA) {
		opt = cfg_lookup_token(token_default, &t.val);
		if (opt) {
			*type = TLS_DOMAIN_DEF;
			     /* Default domain */
			return 0;
		} else {
			if (parse_ipv4(ip, &t, st) < 0) return -1;
		}
	} else {
		ERR("%s:%d:%d: Syntax error, IP address expected\n", 
		    st->file, t.start.line, t.start.col);
		return -1;
	}
	*type = 0;

	     /* Parse port */
	ret = cfg_get_token(&t, st, 0);
	if (ret < 0) return -1;
	if (ret > 0) {
		ERR("%s:%d:%d: Syntax error, ':' expected\n", st->file, st->line, 
			st->col);
		return -1;
	}
	
	if (t.type != ':') {
		ERR("%s:%d:%d: Syntax error, ':' expected\n", 
		    st->file, t.start.line, t.start.col);
		return -1;
	}	
	
	ret = cfg_get_token(&t, st, 0);
	if (ret < 0) return -1;
	if (ret > 0) {
		ERR("%s:%d:%d: Premature end of file, port number missing\n", 
		    st->file, t.start.line, t.start.col);
		return -1;
	}
	
	if (t.type != CFG_TOKEN_ALPHA || (str2int(&t.val, port) < 0)) {
		ERR("%s:%d:%d: Invalid port number '%.*s'\n", 
		    st->file, t.start.line, t.start.col, STR_FMT(&t.val));
		return -1;
	}		
	return 0;
}


static int parse_domain(void* param, cfg_parser_t* st, unsigned int flags)
{
	cfg_token_t t;
	int ret;
	cfg_option_t* opt;

	int type;
	struct ip_addr ip;
	unsigned int port;

	memset(&ip, 0, sizeof(struct ip_addr));

	ret = cfg_get_token(&t, st, 0);
	if (ret < 0) return -1;
	if (ret > 0) {
		ERR("%s:%d:%d: TLS domain type missing\n", 
		    st->file, st->line, st->col);
		return -1;
	}

	if (t.type != CFG_TOKEN_ALPHA || 
	    ((opt = cfg_lookup_token(domain_types, &t.val)) == NULL)) {
		ERR("%s:%d:%d: Invalid TLS domain type %d:'%.*s'\n", 
		    st->file, t.start.line, t.start.col, t.type, STR_FMT(&t.val));
		return -1;
	}
	
	ret = cfg_get_token(&t, st, 0);
	if (ret < 0) return -1;
	if (ret > 0) {
		ERR("%s:%d:%d: TLS domain IP address missing\n", 
		    st->file, st->line, st->col);
		return -1;
	}
	if (t.type != ':') {
		ERR("%s:%d:%d: Syntax error, ':' expected\n", 
		    st->file, t.start.line, t.start.col);
		return -1;
	}	

	port = 0;
	if (parse_hostport(&type, &ip, &port, &t, st) < 0) return -1;

	ret = cfg_get_token(&t, st, 0);
	if (ret < 0) return -1;
	if (ret > 0) {
		ERR("%s:%d:%d: Closing ']' missing\n", 
		    st->file, st->line, st->col);
		return -1;
	}
	if (t.type != ']') {
		ERR("%s:%d:%d: Syntax error, ']' expected\n", 
		    st->file, t.start.line, t.start.col);
		return -1;
	}

	if (cfg_eat_eol(st, flags)) return -1;

	if ((domain = tls_new_domain(opt->val | type, &ip, port)) == NULL) {
		ERR("%s:%d: Cannot create TLS domain structure\n", st->file, st->line);
		return -1;
	}

	ret = tls_add_domain(cfg, domain);
	if (ret < 0) {
		ERR("%s:%d: Error while creating TLS domain structure\n", st->file, 
			st->line);
		tls_free_domain(domain);
		return -1;
	} else if (ret == 1) {
		ERR("%s:%d: Duplicate TLS domain (appears earlier in the config file)\n", 
		    st->file, st->line);
		tls_free_domain(domain);
		return -1;
	}
	
	update_opt_variables();
	cfg_set_options(st, options);
	return 0;
}


/*
 * Create configuration structures from configuration file
 */
tls_domains_cfg_t* tls_load_config(str* filename)
{
	cfg_parser_t* parser;
	str empty;

	parser = NULL;
	if ((cfg = tls_new_cfg()) == NULL) goto error;

	empty.s = 0;
	empty.len = 0;
	if ((parser = cfg_parser_init(&empty, filename)) == NULL) {
		ERR("tls: Error while initializing configuration file parser.\n");
		goto error;
	}

	cfg_section_parser(parser, parse_domain, NULL);

	if (sr_cfg_parse(parser)) goto error;
	cfg_parser_close(parser);
	if (file_path) pkg_free(file_path);
	return cfg;

 error:
	if (parser) cfg_parser_close(parser);
	if (cfg) tls_free_cfg(cfg);
	return 0;
}


/*
 * Convert TLS method string to integer
 */
int tls_parse_method(str* method)
{
    cfg_option_t* opt;

    if (!method) {
        BUG("Invalid parameter value\n");
        return -1;
    }

    opt = cfg_lookup_token(methods, method);
    if (!opt) return -1;

    return opt->val;
}
