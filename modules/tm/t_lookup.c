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
 * SYNONYMS off: hash.md5.branch
 *
 * -jiri
 *
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
 *
 * History:
 * ----------
 * 2003-01-23 options for disabling r-uri matching introduced
 */


#include "defs.h"


#include <assert.h>
#include "../../dprint.h"
#include "../../config.h"
#include "../../parser/parser_f.h"
#include "../../ut.h"
#include "../../timer.h"
#include "../../hash_func.h"
#include "../../globals.h"
#include "../../forward.h"
#include "t_funcs.h"
#include "config.h"
#include "sip_msg.h"
#include "t_hooks.h"
#include "t_lookup.h"


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

#define HF_LEN(_hf) ((_hf)->body.s+(_hf)->body.len-(_hf)->name.s)

/* should be request-uri matching used as a part of pre-3261 
 * transaction matching, as the standard wants us to do so
 * (and is reasonable to do so, to be able to distinguish
 * spirals)? turn only off for better interaction with 
 * devices that are broken and send different r-uri in
 * CANCEL/ACK than in original INVITE
 */
int ruri_matching=1;

/* presumably matching transaction for an e2e ACK */
static struct cell *t_ack;

/* this is a global variable which keeps pointer to
   transaction currently processed by a process; it it
   set by t_lookup_request or t_reply_matching; don't
   dare to change it anywhere else as it would
   break ref_counting
*/

#ifdef _OBSOLETED
struct cell      *T;
#endif

static struct cell *T;

/* number of currently processed message; good to know
   to be able to doublecheck whether we are still working
   on a current transaction or a new message arrived;
   don't even think of changing it
*/
unsigned int     global_msg_id;

struct cell *get_t() { return T; }
void set_t(struct cell *t) { T=t; }
void init_t() {global_msg_id=0; set_t(T_UNDEFINED);}


/* transaction matching a-la RFC-3261 using transaction ID in branch
 * (the function assumes there is magic cookie in branch) */

static struct cell *tid_matching( int hash_index, 
		struct via_body *via1, 
		enum request_method skip_method)
{
	struct cell *p_cell;
	struct sip_msg  *t_msg;


	/* update parsed tid */
	via1->tid.s=via1->branch->value.s+MCOOKIE_LEN;
	via1->tid.len=via1->branch->value.len-MCOOKIE_LEN;

	for ( p_cell = get_tm_table()->entrys[hash_index].first_cell;
		p_cell; p_cell = p_cell->next_cell ) 
	{
		t_msg=p_cell->uas.request;
		if (skip_method & t_msg->REQ_METHOD)
			continue;
		if (t_msg->via1->tid.len!=via1->tid.len)
			continue;
		if (memcmp(t_msg->via1->tid.s, via1->tid.s,
				via1->tid.len)!=0)
			continue;
		/* ok, tid matches -- now make sure that the
		 * originater matches too to avoid confusion with
		 * different senders generating the same tid
		 */
		if (via1->host.len!=t_msg->via1->host.len)
			continue;
		if (memcmp(via1->host.s, t_msg->via1->host.s,
					via1->host.len)!=0)
			continue;
		if (via1->port!=t_msg->via1->port)
			continue;
		if (via1->transport.len!=t_msg->via1->transport.len)
			continue;
		if (memcmp(via1->transport.s, t_msg->via1->transport.s,
					via1->transport.len)!=0)
			continue;
		/* all matched -- we found the transaction ! */
		DBG("DEBUG: RFC3261 transaction matched, tid=%.*s\n",
			via1->tid.len, via1->tid.s);

		return p_cell;
	}
	/* :-( ... we didn't find any */
	DBG("DEBUG: RFC3261 transaction matching failed\n");
	return 0;
}


/* function returns:
 *      negative - transaction wasn't found
 *			(-2 = possibly e2e ACK matched )
 *      positive - transaction found
 */

