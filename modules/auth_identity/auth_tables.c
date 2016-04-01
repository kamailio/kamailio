/*
 * $Id$ 
 *
 * Copyright (c) 2007 iptelorg GmbH
 *
 * This file is part of SIP-router, a free SIP server.
 *
 * SIP-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*!
 * \file
 * \brief SIP-router auth-identity :: Tables
 * \ingroup auth-identity
 * Module: \ref auth-identity
 */

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/sha.h>

#include "../../mem/shm_mem.h"
#include "../../hashes.h"
#include "auth_identity.h"

#define lock_element(_cell)            lock_get(&((_cell)->lock))
#define release_element(_cell)         lock_release(&((_cell)->lock))

static int insert_into_table(ttable *ptable, void *pdata, unsigned int uhash);
static void remove_from_table_unsafe(ttable *ptable, titem *pitem);
static void remove_least(ttable *ptable, unsigned int uhash);
static void* search_item_in_table_unsafe(ttable *ptable,
								  		 const void *pneedle,
								  		 unsigned int uhash);

time_t glb_tnow=0;	/* we need for this for certificate expiration check when
					 * we've to remove the least item from a table */

int init_table(ttable **ptable,	/* table we'd like to init */
			   unsigned int ubucknum,	/* number of buckets */
			   unsigned int uitemlim,	/* maximum number of table intems */
			   table_item_cmp *fcmp,	/* compare funcion used by search */
			   table_item_searchinit *fsinit, /* inits the least item searcher funcion */
			   table_item_cmp *fleast,	/* returns the less item;
										 * used by item remover */
			   table_item_free *ffree,	/* frees the data part of an item */
			   table_item_gc *fgc)	/* tells whether an item is garbage  */
{
	int i1;

	if (!(*ptable = (ttable *) shm_malloc(sizeof(**ptable)))) {
		LOG(L_ERR, "AUTH_IDENTITY:init_table: Not enough shared memory error\n");
		return -1;
	}
	memset(*ptable, 0, sizeof(**ptable));

	if (!((*ptable)->entries = (tbucket *) shm_malloc(sizeof(tbucket)*ubucknum))) {
		LOG(L_ERR, "AUTH_IDENTITY:init_table: Not enough shared memory error\n");
		return -1;
	}
	memset((*ptable)->entries, 0, sizeof(tbucket)*ubucknum);
	for (i1=0; i1<ubucknum; i1++) {
		(*ptable)->entries[i1].pfirst = NULL;
		lock_init(&(*ptable)->entries[i1].lock);
	}

	(*ptable)->uitemlim=uitemlim;
	(*ptable)->ubuckets=ubucknum;

	(*ptable)->fcmp=fcmp;
	(*ptable)->fsearchinit=fsinit;
	(*ptable)->fleast=fleast;
	(*ptable)->ffree=ffree;
	(*ptable)->fgc=fgc;

	return 0;
}

void free_table(ttable *ptable)
{
	unsigned int u1;
	titem *pitem, *previtem;

	if (ptable) {
		for (u1=0; u1 < ptable->ubuckets; u1++)
		{
			pitem=ptable->entries[u1].pfirst;
			while (pitem) {
				previtem=pitem;
				pitem=pitem->pnext;

				ptable->ffree(previtem->pdata);
				shm_free(previtem);
			}
		}
		shm_free(ptable->entries);
		shm_free(ptable);
	}
}

/* appends an item at the end of the bucket specified by uhash */
static int insert_into_table(ttable *ptable, void *pdata, unsigned int uhash)
{
	tbucket *pbucket;
	titem *pitem;
	char bneed2remove=0;

	if (!(pitem=(titem *)shm_malloc(sizeof(*pitem)))) {
		LOG(L_ERR, "AUTH_IDENTITY:insert_into_table: Not enough shared memory error\n");
		return -1;
	}

	memset(pitem, 0, sizeof(*pitem));
	pitem->uhash=uhash;
	pitem->pdata=pdata;

	lock_element(ptable);
	/* if there is not enough room for this item then we'll remove one */
	if (ptable->unum >= ptable->uitemlim)
		bneed2remove=1;
	ptable->unum++;
	release_element(ptable);

	if (bneed2remove)
		remove_least(ptable, uhash);

	/* locates the appropriate bucket */
	pbucket = &ptable->entries[uhash];

	/* insert into that bucket */
	lock_element(pbucket);
	if (pbucket->plast) {
		pbucket->plast->pnext = pitem;
		pitem->pprev = pbucket->plast;
	} else pbucket->pfirst = pitem;
	pbucket->plast = pitem;
	release_element(pbucket);

	return 0;
}


