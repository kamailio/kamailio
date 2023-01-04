/*
 * Copyright (C) 2009 Daniel-Constantin Mierla (asipto.com)
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*!
 * \file
 * \brief Kamailio core :: Extended AVPs
 * \ingroup core
 * Module: \ref core
 */

#include <stdio.h>
#include <string.h>

#include "mem/mem.h"
#include "mem/shm_mem.h"
#include "dprint.h"
#include "hashes.h"
#include "xavp.h"

/*! XAVP list head */
static sr_xavp_t *_xavp_list_head = 0;
/*! Pointer to XAVP current list */
static sr_xavp_t **_xavp_list_crt = &_xavp_list_head;

/*! XAVU list head */
static sr_xavp_t *_xavu_list_head = 0;
/*! Pointer to XAVP current list */
static sr_xavp_t **_xavu_list_crt = &_xavu_list_head;

/*! XAVI list head */
static sr_xavp_t *_xavi_list_head = 0;
/*! Pointer to XAVI current list */
static sr_xavp_t **_xavi_list_crt = &_xavi_list_head;

/*! Helper functions */
static sr_xavp_t *xavp_get_internal(str *name, sr_xavp_t **list, int idx, sr_xavp_t **prv);
static int xavp_rm_internal(str *name, sr_xavp_t **head, int idx);


void xavp_shm_free(void *p)
{
	shm_free(p);
}

void xavp_shm_free_unsafe(void *p)
{
	shm_free_unsafe(p);
}


void xavp_free(sr_xavp_t *xa)
{
	if(xa==NULL) {
		return;
	}
	if(xa->val.type == SR_XTYPE_DATA) {
		if(xa->val.v.data!=NULL && xa->val.v.data->pfree!=NULL) {
			xa->val.v.data->pfree(xa->val.v.data->p, xavp_shm_free);
			shm_free(xa->val.v.data);
		}
	} else if(xa->val.type == SR_XTYPE_SPTR) {
		if(xa->val.v.vptr) {
			shm_free(xa->val.v.vptr);
		}
	} else if(xa->val.type == SR_XTYPE_XAVP) {
		xavp_destroy_list(&xa->val.v.xavp);
	}
	shm_free(xa);
}

void xavp_free_unsafe(sr_xavp_t *xa)
{
	if(xa==NULL) {
		return;
	}
	if(xa->val.type == SR_XTYPE_DATA) {
		if(xa->val.v.data!=NULL && xa->val.v.data->pfree!=NULL) {
			xa->val.v.data->pfree(xa->val.v.data->p, xavp_shm_free_unsafe);
			shm_free_unsafe(xa->val.v.data);
		}
	} else if(xa->val.type == SR_XTYPE_SPTR) {
		if(xa->val.v.vptr) {
			shm_free_unsafe(xa->val.v.vptr);
		}
	} else if(xa->val.type == SR_XTYPE_XAVP) {
		xavp_destroy_list_unsafe(&xa->val.v.xavp);
	}
	shm_free_unsafe(xa);
}

static sr_xavp_t *xavp_new_value(str *name, sr_xval_t *val)
{
	sr_xavp_t *avp;
	int size;
	unsigned int id;

	if(name==NULL || name->s==NULL || name->len<=0 || val==NULL)
		return NULL;
	id = get_hash1_raw(name->s, name->len);

	size = sizeof(sr_xavp_t) + name->len + 1;
	if(val->type == SR_XTYPE_STR)
		size += val->v.s.len + 1;
	avp = (sr_xavp_t*)shm_malloc(size);
	if(avp==NULL) {
		SHM_MEM_ERROR;
		return NULL;
	}
	memset(avp, 0, size);
	avp->id = id;
	avp->name.s = (char*)avp + sizeof(sr_xavp_t);
	memcpy(avp->name.s, name->s, name->len);
	avp->name.s[name->len] = '\0';
	avp->name.len = name->len;
	memcpy(&avp->val, val, sizeof(sr_xval_t));
	if(val->type == SR_XTYPE_STR)
	{
		avp->val.v.s.s = avp->name.s + avp->name.len + 1;
		memcpy(avp->val.v.s.s, val->v.s.s, val->v.s.len);
		avp->val.v.s.s[val->v.s.len] = '\0';
		avp->val.v.s.len = val->v.s.len;
	}

	return avp;
}

int xavp_add(sr_xavp_t *xavp, sr_xavp_t **list)
{
	if (xavp==NULL) {
		return -1;
	}
	/* Prepend new xavp to the list */
	if(list) {
		xavp->next = *list;
		*list = xavp;
	} else {
		xavp->next = *_xavp_list_crt;
		*_xavp_list_crt = xavp;
	}

	return 0;
}

int xavp_add_last(sr_xavp_t *xavp, sr_xavp_t **list)
{
	sr_xavp_t *prev;
	sr_xavp_t *crt;

	if (xavp==NULL) {
		return -1;
	}

	crt = xavp_get_internal(&xavp->name, list, 0, 0);

	prev = NULL;

	while(crt) {
		prev = crt;
		crt = xavp_get_next(prev);
	}

	if(prev==NULL) {
		/* Prepend new xavp to the list */
		if(list) {
			xavp->next = *list;
			*list = xavp;
		} else {
			xavp->next = *_xavp_list_crt;
			*_xavp_list_crt = xavp;
		}
	} else {
		xavp->next = prev->next;
		prev->next = xavp;
	}

	return 0;
}

int xavp_add_after(sr_xavp_t *nxavp, sr_xavp_t *pxavp)
{
	if (nxavp==NULL) {
		return -1;
	}

	if(pxavp==NULL) {
		nxavp->next = *_xavp_list_crt;
		*_xavp_list_crt = nxavp;
	} else {
		nxavp->next = pxavp->next;
		pxavp->next = nxavp;
	}

	return 0;
}

sr_xavp_t *xavp_add_value(str *name, sr_xval_t *val, sr_xavp_t **list)
{
	sr_xavp_t *avp=0;

	avp = xavp_new_value(name, val);
	if (avp==NULL)
		return NULL;

	/* Prepend new value to the list */
	if(list) {
		avp->next = *list;
		*list = avp;
	} else {
		avp->next = *_xavp_list_crt;
		*_xavp_list_crt = avp;
	}

	return avp;
}

sr_xavp_t *xavp_add_value_after(str *name, sr_xval_t *val, sr_xavp_t *pxavp)
{
	sr_xavp_t *avp=0;

	avp = xavp_new_value(name, val);
	if (avp==NULL)
		return NULL;

	/* link new xavp */
	if(pxavp) {
		avp->next = pxavp->next;
		pxavp->next = avp;
	} else {
		avp->next = *_xavp_list_crt;
		*_xavp_list_crt = avp;
	}

	return avp;
}

sr_xavp_t *xavp_add_xavp_value(str *rname, str *name, sr_xval_t *val, sr_xavp_t **list)
{
	sr_xavp_t *ravp=0;
	sr_xavp_t *cavp=0;
	sr_xval_t rval;

	cavp = xavp_new_value(name, val);
	if (cavp==NULL)
		return NULL;

	memset(&rval, 0, sizeof(sr_xval_t));
	rval.type = SR_XTYPE_XAVP;
	rval.v.xavp = cavp;

	ravp = xavp_new_value(rname, &rval);
	if (ravp==NULL) {
		xavp_destroy_list(&cavp);
		return NULL;
	}

	/* Prepend new value to the list */
	if(list) {
		ravp->next = *list;
		*list = ravp;
	} else {
		ravp->next = *_xavp_list_crt;
		*_xavp_list_crt = ravp;
	}

	return ravp;
}

sr_xavp_t *xavp_set_value(str *name, int idx, sr_xval_t *val, sr_xavp_t **list)
{
	sr_xavp_t *avp;
	sr_xavp_t *cur;
	sr_xavp_t *prv=0;

	if(val==NULL)
		return NULL;

	/* Find the current value */
	cur = xavp_get_internal(name, list, idx, &prv);
	if(cur==NULL)
		return NULL;

	avp = xavp_new_value(name, val);
	if (avp==NULL)
		return NULL;

	/* Replace the current value with the new */
	avp->next = cur->next;
	if(prv)
		prv->next = avp;
	else if(list)
		*list = avp;
	else
		*_xavp_list_crt = avp;

	xavp_free(cur);

	return avp;
}

