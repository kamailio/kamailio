/*
 * $Id$
 *
 * Copyright (C) 2007-2008 1&1 Internet AG
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * \file route_config.c
 * \brief Functions for load and save routing data from a config file.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#include <confuse.h>
#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../ut.h"
#include "route_config.h"
#include "route.h"
#include "carrierroute.h"
#include "carrier_tree.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static int save_route_data_recursor(struct route_tree_item * rt, FILE * outfile);

static cfg_t * parse_config();

static int backup_config();

/**
 * reports errors during config file parsing using LOG macro
 *
 * @param cfg points to the current config data structure
 * @param fmt a format string
 * @param ap format arguments
 */
void conf_error(cfg_t *cfg, const char * fmt, va_list ap) {
	// FIXME this don't seems to work reliable, produces strange error messages
	LM_GEN1(L_ERR, (char *) fmt, ap);
}


/**
 * Loads the routing data from the config file given in global
 * variable config_data and stores it in routing tree rd.
 *
 * @param rd Pointer to the route data tree where the routing data
 * shall be loaded into
 *
 * @return 0 means ok, -1 means an error occured
 *
 */
int load_config(struct rewrite_data * rd) {
	cfg_t * cfg = NULL;
	int n, m, o, i, j, k,l;
	cfg_t * d, * p, * t;
	str domain;
	str prefix;
	double prob;
	str rewrite_prefix;
	str rewrite_suffix;
	str rewrite_host;
	str comment;
	int backed_up_size = 0;
	int * backed_up = NULL;
	int backup = 0;
	int status, hash_index, max_targets, strip;

	if ((cfg = parse_config()) == NULL) {
		return -1;
	}

	if ((rd->carriers = shm_malloc(sizeof(struct carrier_tree *))) == NULL) {
		LM_ERR("out of shared memory\n");
		return -1;
	}
	memset(rd->carriers, 0, sizeof(struct carrier_tree *));

	rd->tree_num = 1;
	n = cfg_size(cfg, "domain");
	if (add_carrier_tree(&default_tree, 1, rd, n) == NULL) {
		LM_ERR("couldn't add carrier tree\n");
		return -1;
	}

	memset(rd->carriers[0]->trees, 0, sizeof(struct route_tree *) * n);
	for (i = 0; i < n; i++) {
		d = cfg_getnsec(cfg, "domain", i);
		domain.s = (char *)cfg_title(d);
		if (domain.s==NULL) domain.s="";
		domain.len = strlen(domain.s);
		m = cfg_size(d, "prefix");
		LM_INFO("loading domain %.*s\n", domain.len, domain.s);
		for (j = 0; j < m; j++) {
			p = cfg_getnsec(d, "prefix", j);
			prefix.s = (char *)cfg_title(p);
			if (prefix.s==NULL) prefix.s="";
			prefix.len = strlen(prefix.s);
			if (str_strcasecmp(&prefix, &SP_EMPTY_PREFIX) == 0) {
				prefix.s = "";
				prefix.len = 0;
			}
			LM_INFO("loading prefix %.*s\n", prefix.len, prefix.s);
			max_targets = cfg_getint(p, "max_targets");
			o = cfg_size(p, "target");
			for (k = 0; k < o; k++) {
				t = cfg_getnsec(p, "target", k);
				rewrite_host.s = (char *)cfg_title(t);
				if (rewrite_host.s==NULL) rewrite_host.s="";
				rewrite_host.len = strlen(rewrite_host.s);
				if (str_strcasecmp(&rewrite_host, &SP_EMPTY_PREFIX) == 0) {
					rewrite_host.s = "";
					rewrite_host.len = 0;
				}
				LM_INFO("loading target %.*s\n", rewrite_host.len, rewrite_host.s);
				prob = cfg_getfloat(t, "prob");
				strip = cfg_getint(t, "strip");
				rewrite_prefix.s = (char *)cfg_getstr(t, "rewrite_prefix");
				if (rewrite_prefix.s==NULL) rewrite_prefix.s="";
				rewrite_prefix.len = strlen(rewrite_prefix.s);
				rewrite_suffix.s = (char *)cfg_getstr(t, "rewrite_suffix");
				if (rewrite_suffix.s==NULL) rewrite_suffix.s="";
				rewrite_suffix.len = strlen(rewrite_suffix.s);
				hash_index = cfg_getint(t, "hash_index");
				comment.s = (char *)cfg_getstr(t, "comment");
				if (comment.s==NULL) comment.s="";
				comment.len = strlen(comment.s);
				status = cfg_getint(t, "status");
				if ((backed_up_size = cfg_size(t, "backed_up")) > 0) {
					if ((backed_up = pkg_malloc(sizeof(int) * (backed_up_size + 1))) == NULL) {
						LM_ERR("out of private memory\n");
						return -1;
					}
					for (l = 0; l < backed_up_size; l++) {
						backed_up[l] = cfg_getnint(t, "backed_up", l);
					}
					backed_up[backed_up_size] = -1;
				}
				backup = cfg_getint(t, "backup");
				LM_INFO("adding route for prefix %.*s, to host %.*s, prob %f, backed up: %i, backup: %i\n",
				    prefix.len, prefix.s, rewrite_host.len, rewrite_host.s, prob, backed_up_size, backup);
				if (add_route(rd, 1, &domain, &prefix, 0, 0, max_targets, prob, &rewrite_host,
				              strip, &rewrite_prefix, &rewrite_suffix, status,
				              hash_index, backup, backed_up, &comment) < 0) {
					LM_INFO("Error while adding route\n");
					if (backed_up) {
						pkg_free(backed_up);
					}
					return -1;
				}
				if (backed_up) {
					pkg_free(backed_up);
				}
				backed_up = NULL;
			}
		}

	}
	cfg_free(cfg);
	return 0;
}

