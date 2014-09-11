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

/**
 * \file cr_fifo.c
 * \brief Functions for modifying routing data via fifo commands.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#include <ctype.h>
#include <stdlib.h>

#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../str.h"
#include "../../ut.h"

#include "cr_fifo.h"
#include "carrierroute.h"
#include "cr_config.h"
#include "cr_carrier.h"
#include "cr_domain.h"
#include "cr_rule.h"


/**
 * Defines the option set for the different fifo commands
 * Every line is for a command,
 * The first field defines the required options, the second field defines the
 * optional options and the third field defines the invalid options.
 */
static unsigned int opt_settings[5][3] = {{O_PREFIX|O_DOMAIN|O_HOST|O_PROB, O_R_PREFIX|O_R_SUFFIX|O_H_INDEX, O_NEW_TARGET},
        {O_HOST|O_DOMAIN|O_PREFIX, O_PROB, O_R_PREFIX|O_R_SUFFIX|O_NEW_TARGET|O_H_INDEX},
        {O_HOST|O_NEW_TARGET, O_PREFIX|O_DOMAIN|O_PROB, O_R_PREFIX|O_R_SUFFIX|O_H_INDEX},
        {O_HOST|O_DOMAIN|O_PREFIX, O_PROB|O_NEW_TARGET, O_R_PREFIX|O_R_SUFFIX|O_H_INDEX},
        {O_HOST|O_DOMAIN|O_PREFIX, O_PROB, O_R_PREFIX|O_R_SUFFIX|O_NEW_TARGET|O_H_INDEX}};

int fifo_err;

static int updated;

static int dump_tree_recursor (struct mi_node* msg, struct dtrie_node_t *node, char *prefix);

static struct mi_root* print_replace_help(void);

static int get_fifo_opts(str * buf, fifo_opt_t * opts, unsigned int opt_set[]);

static int update_route_data(fifo_opt_t * opts);

static int update_route_data_recursor(struct dtrie_node_t *node, str * act_domain, fifo_opt_t * opts);

static struct mi_root* print_fifo_err(void);


static int str_toklen(str * str, const char * delims)
{
	int len;
	
	if ((str==NULL) || (str->s==NULL)) {
		/* No more tokens */
		return -1;
	}
	
	len=0;
	while (len<str->len) {
		if (strchr(delims,str->s[len])!=NULL) {
			return len;
		}
		len++;
	}
	
	return len;
}


/**
 * reloads the routing data
 *
 * @param cmd_tree the MI command tree
 * @param param the parameter
 *
 * @return code 200 on success, code 500 on failure
 */
struct mi_root* reload_fifo (struct mi_root* cmd_tree, void *param) {
	struct mi_root * tmp = NULL;

	if (reload_route_data () == -1) {
		tmp = init_mi_tree(500, "failed to re-built tree, see log", 33);
	}
	else {
		tmp = init_mi_tree(200, MI_OK_S, MI_OK_LEN);
	}
	return tmp;
}


/**
 * prints the routing data
 *
 * @param cmd_tree the MI command tree
 * @param param the parameter
 *
 * @return code 200 on success, code 400 or 500 on failure
 */
struct mi_root* dump_fifo (struct mi_root* cmd_tree, void *param) {
	struct route_data_t * rd;
	str *tmp_str;
	str empty_str = str_init("<empty>");

	if((rd = get_data ()) == NULL) {
		LM_ERR("error during retrieve data\n");
		return init_mi_tree(500, "error during command processing", 31);
	}
	
	struct mi_root* rpl_tree;
	struct mi_node* node = NULL;
	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	if(rpl_tree == NULL)
		goto error2;
	node = addf_mi_node_child( &rpl_tree->node, 0, 0, 0, "Printing routing information:");
	if(node == NULL)
		goto error;