static sr_xavp_t *xavp_get_internal(str *name, sr_xavp_t **list, int idx, sr_xavp_t **prv)
{
	sr_xavp_t *avp;
	unsigned int id;
	int n = 0;

	if(name==NULL || name->s==NULL)
		return NULL;
	id = get_hash1_raw(name->s, name->len);

	if(list && *list)
		avp = *list;
	else
		avp = *_xavp_list_crt;
	while(avp)
	{
		if(avp->id==id && avp->name.len==name->len
				&& strncmp(avp->name.s, name->s, name->len)==0)
		{
			if(idx==n)
				return avp;
			n++;
		}
		if(prv)
			*prv = avp;
		avp = avp->next;
	}
	return NULL;
}

sr_xavp_t *xavp_get(str *name, sr_xavp_t *start)
{
	return xavp_get_internal(name, (start)?&start:NULL, 0, NULL);
}

sr_xavp_t *xavp_get_by_index(str *name, int idx, sr_xavp_t **start)
{
	return xavp_get_internal(name, start, idx, NULL);
}

sr_xavp_t *xavp_get_next(sr_xavp_t *start)
{
	sr_xavp_t *avp;

	if(start==NULL)
		return NULL;

	avp = start->next;
	while(avp)
	{
		if(avp->id==start->id && avp->name.len==start->name.len
				&& strncmp(avp->name.s, start->name.s, start->name.len)==0)
			return avp;
		avp=avp->next;
	}

	return NULL;
}

sr_xavp_t *xavp_get_last(str *xname, sr_xavp_t **list)
{
	sr_xavp_t *prev;
	sr_xavp_t *crt;

	crt = xavp_get_internal(xname, list, 0, 0);

	prev = NULL;

	while(crt) {
		prev = crt;
		crt = xavp_get_next(prev);
	}

	return prev;
}

int xavp_rm(sr_xavp_t *xa, sr_xavp_t **head)
{
	sr_xavp_t *avp;
	sr_xavp_t *prv=0;

	if(head!=NULL)
		avp = *head;
	else
		avp=*_xavp_list_crt;

	while(avp)
	{
		if(avp==xa)
		{
			if(prv)
				prv->next=avp->next;
			else if(head!=NULL)
				*head = avp->next;
			else
				*_xavp_list_crt = avp->next;
			xavp_free(avp);
			return 1;
		}
		prv=avp; avp=avp->next;
	}
	return 0;
}

/* Remove xavps
 * idx: <0 remove all xavps with the same name
 *      >=0 remove only the specified index xavp
 * Returns number of xavps that were deleted
 */
static int xavp_rm_internal(str *name, sr_xavp_t **head, int idx)
{
	sr_xavp_t *avp;
	sr_xavp_t *foo;
	sr_xavp_t *prv=0;
	unsigned int id;
	int n=0;
	int count=0;

	if(name==NULL || name->s==NULL || name->len<=0)
		return 0;

	id = get_hash1_raw(name->s, name->len);
	if(head!=NULL)
		avp = *head;
	else
		avp = *_xavp_list_crt;
	while(avp)
	{
		foo = avp;
		avp=avp->next;
		if(foo->id==id && foo->name.len==name->len
				&& strncmp(foo->name.s, name->s, name->len)==0)
		{
			if(idx<0 || idx==n)
			{
				if(prv!=NULL)
					prv->next=foo->next;
				else if(head!=NULL)
					*head = foo->next;
				else
					*_xavp_list_crt = foo->next;
				xavp_free(foo);
				if(idx>=0)
					return 1;
				count++;
			} else {
				prv = foo;
			}
			n++;
		} else {
			prv = foo;
		}
	}
	return count;
}

int xavp_rm_by_name(str *name, int all, sr_xavp_t **head)
{
	return xavp_rm_internal(name, head, -1*all);
}

int xavp_rm_by_index(str *name, int idx, sr_xavp_t **head)
{
	if (idx<0)
		return 0;
	return xavp_rm_internal(name, head, idx);
}

int xavp_rm_child_by_index(str *rname, str *cname, int idx)
{
	sr_xavp_t *avp=NULL;

	if (idx<0) {
		return 0;
	}
	avp = xavp_get(rname, NULL);

	if(avp == NULL || avp->val.type!=SR_XTYPE_XAVP) {
		return 0;
	}
	return xavp_rm_internal(cname, &avp->val.v.xavp, idx);
}

int xavp_count(str *name, sr_xavp_t **start)
{
	sr_xavp_t *avp;
	unsigned int id;
	int n = 0;

	if(name==NULL || name->s==NULL || name->len<=0)
		return -1;
	id = get_hash1_raw(name->s, name->len);

	if(start)
		avp = *start;
	else
		avp=*_xavp_list_crt;
	while(avp)
	{
		if(avp->id==id && avp->name.len==name->len
				&& strncmp(avp->name.s, name->s, name->len)==0)
		{
			n++;
		}
		avp=avp->next;
	}

	return n;
}

/**
 * Left shift xavps
 */
int xavp_lshift(str *name, sr_xavp_t **head, int idx)
{
	sr_xavp_t *avp;
	sr_xavp_t *lhead = NULL;
	sr_xavp_t *lhead_last = NULL;
	sr_xavp_t *ltail = NULL;
	sr_xavp_t *ltail_last = NULL;
	sr_xavp_t *crt=0;
	sr_xavp_t *prv=0;
	unsigned int id;
	int n=0;
	int xcnt;

	if(name==NULL || name->s==NULL || name->len<=0) {
		return 0;
	}

	if(idx==0) {
		return 1;
	}
	xcnt = xavp_count(name, head);
	if(xcnt <= 0) {
		return -2;
	}
	while(idx < 0) {
		idx = xcnt + idx;
	}
	if(idx==0) {
		return 1;
	}
	idx = idx % xcnt;
	if(idx==0) {
		return 1;
	}

	id = get_hash1_raw(name->s, name->len);
	if(head!=NULL)
		avp = *head;
	else
		avp = *_xavp_list_crt;
	while(avp)
	{
		crt = avp;
		avp=avp->next;
		if(crt->id==id && crt->name.len==name->len
				&& strncmp(crt->name.s, name->s, name->len)==0)
		{
			if(prv!=NULL)
				prv->next=crt->next;
			else if(head!=NULL)
				*head = crt->next;
			else
				*_xavp_list_crt = crt->next;
			crt->next = NULL;
			if(n < idx) {
				if(ltail==NULL) {
					ltail = crt;
				}
				if(ltail_last!=NULL) {
					ltail_last->next = crt;
				}
				ltail_last = crt;
			} else {
				if(lhead==NULL) {
					lhead = crt;
				}
				if(lhead_last!=NULL) {
					lhead_last->next = crt;
				}
				lhead_last = crt;
			}
			n++;
		} else {
			prv = crt;
		}
	}

	if(lhead_last) {
		lhead_last->next = ltail;
	}

	if(head!=NULL) {
		if(ltail_last) {
			ltail_last->next = *head;
		}
		*head = lhead;
	} else {
		if(ltail_last) {
			ltail_last->next = *_xavp_list_crt;
		}
		*_xavp_list_crt = lhead;
	}

	return 0;
}

void xavp_destroy_list_unsafe(sr_xavp_t **head)
{
	sr_xavp_t *avp, *foo;

	avp = *head;
	while(avp)
	{
		foo = avp;
		avp = avp->next;
		xavp_free_unsafe(foo);
	}
	*head = 0;
}


void xavp_destroy_list(sr_xavp_t **head)
{
	sr_xavp_t *avp, *foo;

	LM_DBG("destroying xavp list %p\n", *head);
	avp = *head;
	while(avp)
	{
		foo = avp;
		avp = avp->next;
		xavp_free(foo);
	}
	*head = 0;
}


void xavp_reset_list(void)
{
	assert(_xavp_list_crt!=0 );

	if (_xavp_list_crt!=&_xavp_list_head)
		_xavp_list_crt=&_xavp_list_head;
	xavp_destroy_list(_xavp_list_crt);
}


sr_xavp_t **xavp_set_list(sr_xavp_t **head)
{
	sr_xavp_t **avp;

	assert(_xavp_list_crt!=0);

	avp = _xavp_list_crt;
	_xavp_list_crt = head;
	return avp;
}

sr_xavp_t **xavp_get_crt_list(void)
{
	assert(_xavp_list_crt!=0);
	return _xavp_list_crt;
}

