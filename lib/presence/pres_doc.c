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

#include <presence/pres_doc.h>
#include <cds/memory.h>
#include <cds/logger.h>
#include <cds/list.h>

#include <string.h>

presentity_info_t *create_presentity_info(const str_t *presentity)
{
	presentity_info_t *p;
	int len = 0;
	
	if (!is_str_empty(presentity)) len = presentity->len;
	p = (presentity_info_t*)cds_malloc(sizeof(presentity_info_t) + len);
	if (!p) {
		ERROR_LOG("can't allocate memory for presentity info\n");
		return p;
	}
	p->presentity.len = len;
	if (len > 0) {
		p->presentity.s = p->presentity_data;
		memcpy(p->presentity.s, presentity->s, len);
	}
	else p->presentity.s = NULL;
	p->first_tuple = NULL;
	p->last_tuple = NULL;
	
	return p;
}

presence_tuple_info_t *create_tuple_info(const str_t *contact, presence_tuple_status_t status)
{
	presence_tuple_info_t *t;
	t = (presence_tuple_info_t*)cds_malloc(sizeof(*t));
	if (!t) {
		ERROR_LOG("can't allocate memory for presence tuple info\n");
		return t;
	}
	/* str_clear(&t->contact.s); */
	str_dup(&t->contact, contact);
	str_clear(&t->extra_status);
	t->prev = NULL;
	t->next = NULL;
	t->status = status;
	t->priority = 0.0;
	t->expires = 0;
	return t;
}

void add_tuple_info(presentity_info_t *p, presence_tuple_info_t *t)
{
	DOUBLE_LINKED_LIST_ADD(p->first_tuple, p->last_tuple, t);
}

void free_tuple_info(presence_tuple_info_t *t)
{
	if (!t) return;
	str_free_content(&t->contact);
	str_free_content(&t->extra_status);
	cds_free(t);
}

void free_presentity_info(presentity_info_t *p)
{
	presence_tuple_info_t *t;
	
	if (!p) return;
	t = p->first_tuple;
	while (t) {
		free_tuple_info(t);
		t = t->next;
	}
	cds_free(p);
}

list_presence_info_t *create_list_presence_info(const str_t *uri)
{
	list_presence_info_t *p;
	int len = 0;
	
	if (!is_str_empty(uri)) len = uri->len;
	p = (list_presence_info_t*)cds_malloc(sizeof(list_presence_info_t) + len);
	if (!p) {
		ERROR_LOG("can't allocate memory for list presence info\n");
		return p;
	}
	p->list_uri.len = len;
	if (len > 0) {
		p->list_uri.s = p->uri_data;
		memcpy(p->list_uri.s, uri->s, len);
	}
	else p->list_uri.s = NULL;
	
	/* ptr_vector_init(&p->presentity_infos, 8); */
	str_clear(&p->pres_doc);
	str_clear(&p->content_type);
	
	return p;
}

void free_list_presence_info(list_presence_info_t *p)
{
	if (p) {
		DEBUG_LOG(" ... freeing doc\n");
		str_free_content(&p->pres_doc);
		DEBUG_LOG(" ... freeing content type\n");
		str_free_content(&p->content_type);
		DEBUG_LOG(" ... freeing list presence info\n");
		cds_free(p);
	}
}
