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
 * 2007-03-06  syncronized state machine added for dialog state. New tranzition
 *             design based on events; removed num_1xx and num_2xx (bogdan)
 */

#include <stdlib.h>
#include <string.h>

#include "../../dprint.h"
#include "../../ut.h"
#include "../../hash_func.h"
#include "../../mi/mi.h"
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

	dlg->h_entry = core_hash( callid, from_tag->len?from_tag:0, d_table->size);
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


#define ref_dlg_unsafe(_dlg,_cnt)     \
	(_dlg)->ref += (_cnt)
#define unref_dlg_unsafe(_dlg,_cnt,_d_entry)   \
	do { \
		(_dlg)->ref -= (_cnt); \
		DBG("DBUG:dialog:unref_dlg: unref dlg %p with %d -> %d\n",\
			(_dlg),(_cnt),(_dlg)->ref);\
		if ((_dlg)->ref<=0) { \
			unlink_unsafe_dlg( _d_entry, _dlg);\
			destroy_dlg(_dlg);\
		}\
	}while(0)


void unref_dlg(struct dlg_cell *dlg, int cnt)
{
	struct dlg_entry *d_entry;

	d_entry = &(d_table->entries[dlg->h_entry]);

	dlg_lock( d_table, d_entry);
	unref_dlg_unsafe( dlg, cnt, d_entry);
	dlg_unlock( d_table, d_entry);
}


void next_state_dlg(struct dlg_cell *dlg, int event,
								int *old_state, int *new_state, int *unref)
{
	struct dlg_entry *d_entry;

	d_entry = &(d_table->entries[dlg->h_entry]);


	dlg_lock( d_table, d_entry);

	*old_state = dlg->state;

	switch (event) {
		case DLG_EVENT_TDEL:
			switch (dlg->state) {
				case DLG_STATE_UNCONFIRMED:
				case DLG_STATE_EARLY:
					dlg->state = DLG_STATE_DELETED;
					unref_dlg_unsafe(dlg,2,d_entry);
					break;
				case DLG_STATE_CONFIRMED_NA:
				case DLG_STATE_CONFIRMED:
				case DLG_STATE_DELETED:
					unref_dlg_unsafe(dlg,1,d_entry);
					break;
				default:
					LOG(L_CRIT,"BUG:next_state_dlg: bogus event %d in "
						"state %d\n",event,dlg->state);
			}
			break;
		case DLG_EVENT_RPL1xx:
			switch (dlg->state) {
				case DLG_STATE_UNCONFIRMED:
				case DLG_STATE_EARLY:
					dlg->state = DLG_STATE_EARLY;
					break;
				default:
					LOG(L_CRIT,"BUG:next_state_dlg: bogus event %d in "
						"state %d\n",event,dlg->state);
			}
			break;
		case DLG_EVENT_RPL3xx:
			switch (dlg->state) {
				case DLG_STATE_UNCONFIRMED:
				case DLG_STATE_EARLY:
					dlg->state = DLG_STATE_DELETED;
					*unref = 1;
					break;
				default:
					LOG(L_CRIT,"BUG:next_state_dlg: bogus event %d in "
						"state %d\n",event,dlg->state);
			}
			break;
		case DLG_EVENT_RPL2xx:
			switch (dlg->state) {
				case DLG_STATE_DELETED:
					ref_dlg_unsafe(dlg,1);
				case DLG_STATE_UNCONFIRMED:
				case DLG_STATE_EARLY:
					dlg->state = DLG_STATE_CONFIRMED_NA;
					break;
				case DLG_STATE_CONFIRMED_NA:
				case DLG_STATE_CONFIRMED:
					break;
				default:
					LOG(L_CRIT,"BUG:next_state_dlg: bogus event %d in "
						"state %d\n",event,dlg->state);
			}
			break;
		case DLG_EVENT_REQACK:
			switch (dlg->state) {
				case DLG_STATE_CONFIRMED_NA:
					dlg->state = DLG_STATE_CONFIRMED;
					break;
				case DLG_STATE_CONFIRMED:
					break;
				default:
					LOG(L_CRIT,"BUG:next_state_dlg: bogus event %d in "
						"state %d\n",event,dlg->state);
			}
			break;
		case DLG_EVENT_REQBYE:
			switch (dlg->state) {
				case DLG_STATE_CONFIRMED_NA:
				case DLG_STATE_CONFIRMED:
					dlg->state = DLG_STATE_DELETED;
					*unref = 1;
					break;
				default:
					LOG(L_CRIT,"BUG:next_state_dlg: bogus event %d in "
						"state %d\n",event,dlg->state);
			}
			break;
		case DLG_EVENT_REQ:
			switch (dlg->state) {
				case DLG_STATE_CONFIRMED_NA:
				case DLG_STATE_CONFIRMED:
					break;
				default:
					LOG(L_CRIT,"BUG:next_state_dlg: bogus event %d in "
						"state %d\n",event,dlg->state);
			}
			break;
		default:
			LOG(L_CRIT,"BUG:next_state_dlg: unknown event %d\n",
				event);
	}
	*new_state = dlg->state;

	dlg_unlock( d_table, d_entry);

	DBG("DEBUG:dialog:next_state_dlg: dialog %p changed from state %d to "
		"state %d, due event %d\n",dlg,*old_state,*new_state,event);
}




