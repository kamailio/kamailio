/**
 * Copyright (C) 2015 Bicom Systems Ltd, (bicomsystems.com)
 *
 * Author: Seudin Kasumovic (seudin.kasumovic@gmail.com)
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
 *
 */

#ifndef MOD_ERLANG_H_
#define MOD_ERLANG_H_

#include "../../str.h"
#include "../../pvar.h"
#include "../../xavp.h"
#include "erl_api.h"

extern str cookie; /* Erlang cookie */
extern int trace_level; /* Tracing level on the distribution */
extern str cnode_alivename;
extern str cnode_host;
extern int no_cnodes;
extern int rpc_reply_with_struct;

extern str erlang_nodename;
extern str erlang_node_sname;

/* sockets kamailio <-> cnode */
extern int *usocks[2];

extern int csockfd; /* socket to cnode process */

#define KSOCKET	0
#define CSOCKET	1

/**
 * types of command parameter
 */
typedef enum {
	ERL_PARAM_FPARAM,
	ERL_PARAM_XBUFF_SPEC
} erl_param_type;

/**
 * command parameter
 *
 * combine str and PV
 */
typedef struct erl_param_s {
	erl_param_type type;
	union {
		fparam_t fp;
		pv_spec_t sp;
	} value;
} erl_param_t;

/**
 * Erlang ei bind
 */

extern erl_api_t erl_api;

#endif /* MOD_ERLANG_H_ */