int t_lookup_request( struct sip_msg* p_msg , int leave_new_locked )
{
	struct cell         *p_cell;
	unsigned int       isACK;
	struct sip_msg  *t_msg;
	int ret;
	struct via_param *branch;

	/* parse all*/
	if (check_transaction_quadruple(p_msg)==0)
	{
		LOG(L_ERR, "ERROR: TM module: t_lookup_request: too few headers\n");
		set_t(0);	
		/* stop processing */
		return 0;
	}

	/* start searching into the table */
	if (!p_msg->hash_index)
		p_msg->hash_index=hash( p_msg->callid->body , 
			get_cseq(p_msg)->number ) ;
	isACK = p_msg->REQ_METHOD==METHOD_ACK;
	DBG("t_lookup_request: start searching: hash=%d, isACK=%d\n",
		p_msg->hash_index,isACK);


	/* asume not found */
	ret=-1;

	/* first of all, look if there is RFC3261 magic cookie in branch; if
	 * so, we can do very quick matching and skip the old-RFC bizzar
	 * comparison of many header fields
	 */
	if (!p_msg->via1) {
		LOG(L_ERR, "ERROR: t_lookup_request: no via\n");
		set_t(0);	
		return 0;
	}
	branch=p_msg->via1->branch;
	if (branch && branch->value.s && branch->value.len>MCOOKIE_LEN
			&& memcmp(branch->value.s,MCOOKIE,MCOOKIE_LEN)==0) {
		/* huhuhu! the cookie is there -- let's proceed fast */
		LOCK_HASH(p_msg->hash_index);
		p_cell=tid_matching(p_msg->hash_index, p_msg->via1, 
				/* skip transactions with different
				 * method; otherwise CANCEL would 
				 * match the previous INVITE trans.
				 */
				isACK ? ~METHOD_INVITE: ~p_msg->REQ_METHOD);
		if (p_cell) goto found; else goto notfound;
	}

	/* ok -- it's ugly old-fashioned transaction matching */
	DBG("DEBUG: proceeding to pre-RFC3261 transaction matching\n");

	/* lock the whole entry*/
	LOCK_HASH(p_msg->hash_index);

	/* all the transactions from the entry are compared */
	for ( p_cell = get_tm_table()->entrys[p_msg->hash_index].first_cell;
		  p_cell; p_cell = p_cell->next_cell ) 
	{
		t_msg = p_cell->uas.request;

		if (!isACK) {	
			/* compare lengths first */ 
			if (!EQ_LEN(callid)) continue;
			if (!EQ_LEN(cseq)) continue;
			if (!EQ_LEN(from)) continue;
			if (!EQ_LEN(to)) continue;
			if (ruri_matching && !EQ_REQ_URI_LEN) continue;
			if (!EQ_VIA_LEN(via1)) continue;

			/* length ok -- move on */
			if (!EQ_STR(callid)) continue;
			if (!EQ_STR(cseq)) continue;
			if (!EQ_STR(from)) continue;
			if (!EQ_STR(to)) continue;
			if (ruri_matching && !EQ_REQ_URI_STR) continue;
			if (!EQ_VIA_STR(via1)) continue;

			/* request matched ! */
			DBG("DEBUG: non-ACK matched\n");
			goto found;
		} else { /* it's an ACK request*/
			/* ACK's relate only to INVITEs */
			if (t_msg->REQ_METHOD!=METHOD_INVITE) continue;

			/* compare lengths now */
			if (!EQ_LEN(callid)) continue;
			/* CSeq only the number without method ! */
			if (get_cseq(t_msg)->number.len!=get_cseq(p_msg)->number.len)
				continue;
			if (! EQ_LEN(from)) continue;
			/* To only the uri and ... */
			if (get_to(t_msg)->uri.len!=get_to(p_msg)->uri.len)
				continue;
			/* don't care about to-tags -- many UAC screw them
			 * up anyway, and it doesn't hurt if we ignore 
			 * them */
#ifdef ACKTAG
			/* ... its to-tag compared to reply's tag */
			if (p_cell->uas.to_tag.len!=get_to(p_msg)->tag_value.len)
				continue;
#endif

			/* we first skip r-uri and Via and proceed with
			   content of other header-fields */

			if ( memcmp(t_msg->callid->body.s, p_msg->callid->body.s,
				p_msg->callid->body.len)!=0) continue;
			if ( memcmp(get_cseq(t_msg)->number.s, get_cseq(p_msg)->number.s,
				get_cseq(p_msg)->number.len)!=0) continue;
			if (!EQ_STR(from)) continue;
			if (memcmp(get_to(t_msg)->uri.s, get_to(p_msg)->uri.s,
				get_to(t_msg)->uri.len)!=0) continue;
#ifdef ACKTAG
			if (
#ifdef _BUG
				p_cell->uas.to_tag.len!=0 /* to-tags empty */ || 
#endif
				memcmp(p_cell->uas.to_tag.s, get_to(p_msg)->tag_value.s,
				p_cell->uas.to_tag.len)!=0) continue;
#endif
	
			/* ok, now only r-uri or via can mismatch; they must match
			   for non-2xx; if it is a 2xx, we don't try to match
			   (we might have checked that earlier to speed-up, but
			   we still want to see a diagnosti message telling
			   "this ACK presumably belongs to this 2xx transaction";
			   might change in future); the reason is 2xx ACKs are
			   a separate transaction which may carry different
			   r-uri/via1 and is thus also impossible to match it
			   uniquely to a spiraled transaction;
			*/
			if (p_cell->uas.status>=200 && p_cell->uas.status<300) {
				DBG("DEBUG: an ACK hit a 2xx transaction (T=%p); "
					"considered mismatch\n", p_cell );
				/* perhaps there are some spirals on the synonym list, but
				   it makes no sense to iterate the list until bitter end */
				t_ack=p_cell;
				ret=-2;
				break;
			}
			/* its for a >= 300 ... everything must match ! */
			if (ruri_matching && ! EQ_REQ_URI_LEN ) continue;
			if (! EQ_VIA_LEN(via1)) continue;
			if (ruri_matching && !EQ_REQ_URI_STR) continue;
			if (!EQ_VIA_STR(via1)) continue;

			/* wow -- we survived all the check! we matched! */
			DBG("DEBUG: non-2xx ACK matched\n");
			goto found;
		} /* ACK */
	} /* synonym loop */

notfound:
	/* no transaction found */
	set_t(0);
	if (!leave_new_locked) {
		UNLOCK_HASH(p_msg->hash_index);
	}
	DBG("DEBUG: t_lookup_request: no transaction found\n");
	return ret;

found:
	set_t(p_cell);
	REF_UNSAFE( T );
	T->kr|=REQ_EXIST;
	UNLOCK_HASH( p_msg->hash_index );
	DBG("DEBUG: t_lookup_request: transaction found (T=%p)\n",T);
	return 1;
}



