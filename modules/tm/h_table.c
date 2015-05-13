/*
 * Copyright (C) 2001-2003 FhG Fokus
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

/*!
 * \file 
 * \brief TM :: 
 * \ingroup tm
 */

#include <stdlib.h>


#include "../../mem/shm_mem.h"
#include "../../hash_func.h"
#include "../../dprint.h"
#include "../../md5utils.h"
#include "../../ut.h"
#include "../../globals.h"
#include "../../error.h"
#include "../../char_msg_val.h"
#include "defs.h"
#include "t_reply.h"
#include "t_cancel.h"
#include "t_stats.h"
#include "h_table.h"
#include "../../fix_lumps.h" /* free_via_clen_lump */
#include "timer.h"
#include "uac.h" /* free_local_ack */


static enum kill_reason kr;

/* pointer to the big table where all the transaction data
   lives */
struct s_table*  _tm_table;

struct s_table* tm_get_table(void) {
	return _tm_table;
}

void reset_kr(void) {
	kr=0;
}

void set_kr( enum kill_reason _kr )
{
	kr|=_kr;
}


enum kill_reason get_kr() {
	return kr;
}


void lock_hash(int i) 
{

	int mypid;

	mypid = my_pid();
	if (likely(atomic_get(&_tm_table->entries[i].locker_pid) != mypid)) {
		lock(&_tm_table->entries[i].mutex);
		atomic_set(&_tm_table->entries[i].locker_pid, mypid);
	} else {
		/* locked within the same process that called us*/
		_tm_table->entries[i].rec_lock_level++;
	}
}


void unlock_hash(int i) 
{
	if (likely(_tm_table->entries[i].rec_lock_level == 0)) {
		atomic_set(&_tm_table->entries[i].locker_pid, 0);
		unlock(&_tm_table->entries[i].mutex);
	} else  {
		/* recursive locked => decrease rec. lock count */
		_tm_table->entries[i].rec_lock_level--;
	}
}



#ifdef TM_HASH_STATS
unsigned int transaction_count( void )
{
	unsigned int i;
	unsigned int count;

	count=0;	
	for (i=0; i<TABLE_ENTRIES; i++) 
		count+=_tm_table->entries[i].cur_entries;
	return count;
}
#endif



void free_cell( struct cell* dead_cell )
{
	char *b;
	int i;
	struct sip_msg *rpl;
	struct totag_elem *tt, *foo;
	struct tm_callback *cbs, *cbs_tmp;

	release_cell_lock( dead_cell );
	if (unlikely(has_tran_tmcbs(dead_cell, TMCB_DESTROY)))
		run_trans_callbacks(TMCB_DESTROY, dead_cell, 0, 0, 0);

	shm_lock();
	/* UA Server */
	if ( dead_cell->uas.request )
		sip_msg_free_unsafe( dead_cell->uas.request );
	if ( dead_cell->uas.response.buffer )
		shm_free_unsafe( dead_cell->uas.response.buffer );
#ifdef CANCEL_REASON_SUPPORT
	if (unlikely(dead_cell->uas.cancel_reas))
		shm_free_unsafe(dead_cell->uas.cancel_reas);
#endif /* CANCEL_REASON_SUPPORT */

	/* callbacks */
	for( cbs=(struct tm_callback*)dead_cell->tmcb_hl.first ; cbs ; ) {
		cbs_tmp = cbs;
		cbs = cbs->next;
		if (cbs_tmp->release) {
			/* It is safer to release the shm memory lock
			 * otherwise the release function must to be aware of
			 * the lock state (Miklos)
			 */
			shm_unlock();
			cbs_tmp->release(cbs_tmp->param);
			shm_lock();
		}
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
		if (unlikely(dead_cell->uac[i].path.s)) {
			shm_free_unsafe(dead_cell->uac[i].path.s);
		}
		if (unlikely(dead_cell->uac[i].instance.s)) {
			shm_free_unsafe(dead_cell->uac[i].instance.s);
		}
		if (unlikely(dead_cell->uac[i].ruid.s)) {
			shm_free_unsafe(dead_cell->uac[i].ruid.s);
		}
		if (unlikely(dead_cell->uac[i].location_ua.s)) {
			shm_free_unsafe(dead_cell->uac[i].location_ua.s);
		}
	}

#ifdef WITH_AS_SUPPORT
	if (dead_cell->uac[0].local_ack)
		free_local_ack_unsafe(dead_cell->uac[0].local_ack);
#endif

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
#ifdef WITH_XAVP
	if (dead_cell->xavps_list)
		xavp_destroy_list_unsafe( &dead_cell->xavps_list );
#endif

	/* the cell's body */
	shm_free_unsafe( dead_cell );

	shm_unlock();
	t_stats_freed();
}



