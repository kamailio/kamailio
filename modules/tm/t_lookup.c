/*
 * $Id$
 *
 * This C-file takes care of matching requests and replies with
 * existing transactions. Note that we do not do SIP-compliant
 * request matching as asked by SIP spec. We do bitwise matching of 
 * all header fields in requests which form a transaction key. 
 * It is much faster and it worx pretty well -- we haven't 
 * had any interop issue neither in lab nor in bake-offs. The reason
 * is that retransmissions do look same as original requests
 * (it would be really silly if they wuld be mangled). The only
 * exception is we parse To as To in ACK is compared to To in
 * reply and both  of them are constructed by different software.
 * 
 * As for reply matching, we match based on branch value -- that is
 * faster too. There are two versions .. with SYNONYMs #define
 * enabled, the branch includes ordinal number of a transaction
 * in a synonym list in hash table and is somewhat faster but
 * not reboot-resilient. SYNONYMs turned off are little slower
 * but work across reboots as well.
 *
 * The branch parameter is formed as follows:
 * SYNONYMS  on: hash.synonym.branch
 * SYNONYMS off: md5.hash.branch
 *
 * -jiri
 *
 */

#include <assert.h>
#include "../../dprint.h"
#include "../../config.h"
#include "../../parser/parser_f.h"
#include "../../ut.h"
#include "../../timer.h"
#include "hash_func.h"
#include "t_funcs.h"
#include "config.h"
#include "sip_msg.h"


#define EQ_LEN(_hf) (t_msg->_hf->body.len==p_msg->_hf->body.len)
#define EQ_STR(_hf) (memcmp(t_msg->_hf->body.s,\
	translate_pointer(p_msg->orig,p_msg->buf,p_msg->_hf->body.s), \
	p_msg->_hf->body.len)==0)
#define EQ_REQ_URI_LEN\
	(p_msg->first_line.u.request.uri.len==t_msg->first_line.u.request.uri.len)
#define EQ_REQ_URI_STR\
	( memcmp( t_msg->first_line.u.request.uri.s,\
	translate_pointer(p_msg->orig,p_msg->buf,p_msg->first_line.u.request.uri.s),\
	p_msg->first_line.u.request.uri.len)==0)
#define EQ_VIA_LEN(_via)\
	( (p_msg->via1->bsize-(p_msg->_via->name.s-(p_msg->_via->hdr.s+p_msg->_via->hdr.len)))==\
	(t_msg->via1->bsize-(t_msg->_via->name.s-(t_msg->_via->hdr.s+t_msg->_via->hdr.len))) )