/**
 * Parses the config file
 *
 * @return a pointer to the configuration data structure, NULL on failure
 */
static cfg_t * parse_config(void) {
	cfg_t * cfg = NULL;

	cfg_opt_t target_opts[] = {
	                              CFG_STR("comment", 0, CFGF_NONE),
	                              CFG_INT("strip", 0, CFGF_NONE),
	                              CFG_STR("rewrite_prefix", 0, CFGF_NONE),
	                              CFG_FLOAT("prob", 0, CFGF_NONE),
	                              CFG_INT("hash_index", 0, CFGF_NONE),
	                              CFG_STR("rewrite_suffix", 0, CFGF_NONE),
	                              CFG_INT("status", 1, CFGF_NONE),
	                              CFG_INT_LIST("backed_up", NULL, CFGF_NONE),
	                              CFG_INT("backup", -1, CFGF_NONE),
	                              CFG_END()
	                          };

	cfg_opt_t prefix_opts[] = {
	                              CFG_SEC("target", target_opts, CFGF_MULTI | CFGF_TITLE),
	                              CFG_INT("max_targets", -1, CFGF_NONE),
	                              CFG_END()
	                          };

	cfg_opt_t domain_opts[] = {
	                              CFG_SEC("prefix", prefix_opts, CFGF_MULTI | CFGF_TITLE),
	                              CFG_END()
	                          };

	cfg_opt_t opts[] = {
	                       CFG_SEC("domain", domain_opts, CFGF_MULTI | CFGF_TITLE),
	                       CFG_END()
	                   };

	cfg = cfg_init(opts, CFGF_NONE);

	cfg_set_error_function(cfg, conf_error);

	switch (cfg_parse(cfg, config_file)) {
		case CFG_FILE_ERROR: LM_ERR("file not found: %s\n", config_file);
			return NULL;
		case CFG_PARSE_ERROR: LM_ERR("error while parsing %s in line %i, section %s\n",
			                          cfg->filename, cfg->line, cfg->name);
			return NULL;
		case CFG_SUCCESS: break;
	}
	return cfg;
}


/**
 * Stores the routing data rd in config_file
 *
 * @param rd Pointer to the routing tree which shall be saved to file
 *
 * @return 0 means ok, -1 means an error occured
 */
int save_config(struct rewrite_data * rd) {
	FILE * outfile;
	int i,j;

	if(backup_config() < 0){
		return -1;
	}

	if ((outfile = fopen(config_file, "w")) == NULL) {
		LM_ERR("Could not open config file %s\n", config_file);
		return -1;
	}

	i = 0;
	if (rd->tree_num>=1) {
		for (j=0; j< rd->carriers[i]->tree_num; j++) {
			fprintf(outfile, "domain %.*s {\n", rd->carriers[i]->trees[j]->name.len, rd->carriers[i]->trees[j]->name.s);
			if (save_route_data_recursor(rd->carriers[i]->trees[j]->tree, outfile) < 0) {
				goto errout;
			}
			fprintf(outfile, "}\n\n");
		}
	}
	fclose(outfile);
	return 0;
errout:
	fclose(outfile);
	LM_ERR("Cannot save config file %s\n", config_file);
	return -1;
}