	LM_DBG("start processing of data\n");
	int i, j;
 	for (i = 0; i < rd->carrier_num; i++) {
 		if (rd->carriers[i]) {
			tmp_str = (rd->carriers[i] ? rd->carriers[i]->name : &empty_str);
			node = addf_mi_node_child( &rpl_tree->node, 0, 0, 0, "Printing tree for carrier '%.*s' (%i)\n", tmp_str->len, tmp_str->s, rd->carriers[i] ? rd->carriers[i]->id : 0);
			if(node == NULL)
				goto error;
 			for (j=0; j<rd->carriers[i]->domain_num; j++) {
 				if (rd->carriers[i]->domains[j] && rd->carriers[i]->domains[j]->tree) {
					tmp_str = (rd->carriers[i]->domains[j] ? rd->carriers[i]->domains[j]->name : &empty_str);
					node = addf_mi_node_child( &rpl_tree->node, 0, 0, 0, "Printing tree for domain '%.*s' (%i)\n", tmp_str->len, tmp_str->s, rd->carriers[i]->domains[j]->id);
					if(node == NULL)
						goto error;
					if (dump_tree_recursor (&rpl_tree->node, rd->carriers[i]->domains[j]->tree, "") < 0) 
						goto error;
 				}
 			}
		}
	}
	release_data (rd);
	return rpl_tree;

error:
	free_mi_tree(rpl_tree);
error2:
	release_data (rd);
	return 0;
}


/**
 * replaces the host specified by parameters in the
 * fifo command, can be used only in file mode
 * expect one mi node that contains the command
 *
 * @param cmd_tree the MI command tree
 * @param param the parameter
 *
 * @return code 200 on success, code 400 or 500 on failure
 */
struct mi_root* replace_host (struct mi_root* cmd_tree, void *param) {
	struct mi_node *node = NULL;

	int ret;
	fifo_opt_t options;

	if(mode != CARRIERROUTE_MODE_FILE) {
		return init_mi_tree(400, "Not running in config file mode, cannot modify route from command line", 70);
	}
	
	node = cmd_tree->node.kids;
	if (node==NULL || node->next!=NULL)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	
	/* look for command */
	if (node->value.s==NULL)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	if((ret = get_fifo_opts(&node->value, &options, opt_settings[OPT_REPLACE])) <  0) {
		return print_fifo_err();
	}

	options.status = 1;
	options.cmd = OPT_REPLACE;

	if(update_route_data(&options) < 0) {
		return init_mi_tree(500, "failed to update route data, see log", 37);
	}

	return init_mi_tree(200, MI_OK_S, MI_OK_LEN);
}


/**
 * deactivates the host given in the command line options,
 * can be used only in file mode
 * expect one mi node that contains the command
 *
 * @param cmd_tree the MI command tree
 * @param param the parameter
 *
 * @return code 200 on success, code 400 or 500 on failure
 */
struct mi_root* deactivate_host (struct mi_root* cmd_tree, void *param) {
	struct mi_node *node = NULL;

	int ret;
	fifo_opt_t options;

	if(mode != CARRIERROUTE_MODE_FILE) {
		return init_mi_tree(400, "Not running in config file mode, cannot modify route from command line", 70);
	}

	node = cmd_tree->node.kids;
	if (node==NULL || node->next!=NULL)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	
	/* look for command */
	if (node->value.s==NULL)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	if((ret = get_fifo_opts(&node->value, &options, opt_settings[OPT_DEACTIVATE])) <  0) {
		return print_fifo_err();
	}

	options.status = 0;
	options.cmd = OPT_DEACTIVATE;

	if(update_route_data(&options) < 0) {
		return init_mi_tree(500, "failed to update route data, see log", 37);
	}

	return init_mi_tree(200, MI_OK_S, MI_OK_LEN);
}


/**
 * activates the host given in the command line options,
 * can be used only in file mode
 * expect one mi node that contains the command
 *
 * @param cmd_tree the MI command tree
 * @param param the parameter
 *
 * @return code 200 on success, code 400 or 500 on failure
 */
struct mi_root* activate_host (struct mi_root* cmd_tree, void *param) {
	struct mi_node *node = NULL;

	int ret;
	fifo_opt_t options;

	if(mode != CARRIERROUTE_MODE_FILE) {
		return init_mi_tree(400, "Not running in config file mode, cannot modify route from command line", 70);
	}

	node = cmd_tree->node.kids;
	if (node==NULL || node->next!=NULL)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	
	/* look for command */
	if (node->value.s==NULL)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	if((ret = get_fifo_opts(&node->value, &options, opt_settings[OPT_ACTIVATE])) <  0) {
		return print_fifo_err();
	}

	options.status = 1;
	options.cmd = OPT_ACTIVATE;

