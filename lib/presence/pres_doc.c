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

/* ---------------------------------------------------------------- */
/* Helper functions */

static str_t open = STR_STATIC_INIT("open");
static str_t closed = STR_STATIC_INIT("closed");
static str_t unknown = STR_STATIC_INIT("undefined");

str_t* tuple_status2str(presence_tuple_status_t status)
{
	switch (status) {
		case presence_tuple_open: return &open;
		case presence_tuple_closed: return &closed;
		case presence_tuple_undefined_status: return &unknown;
	}
	return &unknown;
}

presence_tuple_status_t str2tuple_status(const str *s)
{
	if (str_nocase_equals(s, &open) == 0) return presence_tuple_open;
	if (str_nocase_equals(s, &closed) == 0) return presence_tuple_closed;
	return presence_tuple_undefined_status;
}

/* ---------------------------------------------------------------- */

presentity_info_t *create_presentity_info(const str_t *presentity_uri)
{
	presentity_info_t *p;
	int len = 0;
	
	if (!is_str_empty(presentity_uri)) len = presentity_uri->len;
	p = (presentity_info_t*)cds_malloc(sizeof(presentity_info_t) + len);
	if (!p) {
		ERROR_LOG("can't allocate memory for presentity info\n");
		return p;
	}
	p->uri.len = len;
	if (len > 0) {
		p->uri.s = p->presentity_data;
		memcpy(p->uri.s, presentity_uri->s, len);
	}
	else p->uri.s = NULL;
	p->first_tuple = NULL;
	p->last_tuple = NULL;
	p->first_note = NULL;
	p->last_note = NULL;

	/* RPID extensions */
	p->first_person = NULL;
	
	return p;
}

presence_tuple_info_t *create_tuple_info(const str_t *contact, const str_t *id, presence_tuple_status_t status)
{
	presence_tuple_info_t *t;
	t = (presence_tuple_info_t*)cds_malloc(sizeof(*t));
	if (!t) {
		ERROR_LOG("can't allocate memory for presence tuple info\n");
		return t;
	}
	/* str_clear(&t->contact.s); */
	if (str_dup(&t->contact, contact) != 0) {
		ERROR_LOG("can't allocate memory for contact\n");
		cds_free(t);
		return NULL;
	}
	if (str_dup(&t->id, id) != 0) {
		ERROR_LOG("can't allocate memory for id\n");
		str_free_content(&t->contact);
		cds_free(t);
		return NULL;
	}
	t->prev = NULL;
	t->next = NULL;
	t->status = status;
	t->priority = 0.0;
	t->first_note = NULL;
	t->last_note = NULL;
	return t;
}

void add_tuple_info(presentity_info_t *p, presence_tuple_info_t *t)
{
	DOUBLE_LINKED_LIST_ADD(p->first_tuple, p->last_tuple, t);
}

void free_presence_note(presence_note_t *n)
{
	if (n) {
		str_free_content(&n->value);
		str_free_content(&n->lang);
		cds_free(n);
	}
}

void free_tuple_info(presence_tuple_info_t *t)
{
	presence_note_t *n, *nn;
	
	if (!t) return;
	str_free_content(&t->contact);
	str_free_content(&t->id);
	
	n = t->first_note;
	while (n) {
		nn = n->next;
		free_presence_note(n);
		n = nn;
	}
	
	cds_free(t);
}

void free_person(person_t *p)
{
	if (p) {
		str_free_content(&p->person_element);
		str_free_content(&p->id);
		/* str_free_content(&p->mood);
		str_free_content(&p->activities); */
	}
	cds_free(p);
}

void free_presentity_info(presentity_info_t *p)
{
	presence_tuple_info_t *t, *tt;
	presence_note_t *n, *nn;
	person_t *np, *ps;
	
	if (!p) return;
	t = p->first_tuple;
	while (t) {
		tt = t->next;
		free_tuple_info(t);
		t = tt;
	}
	
	n = p->first_note;
	while (n) {
		nn = n->next;
		free_presence_note(n);
		n = nn;
	}
	
	ps = p->first_person;
	while (ps) {
		np = ps->next;
		free_person(ps);
		ps = np;
	}
	
	cds_free(p);
}

raw_presence_info_t *create_raw_presence_info(const str_t *uri)
{
	raw_presence_info_t *p;
	int len = 0;
	
	if (!is_str_empty(uri)) len = uri->len;
	p = (raw_presence_info_t*)cds_malloc(sizeof(raw_presence_info_t) + len);
	if (!p) {
		ERROR_LOG("can't allocate memory for list presence info\n");
		return p;
	}
	p->uri.len = len;
	if (len > 0) {
		p->uri.s = p->uri_data;
		memcpy(p->uri.s, uri->s, len);
	}
	else p->uri.s = NULL;
	
	str_clear(&p->pres_doc);
	str_clear(&p->content_type);
	
	return p;
}

