/*
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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


#include "../../mem/shm_mem.h"
#include "../../hash_func.h"
#include "h_table.h"
#include "../../dprint.h"
#include "../../md5utils.h"
/* bogdan test */
#include "../../ut.h"
#include "../../globals.h"
#include "../../error.h"
#include "../../fifo_server.h"
#include "t_reply.h"
#include "t_cancel.h"
#include "t_stats.h"

/* pointer to the big table where all the transaction data
   lives
*/

static struct s_table*  tm_table;

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

	release_cell_lock( dead_cell );
	shm_lock();

	/* UA Server */
	if ( dead_cell->uas.request )
		sip_msg_free_unsafe( dead_cell->uas.request );
	if ( dead_cell->uas.response.buffer )
		shm_free_unsafe( dead_cell->uas.response.buffer );
#ifdef TOTAG
	if (dead_cell->uas.to_tag.s)
		shm_free_unsafe(dead_cell->uas.to_tag.s);
#endif

	/* completion callback */
	if (dead_cell->cbp) shm_free_unsafe(dead_cell->cbp);

	/* UA Clients */
	for ( i =0 ; i<dead_cell->nr_of_outgoings;  i++ )
	{
		/* retransmission buffer */
		if ( (b=dead_cell->uac[i].request.buffer) )
			shm_free_unsafe( b );
#ifdef OLD_CANCEL
		if ( (b=dead_cell->uac[i].request.ack) )
			shm_free_unsafe( b );
		if ( (b=dead_cell->uac[i].request.cancel) )
			shm_free_unsafe( b );
#endif
		b=dead_cell->uac[i].local_cancel.buffer;
		if (b!=0 && b!=BUSY_BUFFER)
			shm_free_unsafe( b );
		rpl=dead_cell->uac[i].reply;
		if (rpl && rpl!=FAKED_REPLY) {
			sip_msg_free_unsafe( rpl );
		}
#ifdef _OBSOLETED
		if ( (b=dead_cell->uac[i].rpl_buffer.s) )
			shm_free_unsafe( b );
#endif
	}

	/* the cell's body */
	shm_free_unsafe( dead_cell );

	shm_unlock();
}




struct cell*  build_cell( struct sip_msg* p_msg )
{
	struct cell* new_cell;
	unsigned int i;
	unsigned int rand;
	int size;
	char *c;
	struct ua_client *uac;

	/* avoid 'unitialized var use' warning */
	rand=0;

	/* allocs a new cell */
	new_cell = (struct cell*)shm_malloc( sizeof( struct cell ) );
	if  ( !new_cell ) {
		ser_error=E_OUT_OF_MEM;
		return NULL;
	}

	/* filling with 0 */
	memset( new_cell, 0, sizeof( struct cell ) );

	/* UAS */
	new_cell->uas.response.retr_timer.tg=TG_RT;
	new_cell->uas.response.fr_timer.tg=TG_FR;
	new_cell->uas.response.fr_timer.payload =
		new_cell->uas.response.retr_timer.payload = &(new_cell->uas.response);
	new_cell->uas.response.my_T=new_cell;

	/* bogdan - debug */
	/*fprintf(stderr,"before clone VIA |%.*s|\n",via_len(p_msg->via1),
		via_s(p_msg->via1,p_msg));*/

	if (p_msg) {
		new_cell->uas.request = sip_msg_cloner(p_msg);
		if (!new_cell->uas.request)
			goto error;
	}

	/* new_cell->uas.to_tag = &( get_to(new_cell->uas.request)->tag_value ); */
	new_cell->uas.response.my_T = new_cell;

	/* UAC */
	for(i=0;i<MAX_BRANCHES;i++)
	{
		uac=&new_cell->uac[i];
		uac->request.my_T = new_cell;
		uac->request.branch = i;
		uac->request.fr_timer.tg = TG_FR;
		uac->request.retr_timer.tg = TG_RT;
		uac->request.retr_timer.payload = 
			uac->request.fr_timer.payload =
			&uac->request;
		uac->local_cancel=uac->request;
	}

	/* global data for transaction */
	if (p_msg) {
		new_cell->hash_index = p_msg->hash_index;
	} else {
		rand = random();
		new_cell->hash_index = rand % TABLE_ENTRIES ;
	}
	new_cell->wait_tl.payload = new_cell;
	new_cell->dele_tl.payload = new_cell;
	new_cell->relaied_reply_branch   = -1;
	/* new_cell->T_canceled = T_UNDEFINED; */
	new_cell->wait_tl.tg=TG_WT;
	new_cell->dele_tl.tg=TG_DEL;

	if (!syn_branch) {
		if (p_msg) {
			/* char value of a proxied transaction is
			   calculated out of header-fileds forming
			   transaction key
			*/
			char_msg_val( p_msg, new_cell->md5 );
		} else {
			/* char value for a UAC transaction is created
			   randomly -- UAC is an originating stateful element 
			   which cannot be refreshed, so the value can be
			   anything
			*/
			/* HACK : not long enough */
			c=new_cell->md5;
			size=MD5_LEN;
			memset(c, '0', size );
			int2reverse_hex( &c, &size, rand );
		}
	}

	init_cell_lock(  new_cell );
	return new_cell;

error:
	shm_free(new_cell);
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

#ifdef _OBSO
error2:
	lock_cleanup();
#endif
error1:
	free_hash_table( );
error0:
	return 0;
}




/*  Takes an already created cell and links it into hash table on the
 *  appropiate entry. */
void insert_into_hash_table_unsafe( struct cell * p_cell )
{
	struct entry* p_entry;

	/* locates the apropiate entry */
	p_entry = &tm_table->entrys[ p_cell->hash_index ];

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
	t_stats_new(p_cell->local);
}




void insert_into_hash_table( struct cell * p_cell)
{
	LOCK_HASH(p_cell->hash_index);
	insert_into_hash_table_unsafe(  p_cell );
	UNLOCK_HASH(p_cell->hash_index);
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
	t_stats_deleted(p_cell->local);

	/* unlock( &(p_entry->mutex) ); */
}

/* print accumulated distribution of the hash table */
int fifo_hash( FILE *stream, char *response_file )
{
	FILE *reply_file;
	unsigned int i;

	reply_file=open_reply_pipe(response_file);
	if (reply_file==0) {
		LOG(L_ERR, "ERROR: fifo_hash: file '%s' not opened\n", 
			response_file);
		return -1;
	}
	fputs( "200 ok\n\tcurrent\ttotal\n", reply_file);
	for (i=0; i<TABLE_ENTRIES; i++) {
		fprintf(reply_file, "%d.\t%lu\t%lu\n", 
			i, tm_table->entrys[i].cur_entries ,
			tm_table->entrys[i].acc_entries );
	}
	fclose(reply_file);
	return 1;
}
