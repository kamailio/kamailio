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
 * 2003-01-23  options for disabling r-uri matching introduced (jiri)
 *              nameser_compat.h (andrei)
 * 2003-01-27  next baby-step to removing ZT - PRESERVE_ZT (jiri)
 * 2003-01-28  scratchpad removed (jiri)
 * 2003-02-13  init_rb() is proto indep. & it uses struct dest_info (andrei)
 * 2003-02-24  s/T_NULL/T_NULL_CELL/ to avoid redefinition conflict w/
 * 2003-02-27  3261 ACK/200 consumption bug removed (jiri)
 * 2003-02-28 scratchpad compatibility abandoned (jiri)
 * 2003-03-01  kr set through a function now (jiri)
 * 2003-03-06  dialog matching introduced for ACKs -- that's important for 
 *             INVITE UAS (like INVITE) and 200/ACK proxy matching (jiri)
 * 2003-03-29  optimization: e2e ACK matching only if callback installed
 *             (jiri)
 * 2003-03-30  set_kr for requests only (jiri)
 * 2003-04-04  bug_fix: RESPONSE_IN callback not called for local
 *             UAC transactions (jiri)
 * 2003-04-07  new transactions inherit on_negative and on_relpy from script
 *             variables on instatntiation (jiri)
 * 2003-04-30  t_newtran clean up (jiri)
 * 2003-08-21  request lookups fixed to skip UAC transactions, 
 *             thanks Ed (jiri)
 * 2003-12-04  global TM callbacks switched to per transaction callbacks
 *             (bogdan)
 * 2004-02-11  FIFO/CANCEL + alignments (hash=f(callid,cseq)) (uli+jiri)
 * 2004-02-13: t->is_invite and t->local replaced with flags (bogdan)
 */

#include "defs.h"


#include <assert.h>
#include "../../comp_defs.h"
#include "../../dprint.h"
#include "../../config.h"
#include "../../parser/parser_f.h"
#include "../../parser/parse_from.h"
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

/* should be request-uri matching used as a part of pre-3261 
 * transaction matching, as the standard wants us to do so
 * (and is reasonable to do so, to be able to distinguish
 * spirals)? turn only off for better interaction with 
 * devices that are broken and send different r-uri in
 * CANCEL/ACK than in original INVITE
 */
int ruri_matching=1;
int via1_matching=1;

/* presumably matching transaction for an e2e ACK */
static struct cell *t_ack;

/* this is a global variable which keeps pointer to
   transaction currently processed by a process; it it
   set by t_lookup_request or t_reply_matching; don't
   dare to change it anywhere else as it would
   break ref_counting
*/

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

