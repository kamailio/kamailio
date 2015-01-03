/*
 * This C-file takes care of matching requests and replies with
 * existing transactions. Note that we do not do SIP-compliant
 * request matching as asked by SIP spec. We do bitwise matching of 
 * all header fields in requests which form a transaction key. 
 * It is much faster and it works pretty well -- we haven't 
 * had any interop issue neither in lab nor in bake-offs. The reason
 * is that retransmissions do look same as original requests
 * (it would be really silly if they would be mangled). The only
 * exception is we parse To as To in ACK is compared to To in
 * reply and both  of them are constructed by different software.
 * 
 * As for reply matching, we match based on branch value -- that is
 * faster too.
 * The branch parameter is formed as follows: hash.md5.branch
 *
 * -jiri
 *
 *
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

#include "defs.h"


#include "../../comp_defs.h"
#include "../../compiler_opt.h"
#include "../../dprint.h"
#include "../../config.h"
#include "../../parser/parser_f.h"
#include "../../parser/parse_from.h"
#include "../../ut.h"
#include "../../timer.h"
#include "../../timer_ticks.h"
#include "../../hash_func.h"
#include "../../globals.h"
#include "../../forward.h"
#include "t_funcs.h"
#include "config.h"
#include "sip_msg.h"
#include "t_hooks.h"
#include "t_fwd.h"
#include "t_lookup.h"
#include "dlg.h" /* for t_lookup_callid */
#include "t_msgbuilder.h" /* for t_lookup_callid */

#define EQ_VIA_LEN(_via)\
	( (p_msg->via1->bsize-(p_msg->_via->name.s-(p_msg->_via->hdr.s+p_msg->_via->hdr.len)))==\
	  	(t_msg->via1->bsize-(t_msg->_via->name.s-(t_msg->_via->hdr.s+t_msg->_via->hdr.len))) )



#define EQ_LEN(_hf) (t_msg->_hf->body.len==p_msg->_hf->body.len)
#define EQ_REQ_URI_LEN\
	(p_msg->first_line.u.request.uri.len==t_msg->first_line.u.request.uri.len)

#define EQ_STR(_hf) (memcmp(t_msg->_hf->body.s,\
	p_msg->_hf->body.s, \
	p_msg->_hf->body.len)==0)
#define EQ_REQ_URI_STR\
	( memcmp( t_msg->first_line.u.request.uri.s,\
	p_msg->first_line.u.request.uri.s,\
	p_msg->first_line.u.request.uri.len)==0)
#define EQ_VIA_STR(_via)\
	( memcmp( t_msg->_via->name.s,\
	 p_msg->_via->name.s,\
	 (t_msg->via1->bsize-(t_msg->_via->name.s-(t_msg->_via->hdr.s+t_msg->_via->hdr.len)))\
	)==0 )



#define HF_LEN(_hf) ((_hf)->len)

/* presumably matching transaction for an e2e ACK */
static struct cell *t_ack;

/* this is a global variable which keeps pointer to
   transaction currently processed by a process; it it
   set by t_lookup_request or t_reply_matching; don't
   dare to change it anywhere else as it would
   break ref_counting.
   It has a valid value only if:
    - it's checked inside a failure or tm on_reply route (not core 
      on_reply[0]!)
    - global_msg_id == msg->id in all the other kinds of routes
      Note that this is the safest check and is valid also for
      failure routes (because fake_env() sets global_msg_id) or tm
      tm onreply routes (the internal reply_received() t_check() will set
      T and global_msg_id).
*/
static struct cell *T;

/* this is a global variable which keeps the current branch
   for the transaction currently processed.
   It has a valid value only if T is valid (global_msg_id==msg->id -- see
   above, and T!=0 and T!=T_UNDEFINED).
   For a request it's value is T_BR_UNDEFINED (it can have valid values only
   for replies).
*/
static int T_branch;

/* number of currently processed message; good to know
   to be able to doublecheck whether we are still working
   on a current transaction or a new message arrived;
   don't even think of changing it.
*/
unsigned int     global_msg_id;



struct cell *get_t() { return T; }
void set_t(struct cell *t, int branch) { T=t; T_branch=branch; }
void init_t() {global_msg_id=0; set_t(T_UNDEFINED, T_BR_UNDEFINED);}
int get_t_branch() { return T_branch; }

static inline int parse_dlg( struct sip_msg *msg )
{
	if (parse_headers(msg, HDR_FROM_F | HDR_CSEQ_F | HDR_TO_F, 0)==-1) {
		LOG(L_ERR, "ERROR: parse_dlg: From or Cseq or To invalid\n");
		return 0;
	}
	if ((msg->from==0)||(msg->cseq==0)||(msg->to==0)) {
		LOG(L_ERR, "ERROR: parse_dlg: missing From or Cseq or To\n");
		return 0;
	}

	if (parse_from_header(msg)==-1) {
		LOG(L_ERR, "ERROR: parse_dlg: From broken\n");
		return 0;
	}
	/* To is automatically parsed through HDR_TO in parse bitmap,
	 * we don't need to worry about it now
	if (parse_to_header(msg)==-1) {
		LOG(L_ERR, "ERROR: tid_matching: To broken\n");
		return 0;
	}
	*/
	return 1;
}

/* is the ACK (p_msg) in p_msg dialog-wise equal to the INVITE (t_msg) 
 * except to-tags? */
static inline int partial_dlg_matching(struct sip_msg *t_msg, struct sip_msg *p_msg)
{
	struct to_body *inv_from;

	if (!EQ_LEN(callid)) return 0;
	if (get_cseq(t_msg)->number.len!=get_cseq(p_msg)->number.len)
		return 0;
	inv_from=get_from(t_msg);
	if (!inv_from) {
		LOG(L_ERR, "ERROR: partial_dlg_matching: INV/From not parsed\n");
		return 0;
	}
	if (inv_from->tag_value.len!=get_from(p_msg)->tag_value.len)
		return 0;
	if (!EQ_STR(callid)) 
		return 0;
	if (memcmp(get_cseq(t_msg)->number.s, get_cseq(p_msg)->number.s,
			get_cseq(p_msg)->number.len)!=0)
		return 0;
	if (memcmp(inv_from->tag_value.s, get_from(p_msg)->tag_value.s,
			get_from(p_msg)->tag_value.len)!=0)
		return 0;
	return 1;
}

/* are to-tags in ACK/200 same as those we sent out? */
static inline int dlg_matching(struct cell *p_cell, struct sip_msg *ack )
{
	if (get_to(ack)->tag_value.len!=p_cell->uas.local_totag.len)
		return 0;
	if (memcmp(get_to(ack)->tag_value.s,p_cell->uas.local_totag.s,
				p_cell->uas.local_totag.len)!=0)
		return 0;
	return 1;
}



/* returns 2 if one of the save totags matches the totag in the current
 * message (which should be an ACK) and 0 if not */
static inline int totag_e2e_ack_matching(struct cell* p_cell, 
												struct sip_msg *ack)
{
	struct totag_elem *i;
	str *tag;
	
	tag=&get_to(ack)->tag_value;
	/* no locking needed for reading/searching, see update_totag_set() */
	for (i=p_cell->fwded_totags; i; i=i->next){
		membar_depends(); /* make sure we don't see some old i content
							(needed on CPUs like Alpha) */
		if (i->tag.len==tag->len && memcmp(i->tag.s, tag->s, tag->len)==0) {
			return 2;
		}
	}
	return 0;
}



/* returns: 0 - no match
 *          1 - full match to a local transaction
 *          2 - full match to a proxied transaction
 *          3 - partial match to a proxied transaction (totag not checked) =>
 *          care must be taken not to falsely match an ACK for a negative
 *          reply to a "fork" of the transaction
 */
static inline int ack_matching(struct cell *p_cell, struct sip_msg *p_msg) 
{
	/* partial dialog matching -- no to-tag, only from-tag, 
	 * callid, cseq number ; */
	if (!partial_dlg_matching(p_cell->uas.request, p_msg)) 
		return 0;

  	/* if this transaction is proxied (as opposed to UAS) we're
	 * done now -- we ignore to-tags; the ACK simply belongs to
	 * this UAS part of dialog, whatever to-tag it gained
	 */
	if (likely(p_cell->relayed_reply_branch!=-2)) {
		if (likely(has_tran_tmcbs(p_cell, 
							TMCB_E2EACK_IN|TMCB_E2EACK_RETR_IN)))
			return totag_e2e_ack_matching(p_cell, p_msg); /* 2 or 0 */
		else
			LOG(L_WARN, "WARNING: ack_matching() attempted on"
					" a transaction with no E2EACK callbacks => the results"
					" are not completely reliable when forking is involved\n");
		return 3; /* e2e proxied ACK partial match */
	}
	/* it's a local dialog -- we wish to verify to-tags too */
	if (dlg_matching(p_cell, p_msg)) {
		return 1;
	}
	return 0;
}

