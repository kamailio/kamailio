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
 * \brief Kamailio core :: Data lumps
 * \ingroup core
 * Module: \ref core
 */



#include "data_lump.h"
#include "dprint.h"
#include "mem/mem.h"
#include "globals.h"
#include "error.h"

#include <stdlib.h>
#include <string.h>

#ifdef DEBUG_DMALLOC
#include <dmalloc.h>
#endif

/* WARNING: all lump add/insert operations expect a pkg_malloc'ed char* 
 * pointer the will be DEALLOCATED when the sip_msg is destroyed! */

enum lump_dir { LD_NEXT, LD_BEFORE, LD_AFTER };

/* adds a header to the end
 * returns  pointer on success, 0 on error */
struct lump* append_new_lump(struct lump** list, char* new_hdr,
							 int len, enum _hdr_types_t type)
{
	struct lump** t;
	struct lump* tmp;
	
	for (t=list;*t;t=&((*t)->next));

	tmp=pkg_malloc(sizeof(struct lump));
	if (tmp==0){
		LM_ERR("out of memory\n");
		return 0;
	}
		
	memset(tmp,0,sizeof(struct lump));
	tmp->type=type;
	tmp->op=LUMP_ADD;
	tmp->u.value=new_hdr;
	tmp->len=len;
	*t=tmp;
	return tmp;
}

/* adds a header right after an anchor point if exists
 * returns  pointer on success, 0 on error */
struct lump* add_new_lump(struct lump** list, char* new_hdr,
							 int len, enum _hdr_types_t type)
{
	struct lump** t;
	struct lump* tmp;
	

	t = (*list) ? &((*list)->next) : list;

	tmp=pkg_malloc(sizeof(struct lump));
	if (tmp==0){
		LM_ERR("out of memory\n");
		return 0;
	}
		
	memset(tmp,0,sizeof(struct lump));
	tmp->type=type;
	tmp->op=LUMP_ADD;
	tmp->u.value=new_hdr;
	tmp->len=len;
	tmp->next=*t;
	*t=tmp;
	return tmp;
}



/* inserts a header to the beginning 
 * returns pointer if success, 0 on error */
struct lump* insert_new_lump(struct lump** list, char* new_hdr,
								int len, enum _hdr_types_t type)
{
	struct lump* tmp;

	tmp=pkg_malloc(sizeof(struct lump));
	if (tmp==0){
		LM_ERR("out of memory\n");
		return 0;
	}
	memset(tmp,0,sizeof(struct lump));
	tmp->next=*list;
	tmp->type=type;
	tmp->op=LUMP_ADD;
	tmp->u.value=new_hdr;
	tmp->len=len;
	*list=tmp;
	return tmp;
}



/* inserts a  header/data lump immediately after hdr 
 * returns pointer on success, 0 on error */
struct lump* insert_new_lump_after( struct lump* after, char* new_hdr,
							int len, enum _hdr_types_t type)
{
	struct lump* tmp;

	tmp=pkg_malloc(sizeof(struct lump));
	if (tmp==0){
		ser_error=E_OUT_OF_MEM;
		LM_ERR("out of memory\n");
		return 0;
	}
	memset(tmp,0,sizeof(struct lump));
	tmp->after=after->after;
	tmp->type=type;
	tmp->op=LUMP_ADD;
	tmp->u.value=new_hdr;
	tmp->len=len;
	after->after=tmp;
	return tmp;
}



/* inserts a  header/data lump immediately before "before" 
 * returns pointer on success, 0 on error */
struct lump* insert_new_lump_before( struct lump* before, char* new_hdr,
							int len, enum _hdr_types_t type)
{
	struct lump* tmp;

	tmp=pkg_malloc(sizeof(struct lump));
	if (tmp==0){
		ser_error=E_OUT_OF_MEM;
		LM_ERR("out of memory\n");
		return 0;
	}
	memset(tmp,0,sizeof(struct lump));
	tmp->before=before->before;
	tmp->type=type;
	tmp->op=LUMP_ADD;
	tmp->u.value=new_hdr;
	tmp->len=len;
	before->before=tmp;
	return tmp;
}