/* function lookups transaction being cancelled by CANCEL in p_msg;
 * it returns:
 *       0 - transaction wasn't found
 *       T - transaction found
 */
struct cell* t_lookupOriginalT(  struct sip_msg* p_msg )
{
	struct cell     *p_cell;
	unsigned int     hash_index;
	struct sip_msg  *t_msg;
	struct via_param *branch;


	/* start searching in the table */
	hash_index = p_msg->hash_index;
	DBG("DEBUG: t_lookupOriginalT: searching on hash entry %d\n",hash_index );


	/* first of all, look if there is RFC3261 magic cookie in branch; if
	 * so, we can do very quick matching and skip the old-RFC bizzar
	 * comparison of many header fields
	 */
	if (!p_msg->via1) {
		LOG(L_ERR, "ERROR: t_lookup_request: no via\n");
		set_t(0);
		return 0;
	}
	branch=p_msg->via1->branch;
	if (branch && branch->value.s && branch->value.len>MCOOKIE_LEN
			&& memcmp(branch->value.s,MCOOKIE,MCOOKIE_LEN)==0) {
		/* huhuhu! the cookie is there -- let's proceed fast */
		LOCK_HASH(hash_index);
		p_cell=tid_matching(hash_index, p_msg->via1, 
				/* we are seeking the original transaction --
				 * skip CANCEL transactions during search
				 */
				METHOD_CANCEL);
		if (p_cell) goto found; else goto notfound;
	}

