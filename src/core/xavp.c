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

	if(name==NULL || name->s==NULL || val==NULL)
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

	if(name==NULL || name->s==NULL)
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

	if(name==NULL || name->s==NULL)
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

void xavp_print_list_content(sr_xavp_t **head, int level)
{
	sr_xavp_t *avp=0;
	sr_xavp_t *start=0;

	if(head!=NULL)
		start = *head;
	else
		start=*_xavp_list_crt;
	LM_INFO("+++++ start XAVP list: %p (%p) (level=%d)\n", start, head, level);
	avp = start;
	while(avp)
	{
		LM_INFO("     *** (l:%d - %p) XAVP name: %s\n", level, avp, avp->name.s);
		LM_INFO("     XAVP id: %u\n", avp->id);
		LM_INFO("     XAVP value type: %d\n", avp->val.type);
		switch(avp->val.type) {
			case SR_XTYPE_NULL:
				LM_INFO("     XAVP value: <null>\n");
			break;
			case SR_XTYPE_INT:
				LM_INFO("     XAVP value (int): %d\n", avp->val.v.i);
			break;
			case SR_XTYPE_STR:
				LM_INFO("     XAVP value (str): %s\n", avp->val.v.s.s);
			break;
			case SR_XTYPE_TIME:
				LM_INFO("     XAVP value (time): %lu\n",
						(long unsigned int)avp->val.v.t);
			break;
			case SR_XTYPE_LONG:
				LM_INFO("     XAVP value (long): %ld\n", avp->val.v.l);
			break;
			case SR_XTYPE_LLONG:
				LM_INFO("     XAVP value (llong): %lld\n", avp->val.v.ll);
			break;
			case SR_XTYPE_XAVP:
				LM_INFO("     XAVP value: <xavp:%p>\n", avp->val.v.xavp);
				xavp_print_list_content(&avp->val.v.xavp, level+1);
			break;
			case SR_XTYPE_VPTR:
				LM_INFO("     XAVP value: <vptr:%p>\n", avp->val.v.vptr);
			break;
			case SR_XTYPE_SPTR:
				LM_INFO("     XAVP value: <sptr:%p>\n", avp->val.v.vptr);
			break;
			case SR_XTYPE_DATA:
				LM_INFO("     XAVP value: <data:%p>\n", avp->val.v.data);
			break;
		}
		LM_INFO("     *** (l:%d - %p) end\n", level, avp);
		avp = avp->next;
	}
	LM_INFO("----- end XAVP list: %p (level=%d)\n", start, level);
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

	if(name==NULL || name->s==NULL) {
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

	if(vavp==NULL || vavp->val.type!=SR_XTYPE_INT)
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
int xavp_set_child_ival(str *rname, str *cname, int ival)
{
	sr_xval_t xval;

	memset(&xval, 0, sizeof(sr_xval_t));
	xval.type = SR_XTYPE_INT;
	xval.v.i = ival;

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
			case SR_XTYPE_INT:
				LM_DBG("     XAVP int value: %d\n", avp->val.v.i);
				ostr.len = snprintf(ostr.s, olen-rlen, "%.*s=%u;",
						avp->name.len, avp->name.s, (unsigned int)avp->val.v.i);
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