void xavx_print_list_content(char *name, sr_xavp_t **head, sr_xavp_t **rlist, int level)
{
	sr_xavp_t *avp=0;
	sr_xavp_t *start=0;

	if(head!=NULL) {
		start = *head;
	} else {
		start=*rlist;
	}
	LM_INFO("+++++ start %s list: %p (%p) (level=%d)\n", name, start, head, level);
	avp = start;
	while(avp)
	{
		LM_INFO("     *** (l:%d - %p) %s name: %s\n", level, avp, name, avp->name.s);
		LM_INFO("     %s id: %u\n", name, avp->id);
		LM_INFO("     %s value type: %d\n", name, avp->val.type);
		switch(avp->val.type) {
			case SR_XTYPE_NULL:
				LM_INFO("     %s value: <null>\n", name);
			break;
			case SR_XTYPE_STR:
				LM_INFO("     %s value (str): %s\n", name, avp->val.v.s.s);
			break;
			case SR_XTYPE_TIME:
				LM_INFO("     %s value (time): %lu\n", name,
						(long unsigned int)avp->val.v.t);
			break;
			case SR_XTYPE_LONG:
				LM_INFO("     %s value (long): %ld\n", name, avp->val.v.l);
			break;
			case SR_XTYPE_LLONG:
				LM_INFO("     %s value (llong): %lld\n", name, avp->val.v.ll);
			break;
			case SR_XTYPE_XAVP:
				LM_INFO("     %s value: <xavp:%p>\n", name, avp->val.v.xavp);
				xavx_print_list_content(name, &avp->val.v.xavp, rlist, level+1);
			break;
			case SR_XTYPE_VPTR:
				LM_INFO("     %s value: <vptr:%p>\n", name, avp->val.v.vptr);
			break;
			case SR_XTYPE_SPTR:
				LM_INFO("     %s value: <sptr:%p>\n", name, avp->val.v.vptr);
			break;
			case SR_XTYPE_DATA:
				LM_INFO("     %s value: <data:%p>\n", name, avp->val.v.data);
			break;
		}
		LM_INFO("     *** (l:%d - %p) end\n", level, avp);
		avp = avp->next;
	}
	LM_INFO("----- end %s list: %p (level=%d)\n", name, start, level);
}

void xavp_print_list_content(sr_xavp_t **head, int level)
{
	xavx_print_list_content("XAVP", head, _xavp_list_crt, level);
}

void xavp_print_list(sr_xavp_t **head)
{
	xavp_print_list_content(head, 0);
}

/**
 * returns a list of str with key names.
 * Example:
 * If we have this structure
 * $xavp(test=>one) = 1
 * $xavp(test[0]=>two) = "2"
 * $xavp(test[0]=>three) = 3
 * $xavp(test[0]=>four) = $xavp(whatever)
 * $xavp(test[0]=>two) = "other 2"
 *
 * xavp_get_list_keys_names(test[0]) returns
 * {"one", "two", "three", "four"}
 *
 * free the struct str_list afterwards
 * but do *NO* free the strings inside
 */
struct str_list *xavp_get_list_key_names(sr_xavp_t *xavp)
{
	sr_xavp_t *avp = NULL;
	struct str_list *result = NULL;
	struct str_list *r = NULL;
	struct str_list *f = NULL;
	int total = 0;

	if(xavp==NULL){
		LM_ERR("xavp is NULL\n");
		return 0;
	}

	if(xavp->val.type!=SR_XTYPE_XAVP){
		LM_ERR("%s not xavp?\n", xavp->name.s);
		return 0;
	}

	avp = xavp->val.v.xavp;

	if (avp)
	{
		result = (struct str_list*)pkg_malloc(sizeof(struct str_list));
		if (result==NULL) {
			PKG_MEM_ERROR;
			return 0;
		}
		r = result;
		r->s.s = avp->name.s;
		r->s.len = avp->name.len;
		r->next = NULL;
		avp = avp->next;
	}

	while(avp)
	{
		f = result;
		while(f)
		{
			if((avp->name.len==f->s.len)&&
				(strncmp(avp->name.s, f->s.s, f->s.len)==0))
			{
				break; /* name already on list */
			}
			f = f->next;
		}
		if (f==NULL)
		{
			r = append_str_list(avp->name.s, avp->name.len, &r, &total);
			if(r==NULL){
				while(result){
					r = result;
					result = result->next;
					pkg_free(r);
				}
				return 0;
			}
		}
		avp = avp->next;
	}
	return result;
}

sr_xavp_t *xavp_clone_level_nodata(sr_xavp_t *xold)
{
	return xavp_clone_level_nodata_with_new_name(xold, &xold->name);
}

/**
 * clone the xavp without values that are custom data
 * - only one list level is cloned, other sublists are ignored
 */
sr_xavp_t *xavp_clone_level_nodata_with_new_name(sr_xavp_t *xold, str *dst_name)
{
	sr_xavp_t *xnew = NULL;
	sr_xavp_t *navp = NULL;
	sr_xavp_t *oavp = NULL;
	sr_xavp_t *pavp = NULL;

	if(xold == NULL)
	{
		return NULL;
	}
	if(xold->val.type==SR_XTYPE_DATA || xold->val.type==SR_XTYPE_SPTR)
	{
		LM_INFO("xavp value type is 'data' - ignoring in clone\n");
		return NULL;
	}
	xnew = xavp_new_value(dst_name, &xold->val);
	if(xnew==NULL)
	{
		LM_ERR("cannot create cloned root xavp\n");
		return NULL;
	}
	LM_DBG("cloned root xavp [%.*s] >> [%.*s]\n", xold->name.len, xold->name.s, dst_name->len, dst_name->s);

	if(xold->val.type!=SR_XTYPE_XAVP)
	{
		return xnew;
	}

	xnew->val.v.xavp = NULL;
	oavp = xold->val.v.xavp;

	while(oavp)
	{
		if(oavp->val.type!=SR_XTYPE_DATA && oavp->val.type!=SR_XTYPE_XAVP
				&& oavp->val.type!=SR_XTYPE_SPTR)
		{
			navp =  xavp_new_value(&oavp->name, &oavp->val);
			if(navp==NULL)
			{
				LM_ERR("cannot create cloned embedded xavp\n");
				if(xnew->val.v.xavp != NULL) {
					xavp_destroy_list(&xnew->val.v.xavp);
				}
				shm_free(xnew);
				return NULL;
			}
			LM_DBG("cloned inner xavp [%.*s]\n", oavp->name.len, oavp->name.s);
			if(xnew->val.v.xavp == NULL)
			{
				/* link to val in head xavp */
				xnew->val.v.xavp = navp;
			} else {
				/* link to prev xavp in the list */
				pavp->next = navp;
			}
			pavp = navp;
		}
		oavp = oavp->next;
	}

	if(xnew->val.v.xavp == NULL)
	{
		shm_free(xnew);
		return NULL;
	}

	return xnew;
}

int xavp_insert(sr_xavp_t *xavp, int idx, sr_xavp_t **list)
{
	sr_xavp_t *crt = 0;
	sr_xavp_t *lst = 0;
	sr_xval_t val;
	int n = 0;
	int i = 0;

	if(xavp==NULL) {
		return -1;
	}

	crt = xavp_get_internal(&xavp->name, list, 0, NULL);

	if (idx == 0 && (!crt || crt->val.type != SR_XTYPE_NULL))
		return xavp_add(xavp, list);

	while(crt!=NULL && n<idx) {
		lst = crt;
		n++;
		crt = xavp_get_next(lst);
	}

	if (crt && crt->val.type == SR_XTYPE_NULL) {
		xavp->next = crt->next;
		crt->next = xavp;

		xavp_rm(crt, list);
		return 0;
	}

	memset(&val, 0, sizeof(sr_xval_t));
	val.type = SR_XTYPE_NULL;
	for(i=0; i<idx-n; i++) {
		crt = xavp_new_value(&xavp->name, &val);
		if(crt==NULL)
			return -1;
		if (lst == NULL) {
			xavp_add(crt, list);
		} else {
			crt->next = lst->next;
			lst->next = crt;
		}
		lst = crt;
	}

	if(lst==NULL) {
		LM_ERR("cannot link the xavp\n");
		return -1;
	}
	xavp->next = lst->next;
	lst->next = xavp;

	return 0;
}

sr_xavp_t *xavp_extract(str *name, sr_xavp_t **list)
{
	sr_xavp_t *avp = 0;
	sr_xavp_t *foo;
	sr_xavp_t *prv = 0;
	unsigned int id;

	if(name==NULL || name->s==NULL || name->len<=0) {
		if(list!=NULL) {
			avp = *list;
			if(avp!=NULL) {
				*list = avp->next;
				avp->next = NULL;
			}
		} else {
			avp = *_xavp_list_crt;
			if(avp!=NULL) {
				*_xavp_list_crt = avp->next;
				avp->next = NULL;
			}
		}

		return avp;
	}

	id = get_hash1_raw(name->s, name->len);
	if(list!=NULL)
		avp = *list;
	else
		avp = *_xavp_list_crt;
	while(avp)
	{
		foo = avp;
		avp=avp->next;
		if(foo->id==id && foo->name.len==name->len
				&& strncmp(foo->name.s, name->s, name->len)==0)
		{
			if(prv!=NULL)
				prv->next=foo->next;
			else if(list!=NULL)
				*list = foo->next;
			else
				*_xavp_list_crt = foo->next;
			foo->next = NULL;
			return foo;
		} else {
			prv = foo;
		}
	}
	return NULL;
}

