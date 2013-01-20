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
#include "ul_mod.h"
#include "utime.h"

static const char* ul_rpc_dump_doc[2] = {
	"Dump user location tables",
	0
};


int rpc_dump_contact(rpc_t* rpc, void* ctx, void *ih, ucontact_t* c)
{
	void* vh;
	str empty_str = {"[not set]", 9};
	str state_str = {"[not set]", 9};
	str socket_str = {"[not set]", 9};
	time_t t;

	t = time(0);
	if(rpc->struct_add(ih, "{", "Contact", &vh)<0)
	{
		rpc->fault(ctx, 500, "Internal error creating contact struct");
		return -1;
	}
	if(rpc->struct_add(vh, "S", "Address", &c->c)<0)
	{
		rpc->fault(ctx, 500, "Internal error adding addr");
		return -1;
	}
	if (c->expires == 0) { if(rpc->struct_add(vh, "s", "Expires", "permanent")<0)
	{
			rpc->fault(ctx, 500, "Internal error adding expire");
			return -1;
	}
	} else if (c->expires == UL_EXPIRED_TIME) {
		if(rpc->struct_add(vh, "s", "Expires", "deleted")<0)
		{
			rpc->fault(ctx, 500, "Internal error adding expire");
			return -1;
		}
	} else if (t > c->expires) {
		if(rpc->struct_add(vh, "s", "Expires", "expired")<0)
		{
			rpc->fault(ctx, 500, "Internal error adding expire");
			return -1;
		}
	} else {
		if(rpc->struct_add(vh, "d", "Expires", (int)(c->expires - t))<0)
		{
			rpc->fault(ctx, 500, "Internal error adding expire");
			return -1;
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
	if(rpc->struct_add(vh, "f", "Q", c->q)<0)
	{
		rpc->fault(ctx, 500, "Internal error adding q");
		return -1;
	}
	if(rpc->struct_add(vh, "S", "Call-ID", &c->callid)<0)
	{
		rpc->fault(ctx, 500, "Internal error adding callid");
		return -1;
	}
	if(rpc->struct_add(vh, "d", "CSeq", c->cseq)<0)
	{
		rpc->fault(ctx, 500, "Internal error adding cseq");
		return -1;
	}
	if(rpc->struct_add(vh, "S", "User-Agent",
			(c->user_agent.len)?&c->user_agent: &empty_str)<0)
	{
		rpc->fault(ctx, 500, "Internal error adding user-agent");
		return -1;
	}
	if(rpc->struct_add(vh, "S", "Received",
			(c->received.len)?&c->received: &empty_str)<0)
	{
		rpc->fault(ctx, 500, "Internal error adding received");
		return -1;
	}
	if(rpc->struct_add(vh, "S", "Path",
			(c->path.len)?&c->path: &empty_str)<0)
	{
		rpc->fault(ctx, 500, "Internal error adding path");
		return -1;
	}
	if(rpc->struct_add(vh, "S", "State", &state_str)<0)
	{
		rpc->fault(ctx, 500, "Internal error adding state");
		return -1;
	}
	if(rpc->struct_add(vh, "d", "Flags", c->flags)<0)
	{
		rpc->fault(ctx, 500, "Internal error adding flags");
		return -1;
	}
	if(rpc->struct_add(vh, "d", "CFlags", c->cflags)<0)
	{
		rpc->fault(ctx, 500, "Internal error adding cflags");
		return -1;
	}
	if(rpc->struct_add(vh, "S", "Socket", &socket_str)<0)
	{
		rpc->fault(ctx, 500, "Internal error adding socket");
		return -1;
	}
	if(rpc->struct_add(vh, "d", "Methods", c->methods)<0)
	{
		rpc->fault(ctx, 500, "Internal error adding methods");
		return -1;
	}
	if(rpc->struct_add(vh, "S", "Ruid", (c->ruid.len)?&c->ruid: &empty_str)<0)
	{
		rpc->fault(ctx, 500, "Internal error adding ruid");
		return -1;
	}
	if(rpc->struct_add(vh, "S", "Instance",
			(c->instance.len)?&c->instance: &empty_str)<0)
	{
		rpc->fault(ctx, 500, "Internal error adding instance");
		return -1;
	}
	if(rpc->struct_add(vh, "d", "Reg-Id", c->reg_id)<0)
	{
		rpc->fault(ctx, 500, "Internal error adding reg_id");
		return -1;
	}
	if(rpc->struct_add(vh, "d", "Last-Keepalive", (int)c->last_keepalive)<0)
	{
		rpc->fault(ctx, 500, "Internal error adding reg_id");
		return -1;
	}
	if(rpc->struct_add(vh, "d", "Last-Modified", (int)c->last_modified)<0)
	{
		rpc->fault(ctx, 500, "Internal error adding reg_id");
		return -1;
	}
	return 0;
}

static void ul_rpc_dump(rpc_t* rpc, void* ctx)
{
	struct urecord* r;
	dlist_t* dl;
	udomain_t* dom;
	str brief = {0, 0};
	int summary = 0;
	ucontact_t* c;
	void* th;
	void* ah;
	void* ih;
	void* sh;
	int max, n, i;

	rpc->scan(ctx, "*S", &brief);

	if(brief.len==5 && (strncmp(brief.s, "brief", 5)==0))
		summary = 1;
	
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
						if (rpc_dump_contact(rpc, ctx, ih, c) == -1) {
							unlock_ulslot(dom, i);
							return;
						}
					}
				}
			}

			unlock_ulslot( dom, i);
		}

		/* extra attributes node */
		if(rpc->struct_add(th, "{", "Stats",    &sh)<0)
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