/* branch-based transaction matching */
static inline int via_matching( struct via_body *inv_via, 
				struct via_body *ack_via )
{
	if (inv_via->tid.len!=ack_via->tid.len)
		return 0;
	if (memcmp(inv_via->tid.s, ack_via->tid.s,
				ack_via->tid.len)!=0)
		return 0;
	/* ok, tid matches -- now make sure that the
	 * originator matches too to avoid confusion with
	 * different senders generating the same tid
	 */
	if (inv_via->host.len!=ack_via->host.len)
		return 0;;
	if (memcmp(inv_via->host.s, ack_via->host.s,
			ack_via->host.len)!=0)
		return 0;
	if (inv_via->port!=ack_via->port)
		return 0;
	if (inv_via->transport.len!=ack_via->transport.len)
		return 0;
	if (memcmp(inv_via->transport.s, ack_via->transport.s,
			ack_via->transport.len)!=0)
		return 0;
	/* everything matched -- we found it */
	return 1;
}


/* transaction matching a-la RFC-3261 using transaction ID in branch
   (the function assumes there is magic cookie in branch) 
   It returns:
	 2 if e2e ACK for a proxied transaction found
     1  if found (covers ACK for local UAS)
	 0  if not found (trans undefined)
	It also sets *cancel if a cancel was found for the searched transaction
*/

static int matching_3261( struct sip_msg *p_msg, struct cell **trans,
			enum request_method skip_method, int* cancel)
{
	struct cell *p_cell;
	struct cell *e2e_ack_trans;
	struct sip_msg  *t_msg;
	struct via_body *via1;
	int is_ack;
	int dlg_parsed;
	int ret = 0;
	struct entry* hash_bucket;

	*cancel=0;
	e2e_ack_trans=0;
	via1=p_msg->via1;
	is_ack=p_msg->REQ_METHOD==METHOD_ACK;
	dlg_parsed=0;
	/* update parsed tid */
	via1->tid.s=via1->branch->value.s+MCOOKIE_LEN;
	via1->tid.len=via1->branch->value.len-MCOOKIE_LEN;

	hash_bucket=&(get_tm_table()->entries[p_msg->hash_index]);
	clist_foreach(hash_bucket, p_cell, next_c){
		prefetch_loc_r(p_cell->next_c, 1);
		t_msg=p_cell->uas.request;
		if (unlikely(!t_msg)) continue;/*don't try matching UAC transactions */
		/* we want to set *cancel for transaction for which there is
		 * already a canceled transaction (e.g. re-ordered INV-CANCEL, or
		 *  INV blocked in dns lookup); we don't care about ACKs */
		if ((is_ack || (t_msg->REQ_METHOD!=METHOD_CANCEL)) && 
				(skip_method & t_msg->REQ_METHOD)) 
			continue;

		/* here we do an exercise which will be removed from future code
		   versions: we try to match end-2-end ACKs if they appear at our
		   server. This allows some applications bound to TM via callbacks
		   to correlate the e2e ACKs with transaction context, e.g., for
		   purpose of accounting. We think it is a bad place here, among
		   other things because it is not reliable. If a transaction loops
		   via SER the ACK can't be matched to proper INVITE transaction
		   (it is a separate transactino with its own branch ID) and it
		   matches all transaction instances in the loop dialog-wise.
		   Eventually, regardless to which transaction in the loop the
		   ACK belongs, only the first one will match.
		*/

		/* dialog matching needs to be applied for ACK/200s but only if
		 * this is a local transaction or its a proxied transaction interested
		 *  in e2e ACKs (has E2EACK* callbacks installed) */
		if (unlikely(is_ack && p_cell->uas.status<300)) {
			if (unlikely(has_tran_tmcbs(p_cell, 
							TMCB_E2EACK_IN|TMCB_E2EACK_RETR_IN) ||
							(p_cell->relayed_reply_branch==-2)  )) {
				/* make sure we have parsed all things we need for dialog
				 * matching */
				if (!dlg_parsed) {
					dlg_parsed=1;
					if (unlikely(!parse_dlg(p_msg))) {
						LOG(L_INFO, "ERROR: matching_3261: dlg parsing "
								"failed\n");
						return 0;
					}
				}
				ret=ack_matching(p_cell /* t w/invite */, p_msg /* ack */);
				if (unlikely(ret>0)) {
					/* if ret==1 => fully matching e2e ack for local trans 
					 * if ret==2 => matching e2e ack for proxied  transaction.
					 *  which is interested in it (E2EACK* callbacks)
					 * if ret==3 => partial match => we should at least
					 *  make sure the ACK is not for a negative reply
					 *  (FIXME: ret==3 should never happen, it's a bug catch
					 *    case)*/
					if (unlikely(ret==1)) goto found;
					if (unlikely(ret==3)){
						if (e2e_ack_trans==0)
							e2e_ack_trans=p_cell;
						continue; /* maybe we get a better 
													   match for a neg. 
													   replied trans. */
					}
					e2e_ack_trans=p_cell;
					goto e2eack_found;
				}
				/* this ACK is neither local "negative" one, nor a proxied
				* end-2-end one, nor an end-2-end one for a UAS transaction
				* -- we failed to match */
				continue;
			}
			/* not interested, it's an ack and a 2xx replied
			  transaction but the transaction is not local and
			  the transaction is not interested in e2eacks (no e2e callbacks)*/
			continue;
		}
		/* now real tid matching occurs  for negative ACKs and any 
		 * other requests */
		if (!via_matching(t_msg->via1 /* inv via */, via1 /* ack */ ))
			continue;
		/* check if call-id is still the same */
		if (cfg_get(tm, tm_cfg, callid_matching) && !EQ_LEN(callid) && !EQ_STR(callid)) {
			LOG(L_ERR, "matching transaction found but callids don't match (received: %.*s stored: %.*s)\n",
			        p_msg->callid->body.len, p_msg->callid->body.s,
			        t_msg->callid->body.len, t_msg->callid->body.s);
			continue;
		}
		if (t_msg->REQ_METHOD==METHOD_CANCEL){
			if ((p_msg->REQ_METHOD!=METHOD_CANCEL) && !is_ack){
			/* found an existing cancel for the searched transaction */
				*cancel=1;
			}
			if (skip_method & t_msg->REQ_METHOD) continue;
		}
found:
		prefetch_w(p_cell); /* great chance of modifiying it */
		/* all matched -- we found the transaction ! */
		DBG("DEBUG: RFC3261 transaction matched, tid=%.*s\n",
			via1->tid.len, via1->tid.s);
		*trans=p_cell;
		return 1;
	}
	/* :-( ... we didn't find any */
	
	/* just check if it we found an e2e ACK previously
	 * (Note: this is not very reliable, since we match e2e proxy ACKs
	 *  w/o totag => for a pre-forked invite it might match the wrong
	 *  transaction) */
	if (e2e_ack_trans) {
e2eack_found:
		*trans=e2e_ack_trans;
		return 2;
	}
	DBG("DEBUG: RFC3261 transaction matching failed\n");
	return 0;
}


/** find the transaction corresponding to a request.
 *  @return - negative - transaction wasn't found (-1) or
 *                        possible e2eACK match (-2).
 *            1        - transaction found
 *            0        - parse error
 * It also sets *cancel if there is already a cancel transaction.
 * Side-effects: sets T and T_branch 
 * (T_branch is always set to T_BR_UNDEFINED).
 */

