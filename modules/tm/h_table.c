/*
 * $Id$
 */

#include "hash_func.h"
#include "h_table.h"
#include "../../dprint.h"
#include "sh_malloc.h"
#include "../../md5utils.h"
/* bogdan test */
#include "../../ut.h"
#include "../../globals.h"
#include "../../error.h"



void free_cell( struct cell* dead_cell )
{
	char *b;
	int i;

	release_cell_lock( dead_cell );
	shm_lock();

	/* UA Server */
	if ( dead_cell->uas.request )
		sip_msg_free_unsafe( dead_cell->uas.request );
	if ( dead_cell->uas.response.buffer )
		shm_free_unsafe( dead_cell->uas.response.buffer );

	/* UA Clients */
	for ( i =0 ; i<dead_cell->nr_of_outgoings;  i++ )
	{
		/* retransmission buffer */
		if ( (b=dead_cell->uac[i].request.buffer) )
		{
			shm_free_unsafe( b );
			b = 0;
		}
		if ( (b=dead_cell->uac[i].request.ack) )
		{
			shm_free_unsafe( b );
			b = 0;
		}
		if ( (b=dead_cell->uac[i].request.cancel) )
		{
			shm_free_unsafe( b );
			b = 0;
		}
		if ( (b=dead_cell->uac[i].rpl_buffer.s) )
		{
			shm_free_unsafe( b );
			b = 0;
		}
	}

	/* the cell's body */
	shm_free_unsafe( dead_cell );

	shm_unlock();
}




struct cell*  build_cell( struct sip_msg* p_msg )
{
	struct cell* new_cell;
	unsigned int i;
#ifndef USE_SYNONIM
	str          src[8];
#endif

	/* do we have the source for the build process? */
	if (!p_msg)
		return NULL;

	/* allocs a new cell */
	new_cell = (struct cell*)sh_malloc( sizeof( struct cell ) );
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

	/* bogdan - debug */
	/*fprintf(stderr,"before clone VIA |%.*s|\n",via_len(p_msg->via1),
		via_s(p_msg->via1,p_msg));*/

	new_cell->uas.request = sip_msg_cloner(p_msg);

    /* bogdan - debug */
    /*fprintf(stderr,"after clone VIA |%.*s|\n",
		via_len(new_cell->uas.request->via1),
		via_s(new_cell->uas.request->via1,new_cell->uas.request) );*/

	if (!new_cell->uas.request)
		goto error;
	new_cell->uas.tag = &( get_to(new_cell->uas.request)->tag_value );
	new_cell->uas.response.my_T = new_cell;

	/* UAC */
	for(i=0;i<MAX_FORK;i++)
	{
		new_cell->uac[i].request.my_T = new_cell;
		new_cell->uac[i].request.branch = i;
		new_cell->uac[i].request.fr_timer.tg = TG_FR;
		new_cell->uac[i].request.retr_timer.tg = TG_RT;
		new_cell->uac[i].request.retr_timer.payload = 
			new_cell->uac[i].request.fr_timer.payload =
			&(new_cell->uac[i].request);
	}

	/* global data for transaction */
	new_cell->hash_index = p_msg->hash_index;
	new_cell->wait_tl.payload = new_cell;
	new_cell->dele_tl.payload = new_cell;
	new_cell->relaied_reply_branch   = -1;
	new_cell->T_canceled = T_UNDEFINED;
	new_cell->wait_tl.tg=TG_WT;
	new_cell->dele_tl.tg=TG_DEL;
#ifndef USE_SYNONIM
	src[0]= p_msg->from->body;
	src[1]= p_msg->to->body;
	src[2]= p_msg->callid->body;
	src[3]= p_msg->first_line.u.request.uri;
	src[4]= get_cseq( p_msg )->number;

