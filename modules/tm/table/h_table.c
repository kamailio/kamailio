#include "h_table.h"

int table_entries;


void free_cell( struct cell* dead_cell )
{
   sh_free( dead_cell->from );
   sh_free( dead_cell->to );
   sh_free( dead_cell->req_tag );
   sh_free( dead_cell->res_tag );
   sh_free( dead_cell->call_id );
   sh_free( dead_cell->cseq_nr );
   sh_free( dead_cell->cseq_method );
   sh_free( dead_cell->incoming_req_uri );
   sh_free( dead_cell->outgoing_req_uri );
   remove_sem( dead_cell->sem );
   sh_free( dead_cell );
}


void free_hash_table( struct s_table *hash_table )
{
   struct cell* p_cell;
   struct cell* tmp_cell;
   int   i;

   if (hash_table)
   {
      if ( hash_table->entrys )
      {

	 /* remove the hash table entry by entry */
         for( i = 0 ; i<table_entries; i++)
          {
             remove_sem( hash_table->entrys[i].sem );
	     /* delete all synonyms at hash-collision-slot i */
	     for( p_cell = hash_table->entrys[i].first_cell; p_cell; p_cell = tmp_cell )
	     {
		tmp_cell = p_cell->next_cell;
                free_cell( p_cell );
              }
          }
          sh_free( hash_table->entrys );
      }

	/* delete all cells on the to-be-deleted list */
       for( p_cell = hash_table->timers[DELETE_LIST].first_cell; p_cell; p_cell = tmp_cell )
        {
                tmp_cell = p_cell->tl[DELETE_LIST].timer_next_cell;
                remove_timer_from_head( hash_table, p_cell, DELETE_LIST );
                free_cell( p_cell );
       }
	
      if ( hash_table->timers)
      {
         sh_free( hash_table->timers );
      }
      sh_free( hash_table );
   }
}



struct s_table* init_hash_table()
{
   struct s_table*  hash_table;
   int       i;

   /*allocs the table*/
   hash_table = sh_malloc(  sizeof( struct s_table ) );
   if ( !hash_table )
   {
       free_hash_table( hash_table );
      return 0;
   }

   /*inits the time*/
   hash_table->time = 0;

   /* allocs the entry's table */
    table_entries = TABLE_ENTRIES;
    hash_table->entrys  = sh_malloc( table_entries * sizeof( struct entry )  );
    if ( !hash_table->entrys )
    {
        free_hash_table( hash_table );
       return 0;
    }

   /* allocs the timer's table */
    hash_table->timers  = sh_malloc( NR_OF_TIMER_LISTS * sizeof( struct timer )  );
    if ( !hash_table->timers )
    {
        free_hash_table( hash_table );
       return 0;
    }

    /* inits the entrys */
    for(  i=0 ; i<table_entries; i++ )
    {
       hash_table->entrys[i].first_cell = 0;
       hash_table->entrys[i].last_cell = 0;
       hash_table->entrys[i].next_label = 1;
       hash_table->entrys[i].sem = create_sem( SEM_KEY , 1 ) ;
    }

   /* inits the timers*/
    for(  i=0 ; i<NR_OF_TIMER_LISTS ; i++ )
    {
       hash_table->timers[i].first_cell = 0;
       hash_table->timers[i].last_cell = 0;
       hash_table->timers[i].sem = create_sem( SEM_KEY , 1 ) ;
    }

#ifdef THREAD
   /* starts the timer thread/ process */
   pthread_create( &(hash_table->timer_thread_id), NULL, timer_routine, hash_table );
#endif

   return  hash_table;
}