static inline void init_synonym_id( struct sip_msg *p_msg, char *hash )
{
	int size;
	char *c;
	unsigned int myrand;

	if (p_msg) {
		/* char value of a proxied transaction is
		   calculated out of header-fields forming
		   transaction key
		*/
		char_msg_val( p_msg, hash );
	} else {
		/* char value for a UAC transaction is created
		   randomly -- UAC is an originating stateful element
		   which cannot be refreshed, so the value can be
		   anything
		*/
		/* HACK : not long enough */
		myrand=rand();
		c = hash;
		size=MD5_LEN;
		memset(c, '0', size );
		int2reverse_hex( &c, &size, myrand );
	}
}

static void inline init_branches(struct cell *t)
{
	unsigned int i;
	struct ua_client *uac;

	for(i=0;i<sr_dst_max_branches;i++)
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
	struct tm_callback *cbs, *cbs_tmp;
#ifdef WITH_XAVP
	sr_xavp_t** xold;
#endif
	unsigned int cell_size;

	/* allocs a new cell, add space for:
	 * md5 (MD5_LEN - sizeof(struct cell.md5))
	 * uac (sr_dst_max_banches * sizeof(struct ua_client) ) */
	cell_size = sizeof( struct cell ) + MD5_LEN - sizeof(((struct cell*)0)->md5)
				+ (sr_dst_max_branches * sizeof(struct ua_client));

	new_cell = (struct cell*)shm_malloc( cell_size );
	if  ( !new_cell ) {
		ser_error=E_OUT_OF_MEM;
		return NULL;
	}

	/* filling with 0 */
	memset( new_cell, 0, cell_size );

	/* UAS */
	new_cell->uas.response.my_T=new_cell;
	init_rb_timers(&new_cell->uas.response);
	/* UAC */
	new_cell->uac = (struct ua_client*)((char*)new_cell + sizeof(struct cell)
							+ MD5_LEN - sizeof(((struct cell*)0)->md5));
	/* timers */
	init_cell_timers(new_cell);

	old = set_avp_list(AVP_TRACK_FROM | AVP_CLASS_URI, 
			&new_cell->uri_avps_from );
	new_cell->uri_avps_from = *old;
	*old = 0;

	old = set_avp_list(AVP_TRACK_TO | AVP_CLASS_URI, 
			&new_cell->uri_avps_to );
	new_cell->uri_avps_to = *old;
	*old = 0;

	old = set_avp_list(AVP_TRACK_FROM | AVP_CLASS_USER, 
			&new_cell->user_avps_from );
	new_cell->user_avps_from = *old;
	*old = 0;

	old = set_avp_list(AVP_TRACK_TO | AVP_CLASS_USER, 
			&new_cell->user_avps_to );
	new_cell->user_avps_to = *old;
	*old = 0;

#ifdef WITH_XAVP
	xold = xavp_set_list(&new_cell->xavps_list );
	new_cell->xavps_list = *xold;
	*xold = 0;
#endif

	     /* We can just store pointer to domain avps in the transaction context,
	      * because they are read-only
	      */
	new_cell->domain_avps_from = get_avp_list(AVP_TRACK_FROM | 
								AVP_CLASS_DOMAIN);
	new_cell->domain_avps_to = get_avp_list(AVP_TRACK_TO | AVP_CLASS_DOMAIN);

	/* enter callback, which may potentially want to parse some stuff,
	 * before the request is shmem-ized */
	if (p_msg) {
		set_early_tmcb_list(p_msg, new_cell);
		if(has_reqin_tmcbs())
			run_reqin_callbacks( new_cell, p_msg, p_msg->REQ_METHOD);
	}

	if (p_msg) {
		new_cell->uas.request = sip_msg_cloner(p_msg,&sip_msg_len);
		if (!new_cell->uas.request)
			goto error;
		new_cell->uas.end_request=((char*)new_cell->uas.request)+sip_msg_len;
	}

	/* UAC */
	init_branches(new_cell);

	new_cell->relayed_reply_branch   = -1;
	/* new_cell->T_canceled = T_UNDEFINED; */

	init_synonym_id(p_msg, new_cell->md5);
	init_cell_lock(  new_cell );
	init_async_lock( new_cell );
	t_stats_created();
	return new_cell;

error:
	/* Other modules may have already registered some
	 * transaction callbacks and may also allocated
	 * additional memory for their parameters,
	 * hence TMCB_DESTROY needs to be called. (Miklos)
	 */
	if (unlikely(has_tran_tmcbs(new_cell, TMCB_DESTROY)))
		run_trans_callbacks(TMCB_DESTROY, new_cell, 0, 0, 0);

	/* free the callback list */
	for( cbs=(struct tm_callback*)new_cell->tmcb_hl.first ; cbs ; ) {
		cbs_tmp = cbs;
		cbs = cbs->next;
		if (cbs_tmp->release) {
			cbs_tmp->release(cbs_tmp->param);
		}
		shm_free( cbs_tmp );
	}
	
	destroy_avp_list(&new_cell->user_avps_from);
	destroy_avp_list(&new_cell->user_avps_to);
	destroy_avp_list(&new_cell->uri_avps_from);
	destroy_avp_list(&new_cell->uri_avps_to);
#ifdef WITH_XAVP
	xavp_destroy_list(&new_cell->xavps_list);
#endif
	shm_free(new_cell);
	/* unlink transaction AVP list and link back the global AVP list (bogdan)*/
	reset_avps();
#ifdef WITH_XAVP
	xavp_reset_list();
#endif
	return NULL;
}



