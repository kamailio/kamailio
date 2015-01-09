/**
 * Copyright (C) 2014 Federico Cabiddu (federico.cabiddu@gmail.com)
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


#include <string.h>
#include <stdio.h>
#include "ts_hash.h"
#include "ts_rpc.h"


static const char* rpc_tsilo_dump_doc[2] = {
	"Dump tsilo transactions table",
	0
};


static const char* rpc_tsilo_lookup_doc[2] = {
	"Lookup one R-URI in the tsilo transactions table",
	0
};

/************************ helper functions ****************************/

/*!
 * \brief Add a node for a transaction
 */
static inline int rpc_dump_transaction(rpc_t* rpc, void* ctx, void *ih, ts_transaction_t* t)
{
	void* vh;

	if(rpc->struct_add(ih, "{", "Transaction", &vh)<0)
	{
		rpc->fault(ctx, 500, "Internal error creating transaction struct");
		return -1;
	}
	if(rpc->struct_add(vh, "d", "Tindex", t->tindex)<0) {
		rpc->fault(ctx, 500, "Internal error adding tindex");
		return -1;
	}

	if(rpc->struct_add(vh, "d", "Tlabel", t->tlabel)<0) {
		rpc->fault(ctx, 500, "Internal error adding tlabel");
		return -1;
	}
	return 0;
}


/*************************** RPC functions *****************************/


/*!
 * \brief Dump the content of the tsilo table
 * \param rpc RPC node that should be filled
 * \param c RPC void pointer
 */
static void rpc_tsilo_dump(rpc_t *rpc, void *c)
{
	ts_transaction_t* trans;
	struct ts_urecord* record;
	struct ts_entry* entry;

	str brief = {0, 0};

	int max, res, n, ntrans, i;
	int short_dump = 0;

	void* th;
	void* ah;
	void* ih;
	void* sh;

	rpc->scan(c, "*S", &brief);

	if(brief.len==5 && (strncmp(brief.s, "brief", 5)==0))
		/* short version */
		short_dump = 1;

	if (rpc->add(c, "{", &th) < 0)
	{
		rpc->fault(c, 500, "Internal error creating top rpc");
		return;
	}

	if (short_dump==0) {
		res = rpc->struct_add(th, "d{",
			"Size",		t_table->size,
			"R-URIs",	&ah);
	} else {
		res = rpc->struct_add(th, "d",
			"Size",		t_table->size);
	}
	if (res<0)
	{
		rpc->fault(c, 500, "Internal error creating inner struct");
		return;
	}

	/* add the entries per hash */
	for(i=0,n=0,max=0,ntrans=0; i<t_table->size; i++) {
		entry = &t_table->entries[i];
		lock_entry(entry);

		n += entry->n;
		if(max<entry->n)
			max= entry->n;
		for( record = entry->first ; record ; record=record->next ) {
			/* add entry */
			if(short_dump==0)
			{
				if(rpc->struct_add(ah, "Sd{",
					"R-URI", &record->ruri,
					"Hash", record->rurihash,
					"Transactions", &ih)<0)
				{
					unlock_entry(entry);
					rpc->fault(c, 500, "Internal error creating ruri struct");
					return;
				}
			}
			for( trans=record->transactions ; trans ; trans=trans->next) {
				ntrans += 1;
				if (short_dump==0) {
					if (rpc_dump_transaction(rpc, c, ih, trans) == -1) {
						unlock_entry(entry);
						return;
					}
				}
			}
		}
		unlock_entry(entry);
	}

	/* extra attributes node */
	if(rpc->struct_add(th, "{", "Stats",    &sh)<0)	{
		rpc->fault(c, 500, "Internal error creating stats struct");
		return;
	}
	if(rpc->struct_add(sh, "ddd",
		"RURIs", n,
		"Max-Slots", max,
		"Transactions", ntrans)<0)
	{
		rpc->fault(c, 500, "Internal error adding stats");
		return;
	}
}
/*!
 * \brief Show the transactions for a given R-URI
 * \param rpc RPC node that should be filled
 * \param c RPC void pointer
 */
static void rpc_tsilo_lookup(rpc_t *rpc, void *c)
{
	ts_transaction_t* trans;
	struct ts_urecord* record;

	str ruri = {0, 0};

	int res;

	void* th;

	if (rpc->scan(c, "S", &ruri) != 1) {
		rpc->fault(c, 500, "No RURI to lookup specified");
		return;
	}

	lock_entry_by_ruri(&ruri);

	/* look for urecord */
	res = get_ts_urecord(&ruri, &record);
	if (res) {
		unlock_entry_by_ruri(&ruri);
		rpc->fault(c, 404, "RURI not found in tsilo table");
		return;
	}

	if (rpc->add(c, "{", &th) < 0)
	{
		unlock_entry_by_ruri(&ruri);
		rpc->fault(c, 500, "Internal error creating top rpc");
		return;
	}

	/* add the transactions */
	for( trans=record->transactions ; trans ; trans=trans->next) {
		if (rpc_dump_transaction(rpc, c, th, trans) == -1) {
			unlock_entry_by_ruri(&ruri);
			return;
		}
	}
	unlock_entry_by_ruri(&ruri);
	return;
}

rpc_export_t rpc_methods[] = {
	{ "ts.dump",	rpc_tsilo_dump,		rpc_tsilo_dump_doc,		0 },
	{ "ts.lookup",	rpc_tsilo_lookup,	rpc_tsilo_lookup_doc,	0 },
    { 0, 0, 0, 0}
};

