/*
 * $Id$
 *
 * usrloc module
 *
 * Copyright (C) 2009 Daniel-Constantin Mierla (asipto.com).
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "../../ip_addr.h"
#include "../../dprint.h"

#include "ul_rpc.h"
#include "dlist.h"
#include "ucontact.h"
#include "udomain.h"

static const char* ul_rpc_dump_doc[2] = {
	"Dump user location tables",
	0
};

static void ul_rpc_dump(rpc_t* rpc, void* ctx)
{
	struct urecord* r;
	dlist_t* dl;
	udomain_t* dom;
	time_t t;
	str brief = {0, 0};
	str empty_str = {"[not set]", 9};
	str state_str = {"[not set]", 9};
	str socket_str = {"[not set]", 9};
	int summary = 0;
	ucontact_t* c;
	void* th;
	void* ah;
	void* ih;
	void* vh;
	void* sh;
	int max, n, i;

	rpc->scan(ctx, "*S", &brief);

	if(brief.len==5 && (strncmp(brief.s, "brief", 5)==0))
		summary = 1;
	
	t = time(0);
	for( dl=root ; dl ; dl=dl->next ) {
		dom = dl->d;
		if (rpc->add(ctx, "{", &th) < 0)
		{
			rpc->fault(ctx, 500, "Internal error creating top rpc");
			return;
		}
		if(rpc->struct_add(th, "Sd{",
					"Domain",  &dl->name,
					"Size",    (int)dom->size,
					"AoRs",    &ah)<0)
		{
			rpc->fault(ctx, 500, "Internal error creating inner struct");
			return;
		}
		for(i=0,n=0,max=0; i<dom->size; i++) {
			lock_ulslot( dom, i);
			n += dom->table[i].n;
			if(max<dom->table[i].n)
				max= dom->table[i].n;
			for( r = dom->table[i].first ; r ; r=r->next ) {
				if(summary==1)
				{
					if(rpc->struct_add(ah, "S",
							"AoR", &r->aor)<0)
					{
						unlock_ulslot( dom, i);
						rpc->fault(ctx, 500, "Internal error creating aor struct");
						return;
					}
				} else {
					if(rpc->struct_add(ah, "Sd{",
							"AoR", &r->aor,
							"HashID", r->aorhash,
							"Contacts", &ih)<0)
					{
						unlock_ulslot( dom, i);
						rpc->fault(ctx, 500, "Internal error creating aor struct");
						return;
					}
					for( c=r->contacts ; c ; c=c->next)
					{
						if(rpc->struct_add(ih, "{",
							"Contact", &vh)<0)
						{
							unlock_ulslot( dom, i);
							rpc->fault(ctx, 500, "Internal error creating contact struct");
							return;
						}
						if(rpc->struct_add(vh, "S",
									"Address", &c->c)<0)
						{
							unlock_ulslot( dom, i);
							rpc->fault(ctx, 500,
									"Internal error adding addr");
								return;
						}
						if (c->expires == 0) {
							if(rpc->struct_add(vh, "s",
									"Expires", "permanent")<0)
							{
								unlock_ulslot( dom, i);
								rpc->fault(ctx, 500,
										"Internal error adding expire");
								return;
							}
						} else if (c->expires == UL_EXPIRED_TIME) {
							if(rpc->struct_add(vh, "s",
									"Expires", "deleted")<0)
							{
								unlock_ulslot( dom, i);
								rpc->fault(ctx, 500,
										"Internal error adding expire");
								return;
							}
						} else if (t > c->expires) {
							if(rpc->struct_add(vh, "s",
									"Expires", "expired")<0)
							{
								unlock_ulslot( dom, i);
								rpc->fault(ctx, 500,
										"Internal error adding expire");
								return;
							}
						} else {
							if(rpc->struct_add(vh, "d",
									"Expires", (int)(c->expires - t))<0)
							{
								unlock_ulslot( dom, i);
								rpc->fault(ctx, 500,
										"Internal error adding expire");
								return;
							}
						}
						if (c->state == CS_NEW) {
							state_str.s = "CS_NEW";
							state_str.len = 6;
						} else if (c->state == CS_SYNC) {
							state_str.s = "CS_SYNC";
							state_str.len = 7;
						} else if (c->state== CS_DIRTY) {
							state_str.s = "CS_DIRTY";
							state_str.len = 8;
						} else {
							state_str.s = "CS_UNKNOWN";
							state_str.len = 10;
						}
						if(c->sock)
						{
							socket_str.s = c->sock->sock_str.s;
							socket_str.len = c->sock->sock_str.len;
						}
						if(rpc->struct_add(vh, "f",
									"Q", c->q)<0)
						{
							unlock_ulslot( dom, i);
							rpc->fault(ctx, 500,
									"Internal error adding q");
							return;
						}
						if(rpc->struct_add(vh, "S",
									"Call-ID", &c->callid)<0)
						{
							unlock_ulslot( dom, i);
							rpc->fault(ctx, 500,
									"Internal error adding callid");
							return;
						}
						if(rpc->struct_add(vh, "d",
									"CSeq", c->cseq)<0)
						{
							unlock_ulslot( dom, i);
							rpc->fault(ctx, 500,
									"Internal error adding cseq");
							return;
						}
						if(rpc->struct_add(vh, "S",
									"User-Agent",
										(c->user_agent.len)?&c->user_agent:
												&empty_str)<0)
						{
							unlock_ulslot( dom, i);
							rpc->fault(ctx, 500,
									"Internal error adding user-agent");
							return;
						}
						if(rpc->struct_add(vh, "S",
									"Received",
										(c->received.len)?&c->received:
												&empty_str)<0)
						{
							unlock_ulslot( dom, i);
							rpc->fault(ctx, 500,
									"Internal error adding received");
							return;
						}
						if(rpc->struct_add(vh, "S",
									"Path",
										(c->path.len)?&c->path:
												&empty_str)<0)
						{
							unlock_ulslot( dom, i);
							rpc->fault(ctx, 500,
									"Internal error adding path");
							return;
						}
						if(rpc->struct_add(vh, "S",
									"State", &state_str)<0)
						{
							unlock_ulslot( dom, i);
							rpc->fault(ctx, 500,
									"Internal error adding state");
							return;
						}
						if(rpc->struct_add(vh, "d",
									"Flags", c->flags)<0)
						{
							unlock_ulslot( dom, i);
							rpc->fault(ctx, 500,
									"Internal error adding flags");
							return;
						}
						if(rpc->struct_add(vh, "d",
									"CFlags", c->cflags)<0)
						{
							unlock_ulslot( dom, i);
							rpc->fault(ctx, 500,
									"Internal error adding cflags");
							return;
						}
						if(rpc->struct_add(vh, "S",
									"Socket", &socket_str)<0)
						{
							unlock_ulslot( dom, i);
							rpc->fault(ctx, 500,
									"Internal error adding socket");
							return;
						}
						if(rpc->struct_add(vh, "d",
									"Methods", c->methods)<0)
						{
							unlock_ulslot( dom, i);
							rpc->fault(ctx, 500,
									"Internal error adding methods");
							return;
						}
						if(rpc->struct_add(vh, "S",
								"Ruid", (c->ruid.len)?&c->ruid: &empty_str)<0)
						{
							unlock_ulslot( dom, i);
							rpc->fault(ctx, 500,
									"Internal error adding ruid");
							return;
						}
						if(rpc->struct_add(vh, "S",
								"Instance",
								(c->instance.len)?&c->instance: &empty_str)<0)
						{
							unlock_ulslot( dom, i);
							rpc->fault(ctx, 500,
									"Internal error adding instance");
							return;
						}
						if(rpc->struct_add(vh, "d",
									"Reg-Id", c->reg_id)<0)
						{
							unlock_ulslot( dom, i);
							rpc->fault(ctx, 500,
									"Internal error adding reg_id");
							return;
						}
						if(rpc->struct_add(vh, "d",
									"Last-Keepalive", (int)c->last_keepalive)<0)
						{
							unlock_ulslot( dom, i);
							rpc->fault(ctx, 500,
									"Internal error adding reg_id");
							return;
						}
						if(rpc->struct_add(vh, "d",
									"Last-Modified", (int)c->last_modified)<0)
						{
							unlock_ulslot( dom, i);
							rpc->fault(ctx, 500,
									"Internal error adding reg_id");
							return;
						}

					}
				}
			}

			unlock_ulslot( dom, i);
		}

		/* extra attributes node */
		if(rpc->struct_add(th, "{",
					"Stats",    &sh)<0)
		{
			rpc->fault(ctx, 500, "Internal error creating stats struct");
			return;
		}
		if(rpc->struct_add(sh, "dd",
				"Records", n,
				"Max-Slots", max)<0)
		{
			rpc->fault(ctx, 500, "Internal error adding stats");
			return;
		}
	}
}

rpc_export_t ul_rpc[] = {
	{"ul.dump",   ul_rpc_dump,   ul_rpc_dump_doc,   0},
	{0, 0, 0, 0}
};