int t_lookup_request( struct sip_msg* p_msg , int leave_new_locked,
						int* cancel)
{
	struct cell         *p_cell;
	unsigned int       isACK;
	struct sip_msg  *t_msg;
	struct via_param *branch;
	int match_status;
	struct cell *e2e_ack_trans;
	struct entry* hash_bucket;

	/* parse all*/
	if (unlikely(check_transaction_quadruple(p_msg)==0))
	{
		LOG(L_ERR, "ERROR: TM module: t_lookup_request: too few headers\n");
		set_t(0, T_BR_UNDEFINED);
		/* stop processing */
		return 0;
	}

	/* start searching into the table */
	if (!(p_msg->msg_flags & FL_HASH_INDEX)){
		p_msg->hash_index=hash( p_msg->callid->body , get_cseq(p_msg)->number);
		p_msg->msg_flags|=FL_HASH_INDEX;
	}
	isACK = p_msg->REQ_METHOD==METHOD_ACK;
	DBG("t_lookup_request: start searching: hash=%d, isACK=%d\n",
		p_msg->hash_index,isACK);


	/* assume not found */
	e2e_ack_trans = 0;

	/* first of all, look if there is RFC3261 magic cookie in branch; if
	 * so, we can do very quick matching and skip the old-RFC bizzar
	 * comparison of many header fields
	 */
	if (!p_msg->via1) {
		LOG(L_ERR, "ERROR: t_lookup_request: no via\n");
		set_t(0, T_BR_UNDEFINED);
		return 0;
	}
	branch=p_msg->via1->branch;
	if (branch && branch->value.s && branch->value.len>MCOOKIE_LEN
			&& memcmp(branch->value.s,MCOOKIE,MCOOKIE_LEN)==0) {
		/* huhuhu! the cookie is there -- let's proceed fast */
		LOCK_HASH(p_msg->hash_index);
		match_status=matching_3261(p_msg,&p_cell, 
				/* skip transactions with different method; otherwise CANCEL 
				 * would  match the previous INVITE trans.  */
				isACK ? ~METHOD_INVITE: ~p_msg->REQ_METHOD, 
				cancel);
		switch(match_status) {
				case 0:	goto notfound;	/* no match */
				case 1:	 goto found; 	/* match */
				case 2:	goto e2e_ack;	/* e2e proxy ACK */
		}
	}

	/* ok -- it's ugly old-fashioned transaction matching -- it is
	 * a bit simplified to be fast -- we don't do all the comparisons
	 * of parsed uri, which was simply too bloated */
	DBG("DEBUG: proceeding to pre-RFC3261 transaction matching\n");
	*cancel=0;
	/* lock the whole entry*/
	LOCK_HASH(p_msg->hash_index);

	hash_bucket=&(get_tm_table()->entries[p_msg->hash_index]);
	
	if (likely(!isACK)) {	
		/* all the transactions from the entry are compared */
		clist_foreach(hash_bucket, p_cell, next_c){
			prefetch_loc_r(p_cell->next_c, 1);
			t_msg = p_cell->uas.request;
			if (!t_msg) continue; /* skip UAC transactions */
			/* for non-ACKs we want same method matching, we 
			 * make an exception for pre-exisiting CANCELs because we
			 * want to set *cancel */
			if ((t_msg->REQ_METHOD!=p_msg->REQ_METHOD) &&
					(t_msg->REQ_METHOD!=METHOD_CANCEL))
					continue;
			/* compare lengths first */ 
			if (!EQ_LEN(callid)) continue;
			/* CSeq only the number without method ! */
			if (get_cseq(t_msg)->number.len!=get_cseq(p_msg)->number.len)
				continue;
			if (!EQ_LEN(from)) continue;
			if (!EQ_LEN(to)) continue;
			if (cfg_get(tm, tm_cfg, ruri_matching) && !EQ_REQ_URI_LEN) 
				continue;
			if (cfg_get(tm, tm_cfg, via1_matching) && !EQ_VIA_LEN(via1))
				continue;
			/* length ok -- move on */
			if (!EQ_STR(callid)) continue;
			if (memcmp(get_cseq(t_msg)->number.s, get_cseq(p_msg)->number.s,
				get_cseq(p_msg)->number.len)!=0) continue;
			if (!EQ_STR(from)) continue;
			if (!EQ_STR(to)) continue;
			if (cfg_get(tm, tm_cfg, ruri_matching) && !EQ_REQ_URI_STR)
				continue;
			if (cfg_get(tm, tm_cfg, via1_matching) && !EQ_VIA_STR(via1))
				continue;
			
			if ((t_msg->REQ_METHOD==METHOD_CANCEL) &&
				(p_msg->REQ_METHOD!=METHOD_CANCEL)){
				/* we've matched an existing CANCEL */
				*cancel=1;
				continue;
			}
			
			/* request matched ! */
			DBG("DEBUG: non-ACK matched\n");
			goto found;
		} /* synonym loop */
	} else { /* it's an ACK request*/
		/* all the transactions from the entry are compared */
		clist_foreach(hash_bucket, p_cell, next_c){
			prefetch_loc_r(p_cell->next_c, 1);
			t_msg = p_cell->uas.request;
			if (!t_msg) continue; /* skip UAC transactions */
			/* ACK's relate only to INVITEs */
			if (t_msg->REQ_METHOD!=METHOD_INVITE) continue;
			/* From|To URI , CallID, CSeq # must be always there */
			/* compare lengths now */
			if (!EQ_LEN(callid)) continue;
			/* CSeq only the number without method ! */
			if (get_cseq(t_msg)->number.len!=get_cseq(p_msg)->number.len)
				continue;
			/* To only the uri -- to many UACs screw up tags  */
			if (get_to(t_msg)->uri.len!=get_to(p_msg)->uri.len)
				continue;
			if (!EQ_STR(callid)) continue;
			if (memcmp(get_cseq(t_msg)->number.s, get_cseq(p_msg)->number.s,
				get_cseq(p_msg)->number.len)!=0) continue;
			if (memcmp(get_to(t_msg)->uri.s, get_to(p_msg)->uri.s,
				get_to(t_msg)->uri.len)!=0) continue;
			
			/* it is e2e ACK/200 */
			if (p_cell->uas.status<300) {
				/* For e2e ACKs, From's tag 'MUST' equal INVITE's, while use
				 * of the URI in this case is to be deprecated (Sec. 12.2.1.1).
				 * Comparing entire From body is dangerous, since some UAs
				 * screw the display name up. */
				if (parse_from_header(p_msg) < 0) {
					ERR("failed to parse From HF; ACK might not match.\n");
					continue;
				}
				if (! STR_EQ(get_from(t_msg)->tag_value, 
						get_from(p_msg)->tag_value))
					continue;
#ifdef TM_E2E_ACK_CHECK_FROM_URI
				if (! STR_EQ(get_from(t_msg)->uri, 
						get_from(p_msg)->uri))
					continue;
#endif

				/* all criteria for proxied ACK are ok */
				if (likely(p_cell->relayed_reply_branch!=-2)) {
					if (unlikely(has_tran_tmcbs(p_cell, 
									TMCB_E2EACK_IN|TMCB_E2EACK_RETR_IN))){
						if (likely(totag_e2e_ack_matching(p_cell, p_msg)==2))
							goto e2e_ack;
						else if (e2e_ack_trans==0)
							e2e_ack_trans=p_cell;
					}
					continue;
				}
				/* it's a local UAS transaction */
				if (dlg_matching(p_cell, p_msg))
					goto found;
				continue;
			} else {
				/* for hbh ACKs, From HF 'MUST' equal INVITE's one */
				if (! EQ_LEN(from)) continue;
				if (! EQ_STR(from)) continue;
			}
			
			/* it is not an e2e ACK/200 -- perhaps it is 
			 * local negative case; in which case we will want
			 * more elements to match: r-uri and via; allow
			 * mismatching r-uri as an config option for broken
			 * UACs */
			if (cfg_get(tm, tm_cfg, ruri_matching) && !EQ_REQ_URI_LEN )
				continue;
			if (cfg_get(tm, tm_cfg, via1_matching) && !EQ_VIA_LEN(via1))
				continue;
			if (cfg_get(tm, tm_cfg, ruri_matching) && !EQ_REQ_URI_STR)
				continue;
			if (cfg_get(tm, tm_cfg, via1_matching) && !EQ_VIA_STR(via1))
				continue;
			
			/* wow -- we survived all the check! we matched! */
			DBG("DEBUG: non-2xx ACK matched\n");
			goto found;
		} /* synonym loop */
	} /* ACK */

notfound:

	if (e2e_ack_trans) {
		p_cell=e2e_ack_trans;
		goto e2e_ack;
	}
		
	/* no transaction found */
	set_t(0, T_BR_UNDEFINED);
	if (!leave_new_locked) {
		UNLOCK_HASH(p_msg->hash_index);
	}
	DBG("DEBUG: t_lookup_request: no transaction found\n");
	return -1;

e2e_ack:
	t_ack=p_cell;	/* e2e proxied ACK */
	set_t(0, T_BR_UNDEFINED);
	if (!leave_new_locked) {
		UNLOCK_HASH(p_msg->hash_index);
	}
	DBG("DEBUG: t_lookup_request: e2e proxy ACK found\n");
	return -2;

found:
	set_t(p_cell, T_BR_UNDEFINED);
	REF_UNSAFE( T );
	set_kr(REQ_EXIST);
	UNLOCK_HASH( p_msg->hash_index );
	DBG("DEBUG: t_lookup_request: transaction found (T=%p)\n",T);
	return 1;
}