	/* no cookies --proceed to old-fashioned pre-3261 t-matching */

	LOCK_HASH(hash_index);

	/* all the transactions from the entry are compared */
	for (p_cell=get_tm_table()->entrys[hash_index].first_cell;
		p_cell; p_cell = p_cell->next_cell )
	{
		t_msg = p_cell->uas.request;

		/* we don't cancel CANCELs ;-) */
		if (p_cell->uas.request->REQ_METHOD==METHOD_CANCEL)
			continue;

		/* check lengths now */	
		if (!EQ_LEN(callid))
			continue;
		if (get_cseq(t_msg)->number.len!=get_cseq(p_msg)->number.len)
			continue;
		if (!EQ_LEN(from))
			continue;
#ifdef CANCEL_TAG
		if (!EQ_LEN(to))
			continue;
#else
		/* relaxed matching -- we don't care about to-tags anymore,
		 * many broken UACs screw them up and ignoring them does not
		 * actually hurt
		 */
		if (get_to(t_msg)->uri.len!=get_to(p_msg)->uri.len)
			continue;
#endif
		if (ruri_matching && !EQ_REQ_URI_LEN)
			continue;
		if (!EQ_VIA_LEN(via1))
			continue;

		/* check the content now */
		if (!EQ_STR(callid))
			continue;
		if (memcmp(get_cseq(t_msg)->number.s,
			get_cseq(p_msg)->number.s,get_cseq(p_msg)->number.len)!=0)
			continue;
		if (!EQ_STR(from))
			continue;
#ifdef CANCEL_TAG
		if (!EQ_STR(to))
			continue;
#else
		if (memcmp(get_to(t_msg)->uri.s, get_to(p_msg)->uri.s,
					get_to(t_msg)->uri.len)!=0)
			continue;
#endif
		if (ruri_matching && !EQ_REQ_URI_STR)
			continue;
		if (!EQ_VIA_STR(via1))
			continue;

		/* found */
		goto found;
	}

notfound:
	/* no transaction found */
	DBG("DEBUG: t_lookupOriginalT: no CANCEL maching found! \n" );
	UNLOCK_HASH(hash_index);
	DBG("DEBUG: t_lookupOriginalT completed\n");
	return 0;

found:
	DBG("DEBUG: t_lookupOriginalT: canceled transaction"
		" found (%p)! \n",p_cell );
	REF_UNSAFE( p_cell );
	UNLOCK_HASH(hash_index);
	DBG("DEBUG: t_lookupOriginalT completed\n");
	return p_cell;
}




/* Returns 0 - nothing found
 *         1  - T found
 */