/*  Un-link a cell from hash_table */
static void remove_from_table_unsafe(ttable *ptable, titem *pitem)
{
	tbucket *pbucket = &(ptable->entries[pitem->uhash]);

	/* unlink the cell from entry list */
	if (pitem->pprev)
		pitem->pprev->pnext = pitem->pnext;
	else
		pbucket->pfirst = pitem->pnext;

	if (pitem->pnext)
		pitem->pnext->pprev = pitem->pprev;
	else
		pbucket->plast = pitem->pprev;

	if (ptable->ffree)
		ptable->ffree(pitem->pdata);

	shm_free(pitem);
}

/* removes the least important item from its bucket or from the following first
   bucket which contains item */
static void remove_least(ttable *ptable, unsigned int uhash)
{
	tbucket *pbucket;
	unsigned int u1, uhashnow;
	titem *pleastitem=NULL, *pnow;
	int ires;

	if (!ptable->fleast)
		return ;
	if (ptable->fsearchinit)
		ptable->fsearchinit();

	for (uhashnow=uhash,u1=0, pbucket=&(ptable->entries[uhash]);
		 u1 < ptable->ubuckets;
		 u1++,pbucket=&(ptable->entries[uhashnow])) {

		lock_element(pbucket);
		/* if there any item in this bucket */
		for (pnow=pbucket->pfirst;pnow;pnow=pnow->pnext) {
			if (!pleastitem) {
				pleastitem=pnow;
				continue;
			}

			/*
 			fleast() return values:
			 1	s2 is less than s1
			 0	s1 and s2 are equal
			-1  s1 is less than s2
			-2	s1 is the least
			-3  s2 is the least
			 */
			ires=ptable->fleast(pleastitem->pdata, pnow->pdata);
			if (ires==1)
				pleastitem=pnow;
			if (ires==-2)
				break;
			if (ires==-3) {
				pleastitem=pnow;
				break;
			}
		}
		/* we found the least item in this bucket */
		if (pleastitem) {

			lock_element(ptable);
			ptable->unum--;
			release_element(ptable);

			remove_from_table_unsafe(ptable, pleastitem);
			release_element(pbucket);
			return ;
		}
		release_element(pbucket);


		/* we're in the last bucket so we start with the first one */
		if (uhashnow + 1 == ptable->ubuckets)
			uhashnow=0;
		else
		/* we step to the next bucket */
			uhashnow++;
	}
}

/* looks for an item in the scepifiad bucket */
static void* search_item_in_table_unsafe(ttable *ptable,
										 const void *pneedle,
										 unsigned int uhash)
{
	tbucket *pbucket = &(ptable->entries[uhash]);
	titem *pnow;
	void *pret=NULL;

	if (!ptable->fcmp)
		return NULL;

	for (pnow=pbucket->pfirst;pnow;pnow=pnow->pnext) {
		if (!ptable->fcmp(pneedle, pnow->pdata)) {
			pret=pnow->pdata;
			break;
		}
	}

	return pret;
}

/* looks for garbage in the hash interval specified by ihashstart and ihashend */
void garbage_collect(ttable *ptable, int ihashstart, int ihashend)
{
	unsigned int unum, uremoved;
	int i1;
	tbucket *pbucket;
	titem *pnow;


	/* there is not any garbage collector funcion available */
	if (!ptable->fgc)
		return;

	if (ptable->fsearchinit)
		ptable->fsearchinit();

	lock_element(ptable);
	unum=ptable->unum;
	release_element(ptable);

	/* if the half of the table is used or there is not so many items in a bucket
	   then we return */
// 	if (unum < ptable->uitemlim/2 && unum < ptable->ubuckets*ITEM_IN_BUCKET_LIMIT)
// 		return ;
 	if (!unum)
 		return ;

	for (i1=ihashstart; i1<=ihashend; i1++) {
		uremoved=0;
		pbucket=&(ptable->entries[i1]);

		lock_element(pbucket);
		for (pnow=pbucket->pfirst;pnow;pnow=pnow->pnext) {
			if (ptable->fgc(pnow->pdata)) {
				remove_from_table_unsafe(ptable, pnow);
				uremoved++;
			}
		}
		/* if we removed any item from table then we would update the item counter */
		if (uremoved) {
			lock_element(ptable);
			ptable->unum-=uremoved;
			release_element(ptable);
		}
		release_element(pbucket);
	}
}


