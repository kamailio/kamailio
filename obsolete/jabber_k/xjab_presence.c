/*
 * $Id$
 *
 * XJAB module
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
 */

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>

#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"

#include "xjab_presence.h"

/**
 * create a presence cell
 */
xj_pres_cell xj_pres_cell_new(void)
{
	xj_pres_cell prc = NULL;
	prc = (xj_pres_cell)pkg_malloc(sizeof(t_xj_pres_cell));
	if(prc == NULL)
		return NULL;
	prc->key = 0;
	prc->userid.s = NULL;
	prc->userid.len = 0;
	prc->state = XJ_PS_OFFLINE;
	prc->status = XJ_PRES_STATUS_NULL;
	prc->cbf = NULL;
	prc->cbp = NULL;
	prc->prev = NULL;
	prc->next = NULL;

	return prc;
}

/**
 * free the presence cell
 */
void xj_pres_cell_free(xj_pres_cell prc)
{
	if(!prc)
		return;
	if(prc->userid.s)
		pkg_free(prc->userid.s);
	pkg_free(prc);
	prc = NULL;
}

/**
 * free all presence cell linked to
 */
void xj_pres_cell_free_all(xj_pres_cell prc)
{
	xj_pres_cell p, p0;
	if(!prc)
		return;
	p = prc;
	while(p)
	{
		p0 = p->next;
		xj_pres_cell_free(p);
		p = p0;
	}
}

/**
 * init a presence cell
 */
int xj_pres_cell_init(xj_pres_cell prc, str* uid, pa_callback_f f, void* p)
{
	if(!prc || !uid || !uid->s || uid->len<=0)
		return -1;
	prc->userid.s = (char*)pkg_malloc(uid->len*sizeof(char));
	if(prc->userid.s == NULL)
		return -1;
	strncpy(prc->userid.s, uid->s, uid->len);
	prc->userid.len = uid->len;
	prc->key = xj_get_hash(uid, NULL);
	prc->cbf = f;
	prc->cbp = p;
	return 0;
}

/**
 * update attributes for a presence cell
 */
int xj_pres_cell_update(xj_pres_cell prc, pa_callback_f f, void *p)
{
	if(!prc)
		return -1;
	prc->cbf = f;
	prc->cbp = p;
	return 0;
}

/**
 * init a presence list
 */
xj_pres_list xj_pres_list_init(void)
{
	xj_pres_list prl = NULL;
	
	prl = (xj_pres_list)pkg_malloc(sizeof(t_xj_pres_list));
	if(!prl)
		return NULL;
	prl->nr = 0;
	prl->clist = NULL;

	return prl;
}

/**
 * free the presence list
 */
void xj_pres_list_free(xj_pres_list prl)
{
	if(!prl)
		return;
	xj_pres_cell_free_all(prl->clist);
	pkg_free(prl);
	prl = NULL;
}

/**
 * add, if does not exist, an user in present list
 */
xj_pres_cell xj_pres_list_add(xj_pres_list prl, xj_pres_cell prc)
{
	xj_pres_cell p, p0;
	if(!prc)
		return NULL;
	if(!prl)
	{
		xj_pres_cell_free(prc);
		return NULL;
	}
	// presence list empty
	if(prl->clist==NULL)
	{
		prl->nr++;
		prl->clist = prc;
		return prc;
	}

	p0 = p = prl->clist;
	while(p && p->key <= prc->key)
	{
		if(p->key == prc->key && p->userid.len == prc->userid.len
			&& !strncasecmp(p->userid.s, prc->userid.s, p->userid.len))
		{ // cell already exist
			// update cbf and cbp
			p->cbf = prc->cbf;
			p->cbp = prc->cbp;
			xj_pres_cell_free(prc);
			return p;
		}
		p0 = p;
		p = p->next;
	}

	// add a the cell in list
	prc->next = p0->next;
	prc->prev = p0;
	if(p0->next)
		p0->next->prev = prc;
	p0->next = prc;
	prl->nr++;

	return prc;
}

/**
 * delete a user from presence list
 */
int xj_pres_list_del(xj_pres_list prl, str *uid)
{
	xj_pres_cell p;
	int lkey;
	if(!prl || !uid || !uid->s || uid->len<=0)
		return -1;
	if(prl->nr<=0 || prl->clist==NULL)
		return 0;
	
	lkey = xj_get_hash(uid, NULL);

	p = prl->clist;
	while(p && p->key <= lkey)
	{
		if(p->key == lkey && p->userid.len == uid->len
			&& !strncasecmp(p->userid.s, uid->s, uid->len))
		{
			prl->nr--;
			if(p->next)
				p->next->prev = p->prev;
			if(p->prev == NULL)
				prl->clist = p->next;
			else
				p->prev->next = p->next;
			xj_pres_cell_free(p);
			return 0;
		}
		p = p->next;
	}

	return 0;
}

/**
 * Check if a user is already in presence list
 */
xj_pres_cell xj_pres_list_check(xj_pres_list prl, str* uid)
{
	xj_pres_cell p;
	int lkey;

	if(!prl || !uid || !uid->s || uid->len<=0 || prl->nr<=0 || prl->clist==NULL)
		return NULL;
	lkey = xj_get_hash(uid, NULL);

	p = prl->clist;
	while(p && p->key <= lkey)
	{
		if(p->key == lkey && p->userid.len == uid->len
				&& !strncasecmp(p->userid.s, uid->s, uid->len))
			return p;
		p = p->next;
	}
	return NULL;
}

/**
 * Notify all users from list
 */
void xj_pres_list_notifyall(xj_pres_list prl, int s)
{
	xj_pres_cell p;
	if(!prl || prl->nr<=0 || prl->clist==NULL)
		return;
	p = prl->clist;
	while(p)
	{
		if(p->cbf)
			(*(p->cbf))(&(p->userid),&(p->userid), (s==XJ_PS_CHECK)?p->state:s,
					p->cbp);
		p = p->next;
	}
}

