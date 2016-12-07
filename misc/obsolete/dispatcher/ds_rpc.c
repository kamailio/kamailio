/**
 * $Id$
 *
 * dispatcher module -- stateless load balancing
 *
 * Copyright (C) 2004-2006 FhG Fokus
 * Copyright (C) 2005-2008 Hendrik Scholz <hendrik.scholz@freenet-ag.de>
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "../../sr_module.h"

#include "dispatcher.h"
#include "ds_rpc.h"

/******************************************************************************
 * 
 * dispatcher_dump()
 *
 * fifo command handler for 'dispatcher_dump' command
 * dumps the active dispatcher config to the pipe
 *
 *****************************************************************************/

void rpc_dump(rpc_t *rpc, void *c) {

    extern int *ds_activelist;
    extern char ***ds_setp_a, ***ds_setp_b;
    extern int **ds_setlen_a, **ds_setlen_b;

    int set, node;

	if (rpc->printf(c,
		"flags: DS_MAX_SETS: %d DS_MAX_NODES: %d DS_MAX_URILEN: %d",
		DS_MAX_SETS, DS_MAX_NODES, DS_MAX_URILEN) < 0) return;
	if (rpc->printf(c,
		"Active dispatcher list: %d", *ds_activelist) < 0) return;
    if (*ds_activelist == 0) {
        for (set = 0; set < DS_MAX_SETS; set++) {
            if (ds_setlen_a[set] == 0) {
				if (rpc->printf(c, "Set %2d is empty", set) < 0) return;
            } else {
				if (rpc->printf(c, "Set %2d:", set) < 0) return;
                for (node = 0; node < (long int) ds_setlen_a[set]; node++) {
					if (rpc->printf(c, "  node %3d %s",
						node, ds_setp_a[set][node]) < 0) return;
                }
            }
        }
    } else {
        for (set = 0; set < DS_MAX_SETS; set++) {
            if (ds_setlen_b[set] == 0) {
				if (rpc->printf(c, "Set %2d is empty", set) < 0) return;
            } else {
				if (rpc->printf(c, "Set %2d:", set) < 0) return;
                for (node = 0; node < (long int) ds_setlen_b[set]; node++) {
					if (rpc->printf(c, "  node %3d %s",
						node, ds_setp_b[set][node]) < 0) return;
                }
            }
        }
    }
	rpc->printf(c, "End of dispatcher list");
    return;
}

/******************************************************************************
 *  
 *  dispatcher_reload()
 *
 *  reload a dispatcher list and activate if successful
 *
 *****************************************************************************/

void rpc_reload(rpc_t *rpc, void *c) {

    extern char *dslistfile;
    extern int *ds_activelist;

    LOG(L_ERR, "DISPATCHER module reloading\n");
    if (ds_load_list(dslistfile) == 0) {
		DS_SWITCH_ACTIVE_LIST
		rpc->printf(c, "dispatcher list %d activated", *ds_activelist);
    } else {
		rpc->printf(c, "dispatcher list reload failed");
    }
	return ;
}

/* rpc function titles */
static const char *rpc_dump_doc[2] = {
	"Dump dispatcher set configuration",
	0
};
static const char *rpc_reload_doc[2] = {
	"Reload dispatcher list from file",
	0
};

rpc_export_t rpc_methods[] = {
    {"dispatcher.dump",     rpc_dump,       rpc_dump_doc,       0},
    {"dispatcher.reload",   rpc_reload,     rpc_reload_doc,     0},
    {0, 0, 0, 0}
};

