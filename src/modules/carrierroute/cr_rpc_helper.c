/*
 * Copyright (C) 2007-2020 1&1 Internet AG
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "cr_rpc.h"
#include "cr_rule.h"
#include "carrierroute.h"
#include "../../core/str.h"
#include "../../core/sr_module.h"

int fifo_err;

static int updated;


static int str_toklen(str *str, const char *delims)
{
	int len;

	if((str == NULL) || (str->s == NULL)) {
		/* No more tokens */
		return -1;
	}

	len = 0;
	while(len < str->len) {
		if(strchr(delims, str->s[len]) != NULL) {
			return len;
		}
		len++;
	}

	return len;
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
static int update_route_data_recursor(
		struct dtrie_node_t *node, str *act_domain, rpc_opt_t *opts)
{
	int i, hash = 0;
	struct route_rule *rr, *prev = NULL, *tmp, *backup;
	struct route_flags *rf;

	rf = (struct route_flags *)(node->data);
	if(rf && rf->rule_list) {
		rr = rf->rule_list;
		while(rr) {
			if((!opts->domain.len
					   || (strncmp(opts->domain.s, OPT_STAR, strlen(OPT_STAR))
							   == 0)
					   || ((opts->domain.len == act_domain->len)
							   && (strncmp(opts->domain.s, act_domain->s,
										   opts->domain.len)
									   == 0)))
					&& ((!opts->prefix.len && !rr->prefix.len)
							|| (strncmp(opts->prefix.s, OPT_STAR,
										strlen(OPT_STAR))
									== 0)
							|| (rr->prefix.len == opts->prefix.len
									&& (strncmp(opts->prefix.s, rr->prefix.s,
												opts->prefix.len)
											== 0)))
					&& ((!opts->host.len && !rr->host.s)
							|| (strncmp(opts->host.s, OPT_STAR,
										strlen(OPT_STAR))
									== 0)
							|| ((strncmp(rr->host.s, opts->host.s,
										 opts->host.len)
										== 0)
									&& (rr->host.len == opts->host.len)))
					&& ((opts->prob < 0) || (opts->prob == rr->prob))) {
				switch(opts->cmd) {
					case OPT_REPLACE:
						LM_INFO("replace host %.*s with %.*s\n", rr->host.len,
								rr->host.s, opts->new_host.len,
								opts->new_host.s);
						if(rr->host.s) {
							shm_free(rr->host.s);
						}
						if(opts->new_host.len) {
							if((rr->host.s = shm_malloc(opts->new_host.len + 1))
									== NULL) {
								SHM_MEM_ERROR;
								FIFO_ERR(E_NOMEM);
								return -1;
							}
							memmove(rr->host.s, opts->new_host.s,
									opts->new_host.len + 1);
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
						if(remove_backed_up(rr) < 0) {
							LM_ERR("could not reset backup hosts\n");
							FIFO_ERR(E_RESET);
							return -1;
						}
						if(opts->new_host.len > 0) {
							LM_INFO("deactivating host %.*s\n", rr->host.len,
									rr->host.s);
							if(opts->new_host.s
									&& (strcmp(opts->new_host.s, rr->host.s)
											== 0)) {
								LM_ERR("Backup host the same as initial host "
									   "%.*s",
										rr->host.len, rr->host.s);
								FIFO_ERR(E_WRONGOPT);
								return -1;
							}
							if(opts->new_host.len == 1
									&& opts->new_host.s[0] == 'a') {
								if((backup = find_auto_backup(rf, rr))
										== NULL) {
									LM_ERR("didn't find auto backup route\n");
									FIFO_ERR(E_NOAUTOBACKUP);
									return -1;
								}
							} else {
								errno = 0;
								hash = strtol(opts->new_host.s, NULL, 10);
								if(errno == EINVAL || errno == ERANGE) {
									if((backup = find_rule_by_hash(rf, hash))
											== NULL) {
										LM_ERR("didn't find given backup route "
											   "(hash %i)\n",
												hash);
										FIFO_ERR(E_NOHASHBACKUP);
										return -1;
									}
								} else {
									if((backup = find_rule_by_host(
												rf, &opts->new_host))
											== NULL) {
										LM_ERR("didn't find given backup route "
											   "(host %.*s)\n",
												opts->new_host.len,
												opts->new_host.s);
										FIFO_ERR(E_NOHOSTBACKUP);
										return -1;
									}
								}
							}
							if(add_backup_rule(rr, backup) < 0) {
								LM_ERR("couldn't set backup route\n");
								FIFO_ERR(E_ADDBACKUP);
								return -1;
							}
						} else {
							if(rr->backed_up) {
								LM_ERR("can't deactivate route without backup "
									   "route because it is backup route for "
									   "others\n");
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
						LM_INFO("activating host %.*s\n", rr->host.len,
								rr->host.s);
						if(remove_backed_up(rr) < 0) {
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
						LM_INFO("removing host %.*s\n", rr->host.len,
								rr->host.s);
						if(rr->backed_up) {
							LM_ERR("cannot remove host %.*s which is backup "
								   "for other hosts\n",
									rr->host.len, rr->host.s);
							FIFO_ERR(E_DELBACKUP);
							return -1;
						}
						if(remove_backed_up(rr) < 0) {
							LM_ERR("could not reset backup hosts\n");
							FIFO_ERR(E_RESET);
							return -1;
						}
						if(prev) {
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
	for(i = 0; i < cr_match_mode; i++) {
		if(node->child[i]) {
			if(update_route_data_recursor(node->child[i], act_domain, opts)
					< 0) {
				return -1;
			}
		}
	}
	return 0;
}


/**
 * Traverses the routing tree and prints route rules if present.
 *
 * @param rpc - RPC API structure
 * @param ctx - RPC context
 * @param gh - RPC structure pointer
 * @param node pointer to the routing tree node
 * @param prefix carries the current scan prefix
 *
 * @return 0 for success, negative result for error
 */
int dump_tree_recursor(rpc_t *rpc, void *ctx, void *gh,
		struct dtrie_node_t *node, char *prefix)
{
	char s[256];
	char *p;
	int i, len;
	struct route_flags *rf;
	struct route_rule *rr;
	struct route_rule_p_list *rl;
	double prob;
	void *hh, *ih;

	len = strlen(prefix);
	if(len > 254) {
		LM_ERR("prefix too large");
		return -1;
	}
	strcpy(s, prefix);
	p = s + len;
	p[1] = '\0';
	for(i = 0; i < cr_match_mode; ++i) {
		if(node->child[i] != NULL) {
			*p = i + '0';
			/* if there is a problem in processing the child nodes .. return an error */
			if(dump_tree_recursor(rpc, ctx, gh, node->child[i], s) < 0)
				return -1;
		}
	}
	*p = '\0';
	for(rf = (struct route_flags *)(node->data); rf != NULL; rf = rf->next) {
		for(rr = rf->rule_list; rr != NULL; rr = rr->next) {
			if(rf->dice_max) {
				prob = (double)(rr->prob * DICE_MAX) / (double)rf->dice_max;
			} else {
				prob = rr->prob;
			}

			if(rpc->array_add(gh, "{", &hh) < 0) {
				rpc->fault(ctx, 500, "Failed to add data to response");
				return -1;
			}

			if(rpc->struct_add(hh, "sfSsdSSS", "prefix",
					   len > 0 ? prefix : "NULL", "prob", prob * 100, "host",
					   &rr->host, "status", (rr->status ? "ON" : "OFF"),
					   "strip", rr->strip, "prefix", &rr->local_prefix,
					   "suffix", &rr->local_suffix, "comment", &rr->comment)
					< 0) {
				rpc->fault(ctx, 500, "Internal error - routes structure");
				return -1;
			}

			if(!rr->status && rr->backup && rr->backup->rr) {
				if(rpc->struct_add(hh, "S", "backup_by", &rr->backup->rr->host)
						< 0) {
					rpc->fault(ctx, 500,
							"Failed to add backup by info to response");
					return -1;
				}
			}
			if(rr->backed_up) {
				if(rpc->struct_add(hh, "[", "backup_for", &ih) < 0) {
					rpc->fault(ctx, 500,
							"Failed to add backup for data to response");
					return -1;
				}
				rl = rr->backed_up;
				i = 0;
				while(rl) {
					if(rl->rr) {
						if(rpc->array_add(ih, "S", &rl->rr->host) < 0) {
							rpc->fault(ctx, 500,
									"Failed to add backup for data to "
									"response");
							return -1;
						}
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
int get_rpc_opts(str *buf, rpc_opt_t *opts, unsigned int opt_set[])
{
	int opt_argc = 0;
	str opt_argv[20];
	int i, op = -1;
	unsigned int used_opts = 0;
	int toklen;

	memset(opt_argv, 0, sizeof(opt_argv));
	memset(opts, 0, sizeof(rpc_opt_t));
	opts->prob = -1;

	while((toklen = str_toklen(buf, " \t\r\n")) >= 0 && opt_argc < 20) {
		buf->s[toklen] =
				'\0'; /* insert zero termination, since strtod might be used later on it */
		opt_argv[opt_argc].len = toklen;
		opt_argv[opt_argc].s = buf->s;
		buf->s += toklen + 1;
		buf->len -= toklen + 1;
		LM_DBG("found arg[%i]: %.*s\n", opt_argc, opt_argv[opt_argc].len,
				opt_argv[opt_argc].s);
		opt_argc++;
	}
	for(i = 0; i < opt_argc; i++) {
		LM_DBG("token %.*s", opt_argv[i].len, opt_argv[i].s);
		if(opt_argv[i].len >= 1) {
			switch(*opt_argv[i].s) {
				case '-':
					/* -{OPTION}{PARAMETER} is not allowed */
					if(opt_argv[i].len != 2) {
						FIFO_ERR(E_WRONGOPT);
						LM_ERR("Unknown option: %.*s\n", opt_argv[i].len,
								opt_argv[i].s);
						return -1;
					}

					switch(opt_argv[i].s[1]) {
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
							LM_ERR("FIFO ERR E_HELP");
							FIFO_ERR(E_HELP);
							return -1;
						default: {
							FIFO_ERR(E_WRONGOPT);
							LM_ERR("Unknown option: %.*s\n", opt_argv[i].len,
									opt_argv[i].s);
							return -1;
						}
					}
					break;
				default:
					switch(op) {
						case OPT_DOMAIN:
							opts->domain = opt_argv[i];
							op = -1;
							break;
						case OPT_PREFIX:
							if(str_strcasecmp(&opt_argv[i], &CR_EMPTY_PREFIX)
									== 0) {
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
							opts->prob = strtod(opt_argv[i].s,
									NULL); /* we can use str.s since we zero terminated it earlier */
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
							LM_ERR("No option given\n");
							FIFO_ERR(E_NOOPT);
							return -1;
						}
					}
					break;
			}
		}
	}
	if((used_opts & opt_set[OPT_INVALID]) != 0) {
		LM_ERR("invalid option\n");
		FIFO_ERR(E_INVALIDOPT);
		return -1;
	}
	if((used_opts & opt_set[OPT_MANDATORY]) != opt_set[OPT_MANDATORY]) {
		LM_ERR("option missing\n");
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
int update_route_data(rpc_opt_t *opts)
{
	struct route_data_t *rd;
	int i, j;
	int domain_id;
	str tmp_domain;
	str tmp_prefix;
	str tmp_host;
	str tmp_rewrite_prefix;
	str tmp_rewrite_suffix;
	str tmp_comment = str_init("");

	if((rd = shm_malloc(sizeof(struct route_data_t))) == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}
	memset(rd, 0, sizeof(struct route_data_t));
	if(load_config(rd) < 0) {
		LM_ERR("could not load config");
		FIFO_ERR(E_LOADCONF);
		return -1;
	}

	if(rule_fixup(rd) < 0) {
		LM_ERR("could not fixup rules");
		FIFO_ERR(E_RULEFIXUP);
		return -1;
	}
	updated = 0;

	if(opts->cmd == OPT_ADD) {
		tmp_domain = opts->domain;
		tmp_prefix = opts->prefix;
		tmp_host = opts->host;
		tmp_rewrite_prefix = opts->rewrite_prefix;
		tmp_rewrite_suffix = opts->rewrite_suffix;
		if(tmp_domain.s == NULL) {
			tmp_domain.s = "";
			tmp_domain.len = 0;
		}
		if(tmp_prefix.s == NULL) {
			tmp_prefix.s = "";
			tmp_prefix.len = 0;
		}
		if(tmp_host.s == NULL) {
			tmp_host.s = "";
			tmp_host.len = 0;
		}
		if(tmp_rewrite_prefix.s == NULL) {
			tmp_rewrite_prefix.s = "";
			tmp_rewrite_prefix.len = 0;
		}
		if(tmp_rewrite_suffix.s == NULL) {
			tmp_rewrite_suffix.s = "";
			tmp_rewrite_suffix.len = 0;
		}

		domain_id = map_name2id(rd->domain_map, rd->domain_num, &tmp_domain);
		if(domain_id < 0) {
			LM_ERR("cannot find id for domain '%.*s'", tmp_domain.len,
					tmp_domain.s);
			goto errout;
		}

		if(add_route(rd, 1, domain_id, &tmp_prefix, 0, 0, 0, opts->prob,
				   &tmp_host, opts->strip, &tmp_rewrite_prefix,
				   &tmp_rewrite_suffix, opts->status, opts->hash_index, -1,
				   NULL, &tmp_comment)
				< 0) {
			goto errout;
		}
		updated = 1;
		if(rule_fixup(rd) < 0) {
			LM_ERR("could not fixup rules after route appending");
			FIFO_ERR(E_RULEFIXUP);
			goto errout;
		}
	} else {
		for(i = 0; i < rd->carrier_num; i++) {
			if(rd->carriers[i]) {
				for(j = 0; j < rd->carriers[i]->domain_num; j++) {
					if(rd->carriers[i]->domains[j]
							&& rd->carriers[i]->domains[j]->tree) {
						if(update_route_data_recursor(
								   rd->carriers[i]->domains[j]->tree,
								   rd->carriers[i]->domains[j]->name, opts)
								< 0) {
							goto errout;
						}
					}
				}
			}
		}
	}

	if(!updated) {
		LM_ERR("no match for update found");
		FIFO_ERR(E_NOUPDATE);
		goto errout;
	}

	if(save_config(rd) < 0) {
		LM_ERR("could not save config");
		FIFO_ERR(E_SAVECONF);
		goto errout;
	}

	if(reload_route_data() == -1) {
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