/* function lookups transaction being canceled by CANCEL in p_msg;
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
	struct entry* hash_bucket;
	int foo;
	int ret;


	/* start searching in the table */
	if (!(p_msg->msg_flags & FL_HASH_INDEX)){
		/* parse all*/
		if (check_transaction_quadruple(p_msg)==0)
		{
			LOG(L_ERR, "ERROR: TM module: t_lookupOriginalT:"
					" too few headers\n");
			/* stop processing */
			return 0;
		}
		p_msg->hash_index=hash( p_msg->callid->body , get_cseq(p_msg)->number);
		p_msg->msg_flags|=FL_HASH_INDEX;
	}
	hash_index = p_msg->hash_index;
	DBG("DEBUG: t_lookupOriginalT: searching on hash entry %d\n",hash_index );


	/* first of all, look if there is RFC3261 magic cookie in branch; if
	 * so, we can do very quick matching and skip the old-RFC bizzar
	 * comparison of many header fields
	 */
	if (!p_msg->via1) {
		LOG(L_ERR, "ERROR: t_lookupOriginalT: no via\n");
		return 0;
	}
	branch=p_msg->via1->branch;
	if (branch && branch->value.s && branch->value.len>MCOOKIE_LEN
			&& memcmp(branch->value.s,MCOOKIE,MCOOKIE_LEN)==0) {
		/* huhuhu! the cookie is there -- let's proceed fast */
		LOCK_HASH(hash_index);
		ret=matching_3261(p_msg, &p_cell,
				/* we are seeking the original transaction --
				 * skip CANCEL transactions during search
				 */
				METHOD_CANCEL, &foo);
		if (ret==1) goto found; else goto notfound;
	}

	/* no cookies --proceed to old-fashioned pre-3261 t-matching */

	LOCK_HASH(hash_index);

	hash_bucket=&(get_tm_table()->entries[hash_index]);
	/* all the transactions from the entry are compared */
	clist_foreach(hash_bucket, p_cell, next_c){
		prefetch_loc_r(p_cell->next_c, 1);
		t_msg = p_cell->uas.request;

		if (!t_msg) continue; /* skip UAC transactions */

		/* we don't cancel CANCELs ;-) */
		if (unlikely(t_msg->REQ_METHOD==METHOD_CANCEL))
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
		if (cfg_get(tm, tm_cfg, ruri_matching) && !EQ_REQ_URI_LEN)
			continue;
		if (cfg_get(tm, tm_cfg, via1_matching) && !EQ_VIA_LEN(via1))
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
		if (cfg_get(tm, tm_cfg, ruri_matching) && !EQ_REQ_URI_STR)
			continue;
		if (cfg_get(tm, tm_cfg, via1_matching) && !EQ_VIA_STR(via1))
			continue;

		/* found */
		goto found;
	}

notfound:
	/* no transaction found */
	DBG("DEBUG: t_lookupOriginalT: no CANCEL matching found! \n" );
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




/** get the transaction corresponding to a reply.
 * @return -1 - nothing found,  1  - T found
 * Side-effects: sets T and T_branch on success.
 */
