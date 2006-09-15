/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 *
 * History
 * -------
 * 2003-03-06  200/INV to-tag list deallocation added;
 *             setting "kill_reason" moved in here -- it is moved
 *             from transaction state to a static var(jiri)
 * 2003-03-16  removed _TOTAG (jiri)
 * 2003-03-30  set_kr for requests only (jiri)
 * 2003-04-04  bug_fix: REQ_IN callback not called for local 
 *             UAC transactions (jiri)
 * 2003-09-12  timer_link->tg will be set only if EXTRA_DEBUG (andrei)
 * 2003-12-04  global callbacks replaceed with callbacks per transaction;
 *             completion callback merged into them as LOCAL_COMPETED (bogdan)
 * 2004-02-11  FIFO/CANCEL + alignments (hash=f(callid,cseq)) (uli+jiri)
 * 2004-02-13  t->is_invite and t->local replaced with flags;
 *             timer_link.payload removed (bogdan)
 * 2004-08-23  avp support added - move and remove avp list to/from
 *             transactions (bogdan)
 * 2006-08-11  dns failover support (andrei)
 */

#include <stdlib.h>


#include "../../mem/shm_mem.h"
#include "../../hash_func.h"
#include "../../dprint.h"
#include "../../md5utils.h"
#include "../../ut.h"
#include "../../globals.h"
#include "../../error.h"
#include "defs.h"
#include "t_reply.h"
#include "t_cancel.h"
#include "t_stats.h"
#include "h_table.h"
#include "fix_lumps.h" /* free_via_clen_lump */
#include "timer.h"

static enum kill_reason kr;

/* pointer to the big table where all the transaction data
   lives */
static struct s_table*  tm_table;



void set_kr( enum kill_reason _kr )
{
	kr|=_kr;
}


enum kill_reason get_kr() {
	return kr;
}


void lock_hash(int i) 
{
	lock(&tm_table->entrys[i].mutex);
}


void unlock_hash(int i) 
{
	unlock(&tm_table->entrys[i].mutex);
}


struct s_table* get_tm_table()
{
	return tm_table;
}


unsigned int transaction_count( void )
{
	unsigned int i;
	unsigned int count;

	count=0;	
	for (i=0; i<TABLE_ENTRIES; i++) 
		count+=tm_table->entrys[i].cur_entries;
	return count;
}



void free_cell( struct cell* dead_cell )
{
	char *b;
	int i;
	struct sip_msg *rpl;
	struct totag_elem *tt, *foo;
	struct tm_callback *cbs, *cbs_tmp;

	release_cell_lock( dead_cell );
	shm_lock();

	/* UA Server */
	if ( dead_cell->uas.request )
		sip_msg_free_unsafe( dead_cell->uas.request );
	if ( dead_cell->uas.response.buffer )
		shm_free_unsafe( dead_cell->uas.response.buffer );

	/* callbacks */
	for( cbs=dead_cell->tmcb_hl.first ; cbs ; ) {
		cbs_tmp = cbs;
		cbs = cbs->next;
		shm_free_unsafe( cbs_tmp );
	}

	/* UA Clients */
	for ( i =0 ; i<dead_cell->nr_of_outgoings;  i++ )
	{
		/* retransmission buffer */
		if ( (b=dead_cell->uac[i].request.buffer) )
			shm_free_unsafe( b );
		b=dead_cell->uac[i].local_cancel.buffer;
		if (b!=0 && b!=BUSY_BUFFER)
			shm_free_unsafe( b );
		rpl=dead_cell->uac[i].reply;
		if (rpl && rpl!=FAKED_REPLY && rpl->msg_flags&FL_SHM_CLONE) {
			sip_msg_free_unsafe( rpl );
		}
#ifdef USE_DNS_FAILOVER
		if (dead_cell->uac[i].dns_h.a){
			DBG("branch %d -> dns_h.srv (%.*s) ref=%d,"
							" dns_h.a (%.*s) ref=%d\n", i,
					dead_cell->uac[i].dns_h.srv?
								dead_cell->uac[i].dns_h.srv->name_len:0,
					dead_cell->uac[i].dns_h.srv?
								dead_cell->uac[i].dns_h.srv->name:"",
					dead_cell->uac[i].dns_h.srv?
								dead_cell->uac[i].dns_h.srv->refcnt.val:0,
					dead_cell->uac[i].dns_h.a->name_len,
					dead_cell->uac[i].dns_h.a->name,
					dead_cell->uac[i].dns_h.a->refcnt.val);
		}
		dns_srv_handle_put_shm_unsafe(&dead_cell->uac[i].dns_h);
#endif
	}

	/* collected to tags */
	tt=dead_cell->fwded_totags;
	while(tt) {
		foo=tt->next;
		shm_free_unsafe(tt->tag.s);
		shm_free_unsafe(tt);
		tt=foo;
	}

	/* free the avp list */
	if (dead_cell->user_avps_from)
		destroy_avp_list_unsafe( &dead_cell->user_avps_from );
	if (dead_cell->user_avps_to)
		destroy_avp_list_unsafe( &dead_cell->user_avps_to );
	if (dead_cell->uri_avps_from)
		destroy_avp_list_unsafe( &dead_cell->uri_avps_from );
	if (dead_cell->uri_avps_to)
		destroy_avp_list_unsafe( &dead_cell->uri_avps_to );

	/* the cell's body */
	shm_free_unsafe( dead_cell );

	shm_unlock();
}