	if(update_route_data(&options) < 0) {
		return init_mi_tree(500, "failed to update route data, see log", 37);
	}

	return init_mi_tree(200, MI_OK_S, MI_OK_LEN);
}


/**
 * adds the host specified by the command line args,
 * can be used only in file mode
 * expect one mi node that contains the command
 *
 * @param cmd_tree the MI command tree
 * @param param the parameter
 *
 * @return code 200 on success, code 400 or 500 on failure
 */
struct mi_root* add_host (struct mi_root* cmd_tree, void *param) {
	struct mi_node *node = NULL;

	int ret;
	fifo_opt_t options;

	if(mode != CARRIERROUTE_MODE_FILE) {
		return init_mi_tree(400, "Not running in config file mode, cannot modify route from command line", 70);
	}

	node = cmd_tree->node.kids;
	if (node==NULL || node->next!=NULL || node->value.s==NULL) {
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);
	}

	if((ret = get_fifo_opts(&node->value, &options, opt_settings[OPT_ADD])) <  0) {
		return print_fifo_err();
	}

	options.status = 1;
	options.cmd = OPT_ADD;

	if(update_route_data(&options) < 0) {
		return init_mi_tree(500, "failed to update route data, see log", 37);
	}

	return init_mi_tree(200, MI_OK_S, MI_OK_LEN);
}


/**
 * deletes the host specified by the command line args,
 * can be used only in file mode
 * expect one mi node that contains the command
 *
 * @param cmd_tree the MI command tree
 * @param param the parameter
 *
 * @return code 200 on success, code 400 or 500 on failure
 */
struct mi_root* delete_host (struct mi_root* cmd_tree, void * param) {
	struct mi_node *node = NULL;

	int ret;
	fifo_opt_t options;

	if(mode != CARRIERROUTE_MODE_FILE) {
		return init_mi_tree(400, "Not running in config file mode, cannot modify route from command line", 70);
	}

	node = cmd_tree->node.kids;
	if (node==NULL || node->next!=NULL)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	
	/* look for command */
	if (node->value.s==NULL)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	if((ret = get_fifo_opts(&node->value, &options, opt_settings[OPT_REMOVE])) <  0) {
		return print_fifo_err();
	}

	options.cmd = OPT_REMOVE;

	if(update_route_data(&options) < 0) {
		return init_mi_tree(500, "failed to update route data, see log", 37);
	}

	return init_mi_tree(200, MI_OK_S, MI_OK_LEN);
}


/**
 * does the work for dump_fifo, traverses the routing tree
 * and prints route rules if present.
 *
 * @param msg MI node that is used to append the informations
 * @param node pointer to the routing tree node
 * @param prefix carries the current scan prefix
 *
 * @return 0 for success, negative result for error 
 */
static int dump_tree_recursor (struct mi_node* msg, struct dtrie_node_t *node, char *prefix) {
	char s[256];
	char *p;
	int i;
	struct route_flags *rf;
	struct route_rule *rr;
	struct route_rule_p_list * rl;
	double prob;
	struct mi_node* tmp_node = NULL;

	strcpy (s, prefix);
	p = s + strlen (s);
	p[1] = '\0';
	for (i = 0; i < cr_match_mode; ++i) {
		if (node->child[i] != NULL) {
			*p = i + '0';
			/* if there is a problem in processing the child nodes .. return an error */
			if(dump_tree_recursor (msg->next, node->child[i], s) < 0)
				return -1;
		}
	}
	*p = '\0';
	for (rf = (struct route_flags *)(node->data); rf != NULL; rf = rf->next) {
		for (rr = rf->rule_list; rr != NULL; rr = rr->next) {
			if(rf->dice_max){
				prob = (double)(rr->prob * DICE_MAX)/(double)rf->dice_max;
			} else {
				prob = rr->prob;
			}
			tmp_node = addf_mi_node_child(msg->next, 0, 0, 0, "%10s: %0.3f %%, '%.*s': %s, '%i', '%.*s', '%.*s', '%.*s'\n",
												 strlen(prefix) > 0 ? prefix : "NULL", prob * 100, rr->host.len, rr->host.s,
												 (rr->status ? "ON" : "OFF"), rr->strip,
												 rr->local_prefix.len, rr->local_prefix.s,
												 rr->local_suffix.len, rr->local_suffix.s,
												 rr->comment.len, rr->comment.s);
			if(!tmp_node) return -1;
			if(!rr->status && rr->backup && rr->backup->rr){
				tmp_node = addf_mi_node_child(msg->next, 0, 0, 0, "            Rule is backed up by: %.*s\n", rr->backup->rr->host.len, rr->backup->rr->host.s);
				if(!tmp_node) return -1;
			}
			if(rr->backed_up){
				rl = rr->backed_up;
				i=0;
				while(rl){
					if(rl->rr){
						tmp_node = addf_mi_node_child(msg->next, 0, 0, 0, "            Rule is backup for: %.*s", rl->rr->host.len, rl->rr->host.s);
						if(!tmp_node) return -1;
					}
					rl = rl->next;
					i++;
				}
			}
		}
	}
	return 0;
}