/* inserts a  subst lump immediately after hdr 
 * returns pointer on success, 0 on error */
struct lump* insert_subst_lump_after( struct lump* after, enum lump_subst subst,
										enum _hdr_types_t type)
{
	struct lump* tmp;
	
	tmp=pkg_malloc(sizeof(struct lump));
	if (tmp==0){
		ser_error=E_OUT_OF_MEM;
		LM_ERR("out of memory\n");
		return 0;
	}
	memset(tmp,0,sizeof(struct lump));
	tmp->after=after->after;
	tmp->type=type;
	tmp->op=LUMP_ADD_SUBST;
	tmp->u.subst=subst;
	tmp->len=0;
	after->after=tmp;
	return tmp;
}



/* inserts a  subst lump immediately before "before" 
 * returns pointer on success, 0 on error */
struct lump* insert_subst_lump_before(	struct lump* before, 
										enum lump_subst subst,
										enum _hdr_types_t type)
{
	struct lump* tmp;
	
	tmp=pkg_malloc(sizeof(struct lump));
	if (tmp==0){
		ser_error=E_OUT_OF_MEM;
		LM_ERR("out of memory\n");
		return 0;
	}
	memset(tmp,0,sizeof(struct lump));
	tmp->before=before->before;
	tmp->type=type;
	tmp->op=LUMP_ADD_SUBST;
	tmp->u.subst=subst;
	tmp->len=0;
	before->before=tmp;
	return tmp;
}



/* inserts a  cond lump immediately after hdr 
 * returns pointer on success, 0 on error */
struct lump* insert_cond_lump_after( struct lump* after, enum lump_conditions c,
										enum _hdr_types_t type)
{
	struct lump* tmp;
	
	tmp=pkg_malloc(sizeof(struct lump));
	if (tmp==0){
		ser_error=E_OUT_OF_MEM;
		LM_ERR("out of memory\n");
		return 0;
	}
	memset(tmp,0,sizeof(struct lump));
	tmp->after=after->after;
	tmp->type=type;
	tmp->op=LUMP_ADD_OPT;
	tmp->u.cond=c;
	tmp->len=0;
	after->after=tmp;
	return tmp;
}



/* inserts a  conditional lump immediately before "before" 
 * returns pointer on success, 0 on error */
struct lump* insert_cond_lump_before(	struct lump* before, 
										enum lump_conditions c,
										enum _hdr_types_t type)
{
	struct lump* tmp;
	
	tmp=pkg_malloc(sizeof(struct lump));
	if (tmp==0){
		ser_error=E_OUT_OF_MEM;
		LM_ERR("out of memory\n");
		return 0;
	}
	memset(tmp,0,sizeof(struct lump));
	tmp->before=before->before;
	tmp->type=type;
	tmp->op=LUMP_ADD_OPT;
	tmp->u.cond=c;
	tmp->len=0;
	before->before=tmp;
	return tmp;
}



/* removes an already existing header/data lump */
/* WARNING: this function adds the lump either to the msg->add_rm or
 * msg->body_lumps list, depending on the offset being greater than msg->eoh,
 * so msg->eoh must be parsed (parse with HDR_EOH) if you think your lump
 *  might affect the body!! */
struct lump* del_lump(struct sip_msg* msg, int offset, int len, enum _hdr_types_t type)
{
	struct lump* tmp;
	struct lump* prev, *t;
	struct lump** list;

	/* extra checks */
	if (offset>msg->len){
		LM_CRIT("offset exceeds message size (%d > %d) aborting...\n",
					offset, msg->len);
		abort();
	}
	if (offset+len>msg->len){
		LM_CRIT("offset + len exceeds message size (%d + %d > %d)\n",
					offset, len,  msg->len);
		abort();
	}
	if (len==0){
		LM_WARN("0 len (offset=%d)\n", offset);
	}
	
