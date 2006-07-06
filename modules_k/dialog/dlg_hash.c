/*
 * $Id$
 *
 * Copyright (C) 2006 Voice System SRL
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 * 2006-04-14  initial version (bogdan)
 */

#include <stdlib.h>
#include <string.h>

#include "../../dprint.h"
#include "../../ut.h"
#include "../../hash_func.h"
#include "../../fifo_server.h"
#include "dlg_hash.h"


#define MAX_LDG_LOCKS  2048
#define MIN_LDG_LOCKS  2


static struct dlg_table *d_table = 0;

#define dlg_lock(_table, _entry) \
		lock_set_get( (_table)->locks, (_entry)->lock_idx);
#define dlg_unlock(_table, _entry) \
		lock_set_release( (_table)->locks, (_entry)->lock_idx);


int init_dlg_table(unsigned int size)
{
	int n;
	int i;

	d_table = (struct dlg_table*)shm_malloc
		( sizeof(struct dlg_table) + size*sizeof(struct dlg_entry));
	if (d_table==0) {
		LOG(L_ERR, "ERROR:dialog:init_dlg_table: no more shm mem (1)\n");
		goto error0;
	}

	memset( d_table, 0, sizeof(struct dlg_table) );
	d_table->size = size;
	d_table->entries = (struct dlg_entry*)(d_table+1);

	n = (size<MAX_LDG_LOCKS)?size:MAX_LDG_LOCKS;
	for(  ; n>=MIN_LDG_LOCKS ; n-- ) {
		d_table->locks = lock_set_alloc(n);
		if (d_table->locks==0)
			continue;
		if (lock_set_init(d_table->locks)==0) {
			lock_set_dealloc(d_table->locks);
			d_table->locks = 0;
			continue;
		}
		d_table->locks_no = n;
		break;
	}

	if (d_table->locks==0) {
		LOG(L_ERR,"ERROR:dialog:init_dlg_table: unable to allocted at least "
			"%d locks for the hash table\n",MIN_LDG_LOCKS);
		goto error1;
	}

	for( i=0 ; i<size; i++ ) {
		memset( &(d_table->entries[i]), 0, sizeof(struct dlg_entry) );
		d_table->entries[i].next_id = rand();
		d_table->entries[i].lock_idx = i % d_table->locks_no;
	}

	return 0;
error1:
	shm_free( d_table );
error0:
	return -1;
}



static inline void destroy_dlg(struct dlg_cell *dlg)
{
	DBG("DBUG:dialog:destroy_dlg: destroing dialog %p\n",dlg);
	if (dlg->to_tag.s && dlg->to_tag.len)
		shm_free(dlg->to_tag.s);
	shm_free(dlg);
}



void destroy_dlg_table()
{
	struct dlg_cell *dlg, *l_dlg;
	int i;

	if (d_table==0)
		return;

	if (d_table->locks) {
		lock_set_destroy(d_table->locks);
		lock_set_dealloc(d_table->locks);
	}

	for( i=0 ; i<d_table->size; i++ ) {
		dlg = d_table->entries[i].first;
		while (dlg) {
			l_dlg = dlg;
			dlg = dlg->next;
			destroy_dlg(l_dlg);
		}

	}

	shm_free(d_table);
	d_table = 0;

	return;
}



struct dlg_cell* build_new_dlg( str *callid, str *from_uri, str *to_uri,
												str *from_tag)
{
	struct dlg_cell *dlg;
	int len;
	char *p;

	len = sizeof(struct dlg_cell) + callid->len + from_uri->len +
		to_uri->len + from_tag->len;
	dlg = (struct dlg_cell*)shm_malloc( len );
	if (dlg==0) {
		LOG(L_ERR,"ERROR:dialog:build_new_dlg: no more shm mem (%d)\n",len);
		return 0;
	}

	memset( dlg, 0, len);
	dlg->state = DLG_STATE_UNCONFIRMED;

	dlg->h_entry = core_hash( from_tag, callid, d_table->size);
	DBG("DEBUG:dialog:build_new_dlg: new dialog on hash %u\n",dlg->h_entry);

	p = (char*)(dlg+1);

	dlg->callid.s = p;
	dlg->callid.len = callid->len;
	memcpy( p, callid->s, callid->len);
	p += callid->len;

	dlg->from_uri.s = p;
	dlg->from_uri.len = from_uri->len;
	memcpy( p, from_uri->s, from_uri->len);
	p += from_uri->len;

	dlg->to_uri.s = p;
	dlg->to_uri.len = to_uri->len;
	memcpy( p, to_uri->s, to_uri->len);
	p += to_uri->len;

	dlg->from_tag.s = p;
	dlg->from_tag.len = from_tag->len;
	memcpy( p, from_tag->s, from_tag->len);
	p += from_tag->len;

	if ( p!=(((char*)dlg)+len) ) {
		LOG(L_CRIT,"BUG:dialog:build_new_dlg: buffer overflow\n");
		shm_free(dlg);
		return 0;
	}

	return dlg;
}