/**
 * parses the command line argument for options
 *
 * @param buf the command line argument
 * @param opts fifo options
 * @param opt_set set of the options
 *
 * @return 0 on success, -1 on failure
 *
 * @see dump_fifo()
 */
static int get_fifo_opts(str * buf, fifo_opt_t * opts, unsigned int opt_set[]) {
	int opt_argc = 0;
	str opt_argv[20];
	int i, op = -1;
	unsigned int used_opts = 0;
	int toklen;

	memset(opt_argv, 0, sizeof(opt_argv));
	memset(opts, 0, sizeof(fifo_opt_t));
	opts->prob = -1;

	while((toklen = str_toklen(buf, " \t\r\n")) >=0 && opt_argc < 20) {
		buf->s[toklen] = '\0'; /* insert zero termination, since strtod might be used later on it */
		opt_argv[opt_argc].len = toklen;
		opt_argv[opt_argc].s = buf->s;
		buf->s += toklen + 1;
		buf->len -= toklen + 1;
		LM_DBG("found arg[%i]: %.*s\n", opt_argc, opt_argv[opt_argc].len, opt_argv[opt_argc].s);
		opt_argc++;
	}
	for (i=0; i<opt_argc; i++) {
		LM_DBG("token %.*s", opt_argv[i].len, opt_argv[i].s);
		if (opt_argv[i].len >= 1) {
			switch(*opt_argv[i].s) {
					case '-': switch(opt_argv[i].s[1]) {
							case OPT_DOMAIN_CHR:
							op = OPT_DOMAIN;
							used_opts |= O_DOMAIN;
							break;
							case OPT_PREFIX_CHR:
							op = OPT_PREFIX;
							used_opts |= O_PREFIX;
							break;
							case OPT_HOST_CHR:
							op = OPT_HOST;
							used_opts |= O_HOST;
							break;
							case OPT_NEW_TARGET_CHR:
							op = OPT_NEW_TARGET;
							used_opts |= O_NEW_TARGET;
							break;
							case OPT_PROB_CHR:
							op = OPT_PROB;
							used_opts |= O_PROB;
							break;
							case OPT_R_PREFIX_CHR:
							op = OPT_R_PREFIX;
							used_opts |= O_R_PREFIX;
							break;
							case OPT_R_SUFFIX_CHR:
							op = OPT_R_SUFFIX;
							used_opts |= O_R_SUFFIX;
							break;
							case OPT_HASH_INDEX_CHR:
							op = OPT_HASH_INDEX;
							used_opts |= O_H_INDEX;
							break;
							case OPT_HELP_CHR:
							FIFO_ERR(E_HELP);
							return -1;
							default: {
								FIFO_ERR(E_WRONGOPT);
								LM_DBG("Unknown option: %.*s\n", opt_argv[i].len, opt_argv[i].s);
								return -1;
							}
					}
					break;
					default: switch(op) {
							case OPT_DOMAIN:
							opts->domain = opt_argv[i];
							op = -1;
							break;
							case OPT_PREFIX:
							if (str_strcasecmp(&opt_argv[i], &CR_EMPTY_PREFIX) == 0) {
								opts->prefix.s = NULL;
								opts->prefix.len = 0;
							} else {
								opts->prefix = opt_argv[i];
							}
							op = -1;
							break;
							case OPT_HOST:
							opts->host = opt_argv[i];
							op = -1;
							break;
							case OPT_NEW_TARGET:
							opts->new_host = opt_argv[i];
							op = -1;
							break;
							case OPT_PROB:
							opts->prob = strtod(opt_argv[i].s, NULL); /* we can use str.s since we zero terminated it earlier */
							op = -1;
							break;
							case OPT_R_PREFIX:
							opts->rewrite_prefix = opt_argv[i];
							op = -1;
							break;
							case OPT_STRIP:
							str2sint(&opt_argv[i], &opts->strip);
							op = -1;
							break;
							case OPT_R_SUFFIX:
							opts->rewrite_suffix = opt_argv[i];
							op = -1;
							break;
							case OPT_HASH_INDEX:
							str2sint(&opt_argv[i], &opts->hash_index);
							op = -1;
							break;
							default: {
								LM_DBG("No option given\n");
								FIFO_ERR(E_NOOPT);
								return -1;
							}
					}
					break;
			}
		}
	}
	if((used_opts & opt_set[OPT_INVALID]) != 0) {
		LM_DBG("invalid option\n");
		FIFO_ERR(E_INVALIDOPT);
		return -1;
	}
	if((used_opts & opt_set[OPT_MANDATORY]) != opt_set[OPT_MANDATORY]) {
		LM_DBG("option missing\n");
		FIFO_ERR(E_MISSOPT);
		return -1;
	}
	return 0;
}