/**
 * return child node of an xavp
 * - $xavp(rname=>cname)
 */
sr_xavp_t* xavp_get_child(str *rname, str *cname)
{
	sr_xavp_t *ravp=NULL;

	ravp = xavp_get(rname, NULL);
	if(ravp==NULL || ravp->val.type!=SR_XTYPE_XAVP)
		return NULL;

	return xavp_get(cname, ravp->val.v.xavp);
}


/**
 * return child node of an xavp if it has int value
 * - $xavp(rname=>cname)
 */
sr_xavp_t* xavp_get_child_with_ival(str *rname, str *cname)
{
	sr_xavp_t *vavp=NULL;

	vavp = xavp_get_child(rname, cname);

	if(vavp==NULL || vavp->val.type!=SR_XTYPE_LONG)
		return NULL;

	return vavp;
}


/**
 * return child node of an xavp if it has string value
 * - $xavp(rname=>cname)
 */
sr_xavp_t* xavp_get_child_with_sval(str *rname, str *cname)
{
	sr_xavp_t *vavp=NULL;

	vavp = xavp_get_child(rname, cname);

	if(vavp==NULL || vavp->val.type!=SR_XTYPE_STR)
		return NULL;

	return vavp;
}

/**
 * Set the value of the first xavp rname with first child xavp cname
 * - replace if it exits; add if it doesn't exist
 * - config operations:
 *   $xavp(rxname=>cname) = xval;
 *     or:
 *   $xavp(rxname[0]=>cname[0]) = xval;
 */
int xavp_set_child_xval(str *rname, str *cname, sr_xval_t *xval)
{
	sr_xavp_t *ravp=NULL;
	sr_xavp_t *cavp=NULL;

	ravp = xavp_get(rname, NULL);
	if(ravp) {
		if(ravp->val.type != SR_XTYPE_XAVP) {
			/* first root xavp does not have xavp list value - remove it */
			xavp_rm(ravp, NULL);
			/* add a new xavp in the root list with a child */
			if(xavp_add_xavp_value(rname, cname, xval, NULL)==NULL) {
				return -1;
			}
		} else {
			/* first root xavp has an xavp list value */
			cavp = xavp_get(cname, ravp->val.v.xavp);
			if(cavp) {
				/* child xavp with same name - remove it */
				/* todo: update in place for int or if allocated size fits */
				xavp_rm(cavp, &ravp->val.v.xavp);
			}
			if(xavp_add_value(cname, xval, &ravp->val.v.xavp)==NULL) {
				return -1;
			}
		}
	} else {
		/* no xavp with rname in root list found */
		if(xavp_add_xavp_value(rname, cname, xval, NULL)==NULL) {
			return -1;
		}
	}

	return 0;
}

/**
 *
 */
int xavp_set_child_ival(str *rname, str *cname, long ival)
{
	sr_xval_t xval;

	memset(&xval, 0, sizeof(sr_xval_t));
	xval.type = SR_XTYPE_LONG;
	xval.v.l = ival;

	return xavp_set_child_xval(rname, cname, &xval);
}

/**
 *
 */
int xavp_set_child_sval(str *rname, str *cname, str *sval)
{
	sr_xval_t xval;

	memset(&xval, 0, sizeof(sr_xval_t));
	xval.type = SR_XTYPE_STR;
	xval.v.s = *sval;

	return xavp_set_child_xval(rname, cname, &xval);
}

/**
 * serialize the values in subfields of an xavp in name=value; format
 * - rname - name of the root list xavp
 * - obuf - buffer were to write the output
 * - olen - the size of obuf
 * return: 0 - not found; -1 - error; >0 - length of output
 */

int xavp_serialize_fields(str *rname, char *obuf, int olen)
{
	sr_xavp_t *ravp = NULL;
	sr_xavp_t *avp = NULL;
	str ostr;
	int rlen;

	ravp = xavp_get(rname, NULL);
	if(ravp==NULL || ravp->val.type!=SR_XTYPE_XAVP) {
		/* not found or not holding subfields */
		return 0;
	}

	rlen = 0;
	ostr.s = obuf;
	avp = ravp->val.v.xavp;
	while(avp) {
		switch(avp->val.type) {
			case SR_XTYPE_LONG:
				LM_DBG("     XAVP long int value: %ld\n", avp->val.v.l);
				ostr.len = snprintf(ostr.s, olen-rlen, "%.*s=%lu;",
						avp->name.len, avp->name.s, (unsigned long)avp->val.v.l);
				if(ostr.len<=0 || ostr.len>=olen-rlen) {
					LM_ERR("failed to serialize int value (%d/%d\n",
							ostr.len, olen-rlen);
					return -1;
				}
			break;
			case SR_XTYPE_STR:
				LM_DBG("     XAVP str value: %s\n", avp->val.v.s.s);
				if(avp->val.v.s.len == 0) {
					ostr.len = snprintf(ostr.s, olen-rlen, "%.*s;",
						avp->name.len, avp->name.s);
				} else {
					ostr.len = snprintf(ostr.s, olen-rlen, "%.*s=%.*s;",
						avp->name.len, avp->name.s,
						avp->val.v.s.len, avp->val.v.s.s);
				}
				if(ostr.len<=0 || ostr.len>=olen-rlen) {
					LM_ERR("failed to serialize int value (%d/%d\n",
							ostr.len, olen-rlen);
					return -1;
				}
			break;
			default:
				LM_DBG("skipping value type: %d\n", avp->val.type);
				ostr.len = 0;
		}
		if(ostr.len>0) {
			ostr.s += ostr.len;
			rlen += ostr.len;
		}
		avp = avp->next;
	}
	return rlen;
}

/**
 *
 */
/*** XAVU - eXtended Attribute Value Unique pair - implementation ***/

/**
 *
 */
void xavu_print_list_content(sr_xavp_t **head, int level)
{
	xavx_print_list_content("XAVU", head, _xavu_list_crt, level);
}

void xavu_print_list(sr_xavp_t **head)
{
	xavu_print_list_content(head, 0);
}

/**
 *
 */
void xavu_reset_list(void)
{
	assert(_xavu_list_crt!=0 );

	if (_xavu_list_crt!=&_xavu_list_head)
		_xavu_list_crt=&_xavu_list_head;
	xavp_destroy_list(_xavu_list_crt);
}

/**
 *
 */
sr_xavp_t **xavu_set_list(sr_xavp_t **head)
{
	sr_xavp_t **avu;

	assert(_xavu_list_crt!=0);

	avu = _xavu_list_crt;
	_xavu_list_crt = head;
	return avu;
}

/**
 *
 */
sr_xavp_t **xavu_get_crt_list(void)
{
	assert(_xavu_list_crt!=0);
	return _xavu_list_crt;
}

/**
 *
 */
static sr_xavp_t *xavu_get_internal(str *name, sr_xavp_t **list, sr_xavp_t **prv)
{
	sr_xavp_t *avu;
	unsigned int id;

	if(name==NULL || name->s==NULL || name->len<=0) {
		return NULL;
	}

	id = get_hash1_raw(name->s, name->len);

	if(list && *list) {
		avu = *list;
	} else {
		avu = *_xavu_list_crt;
	}
	while(avu) {
		if(avu->id==id && avu->name.len==name->len
				&& strncmp(avu->name.s, name->s, name->len)==0) {
			return avu;
		}
		if(prv) {
			*prv = avu;
		}
		avu = avu->next;
	}
	return NULL;
}

/**
 *
 */
sr_xavp_t *xavu_get(str *name, sr_xavp_t *start)
{
	return xavu_get_internal(name, (start)?&start:NULL, NULL);
}

sr_xavp_t *xavu_lookup(str *name, sr_xavp_t **start)
{
	return xavu_get_internal(name, start, NULL);
}

/**
 *
 */
