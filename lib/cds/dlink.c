/* 
 * Copyright (C) 2005 iptelorg GmbH
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
 */

#include <cds/dlink.h>
#include <cds/memory.h>
#include <stdlib.h>

dlink_element_t *dlink_element_alloc(int _data_length)
{
	dlink_element_t *e = (dlink_element_t*)cds_malloc(_data_length + sizeof(dlink_element_t)); /* shm */
	if (e) {
		e->data_length = _data_length;
		e->next = NULL;
		e->prev = NULL;
	}
	return e;
}

void dlink_element_free(dlink_element_t *e)
{
	if (e) cds_free(e); /* shm */
}

/* dlink_element_t *dlink_element_alloc_pkg(int _data_length)
{
	dlink_element_t *e = (dlink_element_t*)DLINK_PKG_ALLOC(_data_length + sizeof(dlink_element_t));
	if (e) {
		e->data_length = _data_length;
		e->next = NULL;
		e->prev = NULL;
	}
	return e;
}

void dlink_element_free_pkg(dlink_element_t *e) 
{
	if (e) DLINK_PKG_FREE(e);
} */

char* dlink_element_data(dlink_element_t *e)
{
	if (e) return e->data;
	else return NULL;
}


void dlink_init(dlink_t *l) 
{
	if (l) {
		l->first = NULL;
		l->last = NULL;
	}
}

void dlink_add(dlink_t *l, dlink_element_t *e)
{
	if ( (!l) || (!e) ) return;
	
	e->next = NULL;
	if (!l->last) {
		l->first = e;
		e->prev = NULL;
	}
	else {
		l->last->next = e;
		e->prev = l->last;
	}
	l->last = e;
}

void dlink_remove(dlink_t *l, dlink_element_t *e)
{
	if ((!l) || (!e)) return;

	if (e == l->first) l->first = e->next;
	if (e == l->last) l->last = e->prev;
	
	if (e->prev) e->prev->next = e->next;
	if (e->next) e->next->prev = e->prev;
	
	e->next = NULL;
	e->prev = NULL;
}

dlink_element_t *dlink_start_walk(dlink_t *l) { if (l) return l->first; else return NULL; }
dlink_element_t *dlink_next_element(dlink_element_t *e) { if (e) return e->next; else return NULL; }
dlink_element_t *dlink_prev_element(dlink_element_t *e) { if (e) return e->prev; else return NULL; }

dlink_element_t *dlink_last_element(dlink_t *l) 
{
	if (l) return l->last;
	else return NULL;
}

void dlink_destroy(dlink_t *l)
{
	dlink_element_t *e,*n;
	e = dlink_start_walk(l);
	while (e) {
		n = dlink_next_element(e);
		dlink_element_free(e);
		e = n;
	}
}
/*
void dlink_destroy_pkg(dlink_t *l)
{
	dlink_element_t *e,*n;
	e = dlink_start_walk(l);
	while (e) {
		n = dlink_next_element(e);
		dlink_element_free_pkg(e);
		e = n;
	}
}
*/