struct cell* add_Transaction( struct s_table* hash_table, 
	char* incoming_req_uri, char* from, char* to, char* tag, 
	char* call_id, char* cseq_nr ,char* cseq_method )
{
   struct cell*    new_cell;
   struct entry* match_entry;
   char*              via_label;
   int                  hash_index;

   hash_index   = hash( call_id , cseq_nr );
   /*will it be faster to repolace  x % 256   with   x - (x>>8)<<8  ?  */
   match_entry = &hash_table->entrys[hash_index];

   new_cell = sh_malloc( sizeof( struct cell ) );
   if  ( !new_cell )
      return 0;

   memset( new_cell, 0, sizeof( struct cell ) );

   /* remember hash entry where the cell lives */
   new_cell->hash_index = hash_index;

   //incoming_req_uri
   new_cell->incoming_req_uri_length = strlen( incoming_req_uri );
   new_cell->incoming_req_uri = sh_malloc( new_cell->incoming_req_uri_length+1 );
   strcpy( new_cell->incoming_req_uri , incoming_req_uri );
   //from
   new_cell->from_length = strlen( from );
   new_cell->from = sh_malloc( new_cell->from_length+1 );
   strcpy( new_cell->from , from );
   //to
   new_cell->to_length = strlen( to );
   new_cell->to = sh_malloc( new_cell->to_length+1 );
   strcpy( new_cell->to , to );
   //req_tag
   if (tag)
   {
      new_cell->req_tag_length = strlen( tag );
      new_cell->req_tag = sh_malloc( new_cell->req_tag_length+1 );
      strcpy( new_cell->req_tag , tag );
   }
   //cseq_nr
   new_cell->cseq_nr_length = strlen( cseq_nr );
   new_cell->cseq_nr = sh_malloc( new_cell->cseq_nr_length+1 );
   strcpy( new_cell->cseq_nr , cseq_nr );
   //cseq_method
   new_cell->cseq_method_length = strlen( cseq_method );
   new_cell->cseq_method = sh_malloc( new_cell->cseq_method_length+1 );
   strcpy( new_cell->cseq_method , cseq_method );
   //call_id
   new_cell->call_id_length = strlen( call_id );
   new_cell->call_id = sh_malloc( new_cell->call_id_length+1 );
   strcpy( new_cell->call_id , call_id );
   //status
   /* new_cell->status = 100; */
   //cell semaphore
   new_cell->sem = create_sem( SEM_KEY , 1 ) ;

   // critical region - inserting the cell at the end of the list
   change_sem( match_entry->sem , -1  );
   new_cell->label = match_entry->next_label++;
   if ( match_entry->last_cell )
   {
      match_entry->last_cell->next_cell = new_cell;
      new_cell->prev_cell = match_entry->last_cell;
   }
   else
      match_entry->first_cell = new_cell;
   match_entry->last_cell = new_cell;
   change_sem( match_entry->sem , +1 );

   //via label
   via_label = sh_malloc( 512 );
   sprintf( via_label , "%da%d", hash_index , new_cell->label );
   new_cell->via_label = sh_malloc( strlen(via_label)+1 );
   strcpy( new_cell->via_label , via_label );
   sh_free( via_label );

   return new_cell;
}




void ref_Cell( struct cell* p_cell)
{
   change_sem( p_cell->sem , -1 );
   p_cell->ref_counter++;
   change_sem( p_cell->sem , +1 );
}


void unref_Cell( struct cell* p_cell)
{
   change_sem( p_cell->sem , -1 );
   p_cell ->ref_counter--;
   change_sem( p_cell->sem , +1 );
}



/*
 *  the method looks for a perfect transaction matche for a request. The request cannot be an ACK because the ACK
 *  request are treated before (and ACK req don't have transactions :-) ). Requests don't have labels for search - the label
 *  matching tech. is nor used, only standard search. No special matching for the tag attr from To header - the request are
 *  different than ACK -> perfect To matche is performed.
 *  WARNING : in case of a returned transaction, this transaction is NOT  unref !!!
 */
struct cell* lookup_for_Transaction_by_req( struct s_table* hash_table, char* from, 
	char* to, char* tag, char* call_id , char* cseq_nr ,char* cseq_method )
{
   struct cell*  p_cell;
   struct cell* tmp_cell;
   int                hash_index=0;
   int                call_id_len, from_len, to_len, tag_len, cseq_nr_len, cseq_method_len;

   /* The lenght of the fields that will be comp */
   hash_index         = hash( call_id , cseq_nr ) ;
   call_id_len           = strlen(call_id);
   from_len              = strlen(from);
   to_len                   = strlen(to);
   tag_len                 = (tag)?strlen(tag):0;
   cseq_nr_len         = strlen(cseq_nr);
   cseq_method_len = strlen(cseq_method);

   //all the cells from the entry are compared
   p_cell     = hash_table->entrys[hash_index].first_cell;
   tmp_cell = 0;
   while( p_cell )
   {
      // the cell is referenceted for reading
      ref_Cell( p_cell );

      //is the wanted cell ?
      if ( p_cell->from_length == from_len && p_cell->to_length == to_len && p_cell->req_tag_length == tag_len && p_cell->call_id_length == call_id_len && p_cell->cseq_nr_length == cseq_nr_len && p_cell->cseq_method_length == cseq_method_len  )
         if ( !strcmp(p_cell->from,from) && !strcmp(p_cell->to,to) && !strncmp(p_cell->req_tag,tag,tag_len) && !strcmp(p_cell->call_id,call_id) && !strcmp(p_cell->cseq_nr,cseq_nr) && !strcmp(p_cell->cseq_method,cseq_method)  )
              return p_cell;
      //next cell
      tmp_cell = p_cell;
      p_cell = p_cell->next_cell;

      //the cell is dereferenceted
      unref_Cell ( tmp_cell );
   }

   return 0;

}