int t_reply_matching( struct sip_msg *p_msg , int *p_branch )
{
	struct cell*  p_cell;
	unsigned int hash_index   = 0;
	unsigned int entry_label  = 0;
	unsigned int branch_id    = 0;
	char  *hashi, *branchi, *p, *n;
	struct entry* hash_bucket;
	int hashl, branchl;
	int scan_space;
	str cseq_method;
	str req_method;

	char *loopi;
	int loopl;
	
	short is_cancel;

	/* make compiler warnings happy */
	loopi=0;
	loopl=0;

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


	/* hash_id */
	n=eat_token2_end( p, p+scan_space, BRANCH_SEPARATOR);
	hashl=n-p;
	scan_space-=hashl;
	if (!hashl || scan_space<2 || *n!=BRANCH_SEPARATOR) goto nomatch2;
	hashi=p;
	p=n+1;scan_space--;

	/* md5 value */
	n=eat_token2_end( p, p+scan_space, BRANCH_SEPARATOR );
	loopl = n-p;
	scan_space-= loopl;
	if (n==p || scan_space<2 || *n!=BRANCH_SEPARATOR)
		goto nomatch2;
	loopi=p;
	p=n+1; scan_space--;

	/* branch id  -  should exceed the scan_space */
	n=eat_token_end( p, p+scan_space );
	branchl=n-p;
	if (!branchl ) goto nomatch2;
	branchi=p;

	/* sanity check */
	if (unlikely(reverse_hex2int(hashi, hashl, &hash_index)<0
		||hash_index>=TABLE_ENTRIES
		|| reverse_hex2int(branchi, branchl, &branch_id)<0
		|| branch_id>=sr_dst_max_branches
		|| loopl!=MD5_LEN)
	) {
		DBG("DEBUG: t_reply_matching: poor reply labels %d label %d "
			"branch %d\n", hash_index, entry_label, branch_id );
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
	hash_bucket=&(get_tm_table()->entries[hash_index]);
	/* all the transactions from the entry are compared */
	clist_foreach(hash_bucket, p_cell, next_c){
		prefetch_loc_r(p_cell->next_c, 1);
		if ( memcmp(p_cell->md5, loopi,MD5_LEN)!=0)
					continue;

		/* sanity check ... too high branch ? */
		if (unlikely(branch_id>=p_cell->nr_of_outgoings))
			continue;

		/* does method match ? (remember -- CANCELs have the same branch
		   as canceled transactions) */
		req_method=p_cell->method;
		if ( /* method match */
			! ((cseq_method.len==req_method.len 
			&& memcmp( cseq_method.s, req_method.s, cseq_method.len )==0)
			/* or it is a local cancel */
			|| (is_cancel && is_invite(p_cell)
				/* commented out -- should_cancel_branch set it to
				   BUSY_BUFFER to avoid collisions with replies;
				   thus, we test here by buffer size
				*/
				/* && p_cell->uac[branch_id].local_cancel.buffer ))) */
				&& p_cell->uac[branch_id].local_cancel.buffer_len ))) 
			continue;

		if (cfg_get(tm, tm_cfg, callid_matching) && 
				p_cell->uas.request && p_cell->uas.request->callid &&
		        (p_msg->callid->body.len != p_cell->uas.request->callid->body.len ||
		         memcmp(p_msg->callid->body.s, p_cell->uas.request->callid->body.s, p_msg->callid->body.len) != 0)
		) {
			LOG(L_ERR, "matching transaction found but callids don't match (received: %.*s stored: %.*s)\n",
		        p_msg->callid->body.len, p_msg->callid->body.s,
		        p_cell->uas.request->callid->body.len, p_cell->uas.request->callid->body.s);
			continue;
		}

		/* we passed all disqualifying factors .... the transaction has been
		   matched !
		*/
		set_t(p_cell, (int)branch_id);
		*p_branch =(int) branch_id;
		REF_UNSAFE( T );
		UNLOCK_HASH(hash_index);
		DBG("DEBUG: t_reply_matching: reply matched (T=%p)!\n",T);
		if(likely(!(p_msg->msg_flags&FL_TM_RPL_MATCHED))) {
			/* if this is a 200 for INVITE, we will wish to store to-tags to be
			 * able to distinguish retransmissions later and not to call
			 * TMCB_RESPONSE_OUT uselessly; we do it only if callbacks are
			 * enabled -- except callback customers, nobody cares about
			 * retransmissions of multiple 200/INV or ACK/200s
			 */
			if (unlikely( is_invite(p_cell) && p_msg->REPLY_STATUS>=200
				&& p_msg->REPLY_STATUS<300
				&& ((!is_local(p_cell) &&
					has_tran_tmcbs(p_cell,
						TMCB_RESPONSE_OUT|TMCB_RESPONSE_READY
						|TMCB_E2EACK_IN|TMCB_E2EACK_RETR_IN) )
				|| (is_local(p_cell)&&has_tran_tmcbs(p_cell, TMCB_LOCAL_COMPLETED))
			)) ) {
				if (parse_headers(p_msg, HDR_TO_F, 0)==-1) {
					LOG(L_ERR, "ERROR: t_reply_matching: to parsing failed\n");
				}
			}
			if (unlikely(has_tran_tmcbs(T, TMCB_RESPONSE_IN |
											TMCB_LOCAL_RESPONSE_IN))){
				if (!is_local(p_cell)) {
					run_trans_callbacks( TMCB_RESPONSE_IN, T, T->uas.request,
											p_msg, p_msg->REPLY_STATUS);
				}else{
					run_trans_callbacks( TMCB_LOCAL_RESPONSE_IN, T, T->uas.request,
											p_msg, p_msg->REPLY_STATUS);
				}
			}
			p_msg->msg_flags |= FL_TM_RPL_MATCHED;
		} else {
			DBG("reply in callbacks already done (T=%p)!\n", T);
		}
		return 1;
	} /* for cycle */

	/* nothing found */
	UNLOCK_HASH(hash_index);
	DBG("DEBUG: t_reply_matching: no matching transaction exists\n");

nomatch2:
	DBG("DEBUG: t_reply_matching: failure to match a transaction\n");
	*p_branch = -1;
	set_t(0, T_BR_UNDEFINED);
	return -1;
}




/** Determine current transaction (w/ e2eack support).
 *
 * script/t_lookup_request  return convention:
 *                   Found      Not Found     Error (e.g. parsing) E2E ACK
 *  @return          1         -1              0                  -2
 *  T                ptr        0              T_UNDEFINED| 0      0
 * Side-effects: sets T and T_branch.
 */
int t_check_msg( struct sip_msg* p_msg , int *param_branch )
{
	int local_branch;
	int canceled;
	int ret;
	
	ret=0;
	/* is T still up-to-date ? */
	DBG("DEBUG: t_check_msg: msg id=%d global id=%d T start=%p\n", 
		p_msg->id,global_msg_id,T);
	if ( p_msg->id != global_msg_id || T==T_UNDEFINED )
	{
		global_msg_id = p_msg->id;
		set_t(T_UNDEFINED, T_BR_UNDEFINED);
		/* transaction lookup */
		if ( p_msg->first_line.type==SIP_REQUEST ) {
			/* force parsing all the needed headers*/
			prefetch_loc_r(p_msg->unparsed+64, 1);
			if (parse_headers(p_msg, HDR_EOH_F, 0 )==-1) {
				LOG(L_ERR, "ERROR: t_check_msg: parsing error\n");
				goto error;
			}
			/* in case, we act as UAS for INVITE and reply with 200,
			 * we will need to run dialog-matching for subsequent
			 * ACK, for which we need From-tag; We also need from-tag
			 * in case people want to have proxied e2e ACKs accounted
			 */
			if (p_msg->REQ_METHOD==METHOD_INVITE 
							&& parse_from_header(p_msg)==-1) {
				LOG(L_ERR, "ERROR: t_check_msg: from parsing failed\n");
				goto error;
			}
			ret=t_lookup_request( p_msg , 0 /* unlock before returning */,
									&canceled);
		} else {
			/* we need Via for branch and Cseq method to distinguish
			   replies with the same branch/cseqNr (CANCEL)
			   and we need all the WWW/Proxy Authenticate headers for
			   401 & 407 replies
			*/
			if (cfg_get(tm, tm_cfg, tm_aggregate_auth) && 
					(p_msg->REPLY_STATUS==401 || p_msg->REPLY_STATUS==407)){
				if (parse_headers(p_msg, HDR_EOH_F,0)==-1){
					LOG(L_WARN, "WARNING: the reply cannot be "
								"completely parsed\n");
					/* try to continue, via1 & cseq are checked below */
				}
			}else if ( parse_headers(p_msg, HDR_VIA1_F|HDR_CSEQ_F|HDR_CALLID_F, 0 )==-1) {
				LOG(L_ERR, "ERROR: reply cannot be parsed\n");
				goto error;
			}
			if ((p_msg->via1==0) || (p_msg->cseq==0) || (p_msg->callid==0)){
				LOG(L_ERR, "ERROR: reply doesn't have a via, cseq or call-id"
							" header\n");
				goto error;
			}
			/* if that is an INVITE, we will also need to-tag
			   for later ACK matching
			*/
			if ( get_cseq(p_msg)->method.len==INVITE_LEN 
				&& memcmp( get_cseq(p_msg)->method.s, INVITE, INVITE_LEN )==0)
			{
				if (parse_headers(p_msg, HDR_TO_F, 0)==-1 || !p_msg->to) {
					LOG(L_ERR, "ERROR: INVITE reply cannot be parsed\n");
					goto error;
				}
			}
			ret=t_reply_matching( p_msg ,
							param_branch!=0?param_branch:&local_branch );
		}
#ifdef EXTRA_DEBUG
		if ( T && T!=T_UNDEFINED && T->flags & (T_IN_AGONY)) {
			LOG( L_WARN, "WARNING: transaction %p scheduled for deletion "
				"and called from t_check_msg (flags=%x) (but it might be ok)"
				"\n", T, T->flags);
		}
#endif
		DBG("DEBUG: t_check_msg: msg id=%d global id=%d T end=%p\n",
			p_msg->id,global_msg_id,T);
	} else { /*  ( p_msg->id == global_msg_id && T!=T_UNDEFINED ) */
		if (T){
			DBG("DEBUG: t_check_msg: T already found!\n");
			ret=1;
		}else{
			DBG("DEBUG: t_check_msg: T previously sought and not found\n");
			ret=-1;
		}
		if (likely(param_branch))
			*param_branch=T_branch;
	}
	return ret;
error:
	return 0;
}



/** Determine current transaction (old version).
 *
 *                   Found      Not Found     Error (e.g. parsing)
 *  @return          1          0             -1
 *  T                ptr        0             T_UNDEFINED | 0
 *
 * Side-effects: sets T and T_branch.
 */
int t_check( struct sip_msg* p_msg , int *param_branch )
{
	int ret;

	ret=t_check_msg(p_msg, param_branch);
	/* fix t_check_msg return */
	switch(ret){
		case -2: /* e2e ack */     return 0;  /* => not found */
		case -1: /* not found */   return 0;  /* => not found */
		case  0: /* parse error */ return -1; /* => error */
		case  1: /* found */       return ret; /* =>  found */
	};
	return ret;
}



int init_rb( struct retr_buf *rb, struct sip_msg *msg)
{
	/*struct socket_info* send_sock;*/
	struct via_body* via;
	int proto;

	/* rb. timers are init. init_t()/new_cell() */
	via=msg->via1;
	/* rb->dst is already init (0) by new_t()/build_cell() */
	if (!reply_to_via) {
		update_sock_struct_from_ip( &rb->dst.to, msg );
		proto=msg->rcv.proto;
	} else {
		/*init retrans buffer*/
		if (update_sock_struct_from_via( &(rb->dst.to), msg, via )==-1) {
			LOG(L_ERR, "ERROR: init_rb: cannot lookup reply dst: %.*s\n",
				via->host.len, via->host.s );
			ser_error=E_BAD_VIA;
			return 0;
		}
		proto=via->proto;
	}
	rb->dst.proto=proto;
	rb->dst.id=msg->rcv.proto_reserved1;
#ifdef USE_COMP
	rb->dst.comp=via->comp_no;
#endif
	rb->dst.send_flags=msg->rpl_send_flags;
	
	membar_write();
	rb->dst.send_sock=msg->rcv.bind_address;
	return 1;
}


static inline void init_new_t(struct cell *new_cell, struct sip_msg *p_msg)
{
	struct sip_msg *shm_msg;
	unsigned int timeout; /* avp timeout gets stored here (in s) */
	ticks_t lifetime;

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
	if (p_msg->REQ_METHOD==METHOD_INVITE){
		/* set flags */
		new_cell->flags |= T_IS_INVITE_FLAG |
			get_msgid_val(user_cell_set_flags, p_msg->id, int);
		new_cell->flags|=T_AUTO_INV_100 &
					(!cfg_get(tm, tm_cfg, tm_auto_inv_100) -1);
		new_cell->flags|=T_DISABLE_6xx &
					(!cfg_get(tm, tm_cfg, disable_6xx) -1);
#ifdef CANCEL_REASON_SUPPORT
		new_cell->flags|=T_NO_E2E_CANCEL_REASON &
					(!!cfg_get(tm, tm_cfg, e2e_cancel_reason) -1);
#endif /* CANCEL_REASON_SUPPORT */
		/* reset flags */
		new_cell->flags &=
			(~ get_msgid_val(user_cell_reset_flags, p_msg->id, int));
		
		lifetime=(ticks_t)get_msgid_val(user_inv_max_lifetime,
												p_msg->id, int);
		if (likely(lifetime==0))
			lifetime=cfg_get(tm, tm_cfg, tm_max_inv_lifetime);
	}else{
		lifetime=(ticks_t)get_msgid_val(user_noninv_max_lifetime, 
											p_msg->id, int);
		if (likely(lifetime==0))
			lifetime=cfg_get(tm, tm_cfg, tm_max_noninv_lifetime);
	}
	new_cell->on_failure=get_on_failure();
	new_cell->on_branch_failure=get_on_branch_failure();
	new_cell->on_reply=get_on_reply();
	new_cell->end_of_life=get_ticks_raw()+lifetime;;
	new_cell->fr_timeout=(ticks_t)get_msgid_val(user_fr_timeout,
												p_msg->id, int);
	new_cell->fr_inv_timeout=(ticks_t)get_msgid_val(user_fr_inv_timeout,
												p_msg->id, int);
	if (likely(new_cell->fr_timeout==0)){
		if (unlikely(!fr_avp2timer(&timeout))) {
			DBG("init_new_t: FR__TIMER = %d s\n", timeout);
			new_cell->fr_timeout=S_TO_TICKS((ticks_t)timeout);
		}else{
			new_cell->fr_timeout=cfg_get(tm, tm_cfg, fr_timeout);
		}
	}
	if (likely(new_cell->fr_inv_timeout==0)){
		if (unlikely(!fr_inv_avp2timer(&timeout))) {
			DBG("init_new_t: FR_INV_TIMER = %d s\n", timeout);
			new_cell->fr_inv_timeout=S_TO_TICKS((ticks_t)timeout);
			new_cell->flags |= T_NOISY_CTIMER_FLAG;
		}else{
			new_cell->fr_inv_timeout=cfg_get(tm, tm_cfg, fr_inv_timeout);
		}
	}
#ifdef TM_DIFF_RT_TIMEOUT
	new_cell->rt_t1_timeout_ms = (retr_timeout_t) get_msgid_val(
														user_rt_t1_timeout_ms,
														p_msg->id, int);
	if (likely(new_cell->rt_t1_timeout_ms == 0))
		new_cell->rt_t1_timeout_ms = cfg_get(tm, tm_cfg, rt_t1_timeout_ms);
	new_cell->rt_t2_timeout_ms = (retr_timeout_t) get_msgid_val(
														user_rt_t2_timeout_ms,
														p_msg->id, int);
	if (likely(new_cell->rt_t2_timeout_ms == 0))
		new_cell->rt_t2_timeout_ms = cfg_get(tm, tm_cfg, rt_t2_timeout_ms);
#endif
	new_cell->on_branch=get_on_branch();
}



/** creates a new transaction from a message.
 * No checks are made if the transaction exists. It is created and
 * added to the tm hashes. T is set to the new transaction.
 * @param p_msg - pointer to sip message
 * @return  >0 on success, <0 on error (an E_* error code, see error.h)
 * Side-effects: sets T and T_branch (T_branch always to T_BR_UNDEFINED).
 */
static inline int new_t(struct sip_msg *p_msg)
{
	struct cell *new_cell;

	/* for ACK-dlw-wise matching, we want From-tags */
	if (p_msg->REQ_METHOD==METHOD_INVITE && parse_from_header(p_msg)<0) {
			LOG(L_ERR, "ERROR: new_t: no valid From in INVITE\n");
			return E_BAD_REQ;
	}
	/* make sure uri will be parsed before cloning */
	if (parse_sip_msg_uri(p_msg)<0) {
		LOG(L_ERR, "ERROR: new_t: uri invalid\n");
		return E_BAD_REQ;
	}
			
	/* add new transaction */
	new_cell = build_cell( p_msg ) ;
	if  ( !new_cell ){
		LOG(L_ERR, "ERROR: new_t: out of mem:\n");
		return E_OUT_OF_MEM;
	} 

#ifdef TM_DEL_UNREF
	INIT_REF(new_cell, 2); /* 1 because it will be ref'ed from the
									   hash and +1 because we set T to it */
#endif
	insert_into_hash_table_unsafe( new_cell, p_msg->hash_index );
	set_t(new_cell, T_BR_UNDEFINED);
#ifndef TM_DEL_UNREF
	INIT_REF_UNSAFE(T);
#endif
	/* init pointers to headers needed to construct local
	   requests such as CANCEL/ACK
	*/
	init_new_t(new_cell, p_msg);
	return 1;
}



/** if no transaction already exists for the message, create a new one.
 * atomic "new_tran" construct; it returns:
 *
 * @return <0	on error
 *          +1	if a request did not match a transaction
 *           0	on retransmission
 * On success, if the request was an ack, the calling function shall
 * forward statelessly. Otherwise it means, a new transaction was
 * introduced and the calling function shall reply/relay/whatever_appropriate.
 * Side-effects: sets T and T_branch (T_branch always to T_BR_UNDEFINED).
*/
int t_newtran( struct sip_msg* p_msg )
{
	int lret, my_err;
	int canceled;


	/* is T still up-to-date ? */
	DBG("DEBUG: t_newtran: msg id=%d , global msg id=%d ,"
		" T on entrance=%p\n",p_msg->id,global_msg_id,T);

	if ( T && T!=T_UNDEFINED  ) {
		/* ERROR message moved to w_t_newtran */
		DBG("DEBUG: t_newtran: "
			"transaction already in process %p\n", T );

		/* t_newtran() has been already called, and the script
		might changed the flags after it, so we must update the flags
		in shm memory -- Miklos */
		if (T->uas.request)
			T->uas.request->flags = p_msg->flags;

		return E_SCRIPT;
	}

	global_msg_id = p_msg->id;
	set_t(T_UNDEFINED, T_BR_UNDEFINED);
	/* first of all, parse everything -- we will store in shared memory 
	   and need to have all headers ready for generating potential replies 
	   later; parsing later on demand is not an option since the request 
	   will be in shmem and applying parse_headers to it would intermix 
	   shmem with pkg_mem
	*/
	
	if (parse_headers(p_msg, HDR_EOH_F, 0 )) {
		LOG(L_ERR, "ERROR: t_newtran: parse_headers failed\n");
		return E_BAD_REQ;
	}
	if ((p_msg->parsed_flag & HDR_EOH_F)!=HDR_EOH_F) {
			LOG(L_ERR, "ERROR: t_newtran: EoH not parsed\n");
			return E_OUT_OF_MEM;
	}
	/* t_lookup_requests attempts to find the transaction; 
	   it also calls check_transaction_quadruple -> it is
	   safe to assume we have from/callid/cseq/to
	*/ 
	lret = t_lookup_request( p_msg, 1 /* leave locked if not found */,
								&canceled );

	/* on error, pass the error in the stack ... nothing is locked yet
	   if 0 is returned */
	if (lret==0) return E_BAD_TUPEL;

	/* transaction found, it's a retransmission  */
	if (lret>0) {
		if (p_msg->REQ_METHOD==METHOD_ACK) {
			if (unlikely(has_tran_tmcbs(T, TMCB_ACK_NEG_IN)))
				run_trans_callbacks(TMCB_ACK_NEG_IN, T, p_msg, 0, 
										p_msg->REQ_METHOD);
			t_release_transaction(T);
		} else {
			if (unlikely(has_tran_tmcbs(T, TMCB_REQ_RETR_IN)))
				run_trans_callbacks(TMCB_REQ_RETR_IN, T, p_msg, 0,
										p_msg->REQ_METHOD);
			t_retransmit_reply(T);
		}
		/* things are done -- return from script */
		return 0;
	}

	/* from now on, be careful -- hash table is locked */

	if (lret==-2) { /* was it an e2e ACK ? if so, trigger a callback */
		/* no callbacks? complete quickly */
		if (likely( !has_tran_tmcbs(t_ack, 
						TMCB_E2EACK_IN|TMCB_E2EACK_RETR_IN) )) {
			UNLOCK_HASH(p_msg->hash_index);
			return 1;
		} 
		REF_UNSAFE(t_ack);
		UNLOCK_HASH(p_msg->hash_index);
		/* we don't call from within REPLY_LOCK -- that introduces
		 * a race condition; however, it is so unlikely and the
		 * impact is so small (callback called multiple times of
		 * multiple ACK/200s received in parallel), that we do not
		 * better waste time in locks  */
		if (unmatched_totag(t_ack, p_msg)) {
			if (likely (has_tran_tmcbs(t_ack, TMCB_E2EACK_IN)))
				run_trans_callbacks( TMCB_E2EACK_IN , t_ack, p_msg, 0,
										-p_msg->REQ_METHOD );
		}else if (unlikely(has_tran_tmcbs(t_ack, TMCB_E2EACK_RETR_IN))){
			run_trans_callbacks( TMCB_E2EACK_RETR_IN , t_ack, p_msg, 0,
									-p_msg->REQ_METHOD );
		}
		UNREF(t_ack);
		return 1;
	}


	/* transaction not found, it's a new request (lret<0, lret!=-2);
	   establish a new transaction ... */
	if (p_msg->REQ_METHOD==METHOD_ACK) { /* ... unless it is in ACK */
		my_err=1;
		goto new_err;
	}

	my_err=new_t(p_msg);
	if (my_err<0) {
		LOG(L_ERR, "ERROR: t_newtran: new_t failed\n");
		goto new_err;
	}
	if (canceled) T->flags|=T_CANCELED; /* mark it for future ref. */


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
	if (!init_rb( &T->uas.response, p_msg)) {
		LOG(L_ERR, "ERROR: t_newtran: unresolvable via1\n");
		put_on_wait( T );
		t_unref(p_msg);
		return E_BAD_VIA;
	}

	return 1;


new_err:
	UNLOCK_HASH(p_msg->hash_index);
	return my_err;

}



/** releases the current transaction (corresp. to p_msg).
 * The current transaction (T) corresponding to the sip message being
 * processed is released. Delayed replies are sent (if no other reply
 * was sent in the script). Extra checks are made to see if the transaction
 * was forwarded, explicitly replied or explicitly released. If not the
 * transaction * is force-killed and a warning is logged (script error).
 * @param p_msg - sip message being processed
 * @return -1 on error, 1 on success.
 * Side-effects: resets T and T_branch to T_UNDEFINED, T_BR_UNDEFINED,
 *  resets tm_error.
 */
int t_unref( struct sip_msg* p_msg  )
{
	enum kill_reason kr;

	if (T==T_UNDEFINED || T==T_NULL_CELL)
		return -1;
	if (p_msg->first_line.type==SIP_REQUEST){
		kr=get_kr();
		if (unlikely(kr == REQ_ERR_DELAYED)){
			DBG("t_unref: delayed error reply generation(%d)\n", tm_error);
			if (unlikely(is_route_type(FAILURE_ROUTE))){
				BUG("tm: t_unref: called w/ kr=REQ_ERR_DELAYED in failure"
						" route for %p\n", T);
			}else if (unlikely( kill_transaction(T, tm_error)<=0 )){
				ERR("ERROR: t_unref: generation of a delayed stateful reply"
						" failed\n");
				t_release_transaction(T);
			}
		}else if ( unlikely (kr==0 ||(p_msg->REQ_METHOD==METHOD_ACK && 
								!(kr & REQ_RLSD)))) {
			LOG(L_WARN, "WARNING: script writer didn't release transaction\n");
			t_release_transaction(T);
		}else if (unlikely((kr & REQ_ERR_DELAYED) &&
					 (kr & ~(REQ_RLSD|REQ_RPLD|REQ_ERR_DELAYED|REQ_FWDED)))){
			BUG("tm: t_unref: REQ_ERR DELAYED should have been caught much"
					" earlier for %p: %d (hex %x)\n",T, kr, kr);
			t_release_transaction(T);
		}
	}
	tm_error=0; /* clear it */
	UNREF( T );
	set_t(T_UNDEFINED, T_BR_UNDEFINED);
	return 1;
}



int t_get_trans_ident(struct sip_msg* p_msg, unsigned int* hash_index, unsigned int* label)
{
    struct cell* t;
    if(t_check(p_msg,0) != 1){
	LOG(L_ERR,"ERROR: t_get_trans_ident: no transaction found\n");
	return -1;
    }
    t = get_t();
    if(!t){
	LOG(L_ERR,"ERROR: t_get_trans_ident: transaction found is NULL\n");
	return -1;
    }
    
    *hash_index = t->hash_index;
    *label = t->label;

    return 1;
}

#ifdef WITH_AS_SUPPORT
/**
 * Returns the hash coordinates of the transaction current CANCEL is targeting.
 */
int t_get_canceled_ident(struct sip_msg* msg, unsigned int* hash_index, 
		unsigned int* label)
{
	struct cell *orig;
	if (msg->REQ_METHOD != METHOD_CANCEL) {
		WARN("looking up original transaction for non-CANCEL method (%d).\n",
				msg->REQ_METHOD);
		return -1;
	}
	orig = t_lookupOriginalT(msg);
	if ((orig == T_NULL_CELL) || (orig == T_UNDEFINED))
		return -1;
	*hash_index = orig->hash_index;
	*label = orig->label;
	DEBUG("original T found @%p, %d:%d.\n", orig, *hash_index, *label);
	/* TODO: why's w_t_lookup_cancel setting T to 'undefined'?? */
	UNREF(orig);
	return 1;
}
#endif /* WITH_AS_SUPPORT */



/** lookup a transaction based on its identifier (hash_index:label).
 * @param trans - double pointer to cell structure, that will be filled
 *                with the result (a pointer to an existing transaction or
 *                0).
 * @param hash_index - searched transaction hash_index (part of the ident).
 * @param label - searched transaction label (part of the ident).
 * @return -1 on error/not found, 1 on success (found)
 * Side-effects: sets T and T_branch (T_branch always to T_BR_UNDEFINED).
 */
int t_lookup_ident(struct cell ** trans, unsigned int hash_index, 
					unsigned int label)
{
	struct cell* p_cell;
	struct entry* hash_bucket;

	if(unlikely(hash_index >= TABLE_ENTRIES)){
		LOG(L_ERR,"ERROR: t_lookup_ident: invalid hash_index=%u\n",hash_index);
		return -1;
	}
	
	LOCK_HASH(hash_index);

#ifndef E2E_CANCEL_HOP_BY_HOP
#warning "t_lookup_ident() can only reliably match INVITE transactions in " \
		"E2E_CANCEL_HOP_BY_HOP mode"
#endif
	hash_bucket=&(get_tm_table()->entries[hash_index]);
	/* all the transactions from the entry are compared */
	clist_foreach(hash_bucket, p_cell, next_c){
		prefetch_loc_r(p_cell->next_c, 1);
		if(p_cell->label == label){
			REF_UNSAFE(p_cell);
    			UNLOCK_HASH(hash_index);
			set_t(p_cell, T_BR_UNDEFINED);
			*trans=p_cell;
			DBG("DEBUG: t_lookup_ident: transaction found\n");
			return 1;
		}
    }
	
	UNLOCK_HASH(hash_index);
	set_t(0, T_BR_UNDEFINED);
	*trans=p_cell;

	DBG("DEBUG: t_lookup_ident: transaction not found\n");
    
    return -1;
}



/** check if a transaction is local or not.
 * Check if the transaction corresponding to the current message
 * is local or not.
 * @param p_msg - pointer to sip_msg
 * @return -1 on error, 0 if the transaction is not local, 1 if it is local.
 * Side-effects: sets T and T_branch.
 */
int t_is_local(struct sip_msg* p_msg)
{
    struct cell* t;
    if(t_check(p_msg,0) != 1){
	LOG(L_ERR,"ERROR: t_is_local: no transaction found\n");
	return -1;
    }
    t = get_t();
    if(!t){
	LOG(L_ERR,"ERROR: t_is_local: transaction found is NULL\n");
	return -1;
    }
    
    return is_local(t);
}

/** lookup a transaction based on callid and cseq.
 * The parameters are pure header field content only,
 * e.g. "123@10.0.0.1" and "11"
 * @param trans - double pointer to the transaction, filled with a pointer
 *                to the found transaction on success and with 0 if the
 *                transaction was not found.
 * @param callid - callid for the searched transaction.
 * @param cseq - cseq for the searched transaction.
 * @return -1 on error/not found, 1 if found.
 * Side-effects: sets T and T_branch (T_branch always to T_BR_UNDEFINED).
 */
int t_lookup_callid(struct cell ** trans, str callid, str cseq) {
	struct cell* p_cell;
	unsigned hash_index;
	struct entry* hash_bucket;

	/* I use MAX_HEADER, not sure if this is a good choice... */
	char callid_header[MAX_HEADER];
	char cseq_header[MAX_HEADER];
	/* save return value of print_* functions here */
	char* endpos;

	/* need method, which is always INVITE in our case */
	/* CANCEL is only useful after INVITE */
	str invite_method;
	char* invite_string = INVITE;
	
	invite_method.s = invite_string;
	invite_method.len = INVITE_LEN;
	
	/* lookup the hash index where the transaction is stored */
	hash_index=hash(callid, cseq);

	if(unlikely(hash_index >= TABLE_ENTRIES)){
		LOG(L_ERR,"ERROR: t_lookup_callid: invalid hash_index=%u\n",hash_index);
		return -1;
	}

	/* create header fields the same way tm does itself, then compare headers */
	endpos = print_callid_mini(callid_header, callid);
	DBG("created comparable call_id header field: >%.*s<\n", 
			(int)(endpos - callid_header), callid_header); 

	endpos = print_cseq_mini(cseq_header, &cseq, &invite_method);
	DBG("created comparable cseq header field: >%.*s<\n", 
			(int)(endpos - cseq_header), cseq_header); 

	LOCK_HASH(hash_index);
	DBG("just locked hash index %u, looking for transactions there:\n", hash_index);

	hash_bucket=&(get_tm_table()->entries[hash_index]);
	/* all the transactions from the entry are compared */
	clist_foreach(hash_bucket, p_cell, next_c){
		
		prefetch_loc_r(p_cell->next_c, 1);
		/* compare complete header fields, casecmp to make sure invite=INVITE*/
		if ((strncmp(callid_header, p_cell->callid.s, p_cell->callid.len) == 0)
			&& (strncasecmp(cseq_header, p_cell->cseq_n.s, p_cell->cseq_n.len)
				== 0)) {
			DBG("we have a match: callid=>>%.*s<< cseq=>>%.*s<<\n",
					p_cell->callid.len, p_cell->callid.s, p_cell->cseq_n.len,
					p_cell->cseq_n.s);
			REF_UNSAFE(p_cell);
			UNLOCK_HASH(hash_index);
			set_t(p_cell, T_BR_UNDEFINED);
			*trans=p_cell;
			DBG("DEBUG: t_lookup_callid: transaction found.\n");
			return 1;
		}
		DBG("NO match: callid=%.*s cseq=%.*s\n", p_cell->callid.len, 
			p_cell->callid.s, p_cell->cseq_n.len, p_cell->cseq_n.s);
			
	}

	UNLOCK_HASH(hash_index);
	DBG("DEBUG: t_lookup_callid: transaction not found.\n");
    
	return -1;
}



/* params: fr_inv & fr value in ms, 0 means "do not touch"
 * ret: 1 on success, -1 on error (script safe)*/
int t_set_fr(struct sip_msg* msg, unsigned int fr_inv_to, unsigned int fr_to)
{
	struct cell *t;
	ticks_t fr_inv, fr;
	
	
	fr_inv=MS_TO_TICKS((ticks_t)fr_inv_to);
	if ((fr_inv==0) && (fr_inv_to!=0)){
		ERR("t_set_fr_inv: fr_inv_timeout too small (%d)\n", fr_inv_to);
		return -1;
	}
	fr=MS_TO_TICKS((ticks_t)fr_to);
	if ((fr==0) && (fr_to!=0)){
		ERR("t_set_fr_inv: fr_timeout too small (%d)\n", fr_to);
		return -1;
	}
	
	t=get_t();
	/* in REPLY_ROUTE and FAILURE_ROUTE T will be set to current transaction;
	 * in REQUEST_ROUTE T will be set only if the transaction was already
	 * created; if not -> use the static variables */
	if (!t || t==T_UNDEFINED ){
		set_msgid_val(user_fr_inv_timeout, msg->id, int, (int)fr_inv);
		set_msgid_val(user_fr_timeout, msg->id, int, (int)fr);
	}else{
		change_fr(t, fr_inv, fr); /* change running uac timers */
	}
	return 1;
}

/* reset fr_timer and fr_inv_timer to the default values */
int t_reset_fr()
{
	struct cell *t;

	t=get_t();
	/* in REPLY_ROUTE and FAILURE_ROUTE T will be set to current transaction;
	 * in REQUEST_ROUTE T will be set only if the transaction was already
	 * created; if not -> use the static variables */
	if (!t || t==T_UNDEFINED ){
		memset(&user_fr_inv_timeout, 0, sizeof(user_fr_inv_timeout));
		memset(&user_fr_timeout, 0, sizeof(user_fr_timeout));
	}else{
		change_fr(t,
			cfg_get(tm, tm_cfg, fr_inv_timeout),
			cfg_get(tm, tm_cfg, fr_timeout)); /* change running uac timers */
	}
	return 1;
}

#ifdef TM_DIFF_RT_TIMEOUT

/* params: retr. t1 & retr. t2 value in ms, 0 means "do not touch"
 * ret: 1 on success, -1 on error (script safe)*/
int t_set_retr(struct sip_msg* msg, unsigned int t1_ms, unsigned int t2_ms)
{
	struct cell *t;
	ticks_t retr_t1, retr_t2;
	
	
	retr_t1=MS_TO_TICKS((ticks_t)t1_ms);
	if (unlikely((retr_t1==0) && (t1_ms!=0))){
		ERR("t_set_retr: retr. t1 interval too small (%u)\n", t1_ms);
		return -1;
	}
	if (unlikely(MAX_UVAR_VALUE(t->rt_t1_timeout_ms) < t1_ms)){
		ERR("t_set_retr: retr. t1 interval too big: %d (max %lu)\n",
				t1_ms, MAX_UVAR_VALUE(t->rt_t1_timeout_ms)); 
		return -1;
	} 
	retr_t2=MS_TO_TICKS((ticks_t)t2_ms);
	if (unlikely((retr_t2==0) && (t2_ms!=0))){
		ERR("t_set_retr: retr. t2 interval too small (%d)\n", t2_ms);
		return -1;
	}
	if (unlikely(MAX_UVAR_VALUE(t->rt_t2_timeout_ms) < t2_ms)){
		ERR("t_set_retr: retr. t2 interval too big: %u (max %lu)\n",
				t2_ms, MAX_UVAR_VALUE(t->rt_t2_timeout_ms)); 
		return -1;
	} 
	
	t=get_t();
	/* in REPLY_ROUTE and FAILURE_ROUTE T will be set to current transaction;
	 * in REQUEST_ROUTE T will be set only if the transaction was already
	 * created; if not -> use the static variables */
	if (!t || t==T_UNDEFINED ){
		set_msgid_val(user_rt_t1_timeout_ms, msg->id, int, (int)t1_ms);
		set_msgid_val(user_rt_t2_timeout_ms, msg->id, int, (int)t2_ms);
	}else{
		change_retr(t, 1, t1_ms, t2_ms); /* change running uac timers */
	}
	return 1;
}

/* reset retr. t1 and t2 to the default values */
int t_reset_retr()
{
	struct cell *t;

	t=get_t();
	/* in REPLY_ROUTE and FAILURE_ROUTE T will be set to current transaction;
	 * in REQUEST_ROUTE T will be set only if the transaction was already
	 * created; if not -> use the static variables */
	if (!t || t==T_UNDEFINED ){
		memset(&user_rt_t1_timeout_ms, 0, sizeof(user_rt_t1_timeout_ms));
		memset(&user_rt_t2_timeout_ms, 0, sizeof(user_rt_t2_timeout_ms));
	}else{
		 /* change running uac timers */
		change_retr(t,
			1,
			cfg_get(tm, tm_cfg, rt_t1_timeout_ms),
			cfg_get(tm, tm_cfg, rt_t2_timeout_ms));
	}
	return 1;
}
#endif


/* params: maximum transaction lifetime for inv and non-inv
 *         0 means do not touch"
 * ret: 1 on success, -1 on error (script safe)*/
int t_set_max_lifetime(struct sip_msg* msg,
						unsigned int lifetime_inv_to,
						unsigned int lifetime_noninv_to)
{
	struct cell *t;
	ticks_t max_inv_lifetime, max_noninv_lifetime;
	
	
	max_noninv_lifetime=MS_TO_TICKS((ticks_t)lifetime_noninv_to);
	max_inv_lifetime=MS_TO_TICKS((ticks_t)lifetime_inv_to);
	if (unlikely((max_noninv_lifetime==0) && (lifetime_noninv_to!=0))){
		ERR("t_set_max_lifetime: non-inv. interval too small (%d)\n",
				lifetime_noninv_to);
		return -1;
	}
	if (unlikely((max_inv_lifetime==0) && (lifetime_inv_to!=0))){
		ERR("t_set_max_lifetime: inv. interval too small (%d)\n",
				lifetime_inv_to);
		return -1;
	}
	
	t=get_t();
	/* in REPLY_ROUTE and FAILURE_ROUTE T will be set to current transaction;
	 * in REQUEST_ROUTE T will be set only if the transaction was already
	 * created; if not -> use the static variables */
	if (!t || t==T_UNDEFINED ){
		set_msgid_val(user_noninv_max_lifetime, msg->id, int,
						(int)max_noninv_lifetime);
		set_msgid_val(user_inv_max_lifetime, msg->id, int,
						(int)max_inv_lifetime);
	}else{
		change_end_of_life(t, 1, is_invite(t)?max_inv_lifetime:
												max_noninv_lifetime);
	}
	return 1;
}

/* reset maximum invite/non-invite lifetime to the default value */
int t_reset_max_lifetime()
{
	struct cell *t;

	t=get_t();
	/* in REPLY_ROUTE and FAILURE_ROUTE T will be set to current transaction;
	 * in REQUEST_ROUTE T will be set only if the transaction was already
	 * created; if not -> use the static variables */
	if (!t || t==T_UNDEFINED ){
		memset(&user_inv_max_lifetime, 0, sizeof(user_inv_max_lifetime));
		memset(&user_noninv_max_lifetime, 0, sizeof(user_noninv_max_lifetime));
	}else{
		change_end_of_life(t,
				1,
				is_invite(t)?
					cfg_get(tm, tm_cfg, tm_max_inv_lifetime):
					cfg_get(tm, tm_cfg, tm_max_noninv_lifetime)
				);
	}
	return 1;
}

#ifdef WITH_TM_CTX

tm_ctx_t _tm_ctx;

tm_ctx_t* tm_ctx_get(void)
{
	return &_tm_ctx;
}

void tm_ctx_init(void)
{
	memset(&_tm_ctx, 0, sizeof(tm_ctx_t));
	_tm_ctx.branch_index = T_BR_UNDEFINED;
}

void tm_ctx_set_branch_index(int v)
{
	_tm_ctx.branch_index = v;
}

#endif

