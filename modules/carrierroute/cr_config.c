/*
 * $Id$
 *
 * Copyright (C) 2007-2008 1&1 Internet AG
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
 */

/**
 * \file cr_config.c
 * \brief Functions for load and save routing data from a config file.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#include <confuse.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../ut.h"
#include "cr_config.h"
#include "carrierroute.h"
#include "cr_rule.h"
#include "cr_domain.h"
#include "cr_carrier.h"


/**
 * reports errors during config file parsing using LOG macro
 *
 * @param cfg points to the current config data structure
 * @param fmt a format string
 * @param ap format arguments
 */
static void conf_error(cfg_t *cfg, const char * fmt, va_list ap) {
	int ret;
	static char buf[1024];

	ret = vsnprintf(buf, sizeof(buf), fmt, ap);
	if (ret < 0 || ret >= sizeof(buf)) {
		LM_ERR("could not print error message\n");
	} else {
		// FIXME this don't seems to work reliable in all cases, charset 
		// problems
		LM_GEN1(L_ERR, "%s", buf);
	}
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
	if (cfg == NULL) {
		LM_ERR("could not initialize configuration\n");
		return NULL;
	}

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


static int backup_config(void) {
	FILE * from, * to;
	char * backup_file, ch;
	LM_INFO("start configuration backup\n");
	if((backup_file = pkg_malloc(strlen(config_file) + strlen (".bak") + 1)) == NULL){
		PKG_MEM_ERROR;
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
			goto errclose;
		}
		if (!feof(from)) fputc(ch, to);
		if (ferror(to)) {
			LM_ERR("Error writing destination file.\n");
			goto errclose;
		}
	}

	if (fclose(from)==EOF) {
		LM_ERR("Error closing source file.\n");
		fclose(to);
		goto errout;
	}

	if (fclose(to)==EOF) {
		LM_ERR("Error closing destination file.\n");
		goto errout;
	}
	LM_NOTICE("backup written to %s\n", backup_file);
	pkg_free(backup_file);
	return 0;
errclose:
	/* close the files so that resource leak is prevented ; ignore errors*/
	fclose(from);
	fclose(to);
errout:
	pkg_free(backup_file);
	return -1;
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
int load_config(struct route_data_t * rd) {
	cfg_t * cfg = NULL;
	int m, o, i, j, k,l, status, hash_index, max_targets, strip;
	cfg_t * d, * p, * t;
	struct carrier_data_t * tmp_carrier_data;
	int domain_id;
	str domain, prefix, rewrite_prefix, rewrite_suffix, rewrite_host, comment;
	double prob;
	int * backed_up = NULL;
	int backed_up_size, backup;
	backed_up_size = backup = 0;

	if ((cfg = parse_config()) == NULL) {
		return -1;
	}

	rd->carrier_num = 1;
	rd->first_empty_carrier = 0;
	rd->domain_num = cfg_size(cfg, "domain");

	if ((rd->carriers = shm_malloc(sizeof(struct carrier_data_t *))) == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}
	memset(rd->carriers, 0, sizeof(struct carrier_data_t *));

	/* Create carrier map */
	if ((rd->carrier_map = shm_malloc(sizeof(struct name_map_t))) == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}
	memset(rd->carrier_map, 0, sizeof(struct name_map_t));
	rd->carrier_map[0].id = 1;
	rd->carrier_map[0].name.len = default_tree.len;
	rd->carrier_map[0].name.s = shm_malloc(rd->carrier_map[0].name.len);
	if (rd->carrier_map[0].name.s == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}
	memcpy(rd->carrier_map[0].name.s, default_tree.s, rd->carrier_map[0].name.len);

	/* Create domain map */
	if ((rd->domain_map = shm_malloc(sizeof(struct name_map_t) * rd->domain_num)) == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}
	memset(rd->domain_map, 0, sizeof(struct name_map_t) * rd->domain_num);
	for (i=0; i<rd->domain_num; i++) {
		d = cfg_getnsec(cfg, "domain", i);
		domain.s = (char *)cfg_title(d);
		if (domain.s==NULL) domain.s="";
		domain.len = strlen(domain.s);
		rd->domain_map[i].id = i+1;
		rd->domain_map[i].name.len = domain.len;
		rd->domain_map[i].name.s = shm_malloc(rd->domain_map[i].name.len);
		if (rd->domain_map[i].name.s == NULL) {
			SHM_MEM_ERROR;
			return -1;
		}
		memcpy(rd->domain_map[i].name.s, domain.s, rd->domain_map[i].name.len);
	}
	/* sort domain map by id for faster access */
	qsort(rd->domain_map, rd->domain_num, sizeof(rd->domain_map[0]), compare_name_map);

	/* Create and insert carrier data structure */
	tmp_carrier_data = create_carrier_data(1, &rd->carrier_map[0].name, rd->domain_num);
	if (tmp_carrier_data == NULL) {
		LM_ERR("can't create new carrier\n");
		return -1;
	}
	if (add_carrier_data(rd, tmp_carrier_data) < 0) {
		LM_ERR("couldn't add carrier data\n");
		destroy_carrier_data(tmp_carrier_data);
		return -1;
	}

	/* add all routes */
	for (i = 0; i < rd->domain_num; i++) {
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
			if (str_strcasecmp(&prefix, &CR_EMPTY_PREFIX) == 0) {
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
				if (str_strcasecmp(&rewrite_host, &CR_EMPTY_PREFIX) == 0) {
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
						PKG_MEM_ERROR;
						return -1;
					}
					for (l = 0; l < backed_up_size; l++) {
						backed_up[l] = cfg_getnint(t, "backed_up", l);
					}
					backed_up[backed_up_size] = -1;
				}
				backup = cfg_getint(t, "backup");

				domain_id = map_name2id(rd->domain_map, rd->domain_num, &domain);
				if (domain_id < 0) {
					LM_ERR("cannot find id for domain '%.*s'", domain.len, domain.s);
					if (backed_up) {
						pkg_free(backed_up);
					}
					return -1;
				}

				LM_INFO("adding route for prefix %.*s, to host %.*s, prob %f, backed up: %i, backup: %i\n",
				    prefix.len, prefix.s, rewrite_host.len, rewrite_host.s, prob, backed_up_size, backup);
				if (add_route(rd, 1, domain_id, &prefix, 0, 0, max_targets, prob, &rewrite_host,
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
 * Does the work for save_config, traverses the routing data tree
 * and writes each rule to file.
 *
 * @param node the current prefix tree node
 * @param outfile the filehandle to which the config data is written
 *
 * @return 0 on success, -1 on failure
 */
static int save_route_data_recursor(struct dtrie_node_t * node, FILE * outfile) {
	int i;
	struct route_flags *rf;
	struct route_rule * rr;
	struct route_rule_p_list * rl;
	str *tmp_str;
	str null_str = str_init("NULL");

	/* no support for flag lists in route config */
	rf = (struct route_flags *)(node->data);
	if (rf && rf->rule_list) {
		rr = rf->rule_list;
		tmp_str = (rr->prefix.len ? &rr->prefix : &null_str);
		fprintf(outfile, "\tprefix %.*s {\n", tmp_str->len, tmp_str->s);
		fprintf(outfile, "\t\tmax_targets = %i\n\n", rf->max_targets);
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
	for (i = 0; i < cr_match_mode; i++) {
		if (node->child[i]) {
			if (save_route_data_recursor(node->child[i], outfile) < 0) {
				return -1;
			}
		}
	}
	return 0;
}


/**
 * Stores the routing data rd in config_file
 *
 * @param rd Pointer to the routing tree which shall be saved to file
 *
 * @return 0 means ok, -1 means an error occured
 */
int save_config(struct route_data_t * rd) {
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
	if (rd->carrier_num>=1) {
		for (j=0; j< rd->carriers[i]->domain_num; j++) {
			fprintf(outfile, "domain %.*s {\n", rd->carriers[i]->domains[j]->name->len, rd->carriers[i]->domains[j]->name->s);
			if (save_route_data_recursor(rd->carriers[i]->domains[j]->tree, outfile) < 0) {
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