static inline int parse_dlg( struct sip_msg *msg )
{
	if (parse_headers(msg, HDR_FROM | HDR_CSEQ | HDR_TO, 0)==-1) {
		LOG(L_ERR, "ERROR: parse_dlg: From or Cseq or To invalid\n");
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
	if (p_cell->relaied_reply_branch!=-2) {
		return 2; /* e2e proxied ACK */
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
	 * originater matches too to avoid confusion with
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
*/

static int matching_3261( struct sip_msg *p_msg, struct cell **trans,
			enum request_method skip_method)
{
	struct cell *p_cell;
	struct sip_msg  *t_msg;
	struct via_body *via1;
	int is_ack;
	int dlg_parsed;
	int ret;

	via1=p_msg->via1;
	is_ack=p_msg->REQ_METHOD==METHOD_ACK;
	dlg_parsed=0;
	/* update parsed tid */
	via1->tid.s=via1->branch->value.s+MCOOKIE_LEN;
	via1->tid.len=via1->branch->value.len-MCOOKIE_LEN;

	for ( p_cell = get_tm_table()->entrys[p_msg->hash_index].first_cell;
		p_cell; p_cell = p_cell->next_cell ) 
	{
		t_msg=p_cell->uas.request;
		if (!t_msg) continue;  /* don't try matching UAC transactions */
		if (skip_method & t_msg->REQ_METHOD) continue;

		/* dialog matching needs to be applied for ACK/200s */
		if (is_ack && p_cell->uas.status<300) {
			/* make sure we have parsed all things we need for dialog
			 * matching */
			if (!dlg_parsed) {
				dlg_parsed=1;
				if (!parse_dlg(p_msg)) {
					LOG(L_ERR, "ERROR: matching_3261: dlg parsing failed\n");
					return 0;
				}
			}
			ret=ack_matching(p_cell /* t w/invite */, p_msg /* ack */);
			if (ret>0) {
				*trans=p_cell;
				return ret; /* 2: e2e proxied ACK, 1 e2e UAS ACK */
			}
			/* this ACK is neither local "negative" one, nor a proxied
			 * end-2-end one, nor an end-2-end one for a UAS transaction
			 * -- we failed to match */
			continue;
		}
		/* now real tid matching occurs  for negative ACKs and any 
	 	 * other requests */
		if (!via_matching(t_msg->via1 /* inv via */, via1 /* ack */ ))
			continue;
		/* all matched -- we found the transaction ! */
		DBG("DEBUG: RFC3261 transaction matched, tid=%.*s\n",
			via1->tid.len, via1->tid.s);

		*trans=p_cell;
		return 1;
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
	int match_status;

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
		p_msg->hash_index=hash( p_msg->callid->body , get_cseq(p_msg)->number ) ;
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
		match_status=matching_3261(p_msg,&p_cell, 
				/* skip transactions with different method; otherwise CANCEL would 
	 	 		 * match the previous INVITE trans.  */
				isACK ? ~METHOD_INVITE: ~p_msg->REQ_METHOD);
		switch(match_status) {
				case 0:	goto notfound;	/* no match */
				case 1:	goto found; 	/* match */
				case 2:	goto e2e_ack;	/* e2e proxy ACK */
		}
	}

	/* ok -- it's ugly old-fashioned transaction matching -- it is
	 * a bit simplified to be fast -- we don't do all the comparisons
	 * of parsed uri, which was simply too bloated */
	DBG("DEBUG: proceeding to pre-RFC3261 transaction matching\n");

	/* lock the whole entry*/
	LOCK_HASH(p_msg->hash_index);

	/* all the transactions from the entry are compared */
	for ( p_cell = get_tm_table()->entrys[p_msg->hash_index].first_cell;
		  p_cell; p_cell = p_cell->next_cell ) 
	{
		t_msg = p_cell->uas.request;

		if (!t_msg) continue; /* skip UAC transactions */

		if (!isACK) {	
			/* compare lengths first */ 
			if (!EQ_LEN(callid)) continue;
			if (!EQ_LEN(cseq)) continue;
			if (!EQ_LEN(from)) continue;
			if (!EQ_LEN(to)) continue;
			if (ruri_matching && !EQ_REQ_URI_LEN) continue;
			if (via1_matching && !EQ_VIA_LEN(via1)) continue;

			/* length ok -- move on */
			if (!EQ_STR(callid)) continue;
			if (!EQ_STR(cseq)) continue;
			if (!EQ_STR(from)) continue;
			if (!EQ_STR(to)) continue;
			if (ruri_matching && !EQ_REQ_URI_STR) continue;
			if (via1_matching && !EQ_VIA_STR(via1)) continue;

			/* request matched ! */
			DBG("DEBUG: non-ACK matched\n");
			goto found;
		} else { /* it's an ACK request*/
			/* ACK's relate only to INVITEs */
			if (t_msg->REQ_METHOD!=METHOD_INVITE) continue;

			/* From|To URI , CallID, CSeq # must be always there */
			/* compare lengths now */
			if (!EQ_LEN(callid)) continue;
			/* CSeq only the number without method ! */
			if (get_cseq(t_msg)->number.len!=get_cseq(p_msg)->number.len)
				continue;
			if (! EQ_LEN(from)) continue;
			/* To only the uri -- to many UACs screw up tags  */
			if (get_to(t_msg)->uri.len!=get_to(p_msg)->uri.len)
				continue;
			if (!EQ_STR(callid)) continue;
			if (memcmp(get_cseq(t_msg)->number.s, get_cseq(p_msg)->number.s,
				get_cseq(p_msg)->number.len)!=0) continue;
			if (!EQ_STR(from)) continue;
			if (memcmp(get_to(t_msg)->uri.s, get_to(p_msg)->uri.s,
				get_to(t_msg)->uri.len)!=0) continue;

			/* it is e2e ACK/200 */
			if (p_cell->uas.status<300) {
				/* all criteria for proxied ACK are ok */
				if (p_cell->relaied_reply_branch!=-2) 
					goto e2e_ack;
				/* it's a local UAS transaction */
				if (dlg_matching(p_cell, p_msg))
					goto found;
				continue;
			}

			/* it is not an e2e ACK/200 -- perhaps it is 
			 * local negative case; in which case we will want
			 * more elements to match: r-uri and via; allow
			 * mismatching r-uri as an config option for broken
			 * UACs */
			if (ruri_matching && !EQ_REQ_URI_LEN ) continue;
			if (via1_matching && !EQ_VIA_LEN(via1)) continue;
			if (ruri_matching && !EQ_REQ_URI_STR) continue;
			if (via1_matching && !EQ_VIA_STR(via1)) continue;

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
	return -1;

e2e_ack:
	t_ack=p_cell;	/* e2e proxied ACK */
	set_t(0);
	if (!leave_new_locked) {
		UNLOCK_HASH(p_msg->hash_index);
	}
	DBG("DEBUG: t_lookup_request: e2e proxy ACK found\n");
	return -2;

found:
	set_t(p_cell);
	REF_UNSAFE( T );
	set_kr(REQ_EXIST);
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
	int ret;


	/* start searching in the table */
	hash_index = p_msg->hash_index;
	DBG("DEBUG: t_lookupOriginalT: searching on hash entry %d\n",hash_index );


	/* first of all, look if there is RFC3261 magic cookie in branch; if
	 * so, we can do very quick matching and skip the old-RFC bizzar
	 * comparison of many header fields
	 */
	if (!p_msg->via1) {
		LOG(L_ERR, "ERROR: t_lookupOriginalT: no via\n");
		set_t(0);
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
				METHOD_CANCEL);
		if (ret==1) goto found; else goto notfound;
	}

	/* no cookies --proceed to old-fashioned pre-3261 t-matching */

	LOCK_HASH(hash_index);

	/* all the transactions from the entry are compared */
	for (p_cell=get_tm_table()->entrys[hash_index].first_cell;
		p_cell; p_cell = p_cell->next_cell )
	{
		t_msg = p_cell->uas.request;

		if (!t_msg) continue; /* skip UAC transactions */

		/* we don't cancel CANCELs ;-) */
		if (t_msg->REQ_METHOD==METHOD_CANCEL)
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
		if (via1_matching && !EQ_VIA_LEN(via1))
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
		if (via1_matching && !EQ_VIA_STR(via1))
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
			|| (is_cancel && is_invite(p_cell)
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
		/* if this is a 200 for INVITE, we will wish to store to-tags to be
		 * able to distuingish retransmissions later and not to call
 		 * TMCB_RESPONSE_OUT uselessly; we do it only if callbacks are
		 * enabled -- except callback customers, nobody cares about 
		 * retransmissions of multiple 200/INV or ACK/200s
		 */
		if (is_invite(p_cell) && p_msg->REPLY_STATUS>=200 
		&& p_msg->REPLY_STATUS<300 
		&& ( (!is_local(p_cell) &&
				has_tran_tmcbs(p_cell,TMCB_RESPONSE_OUT|TMCB_E2EACK_IN) )
			|| (is_local(p_cell)&&has_tran_tmcbs(p_cell,TMCB_LOCAL_COMPLETED))
		)) {
			if (parse_headers(p_msg, HDR_TO, 0)==-1) {
				LOG(L_ERR, "ERROR: t_reply_matching: to parsing failed\n");
			}
		}
		if (!is_local(p_cell)) {
			run_trans_callbacks( TMCB_RESPONSE_IN, T, T->uas.request, p_msg,
				p_msg->REPLY_STATUS);
		}
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




/* Determine current transaction
 *
 *                   Found      Not Found     Error (e.g. parsing)
 *  Return Value     1          0             -1
 *  T                ptr        0             T_UNDEFINED
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
			if (parse_headers(p_msg, HDR_EOH, 0 )==-1) {
				LOG(L_ERR, "ERROR: t_check: parsing error\n");
				return -1;
			}
			/* in case, we act as UAS for INVITE and reply with 200,
			 * we will need to run dialog-matching for subsequent
			 * ACK, for which we need From-tag; We also need from-tag
			 * in case people want to have proxied e2e ACKs accounted
			 */
			if (p_msg->REQ_METHOD==METHOD_INVITE 
							&& parse_from_header(p_msg)==-1) {
				LOG(L_ERR, "ERROR: t_check: from parsing failed\n");
				return -1;
			}
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

	return T ? (T==T_UNDEFINED ? -1 : 1 ) : 0;
}

int init_rb( struct retr_buf *rb, struct sip_msg *msg)
{
	struct socket_info* send_sock;
	struct via_body* via;
	int proto;

	via=msg->via1;
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
	rb->dst.proto_reserved1=msg->rcv.proto_reserved1;
	send_sock=get_send_socket(&rb->dst.to, proto);
	if (send_sock==0) {
		LOG(L_ERR, "ERROR: init_rb: cannot fwd to af %d, proto %d "
			"no socket\n", rb->dst.to.s.sa_family, proto);
		ser_error=E_BAD_VIA;
		return 0;
	}
	rb->dst.send_sock=send_sock;
	
    return 1;
}


static inline void init_new_t(struct cell *new_cell, struct sip_msg *p_msg)
{
	struct sip_msg *shm_msg;

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
	if (p_msg->REQ_METHOD==METHOD_INVITE) new_cell->flags |= T_IS_INVITE_FLAG;
	new_cell->on_negative=get_on_negative();
	new_cell->on_reply=get_on_reply();
}

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

	insert_into_hash_table_unsafe( new_cell, p_msg->hash_index );
	set_t(new_cell);
	INIT_REF_UNSAFE(T);
	/* init pointers to headers needed to construct local
	   requests such as CANCEL/ACK
	*/
	init_new_t(new_cell, p_msg);
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
	int lret, my_err;


	/* is T still up-to-date ? */
	DBG("DEBUG: t_newtran: msg id=%d , global msg id=%d ,"
		" T on entrance=%p\n",p_msg->id,global_msg_id,T);

	if ( T && T!=T_UNDEFINED  ) {
		LOG(L_ERR, "ERROR: t_newtran: "
			"transaction already in process %p\n", T );
		return E_SCRIPT;
	}

	global_msg_id = p_msg->id;
	T = T_UNDEFINED;
	/* first of all, parse everything -- we will store in shared memory 
	   and need to have all headers ready for generating potential replies 
	   later; parsing later on demand is not an option since the request 
	   will be in shmem and applying parse_headers to it would intermix 
	   shmem with pkg_mem
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

	/* on error, pass the error in the stack ... nothing is locked yet
	   if 0 is returned */
	if (lret==0) return E_BAD_TUPEL;

	/* transaction found, it's a retransmission  */
	if (lret>0) {
		if (p_msg->REQ_METHOD==METHOD_ACK) {
			t_release_transaction(T);
		} else {
			t_retransmit_reply(T);
		}
		/* things are done -- return from script */
		return 0;
	}

	/* from now on, be careful -- hash table is locked */

	if (lret==-2) { /* was it an e2e ACK ? if so, trigger a callback */
		/* no callbacks? complete quickly */
		if ( !has_tran_tmcbs(t_ack,TMCB_E2EACK_IN) ) {
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
			run_trans_callbacks( TMCB_E2EACK_IN , t_ack, p_msg, 0,
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
		LOG(L_ERR, "ERROR: t_newtran: unresolveable via1\n");
		put_on_wait( T );
		t_unref(p_msg);
		return E_BAD_VIA;
	}

	return 1;


new_err:
	UNLOCK_HASH(p_msg->hash_index);
	return my_err;

}


int t_unref( struct sip_msg* p_msg  )
{
	enum kill_reason kr;

	if (T==T_UNDEFINED || T==T_NULL_CELL)
		return -1;
	if (p_msg->first_line.type==SIP_REQUEST){
		kr=get_kr();
		if (kr==0 
				||(p_msg->REQ_METHOD==METHOD_ACK && !(kr & REQ_RLSD))) {
			LOG(L_WARN, "WARNING: script writer didn't release transaction\n");
			t_release_transaction(T);
		}
	}
	UNREF( T );
	set_t(T_UNDEFINED);
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

int t_lookup_ident(struct cell ** trans, unsigned int hash_index, unsigned int label)
{
    struct cell* p_cell;

    if(hash_index >= TABLE_ENTRIES){
		LOG(L_ERR,"ERROR: t_lookup_ident: invalid hash_index=%u\n",hash_index);
		return -1;
    }

    LOCK_HASH(hash_index);

    /* all the transactions from the entry are compared */
    for ( p_cell = get_tm_table()->entrys[hash_index].first_cell;
	  p_cell; p_cell = p_cell->next_cell ) 
    {
		if(p_cell->label == label){
			REF_UNSAFE(p_cell);
    			UNLOCK_HASH(hash_index);
			set_t(p_cell);
			*trans=p_cell;
			DBG("DEBUG: t_lookup_ident: transaction found\n");
			return 1;
		}
    }
	
	UNLOCK_HASH(hash_index);
	set_t(0);
	*trans=p_cell;

	DBG("DEBUG: t_lookup_ident: transaction not found\n");
    
    return -1;
}

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

/* lookup a transaction by callid and cseq, parameters are pure
 * header field content only, e.g. "123@10.0.0.1" and "11"
 */
int t_lookup_callid(struct cell ** trans, str callid, str cseq) {
	struct cell* p_cell;
	unsigned hash_index;

	/* I use MAX_HEADER, not shure if this is a good choice... */
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

	if(hash_index >= TABLE_ENTRIES){
		LOG(L_ERR,"ERROR: t_lookup_callid: invalid hash_index=%u\n",hash_index);
		return -1;
	}

	/* create header fields the same way tm does itself, then compare headers */
	endpos = print_callid_mini(callid_header, callid);
	DBG("created comparable call_id header field: >%.*s<\n", endpos - callid_header, callid_header); 

	endpos = print_cseq_mini(cseq_header, &cseq, &invite_method);
	DBG("created comparable cseq header field: >%.*s<\n", endpos - cseq_header, cseq_header); 

	LOCK_HASH(hash_index);
	DBG("just locked hash index %u, looking for transactions there:\n", hash_index);

	/* all the transactions from the entry are compared */
	for ( p_cell = get_tm_table()->entrys[hash_index].first_cell;
	  p_cell; p_cell = p_cell->next_cell ) {
		
		/* compare complete header fields, casecmp to make shure invite=INVITE */
		if ( (strncmp(callid_header, p_cell->callid.s, p_cell->callid.len) == 0)
			&& (strncasecmp(cseq_header, p_cell->cseq_n.s, p_cell->cseq_n.len) == 0) ) {
			DBG("we have a match: callid=>>%.*s<< cseq=>>%.*s<<\n", p_cell->callid.len, 
				p_cell->callid.s, p_cell->cseq_n.len, p_cell->cseq_n.s);
			REF_UNSAFE(p_cell);
			UNLOCK_HASH(hash_index);
			set_t(p_cell);
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