/*
 * Make a copy of a str structure using shm_malloc
 */
static int str_duplicate(str* _d, str* _s)
{

	_d->s = (char *)shm_malloc(sizeof(char)*(_s->len));
	if (!_d->s) {
		LOG(L_ERR, "AUTH_IDENTITY:str_duplicate: No enough shared memory\n");
		return -1;
	}

	memcpy(_d->s, _s->s, _s->len);
	_d->len = _s->len;
	return 0;
}

/*
 *
 * Certificate table specific funcions
 *
 */
int cert_item_cmp(const void *s1, const void *s2)
{
	tcert_item *p1=(tcert_item*)s1, *p2=(tcert_item*)s2;

	return !(p1->surl.len==p2->surl.len && !memcmp(p1->surl.s, p2->surl.s, p2->surl.len));
}

void cert_item_init()
{
	/* we need for this for certificate expiration check when
	 * we've to remove an item from the table */
	glb_tnow=time(0);
}

/* we remove a certificate if expired or if accessed less than an other */
int cert_item_least(const void *s1, const void *s2)
{
	if (((tcert_item *)s1)->ivalidbefore < glb_tnow)
		return -2;
	if (((tcert_item *)s2)->ivalidbefore < glb_tnow)
		return -3;
	return (((tcert_item *)s1)->uaccessed < ((tcert_item *)s2)->uaccessed) ? -1 : 1;
}

/* frees a certificate item */
void cert_item_free(const void *sitem)
{
	shm_free(((tcert_item *)sitem)->surl.s);
	shm_free(((tcert_item *)sitem)->scertpem.s);
	shm_free((tcert_item *)sitem);
}

/* looks for a certificate in a table and increases access counter of that
   table item */
int get_cert_from_table(ttable *ptable, str *skey, tcert_item *ptarget)
{
	tcert_item* tmp_tcert_item;
	unsigned int uhash;
	int iret=0;

	uhash=get_hash1_raw(skey->s, skey->len) & (CERTIFICATE_TABLE_ENTRIES-1);

	/* we lock the whole bucket */
	lock_element(&ptable->entries[uhash]);

	tmp_tcert_item = search_item_in_table_unsafe(ptable,
												 (const void *)skey,
												 uhash);
	/* make a copy of found certificate and after the certificate
	 * verification we'll add it to certificate table */
	if (tmp_tcert_item) {
		memcpy(ptarget->scertpem.s, tmp_tcert_item->scertpem.s, tmp_tcert_item->scertpem.len);
		ptarget->scertpem.len=tmp_tcert_item->scertpem.len;
		/* we accessed this certificate */
		tmp_tcert_item->uaccessed++;
	}
	else
		iret=1;

	release_element(&ptable->entries[uhash]);

	return iret;
}

/* inserts an item to table, and removes the least item if the table is full */
int addcert2table(ttable *ptable, tcert_item *pcert)
{
	tcert_item *pshmcert;
	unsigned int uhash;

	if (!(pshmcert=(tcert_item *)shm_malloc(sizeof(*pshmcert)))) {
		LOG(L_ERR, "AUTH_IDENTITY:addcert2table: No enough shared memory\n");
		return -1;
	}
	memset(pshmcert, 0, sizeof(*pshmcert));
	if (str_duplicate(&pshmcert->surl, &pcert->surl))
		return -2;

	if (str_duplicate(&pshmcert->scertpem, &pcert->scertpem))
		return -3;

	pshmcert->ivalidbefore=pcert->ivalidbefore;
	pshmcert->uaccessed=1;

	uhash=get_hash1_raw(pcert->surl.s, pcert->surl.len) & (CERTIFICATE_TABLE_ENTRIES-1);

	if (insert_into_table(ptable, (void*)pshmcert, uhash))
		return -4;

	return 0;
}

/*
 *
 * Call-ID table specific funcions
 *
 */

int cid_item_cmp(const void *s1, const void *s2)
{
	tcid_item *p1=(tcid_item*)s1, *p2=(tcid_item*)s2;

	return !(p1->scid.len==p2->scid.len && !memcmp(p1->scid.s, p2->scid.s, p2->scid.len));
}

void cid_item_init()
{
	glb_tnow=time(0);
}

