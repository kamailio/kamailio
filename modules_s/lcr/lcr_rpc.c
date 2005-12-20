/*
 * Various lcr related functions
 *
 * Copyright (C) 2005 Juha Heinanen
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "lcr_mod.h"
#include "lcr_rpc.h"

static const char* reload_doc[2] = {
	"Reload gateway table from database.",
	0
};


static void reload(rpc_t* rpc, void* c)
{
	if (reload_gws () != 1) {
		rpc->fault(c, 500, "LCR Gateway Reload Failed");
	}
}


static const char* dump_doc[2] = {
	"Dump the contents of the gateway table.",
	0
};


static void dump (rpc_t* rpc, void* c)
{
	void* st;
	unsigned int i;
	uri_transport transport;
	
	for (i = 0; i < MAX_NO_OF_GWS; i++) {
		if ((*gws)[i].ip_addr == 0) {
			return;
		}
		
		if (rpc->add(c, "{", &st) < 0) return;

		if ((*gws)[i].scheme == SIP_URI_T) {
		    rpc->struct_add(st, "s", "scheme", "sip");
		} else {
		    rpc->struct_add(st, "s", "scheme", "sips");
		}
		if ((*gws)[i].port == 0) {
			rpc->struct_printf(st, "host", "%d.%d.%d.%d",
					   ((*gws)[i].ip_addr << 24) >> 24,
					   (((*gws)[i].ip_addr >> 8) << 24) >> 24,
					   (((*gws)[i].ip_addr >> 16) << 24) >> 24,
					   (*gws)[i].ip_addr >> 24);
		} else {
			rpc->struct_printf(st, "host", "%d.%d.%d.%d:%d",
					   ((*gws)[i].ip_addr << 24) >> 24,
					   (((*gws)[i].ip_addr >> 8) << 24) >> 24,
					   (((*gws)[i].ip_addr >> 16) << 24) >> 24,
					   (*gws)[i].ip_addr >> 24,
					   (*gws)[i].port);
		}
		transport = (*gws)[i].transport;
		if (transport == PROTO_UDP) {
			rpc->struct_add(st, "s", "transport", "UDP");
		} else  if (transport == PROTO_TCP) {
			rpc->struct_add(st, "s", "transport", "TCP");
		} else  if (transport == PROTO_TLS) {
			rpc->struct_add(st, "s", "transport", "TLS");
		}
	        if ((*gws)[i].prefix_len) {
			rpc->struct_add(st, "s", "prefix", (*gws)[i].prefix);
		}
	}
}


rpc_export_t lcr_rpc[] = {
	{"lcr.reload", reload, reload_doc, 0},
	{"lcr.dump",   dump,   dump_doc,   0},
	{0, 0, 0, 0}
};