/**
 * loads the config data into shared memory (but doesn't really
 * share it), updates the routing data and writes it to the config
 * file. Afterwards, the global routing data is reloaded.
 *
 * @param opts pointer to the option structure which contains
 * data to be modified or to be added
 *
 * @return 0 on success, -1 on failure
 */
static int update_route_data(fifo_opt_t * opts) {
	struct route_data_t * rd;
	int i,j;
	int domain_id;
	str tmp_domain;
	str tmp_prefix;
	str tmp_host;
	str tmp_rewrite_prefix;
	str tmp_rewrite_suffix;
	str tmp_comment = str_init("");

	if ((rd = shm_malloc(sizeof(struct route_data_t))) == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}
	memset(rd, 0, sizeof(struct route_data_t));
	if (load_config(rd) < 0) {
		LM_ERR("could not load config");
		FIFO_ERR(E_LOADCONF);
		return -1;
	}

	if (rule_fixup(rd) < 0) {
		LM_ERR("could not fixup rules");
		FIFO_ERR(E_RULEFIXUP);
		return -1;
	}
	updated = 0;

	if (opts->cmd == OPT_ADD) {
		tmp_domain=opts->domain;
		tmp_prefix=opts->prefix;
		tmp_host=opts->host;
		tmp_rewrite_prefix=opts->rewrite_prefix;
		tmp_rewrite_suffix=opts->rewrite_suffix;
		if (tmp_domain.s==NULL) {
			tmp_domain.s="";
			tmp_domain.len=0;
		}
		if (tmp_prefix.s==NULL) {
			tmp_prefix.s="";
			tmp_prefix.len=0;
		}
		if (tmp_host.s==NULL) {
			tmp_host.s="";
			tmp_host.len=0;
		}
		if (tmp_rewrite_prefix.s==NULL) {
			tmp_rewrite_prefix.s="";
			tmp_rewrite_prefix.len=0;
		}
		if (tmp_rewrite_suffix.s==NULL) {
			tmp_rewrite_suffix.s="";
			tmp_rewrite_suffix.len=0;
		}

		domain_id = map_name2id(rd->domain_map, rd->domain_num, &tmp_domain);
		if (domain_id < 0) {
			LM_ERR("cannot find id for domain '%.*s'", tmp_domain.len, tmp_domain.s);
			goto errout;
		}

		if (add_route(rd, 1, domain_id, &tmp_prefix, 0, 0, 0, opts->prob,
		              &tmp_host, opts->strip, &tmp_rewrite_prefix, &tmp_rewrite_suffix,
		              opts->status, opts->hash_index, -1, NULL, &tmp_comment) < 0) {
			goto errout;
		}
		updated = 1;
		if (rule_fixup(rd) < 0) {
			LM_ERR("could not fixup rules after route appending");
			FIFO_ERR(E_RULEFIXUP);
			goto errout;
		}
	} else {
		for (i=0; i<rd->carrier_num; i++) {
			if(rd->carriers[i]){
				for (j=0; j<rd->carriers[i]->domain_num; j++) {
					if (rd->carriers[i]->domains[j] && rd->carriers[i]->domains[j]->tree) {
						if (update_route_data_recursor(rd->carriers[i]->domains[j]->tree, rd->carriers[i]->domains[j]->name, opts) < 0) {
							goto errout;
						}
					}
				}
			}
		}
	}

	if(!updated){
		LM_ERR("no match for update found");
		FIFO_ERR(E_NOUPDATE);
		goto errout;
	}

	if (save_config(rd) < 0) {
		LM_ERR("could not save config");
		FIFO_ERR(E_SAVECONF);
		goto errout;
	}

	if (reload_route_data() == -1) {
		LM_ERR("could not reload route data");
		FIFO_ERR(E_LOADCONF);
		goto errout;
	}

	clear_route_data(rd);
	return 0;
errout:
	clear_route_data(rd);
	return -1;
}


