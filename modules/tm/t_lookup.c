/*
 * $Id$
 *
 */

#include "../../dprint.h"
#include "../../config.h"
#include "../../parser_f.h"
#include "../../ut.h"
#include "../../timer.h"
#include "hash_func.h"
#include "t_funcs.h"
#include "config.h"


#define EQ_LEN(_hf) (t_msg->_hf->body.len==p_msg->_hf->body.len)
#define EQ_STR(_hf) (memcmp(t_msg->_hf->body.s, p_msg->_hf->body.s, \
		p_msg->_hf->body.len)==0)



static int reverse_hex2int( char *c, int len )
{
	char *pc;
	int r;
	char mychar;

	r=0;
	for (pc=c+len-1; len>0; pc--, len--) {
		r <<= 4 ;
		mychar=*pc;
		if ( mychar >='0' && mychar <='9') r+=mychar -'0';
		else if (mychar >='a' && mychar <='f') r+=mychar -'a'+10;
		else if (mychar  >='A' && mychar <='F') r+=mychar -'A'+10;
		else return -1;
	}
	return r;
}

inline static int int2reverse_hex( char **c, int *size, int nr )
{
	unsigned short digit;

	if (*size && nr==0) {
		**c = '0';
		(*c)++;
		(*size)--;
		return 1;
	}

	while (*size && nr ) {
		digit = nr & 0xf ;
		**c= digit >= 10 ? digit + 'a' - 10 : digit + '0';
		nr >>= 4;
		(*c)++;
		(*size)--;
	}
	return nr ? -1 /* number not processed; too little space */ : 1;
}

/* function returns:
 *      -1 - transaction wasn't found
 *       1  - transaction found
 */
int t_lookup_request( struct sip_msg* p_msg )
{
   struct cell      *p_cell;
   struct cell      *tmp_cell;
   unsigned int  hash_index=0;
   unsigned int  isACK;
   struct sip_msg	*t_msg;

   /* parse all*/
   if (check_transaction_quadruple(p_msg)==0)
   {
      LOG(L_ERR, "ERROR: TM module: t_lookup_request: too few headers\n");
      T=0;
      /* stop processing */
      return 0;
   }

   /* start searching into the table */
   hash_index = hash( p_msg->callid->body , get_cseq(p_msg)->number ) ;
   isACK = p_msg->REQ_METHOD==METHOD_ACK;
   DBG("t_lookup_request: start searching:  hash=%d, isACK=%d\n",hash_index,isACK);

   /* lock the hole entry*/
   lock( hash_table->entrys[hash_index].mutex );

   /* all the transactions from the entry are compared */
   p_cell     = hash_table->entrys[hash_index].first_cell;
   tmp_cell = 0;
   while( p_cell )
   {
/*
		int abba;

		t_msg = p_cell->inbound_request;



		if ( EQ_LEN(from) && EQ_LEN(callid) &&
		  EQ_STR(callid) && EQ_STR(callid) &&
		  /* we compare only numerical parts of CSEQ ...
		     the method name should be the same as in first line
		  memcmp( get_cseq(t_msg)->number.s , get_cseq(p_msg)->number.s ,
				get_cseq(p_msg)->number.len ) ==0 )
		{
			if (!isACK) {
				if (t_msg->REQ_METHOD == p_msg->REQ_METHOD &&
					EQ_LEN(to) && EQ_STR(to))
					goto found;
			} else { /* ACK
				if (t_msg->REQ_METHOD == METHOD_INVITE  &&
					//p_cell->tag &&  p_cell->tag->len==p_msg->tag->body.len &&
            		//if ( /*tag memcmp( p_cell->tag->s , p_msg->tag->body.s ,
					// p_msg->tag->body.len ) ==0 )
					EQ_STR( to ) ) {
					goto found;
				}
			} /* ACK
		} /* common HFs equal
*/
     t_msg = p_cell->inbound_request;

      /* is it the wanted transaction ? */
      if ( !isACK )
      { /* is not an ACK request */
         /* first only the length are checked */
         if ( /*from length*/ EQ_LEN(from) )
            if ( /*to length*/ EQ_LEN(to) )
               if ( /*callid length*/ EQ_LEN(callid) )
                  if ( /*cseq length*/ EQ_LEN(cseq) )
                     /* so far the lengths are the same -> let's check the contents */
                        if ( /*from*/ EQ_STR(from) )
                           if ( /*to*/ EQ_STR(to) )
                               if ( /*callid*/ EQ_STR(callid) )
                                  if ( /*cseq*/ EQ_STR(cseq) )
                                     { /* WE FOUND THE GOLDEN EGG !!!! */
                                        goto found;
                                     }
      }
      else
      { /* it's a ACK request*/
         /* first only the length are checked */
         if ( /*from length*/ EQ_LEN(from) )
            //if ( /*to length*/ p_cell->inbound_request->to->body.len == p_msg->to->body.len )
               if ( /*callid length*/ EQ_LEN(callid) )
                  if ( /*cseq_nr length*/ get_cseq(t_msg)->number.len == get_cseq(p_msg)->number.len )
                      if ( /*cseq_method type*/ t_msg->first_line.u.request.method_value == METHOD_INVITE  )
                         //if ( /*tag length*/ p_cell->tag &&  p_cell->tag->len==p_msg->tag->body.len )
                            /* so far the lengths are the same -> let's check the contents */
                            if ( /*from*/ EQ_STR(from) )
                               //if ( /*to*/ !memcmp( p_cell->inbound_request->to->body.s , p_msg->to->body.s , p_msg->to->body.len)  )
                                  //if ( /*tag*/ !memcmp( p_cell->tag->s , p_msg->tag->body.s , p_msg->tag->body.len ) )
                                     if ( /*callid*/ !memcmp( t_msg->callid->body.s , p_msg->callid->body.s , p_msg->callid->body.len ) )
                                        if ( /*cseq_nr*/ !memcmp( get_cseq(t_msg)->number.s , get_cseq(p_msg)->number.s , get_cseq(p_msg)->number.len ) )
                                           { /* WE FOUND THE GOLDEN EGG !!!! */
                                              goto found;
                                           }
      }
      /* next transaction */
      tmp_cell = p_cell;
      p_cell = p_cell->next_cell;
   } /* synonym loop */

   /* no transaction found */
   T = 0;
   unlock( hash_table->entrys[hash_index].mutex );
   DBG("DEBUG: t_lookup_request: no transaction found\n");
   return -1;

found:
   T=p_cell;
   T_REF( T );
   DBG("DEBUG:XXXXXXXXXXXXXXXXXXXXX t_lookup_request: "
                   "transaction found ( T=%p , ref=%x)\n",T,T->ref_bitmap);
   unlock( hash_table->entrys[hash_index].mutex );
   return 1;
}