	/* topmost Via is part of transaction key as well ! */
	src[5]= p_msg->via1->host;
	src[6]= p_msg->via1->port_str;
	if (p_msg->via1->branch) {
		src[7]= p_msg->via1->branch->value;
		MDStringArray ( new_cell->md5, src, 8 );
	} else {
		MDStringArray ( new_cell->md5, src, 7 );
	}
 #endif

	init_cell_lock(  new_cell );
	return new_cell;

error:
	sh_free(new_cell);
	return NULL;
}




/* Release all the data contained by the hash table. All the aux. structures
 *  as sems, lists, etc, are also released */
void free_hash_table( struct s_table *hash_table )
{
	struct cell* p_cell;
	struct cell* tmp_cell;
	int    i;

	if (hash_table)
	{
		/* remove the data contained by each entry */
		for( i = 0 ; i<TABLE_ENTRIES; i++)
		{
			release_entry_lock( (hash_table->entrys)+i );
			/* delete all synonyms at hash-collision-slot i */
			p_cell=hash_table->entrys[i].first_cell;
			for( ; p_cell; p_cell = tmp_cell )
			{
				tmp_cell = p_cell->next_cell;
				free_cell( p_cell );
			}
		}

		/* the mutexs for sync the lists are released*/
		for ( i=0 ; i<NR_OF_TIMER_LISTS ; i++ )
			release_timerlist_lock( &(hash_table->timers[i]) );

		sh_free( hash_table );
	}
}




/*
 */
struct s_table* init_hash_table()
{
	struct s_table*  hash_table;
	int              i;

	/*allocs the table*/
	hash_table = (struct s_table*)sh_malloc( sizeof( struct s_table ) );
	if ( !hash_table )
		goto error;

	memset( hash_table, 0, sizeof (struct s_table ) );

	/* try first allocating all the structures needed for syncing */
	if (lock_initialize()==-1)
		goto error;

	/* inits the entrys */
	for(  i=0 ; i<TABLE_ENTRIES; i++ )
	{
		init_entry_lock( hash_table , (hash_table->entrys)+i );
		hash_table->entrys[i].next_label = rand();
	}

	/* inits the timers*/
	for(  i=0 ; i<NR_OF_TIMER_LISTS ; i++ )
		init_timer_list( hash_table, i );

	return  hash_table;

error:
	free_hash_table( hash_table );
	lock_cleanup();
	return 0;
}




/*  Takes an already created cell and links it into hash table on the
 *  appropiate entry. */
void insert_into_hash_table_unsafe( struct s_table *hash_table,
											struct cell * p_cell )
{
	struct entry* p_entry;

	/* locates the apropiate entry */
	p_entry = &hash_table->entrys[ p_cell->hash_index ];

	p_cell->label = p_entry->next_label++;
	if ( p_entry->last_cell )
	{
		p_entry->last_cell->next_cell = p_cell;
		p_cell->prev_cell = p_entry->last_cell;
	} else p_entry->first_cell = p_cell;

	p_entry->last_cell = p_cell;
}




void insert_into_hash_table(struct s_table *hash_table,  struct cell * p_cell)
{
	lock( &(hash_table->entrys[ p_cell->hash_index ].mutex) );
	insert_into_hash_table_unsafe( hash_table,  p_cell );
	unlock( &(hash_table->entrys[ p_cell->hash_index ].mutex) );
}




/*  Un-link a  cell from hash_table, but the cell itself is not released */
void remove_from_hash_table(struct s_table *hash_table,  struct cell * p_cell)
{
	struct entry*  p_entry  = &(hash_table->entrys[p_cell->hash_index]);

	/* unlink the cell from entry list */
	lock( &(p_entry->mutex) );

	if ( p_cell->prev_cell )
		p_cell->prev_cell->next_cell = p_cell->next_cell;
	else
		p_entry->first_cell = p_cell->next_cell;

	if ( p_cell->next_cell )
		p_cell->next_cell->prev_cell = p_cell->prev_cell;
	else
		p_entry->last_cell = p_cell->prev_cell;

	unlock( &(p_entry->mutex) );
}