/*
 *  the method looks for a pair transaction for an ACK request. Requests don't have labels for search - the label matching tech.
 *  is nor used, only standard search. Being a ACK, a special matching for the tag attr from To header is performed.
 *  WARNING : in case of a returned transaction, this transaction is NOT  unref !!!
 */
struct cell* lookup_for_Transaction_by_ACK( struct s_table* hash_table, 
	char* from, char* to, char* tag, char* call_id, char* cseq_nr )
{
   struct cell*  p_cell;
   struct cell*  tmp_cell;
   char*            trans_tag;
   int                hash_index=0;
   int                call_id_len, from_len, to_len, tag_len, cseq_nr_len;
   int                trans_tag_len;

   /* The lenght of the fields that will be comp */
   hash_index         = hash( call_id , cseq_nr ) ;
   call_id_len           = strlen(call_id);
   from_len              = strlen(from);
   to_len                   = strlen(to);
   tag_len                 = (tag)?strlen(tag):0;
   cseq_nr_len         = strlen(cseq_nr);

   //all the cells from the entry are compared
   p_cell     = hash_table->entrys[hash_index].first_cell;
   tmp_cell = 0;
   while( p_cell )
   {
      // the cell is referenceted for reading
      ref_Cell( p_cell );

      //is the wanted cell ?
      if ( p_cell->from_length == from_len && p_cell->to_length == to_len &&p_cell->call_id_length == call_id_len && p_cell->cseq_nr_length == cseq_nr_len && p_cell->cseq_method_length == 3  )
         if ( !strcmp(p_cell->from,from) && !strcmp(p_cell->to,to) && !strcmp(p_cell->call_id,call_id) && !strcmp(p_cell->cseq_nr,cseq_nr) && !strcmp(p_cell->cseq_method,"ACK")  )
            {
             trans_tag        = p_cell->res_tag;
             trans_tag_len = p_cell->res_tag_length;
             if ( !p_cell->res_tag )
                {
                   trans_tag        = p_cell->req_tag;
                   trans_tag_len = p_cell->req_tag_length;
                }

                if ( trans_tag_len == tag_len && !strncmp(trans_tag,tag,tag_len) )
                  return p_cell;
            }
      //next cell
      tmp_cell = p_cell;
      p_cell = p_cell->next_cell;

      //the cell is dereferenceted
      unref_Cell ( tmp_cell );
   }

   return 0;

}



/*
 *  the method looks for a pair transaction for a CANCEL request. Requests don't have labels for search - the label
 *  matching tech. is nor used, only standard search. No special matching for the tag attr from To header - the request are
 *  different than ACK -> perfect To match is performed.
 *  WARNING : in case of a returned transaction, this transaction is NOT  unref !!!
 */
struct cell* lookup_for_Transaction_by_CANCEL( struct s_table* hash_table,
	char *req_uri, char* from, char* to, char* tag, char* call_id, char* cseq_nr )
{
   struct cell*  p_cell;
   struct cell* tmp_cell;
   int                hash_index=0;
   int                req_uri_len, call_id_len, from_len, to_len, tag_len, cseq_nr_len;

   /* The lenght of the fields that will be comp */
   hash_index         = hash( call_id , cseq_nr ) ;
   req_uri_len          = strlen(req_uri);
   call_id_len           = strlen(call_id);
   from_len              = strlen(from);
   to_len                   = strlen(to);
   tag_len                 = (tag)?strlen(tag):0;
   cseq_nr_len         = strlen(cseq_nr);

   //all the cells from the entry are compared
   p_cell     = hash_table->entrys[hash_index].first_cell;
   tmp_cell = 0;
   while( p_cell )
   {
      // the cell is referenceted for reading
      ref_Cell( p_cell );

      //is the wanted cell ?
      if ( p_cell->incoming_req_uri_length == req_uri_len && p_cell->from_length == from_len && p_cell->to_length == to_len && p_cell->req_tag_length == tag_len && p_cell->call_id_length == call_id_len && p_cell->cseq_nr_length == cseq_nr_len && p_cell->cseq_method_length == 6  )
         if ( !strcmp(p_cell->incoming_req_uri,req_uri) && !strcmp(p_cell->from,from) && !strcmp(p_cell->to,to) && !strcmp(p_cell->req_tag,tag) && !strcmp(p_cell->call_id,call_id) && !strcmp(p_cell->cseq_nr,cseq_nr) && !strcmp(p_cell->cseq_method,"CANCEL")  )
              return p_cell;
      //next cell
      tmp_cell = p_cell;
      p_cell = p_cell->next_cell;

      //the cell is dereferenceted
      unref_Cell ( tmp_cell );
   }

   return 0;

}