/* function returns:
 *       0 - transaction wasn't found
 *       T - transaction found
 */
struct cell* t_lookupOriginalT(  struct s_table* hash_table , struct sip_msg* p_msg )
{
   struct cell      *p_cell;
   struct cell      *tmp_cell;
   unsigned int  hash_index=0;

   /* it's a CANCEL request for sure */

   /* start searching into the table */
   hash_index = hash( p_msg->callid->body , get_cseq(p_msg)->number  ) ;
   DBG("DEBUG: t_lookupOriginalT: searching on hash entry %d\n",hash_index );

   /* all the transactions from the entry are compared */
   p_cell     = hash_table->entrys[hash_index].first_cell;
   tmp_cell = 0;
   while( p_cell )
   {
      /* is it the wanted transaction ? */
      /* first only the length are checked */
      if ( /*from length*/ p_cell->inbound_request->from->body.len == p_msg->from->body.len )
         if ( /*to length*/ p_cell->inbound_request->to->body.len == p_msg->to->body.len )
            //if ( /*tag length*/ (!p_cell->inbound_request->tag && !p_msg->tag) || (p_cell->inbound_request->tag && p_msg->tag && p_cell->inbound_request->tag->body.len == p_msg->tag->body.len) )
               if ( /*callid length*/ p_cell->inbound_request->callid->body.len == p_msg->callid->body.len )
                  if ( /*cseq_nr length*/ get_cseq(p_cell->inbound_request)->number.len == get_cseq(p_msg)->number.len )
                      if ( /*cseq_method type*/ p_cell->inbound_request->REQ_METHOD!=METHOD_CANCEL )
                         if ( /*req_uri length*/ p_cell->inbound_request->first_line.u.request.uri.len == p_msg->first_line.u.request.uri.len )
                             /* so far the lengths are the same -> let's check the contents */
                             if ( /*from*/ memcmp( p_cell->inbound_request->from->body.s , p_msg->from->body.s , p_msg->from->body.len )==0 )
                                if ( /*to*/ memcmp( p_cell->inbound_request->to->body.s , p_msg->to->body.s , p_msg->to->body.len)==0  )
                                   //if ( /*tag*/ (!p_cell->inbound_request->tag && !p_msg->tag) || (p_cell->inbound_request->tag && p_msg->tag && memcmp( p_cell->inbound_request->tag->body.s , p_msg->tag->body.s , p_msg->tag->body.len )==0) )
                                      if ( /*callid*/ memcmp( p_cell->inbound_request->callid->body.s , p_msg->callid->body.s , p_msg->callid->body.len )==0 )
                                          if ( /*cseq_nr*/ memcmp( get_cseq(p_cell->inbound_request)->number.s , get_cseq(p_msg)->number.s , get_cseq(p_msg)->number.len )==0 )
                                             if ( /*req_uri*/ memcmp( p_cell->inbound_request->first_line.u.request.uri.s , p_msg->first_line.u.request.uri.s , p_msg->first_line.u.request.uri.len )==0 )
                                             { /* WE FOUND THE GOLDEN EGG !!!! */
                                                return p_cell;
                                             }
      /* next transaction */
      tmp_cell = p_cell;
      p_cell = p_cell->next_cell;
   }