int dlg_set_totag(struct dlg_cell *dlg, str *tag)
{
	dlg->to_tag.s = (char*)shm_malloc( tag->len );
	if (dlg->to_tag.s==0) {
		LOG(L_ERR,"ERROR:dialog:dlg_set_totag: no more shm mem (%d)\n",
				tag->len);
		return -1;
	}
	memcpy( dlg->to_tag.s, tag->s, tag->len);
	dlg->to_tag.len = tag->len;
	return 0;
}



struct dlg_cell* lookup_dlg( unsigned int h_entry, unsigned int h_id)
{
	struct dlg_cell *dlg;
	struct dlg_entry *d_entry;

	if (h_entry>=d_table->size)
		goto not_found;

	d_entry = &(d_table->entries[h_entry]);

	dlg_lock( d_table, d_entry);

	for( dlg=d_entry->first ; dlg ; dlg=dlg->next ) {
		if (dlg->h_id == h_id) {
			if (dlg->state==DLG_STATE_DELETED) {
				dlg_unlock( d_table, d_entry);
				goto not_found;
			}
			dlg->ref++;
			dlg_unlock( d_table, d_entry);
			DBG("DEBUG:dialog:lookup_dlg: dialog id=%u found on entry %u\n",
				h_id, h_entry);
			return dlg;
		}
	}

	dlg_unlock( d_table, d_entry);
not_found:
	DBG("DEBUG:dialog:lookup_dlg: no dialog id=%u found on entry %u\n",
		h_id, h_entry);
	return 0;
}



void link_dlg(struct dlg_cell *dlg, int n)
{
	struct dlg_entry *d_entry;

	d_entry = &(d_table->entries[dlg->h_entry]);

	dlg_lock( d_table, d_entry);

	dlg->h_id = d_entry->next_id++;
	if (d_entry->first==0) {
		d_entry->first = d_entry->last = dlg;
	} else {
		d_entry->last->next = dlg;
		dlg->prev = d_entry->last;
		d_entry->last = dlg;
	}

	dlg->ref += 1 + n;

	dlg_unlock( d_table, d_entry);
	return;
}



static inline void unlink_unsafe_dlg(struct dlg_entry *d_entry,
													struct dlg_cell *dlg)
{
	if (dlg->next)
		dlg->next->prev = dlg->prev;
	else
		d_entry->last = dlg->prev;
	if (dlg->prev)
		dlg->prev->next = dlg->next;
	else
		d_entry->first = dlg->next;

	dlg->next = dlg->prev = 0;

	return;
}



void ref_dlg(struct dlg_cell *dlg)
{
	struct dlg_entry *d_entry;

	d_entry = &(d_table->entries[dlg->h_entry]);

	dlg_lock( d_table, d_entry);

	dlg->ref++;

	dlg_unlock( d_table, d_entry);
}



void unref_dlg(struct dlg_cell *dlg, int n, int delete)
{
	struct dlg_entry *d_entry;

	d_entry = &(d_table->entries[dlg->h_entry]);


	dlg_lock( d_table, d_entry);

	dlg->ref -= n;
	
	DBG("DBUG:dialog:unref_dlg: unref dlg %p with %d (delete=%d)-> %d\n",
		dlg,n,delete,dlg->ref);

	if (delete)
		dlg->state = DLG_STATE_DELETED;

	if (dlg->ref<=0) {
		unlink_unsafe_dlg( d_entry, dlg);
		destroy_dlg(dlg);
	}

	dlg_unlock( d_table, d_entry);
}


int fifo_print_dlgs(FILE *fifo, char *response_file )
{
	struct dlg_cell *dlg;
	FILE *rpl;
	int i;

	rpl = open_reply_pipe( response_file );
	if (rpl==0) {
		LOG(L_ERR,"ERROR:dialog:fifo_print_dlgs: failed to open reply fifo\n");
		return -1;
	}

	fprintf(rpl,"200 OK\n");
	for( i=0 ; i<d_table->size ; i++ ) {
		dlg_lock( d_table, &(d_table->entries[i]) );
		for( dlg=d_table->entries[i].first ; dlg ; dlg=dlg->next ) {
			fprintf(rpl, "hash=%u, label=%u, ptr=%p\n"
					"\tstate=%d, timeout=%d\n"
					"\tcallid='%.*s'\n"
					"\tfrom uri='%.*s', tag='%.*s'\n"
					"\tto uri='%.*s', tag='%.*s'\n",
					dlg->h_entry, dlg->h_id, dlg,
					dlg->state, dlg->tl.timeout,
					dlg->callid.len, dlg->callid.s,
					dlg->from_uri.len, dlg->from_uri.s,
					dlg->from_tag.len, dlg->from_tag.s,
					dlg->to_uri.len, dlg->to_uri.s,
					dlg->to_tag.len, ZSW(dlg->to_tag.s) );
		}
		dlg_unlock( d_table, &(d_table->entries[i]) );
	}

	fclose(rpl);
	return 0;
}
