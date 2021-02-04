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

#ifndef __CR_RPC_H
#define __CR_RPC_H

#include "cr_data.h"
#include "cr_carrier.h"
#include "cr_domain.h"
#include "cr_config.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include "../../core/str.h"
#include "../../lib/trie/dtrie.h"

#define E_MISC -1
#define E_NOOPT -2
#define E_WRONGOPT -3
#define E_NOMEM -4
#define E_RESET -5
#define E_NOAUTOBACKUP -6
#define E_NOHASHBACKUP -7
#define E_NOHOSTBACKUP -8
#define E_ADDBACKUP -9
#define E_DELBACKUP -10
#define E_LOADCONF -11
#define E_SAVECONF -12
#define E_INVALIDOPT -13
#define E_MISSOPT -14
#define E_RULEFIXUP -15
#define E_NOUPDATE -16
#define E_HELP -17

#define OPT_ADD 0
#define OPT_REMOVE 1
#define OPT_REPLACE 2
#define OPT_DEACTIVATE 3
#define OPT_ACTIVATE 4

#define OPT_MANDATORY 0
#define OPT_OPTIONAL 1
#define OPT_INVALID 2

#define OPT_PREFIX 0
#define OPT_DOMAIN 1
#define OPT_HOST 2
#define OPT_NEW_TARGET 3
#define OPT_PROB 4
#define OPT_R_PREFIX 5
#define OPT_R_SUFFIX 6
#define OPT_HASH_INDEX 7
#define OPT_STRIP 8

/**
 * Flags for options to determine which options are used
 */
#define O_PREFIX           1
#define O_DOMAIN     (1 << 1)
#define O_HOST       (1 << 2)
#define O_NEW_TARGET (1 << 3)
#define O_PROB       (1 << 4)
#define O_R_PREFIX   (1 << 5)
#define O_R_SUFFIX   (1 << 6)
#define O_H_INDEX    (1 << 7)
#define O_STRIP      (1 << 8)

/**
 * Constants define option characters
 */
#define OPT_PREFIX_CHR 'p'
#define OPT_DOMAIN_CHR 'd'
#define OPT_HOST_CHR 'h'
#define OPT_NEW_TARGET_CHR 't'
#define OPT_PROB_CHR 'w'
#define OPT_R_PREFIX_CHR 'P'
#define OPT_R_SUFFIX_CHR 'S'
#define OPT_HASH_INDEX_CHR 'i'
#define OPT_STRIP_CHR 's'
#define OPT_HELP_CHR '?'

#define OPT_STAR "*"

#define FIFO_ERR(e) (fifo_err = e)

extern int fifo_err;

/**
 * @struct rpc_opt
 * Holds values and command type when a command was passed to fifo
 */
typedef struct rpc_opt {
	unsigned int cmd; /*!< Command type passed to FIFO */
	unsigned int opts; /*!< Options used */
	str domain; /*!< the routing domain */
	str prefix; /*!< the scan prefix */
	double prob; /*!< the weight of the route rule */
	str host; /*!< the hostname or address */
	int strip; /*!< the number of digits to be stripped off */
	str new_host; /*!< the new host when a host is going to be replaced */
	str rewrite_prefix; /*!< the rewrite prefix */
	str rewrite_suffix; /*!< the rewrite suffix */
	int hash_index; /*!< the hash index */
	int status; /*!< the status */
}
rpc_opt_t;

extern rpc_export_t cr_rpc_methods[];

int dump_tree_recursor (rpc_t* rpc, void* ctx, void *gh,
			struct dtrie_node_t *node, char *prefix);

int get_rpc_opts(str * buf, rpc_opt_t * opts, unsigned int opt_set[]);
int update_route_data(rpc_opt_t * opts);

#endif