/* Release all the data contained by the hash table. All the aux. structures
 *  as sems, lists, etc, are also released */
void free_hash_table(  )
{
	struct cell* p_cell;
	struct cell* tmp_cell;
	int    i;

	if (_tm_table)
	{
		/* remove the data contained by each entry */
		for( i = 0 ; i<TABLE_ENTRIES; i++)
		{
			release_entry_lock( (_tm_table->entries)+i );
			/* delete all synonyms at hash-collision-slot i */
			clist_foreach_safe(&_tm_table->entries[i], p_cell, tmp_cell,
									next_c){
				free_cell(p_cell);
			}
		}
		shm_free(_tm_table);
		_tm_table = 0;
	}
}




/*
 */
struct s_table* init_hash_table()
{
	int              i;

	/*allocs the table*/
	_tm_table= (struct s_table*)shm_malloc( sizeof( struct s_table ) );
	if ( !_tm_table) {
		LOG(L_ERR, "ERROR: init_hash_table: no shmem for TM table\n");
		goto error0;
	}

	memset( _tm_table, 0, sizeof (struct s_table ) );

	/* try first allocating all the structures needed for syncing */
	if (lock_initialize()==-1)
		goto error1;

	/* inits the entriess */
	for(  i=0 ; i<TABLE_ENTRIES; i++ )
	{
		init_entry_lock( _tm_table, (_tm_table->entries)+i );
		_tm_table->entries[i].next_label = rand();
		/* init cell list */
		clist_init(&_tm_table->entries[i], next_c, prev_c);
	}

	return  _tm_table;

error1:
	free_hash_table( );
error0:
	return 0;
}


/**
 * backup xdata from/to msg context to local var and use T lists
 * - mode = 0 - from msg context to _txdata and use T lists
 * - mode = 1 - restore to msg context from _txdata
 */