/**
 * Does the work for save_config, traverses the routing data tree
 * and writes each rule to file.
 *
 * @param rt the current route tree node
 * @param outfile the filehandle to which the config data is written
 *
 * @return 0 on success, -1 on failure
 */
static int save_route_data_recursor(struct route_tree_item * rt, FILE * outfile) {
	int i;
	struct route_rule * rr;
	struct route_rule_p_list * rl;
	str *tmp_str;
	str null_str = str_init("NULL");

	/* no support for flag lists in route config */
	if (rt->flag_list && rt->flag_list->rule_list) {
		rr = rt->flag_list->rule_list;
		tmp_str = (rr->prefix.len ? &rr->prefix : &null_str);
		fprintf(outfile, "\tprefix %.*s {\n", tmp_str->len, tmp_str->s);
		fprintf(outfile, "\t\tmax_targets = %i\n\n", rt->flag_list->max_targets);
		while (rr) {
			tmp_str = (rr->host.len ? &rr->host : &null_str);
			fprintf(outfile, "\t\ttarget %.*s {\n", tmp_str->len, tmp_str->s);
			fprintf(outfile, "\t\t\tprob = %f\n", rr->orig_prob);
			fprintf(outfile, "\t\t\thash_index = %i\n", rr->hash_index);
			fprintf(outfile, "\t\t\tstatus = %i\n", rr->status);
			if (rr->strip > 0) {
				fprintf(outfile, "\t\t\tstrip = \"%i\"\n", rr->strip);
			}
			if (rr->local_prefix.len) {
				fprintf(outfile, "\t\t\trewrite_prefix = \"%.*s\"\n", rr->local_prefix.len, rr->local_prefix.s);
			}
			if (rr->local_suffix.len) {
				fprintf(outfile, "\t\t\trewrite_suffix: \"%.*s\"\n", rr->local_suffix.len, rr->local_suffix.s);
			}
			if (rr->backup) {
				fprintf(outfile, "\t\t\tbackup = %i\n", rr->backup->hash_index);
			}
			if (rr->backed_up) {
				rl = rr->backed_up;
				fprintf(outfile, "\t\t\tbacked_up = {");
				i=0;
				while (rl) {
					if (i>0) {
						fprintf(outfile, ", ");
					}
					fprintf(outfile, "%i", rl->hash_index);
					rl = rl->next;
					i++;
				}
				fprintf(outfile, "}\n");
			}
			if (rr->comment.len) {
				fprintf(outfile, "\t\t\tcomment = \"%.*s\"\n", rr->comment.len, rr->comment.s);
			}
			fprintf(outfile, "\t\t}\n");
			rr = rr->next;
		}
		fprintf(outfile, "\t}\n");
	}
	for (i = 0; i < 10; i++) {
		if (rt->nodes[i]) {
			if (save_route_data_recursor(rt->nodes[i], outfile) < 0) {
				return -1;
			}
		}
	}
	return 0;
}

static int backup_config(void) {
	FILE * from, * to;
	char * backup_file, ch;
	LM_INFO("start configuration backup\n");
	if((backup_file = pkg_malloc(strlen(config_file) + strlen (".bak") + 1)) == NULL){
		LM_ERR("out of private memory\n");
		return -1;
	}
	if(!strcpy(backup_file, config_file)){
		LM_ERR("can't copy filename\n");
		goto errout;
	}
	if(!strcat(backup_file, ".bak")){
		LM_ERR("can't attach suffix\n");
		goto errout;
	}
	/* open source file */
	if ((from = fopen(config_file, "rb"))==NULL) {
		LM_ERR("Cannot open source file.\n");
		goto errout;
	}

	/* open destination file */
	if ((to = fopen(backup_file, "wb"))==NULL) {
		LM_ERR("Cannot open destination file.\n");
		fclose(from);
		goto errout;
	}

	/* copy the file */
	while (!feof(from)) {
		ch = fgetc(from);
		if (ferror(from)) {
			LM_ERR("Error reading source file.\n");
			goto errout;
		}
		if (!feof(from)) fputc(ch, to);
		if (ferror(to)) {
			LM_ERR("Error writing destination file.\n");
			goto errout;
		}
	}

	if (fclose(from)==EOF) {
		LM_ERR("Error closing source file.\n");
		goto errout;
	}

	if (fclose(to)==EOF) {
		LM_ERR("Error closing destination file.\n");
		goto errout;
	}
	LM_NOTICE("backup written to %s\n", backup_file);
	pkg_free(backup_file);
	return 0;
errout:
	pkg_free(backup_file);
	return -1;
}