int xavu_rm(sr_xavp_t *xa, sr_xavp_t **head)
{
	sr_xavp_t *avu;
	sr_xavp_t *prv=0;

	if(head!=NULL)
		avu = *head;
	else
		avu=*_xavu_list_crt;

	while(avu) {
		if(avu==xa) {
			if(prv) {
				prv->next=avu->next;
			} else if(head!=NULL) {
				*head = avu->next;
			} else {
				*_xavu_list_crt = avu->next;
			}
			xavp_free(avu);
			return 1;
		}
		prv=avu; avu=avu->next;
	}
	return 0;
}

/**
 *
 */
int xavu_rm_by_name(str *name, sr_xavp_t **head)
{
	sr_xavp_t *avu;
	sr_xavp_t *foo;
	sr_xavp_t *prv=0;
	unsigned int id;


	if(name==NULL || name->s==NULL || name->len<=0) {
		return -1;
	}

	id = get_hash1_raw(name->s, name->len);
	if(head!=NULL) {
		avu = *head;
	} else {
		avu = *_xavu_list_crt;
	}
	while(avu) {
		foo = avu;
		avu = avu->next;
		if(foo->id==id && foo->name.len==name->len
				&& strncmp(foo->name.s, name->s, name->len)==0) {
			if(prv!=NULL) {
				prv->next=foo->next;
			} else if(head!=NULL) {
				*head = foo->next;
			} else {
				*_xavu_list_crt = foo->next;
			}
			xavp_free(foo);
		} else {
			prv = foo;
		}
	}
	return 0;
}

/**
 *
 */
int xavu_rm_child_by_name(str *rname, str *cname)
{
	sr_xavp_t *avu=NULL;

	avu = xavu_lookup(rname, NULL);

	if(avu == NULL || avu->val.type!=SR_XTYPE_XAVP) {
		return 0;
	}
	return xavu_rm_by_name(cname, &avu->val.v.xavp);
}

/**
 *
 */
sr_xavp_t *xavu_set_xval(str *name, sr_xval_t *val, sr_xavp_t **list)
{
	sr_xavp_t *avu;
	sr_xavp_t *crt;
	sr_xavp_t *prv=0;

	if(val==NULL) {
		return NULL;
	}

	avu = xavp_new_value(name, val);
	if (avu==NULL) {
		return NULL;
	}

	/* find the current value */
	crt = xavu_get_internal(name, list, &prv);
	if(crt==NULL) {
		/* add a new one in the list */
		avu->next = *_xavu_list_crt;
		*_xavu_list_crt = avu;
		return avu;
	}

	/* replace the current value with the new */
	avu->next = crt->next;
	if(prv) {
		prv->next = avu;
	} else if(list) {
		*list = avu;
	} else {
		*_xavu_list_crt = avu;
	}

	xavp_free(crt);

	return avu;
}

/**
 *
 */
sr_xavp_t *xavu_set_ival(str *rname, long ival)
{
	sr_xval_t xval;

	memset(&xval, 0, sizeof(sr_xval_t));
	xval.type = SR_XTYPE_LONG;
	xval.v.l = ival;

	return xavu_set_xval(rname, &xval, NULL);
}

/**
 *
 */
sr_xavp_t *xavu_set_sval(str *rname, str *sval)
{
	sr_xval_t xval;

	memset(&xval, 0, sizeof(sr_xval_t));
	xval.type = SR_XTYPE_STR;
	xval.v.s = *sval;

	return xavu_set_xval(rname, &xval, NULL);
}

/**
 *
 */
sr_xavp_t *xavu_set_xavu_value(str *rname, str *name, sr_xval_t *val, sr_xavp_t **list)
{
	sr_xavp_t *ravu=0;
	sr_xavp_t *cavu=0;
	sr_xval_t rval;

	cavu = xavp_new_value(name, val);
	if (cavu==NULL) {
		return NULL;
	}

	memset(&rval, 0, sizeof(sr_xval_t));
	rval.type = SR_XTYPE_XAVP;
	rval.v.xavp = cavu;

	ravu = xavp_new_value(rname, &rval);
	if (ravu==NULL) {
		xavp_destroy_list(&cavu);
		return NULL;
	}

	/* Prepend new value to the list */
	if(list) {
		ravu->next = *list;
		*list = ravu;
	} else {
		ravu->next = *_xavu_list_crt;
		*_xavu_list_crt = ravu;
	}

	return ravu;
}

/**
 * Set the value of the  xavu rname with child xavu cname
 * - set if it exits; add if it doesn't exist
 * - config operations:
 *   $xavu(rxname=>cname) = xval;
 */
sr_xavp_t *xavu_set_child_xval(str *rname, str *cname, sr_xval_t *xval)
{
	sr_xavp_t *ravu=NULL;
	sr_xavp_t *cavu=NULL;

	ravu = xavu_get(rname, NULL);
	if(ravu) {
		if(ravu->val.type != SR_XTYPE_XAVP) {
			/* first root xavp does not have xavp list value - remove it */
			xavp_rm(ravu, NULL);
			/* add a new xavp in the root list with a child */
			return xavu_set_xavu_value(rname, cname, xval, NULL);
		} else {
			/* first root xavp has an xavp list value */
			cavu = xavu_get(cname, ravu->val.v.xavp);
			if(cavu) {
				/* child xavp with same name - remove it */
				/* todo: update in place for int or if allocated size fits */
				xavp_rm(cavu, &ravu->val.v.xavp);
			}
			return xavp_add_value(cname, xval, &ravu->val.v.xavp);
		}
	} else {
		/* no xavp with rname in root list found */
		return xavu_set_xavu_value(rname, cname, xval, NULL);
	}
}

/**
 *
 */
sr_xavp_t *xavu_set_child_ival(str *rname, str *cname, long ival)
{
	sr_xval_t xval;

	memset(&xval, 0, sizeof(sr_xval_t));
	xval.type = SR_XTYPE_LONG;
	xval.v.l = ival;

	return xavu_set_child_xval(rname, cname, &xval);
}

/**
 *
 */
sr_xavp_t *xavu_set_child_sval(str *rname, str *cname, str *sval)
{
	sr_xval_t xval;

	memset(&xval, 0, sizeof(sr_xval_t));
	xval.type = SR_XTYPE_STR;
	xval.v.s = *sval;

	return xavu_set_child_xval(rname, cname, &xval);
}

/**
 * return child node of an xavp
 * - $xavu(rname=>cname)
 */
sr_xavp_t* xavu_get_child(str *rname, str *cname)
{
	sr_xavp_t *ravp=NULL;

	ravp = xavu_get(rname, NULL);
	if(ravp==NULL || ravp->val.type!=SR_XTYPE_XAVP)
		return NULL;

	return xavu_get(cname, ravp->val.v.xavp);
}


/**
 * return child node of an xavp if it has int value
 * - $xavu(rname=>cname)
 */
sr_xavp_t* xavu_get_child_with_ival(str *rname, str *cname)
{
	sr_xavp_t *vavp=NULL;

	vavp = xavu_get_child(rname, cname);

	if(vavp==NULL || vavp->val.type!=SR_XTYPE_LONG)
		return NULL;

	return vavp;
}


/**
 * return child node of an xavp if it has string value
 * - $xavu(rname=>cname)
 */
sr_xavp_t* xavu_get_child_with_sval(str *rname, str *cname)
{
	sr_xavp_t *vavp=NULL;

	vavp = xavu_get_child(rname, cname);

	if(vavp==NULL || vavp->val.type!=SR_XTYPE_STR)
		return NULL;

	return vavp;
}


/**
 * serialize the values in subfields of an xavu in name=value; format
 * - rname - name of the root list xavu
 * - obuf - buffer were to write the output
 * - olen - the size of obuf
 * return: 0 - not found; -1 - error; >0 - length of output
 */
int xavu_serialize_fields(str *rname, char *obuf, int olen)
{
	sr_xavp_t *ravu = NULL;
	sr_xavp_t *avu = NULL;
	str ostr;
	int rlen;

	ravu = xavu_get(rname, NULL);
	if(ravu==NULL || ravu->val.type!=SR_XTYPE_XAVP) {
		/* not found or not holding subfields */
		return 0;
	}

	rlen = 0;
	ostr.s = obuf;
	avu = ravu->val.v.xavp;
	while(avu) {
		switch(avu->val.type) {
			case SR_XTYPE_LONG:
				LM_DBG("     XAVP long int value: %ld\n", avu->val.v.l);
				ostr.len = snprintf(ostr.s, olen-rlen, "%.*s=%lu;",
						avu->name.len, avu->name.s, (unsigned long)avu->val.v.l);
				if(ostr.len<=0 || ostr.len>=olen-rlen) {
					LM_ERR("failed to serialize int value (%d/%d\n",
							ostr.len, olen-rlen);
					return -1;
				}
			break;
			case SR_XTYPE_STR:
				LM_DBG("     XAVP str value: %s\n", avu->val.v.s.s);
				if(avu->val.v.s.len == 0) {
					ostr.len = snprintf(ostr.s, olen-rlen, "%.*s;",
						avu->name.len, avu->name.s);
				} else {
					ostr.len = snprintf(ostr.s, olen-rlen, "%.*s=%.*s;",
						avu->name.len, avu->name.s,
						avu->val.v.s.len, avu->val.v.s.s);
				}
				if(ostr.len<=0 || ostr.len>=olen-rlen) {
					LM_ERR("failed to serialize int value (%d/%d\n",
							ostr.len, olen-rlen);
					return -1;
				}
			break;
			default:
				LM_DBG("skipping value type: %d\n", avu->val.type);
				ostr.len = 0;
		}
		if(ostr.len>0) {
			ostr.s += ostr.len;
			rlen += ostr.len;
		}
		avu = avu->next;
	}
	return rlen;
}