/* we remove a call-id if older than an other */
int cid_item_least(const void *s1, const void *s2)
{
	if (((tcid_item *)s1)->ivalidbefore < glb_tnow)
		return -2;
	if (((tcid_item *)s2)->ivalidbefore < glb_tnow)
		return -3;

	return (((tcid_item *)s1)->ivalidbefore < ((tcid_item *)s2)->ivalidbefore) ? -1 : 1;
}

/* tells whether an item is garbage */
int cid_item_gc(const void *s1)
{
	return (((tcid_item *)s1)->ivalidbefore < glb_tnow);
}

/* frees a call-id item */
void cid_item_free(const void *sitem)
{
	tcid_item *pcid=(tcid_item *)sitem;
	tdlg_item *pdlgs, *pdlgs_next;

	shm_free(pcid->scid.s);

	pdlgs_next=pcid->pdlgs;
	while (pdlgs_next) {
		pdlgs=pdlgs_next;
		pdlgs_next=pdlgs_next->pnext;
		shm_free (pdlgs->sftag.s);
		shm_free (pdlgs);
	}

	shm_free((tcert_item *)sitem);
}

/* inserts a callid item to table, and removes the least item if the table is full */
int proc_cid(ttable *ptable,
			 str *scid,
			 str *sftag,
			 unsigned int ucseq,
			 time_t ivalidbefore)
{
	tcid_item *pshmcid, *pcid_item;
	tdlg_item *pshmdlg, *pdlg_item, *pdlg_item_prev;
	unsigned int uhash;

	/* we suppose that this SIP request is not replayed so it doesn't exist in
	   the table so we prepare to insert */
	if (!(pshmdlg=(tdlg_item *)shm_malloc(sizeof(*pshmdlg)))) {
		LOG(L_ERR, "AUTH_IDENTITY:addcid2table: No enough shared memory\n");
		return -1;
	}
	memset(pshmdlg, 0, sizeof(*pshmdlg));
	if (str_duplicate(&pshmdlg->sftag, sftag))
		return -2;
	pshmdlg->ucseq=ucseq;


	/* we're looking for this call-id item if exists */
	uhash=get_hash1_raw(scid->s, scid->len) & (CALLID_TABLE_ENTRIES-1);

	lock_element(&ptable->entries[uhash]);

	pcid_item = search_item_in_table_unsafe(ptable,
											(const void *)scid, /* Call-id is the key */
											uhash);
	/* we've found one call-id so we're looking for the required SIP request */
	if (pcid_item) {
		for (pdlg_item=pcid_item->pdlgs, pdlg_item_prev=NULL;
		     pdlg_item;
			 pdlg_item=pdlg_item->pnext) {
			if (pdlg_item->sftag.len==sftag->len
				&& !memcmp(pdlg_item->sftag.s, sftag->s, sftag->len)) {
				/* we found this call with this from tag */
				if (pdlg_item->ucseq>=ucseq) {
					/* we've found this or older request in the table!
					   this call is replayed! */
					release_element(&ptable->entries[uhash]);

					shm_free(pshmdlg->sftag.s);
					shm_free(pshmdlg);
					return AUTH_FOUND;
				} else {
					/* this is another later request whithin this dialog so we
					   update the saved cseq */
					pdlg_item->ucseq=ucseq;
					release_element(&ptable->entries[uhash]);

					shm_free(pshmdlg->sftag.s);
					shm_free(pshmdlg);
					return 0;
				}
			}
			/* we save the previous dialog item in order to append a new item more easily */
			pdlg_item_prev ?
				(pdlg_item_prev=pdlg_item_prev->pnext) :
				(pdlg_item_prev=pdlg_item);
		}
		/* we append this to item dialogs*/
		pdlg_item_prev->pnext=pshmdlg;
		/* this is the latest request; we hold all request concerned this
		   call-id until the latest request is valid */
		pcid_item->ivalidbefore=ivalidbefore;
	}

	release_element(&ptable->entries[uhash]);

	if (!pcid_item) {
		/* this is the first request with this call-id */
		if (!(pshmcid=(tcid_item *)shm_malloc(sizeof(*pshmcid)))) {
			LOG(L_ERR, "AUTH_IDENTITY:addcid2table: No enough shared memory\n");
			return -4;
		}
		memset(pshmcid, 0, sizeof(*pshmcid));
		if (str_duplicate(&pshmcid->scid, scid)) {
			return -5;
		}
		pshmcid->ivalidbefore=ivalidbefore;
		pshmcid->pdlgs=pshmdlg;
		if (insert_into_table(ptable, (void*)pshmcid, uhash))
			return -6;
	}

	return 0;
}
