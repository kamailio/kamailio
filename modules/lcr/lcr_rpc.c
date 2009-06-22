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

#include "lcr_rpc.h"

#ifdef RPC_SUPPORT

#include "lcr_mod.h"
#include "../../ip_addr.h"


static const char* reload_doc[2] = {
	"Reload gw and lcr tables from database.",
	0
};


static void reload(rpc_t* rpc, void* c)
{
        int i;
	lock_get(reload_lock);
	for (i = 1; i <= lcr_count; i++) {
	        if (reload_gws_and_lcrs(i) != 1)
		        rpc->fault(c, 500, "LCR Module Reload Failed");
	}
	lock_release(reload_lock);
}


static const char* dump_gws_doc[2] = {
	"Dump the contents of the gw table.",
	0
};


static void dump_gws(rpc_t* rpc, void* c)
{
	void* st;
	unsigned int i, j;
	enum sip_protos transport;
	str hostname;
	str tag;
	struct gw_info *gws;

	for (j = 1; j <= lcr_count; j++) {
	
	    gws = gwtp[j];

	    for (i = 1; i <= gws[0].ip_addr; i++) {
		if (rpc->add(c, "{", &st) < 0) return;
		rpc->struct_add(st, "d", "lcr_id", j);
		rpc->struct_add(st, "d", "grp_id", gws[i].grp_id);
		rpc->struct_printf(st,   "ip_addr", "%d.%d.%d.%d",
				   (gws[i].ip_addr << 24) >> 24,
				   ((gws[i].ip_addr >> 8) << 24) >> 24,
				   ((gws[i].ip_addr >> 16) << 24) >> 24,
				   gws[i].ip_addr >> 24);
		hostname.s=gws[i].hostname;
		hostname.len=gws[i].hostname_len;
		rpc->struct_add(st, "S", "hostname", &hostname);
		if  (gws[i].port > 0)
			rpc->struct_add(st, "d", "port", gws[i].port);
		if (gws[i].scheme == SIP_URI_T) {
		    rpc->struct_add(st, "s", "scheme", "sip");
		} else {
		    rpc->struct_add(st, "s", "scheme", "sips");
		}
		transport = gws[i].transport;
		switch(transport){
			case PROTO_UDP:
				rpc->struct_add(st, "s", "transport", "UDP");
				break;
			case PROTO_TCP:
				rpc->struct_add(st, "s", "transport", "TCP");
				break;
			case PROTO_TLS:
				rpc->struct_add(st, "s", "transport", "TLS");
				break;
			case PROTO_SCTP:
				rpc->struct_add(st, "s", "transport", "SCTP");
				break;
			case PROTO_NONE:
			    break;
		}
		tag.s=gws[i].tag;
		tag.len=gws[i].tag_len;
		rpc->struct_add(st, "dSddd",
				"strip",  gws[i].strip,
				"tag",    gws[i].tag, /* FIXME */
				"weight", gws[i].weight,
				"flags",  &tag,
				"defunct_until",  &gws[i].defunct_until
				);
	    }
	}
}



static const char* dump_lcrs_doc[2] = {
	"Dump the contents of the lcr table.",
	0
};


static void dump_lcrs(rpc_t* rpc, void* c)
{
        int i, j;
	struct lcr_info **lcrs, *lcr_rec;
	void* st;
	str prefix, from_uri, lcr_id;

	for (j = 1; j <= lcr_count; j++) {
	    
	    lcrs = lcrtp[j];

	    for (i = 0; i < lcr_hash_size_param; i++) {
		lcr_rec = lcrs[i];
		while(lcr_rec){
			if (rpc->add(c, "{", &st) < 0) return;
			lcr_id.s = int2str(j, &(lcr_id.len));
			prefix.s=lcr_rec->prefix;
			prefix.len=lcr_rec->prefix_len;
			from_uri.s=lcr_rec->from_uri;
			from_uri.len=lcr_rec->from_uri_len;
			rpc->struct_add(st, "dSSdd",
					"lcr_id", &lcr_id,
					"prefix", &prefix,
					"from_uri", &from_uri,
					"grp_id", lcr_rec->grp_id,
					"priority", lcr_rec->priority
					);
			lcr_rec=lcr_rec->next;
		}
	    }
	    lcr_rec=lcrs[lcr_hash_size_param];
	    while(lcr_rec){
		rpc->add(c, "d", lcr_rec->prefix_len);
		lcr_rec=lcr_rec->next;
	    }
	}
}



rpc_export_t lcr_rpc[] = {
	{"lcr.reload", reload, reload_doc, 0},
	{"lcr.dump_gws",   dump_gws,   dump_gws_doc,   0},
	{"lcr.dump_lcrs",   dump_lcrs,   dump_lcrs_doc,   0},
	{0, 0, 0, 0}
};

#endif /* RPC_SUPPORT */