/**
 * Does the work for update_route_data by recursively
 * traversing the routing tree
 *
 * @param node points to the current routing tree node
 * @param act_domain routing domain which is currently
 * searched
 * @param opts points to the fifo command option structure
 *
 * @see update_route_data()
 *
 * @return 0 on success, -1 on failure
 */
static int update_route_data_recursor(struct dtrie_node_t *node, str * act_domain, fifo_opt_t * opts) {
	int i, hash = 0;
	struct route_rule * rr, * prev = NULL, * tmp, * backup;
	struct route_flags *rf;

	rf = (struct route_flags *)(node->data);
	if (rf && rf->rule_list) {
		rr = rf->rule_list;
		while (rr) {
			if ((!opts->domain.len || (strncmp(opts->domain.s, OPT_STAR, strlen(OPT_STAR)) == 0)
			        || ((opts->domain.len == act_domain->len) && (strncmp(opts->domain.s, act_domain->s, opts->domain.len) == 0)))
			        && ((!opts->prefix.len && !rr->prefix.len) || (strncmp(opts->prefix.s, OPT_STAR, strlen(OPT_STAR)) == 0)
			            || (rr->prefix.len == opts->prefix.len && (strncmp(opts->prefix.s, rr->prefix.s, opts->prefix.len) == 0)))
			        && ((!opts->host.len && !rr->host.s) || (strncmp(opts->host.s, OPT_STAR, strlen(OPT_STAR)) == 0)
			            || ((strncmp(rr->host.s, opts->host.s, opts->host.len) == 0) && (rr->host.len == opts->host.len)))
			        && ((opts->prob < 0) || (opts->prob == rr->prob))) {
				switch (opts->cmd) {
					case OPT_REPLACE:
						LM_INFO("replace host %.*s with %.*s\n", rr->host.len, rr->host.s, opts->new_host.len, opts->new_host.s);
						if (rr->host.s) {
							shm_free(rr->host.s);
						}
						if (opts->new_host.len) {
							if ((rr->host.s = shm_malloc(opts->new_host.len + 1)) == NULL) {
								SHM_MEM_ERROR;
								FIFO_ERR(E_NOMEM);
								return -1;
							}
							memmove(rr->host.s, opts->new_host.s, opts->new_host.len + 1);
							rr->host.len = opts->new_host.len;
							rr->host.s[rr->host.len] = '\0';
						} else {
							rr->host.len = 0;
						}
						rr->status = opts->status;
						prev = rr;
						rr = rr->next;
						updated = 1;
						break;
					case OPT_DEACTIVATE:
						if (remove_backed_up(rr) < 0) {
							LM_ERR("could not reset backup hosts\n");
							FIFO_ERR(E_RESET);
							return -1;
						}
						if (opts->new_host.len > 0) {
							LM_INFO("deactivating host %.*s\n", rr->host.len, rr->host.s);
							if ( opts->new_host.s && (strcmp(opts->new_host.s, rr->host.s) == 0)){
								LM_ERR("Backup host the same as initial host %.*s",rr->host.len, rr->host.s);
								FIFO_ERR(E_WRONGOPT);
								return -1;
							}
							if (opts->new_host.len == 1 && opts->new_host.s[0] == 'a') {
								if ((backup = find_auto_backup(rf, rr)) == NULL) {
									LM_ERR("didn't find auto backup route\n");
									FIFO_ERR(E_NOAUTOBACKUP);
									return -1;
								}
							} else {
								errno = 0;
								hash = strtol(opts->new_host.s, NULL, 10);
								if (errno == EINVAL || errno == ERANGE) {
									if ((backup = find_rule_by_hash(rf, hash)) == NULL) {
										LM_ERR("didn't find given backup route (hash %i)\n", hash);
										FIFO_ERR(E_NOHASHBACKUP);
										return -1;
									}
								} else {
									if ((backup = find_rule_by_host(rf, &opts->new_host)) == NULL) {
										LM_ERR("didn't find given backup route (host %.*s)\n", opts->new_host.len, opts->new_host.s);
										FIFO_ERR(E_NOHOSTBACKUP);
										return -1;
									}
								}
							}
							if (add_backup_rule(rr, backup) < 0) {
								LM_ERR("couldn't set backup route\n");
								FIFO_ERR(E_ADDBACKUP);
								return -1;
							}
						} else {
							if(rr->backed_up){
								LM_ERR("can't deactivate route without backup route because it is backup route for others\n");
								FIFO_ERR(E_DELBACKUP);
								return -1;
							}
						}
						rr->status = opts->status;
						prev = rr;
						rr = rr->next;
						updated = 1;
						break;
					case OPT_ACTIVATE:
						LM_INFO("activating host %.*s\n", rr->host.len, rr->host.s);
						if (remove_backed_up(rr) < 0) {
							LM_ERR("could not reset backup hosts\n");
							FIFO_ERR(E_RESET);
							return -1;
						}
						rr->status = opts->status;
						prev = rr;
						rr = rr->next;
						updated = 1;
						break;
					case OPT_REMOVE:
						LM_INFO("removing host %.*s\n", rr->host.len, rr->host.s);
						if (rr->backed_up){
							LM_ERR("cannot remove host %.*s which is backup for other hosts\n", rr->host.len, rr->host.s);
							FIFO_ERR(E_DELBACKUP);
							return -1;
						}
						if (remove_backed_up(rr) < 0) {
							LM_ERR("could not reset backup hosts\n");
							FIFO_ERR(E_RESET);
							return -1;
						}
						if (prev) {
							prev->next = rr->next;
							tmp = rr;
							rr = prev;
							destroy_route_rule(tmp);
							prev = rr;
							rr = rr->next;
						} else {
							rf->rule_list = rr->next;
							tmp = rr;
							rr = rf->rule_list;
							destroy_route_rule(tmp);
						}
						rf->rule_num--;
						rf->max_targets--;
						updated = 1;
						break;
					default:
						rr = rr->next;
						break;
				}
			} else {
				prev = rr;
				rr = rr->next;
			}
		}
	}
	for (i=0; i<cr_match_mode; i++) {
		if (node->child[i]) {
			if (update_route_data_recursor(node->child[i], act_domain, opts) < 0) {
				return -1;
			}
		}
	}
	return 0;
}