void free_raw_presence_info(raw_presence_info_t *p)
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

presence_note_t *create_presence_note(const str_t *note, const str_t *lang)
{
	presence_note_t *t;
	t = (presence_note_t*)cds_malloc(sizeof(*t));
	if (!t) {
		ERROR_LOG("can't allocate memory for presence note\n");
		return t;
	}
	/* str_clear(&t->contact.s); */
	if (str_dup(&t->value, note) < 0) {
		ERROR_LOG("can't duplicate note value\n");
		cds_free(t);
		return NULL;
	}
	if (str_dup(&t->lang, lang) < 0) {
		ERROR_LOG("can't duplicate note lang\n");
		str_free_content(&t->value);
		cds_free(t);
		return NULL;
	}
	t->prev = NULL;
	t->next = NULL;
	return t;
}

presence_note_t *create_presence_note_zt(const char *note, const char *lang)
{
	str_t note_s;
	str_t lang_s;

	note_s = zt2str((char*)note);
	lang_s = zt2str((char*)lang);
	
	return create_presence_note(&note_s, &lang_s);
}

person_t *create_person(const str_t *element, const str_t *id)
{
	person_t *t;
	t = (person_t*)cds_malloc(sizeof(*t));
	if (!t) {
		ERROR_LOG("can't allocate memory for person\n");
		return t;
	}
	/* str_clear(&t->contact.s); */
	if (str_dup(&t->person_element, element) < 0) {
		ERROR_LOG("can't duplicate person element\n");
		cds_free(t);
		return NULL;
	}
	if (str_dup(&t->id, id) < 0) {
		ERROR_LOG("can't duplicate person element id\n");
		str_free_content(&t->person_element);
		cds_free(t);
		return NULL;
	}
	t->next = NULL;
	return t;
}

/*************************************************************/
static int copy_tuple_notes(presence_tuple_info_t *dst_info, 
		const presence_tuple_info_t *src)
{
	presence_note_t *n, *nn;

	n = src->first_note;
	while (n) {
		nn = create_presence_note(&n->value, &n->lang);
		if (!nn) {
			ERR("can't create presence note\n");
			return -1;
		}
		DOUBLE_LINKED_LIST_ADD(dst_info->first_note, dst_info->last_note, nn);
		n = n->next;
	}
	return 0;
}

presentity_info_t *dup_presentity_info(presentity_info_t *p)
{
	presentity_info_t *pinfo;
	presence_tuple_info_t *tinfo, *t;
	presence_note_t *n, *pan;
	person_t *ps, *last_ps, *paps;
	int err = 0;

	/* DBG("p2p_info()\n"); */
	if (!p) return NULL;
/*	pinfo = (presentity_info_t*)cds_malloc(sizeof(*pinfo)); */
	pinfo = create_presentity_info(&p->uri);
	if (!pinfo) {
		ERROR_LOG("can't allocate memory\n");
		return NULL;
	}
	/* DBG("p2p_info(): created presentity info\n"); */

	t = p->first_tuple;
	while (t) {
		tinfo = create_tuple_info(&t->contact, &t->id, t->status);
		if (!tinfo) {
			ERROR_LOG("can't create tuple info\n");
			err = 1;
			break;
		}
		tinfo->priority = t->priority;
		/* tinfo->expires = t->expires; ??? */
		add_tuple_info(pinfo, tinfo);
		if (copy_tuple_notes(tinfo, t) < 0) {
			ERROR_LOG("can't copy tuple notes\n");
			err = 1;
			break;
		}
		t = t->next;
	}

	/* notes */
	if (!err) {
		pan = p->first_note;
		while (pan) {
			n = create_presence_note(&pan->value, &pan->lang);
			if (n) DOUBLE_LINKED_LIST_ADD(pinfo->first_note, pinfo->last_note, n);
			else {
				ERROR_LOG("can't copy presence notes\n");
				err = 1;
				break;
			}
			pan = pan->next;
		}
	}
	
	/* person elements */
	if (!err) {
		last_ps = NULL;
		paps = p->first_person;
		while (paps) {
			ps = create_person(&paps->person_element, &paps->id);
			if (ps) LINKED_LIST_ADD(pinfo->first_person, last_ps, ps);
			else {
				ERROR_LOG("can't copy person elements\n");
				err = 1;
				break;
			}
			paps = paps->next;
		}
	}
	
	if (err) {
		free_presentity_info(pinfo);
		return NULL;
	}
	
	/* DBG("p2p_info() finished\n"); */
	return pinfo;
}

