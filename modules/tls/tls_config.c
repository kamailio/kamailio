/*
 * TLS module
 *
 * Copyright (C) 2010 iptelorg GmbH
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*!
 * \file
 * \brief Kamailio TLS support :: Configuration file parser
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
#include "../../stats.h"

#include <dirent.h>
#include <sys/stat.h>

static tls_domains_cfg_t* cfg = NULL;
static tls_domain_t* domain = NULL;

static int parse_ipv6(struct ip_addr* ip, cfg_token_t* token, 
		cfg_parser_t* st)
{
	int ret;
	cfg_token_t t;
	struct ip_addr* ipv6;
	str ip6_str;
	char ip6_buff[IP_ADDR_MAX_STR_SIZE+3];

	ip6_buff[0] = '\0';
	while(1) {
		ret = cfg_get_token(&t, st, 0);
		if (ret != 0) goto err;
		if (t.type == ']') break;
		if (t.type != CFG_TOKEN_ALPHA && t.type != ':') goto err;
		strncat(ip6_buff, t.val.s, t.val.len);
	}
	ip6_str.s = ip6_buff;
	ip6_str.len = strlen(ip6_buff);
	LM_DBG("found IPv6 address [%.*s]\n", ip6_str.len, ip6_str.s);
	ipv6 = str2ip6(&ip6_str);
	if (ipv6 == 0) goto err;
	*ip = *ipv6;
	return 0;

err:
	ERR("%s:%d:%d: Invalid IPv6 address\n", 
			st->file, token->start.line, token->start.col);
	return -1;
}


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
	{"SSLv2",   .val = TLS_USE_SSLv2},
	{"SSLv3",   .val = TLS_USE_SSLv3},
	{"SSLv23",  .val = TLS_USE_SSLv23},
	{"TLSv1",   .val = TLS_USE_TLSv1},
	{"TLSv1.0", .val = TLS_USE_TLSv1},
	{"TLSv1+",  .val = TLS_USE_TLSv1_PLUS},
	{"TLSv1.0+", .val = TLS_USE_TLSv1_PLUS},
	{"TLSv1.1",  .val = TLS_USE_TLSv1_1},
	{"TLSv1.1+", .val = TLS_USE_TLSv1_1_PLUS},
	{"TLSv1.2",  .val = TLS_USE_TLSv1_2},
	{"TLSv1.2+", .val = TLS_USE_TLSv1_2_PLUS},
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
	{"server_name",         .f = cfg_parse_str_opt, .flags = CFG_STR_SHMMEM},
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
	options[15].param = &domain->server_name;
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
		if (parse_ipv6(ip, &t, st) < 0) return -1;
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
	struct stat file_status;
	char tmp_name[13] = "configXXXXXX";
	str filename_str;
	DIR *dir;
	struct dirent *ent;
	int out_fd, in_fd, filename_is_directory;
	char *file_path, ch;

	parser = NULL;
	memset(&file_status, 0, sizeof(struct stat));
	dir = (DIR *)NULL;
	in_fd = out_fd = filename_is_directory = 0;
	file_path = (char *)0;

	if ((cfg = tls_new_cfg()) == NULL) goto error;

	if (stat(filename->s, &file_status) != 0) {
		LOG(L_ERR, "cannot stat config file %s\n", filename->s);
		goto error;
	}
	if (S_ISDIR(file_status.st_mode)) {
		filename_is_directory = 1;
		dir = opendir(filename->s);
		if (dir == NULL) {
			LOG(L_ERR, "cannot open directory file %s\n", filename->s);
			goto error;
		}
		out_fd = mkstemp(&(tmp_name[0]));
		if (out_fd == -1) {
			LOG(L_ERR, "cannot make tmp file %s\n", &(tmp_name[0]));
			goto error;
		}
		while ((ent = readdir(dir)) != NULL) {
			if(file_path) pkg_free(file_path);
			file_path = pkg_malloc(filename->len + 1 + 256);
			memcpy(file_path, filename->s, filename->len);
			file_path[filename->len] = '/';
			strcpy(file_path + filename->len + 1, ent->d_name);
			if (stat(file_path, &file_status) != 0) {
				LOG(L_ERR, "cannot get status of config file %s\n",
						file_path);
				goto error;
			}
			if (S_ISREG(file_status.st_mode)) {
				in_fd = open(file_path, O_RDONLY);
				if (in_fd == -1) {
					LOG(L_ERR, "cannot open config file %s\n",
							file_path);
					goto error;
				}
				pkg_free(file_path);
				file_path = NULL;
				while (read(in_fd, &ch, 1)) {
					write(out_fd, &ch, 1);
				}
				close(in_fd);
				in_fd = 0;
				ch = '\n';
				write(out_fd, &ch, 1);
			}
		}
		closedir(dir);
		close(out_fd);
		dir = (DIR *)NULL;
		out_fd = 0;
	}

	empty.s = 0;
	empty.len = 0;
	if (filename_is_directory) {
		filename_str.s = &(tmp_name[0]);
		filename_str.len = strlen(&(tmp_name[0]));
		if ((parser = cfg_parser_init(&empty, &filename_str)) == NULL) {
			ERR("tls: Error while initializing configuration file parser.\n");
			unlink(&(tmp_name[0]));
			goto error;
		}
		unlink(&(tmp_name[0]));
	} else {
		if ((parser = cfg_parser_init(&empty, filename)) == NULL) {
			ERR("tls: Error while initializing configuration file parser.\n");
			goto error;
		}	
	}

	cfg_section_parser(parser, parse_domain, NULL);
	if (sr_cfg_parse(parser)) goto error;
	cfg_parser_close(parser);
	if (file_path) pkg_free(file_path);
	return cfg;

error:
	if (dir) closedir(dir);
	if (out_fd > 0) {
		close(out_fd);
		unlink(&(tmp_name[0]));
	}
	if (file_path) pkg_free(file_path);
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

#if OPENSSL_VERSION_NUMBER < 0x1000100fL
	if(opt->val == TLS_USE_TLSv1_1 || opt->val == TLS_USE_TLSv1_1_PLUS) {
		LM_ERR("tls v1.1 not supported by this libssl version: %ld\n",
				(long)OPENSSL_VERSION_NUMBER);
		return -1;
	}
#endif
#if OPENSSL_VERSION_NUMBER < 0x1000105fL
	if(opt->val == TLS_USE_TLSv1_2 || opt->val == TLS_USE_TLSv1_2_PLUS) {
		LM_ERR("tls v1.2 not supported by this libssl version: %ld\n",
				(long)OPENSSL_VERSION_NUMBER);
		return -1;
	}
#endif

	return opt->val;
}
