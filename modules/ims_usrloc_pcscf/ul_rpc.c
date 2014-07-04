/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * 
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by 
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an 
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 * 
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Fokus. 
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to 
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
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

#include "../../ip_addr.h"
#include "../../dprint.h"

#include "ul_rpc.h"
#include "dlist.h"
#include "udomain.h"

static const char* ul_rpc_dump_doc[2] = {
	"Dump PCSCF contacts and associated identitites",
	0
};

static void ul_rpc_dump(rpc_t* rpc, void* ctx) {
	dlist_t* dl;
	udomain_t* dom;
//	time_t t;
	void* th;
	void* ah;
	void* sh;
	int max, n, i;

//	t = time(0);
	for (dl = root; dl; dl = dl->next) {
		dom = dl->d;
		if (rpc->add(ctx, "{", &th) < 0) {
			rpc->fault(ctx, 500, "Internal error creating top rpc");
			return;
		}
		if (rpc->struct_add(th, "Sd{", "Domain", &dl->name, "Size",
				(int) dom->size, "AoRs", &ah) < 0) {
			rpc->fault(ctx, 500, "Internal error creating inner struct");
			return;
		}

		for (i = 0, n = 0, max = 0; i < dom->size; i++) {
//			lock_ulslot(dom, i);
			n += dom->table[i].n;
			if (max < dom->table[i].n)
				max = dom->table[i].n;
//			for (c = dom->table[i].first; c; c = c->next) {
//				if (rpc->struct_add(ah, "S", "AoR", &c->aor) < 0) {
//					unlock_ulslot(dom, i);
//					rpc->fault(ctx, 500, "Internal error creating aor struct");
//					return;
//				}
//				if (rpc->struct_add(ah, "s", "State", reg_state_to_string(c->reg_state)) < 0) {
//					unlock_ulslot(dom, i);
//					rpc->fault(ctx, 500, "Internal error creating reg state struct");
//					return;
//				}
//				if (c->expires == 0) {
//					if (rpc->struct_add(ah, "s", "Expires", "permanent") < 0) {
//						unlock_ulslot(dom, i);
//						rpc->fault(ctx, 500, "Internal error adding expire");
//						return;
//					}
//				} else if (c->expires == -1/*UL_EXPIRED_TIME*/) {
//					if (rpc->struct_add(ah, "s", "Expires", "deleted") < 0) {
//						unlock_ulslot(dom, i);
//						rpc->fault(ctx, 500, "Internal error adding expire");
//						return;
//					}
//				} else if (t > c->expires) {
//					if (rpc->struct_add(ah, "s", "Expires", "expired") < 0) {
//						unlock_ulslot(dom, i);
//						rpc->fault(ctx, 500, "Internal error adding expire");
//						return;
//					}
//				} else {
//					if (rpc->struct_add(ah, "d", "Expires", (int) (c->expires - t)) < 0) {
//						unlock_ulslot(dom, i);
//						rpc->fault(ctx, 500, "Internal error adding expire");
//						return;
//					}
//				}
//
//				if (rpc->struct_add(ah, "S", "Path", &c->path) < 0) {
//					unlock_ulslot(dom, i);
//					rpc->fault(ctx, 500, "Internal error creating path struct");
//					return;
//				}
//
//				if (rpc->struct_add(ah, "{", "Service Routes", &sr) < 0) {
//					unlock_ulslot(dom, i);
//					rpc->fault(ctx, 500, "Internal error creating Service Routes");
//					return;
//				}
//
//				for (j = 0; j < c->num_service_routes; j++) {
//					if (rpc->struct_add(sr, "S", "Route", &c->service_routes[j]) < 0) {
//						unlock_ulslot(dom, i);
//						rpc->fault(ctx, 500, "Internal error creating Service Route struct");
//						return;
//					}
//				}
//
//				if (rpc->struct_add(ah, "{", "Public Identities", &ih) < 0) {
//					unlock_ulslot(dom, i);
//					rpc->fault(ctx, 500, "Internal error creating IMPU struct");
//					return;
//				}
//
//				for (p = c->head; p; p = p->next) {
//					if (rpc->struct_add(ih, "S", "IMPU", &p->public_identity) < 0) {
//						unlock_ulslot(dom, i);
//						rpc->fault(ctx, 500, "Internal error creating IMPU struct");
//						return;
//					}
//				}
//			}
//			unlock_ulslot(dom, i);
		}
		if (rpc->struct_add(ah, "{", "Stats", &sh) > 0) {
			rpc->fault(ctx, 500, "Internal error creating stats");
		}
		if (rpc->struct_add(sh, "dd", "Records", n, "Max-Slots", max) < 0) {
			rpc->fault(ctx, 500, "Internal error creating stats struct");
		}
	}
}

rpc_export_t ul_rpc[] = {
	{"ulpcscf.status",   ul_rpc_dump,   ul_rpc_dump_doc,   0},
	{0, 0, 0, 0}
};


