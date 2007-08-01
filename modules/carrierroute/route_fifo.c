/*
 * $Id$
 *
 * Copyright (C) 2007 1&1 Internet AG
 *
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/**
 * @file route_fifo.c
 *
 * @author: Jonas Appel <jonas dot appel at schlund dot de>
 *
 * @date Mi Jan 24 2007
 *
 * Copyright: 2007 1 & 1 Internet AG
 *
 * @brief Functions for modifying routing data vua fifo commands
 *
 */

#include "route_fifo.h"
#include "carrierroute.h"
#include "route_rule.h"
#include "route_config.h"

#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include <getopt.h>
#include <ctype.h>
#include <stdlib.h>
#include "../../str.h"

/**
 * @var defines the option set for the different fifo commands
 * Every line is for a command,
 * The first field defines the required options, the second field defines the
 * optional options and the third field defines the invalid options.
 */
static unsigned int opt_settings[5][3] = {{O_PREFIX|O_DOMAIN|O_HOST|O_PROB, O_R_PREFIX|O_R_SUFFIX|O_H_INDEX, O_NEW_TARGET},
        {O_HOST|O_DOMAIN|O_PREFIX, O_PROB, O_R_PREFIX|O_R_SUFFIX|O_NEW_TARGET|O_H_INDEX},
        {O_HOST|O_NEW_TARGET, O_PREFIX|O_DOMAIN|O_PROB, O_R_PREFIX|O_R_SUFFIX|O_H_INDEX},
        {O_HOST|O_DOMAIN|O_PREFIX, O_PROB, O_R_PREFIX|O_R_SUFFIX|O_NEW_TARGET|O_H_INDEX},
        {O_HOST|O_DOMAIN|O_PREFIX, O_PROB, O_R_PREFIX|O_R_SUFFIX|O_NEW_TARGET|O_H_INDEX}};


static int dump_tree_recursor (struct mi_node* msg, struct route_tree_item *tree, char *prefix);

static struct mi_root* print_replace_help();

static int get_fifo_opts(char * buf, fifo_opt_t * opts, unsigned int opt_set[], struct mi_root* error_msg);

static int update_route_data(fifo_opt_t * opts);

static int update_route_data_recursor(struct route_tree_item * rt, char * act_domain, fifo_opt_t * opts);

/**
 * reloads the routing data
 *
 * @param mi_root the fifo command tree
 * @param param the parameter
 *
 * @return code 200 on success, code 500 on failure
 */
struct mi_root* reload_fifo (struct mi_root* cmd_tree, void *param) {
	struct mi_root * tmp = NULL;