static inline void init_synonym_id( struct cell *t )
{
	struct sip_msg *p_msg;
	int size;
	char *c;
	unsigned int myrand;

	if (!syn_branch) {
		p_msg=t->uas.request;
		if (p_msg) {
			/* char value of a proxied transaction is
			   calculated out of header-fields forming
			   transaction key
			*/
			char_msg_val( p_msg, t->md5 );
		} else {
			/* char value for a UAC transaction is created
			   randomly -- UAC is an originating stateful element 
			   which cannot be refreshed, so the value can be
			   anything
			*/
			/* HACK : not long enough */
			myrand=rand();
			c=t->md5;
			size=MD5_LEN;
			memset(c, '0', size );
			int2reverse_hex( &c, &size, myrand );
		}
	}
}

static void inline init_branches(struct cell *t)
{
	unsigned int i;
	struct ua_client *uac;

	for(i=0;i<MAX_BRANCHES;i++)
	{
		uac=&t->uac[i];
		uac->request.my_T = t;
		uac->request.branch = i;
		init_rb_timers(&uac->request);
		uac->local_cancel=uac->request;
#ifdef USE_DNS_FAILOVER
		dns_srv_handle_init(&uac->dns_h);
#endif
	}
}


struct cell*  build_cell( struct sip_msg* p_msg )
{
	struct cell* new_cell;
	int          sip_msg_len;
	avp_list_t* old;

	/* allocs a new cell */
	new_cell = (struct cell*)shm_malloc( sizeof( struct cell ) );
	if  ( !new_cell ) {
		ser_error=E_OUT_OF_MEM;
		return NULL;
	}

	/* filling with 0 */
	memset( new_cell, 0, sizeof( struct cell ) );

	/* UAS */
	new_cell->uas.response.my_T=new_cell;
	init_rb_timers(&new_cell->uas.response);
	/* timers */
	init_cell_timers(new_cell);

	old = set_avp_list(AVP_TRACK_FROM | AVP_CLASS_URI,  &new_cell->uri_avps_from );
	new_cell->uri_avps_from = *old;
	*old = 0;

	old = set_avp_list(AVP_TRACK_TO | AVP_CLASS_URI,  &new_cell->uri_avps_to );
	new_cell->uri_avps_to = *old;
	*old = 0;

	old = set_avp_list(AVP_TRACK_FROM | AVP_CLASS_USER,  &new_cell->user_avps_from );
	new_cell->user_avps_from = *old;
	*old = 0;

	old = set_avp_list(AVP_TRACK_TO | AVP_CLASS_USER,  &new_cell->user_avps_to );
	new_cell->user_avps_to = *old;
	*old = 0;

	     /* We can just store pointer to domain avps in the transaction context,
	      * because they are read-only
	      */
	new_cell->domain_avps_from = get_avp_list(AVP_TRACK_FROM | AVP_CLASS_DOMAIN);
	new_cell->domain_avps_to = get_avp_list(AVP_TRACK_TO | AVP_CLASS_DOMAIN);

	/* enter callback, which may potentially want to parse some stuff,
	 * before the request is shmem-ized */
	if ( p_msg && has_reqin_tmcbs() )
		run_reqin_callbacks( new_cell, p_msg, p_msg->REQ_METHOD);