int t_reply_matching( struct sip_msg *p_msg , int *p_branch )
{
	struct cell*  p_cell;
	int hash_index   = 0;
	int entry_label  = 0;
	int branch_id    = 0;
	char  *hashi, *branchi, *p, *n;
	int hashl, branchl;
	int scan_space;
	str cseq_method;
	str req_method;

	char *loopi;
	int loopl;
	char *syni;
	int synl;
	
	short is_cancel;

	/* make compiler warnnings happy */
	loopi=0;
	loopl=0;
	syni=0;
	synl=0;

	/* split the branch into pieces: loop_detection_check(ignored),
	 hash_table_id, synonym_id, branch_id */

	if (!(p_msg->via1 && p_msg->via1->branch && p_msg->via1->branch->value.s))
		goto nomatch2;

	/* we do RFC 3261 tid matching and want to see first if there is
	 * magic cookie in branch */
	if (p_msg->via1->branch->value.len<=MCOOKIE_LEN)
		goto nomatch2;
	if (memcmp(p_msg->via1->branch->value.s, MCOOKIE, MCOOKIE_LEN)!=0)
		goto nomatch2;

	p=p_msg->via1->branch->value.s+MCOOKIE_LEN;
	scan_space=p_msg->via1->branch->value.len-MCOOKIE_LEN;

#ifdef OBSOLETED
	p=p_msg->via1->branch->value.s;
	scan_space=p_msg->via1->branch->value.len;
#endif


	/* hash_id */
	n=eat_token2_end( p, p+scan_space, BRANCH_SEPARATOR);
	hashl=n-p;
	scan_space-=hashl;
	if (!hashl || scan_space<2 || *n!=BRANCH_SEPARATOR) goto nomatch2;
	hashi=p;
	p=n+1;scan_space--;

	if (!syn_branch) {
		/* md5 value */
		n=eat_token2_end( p, p+scan_space, BRANCH_SEPARATOR );
		loopl = n-p;
		scan_space-= loopl;
		if (n==p || scan_space<2 || *n!=BRANCH_SEPARATOR) 
			goto nomatch2;
		loopi=p;
		p=n+1; scan_space--;
	} else {
		/* synonym id */
		n=eat_token2_end( p, p+scan_space, BRANCH_SEPARATOR);
		synl=n-p;
		scan_space-=synl;
		if (!synl || scan_space<2 || *n!=BRANCH_SEPARATOR) 
			goto nomatch2;
		syni=p;
		p=n+1;scan_space--;
	}

	/* branch id  -  should exceed the scan_space */
	n=eat_token_end( p, p+scan_space );
	branchl=n-p;
	if (!branchl ) goto nomatch2;
	branchi=p;

	/* sanity check */
	if ((hash_index=reverse_hex2int(hashi, hashl))<0
		||hash_index>=TABLE_ENTRIES
		|| (branch_id=reverse_hex2int(branchi, branchl))<0
		||branch_id>=MAX_BRANCHES
		|| (syn_branch ? (entry_label=reverse_hex2int(syni, synl))<0 
			: loopl!=MD5_LEN )
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
	cseq_method=get_cseq(p_msg)->method;
	is_cancel=cseq_method.len==CANCEL_LEN 
		&& memcmp(cseq_method.s, CANCEL, CANCEL_LEN)==0;
	LOCK_HASH(hash_index);
	for (p_cell = get_tm_table()->entrys[hash_index].first_cell; p_cell; 
		p_cell=p_cell->next_cell) {

		/* first look if branch matches */

		if (syn_branch) {
			if (p_cell->label != entry_label) 
				continue;
		} else {
			if ( memcmp(p_cell->md5, loopi,MD5_LEN)!=0)
					continue;
		}

		/* sanity check ... too high branch ? */
		if ( branch_id>=p_cell->nr_of_outgoings )
			continue;

		/* does method match ? (remember -- CANCELs have the same branch
		   as cancelled transactions) */
		req_method=p_cell->method;
		if ( /* method match */
			! ((cseq_method.len==req_method.len 
			&& memcmp( cseq_method.s, req_method.s, cseq_method.len )==0)
			/* or it is a local cancel */
			|| (is_cancel && p_cell->is_invite 
				/* commented out -- should_cancel_branch set it to
				   BUSY_BUFFER to avoid collisions with repliesl;
				   thus, we test here by bbuffer size
				*/
				/* && p_cell->uac[branch_id].local_cancel.buffer ))) */
				&& p_cell->uac[branch_id].local_cancel.buffer_len ))) 
			continue;


		/* we passed all disqualifying factors .... the transaction has been
		   matched !
		*/
		set_t(p_cell);
		*p_branch = branch_id;
		REF_UNSAFE( T );
		UNLOCK_HASH(hash_index);
		DBG("DEBUG: t_reply_matching: reply matched (T=%p)!\n",T);
		return 1;
	} /* for cycle */

	/* nothing found */
	UNLOCK_HASH(hash_index);
	DBG("DEBUG: t_reply_matching: no matching transaction exists\n");

nomatch2:
	DBG("DEBUG: t_reply_matching: failure to match a transaction\n");
	*p_branch = -1;
	set_t(0);
	return -1;
}




/* Functions update T (T gets either a valid pointer in it or it equals zero) if no transaction
  * for current message exists;
  * it returns 1 if found, 0 if not found, -1 on error
  */
int t_check( struct sip_msg* p_msg , int *param_branch )
{
	int local_branch;

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
			if (parse_headers(p_msg, HDR_EOH, 0 )==-1)
				return -1;
			t_lookup_request( p_msg , 0 /* unlock before returning */ );
		} else {
			/* we need Via for branch and Cseq method to distinguish
			   replies with the same branch/cseqNr (CANCEL)
			*/
			if ( parse_headers(p_msg, HDR_VIA1|HDR_CSEQ, 0 )==-1
			|| !p_msg->via1 || !p_msg->cseq ) {
				LOG(L_ERR, "ERROR: reply cannot be parsed\n");
				return -1;
			}

			/* if that is an INVITE, we will also need to-tag
			   for later ACK matching
			*/
            if ( get_cseq(p_msg)->method.len==INVITE_LEN 
				&& memcmp( get_cseq(p_msg)->method.s, INVITE, INVITE_LEN )==0 ) {
					if (parse_headers(p_msg, HDR_TO, 0)==-1
						|| !p_msg->to)  {
						LOG(L_ERR, "ERROR: INVITE reply cannot be parsed\n");
						return -1;
					}
			}

			t_reply_matching( p_msg ,
				param_branch!=0?param_branch:&local_branch );

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

int init_rb( struct retr_buf *rb, struct sip_msg *msg )
{
	struct socket_info* send_sock;
	struct via_body* via;

	if (!reply_to_via) {
		update_sock_struct_from_ip( &rb->to, msg );
	} else {
		via=msg->via1;
		/*init retrans buffer*/
		if (update_sock_struct_from_via( &(rb->to),via )==-1) {
			LOG(L_ERR, "ERROR: init_rb: cannot lookup reply dst: %.*s\n",
				via->host.len, via->host.s );
			ser_error=E_BAD_VIA;
			return 0;
		}
	}
	send_sock=get_send_socket(&rb->to, msg->rcv.proto);
	if (send_sock==0) {
		LOG(L_ERR, "ERROR: init_rb: cannot fwd to af %d "
			"no socket\n", rb->to.s.sa_family);
		ser_error=E_BAD_VIA;
		return 0;
	}
	rb->send_sock=send_sock;
    return 1;
}



/* atomic "new_tran" construct; it returns:

	<0	on error

	+1	if a request did not match a transaction
		- it that was an ack, the calling function
		  shall forward statelessy
		- otherwise it means, a new transaction was
		  introduced and the calling function
		  shall reply/relay/whatever_appropriate

	0 on retransmission
*/
int t_newtran( struct sip_msg* p_msg )
{

	int ret, lret;
	struct cell *new_cell;
	struct sip_msg *shm_msg;

	ret=1;

	/* is T still up-to-date ? */
	DBG("DEBUG: t_addifnew: msg id=%d , global msg id=%d ,"
		" T on entrance=%p\n",p_msg->id,global_msg_id,T);

	if ( T && T!=T_UNDEFINED  ) {
		LOG(L_ERR, "ERROR: t_newtran: "
			"transaction already in process %p\n", T );
		return E_SCRIPT;
	}

	global_msg_id = p_msg->id;
	T = T_UNDEFINED;
	/* first of all, parse everything -- we will store
	   in shared memory and need to have all headers
	   ready for generating potential replies later;
	   parsing later on demand is not an option since
	   the request will be in shmem and applying 
	   parse_headers to it would intermix shmem with
	   pkg_mem
	*/
	
	if (parse_headers(p_msg, HDR_EOH, 0 )) {
		LOG(L_ERR, "ERROR: t_newtran: parse_headers failed\n");
		return E_BAD_REQ;
	}
	if ((p_msg->parsed_flag & HDR_EOH)!=HDR_EOH) {
			LOG(L_ERR, "ERROR: t_newtran: EoH not parsed\n");
			return E_OUT_OF_MEM;
	}
	/* t_lookup_requests attmpts to find the transaction; 
	   it also calls check_transaction_quadruple -> it is
	   safe to assume we have from/callid/cseq/to
	*/ 
	lret = t_lookup_request( p_msg, 1 /* leave locked if not found */ );
	/* on error, pass the error in the stack ... */
	if (lret==0) return E_BAD_TUPEL;
	/* transaction not found, it's a new request;
	   establish a new transaction (unless it is an ACK) */
	if (lret<0) {
		new_cell=0;
		if ( p_msg->REQ_METHOD!=METHOD_ACK ) {
			/* add new transaction */
			new_cell = build_cell( p_msg ) ;
			if  ( !new_cell ){
				LOG(L_ERR, "ERROR: t_addifnew: out of mem:\n");
				ret = E_OUT_OF_MEM;
			} else {
				insert_into_hash_table_unsafe( new_cell );
				set_t(new_cell);
				INIT_REF_UNSAFE(T);
				/* init pointers to headers needed to construct local
				   requests such as CANCEL/ACK
				*/

				shm_msg=new_cell->uas.request;
				new_cell->from.s=shm_msg->from->name.s;
				new_cell->from.len=HF_LEN(shm_msg->from);
				new_cell->to.s=shm_msg->to->name.s;
				new_cell->to.len=HF_LEN(shm_msg->to);
				new_cell->callid.s=shm_msg->callid->name.s;
				new_cell->callid.len=HF_LEN(shm_msg->callid);
				new_cell->cseq_n.s=shm_msg->cseq->name.s;
				new_cell->cseq_n.len=get_cseq(shm_msg)->number.s
					+get_cseq(shm_msg)->number.len
					-shm_msg->cseq->name.s;

				new_cell->method=new_cell->uas.request->first_line.u.request.method;
				new_cell->is_invite=p_msg->REQ_METHOD==METHOD_INVITE;
			}

		}

		/* was it an e2e ACK ? if so, trigger a callback */
		if (lret==-2) {
				REF_UNSAFE(t_ack);
				UNLOCK_HASH(p_msg->hash_index);
				callback_event( TMCB_E2EACK, t_ack, p_msg, p_msg->REQ_METHOD );
				UNREF(t_ack);
		} else { /* not e2e ACK */
			UNLOCK_HASH(p_msg->hash_index);
			/* now, when the transaction state exists, check if
		 	   there is a meaningful Via and calculate it; better
		 	   do it now than later: state is established so that
		 	   subsequent retransmissions will be absorbed and will
	 		   not possibly block during Via DNS resolution; doing
			   it later would only burn more CPU as if there is an
			   error, we cannot relay later whatever comes out of the
  			   the transaction 
			*/
			if (new_cell && p_msg->REQ_METHOD!=METHOD_ACK) {
				if (!init_rb( &T->uas.response, p_msg)) {
					LOG(L_ERR, "ERROR: t_newtran: unresolveable via1\n");
					put_on_wait( T );
					t_unref(p_msg);
					ret=E_BAD_VIA;
				}
			}
		}

		return ret;
	} 

	/* transaction found, it's a retransmission  or hbh ACK */
	if (p_msg->REQ_METHOD==METHOD_ACK) {
		t_release_transaction(T);
	} else {
		t_retransmit_reply(T);
	}
	/* things are done -- return from script */
	return 0;

}


int t_unref( struct sip_msg* p_msg  )
{
	if (T==T_UNDEFINED || T==T_NULL)
		return -1;
	if (T->kr==0 
		||(p_msg->REQ_METHOD==METHOD_ACK && !(T->kr & REQ_RLSD))) {
		LOG(L_WARN, "WARNING: script writer didn't release transaction\n");
		t_release_transaction(T);
	}
	UNREF( T );
	set_t(T_UNDEFINED);
	return 1;
}