/**
 * prints a short help text for fifo command usage
 */
static struct mi_root* print_replace_help(void) {
       struct mi_root* rpl_tree;
       struct mi_node* node;

       rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN );
       if(rpl_tree == NULL)
               return 0;

       node = addf_mi_node_child( &rpl_tree->node, 0, 0, 0, "carrierroute options usage:");
       if(node == NULL)
               goto error;
       node = addf_mi_node_child( &rpl_tree->node, 0, 0, 0, "\t-%c searched/new remote host\n", OPT_HOST_CHR);
       if(node == NULL)
               goto error;
       node = addf_mi_node_child( &rpl_tree->node, 0, 0, 0, "\t-%c replacement/backup host", OPT_NEW_TARGET_CHR);
       if(node == NULL)
               goto error;
       node = addf_mi_node_child( &rpl_tree->node, 0, 0, 0, "\t-%c: searched/new domain", OPT_DOMAIN_CHR);
       if(node == NULL)
               goto error;
       node = addf_mi_node_child( &rpl_tree->node, 0, 0, 0, "\t-%c: searched/new prefix", OPT_PREFIX_CHR);
       if(node == NULL)
               goto error;
       node = addf_mi_node_child( &rpl_tree->node, 0, 0, 0, "\t-%c: searched/new weight (0..1)", OPT_PROB_CHR);
       if(node == NULL)
               goto error;
       node = addf_mi_node_child( &rpl_tree->node, 0, 0, 0, "\t-%c: new rewrite prefix", OPT_R_PREFIX_CHR);
       if(node == NULL)
               goto error;
       node = addf_mi_node_child( &rpl_tree->node, 0, 0, 0, "\t-%c: new rewrite suffix", OPT_R_SUFFIX_CHR);
       if(node == NULL)
               goto error;
       node = addf_mi_node_child( &rpl_tree->node, 0, 0, 0, "\t-%c: new hash index", OPT_HASH_INDEX_CHR);
       if(node == NULL)
               goto error;
       node = addf_mi_node_child( &rpl_tree->node, 0, 0, 0, "\t-%c: prints this help", OPT_HELP_CHR);
       if(node == NULL)
               goto error;

       return rpl_tree;