void tm_xdata_swap(tm_cell_t *t, tm_xlinks_t *xd, int mode)
{
	static tm_xlinks_t _txdata;
	tm_xlinks_t *x;

	if(xd==NULL)
		x = &_txdata;
	else
		x = xd;

	if(mode==0) {
		if(t==NULL)
			return;
		x->uri_avps_from = set_avp_list(AVP_TRACK_FROM | AVP_CLASS_URI, &t->uri_avps_from );
		x->uri_avps_to = set_avp_list(AVP_TRACK_TO | AVP_CLASS_URI, &t->uri_avps_to );
		x->user_avps_from = set_avp_list(AVP_TRACK_FROM | AVP_CLASS_USER, &t->user_avps_from );
		x->user_avps_to = set_avp_list(AVP_TRACK_TO | AVP_CLASS_USER, &t->user_avps_to );
		x->domain_avps_from = set_avp_list(AVP_TRACK_FROM | AVP_CLASS_DOMAIN, &t->domain_avps_from );
		x->domain_avps_to = set_avp_list(AVP_TRACK_TO | AVP_CLASS_DOMAIN, &t->domain_avps_to );
#ifdef WITH_XAVP
		x->xavps_list = xavp_set_list(&t->xavps_list);
#endif
	} else if(mode==1) {
		/* restore original avp list */
		set_avp_list( AVP_TRACK_FROM | AVP_CLASS_URI, x->uri_avps_from );
		set_avp_list( AVP_TRACK_TO | AVP_CLASS_URI, x->uri_avps_to );
		set_avp_list( AVP_TRACK_FROM | AVP_CLASS_USER, x->user_avps_from );
		set_avp_list( AVP_TRACK_TO | AVP_CLASS_USER, x->user_avps_to );
		set_avp_list( AVP_TRACK_FROM | AVP_CLASS_DOMAIN, x->domain_avps_from );
		set_avp_list( AVP_TRACK_TO | AVP_CLASS_DOMAIN, x->domain_avps_to );
#ifdef WITH_XAVP
		xavp_set_list(x->xavps_list);
#endif
	}

}

/**
 * replace existing lists with newxd and backup in bakxd or restore from bakxd
 */
void tm_xdata_replace(tm_xdata_t *newxd, tm_xlinks_t *bakxd)
{
	if(newxd==NULL && bakxd!=NULL) {
		set_avp_list(AVP_TRACK_FROM | AVP_CLASS_URI, bakxd->uri_avps_from);
		set_avp_list(AVP_TRACK_TO | AVP_CLASS_URI, bakxd->uri_avps_to);
		set_avp_list(AVP_TRACK_FROM | AVP_CLASS_USER, bakxd->user_avps_from);
		set_avp_list(AVP_TRACK_TO | AVP_CLASS_USER, bakxd->user_avps_to);
		set_avp_list(AVP_TRACK_FROM | AVP_CLASS_DOMAIN, bakxd->domain_avps_from);
		set_avp_list(AVP_TRACK_TO | AVP_CLASS_DOMAIN, bakxd->domain_avps_to);
#ifdef WITH_XAVP
		xavp_set_list(bakxd->xavps_list);
#endif
		return;
	}

	if(newxd!=NULL && bakxd!=NULL) {
		bakxd->uri_avps_from = set_avp_list(AVP_TRACK_FROM | AVP_CLASS_URI,
				&newxd->uri_avps_from);
		bakxd->uri_avps_to = set_avp_list(AVP_TRACK_TO | AVP_CLASS_URI,
				&newxd->uri_avps_to);
		bakxd->user_avps_from = set_avp_list(AVP_TRACK_FROM | AVP_CLASS_USER,
				&newxd->user_avps_from);
		bakxd->user_avps_to = set_avp_list(AVP_TRACK_TO | AVP_CLASS_USER,
				&newxd->user_avps_to);
		bakxd->domain_avps_from = set_avp_list(AVP_TRACK_FROM | AVP_CLASS_DOMAIN,
				&newxd->domain_avps_from);
		bakxd->domain_avps_to = set_avp_list(AVP_TRACK_TO | AVP_CLASS_DOMAIN,
				&newxd->domain_avps_to);
#ifdef WITH_XAVP
		bakxd->xavps_list = xavp_set_list(&newxd->xavps_list);
#endif
		return;
	}
}