struct mi_root * mi_print_dlgs(struct mi_root *cmd_tree, void *param )
{
	struct dlg_cell *dlg;
	struct mi_node* rpl = NULL, *node= NULL;
	struct mi_attr* attr= NULL;
	struct mi_root* rpl_tree= NULL;
	int i, len;
	char* p;

	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	if (rpl_tree==0)
		return 0;
	rpl = &rpl_tree->node;

	for( i=0 ; i<d_table->size ; i++ ) {
		dlg_lock( d_table, &(d_table->entries[i]) );

		for( dlg=d_table->entries[i].first ; dlg ; dlg=dlg->next ) {
			node = add_mi_node_child(rpl, 0, "dialog",6 , 0, 0 );
			if (node==0)
				goto error;

			attr = addf_mi_attr( node, 0, "hash", 4, "%u:%u",
					dlg->h_entry, dlg->h_id );
			if (attr==0)
				goto error;

			p= int2str((unsigned long)dlg->state, &len);
			attr = add_mi_attr( node, MI_DUP_VALUE, "state", 5, p, len);
			if (attr==0)
				goto error;

			p= int2str((unsigned long)dlg->lifetime, &len);
			attr = add_mi_attr( node, MI_DUP_VALUE, "timeout", 7, p, len);
			if (attr==0)
				goto error;

			attr = add_mi_attr(node, MI_DUP_VALUE, "callid", 6,
					dlg->callid.s, dlg->callid.len);
			if(attr == 0)
				goto error;

			attr = add_mi_attr(node, MI_DUP_VALUE, "from_uri", 8,
					dlg->from_uri.s, dlg->from_uri.len);
			if(attr == 0)
				goto error;

			attr = add_mi_attr(node, MI_DUP_VALUE, "from_tag", 8,
					dlg->from_tag.s, dlg->from_tag.len);
			if(attr == 0)
				goto error;
	
			attr = add_mi_attr(node, MI_DUP_VALUE, "to_uri", 6,
					dlg->to_uri.s, dlg->to_uri.len);
			if(attr == 0)
				goto error;

			attr = add_mi_attr(node, MI_DUP_VALUE, "to_tag", 6,
					dlg->to_tag.s, dlg->to_tag.len);
			if(attr == 0)
				goto error;

		}
		dlg_unlock( d_table, &(d_table->entries[i]) );
	}
	return rpl_tree;

error:
	LOG(L_ERR,"ERROR:mi_ps: failed to add node\n");
	free_mi_tree(rpl_tree);
	return 0;

}
