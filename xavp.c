/*
 * $Id$
 *
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
/*
 * History:
 * --------
 *  2009-05-20  created by daniel
 */


#ifdef WITH_XAVP

#include <stdio.h>
#include <string.h>

#include "mem/shm_mem.h"
#include "dprint.h"
#include "hashes.h"
#include "xavp.h"

/*! XAVP list head */
static sr_xavp_t *_xavp_list_head = 0;
/*! Pointer to XAVP current list */
static sr_xavp_t **_xavp_list_crt = &_xavp_list_head;

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
	if(xa->val.type == SR_XTYPE_DATA) {
		if(xa->val.v.data!=NULL && xa->val.v.data->pfree!=NULL) {
			xa->val.v.data->pfree(xa->val.v.data->p, xavp_shm_free);
			shm_free(xa->val.v.data);
		}
	} else if(xa->val.type == SR_XTYPE_XAVP) {
		xavp_destroy_list(&xa->val.v.xavp);
	}
	shm_free(xa);
}

void xavp_free_unsafe(sr_xavp_t *xa)
{
	if(xa->val.type == SR_XTYPE_DATA) {
		if(xa->val.v.data!=NULL && xa->val.v.data->pfree!=NULL) {
			xa->val.v.data->pfree(xa->val.v.data->p, xavp_shm_free_unsafe);
			shm_free_unsafe(xa->val.v.data);
		}
	} else if(xa->val.type == SR_XTYPE_XAVP) {
		xavp_destroy_list_unsafe(&xa->val.v.xavp);
	}
	shm_free_unsafe(xa);
}

sr_xavp_t *xavp_add_value(str *name, sr_xval_t *val, sr_xavp_t **list)
{
	sr_xavp_t *avp=0;
	int size;

	if(name==NULL || name->s==NULL || val==NULL)
		return NULL;

	size = sizeof(sr_xavp_t) + name->len + 1;
	if(val->type == SR_XTYPE_STR)
		size += val->v.s.len + 1;
	avp = (sr_xavp_t*)shm_malloc(size);
	if(avp==NULL)
		return NULL;
	memset(avp, 0, size);
	avp->id = get_hash1_raw(name->s, name->len);
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
	if(list) {
		avp->next = *list;
		*list = avp;
	} else {
		avp->next = *_xavp_list_crt;
		*_xavp_list_crt = avp;
	}

	return avp;
}

sr_xavp_t *xavp_set_value(str *name, int idx, sr_xval_t *val, sr_xavp_t **list)
{
	sr_xavp_t *avp=0;
	sr_xavp_t *prv=0;
	sr_xavp_t *tmp=0;
	unsigned int id;
	int size;
	int n=0;

	if(name==NULL || name->s==NULL || val==NULL)
		return NULL;

	id = get_hash1_raw(name->s, name->len);
	if(list)
		avp = *list;
	else
		avp=*_xavp_list_crt;
	while(avp)
	{
		if(avp->id==id && avp->name.len==name->len
				&& strncmp(avp->name.s, name->s, name->len)==0)
		{
			if(idx==n)
				return avp;
			n++;
		}
		prv = avp;
		avp=avp->next;
	}
	if(avp==NULL)
		return NULL;
	tmp = avp;

	size = sizeof(sr_xavp_t) + name->len + 1;
	if(val->type == SR_XTYPE_STR)
		size += val->v.s.len + 1;
	avp = (sr_xavp_t*)shm_malloc(size);
	if(avp==NULL)
		return NULL;
	memset(avp, 0, size);
	avp->id = get_hash1_raw(name->s, name->len);
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
	if(prv)
	{
			avp->next = prv->next;
			prv->next = avp;
	} else {
		if(list) {
			avp->next = *list;
			*list = avp;
		} else {
			avp->next = *_xavp_list_crt;
			*_xavp_list_crt = avp;
		}
	}
	xavp_free(tmp);

	return avp;
}

sr_xavp_t *xavp_get(str *name, sr_xavp_t *start)
{
	sr_xavp_t *avp=0;
	unsigned int id;

	if(name==NULL || name->s==NULL)
		return NULL;
	id = get_hash1_raw(name->s, name->len);
	
	if(start)
		avp = start;
	else
		avp=*_xavp_list_crt;
	while(avp)
	{
		if(avp->id==id && avp->name.len==name->len
				&& strncmp(avp->name.s, name->s, name->len)==0)
			return avp;
		avp=avp->next;
	}

	return NULL;
}