	if (prepare_route_tree () == -1) {
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
 * @param mi_root the fifo command tree
 * @param param the parameter
 *
 * @return code 200 on success, code 400 or 500 on failure
 */
struct mi_root* dump_fifo (struct mi_root* cmd_tree, void *param) {
	struct rewrite_data * rd;

	if((rd = get_data ()) == NULL) {
		LM_ERR("error during retrieve data\n");
		return init_mi_tree(500, "error during command processing", 31);
	}
		
	struct mi_root* rpl_tree;
	struct mi_node* node = NULL;
	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	if(rpl_tree == NULL)
		return 0;
	node = addf_mi_node_child( &rpl_tree->node, 0, 0, 0, "Printing routing information:");
	if(node == NULL)
		goto error;

	LM_DBG("start processing of data\n");
	int i, j;
 	for (i = 0; i < rd->tree_num; i++) {
 		if (rd->carriers[i]) {
			node = addf_mi_node_child( &rpl_tree->node, 0, 0, 0, "Printing tree for carrier %s (%i)\n", rd->carriers[i] ? rd->carriers[i]->name.s : "<empty>", rd->carriers[i] ? rd->carriers[i]->id : 0);
			if(node == NULL)
				goto error;
 			for (j=0; j<rd->carriers[i]->tree_num; j++) {
 				if (rd->carriers[i]->trees[j] && rd->carriers[i]->trees[j]->tree) {
					node = addf_mi_node_child( &rpl_tree->node, 0, 0, 0, "Printing tree for domain %s\n", rd->carriers[i]->trees[j] ? rd->carriers[i]->trees[j]->name.s : "<empty>");
					if(node == NULL)
						goto error;
 					dump_tree_recursor (&rpl_tree->node, rd->carriers[i]->trees[j]->tree, "");
				}
 			}
		}
	}
	release_data (rd);
	return rpl_tree;
	return 0;

error:
	release_data (rd);
	free_mi_tree(rpl_tree);
	return 0;
}

/**
 * replaces the host specified by parameters in the
 * fifo command, can be used only in file mode
 * expect one mi node that contains the command
 *
 * @param mi_root the fifo command tree
 * @param param the parameter
 *
 * @return code 200 on success, code 400 or 500 on failure
 */
struct mi_root* replace_host (struct mi_root* cmd_tree, void *param) {
	struct mi_node *node = NULL;

	int ret;
	fifo_opt_t options;

	if(mode != SP_ROUTE_MODE_FILE) {
		return init_mi_tree(400, "Not running in config file mode, cannot modify route from command line", 70);
	}
	
	node = cmd_tree->node.kids;
	if (node==NULL || node->next!=NULL)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	
	/* look for command */
	char* buf = node->value.s;
	if (buf==NULL)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	struct mi_root* error_msg = NULL;
	if((ret = get_fifo_opts(buf, &options, opt_settings[OPT_REPLACE], error_msg)) <  0) {
		if(ret == -2) {
			return error_msg;
		} else {
			return init_mi_tree(500, "error during command processing", 31);
		}
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
 * @param mi_root the fifo command tree
 * @param param the parameter
 *
 * @return code 200 on success, code 400 or 500 on failure
 */
struct mi_root* deactivate_host (struct mi_root* cmd_tree, void *param) {
	struct mi_node *node = NULL;

	int ret;
	fifo_opt_t options;

	if(mode != SP_ROUTE_MODE_FILE) {
		return init_mi_tree(400, "Not running in config file mode, cannot modify route from command line", 70);
	}

	node = cmd_tree->node.kids;
	if (node==NULL || node->next!=NULL)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	
	/* look for command */
	char* buf = node->value.s;
	if (buf==NULL)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	struct mi_root* error_msg = NULL;
	if((ret = get_fifo_opts(buf, &options, opt_settings[OPT_DEACTIVATE], error_msg)) <  0) {
		if(ret == -2) {
			return error_msg;
		} else {
			return init_mi_tree(500, "error during command processing", 31);
		}
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
 * @param mi_root the fifo command tree
 * @param param the parameter
 *
 * @return code 200 on success, code 400 or 500 on failure
 */
struct mi_root* activate_host (struct mi_root* cmd_tree, void *param) {
	struct mi_node *node = NULL;

	int ret;
	fifo_opt_t options;

	if(mode != SP_ROUTE_MODE_FILE) {
		return init_mi_tree(400, "Not running in config file mode, cannot modify route from command line", 70);
	}

	node = cmd_tree->node.kids;
	if (node==NULL || node->next!=NULL)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	
	/* look for command */
	char* buf = node->value.s;
	if (buf==NULL)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	struct mi_root* error_msg = NULL;
	if((ret = get_fifo_opts(buf, &options, opt_settings[OPT_ACTIVATE], error_msg)) <  0) {
		if(ret == -2) {
			return error_msg;
		} else {
			return init_mi_tree(500, "error during command processing", 31);
		}
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
 * @param mi_root the fifo command tree
 * @param param the parameter
 *
 * @return code 200 on success, code 400 or 500 on failure
 */
struct mi_root* add_host (struct mi_root* cmd_tree, void *param) {
	struct mi_node *node = NULL;

	int ret;
	fifo_opt_t options;

	if(mode != SP_ROUTE_MODE_FILE) {
		return init_mi_tree(400, "Not running in config file mode, cannot modify route from command line", 70);
	}

	node = cmd_tree->node.kids;
	if (node==NULL || node->next!=NULL)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	
	/* look for command */
	char* buf = node->value.s;
	if (buf==NULL)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	struct mi_root* error_msg = NULL;
	if((ret = get_fifo_opts(buf, &options, opt_settings[OPT_ADD], error_msg)) <  0) {
		if(ret == -2) {
			return error_msg;
		} else {
			return init_mi_tree(500, "error during command processing", 31);
		}
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
 * @param mi_root the fifo command tree
 * @param param the parameter
 *
 * @return code 200 on success, code 400 or 500 on failure
 */
struct mi_root* delete_host (struct mi_root* cmd_tree, void * param) {
	struct mi_node *node = NULL;

	int ret;
	fifo_opt_t options;

	if(mode != SP_ROUTE_MODE_FILE) {
		return init_mi_tree(400, "Not running in config file mode, cannot modify route from command line", 70);
	}

	node = cmd_tree->node.kids;
	if (node==NULL || node->next!=NULL)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	
	/* look for command */
	char* buf = node->value.s;
	if (buf==NULL)
		return init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	struct mi_root* error_msg = NULL;
	if((ret = get_fifo_opts(buf, &options, opt_settings[OPT_REMOVE], error_msg)) <  0) {
		if(ret == -2) {
			return error_msg;
		} else {
			return init_mi_tree(500, "error during command processing", 31);
		}
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
 * @param tree pointer to the routing tree node
 * @param prefix carries the current scan prefix
 * @param file filehandle to the output file
 *
 * @return mi node containing the route rules
 */
static int dump_tree_recursor (struct mi_node* msg, struct route_tree_item *tree, char *prefix) {
	char s[256];
	char *p;
	int i;
	struct route_rule *rr;

	strcpy (s, prefix);
	p = s + strlen (s);
	p[1] = '\0';
	for (i = 0; i < 10; ++i) {
		if (tree->nodes[i] != NULL) {
			*p = i + '0';
			dump_tree_recursor (msg->next, tree->nodes[i], s);
		}
	}
	*p = '\0';
	for (rr = tree->rule_list; rr != NULL; rr = rr->next) {
		addf_mi_node_child(msg->next, 0, 0, 0, "%10s: %0.3f %%, '%.*s': %s, '%i', '%.*s', '%.*s', '%.*s'\n",
		         prefix, rr->prob * 100, rr->host.len, rr->host.s,
		         (rr->status ? "ON" : "OFF"), rr->strip,
		         rr->local_prefix.len, rr->local_prefix.s,
		         rr->local_suffix.len, rr->local_suffix.s,
		         rr->comment.len, rr->comment.s);
	}
	return 0;
}

/**
 * prints a short help text for fifo command usage
 *
 * @param file filehandle for writing output
 */
static struct mi_root* print_replace_help() {
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
	node = addf_mi_node_child( &rpl_tree->node, 0, 0, 0, "\t-%c replacement host", OPT_NEW_TARGET_CHR);
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
 * parses the command line argument for options
 *
 * @param buf the command line argument
 * @param opts pointer t
 *
 * @return 0 on success, -1 on failure
 *
 * @see dump_fifo()
 */
static int get_fifo_opts(char * buf, fifo_opt_t * opts, unsigned int opt_set[], struct mi_root* error_msg) {
	int opt_argc = 0;
	char * opt_argv[20];
	int i, op = -1;
	unsigned int used_opts = 0;
	error_msg = init_mi_tree( 200, MI_OK_S, MI_OK_LEN );

	memset(opt_argv, 0, sizeof(opt_argv));
	memset(opts, 0, sizeof(fifo_opt_t));
	opts->prob = -1;

	while((opt_argv[opt_argc] = strsep(&buf, " \t\r\n")) != NULL && opt_argc < 20) {
		LM_DBG("found arg[%i]: %s\n", opt_argc, opt_argv[opt_argc]);
		opt_argc++;
	}
	opt_argv[opt_argc] = NULL;

	for(i=0; i<opt_argc; i++) {
		if(opt_argv[i] && strlen(opt_argv[i]) >= 1) {
			switch(*opt_argv[i]) {
					case '-': switch(*(opt_argv[i] + 1)) {
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
							error_msg = print_replace_help();
							return -2;
							default: {
								error_msg = init_mi_tree( 400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);
								LM_DBG("Unknown option: %s\n", opt_argv[i]);
								return -2;
							}
					}
					break;
					default: switch(op) {
							case OPT_DOMAIN:
							opts->domain.s = opt_argv[i];
							opts->domain.len = strlen(opt_argv[i]);
							op = -1;
							break;
							case OPT_PREFIX:
							opts->prefix.s = opt_argv[i];
							opts->prefix.len = strlen(opt_argv[i]);
							op = -1;
							break;
							case OPT_HOST:
							opts->host.s = opt_argv[i];
							opts->host.len = strlen(opt_argv[i]);
							op = -1;
							break;
							case OPT_NEW_TARGET:
							opts->new_host.s = opt_argv[i];
							opts->new_host.len = strlen(opt_argv[i]);
							op = -1;
							break;
							case OPT_PROB:
							opts->prob = strtod(opt_argv[i], NULL);
							op = -1;
							break;
							case OPT_R_PREFIX:
							opts->rewrite_prefix.s = opt_argv[i];
							opts->rewrite_prefix.len = strlen(opt_argv[i]);
							op = -1;
							break;
							case OPT_STRIP:
							opts->strip = atoi(opt_argv[i]);
							op = -1;
							break;
							case OPT_R_SUFFIX:
							opts->rewrite_suffix.s = opt_argv[i];
							opts->rewrite_suffix.len = strlen(opt_argv[i]);
							op = -1;
							break;
							case OPT_HASH_INDEX:
							opts->hash_index = atoi(opt_argv[i]);
							op = -1;
							break;
							default: {
								error_msg = init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);
								LM_DBG("No option given\n");
								return -2;
							}
					}
					break;
			}
		}
	}
	if((used_opts & opt_set[OPT_INVALID]) != 0) {
		error_msg = init_mi_tree( 400, MI_BAD_PARM_S, MI_BAD_PARM_LEN);
		LM_DBG("invalid option\n");
		return -2;
	}
	if((used_opts & opt_set[OPT_MANDATORY]) != opt_set[OPT_MANDATORY]) {
		error_msg = init_mi_tree(400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);
		LM_DBG("option missing\n");
		return -2;
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
	struct rewrite_data * rd;
	int i,j;

	if ((rd = shm_malloc(sizeof(struct rewrite_data))) == NULL) {
		LM_ERR("out of shared mem\n");
		return -1;
	}
	memset(rd, 0, sizeof(struct rewrite_data));
	if (load_config(rd) < 0) {
		return -1;
	}

	if (opts->cmd == OPT_ADD) {
		if (add_route(rd, 1, opts->domain.s, opts->prefix.s, 0, opts->prob,
		              opts->host.s, opts->strip, opts->rewrite_prefix.s, opts->rewrite_suffix.s,
		              opts->status, opts->hash_index, NULL) < 0) {
			goto errout;
		}
	} else {
		for (i=0; i<rd->tree_num; i++) {
			if(rd->carriers[i]){
			for (j=0; j<rd->carriers[i]->tree_num; j++) {
				if (rd->carriers[i]->trees[j] && rd->carriers[i]->trees[j]->tree) {
					if (update_route_data_recursor(rd->carriers[i]->trees[j]->tree, rd->carriers[i]->trees[j]->name.s, opts) < 0) {
						goto errout;
					}
				}
			}
			}
		}
	}

	if (save_config(rd) < -1) {
		goto errout;
	}

	if (prepare_route_tree() == -1) {
		goto errout;
	}

	destroy_rewrite_data(rd);
	return 0;
errout:
	destroy_rewrite_data(rd);
	return -1;
}

/**
 * Does the work for update_route_data by recursively
 * traversing the routing tree
 *
 * @param rt points to the current routing tree node
 * @param act_domain routing domain which is currently
 * searched
 * @param opts points to the fifo command option structure
 *
 * @see update_route_data()
 *
 * @return 0 on success, -1 on failure
 */
static int update_route_data_recursor(struct route_tree_item * rt, char * act_domain, fifo_opt_t * opts) {
	int i;
	struct route_rule * rr, * prev = NULL, * tmp;
	if (rt->rule_list) {
		rr = rt->rule_list;
		while (rr) {
			if ((!opts->domain.len || (strncmp(opts->domain.s, OPT_STAR, strlen(OPT_STAR)) == 0)
			        || ((strncmp(opts->domain.s, act_domain, opts->domain.len) == 0) && (opts->domain.len == strlen(act_domain))))
			        && ((!opts->prefix.len && !rr->prefix.len) || (strncmp(opts->prefix.s, OPT_STAR, strlen(OPT_STAR)) == 0)
			            || (rr->prefix.len == opts->prefix.len && (strncmp(opts->prefix.s, rr->prefix.s, opts->prefix.len) == 0)))
			        && ((!opts->host.len && !rr->host.s) || (strncmp(opts->host.s, OPT_STAR, strlen(OPT_STAR)) == 0)
			            || ((strncmp(rr->host.s, opts->host.s, opts->host.len) == 0) && (rr->host.len == opts->host.len)))
			        && ((opts->prob < 0) || (opts->prob == rr->prob))) {
				switch (opts->cmd) {
					case OPT_REPLACE:
						LM_INFO("replace host %s with %s\n", rr->host.s, opts->new_host.s);
						if (rr->host.s) {
							shm_free(rr->host.s);
						}
						if (opts->new_host.len) {
							if ((rr->host.s = shm_malloc(opts->new_host.len + 1)) == NULL) {
								LM_ERR("out of shared mem\n");
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
						break;
					case OPT_DEACTIVATE:
					case OPT_ACTIVATE:
						LM_INFO("(de)activating host %s\n", rr->host.s);
						rr->status = opts->status;
						prev = rr;
						rr = rr->next;
						break;
					case OPT_REMOVE:
						LM_INFO("removing host %s\n", rr->host.s);
						if (prev) {
							prev->next = rr->next;
							tmp = rr;
							rr = prev;
							destroy_route_rule(tmp);
							prev = rr;
							rr = rr->next;
						} else {
							rt->rule_list = rr->next;
							tmp = rr;
							rr = rt->rule_list;
							destroy_route_rule(tmp);
						}
						rt->rule_num--;
						rt->max_locdb--;
						break;
					default: rr = rr->next; break;
				}
			} else {
				prev = rr;
				rr = rr->next;
			}
		}
	}
	for (i=0; i<10; i++) {
		if (rt->nodes[i]) {
			if (update_route_data_recursor(rt->nodes[i], act_domain, opts) < 0) {
				return -1;
			}
		}
	}
	return 0;
}