/*
 *  the method looks for a perfect transaction match for a response. The label matching tech. is  used, but also
 *  standard search if no label is present or the label is corupted. A tag matching is performed only if the original
 *  request had a tag.
 *  WARNING : in case of a returned transaction, this transaction is NOT  unref !!!
 */
struct cell* lookup_for_Transaction_by_res( struct s_table* hash_table, char* label, 
	char* from, char* to, char* tag, char* call_id, char* cseq_nr ,char* cseq_method )
{
   struct cell*  p_cell;
   struct cell* tmp_cell;
   int                hash_index=0;
   int                call_id_len, from_len, to_len, tag_len, cseq_nr_len, cseq_method_len;

   if ( label )
   {
      //the mesasge has a label -> search using label matching
      int      entry_label  = 0;
      char*  p = label;
      char*  tmp;

      //looking for the hash index  value
      for(  ; p && *p>='0' && *p<='9' ; p++ )
         hash_index = hash_index * 10 + (*p - '0')  ;

      //if the hash index is corect
      if  ( p && p!=label && hash_index<table_entries-1 )
      {
         //looking for the entry label value
         for( tmp=++p ; p && *p>='0' && *p<='9' ; p++ )
             entry_label = entry_label * 10 + (*p - '0')  ;

         //if the entry label also is corect
         if  ( tmp!=p )
         {
             //all the cells from the entry are scan to detect an entry_label matching
             p_cell     = hash_table->entrys[hash_index].first_cell;
             tmp_cell = 0;
             while( p_cell )
             {
                // the cell is referenceted for reading
                ref_Cell( p_cell );
                //is the cell with the wanted entry_label
                if ( p_cell->label = entry_label )
                   return p_cell;
                //next cell
                tmp_cell = p_cell;
                p_cell = p_cell->next_cell;

                //the cell is dereferenceted
                unref_Cell ( tmp_cell );
             }
          }
      }
   }

   //the message doesnot containe a label  or  the label is incorect-> normal seq. search.
   hash_index         = hash( call_id , cseq_nr ) ;
   call_id_len           = strlen(call_id);
   from_len              = strlen(from);
   to_len                   = strlen(to);
   tag_len                 = (tag)?strlen(tag):0;
   cseq_nr_len         = strlen(cseq_nr);
   cseq_method_len = strlen(cseq_method);

   //all the cells from the entry are compared
   p_cell     = hash_table->entrys[hash_index].first_cell;
   tmp_cell = 0;
   while( p_cell )
   {
      // the cell is referenceted for reading
      ref_Cell( p_cell );
      //is the wanted cell ?
      if ( p_cell->from_length == from_len && p_cell->to_length == to_len && p_cell->req_tag_length == tag_len && p_cell->call_id_length == call_id_len && p_cell->cseq_nr_length == cseq_nr_len && p_cell->cseq_method_length == cseq_method_len  )
         if ( !strcmp(p_cell->from,from) && !strcmp(p_cell->to,to) && !strncmp(p_cell->req_tag,tag,tag_len) && !strcmp(p_cell->call_id,call_id) && !strcmp(p_cell->cseq_nr,cseq_nr) && !strcmp(p_cell->cseq_method,cseq_method) )
            return p_cell;
      //next cell
      tmp_cell = p_cell;
      p_cell = p_cell->next_cell;

      //the cell is dereferenceted
      unref_Cell ( tmp_cell );
   }

   return 0;
}















