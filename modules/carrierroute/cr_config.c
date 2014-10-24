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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*!
 * \file cr_config.c
 * \brief Functions for load and save routing data from a config file.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

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
#include "parser_carrierroute.h"

enum target_opt_ids { TO_ID_COMMENT = 0, TO_ID_STRIP, TO_ID_REWR_PREFIX, TO_ID_PROB, TO_ID_HASH_INDEX,
					  TO_ID_REWR_SUFFIX, TO_ID_STATUS, TO_ID_BACKED_UP, TO_ID_BACKUP, TO_MAX_IDS };
enum prefix_opt_ids { PO_MAX_TARGETS = 0, PO_MAX_IDS };

option_description target_options[TO_MAX_IDS];
option_description prefix_options[PO_MAX_IDS];

static void reset_opts(option_description * opts, int size){
	int i;
	if ( NULL == opts){
		LM_ERR("Trying to init a NULL pointer location \n");
		return;
	}
	for (i=0; i < size; i++){
		memset(&(opts[i].value),'\0', sizeof(union opt_data));
		opts[i].visited = 0;
		opts[i].no_elems = 0;
		if ( CFG_STR == opts[i].type ){
			opts[i].value.string_data.s = opts[i].str_buf;
			strcpy(opts[i].str_buf,"");
			opts[i].value.string_data.len = 0;
		}
	}

	opts[TO_ID_STRIP     ].value.int_data=0;
	opts[TO_ID_PROB      ].value.float_data=0;
	opts[TO_ID_HASH_INDEX].value.int_data=0;
	opts[TO_ID_STATUS    ].value.int_data=0;
	opts[TO_ID_BACKUP    ].value.int_data=-1;

	return;
}

static int init_target_opts(option_description * opts){
	if ( NULL == opts){
		LM_DBG("Trying to init a NULL pointer location \n");
		return -1;
	}
	memset(opts, '\0', sizeof(option_description) * TO_MAX_IDS);

	strcpy((char*)(opts[TO_ID_COMMENT].name),    "comment");
	strcpy((char*)(opts[TO_ID_STRIP].name),      "strip");
	strcpy((char*)(opts[TO_ID_REWR_PREFIX].name),"rewrite_prefix");
	strcpy((char*)(opts[TO_ID_PROB].name),       "prob");
	strcpy((char*)(opts[TO_ID_HASH_INDEX].name), "hash_index");
	strcpy((char*)(opts[TO_ID_REWR_SUFFIX].name),"rewrite_suffix");
	strcpy((char*)(opts[TO_ID_STATUS].name),     "status");
	strcpy((char*)(opts[TO_ID_BACKED_UP].name),  "backed_up");
	strcpy((char*)(opts[TO_ID_BACKUP].name),     "backup");

	opts[TO_ID_COMMENT    ].type=CFG_STR;
	opts[TO_ID_STRIP      ].type=CFG_INT;
	opts[TO_ID_REWR_PREFIX].type=CFG_STR;
	opts[TO_ID_PROB       ].type=CFG_FLOAT;
	opts[TO_ID_HASH_INDEX ].type=CFG_INT;
	opts[TO_ID_REWR_SUFFIX].type=CFG_STR;
	opts[TO_ID_STATUS     ].type=CFG_INT;
	opts[TO_ID_BACKED_UP  ].type=CFG_INT_LIST;
	opts[TO_ID_BACKUP     ].type=CFG_INT;

	reset_opts(opts, TO_MAX_IDS);
	return 0;
}