#define EQ_VIA_STR(_via)\
	( memcmp( t_msg->_via->name.s,\
	 translate_pointer(p_msg->orig,p_msg->buf,p_msg->_via->name.s),\
	 (t_msg->via1->bsize-(t_msg->_via->name.s-(t_msg->_via->hdr.s+t_msg->_via->hdr.len)))\
	)==0 )


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
int t_lookup_request( struct sip_msg* p_msg , int leave_new_locked )
{
	struct cell         *p_cell;
	struct cell         *tmp_cell;
	unsigned int       isACK;
	struct sip_msg  *t_msg;

	/* parse all*/
	if (check_transaction_quadruple(p_msg)==0)
	{
		LOG(L_ERR, "ERROR: TM module: t_lookup_request: too few headers\n");
		T=0;
		/* stop processing */
		return 0;
	}

	/* start searching into the table */
	p_msg->hash_index=hash( p_msg->callid->body , get_cseq(p_msg)->number ) ;
	isACK = p_msg->REQ_METHOD==METHOD_ACK;
	DBG("t_lookup_request: start searching: hash=%d, isACK=%d\n",
		p_msg->hash_index,isACK);

	/* lock the hole entry*/
	lock(&(hash_table->entrys[p_msg->hash_index].mutex));

	/* all the transactions from the entry are compared */
	p_cell   = hash_table->entrys[p_msg->hash_index].first_cell;
	tmp_cell = 0;
	while( p_cell )
	{
		t_msg = p_cell->uas.request;

		/* is it the wanted transaction ? */
		if ( !isACK )
		{ /* is not an ACK request */
			/* first only the length are checked */
			if ( /*callied*/EQ_LEN(callid) && /*cseq*/EQ_LEN(cseq)
			&& /*req URI*/EQ_REQ_URI_LEN && /*VIA*/EQ_VIA_LEN(via1)
			&& /*from*/EQ_LEN(from) && /*to*/EQ_LEN(to)  )
				/* so far the lengths are the same
				-> let's check the contents */
				if ( /*callid*/EQ_STR(callid) && /*cseq*/EQ_STR(cseq)
				&& /*req URI*/EQ_REQ_URI_STR && /*VIA*/EQ_VIA_STR(via1)
				&& /*from*/EQ_STR(from) && /*to*/EQ_STR(to) )
					{ /* WE FOUND THE GOLDEN EGG !!!! */
						goto found;
					}
		}
		else
		{ /* it's a ACK request*/
			/* first only the length are checked */
			/* use shortcut; -jiri
			if ( t_msg->first_line.u.request.method_value==METHOD_INVITE */
			if (t_msg->REQ_METHOD==METHOD_INVITE
			/* && (fprintf(stderr,"------Method name OK->testing callid len...\n")) */
			&& /*callid length*/ EQ_LEN(callid)
			/* && (fprintf(stderr,"------CallID OK -> testing cseq nr len\n")) */
			&& get_cseq(t_msg)->number.len==get_cseq(p_msg)->number.len
			/* && (fprintf(stderr,"------Cseq nr OK -> testing from len\n")) */
			&& /*from length*/ EQ_LEN(from)
			/* && (fprintf(stderr,"------from OK -> testing To uri len\n")) */
			&& /*to uri*/get_to(t_msg)->uri.len==get_to(p_msg)->uri.len
			/* && (fprintf(stderr,"------To uri OK -> testing To tag len\n")) */
			&& /*to tag*/p_cell->uas.tag->len==get_to(p_msg)->tag_value.len
			/* && (fprintf(stderr,"------To tag OK -> testing uri len\n")) */

			/* in ACKs to 200, r-uri and Via may be different than in
			   original INVITE; we still try to match the transaction
			   so that we can retransmit an ACK on resent 200 -- different
			   from SIP spec which kills transaction state after INVITE-200
			   and considers 200-ACK a new transaction which just happens
			   to have the same CSeq. -jiri
			*/

			&& /*req URI*/(p_cell->uas.status==200 || EQ_REQ_URI_LEN )
			/* && (fprintf(stderr,"------uri OK -> testing via len\n")) */
			&& /*VIA*/(p_cell->uas.status==200 || EQ_VIA_LEN(via1)) )
				/* so far the lengths are the same
				-> let's check the contents */
				if ( /* fprintf(stderr,"------callid |%.*s| |%.*s|\n",
					p_msg->callid->body.len,p_msg->callid->body.s,
					t_msg->callid->body.len,t_msg->callid->body.s)
				&& */ /*callid*/!memcmp( t_msg->callid->body.s,
					p_msg->callid->body.s,p_msg->callid->body.len)
				/* && fprintf(stderr,"------cseq |%.*s| |%.*s|\n",
					get_cseq(p_msg)->number.len,get_cseq(p_msg)->number.s,
					get_cseq(t_msg)->number.len,get_cseq(t_msg)->number.s) */
				&& /*cseq nr*/!memcmp(get_cseq(t_msg)->number.s,
					get_cseq(p_msg)->number.s,get_cseq(p_msg)->number.len)
				/* &&  fprintf(stderr,"------from |%.*s| |%.*s|\n",
					p_msg->from->body.len, translate_pointer(p_msg->orig,
						p_msg->buf,p_msg->from->body.s),
					t_msg->from->body.len,t_msg->from->body.s) */
				&& /*from*/EQ_STR(from)
				/* && fprintf(stderr,"------to uri |%.*s| |%.*s|\n",
					get_to(p_msg)->uri.len,get_to(p_msg)->uri.s,
					get_to(t_msg)->uri.len,get_to(t_msg)->uri.s) */
				&& /*to uri*/!memcmp(get_to(t_msg)->uri.s,
					get_to(p_msg)->uri.s,get_to(t_msg)->uri.len)
				/* && fprintf(stderr,"------to tag |%.*s| |%.*s|\n",
                    get_to(p_msg)->tag_value.len,get_to(p_msg)->tag_value.s,
                    p_cell->uas.tag->len, p_cell->uas.tag->s) */
				&& /*to tag*/!memcmp(p_cell->uas.tag->s,
					get_to(p_msg)->tag_value.s,p_cell->uas.tag->len)
				/* && fprintf(stderr,"------URI %d |%.*s| |%.*s|\n",
					p_cell->uas.status,p_msg->first_line.u.request.uri.len,
					translate_pointer(p_msg->orig, p_msg->buf,
						p_msg->first_line.u.request.uri.s),
					t_msg->first_line.u.request.uri.len,
					t_msg->first_line.u.request.uri.s) */
				&& /*req URI*/(p_cell->uas.status==200 || EQ_REQ_URI_STR)
				/* && fprintf(stderr,"------VIA %d |%.*s| |%.*s|\n",
					p_cell->uas.status, 
					(p_msg->via1->bsize-(p_msg->via1->name.s-
						(p_msg->via1->hdr.s+p_msg->via1->hdr.len))),
					translate_pointer(p_msg->orig,p_msg->buf,
						p_msg->via1->name.s),
                    (t_msg->via1->bsize-(t_msg->via1->name.s-
						(t_msg->via1->hdr.s+t_msg->via1->hdr.len))),
					t_msg->via1->name.s) */
				&& /*VAI*/(p_cell->uas.status==200 ||EQ_VIA_STR(via1)) )
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
	if (!leave_new_locked)
		unlock(&(hash_table->entrys[p_msg->hash_index].mutex));
	DBG("DEBUG: t_lookup_request: no transaction found\n");
	/* DON'T FORGET TO REMOVE IT!!!!! bogdan */
	//if (isACK) assert(0);
	return -1;

found:
	T=p_cell;
	T_REF( T );
	DBG("DEBUG: t_lookup_request: transaction found (T=%p , ref=%x)\n",
		T,T->ref_bitmap);
	unlock(&(hash_table->entrys[p_msg->hash_index].mutex));
	return 1;
}




/* function returns:
 *       0 - transaction wasn't found
 *       T - transaction found
 */
struct cell* t_lookupOriginalT(  struct s_table* hash_table ,
													struct sip_msg* p_msg )
{
	struct cell     *p_cell;
	struct cell     *tmp_cell;
	unsigned int     hash_index=0;
	struct sip_msg  *t_msg=0;


	/* start searching into the table */
	hash_index = p_msg->hash_index;
	DBG("DEBUG: t_lookupOriginalT: searching on hash entry %d\n",hash_index );

	/* all the transactions from the entry are compared */
	p_cell   = hash_table->entrys[hash_index].first_cell;
	tmp_cell = 0;
	while( p_cell )
	{
		t_msg = p_cell->uas.request;

		/* is it the wanted transaction ? */
		/* first only the length are checked */
		if ( /* fprintf(stderr,"starting\n")  && */ p_cell->uas.request->REQ_METHOD!=METHOD_CANCEL
			/* && fprintf(stderr,"checking callid length....\n") */
			&& /*callid length*/ EQ_LEN(callid)
			/* && fprintf(stderr,"OK. checking cseg nr len....\n")	 */
			&& get_cseq(t_msg)->number.len==get_cseq(p_msg)->number.len
			/* && fprintf(stderr,"OK. checking REQ_URI len.... \n") */
			&& EQ_REQ_URI_LEN
			/* && fprintf(stderr,"OK. checking VIA %d %d....\n",
				(p_msg->via1->bsize-(p_msg->via1->name.s-
					(p_msg->via1->hdr.s+p_msg->via1->hdr.len))),
				(t_msg->via1->bsize-(t_msg->via1->name.s-
					(t_msg->via1->hdr.s+t_msg->via1->hdr.len)))) */
            /* && fprintf(stderr,"OK. VIA |%.*s| |%.*s|\n",
                (p_msg->via1->bsize-(p_msg->via1->name.s-
                     (p_msg->via1->hdr.s+p_msg->via1->hdr.len))),
                translate_pointer(p_msg->orig,p_msg->buf,
                       p_msg->via1->name.s),
                (t_msg->via1->bsize-(t_msg->via1->name.s-
                      (t_msg->via1->hdr.s+t_msg->via1->hdr.len))),
                t_msg->via1->name.s) */
			&& EQ_VIA_LEN(via1) 
			/* && fprintf(stderr,"OK. checking FROM len... \n") */
			&& EQ_LEN(from)
			/* && fprintf(stderr,"OK. checking TO len... \n") */
			&& EQ_LEN(to)
			/* && fprintf(stderr,"OK\n") */ )
				/* so far the lengths are the same
				 let's check the contents */
				if (
                	/* fprintf(stderr,"checking callid |%.*s| |%.*s|\n",
                    	p_msg->callid->body.len, translate_pointer(p_msg->orig,
                        	p_msg->buf,p_msg->callid->body.s),
                    	t_msg->callid->body.len,t_msg->callid->body.s) 
					&& *//*callid*/ EQ_STR(callid)
					/* && fprintf(stderr,"OK. cseq nr |%.*s| |%.*s|\n",
						get_cseq(p_msg)->number.len,get_cseq(p_msg)->number.s,
						get_cseq(t_msg)->number.len,get_cseq(t_msg)->number.s) */
					&& /*cseq_nr*/ !memcmp(get_cseq(t_msg)->number.s,
						get_cseq(p_msg)->number.s,get_cseq(p_msg)->number.len)
	                /* && fprintf(stderr,"OK. URI %d |%.*s| |%.*s|\n",
                    	p_cell->uas.status,p_msg->first_line.u.request.uri.len,
                    	translate_pointer(p_msg->orig, p_msg->buf,
                        	p_msg->first_line.u.request.uri.s),
                    	t_msg->first_line.u.request.uri.len,
                    	t_msg->first_line.u.request.uri.s) */
					&& EQ_REQ_URI_STR
	                /* && fprintf(stderr,"OK. VIA |%.*s| |%.*s|\n",
        	            (p_msg->via1->bsize-(p_msg->via1->name.s-
            	            (p_msg->via1->hdr.s+p_msg->via1->hdr.len))),
                	    translate_pointer(p_msg->orig,p_msg->buf,
                    	    p_msg->via1->name.s),
           				(t_msg->via1->bsize-(t_msg->via1->name.s-
                    	    (t_msg->via1->hdr.s+t_msg->via1->hdr.len))),
						t_msg->via1->name.s) */
					&& EQ_VIA_STR(via1)
	                /* && fprintf(stderr,"OK. from |%.*s| |%.*s|\n",
                        p_msg->from->body.len, translate_pointer(p_msg->orig,
                            p_msg->buf,p_msg->from->body.s),
                        t_msg->from->body.len,t_msg->from->body.s) */
					&& EQ_STR(from)
					/* && fprintf(stderr,"OK\n") */ )
					{ /* WE FOUND THE GOLDEN EGG !!!! */
						DBG("DEBUG: t_lookupOriginalT: canceled transaction"
							" found (%p)! \n",p_cell );
						return p_cell;
					}
		/* next transaction */
		tmp_cell = p_cell;
		p_cell = p_cell->next_cell;
	}

	/* no transaction found */
	DBG("DEBUG: t_lookupOriginalT: no CANCEL maching found! \n" );
	return 0;
}




/* Returns 0 - nothing found
 *         1  - T found
 */
int t_reply_matching( struct sip_msg *p_msg , int *p_branch ,
											 int *local_cancel)
{
	struct cell*  p_cell;
	int hash_index   = 0;
	int entry_label  = 0;
	int branch_id    = 0;
	char  *hashi, *branchi, *p, *n;
	int hashl, branchl;
	int scan_space;
#ifndef USE_SYNONIM
	char *loopi;
	int loopl;
#else
	char *syni;
	int synl;
#endif

	/* split the branch into pieces: loop_detection_check(ignored),
	 hash_table_id, synonym_id, branch_id */

	if (!(p_msg->via1 && p_msg->via1->branch && p_msg->via1->branch->value.s))
		goto nomatch2;

	p=p_msg->via1->branch->value.s;
	scan_space=p_msg->via1->branch->value.len;

#ifndef USE_SYNONIM
	/* loop detection ... ignore */
	n=eat_token2_end( p, p+scan_space, BRANCH_SEPARATOR );
	loopl = n-p;
	scan_space-= loopl;
	if (n==p || scan_space<2 || *n!=BRANCH_SEPARATOR) goto nomatch2;
	loopi=p;
	p=n+1; scan_space--;
#endif

	/* hash_id */
	n=eat_token2_end( p, p+scan_space, BRANCH_SEPARATOR);
	hashl=n-p;
	scan_space-=hashl;
	if (!hashl || scan_space<2 || *n!=BRANCH_SEPARATOR) goto nomatch2;
	hashi=p;
	p=n+1;scan_space--;

#ifdef USE_SYNONIM
	/* sequence id */
	n=eat_token2_end( p, p+scan_space, BRANCH_SEPARATOR);
	synl=n-p;
	scan_space-=synl;
	if (!synl || scan_space<2 || *n!=BRANCH_SEPARATOR) goto nomatch2;
	syni=p;
	p=n+1;scan_space--;
#endif

	/* branch id  -  should exceed the scan_space */
	n=eat_token_end( p, p+scan_space );
	branchl=n-p;
	if (!branchl ) goto nomatch2;
	branchi=p;

	/* sanity check */
	if ((hash_index=reverse_hex2int(hashi, hashl))<0||hash_index>=TABLE_ENTRIES
		|| (branch_id=reverse_hex2int(branchi, branchl))<0||branch_id>=MAX_FORK
#ifdef USE_SYNONIM
		|| (entry_label=reverse_hex2int(syni, synl))<0
#else
		|| loopl!=MD5_LEN
#endif
	) {
		DBG("DEBUG: t_reply_matching: poor reply lables %d label %d "
			"branch %d\n",hash_index, entry_label, branch_id );
		goto nomatch2;
	}


	DBG("DEBUG: t_reply_matching: hash %d label %d branch %d\n",
		hash_index, entry_label, branch_id );


	/* search the hash table list at entry 'hash_index'; lock the
	   entry first 
	*/
	lock(&(hash_table->entrys[hash_index].mutex));
	for (p_cell = hash_table->entrys[hash_index].first_cell; p_cell; 
		p_cell=p_cell->next_cell) {

		/* does method match ? */
		if (get_cseq(p_msg)->method.len==
			  get_cseq(p_cell->uas.request)->method.len 
			&& get_cseq(p_msg)->method.s[0]==
			  get_cseq(p_cell->uas.request)->method.s[0]) {
				*local_cancel=0;
		/* or is it perhaps a CANCEL ? */
		} else if ( p_cell->uas.request->REQ_METHOD==METHOD_INVITE 
			&& get_cseq(p_msg)->method.len==CANCEL_LEN 
			&& memcmp( get_cseq(p_msg)->method.s, CANCEL, CANCEL_LEN )==0 
			&& p_cell->uac[branch_id].request.cancel!=NO_CANCEL 
			&& p_cell->uac[branch_id].request.cancel!=EXTERNAL_CANCEL ) {
				*local_cancel=1;
		} else { /* method mismatched */
			continue;
		};
		#ifdef USE_SYNONIM
		if (p_cell->label != entry_label) 
			continue;
		#else
		if ( p_cell->uas.request->add_to_branch_len<MD5_LEN 
			 || memcmp(p_cell->uas.request->add_to_branch_s,loopi,MD5_LEN)!=0)
				continue;
		#endif
		/* sanity check ... too high branch ? */
		if ( branch_id>=p_cell->nr_of_outgoings )
			continue;
		/* we passed all disqualifying factors .... the transaction has been
		   matched !
		*/
		T=p_cell;
		*p_branch = branch_id;
		T_REF( T );
		unlock(&(hash_table->entrys[hash_index].mutex));
		DBG("DEBUG: t_reply_matching: reply matched (T=%p,ref=%x)!\n",
			T,T->ref_bitmap);
		return 1;
	} /* for cycle */

	/* nothing found */
	unlock(&(hash_table->entrys[hash_index].mutex));
	DBG("DEBUG: t_reply_matching: no matching transaction exists\n");

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
int t_check( struct sip_msg* p_msg , int *param_branch, int *param_cancel)
{
	int local_branch;
	int local_cancel;

	/* is T still up-to-date ? */
	DBG("DEBUG: t_check: msg id=%d global id=%d T start=%p\n", 
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
			t_lookup_request( p_msg , 0 /* unlock before returning */ );
		} else {
			if ( parse_headers(p_msg, HDR_VIA1|HDR_TO|HDR_CSEQ )==-1
			|| !p_msg->via1 || !p_msg->to || !p_msg->cseq )
				return -1;
			t_reply_matching( p_msg ,
				((param_branch!=0)?(param_branch):(&local_branch)),
				((param_cancel!=0)?(param_cancel):(&local_cancel)));

		}
#ifdef EXTRA_DEBUG
		if ( T && T!=T_UNDEFINED && T->damocles) {
			LOG( L_ERR, "ERROR: transaction %p scheduled for deletion "
				"and called from t_check\n", T);
			abort();
		}
#endif
		DBG("DEBUG: t_check: msg id=%d global id=%d T end=%p\n",
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
	char *begin;
	int size, orig_size;

	/* this is actually a hack made by Bogdan; I wanted to have a structure
	   to which anybody can append some branch stuff which may be utilizied
	   during reply processing; Bogdan ignored that and resets it all the
	   time to construct multiple branches for multiple via's during
	   forking (otherwise, the next branch would be now appended to
	   previous branch)

	   keywords: HACK
	*/
     	
	p_msg->add_to_branch_len = 0; /*bogdan*/


	begin=p_msg->add_to_branch_s+p_msg->add_to_branch_len;
	orig_size = size=MAX_BRANCH_PARAM_LEN - p_msg->add_to_branch_len;

#ifndef USE_SYNONIM
	if (memcpy(begin,trans->md5,MD5_LEN)) {begin+=MD5_LEN;size-=MD5_LEN;} else return -1;
	if (size) { *begin=BRANCH_SEPARATOR; begin++; size--; } else return -1;
#endif
	if (int2reverse_hex( &begin, &size, trans->hash_index)==-1) return -1;
#ifdef USE_SYNONIM
	if (size) { *begin=BRANCH_SEPARATOR; begin++; size--; } else return -1;
	if (int2reverse_hex( &begin, &size, trans->label)==-1) return -1;
#endif
	if (size) { *begin=BRANCH_SEPARATOR; begin++; size--; } else return -1;
	if (int2reverse_hex( &begin, &size, branch)==-1) return -1;

	p_msg->add_to_branch_len+=(orig_size-size);
	DBG("DEBUG: XXX branch label created now: %.*s\n",
		p_msg->add_to_branch_len, p_msg->add_to_branch_s );
	return 0;

}




/* atomic "add_if_new" construct; it returns:
	AIN_ERROR	if a fatal error (e.g, parsing) occured
	AIN_RETR	it's a retransmission
	AIN_NEW		it's a new request
	AIN_NEWACK	it's an ACK for which no transaction exists
	AIN_OLDACK	it's an ACK for an existing transaction
*/
enum addifnew_status t_addifnew( struct sip_msg* p_msg )
{

	int ret, lret;
	struct cell *new_cell;

	/* is T still up-to-date ? */
	DBG("DEBUG: t_addifnew: msg id=%d , global msg id=%d ,"
		" T on entrance=%p\n",p_msg->id,global_msg_id,T);
	if ( p_msg->id != global_msg_id || T==T_UNDEFINED 
		/* if someone tried to do something previously by mistake with
		   a transaction which did not exist yet, try to look-up
		   the transacion too */
		|| T==T_NULL)
	{
		global_msg_id = p_msg->id;
		T = T_UNDEFINED;
		/* transaction lookup */
		/* force parsing all the needed headers*/
		if (parse_headers(p_msg, HDR_EOH )==-1)
			return AIN_ERROR;
		lret = t_lookup_request( p_msg, 1 /* leave locked */ );
		if (lret==0) return AIN_ERROR;
		if (lret==-1) {
			/* transaction not found, it's a new request */
			if ( p_msg->REQ_METHOD==METHOD_ACK ) {
				ret=AIN_NEWACK;
			} else {
				/* add new transaction */
				new_cell = build_cell( p_msg ) ;
				if  ( !new_cell ){
					LOG(L_ERR, "ERROR: t_addifnew: out of mem:\n");
					ret = AIN_ERROR;
				} else {
					insert_into_hash_table_unsafe( hash_table , new_cell );
					ret = AIN_NEW;
					T=new_cell;
					T_REF(T);
				}
			}
			unlock(&(hash_table->entrys[p_msg->hash_index].mutex));
			return ret;
		} else {
			/* tramsaction found, it's a retransmission  or ACK */
			if (p_msg->REQ_METHOD!=METHOD_ACK)
				return AIN_RETR;
			else {
				if (T->uas.isACKed)
					return AIN_RTRACK;
				else
					return AIN_OLDACK;
				}
			//return p_msg->REQ_METHOD==METHOD_ACK ? AIN_OLDACK : AIN_RETR;
		}
	} else {
		if (T)
			LOG(L_ERR, "ERROR: t_addifnew: already "
			"processing this message, T found!\n");
		else
			LOG(L_ERR, "ERROR: t_check_new_request: already "
			"processing this message, T not found!\n");
		return AIN_ERROR;
	}
}