   /* no transaction found */
   return 0;
}




/* Returns 0 - nothing found
  *              1  - T found
  */
int t_reply_matching( struct sip_msg *p_msg , unsigned int *p_branch )
{
   struct cell*  p_cell;
   unsigned int hash_index = 0;
   unsigned int entry_label  = 0;
   unsigned int branch_id    = 0;
   char  *hashi, *syni, *branchi, *p, *n;
   int hashl, synl, branchl;
   int scan_space;

   /* split the branch into pieces: loop_detection_check(ignored),
      hash_table_id, synonym_id, branch_id*/

   if (! ( p_msg->via1 && p_msg->via1->branch && p_msg->via1->branch->value.s) )
	goto nomatch2;

   p=p_msg->via1->branch->value.s;
   scan_space=p_msg->via1->branch->value.len;

   /* loop detection ... ignore */
   n=eat_token2_end( p, p+scan_space, BRANCH_SEPARATOR );
   scan_space-=n-p;
   if (n==p || scan_space<2 || *n!=BRANCH_SEPARATOR) goto nomatch2;
   p=n+1; scan_space--;

   /* hash_id */
   n=eat_token2_end( p, p+scan_space, BRANCH_SEPARATOR);
   hashl=n-p;
   scan_space-=hashl;
   if (!hashl || scan_space<2 || *n!=BRANCH_SEPARATOR) goto nomatch2;
   hashi=p;
   p=n+1;scan_space--;


   /* sequence id */
   n=eat_token2_end( p, p+scan_space, BRANCH_SEPARATOR);
   synl=n-p;
   scan_space-=synl;
   if (!synl || scan_space<2 || *n!=BRANCH_SEPARATOR) goto nomatch2;
   syni=p;
   p=n+1;scan_space--;

   /* branch id */  /*  should exceed the scan_space */
   n=eat_token_end( p, p+scan_space );
   branchl=n-p;
   if (!branchl ) goto nomatch2;
   branchi=p;


   hash_index=reverse_hex2int(hashi, hashl);
   entry_label=reverse_hex2int(syni, synl);
   branch_id=reverse_hex2int(branchi, branchl);
	if (hash_index==-1 || entry_label==-1 || branch_id==-1) {
		DBG("DEBUG: t_reply_matching: poor reply lables %d label %d branch %d\n",
			hash_index, entry_label, branch_id );
		goto nomatch2;
	}


   DBG("DEBUG: t_reply_matching: hash %d label %d branch %d\n",
	hash_index, entry_label, branch_id );

   /* sanity check */
   if (hash_index<0 || hash_index >=TABLE_ENTRIES ||
    entry_label<0 || branch_id<0 || branch_id>=MAX_FORK ) {
          DBG("DBG: t_reply_matching: snaity check failed\n");
         goto nomatch2;
   }

   /* lock the hole entry*/
   lock( hash_table->entrys[hash_index].mutex );

   /*all the cells from the entry are scan to detect an entry_label matching */
   p_cell     = hash_table->entrys[hash_index].first_cell;
   while( p_cell )
   {
      /* is it the cell with the wanted entry_label? */
      if ( p_cell->label == entry_label )
         /* has the transaction the wanted branch? */
         if ( p_cell->nr_of_outgoings>branch_id && p_cell->outbound_request[branch_id] )
         {/* WE FOUND THE GOLDEN EGG !!!! */
             T = p_cell;
             *p_branch = branch_id;
             T_REF( T );
             unlock( hash_table->entrys[hash_index].mutex );
             DBG("DEBUG:XXXXXXXXXXXXXXXXXXXXX t_reply_matching: reply matched (T=%p, ref=%x)!\n",T,T->ref_bitmap);
            return 1;
         }
      /* next cell */
      p_cell = p_cell->next_cell;
   } /* while p_cell */

   /* nothing found */
   DBG("DEBUG: t_reply_matching: no matching transaction exists\n");

nomatch:
   unlock( hash_table->entrys[hash_index].mutex );
nomatch2:
   DBG("DEBUG: t_reply_matching: failure to match a transaction\n");
   *p_branch = -1;
   T = 0;
   return -1;
}