static int init_prefix_opts(option_description * opts){
	if ( NULL == opts){
		LM_DBG("Trying to init a NULL pointer location \n");
		return -1;
	}
	memset(opts, '\0', sizeof(option_description) * PO_MAX_IDS);
	strcpy((char*)(opts[PO_MAX_TARGETS].name), "max_targets");
	opts[PO_MAX_TARGETS].type=CFG_INT;
	opts[PO_MAX_TARGETS].value.int_data=-1;
	return 0;
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
 * The function mixes code parsing calls with rd structure
 * completion.
 *
 * @param rd Pointer to the route data tree where the routing data
 * shall be loaded into
 *
 * @return 0 means ok, -1 means an error occured
 *
 */
int load_config(struct route_data_t * rd) {
	FILE * file;

	int ret_domain, ret_prefix, ret_target, ret_prefix_opts, ret_target_opts;
	int domain_id, allocated_domain_num = DEFAULT_DOMAIN_NUM;
	str domain_name, prefix_name, rewrite_host;
	char domain_buf[CR_MAX_LINE_SIZE], prefix_buf[CR_MAX_LINE_SIZE],  rewrite_buf[CR_MAX_LINE_SIZE];

	str rewrite_prefix, rewrite_suffix, comment;
	struct domain_data_t *domain_data = NULL;
	struct carrier_data_t * tmp_carrier_data;
	int hash_index, max_targets = 0, strip;
	double prob;
	int * backed_up = NULL;
	int backed_up_size = 0, backup = 0, status;
	void* p_realloc;
	int i=0, l, k;

	domain_name.s = domain_buf; domain_name.len = CR_MAX_LINE_SIZE;
	prefix_name.s = prefix_buf; prefix_name.len = CR_MAX_LINE_SIZE;
	rewrite_host.s = rewrite_buf; rewrite_host.len = CR_MAX_LINE_SIZE;

	/* open configuration file */
	if ((file = fopen(config_file, "rb"))==NULL) {
		LM_ERR("Cannot open source file.\n");
		return -1;
	}

	rd->carrier_num = 1;
	rd->first_empty_carrier = 0;
	rd->domain_num = 0;

	if ((rd->carriers = shm_malloc(sizeof(struct carrier_data_t *))) == NULL) {
		SHM_MEM_ERROR;
		goto errclose;
	}
	memset(rd->carriers, 0, sizeof(struct carrier_data_t *));

	/* Create carrier map */
	if ((rd->carrier_map = shm_malloc(sizeof(struct name_map_t))) == NULL) {
		SHM_MEM_ERROR;
		goto errclose;
	}

	memset(rd->carrier_map, 0, sizeof(struct name_map_t));
	rd->carrier_map[0].id = 1;
	rd->carrier_map[0].name.len = default_tree.len;
	rd->carrier_map[0].name.s = shm_malloc(rd->carrier_map[0].name.len);

	if (rd->carrier_map[0].name.s == NULL) {
		SHM_MEM_ERROR;
		goto errclose;
	}
	memcpy(rd->carrier_map[0].name.s, default_tree.s, rd->carrier_map[0].name.len);

	/* Create domain map */
	if ((rd->domain_map = shm_malloc(sizeof(struct name_map_t) * allocated_domain_num)) == NULL) {
		SHM_MEM_ERROR;
		goto errclose;
	}
	memset(rd->domain_map, 0, sizeof(struct name_map_t) * allocated_domain_num);

	/* Create and insert carrier data structure */
	tmp_carrier_data = create_carrier_data(1, &rd->carrier_map[0].name, allocated_domain_num);
	tmp_carrier_data->domain_num = 0;
	tmp_carrier_data->id = 1;
	tmp_carrier_data->name = &(rd->carrier_map[0].name);

	if (tmp_carrier_data == NULL) {
		LM_ERR("can't create new carrier\n");
		goto errclose;
	}
	if (add_carrier_data(rd, tmp_carrier_data) < 0) {
		LM_ERR("couldn't add carrier data\n");
		destroy_carrier_data(tmp_carrier_data);
		goto errclose;
	}

	init_prefix_opts(prefix_options);
	init_target_opts(target_options);

	/* add all routes by parsing the route conf file */
	/* while there are domain structures, get name and parse the structure*/
	while ((ret_domain = parse_struct_header(file, "domain", &domain_name))
			== SUCCESSFUL_PARSING) {

		domain_id = ++rd->domain_num;
		tmp_carrier_data->domain_num++;

		/* (re)allocate memory for a maximum of MAX_DOMAIN_NUM domains
		 rd is not fully allocated from the start as this would require the preparsing
		 of the entire route file */
		if ( rd->domain_num > allocated_domain_num){

			if (MAX_DOMAIN_NUM <= allocated_domain_num){
				LM_ERR("Maximum number of domains reached");
				break;
			}

			LM_INFO("crt_alloc_size=%d must be increased \n", allocated_domain_num);
			allocated_domain_num *= 2;

			if ( ( p_realloc = shm_realloc(rd->domain_map,
					sizeof(struct name_map_t) * allocated_domain_num) ) == NULL)
			{
				SHM_MEM_ERROR;
				goto errclose;
			}

			rd->domain_map = (struct name_map_t *)p_realloc;

			if (( p_realloc = shm_realloc( rd->carriers[0]->domains,
					sizeof(struct domain_data_t *) * allocated_domain_num)) == NULL) {
				SHM_MEM_ERROR;
				goto errclose;
			}
			rd->carriers[0]->domains = (struct domain_data_t **)p_realloc;

			for (i=0; i<rd->domain_num-1; i++){
				rd->carriers[0]->domains[i]->name = &(rd->domain_map[i].name);
			}
		}// end of mem (re)allocation for domains

		/*insert domain in domain map*/
		rd->domain_map[domain_id-1].id = domain_id;
		rd->domain_map[domain_id-1].name.len = domain_name.len;
		rd->domain_map[domain_id-1].name.s = shm_malloc(rd->domain_map[domain_id-1].name.len);
		if (rd->domain_map[domain_id-1].name.s == NULL) {
			SHM_MEM_ERROR;
			goto errclose;
		}
		memcpy(rd->domain_map[domain_id-1].name.s, domain_name.s, rd->domain_map[domain_id-1].name.len);

		/* create new domain data */
		if ((domain_data = create_domain_data(domain_id,&(rd->domain_map[domain_id-1].name))) == NULL) {
			LM_ERR("could not create new domain data\n");
			goto errclose;
		}

		if (add_domain_data(tmp_carrier_data, domain_data, domain_id-1) < 0) {
			LM_ERR("could not add domain data\n");
			destroy_domain_data(domain_data);
			goto errclose;
		}
		LM_DBG("added domain %d '%.*s' to carrier %d '%.*s'\n",
				domain_id, domain_name.len, domain_name.s,
				tmp_carrier_data->id, tmp_carrier_data->name->len, tmp_carrier_data->name->s);

		/* while there are prefix structures, get name and parse the structure */
		while ((ret_prefix = parse_struct_header(file, "prefix", &prefix_name))
				== SUCCESSFUL_PARSING) {

			reset_opts(prefix_options, PO_MAX_IDS);
			if (str_strcasecmp(&prefix_name, &CR_EMPTY_PREFIX) == 0) {
				prefix_name.s[0] = '\0';
				prefix_name.len = 0;
			}

			/* look for max_targets = value which is described in prefix_options */
			if ((ret_prefix_opts = parse_options(file, prefix_options,
					PO_MAX_IDS, "target")) != SUCCESSFUL_PARSING) {
				LM_ERR("Error in parsing \n");
				goto errclose;
			}

			max_targets = prefix_options[PO_MAX_TARGETS].value.int_data;
			/* look for max_targets target structures */
			for ( k = 0; k < max_targets; k++) {
				/* parse the target header, get name and continue*/
				ret_target = parse_struct_header(file, "target", &rewrite_host);
				if (ret_target != SUCCESSFUL_PARSING) {
					LM_ERR("Error in parsing \n");
					goto errclose;
				}

				reset_opts(target_options, TO_MAX_IDS);
				/* look for the target options: prob, hash index, status, etc*/
				ret_target_opts = parse_options(file, target_options, TO_MAX_IDS, "}");
				if ( SUCCESSFUL_PARSING == ret_target_opts ){
					/* parsing target structure closing bracket*/
					parse_struct_stop(file);
				}else{
					LM_ERR("Error in parsing in target options \n");
					goto errclose;
				}
				/* intermediary variables for more lisibility */
				if (str_strcasecmp(&rewrite_host, &CR_EMPTY_PREFIX) == 0) {
					rewrite_host.s[0] = '\0';
					rewrite_host.len = 0;
				}
				LM_DBG("loading target %.*s\n", rewrite_host.len, rewrite_host.s);
				prob = target_options[TO_ID_PROB].value.float_data;
				strip = target_options[TO_ID_STRIP].value.int_data;
				rewrite_prefix.s = target_options[TO_ID_REWR_PREFIX].value.string_data.s;
				rewrite_prefix.len = target_options[TO_ID_REWR_PREFIX].value.string_data.len;
				rewrite_suffix.s = target_options[TO_ID_REWR_SUFFIX].value.string_data.s;
				rewrite_suffix.len = target_options[TO_ID_REWR_SUFFIX].value.string_data.len;
				hash_index = target_options[TO_ID_HASH_INDEX].value.int_data;
				comment.s = target_options[TO_ID_COMMENT].value.string_data.s;
				comment.len = target_options[TO_ID_COMMENT].value.string_data.len;
				status = target_options[TO_ID_STATUS].value.int_data;

				if ( (backed_up_size = target_options[TO_ID_BACKED_UP].no_elems) > 0){
					if ((backed_up = pkg_malloc(sizeof(int) * (backed_up_size + 1))) == NULL) {
						PKG_MEM_ERROR;
						goto errclose;
					}
					for (l = 0; l < backed_up_size; l++) {
						backed_up[l] = target_options[TO_ID_BACKED_UP].value.int_list[l];
					}
					backed_up[backed_up_size] = -1;
				}
				backup = target_options[TO_ID_BACKUP].value.int_data;

				LM_DBG("\n Adding route to tree <'%.*s'>: prefix_name:%s\n,"
						" max_targets =%d\n, prob=%f\n, rewr_host=%s\n,"
						" strip=%i\n, rwr_prefix=%s\n, rwr_suff=%s\n,"
						" status=%i\n, hash_index=%i\n, comment=%s \n",
						domain_data->name->len, domain_data->name->s, prefix_name.s,
						max_targets, prob, rewrite_host.s, strip, rewrite_prefix.s,
						rewrite_suffix.s, status, hash_index, comment.s);

				if (add_route_to_tree(domain_data->tree, &prefix_name, 0, 0,
						&prefix_name, max_targets, prob, &rewrite_host,
						strip, &rewrite_prefix, &rewrite_suffix, status,
						hash_index, backup, backed_up, &comment) < 0) {
					LM_INFO("Error while adding route\n");
					if (backed_up) {
						pkg_free(backed_up);
					}
					goto errclose;
				}

				if (backed_up) {
					pkg_free(backed_up);
				}
				backed_up = NULL;
			}

			if (k != prefix_options[0].value.int_data ) {
				LM_ERR("Error in parsing: max_targets =%i, actual targets =%i \n",
						prefix_options[0].value.int_data, i);
				goto errclose;
			}
			/* parsing prefix structure closing bracket */
			if (parse_struct_stop(file) != SUCCESSFUL_PARSING) {
				LM_ERR("Error in parsing targets, expecting } \n");
				goto errclose;

			}
		} // END OF PREFIX part

		/* parsing domain structure closing bracket */
		if (parse_struct_stop(file) != SUCCESSFUL_PARSING) {
			LM_ERR("Error in parsing targets, expecting } \n");
			goto errclose;
		}
	}

	if (EOF_REACHED != ret_domain){
		LM_ERR("Error appeared while parsing domain header \n");
		goto errclose;
	}

	LM_INFO("File parsed successfully \n");
	fclose(file);
	return 0;
errclose:
	fclose(file);
	return -1;
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