/**
 *
 */
/*** XAVI - eXtended Attribute Value Insensitive case - implementation ***/
/*! Helper functions */
static sr_xavp_t *xavi_get_internal(str *name, sr_xavp_t **list, int idx, sr_xavp_t **prv);
static int xavi_rm_internal(str *name, sr_xavp_t **head, int idx);

/**
 *
 */
static sr_xavp_t *xavi_new_value(str *name, sr_xval_t *val)
{
	sr_xavp_t *avi;
	int size;
	unsigned int id;

	if(name==NULL || name->s==NULL || name->len<=0 || val==NULL)
		return NULL;
	id = get_hash1_case_raw(name->s, name->len);

	size = sizeof(sr_xavp_t) + name->len + 1;
	if(val->type == SR_XTYPE_STR)
		size += val->v.s.len + 1;
	avi = (sr_xavp_t*)shm_malloc(size);
	if(avi==NULL) {
		SHM_MEM_ERROR;
		return NULL;
	}
	memset(avi, 0, size);
	avi->id = id;
	avi->name.s = (char*)avi + sizeof(sr_xavp_t);
	memcpy(avi->name.s, name->s, name->len);
	avi->name.s[name->len] = '\0';
	avi->name.len = name->len;
	memcpy(&avi->val, val, sizeof(sr_xval_t));
	if(val->type == SR_XTYPE_STR)
	{
		avi->val.v.s.s = avi->name.s + avi->name.len + 1;
		memcpy(avi->val.v.s.s, val->v.s.s, val->v.s.len);
		avi->val.v.s.s[val->v.s.len] = '\0';
		avi->val.v.s.len = val->v.s.len;
	}

	return avi;
}

/**
 *
 */
int xavi_add(sr_xavp_t *xavi, sr_xavp_t **list)
{
	if (xavi==NULL) {
		return -1;
	}
	/* Prepend new xavi to the list */
	if(list) {
		xavi->next = *list;
		*list = xavi;
	} else {
		xavi->next = *_xavi_list_crt;
		*_xavi_list_crt = xavi;
	}

	return 0;
}

/**
 *
 */
int xavi_add_last(sr_xavp_t *xavi, sr_xavp_t **list)
{
	sr_xavp_t *prev;
	sr_xavp_t *crt;

	if (xavi==NULL) {
		return -1;
	}

	crt = xavi_get_internal(&xavi->name, list, 0, 0);

	prev = NULL;

	while(crt) {
		prev = crt;
		crt = xavi_get_next(prev);
	}

	if(prev==NULL) {
		/* Prepend new xavi to the list */
		if(list) {
			xavi->next = *list;
			*list = xavi;
		} else {
			xavi->next = *_xavi_list_crt;
			*_xavi_list_crt = xavi;
		}
	} else {
		xavi->next = prev->next;
		prev->next = xavi;
	}

	return 0;
}

/**
 *
 */
int xavi_add_after(sr_xavp_t *nxavi, sr_xavp_t *pxavi)
{
	if (nxavi==NULL) {
		return -1;
	}

	if(pxavi==NULL) {
		nxavi->next = *_xavi_list_crt;
		*_xavi_list_crt = nxavi;
	} else {
		nxavi->next = pxavi->next;
		pxavi->next = nxavi;
	}

	return 0;
}

/**
 *
 */
sr_xavp_t *xavi_add_value(str *name, sr_xval_t *val, sr_xavp_t **list)
{
	sr_xavp_t *avi=0;

	avi = xavi_new_value(name, val);
	if (avi==NULL)
		return NULL;

	/* Prepend new value to the list */
	if(list) {
		avi->next = *list;
		*list = avi;
	} else {
		avi->next = *_xavi_list_crt;
		*_xavi_list_crt = avi;
	}

	return avi;
}

/**
 *
 */
sr_xavp_t *xavi_add_value_after(str *name, sr_xval_t *val, sr_xavp_t *pxavi)
{
	sr_xavp_t *avi=0;

	avi = xavi_new_value(name, val);
	if (avi==NULL)
		return NULL;

	/* link new xavi */
	if(pxavi) {
		avi->next = pxavi->next;
		pxavi->next = avi;
	} else {
		avi->next = *_xavi_list_crt;
		*_xavi_list_crt = avi;
	}

	return avi;
}

/**
 *
 */
sr_xavp_t *xavi_add_xavi_value(str *rname, str *name, sr_xval_t *val, sr_xavp_t **list)
{
	sr_xavp_t *ravi=0;
	sr_xavp_t *cavi=0;
	sr_xval_t rval;

	cavi = xavi_new_value(name, val);
	if (cavi==NULL)
		return NULL;

	memset(&rval, 0, sizeof(sr_xval_t));
	rval.type = SR_XTYPE_XAVP;
	rval.v.xavp = cavi;

	ravi = xavi_new_value(rname, &rval);
	if (ravi==NULL) {
		xavi_destroy_list(&cavi);
		return NULL;
	}

	/* Prepend new value to the list */
	if(list) {
		ravi->next = *list;
		*list = ravi;
	} else {
		ravi->next = *_xavi_list_crt;
		*_xavi_list_crt = ravi;
	}

	return ravi;
}

/**
 *
 */
sr_xavp_t *xavi_set_value(str *name, int idx, sr_xval_t *val, sr_xavp_t **list)
{
	sr_xavp_t *avi;
	sr_xavp_t *cur;
	sr_xavp_t *prv=0;

	if(val==NULL)
		return NULL;

	/* Find the current value */
	cur = xavi_get_internal(name, list, idx, &prv);
	if(cur==NULL)
		return NULL;

	avi = xavi_new_value(name, val);
	if (avi==NULL)
		return NULL;

	/* Replace the current value with the new */
	avi->next = cur->next;
	if(prv)
		prv->next = avi;
	else if(list)
		*list = avi;
	else
		*_xavi_list_crt = avi;

	xavi_free(cur);

	return avi;
}

/**
 *
 */
static sr_xavp_t *xavi_get_internal(str *name, sr_xavp_t **list, int idx, sr_xavp_t **prv)
{
	sr_xavp_t *avi;
	unsigned int id;
	int n = 0;

	if(name==NULL || name->s==NULL || name->len<=0)
		return NULL;
	id = get_hash1_case_raw(name->s, name->len);

	if(list && *list)
		avi = *list;
	else
		avi = *_xavi_list_crt;
	while(avi)
	{
		if(avi->id==id && avi->name.len==name->len
				&& strncasecmp(avi->name.s, name->s, name->len)==0)
		{
			if(idx==n)
				return avi;
			n++;
		}
		if(prv)
			*prv = avi;
		avi = avi->next;
	}
	return NULL;
}

/**
 *
 */
sr_xavp_t *xavi_get(str *name, sr_xavp_t *start)
{
	return xavi_get_internal(name, (start)?&start:NULL, 0, NULL);
}

/**
 *
 */
sr_xavp_t *xavi_get_by_index(str *name, int idx, sr_xavp_t **start)
{
	return xavi_get_internal(name, start, idx, NULL);
}

/**
 *
 */
sr_xavp_t *xavi_get_next(sr_xavp_t *start)
{
	sr_xavp_t *avi;

	if(start==NULL)
		return NULL;

	avi = start->next;
	while(avi)
	{
		if(avi->id==start->id && avi->name.len==start->name.len
				&& strncasecmp(avi->name.s, start->name.s, start->name.len)==0)
			return avi;
		avi=avi->next;
	}

	return NULL;
}

/**
 *
 */