	if (p_msg) {
		/* clean possible previous added vias/clen header or else they would 
		 * get propagated in the failure routes */
		free_via_clen_lump(&p_msg->add_rm);
		new_cell->uas.request = sip_msg_cloner(p_msg,&sip_msg_len);
		if (!new_cell->uas.request)
			goto error;
		new_cell->uas.end_request=((char*)new_cell->uas.request)+sip_msg_len;
	}

	/* UAC */
	init_branches(new_cell);

	new_cell->relayed_reply_branch   = -1;
	/* new_cell->T_canceled = T_UNDEFINED; */

	init_synonym_id(new_cell);
	init_cell_lock(  new_cell );
	return new_cell;

error:
	destroy_avp_list(&new_cell->user_avps_from);
	destroy_avp_list(&new_cell->user_avps_to);
	destroy_avp_list(&new_cell->uri_avps_from);
	destroy_avp_list(&new_cell->uri_avps_to);
	shm_free(new_cell);
	/* unlink transaction AVP list and link back the global AVP list (bogdan)*/
	reset_avps();
	return NULL;
}



/* Release all the data contained by the hash table. All the aux. structures
 *  as sems, lists, etc, are also released */
void free_hash_table(  )
{
	struct cell* p_cell;
	struct cell* tmp_cell;
	int    i;

	if (tm_table)
	{
		/* remove the data contained by each entry */
		for( i = 0 ; i<TABLE_ENTRIES; i++)
		{
			release_entry_lock( (tm_table->entrys)+i );
			/* delete all synonyms at hash-collision-slot i */
			p_cell=tm_table->entrys[i].first_cell;
			for( ; p_cell; p_cell = tmp_cell )
			{
				tmp_cell = p_cell->next_cell;
				free_cell( p_cell );
			}
		}
		shm_free(tm_table);
	}
}




/*
 */
struct s_table* init_hash_table()
{
	int              i;

	/*allocs the table*/
	tm_table= (struct s_table*)shm_malloc( sizeof( struct s_table ) );
	if ( !tm_table) {
		LOG(L_ERR, "ERROR: init_hash_table: no shmem for TM table\n");
		goto error0;
	}

	memset( tm_table, 0, sizeof (struct s_table ) );

	/* try first allocating all the structures needed for syncing */
	if (lock_initialize()==-1)
		goto error1;

	/* inits the entrys */
	for(  i=0 ; i<TABLE_ENTRIES; i++ )
	{
		init_entry_lock( tm_table, (tm_table->entrys)+i );
		tm_table->entrys[i].next_label = rand();
	}

	return  tm_table;

error1:
	free_hash_table( );
error0:
	return 0;
}




/*  Takes an already created cell and links it into hash table on the
 *  appropriate entry. */
void insert_into_hash_table_unsafe( struct cell * p_cell, unsigned int _hash )
{
	struct entry* p_entry;

	p_cell->hash_index=_hash;

	/* locates the appropriate entry */
	p_entry = &tm_table->entrys[ _hash ];

	p_cell->label = p_entry->next_label++;
	if ( p_entry->last_cell )
	{
		p_entry->last_cell->next_cell = p_cell;
		p_cell->prev_cell = p_entry->last_cell;
	} else p_entry->first_cell = p_cell;

	p_entry->last_cell = p_cell;

	/* update stats */
	p_entry->cur_entries++;
	p_entry->acc_entries++;
	t_stats_new( is_local(p_cell) );
}


/*  Un-link a  cell from hash_table, but the cell itself is not released */
void remove_from_hash_table_unsafe( struct cell * p_cell)
{
	struct entry*  p_entry  = &(tm_table->entrys[p_cell->hash_index]);

	/* unlink the cell from entry list */
	/* lock( &(p_entry->mutex) ); */

	if ( p_cell->prev_cell )
		p_cell->prev_cell->next_cell = p_cell->next_cell;
	else
		p_entry->first_cell = p_cell->next_cell;

	if ( p_cell->next_cell )
		p_cell->next_cell->prev_cell = p_cell->prev_cell;
	else
		p_entry->last_cell = p_cell->prev_cell;
	/* update stats */
#	ifdef EXTRA_DEBUG
	if (p_entry->cur_entries==0) {
		LOG(L_CRIT, "BUG: bad things happened: cur_entries=0\n");
		abort();
	}
#	endif
	p_entry->cur_entries--;
	t_stats_deleted( is_local(p_cell) );

	/* unlock( &(p_entry->mutex) ); */
}