/* Functions update T (T gets either a valid pointer in it or it equals zero) if no transaction
  * for current message exists;
  * it returns 1 if found, 0 if not found, -1 on error
  */
int t_check( struct sip_msg* p_msg , int *param_branch)
{
	int local_branch;

	/* is T still up-to-date ? */
	DBG("DEBUG: t_check : msg id=%d , global msg id=%d , T on entrance=%p\n", 
		p_msg->id,global_msg_id,T);
	if ( p_msg->id != global_msg_id || T==T_UNDEFINED )
	{
		global_msg_id = p_msg->id;
		T = T_UNDEFINED;
		/* transaction lookup */
		if ( p_msg->first_line.type==SIP_REQUEST ) {

			/* force parsing all the needed headers*/
			if (parse_headers(p_msg, HDR_EOH )==-1)
				return -1;
		t_lookup_request( p_msg );
	 	} else {
		 	if ( parse_headers(p_msg, HDR_VIA1|HDR_VIA2|HDR_TO|HDR_CSEQ )==-1 ||
			!p_msg->via1 || !p_msg->via2 || !p_msg->to || !p_msg->cseq )
			return -1;
		t_reply_matching( p_msg , ((param_branch!=0)?(param_branch):(&local_branch)) );
		}
#		ifdef EXTRA_DEBUG
		if ( T && T!=T_UNDEFINED && T->damocles) {
			LOG( L_ERR, "ERROR: transaction %p scheduled for deletion "
				"and called from t_check\n", T);
			abort();
		}
#		endif
		DBG("DEBUG: t_check : msg id=%d , global msg id=%d , T on finish=%p\n",
			p_msg->id,global_msg_id,T);
	} else {
		if (T)
			DBG("DEBUG: t_check: T alredy found!\n");
		else
			DBG("DEBUG: t_check: T previously sought and not found\n");
	}

	return ((T)?1:0) ;
}



/* append appropriate branch labels for fast reply-transaction matching
   to outgoing requests
*/
int add_branch_label( struct cell *trans, struct sip_msg *p_msg, int branch )
{
	char *c;

	char *begin;
	unsigned int size, orig_size, n;

	begin=p_msg->add_to_branch_s+p_msg->add_to_branch_len;
	orig_size = size=MAX_BRANCH_PARAM_LEN - p_msg->add_to_branch_len;

	if (size) { *begin=BRANCH_SEPARATOR; begin++; size--; } else return -1;
	if (int2reverse_hex( &begin, &size, trans->hash_index)==-1) return -1;
	if (size) { *begin=BRANCH_SEPARATOR; begin++; size--; } else return -1;
	if (int2reverse_hex( &begin, &size, trans->label)==-1) return -1;
	if (size) { *begin=BRANCH_SEPARATOR; begin++; size--; } else return -1;
	if (int2reverse_hex( &begin, &size, branch)==-1) return -1;

	p_msg->add_to_branch_len+=(orig_size-size);
	DBG("DEBUG: XXX branch label created now: %*s (%d)\n",
		p_msg->add_to_branch_len, p_msg->add_to_branch_s );
	return 0;

}