static const char* ul_rpc_lookup_doc[2] = {
	"Lookup one AOR in the usrloc location table",
	0
};

/*!
 * \brief Search a domain in the global domain list
 * \param table domain (table) name
 * \return pointer to domain if found, 0 if not found
 */
static inline udomain_t* rpc_find_domain(str* table)
{
	dlist_t* dom;

	for( dom=root ; dom ; dom=dom->next ) {
		if ((dom->name.len == table->len) &&
		!memcmp(dom->name.s, table->s, table->len))
			return dom->d;
	}
	return 0;
}

/*!
 * \brief Convert address of record
 *
 * Convert an address of record string to lower case, and truncate
 * it when use_domain is not set.
 * \param aor address of record
 * \return 0 on success, -1 on error
 */
static inline int rpc_fix_aor(str *aor)
{
	char *p;

	p = memchr( aor->s, '@', aor->len);
	if (use_domain) {
		if (p==NULL)
			return -1;
	} else {
		if (p)
			aor->len = p - aor->s;
	}
	strlower(aor);

	return 0;
}

/*!
 * \brief Dumps the contacts of an AOR
 * \param rpc	Handle to RPC structure
 * \param ctx not used
 * \note expects 2 arguments: the table name and the AOR
 */
static void ul_rpc_lookup(rpc_t* rpc, void* ctx)
{
	udomain_t* dom;
	str table = {0, 0};
	str aor = {0, 0};
	void* th;
	urecord_t *rec;
	ucontact_t* con;
	int ret;
	int rpl_tree;

	if (rpc->scan(ctx, "S", &table) != 1) {
		rpc->fault(ctx, 500, "Not enough parameters (table and AOR to lookup)");
		return;
	}
	if (rpc->scan(ctx, "S", &aor) != 1) {
		rpc->fault(ctx, 500, "Not enough parameters (table and AOR to lookup)");
		return;
	}

	/* look for table */
	dom = rpc_find_domain( &table );
	if (dom == NULL) {
		rpc->fault(ctx, 500, "Domain not found");
		return;
	}

	/* process the aor */
	if ( rpc_fix_aor(&aor) != 0 ) {
		rpc->fault(ctx, 500, "Domain missing in AOR");
		return;
	}

	lock_udomain( dom, &aor);

	ret = get_urecord( dom, &aor, &rec);
	if (ret == 1) {
		unlock_udomain( dom, &aor);
		rpc->fault(ctx, 500, "AOR not found in location table");
		return;
	}

	get_act_time();
	rpl_tree = 0;

	if (rpc->add(ctx, "{", &th) < 0)
	{
		rpc->fault(ctx, 500, "Internal error creating outer rpc");
		return;
	}

	/* We have contacts, list them */
	for( con=rec->contacts ; con ; con=con->next) {
		if (VALID_CONTACT( con, act_time)) {
			rpl_tree++;
			if (rpc_dump_contact(rpc, ctx, th, con) == -1) {
				unlock_udomain(dom, &aor);
				return;
			}
		}
	}

	unlock_udomain( dom, &aor);

	if (rpl_tree==0) {
		rpc->fault(ctx, 500, "AOR has no contacts");
		return;
	}
	return;
}

rpc_export_t ul_rpc[] = {
	{"ul.dump",   ul_rpc_dump,   ul_rpc_dump_doc,   0},
	{"ul.lookup",   ul_rpc_lookup,   ul_rpc_lookup_doc,   0},
	{0, 0, 0, 0}
};