sr_xavp_t *xavi_get_last(str *xname, sr_xavp_t **list)
{
	sr_xavp_t *prev;
	sr_xavp_t *crt;

	crt = xavi_get_internal(xname, list, 0, 0);

	prev = NULL;

	while(crt) {
		prev = crt;
		crt = xavi_get_next(prev);
	}

	return prev;
}

/**
 *
 */
int xavi_rm(sr_xavp_t *xa, sr_xavp_t **head)
{
	sr_xavp_t *avi;
	sr_xavp_t *prv=0;

	if(head!=NULL)
		avi = *head;
	else
		avi=*_xavi_list_crt;

	while(avi)
	{
		if(avi==xa)
		{
			if(prv)
				prv->next=avi->next;
			else if(head!=NULL)
				*head = avi->next;
			else
				*_xavi_list_crt = avi->next;
			xavi_free(avi);
			return 1;
		}
		prv=avi; avi=avi->next;
	}
	return 0;
}

/* Remove xavis
 * idx: <0 remove all xavis with the same name
 *      >=0 remove only the specified index xavi
 * Returns number of xavis that were deleted
 */
static int xavi_rm_internal(str *name, sr_xavp_t **head, int idx)
{
	sr_xavp_t *avi;
	sr_xavp_t *foo;
	sr_xavp_t *prv=0;
	unsigned int id;
	int n=0;
	int count=0;

	if(name==NULL || name->s==NULL || name->len<=0)
		return 0;

	id = get_hash1_case_raw(name->s, name->len);
	if(head!=NULL)
		avi = *head;
	else
		avi = *_xavi_list_crt;
	while(avi)
	{
		foo = avi;
		avi=avi->next;
		if(foo->id==id && foo->name.len==name->len
				&& strncasecmp(foo->name.s, name->s, name->len)==0)
		{
			if(idx<0 || idx==n)
			{
				if(prv!=NULL)
					prv->next=foo->next;
				else if(head!=NULL)
					*head = foo->next;
				else
					*_xavi_list_crt = foo->next;
				xavi_free(foo);
				if(idx>=0)
					return 1;
				count++;
			} else {
				prv = foo;
			}
			n++;
		} else {
			prv = foo;
		}
	}
	return count;
}

/**
 *
 */
int xavi_rm_by_name(str *name, int all, sr_xavp_t **head)
{
	return xavi_rm_internal(name, head, -1*all);
}

/**
 *
 */
int xavi_rm_by_index(str *name, int idx, sr_xavp_t **head)
{
	if (idx<0)
		return 0;
	return xavi_rm_internal(name, head, idx);
}

/**
 *
 */
int xavi_rm_child_by_index(str *rname, str *cname, int idx)
{
	sr_xavp_t *avi=NULL;

	if (idx<0) {
		return 0;
	}
	avi = xavi_get(rname, NULL);

	if(avi == NULL || avi->val.type!=SR_XTYPE_XAVP) {
		return 0;
	}
	return xavi_rm_internal(cname, &avi->val.v.xavp, idx);
}

/**
 *
 */
int xavi_count(str *name, sr_xavp_t **start)
{
	sr_xavp_t *avi;
	unsigned int id;
	int n = 0;

	if(name==NULL || name->s==NULL || name->len<=0)
		return -1;
	id = get_hash1_case_raw(name->s, name->len);

	if(start)
		avi = *start;
	else
		avi=*_xavi_list_crt;
	while(avi)
	{
		if(avi->id==id && avi->name.len==name->len
				&& strncasecmp(avi->name.s, name->s, name->len)==0)
		{
			n++;
		}
		avi=avi->next;
	}

	return n;
}

/**
 *
 */
void xavi_reset_list(void)
{
	assert(_xavi_list_crt!=0 );

	if (_xavi_list_crt!=&_xavi_list_head)
		_xavi_list_crt=&_xavi_list_head;
	xavi_destroy_list(_xavi_list_crt);
}

/**
 *
 */
sr_xavp_t **xavi_set_list(sr_xavp_t **head)
{
	sr_xavp_t **avi;

	assert(_xavi_list_crt!=0);

	avi = _xavi_list_crt;
	_xavi_list_crt = head;
	return avi;
}

/**
 *
 */
sr_xavp_t **xavi_get_crt_list(void)
{
	assert(_xavi_list_crt!=0);
	return _xavi_list_crt;
}

/**
 *
 */
void xavi_print_list_content(sr_xavp_t **head, int level)
{
	xavx_print_list_content("XAVI", head, _xavi_list_crt, level);
}

/**
 *
 */
void xavi_print_list(sr_xavp_t **head)
{
	xavi_print_list_content(head, 0);
}

/**
 * returns a list of str with key names.
 * Example:
 * If we have this structure
 * $xavi(test=>one) = 1
 * $xavi(test[0]=>two) = "2"
 * $xavi(test[0]=>three) = 3
 * $xavi(test[0]=>four) = $xavp(whatever)
 * $xavi(test[0]=>two) = "other 2"
 *
 * xavi_get_list_keys_names(test[0]) returns
 * {"one", "two", "three", "four"}
 *
 * free the struct str_list afterwards
 * but do *NOT* free the strings inside
 */
struct str_list *xavi_get_list_key_names(sr_xavp_t *xavi)
{
	sr_xavp_t *avi = NULL;
	struct str_list *result = NULL;
	struct str_list *r = NULL;
	struct str_list *f = NULL;
	int total = 0;

	if(xavi==NULL){
		LM_ERR("xavi is NULL\n");
		return 0;
	}

	if(xavi->val.type!=SR_XTYPE_XAVP){
		LM_ERR("%s not xavp?\n", xavi->name.s);
		return 0;
	}

	avi = xavi->val.v.xavp;

	if (avi)
	{
		result = (struct str_list*)pkg_malloc(sizeof(struct str_list));
		if (result==NULL) {
			PKG_MEM_ERROR;
			return 0;
		}
		r = result;
		r->s.s = avi->name.s;
		r->s.len = avi->name.len;
		r->next = NULL;
		avi = avi->next;
	}

	while(avi)
	{
		f = result;
		while(f)
		{
			if((avi->name.len==f->s.len)&&
				(strncasecmp(avi->name.s, f->s.s, f->s.len)==0))
			{
				break; /* name already on list */
			}
			f = f->next;
		}
		if (f==NULL)
		{
			r = append_str_list(avi->name.s, avi->name.len, &r, &total);
			if(r==NULL){
				while(result){
					r = result;
					result = result->next;
					pkg_free(r);
				}
				return 0;
			}
		}
		avi = avi->next;
	}
	return result;
}

sr_xavp_t *xavi_clone_level_nodata(sr_xavp_t *xold)
{
	return xavi_clone_level_nodata_with_new_name(xold, &xold->name);
}

/**
 * clone the xavi without values that are custom data
 * - only one list level is cloned, other sublists are ignored
 */
sr_xavp_t *xavi_clone_level_nodata_with_new_name(sr_xavp_t *xold, str *dst_name)
{
	sr_xavp_t *xnew = NULL;
	sr_xavp_t *navi = NULL;
	sr_xavp_t *oavi = NULL;
	sr_xavp_t *pavi = NULL;

	if(xold == NULL)
	{
		return NULL;
	}
	if(xold->val.type==SR_XTYPE_DATA || xold->val.type==SR_XTYPE_SPTR)
	{
		LM_INFO("xavi value type is 'data' - ignoring in clone\n");
		return NULL;
	}
	xnew = xavi_new_value(dst_name, &xold->val);
	if(xnew==NULL)
	{
		LM_ERR("cannot create cloned root xavi\n");
		return NULL;
	}
	LM_DBG("cloned root xavi [%.*s] >> [%.*s]\n", xold->name.len, xold->name.s, dst_name->len, dst_name->s);

	if(xold->val.type!=SR_XTYPE_XAVP)
	{
		return xnew;
	}

	xnew->val.v.xavp = NULL;
	oavi = xold->val.v.xavp;

	while(oavi)
	{
		if(oavi->val.type!=SR_XTYPE_DATA && oavi->val.type!=SR_XTYPE_XAVP
				&& oavi->val.type!=SR_XTYPE_SPTR)
		{
			navi =  xavi_new_value(&oavi->name, &oavi->val);
			if(navi==NULL)
			{
				LM_ERR("cannot create cloned embedded xavi\n");
				if(xnew->val.v.xavp != NULL) {
					xavi_destroy_list(&xnew->val.v.xavp);
				}
				shm_free(xnew);
				return NULL;
			}
			LM_DBG("cloned inner xavi [%.*s]\n", oavi->name.len, oavi->name.s);
			if(xnew->val.v.xavp == NULL)
			{
				/* link to val in head xavi */
				xnew->val.v.xavp = navi;
			} else {
				/* link to prev xavi in the list */
				pavi->next = navi;
			}
			pavi = navi;
		}
		oavi = oavi->next;
	}

	if(xnew->val.v.xavp == NULL)
	{
		shm_free(xnew);
		return NULL;
	}

	return xnew;
}