	tmp=pkg_malloc(sizeof(struct lump));
	if (tmp==0){
		LM_ERR("out of memory\n");
		return 0;
	}
	memset(tmp,0,sizeof(struct lump));
	tmp->op=LUMP_DEL;
	tmp->type=type;
	tmp->u.offset=offset;
	tmp->len=len;
	prev=0;
	/* check to see whether this might be a body lump */
	if ((msg->eoh) && (offset> (int)(msg->eoh-msg->buf)))
		list=&msg->body_lumps;
	else
		list=&msg->add_rm;
	for (t=*list;t; prev=t, t=t->next){
		/* insert it sorted after offset */
		if (((t->op==LUMP_DEL)||(t->op==LUMP_NOP))&&(t->u.offset>offset))
			break;
	}
	tmp->next=t;
	if (prev) prev->next=tmp;
	else *list=tmp;
	return tmp;
}



/* add an anchor
 * WARNING: this function adds the lump either to the msg->add_rm or
 * msg->body_lumps list, depending on the offset being greater than msg->eoh,
 * so msg->eoh must be parsed (parse with HDR_EOH) if you think your lump
 *  might affect the body!! */
struct lump* anchor_lump(struct sip_msg* msg, int offset, int len, enum _hdr_types_t type)
{
	struct lump* tmp;
	struct lump* prev, *t;
	struct lump** list;

	
	/* extra checks */
	if (offset>msg->len){
		LM_CRIT("offset exceeds message size (%d > %d) aborting...\n",
					offset, msg->len);
		abort();
	}
	if (len){
		LM_WARN("len !=0 (%d)\n", len);
		if (offset+len>msg->len)
			LM_WARN("offset + len exceeds message size (%d + %d > %d)\n",
					offset, len,  msg->len);
	}
	
	tmp=pkg_malloc(sizeof(struct lump));
	if (tmp==0){
		ser_error=E_OUT_OF_MEM;
		LM_ERR("out of memory\n");
		return 0;
	}
	memset(tmp,0,sizeof(struct lump));
	tmp->op=LUMP_NOP;
	tmp->type=type;
	tmp->u.offset=offset;
	tmp->len=len;
	prev=0;
	/* check to see whether this might be a body lump */
	if ((msg->eoh) && (offset> (int)(msg->eoh-msg->buf)))
		list=&msg->body_lumps;
	else
		list=&msg->add_rm;
		
	for (t=*list;t; prev=t, t=t->next){
		/* insert it sorted after offset */
		if (((t->op==LUMP_DEL)||(t->op==LUMP_NOP))&&(t->u.offset>offset))
			break;
	}
	tmp->next=t;
	
	if (prev) prev->next=tmp;
	else *list=tmp;
	return tmp;
}

/* add an anchor
 * Similar to anchor_lump() but this function checks whether or not a lump
 * has already been added to the same position. If an existing lump is found
 * then it is returned without adding a new one and is_ref is set to 1.
 *
 * WARNING: this function adds the lump either to the msg->add_rm or
 * msg->body_lumps list, depending on the offset being greater than msg->eoh,
 * so msg->eoh must be parsed (parse with HDR_EOH) if you think your lump
 *  might affect the body!! */
struct lump* anchor_lump2(struct sip_msg* msg, int offset, int len, enum _hdr_types_t type,
		int *is_ref)
{
	struct lump* tmp;
	struct lump* prev, *t;
	struct lump** list;

	
	/* extra checks */
	if (offset>msg->len){
		LM_CRIT("offset exceeds message size (%d > %d) aborting...\n",
					offset, msg->len);
		abort();
	}
	if (len){
		LM_WARN("len !=0 (%d)\n", len);
		if (offset+len>msg->len)
			LM_WARN("offset + len exceeds message size (%d + %d > %d)\n",
					offset, len,  msg->len);
	}
	
	prev=0;
	/* check to see whether this might be a body lump */
	if ((msg->eoh) && (offset> (int)(msg->eoh-msg->buf)))
		list=&msg->body_lumps;
	else
		list=&msg->add_rm;
		
	for (t=*list;t; prev=t, t=t->next){
		/* insert it sorted after offset */
		if (((t->op==LUMP_DEL)||(t->op==LUMP_NOP))&&(t->u.offset>=offset))
			break;
	}
	if (t && (t->u.offset==offset)) {
		/* A lump with the same offset is found */
		*is_ref=1;
		return t;
	}

	tmp=pkg_malloc(sizeof(struct lump));
	if (tmp==0){
		ser_error=E_OUT_OF_MEM;
		LM_ERR("out of memory\n");
		return 0;
	}
	memset(tmp,0,sizeof(struct lump));
	tmp->op=LUMP_NOP;
	tmp->type=type;
	tmp->u.offset=offset;
	tmp->len=len;

	tmp->next=t;
	
	if (prev) prev->next=tmp;
	else *list=tmp;

	*is_ref=0;
	return tmp;
}