error:
       free_mi_tree(rpl_tree);
       return 0;
}


/**
 * interpret the fifo errors, creates a mi tree
 * @todo this is currently not evaluated for errors during update_route_data
 */
struct mi_root* print_fifo_err(void) {
	struct mi_root* rpl_tree;
	
	switch (fifo_err) {
		case E_MISC: 
			rpl_tree = init_mi_tree( 400, "An error occured", 17);
			if(rpl_tree == NULL)
				return 0;
			break;
		case E_NOOPT:
			rpl_tree = init_mi_tree( 400, "No option given", 16);
			if(rpl_tree == NULL)
				return 0;
			break;
		case E_WRONGOPT:
			rpl_tree = init_mi_tree( 400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);
			if(rpl_tree == NULL)
				return 0;
			break;
		case E_NOMEM:
			rpl_tree = init_mi_tree( 500, "Out of memory", 14);
			if(rpl_tree == NULL)
				return 0;
			break;
		case E_RESET:
			rpl_tree = init_mi_tree( 500, "Could not reset backup routes", 30);
			if(rpl_tree == NULL)
				return 0;
			break;
		case E_NOAUTOBACKUP:
			rpl_tree = init_mi_tree( 400, "No auto backup route found", 27);
			if(rpl_tree == NULL)
				return 0;
			break;
		case E_NOHASHBACKUP:
			rpl_tree = init_mi_tree( 400, "No backup route for given hash found", 37);
			if(rpl_tree == NULL)
				return 0;
			break;
		case E_NOHOSTBACKUP:
			rpl_tree = init_mi_tree( 400, "No backup route for given host found", 37);
			if(rpl_tree == NULL)
				return 0;
			break;
		case E_ADDBACKUP:
			rpl_tree = init_mi_tree( 500, "Could not set backup route", 27);
			if(rpl_tree == NULL)
				return 0;
			break;
		case E_DELBACKUP:
			rpl_tree = init_mi_tree( 400, "Could not delete or deactivate route, it is backup for other routes", 68);
			if(rpl_tree == NULL)
				return 0;
			break;
		case E_LOADCONF:
			rpl_tree = init_mi_tree( 500, "Could not load config from file", 32);
			if(rpl_tree == NULL)
				return 0;
			break;
		case E_SAVECONF:
			rpl_tree = init_mi_tree( 500, "Could not save config", 22);
			if(rpl_tree == NULL)
				return 0;
			break;
		case E_INVALIDOPT:
			rpl_tree = init_mi_tree( 400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);
			if(rpl_tree == NULL)
				return 0;
			break;
		case E_MISSOPT:
			rpl_tree = init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);
			if(rpl_tree == NULL)
				return 0;
			break;
		case E_RULEFIXUP:
			rpl_tree = init_mi_tree( 500, "Could not fixup rules", 22);
			if(rpl_tree == NULL)
				return 0;
			break;
		case E_NOUPDATE:
			rpl_tree = init_mi_tree( 500, "No match for update found", 26);
			if(rpl_tree == NULL)
				return 0;
			break;
		case E_HELP:
			return print_replace_help();
			break;
		default:
			rpl_tree = init_mi_tree( 500, "An error occured", 17);
			if(rpl_tree == NULL)
				return 0;
			break;
	}
	return rpl_tree;
}