int xavi_insert(sr_xavp_t *xavi, int idx, sr_xavp_t **list)
{
	sr_xavp_t *crt = 0;
	sr_xavp_t *lst = 0;
	sr_xval_t val;
	int n = 0;
	int i = 0;

	if(xavi==NULL) {
		return -1;
	}

	crt = xavi_get_internal(&xavi->name, list, 0, NULL);

	if (idx == 0 && (!crt || crt->val.type != SR_XTYPE_NULL))
		return xavi_add(xavi, list);

	while(crt!=NULL && n<idx) {
		lst = crt;
		n++;
		crt = xavi_get_next(lst);
	}

	if (crt && crt->val.type == SR_XTYPE_NULL) {
		xavi->next = crt->next;
		crt->next = xavi;

		xavi_rm(crt, list);
		return 0;
	}

	memset(&val, 0, sizeof(sr_xval_t));
	val.type = SR_XTYPE_NULL;
	for(i=0; i<idx-n; i++) {
		crt = xavi_new_value(&xavi->name, &val);
		if(crt==NULL)
			return -1;
		if (lst == NULL) {
			xavi_add(crt, list);
		} else {
			crt->next = lst->next;
			lst->next = crt;
		}
		lst = crt;
	}

	if(lst==NULL) {
		LM_ERR("cannot link the xavi\n");
		return -1;
	}
	xavi->next = lst->next;
	lst->next = xavi;

	return 0;
}

sr_xavp_t *xavi_extract(str *name, sr_xavp_t **list)
{
	sr_xavp_t *avi = 0;
	sr_xavp_t *foo;
	sr_xavp_t *prv = 0;
	unsigned int id;

	if(name==NULL || name->s==NULL || name->len<=0) {
		if(list!=NULL) {
			avi = *list;
			if(avi!=NULL) {
				*list = avi->next;
				avi->next = NULL;
			}
		} else {
			avi = *_xavi_list_crt;
			if(avi!=NULL) {
				*_xavi_list_crt = avi->next;
				avi->next = NULL;
			}
		}

		return avi;
	}

	id = get_hash1_case_raw(name->s, name->len);
	if(list!=NULL)
		avi = *list;
	else
		avi = *_xavi_list_crt;
	while(avi)
	{
		foo = avi;
		avi=avi->next;
		if(foo->id==id && foo->name.len==name->len
				&& strncasecmp(foo->name.s, name->s, name->len)==0)
		{
			if(prv!=NULL)
				prv->next=foo->next;
			else if(list!=NULL)
				*list = foo->next;
			else
				*_xavi_list_crt = foo->next;
			foo->next = NULL;
			return foo;
		} else {
			prv = foo;
		}
	}
	return NULL;
}

/**
 * return child node of an xavi
 * - $xavi(rname=>cname)
 */
sr_xavp_t* xavi_get_child(str *rname, str *cname)
{
	sr_xavp_t *ravi=NULL;

	ravi = xavi_get(rname, NULL);
	if(ravi==NULL || ravi->val.type!=SR_XTYPE_XAVP)
		return NULL;

	return xavi_get(cname, ravi->val.v.xavp);
}


/**
 * return child node of an xavi if it has int value
 * - $xavi(rname=>cname)
 */
sr_xavp_t* xavi_get_child_with_ival(str *rname, str *cname)
{
	sr_xavp_t *vavi=NULL;

	vavi = xavi_get_child(rname, cname);

	if(vavi==NULL || vavi->val.type!=SR_XTYPE_LONG)
		return NULL;

	return vavi;
}


/**
 * return child node of an xavi if it has string value
 * - $xavi(rname=>cname)
 */
sr_xavp_t* xavi_get_child_with_sval(str *rname, str *cname)
{
	sr_xavp_t *vavi=NULL;

	vavi = xavi_get_child(rname, cname);

	if(vavi==NULL || vavi->val.type!=SR_XTYPE_STR)
		return NULL;

	return vavi;
}

/**
 * Set the value of the first xavi rname with first child xavi cname
 * - replace if it exits; add if it doesn't exist
 * - config operations:
 *   $xavi(rxname=>cname) = xval;
 *     or:
 *   $xavi(rxname[0]=>cname[0]) = xval;
 */
int xavi_set_child_xval(str *rname, str *cname, sr_xval_t *xval)
{
	sr_xavp_t *ravi=NULL;
	sr_xavp_t *cavi=NULL;

	ravi = xavi_get(rname, NULL);
	if(ravi) {
		if(ravi->val.type != SR_XTYPE_XAVP) {
			/* first root xavi does not have xavi list value - remove it */
			xavi_rm(ravi, NULL);
			/* add a new xavi in the root list with a child */
			if(xavi_add_xavi_value(rname, cname, xval, NULL)==NULL) {
				return -1;
			}
		} else {
			/* first root xavi has an xavi list value */
			cavi = xavi_get(cname, ravi->val.v.xavp);
			if(cavi) {
				/* child xavi with same name - remove it */
				/* todo: update in place for int or if allocated size fits */
				xavi_rm(cavi, &ravi->val.v.xavp);
			}
			if(xavi_add_value(cname, xval, &ravi->val.v.xavp)==NULL) {
				return -1;
			}
		}
	} else {
		/* no xavi with rname in root list found */
		if(xavi_add_xavi_value(rname, cname, xval, NULL)==NULL) {
			return -1;
		}
	}

	return 0;
}

/**
 *
 */
int xavi_set_child_ival(str *rname, str *cname, long ival)
{
	sr_xval_t xval;

	memset(&xval, 0, sizeof(sr_xval_t));
	xval.type = SR_XTYPE_LONG;
	xval.v.l = ival;

	return xavi_set_child_xval(rname, cname, &xval);
}

/**
 *
 */
int xavi_set_child_sval(str *rname, str *cname, str *sval)
{
	sr_xval_t xval;

	memset(&xval, 0, sizeof(sr_xval_t));
	xval.type = SR_XTYPE_STR;
	xval.v.s = *sval;

	return xavi_set_child_xval(rname, cname, &xval);
}

/**
 * serialize the values in subfields of an xavi in name=value; format
 * - rname - name of the root list xavi
 * - obuf - buffer were to write the output
 * - olen - the size of obuf
 * return: 0 - not found; -1 - error; >0 - length of output
 */
int xavi_serialize_fields(str *rname, char *obuf, int olen)
{
	sr_xavp_t *ravi = NULL;
	sr_xavp_t *avi = NULL;
	str ostr;
	int rlen;

	ravi = xavi_get(rname, NULL);
	if(ravi==NULL || ravi->val.type!=SR_XTYPE_XAVP) {
		/* not found or not holding subfields */
		return 0;
	}

	rlen = 0;
	ostr.s = obuf;
	avi = ravi->val.v.xavp;
	while(avi) {
		switch(avi->val.type) {
			case SR_XTYPE_LONG:
				LM_DBG("     XAVP long int value: %ld\n", avi->val.v.l);
				ostr.len = snprintf(ostr.s, olen-rlen, "%.*s=%lu;",
						avi->name.len, avi->name.s, (unsigned long)avi->val.v.l);
				if(ostr.len<=0 || ostr.len>=olen-rlen) {
					LM_ERR("failed to serialize int value (%d/%d\n",
							ostr.len, olen-rlen);
					return -1;
				}
			break;
			case SR_XTYPE_STR:
				LM_DBG("     XAVP str value: %s\n", avi->val.v.s.s);
				if(avi->val.v.s.len == 0) {
					ostr.len = snprintf(ostr.s, olen-rlen, "%.*s;",
						avi->name.len, avi->name.s);
				} else {
					ostr.len = snprintf(ostr.s, olen-rlen, "%.*s=%.*s;",
						avi->name.len, avi->name.s,
						avi->val.v.s.len, avi->val.v.s.s);
				}
				if(ostr.len<=0 || ostr.len>=olen-rlen) {
					LM_ERR("failed to serialize int value (%d/%d\n",
							ostr.len, olen-rlen);
					return -1;
				}
			break;
			default:
				LM_DBG("skipping value type: %d\n", avi->val.type);
				ostr.len = 0;
		}
		if(ostr.len>0) {
			ostr.s += ostr.len;
			rlen += ostr.len;
		}
		avi = avi->next;
	}
	return rlen;
}