void free_lump(struct lump* lmp)
{
	if (lmp && (lmp->op==LUMP_ADD)){
		if (lmp->u.value){
			if (lmp->flags &(LUMPFLAG_DUPED|LUMPFLAG_SHMEM)){
				LM_CRIT("non free-able lump: %p flags=%x\n", lmp, lmp->flags);
				abort();
			}else{
				pkg_free(lmp->u.value);
				lmp->u.value=0;
				lmp->len=0;
			}
		}
	}
}



void free_lump_list(struct lump* l)
{
	struct lump* t, *r, *foo,*crt;
	t=l;
	while(t){
		crt=t;
		t=t->next;
	/*
		 dangerous recursive clean
		if (crt->before) free_lump_list(crt->before);
		if (crt->after)  free_lump_list(crt->after);
	*/
		/* no more recursion, clean after and before and that's it */
		r=crt->before;
		while(r){
			foo=r; r=r->before;
			free_lump(foo);
			pkg_free(foo);
		}
		r=crt->after;
		while(r){
			foo=r; r=r->after;
			free_lump(foo);
			pkg_free(foo);
		}
		
		/*clean current elem*/
		free_lump(crt);
		pkg_free(crt);
	}
}

/* free (shallow-ly) a lump and its after/before lists */
static void free_shallow_lump( struct lump *l )
{
	struct lump *r, *foo;

	r=l->before;
	while(r){
		foo=r; r=r->before;
		pkg_free(foo);
	}
	r=l->after;
	while(r){
		foo=r; r=r->after;
		pkg_free(foo);
	}
	pkg_free(l);
}

/* duplicate (shallow-ly) a lump list into pkg memory */
static struct lump *dup_lump_list_r( struct lump *l, 
				enum lump_dir dir, int *error)
{
	int deep_error;
	struct lump *new_lump;

	deep_error=0; /* optimist: assume success in recursion */
	/* if at list end, terminate recursion successfully */
	if (!l) { *error=0; return 0; }
	/* otherwise duplicate current element */
	new_lump=pkg_malloc(sizeof(struct lump));
	if (!new_lump) { *error=1; return 0; }

	memcpy(new_lump, l, sizeof(struct lump));
	new_lump->flags=LUMPFLAG_DUPED;
	new_lump->next=new_lump->before=new_lump->after=0;

	switch(dir) {
		case LD_NEXT:	
				new_lump->before=dup_lump_list_r(l->before, 
								LD_BEFORE, &deep_error);
				if (deep_error) goto deeperror;
				new_lump->after=dup_lump_list_r(l->after, 
								LD_AFTER, &deep_error);
				if (deep_error) goto deeperror;
				new_lump->next=dup_lump_list_r(l->next, 
								LD_NEXT, &deep_error);
				break;
		case LD_BEFORE:
				new_lump->before=dup_lump_list_r(l->before, 
								LD_BEFORE, &deep_error);
				break;
		case LD_AFTER:
				new_lump->after=dup_lump_list_r(l->after, 
								LD_AFTER, &deep_error);
				break;
		default:
				LM_CRIT("unknown dir: %d\n", dir );
				deep_error=1;
	}
	if (deep_error) goto deeperror;