sr_xavp_t *xavp_get_by_index(str *name, int idx, sr_xavp_t **start)
{
	sr_xavp_t *avp=0;
	unsigned int id;
	int n = 0;

	if(name==NULL || name->s==NULL)
		return NULL;
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
			if(idx==n)
				return avp;
			n++;
		}
		avp=avp->next;
	}

	return NULL;
}


sr_xavp_t *xavp_get_next(sr_xavp_t *start)
{
	sr_xavp_t *avp=0;

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


int xavp_rm(sr_xavp_t *xa, sr_xavp_t **head)
{
	sr_xavp_t *avp=0;
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
			else
				if(head!=NULL)
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


int xavp_rm_by_name(str *name, int all, sr_xavp_t **head)
{
	sr_xavp_t *avp=0;
	sr_xavp_t *foo=0;
	sr_xavp_t *prv=0;
	unsigned int id = 0;
	int n=0;

	if(name==NULL || name->s==NULL)
		return 0;

	id = get_hash1_raw(name->s, name->len);
	if(head!=NULL)
		avp = *head;
	else
		avp=*_xavp_list_crt;
	while(avp)
	{
		foo = avp;
		avp=avp->next;
		if(foo->id==id && foo->name.len==name->len
				&& strncmp(foo->name.s, name->s, name->len)==0)
		{
			if(prv!=NULL)
				prv->next=foo->next;
			else
				if(head!=NULL)
					*head = foo->next;
				else
					*_xavp_list_crt = foo->next;
			xavp_free(foo);
			n++;
			if(all==0)
				return n;
		} else {
			prv = foo;
		}
	}
	return n;
}

int xavp_rm_by_index(str *name, int idx, sr_xavp_t **head)
{
	sr_xavp_t *avp=0;
	sr_xavp_t *foo=0;
	sr_xavp_t *prv=0;
	unsigned int id = 0;
	int n=0;

	if(name==NULL || name->s==NULL)
		return 0;
	if(idx<0)
		return 0;

	id = get_hash1_raw(name->s, name->len);
	if(head!=NULL)
		avp = *head;
	else
		avp=*_xavp_list_crt;
	while(avp)
	{
		foo = avp;
		avp=avp->next;
		if(foo->id==id && foo->name.len==name->len
				&& strncmp(foo->name.s, name->s, name->len)==0)
		{
			if(idx==n)
			{
				if(prv!=NULL)
					prv->next=foo->next;
				else
					if(head!=NULL)
						*head = foo->next;
					else
						*_xavp_list_crt = foo->next;
				xavp_free(foo);
				return 1;
			}
			n++;
		}
		prv = foo;
	}
	return 0;
}


int xavp_count(str *name, sr_xavp_t **start)
{
	sr_xavp_t *avp=0;
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
	LM_DBG("+++++ start XAVP list: %p (level=%d)\n", start, level);
	avp = start;
	while(avp)
	{
		LM_DBG("     *** XAVP name: %s\n", avp->name.s);
		LM_DBG("     XAVP id: %u\n", avp->id);
		LM_DBG("     XAVP value type: %d\n", avp->val.type);
		switch(avp->val.type) {
			case SR_XTYPE_NULL:
				LM_DBG("     XAVP value: <null>\n");
			break;
			case SR_XTYPE_INT:
				LM_DBG("     XAVP value: %d\n", avp->val.v.i);
			break;
			case SR_XTYPE_STR:
				LM_DBG("     XAVP value: %s\n", avp->val.v.s.s);
			break;
			case SR_XTYPE_TIME:
				LM_DBG("     XAVP value: %lu\n", avp->val.v.t);
			break;
			case SR_XTYPE_LONG:
				LM_DBG("     XAVP value: %ld\n", avp->val.v.l);
			break;
			case SR_XTYPE_LLONG:
				LM_DBG("     XAVP value: %lld\n", avp->val.v.ll);
			break;
			case SR_XTYPE_XAVP:
				LM_DBG("     XAVP value: <xavp:%p>\n", avp->val.v.xavp);
				xavp_print_list_content(&avp->val.v.xavp, level+1);
			break;
			case SR_XTYPE_DATA:
				LM_DBG("     XAVP value: <data:%p>\n", avp->val.v.data);
			break;
		}
		avp = avp->next;
	}
	LM_DBG("----- end XAVP list: %p (level=%d)\n", start, level);
}

void xavp_print_list(sr_xavp_t **head)
{
	xavp_print_list_content(head, 0);
}
#endif