	*error=0;
	return new_lump;

deeperror:
	LM_ERR("out of mem\n");
	free_shallow_lump(new_lump);
	*error=1;
	return 0;
}



/* shallow pkg copy of a lump list
 *
 * if either original list empty or error occur returns, 0
 * is returned, pointer to the copy otherwise
 */
struct lump* dup_lump_list( struct lump *l )
{
	int deep_error;

	deep_error=0;
	return dup_lump_list_r(l, LD_NEXT, &deep_error);
}



void free_duped_lump_list(struct lump* l)
{
	struct lump *r, *foo,*crt;
	while(l){
		crt=l;
		l=l->next;

		r=crt->before;
		while(r){
			foo=r; r=r->before;
			/* (+): if a new item was introduced to the shallow-ly
			 * duped list, remove it completely, preserve it
			 * otherwise (it is still referred by original list)
			 */
			if (foo->flags!=LUMPFLAG_DUPED) 
					free_lump(foo);
			pkg_free(foo);
		}
		r=crt->after;
		while(r){
			foo=r; r=r->after;
			if (foo->flags!=LUMPFLAG_DUPED) /* (+) ... see above */
				free_lump(foo);
			pkg_free(foo);
		}
		
		/*clean current elem*/
		if (crt->flags!=LUMPFLAG_DUPED) /* (+) ... see above */
			free_lump(crt);
		pkg_free(crt);
	}
}



void del_nonshm_lump( struct lump** lump_list )
{
	struct lump *r, *foo, *crt, **prev, *prev_r;

	prev = lump_list;
	crt = *lump_list;

	while (crt) {
		if (!(crt->flags&LUMPFLAG_SHMEM)) {
			/* unlink it */
			foo = crt;
			crt = crt->next;
			foo->next = 0;
			/* update the 'next' link of the previous lump */
			*prev = crt;
			/* entire before/after list must be removed */
			free_lump_list( foo );
		} else {
			/* check on before and prev list for non-shmem lumps */
			r = crt->after;
			prev_r = crt;
			while(r){
				foo=r; r=r->after;
				if (!(foo->flags&LUMPFLAG_SHMEM)) {
					prev_r->after = r;
					free_lump(foo);
					pkg_free(foo);
				} else {
					prev_r = foo;
				}
			}
			/* before */
			r = crt->before;
			prev_r = crt;
			while(r){
				foo=r; r=r->before;
				if (!(foo->flags&LUMPFLAG_SHMEM)) {
					prev_r->before = r;
					free_lump(foo);
					pkg_free(foo);
				} else {
					prev_r = foo;
				}
			}
			/* go to next lump */
			prev = &(crt->next);
			crt = crt->next;
		}
	}
}

unsigned int count_applied_lumps(struct lump *ll, int type)
{
	unsigned int n = 0;
	struct lump *l = 0;

	for(l=ll; l; l=l->next) {
		if (l->op==LUMP_NOP && l->type==type) {
			if (l->after && l->after->op==LUMP_ADD_OPT) {
				if (LUMP_IS_COND_TRUE(l->after)) {
					n++;
				}
			} else {
				n++;
			}
		}
	}
	return n;
}

/**
 * remove a lump in a root list
 * - it doesn't look for lumps added as before/after rule
 * - destroys the entire lump, with associated before/after rules
 */
int remove_lump(sip_msg_t *msg, struct lump *l)
{
	struct lump *t = NULL;
	struct lump *prev = NULL;
	struct lump **list = NULL;

	list=&msg->add_rm;	
	for (t=*list; t; prev=t, t=t->next) {
		if(t==l)
			break;
	}
	if(t==NULL) {
		list=&msg->body_lumps;
		for (t=*list; t; prev=t, t=t->next) {
			if(t==l)
				break;
		}
	}
	if(t!=NULL) {
		if(prev==NULL) {
			*list = t->next;
		} else {
			prev->next = t->next;
		}
		/* detach and free all its content */
		t->next = NULL;
		free_lump_list(t);
		return 1;
	}
	return 0;
}
