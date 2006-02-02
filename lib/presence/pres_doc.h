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

#ifndef __PRESENCE_INFO_H
#define __PRESENCE_INFO_H

#include <cds/sstr.h>
#include <cds/ptr_vector.h>
#include <time.h>

typedef struct _presence_note_t {
	str_t value;
	str_t lang;
	struct _presence_note_t *prev, *next;
} presence_note_t;

typedef enum {
	presence_tuple_open,
	presence_tuple_closed
} presence_tuple_status_t;

typedef enum {
	presence_auth_rejected,
	presence_auth_polite_block,
	presence_auth_unresolved,
	presence_auth_granted
} presence_authorization_status_t;

typedef struct _presence_tuple_info_t {
	str_t contact;
	str_t id;
	double priority;
	time_t expires;
	presence_tuple_status_t status;
	str_t extra_status;
	struct _presence_tuple_info_t *next, *prev;
	presence_note_t *first_note, *last_note;/* published notes */
} presence_tuple_info_t;

/* additional data taken from RPID specification */
typedef struct _person_t {
	str_t id;

	/* mood has absolutely no value for our processing - 
	 * we hold there the content of <mood> element */
/*	str_t mood;*/
	
	/* other such element */
/*	str_t activities;*/
	str_t person_element;

	struct _person_t *next; /* there can be more person elements in PIDF */
} person_t;

typedef struct {
	str_t presentity; /* do not modify this !*/
	presence_tuple_info_t *first_tuple, *last_tuple;
	presence_authorization_status_t auth;
	presence_note_t *first_note, *last_note;/* published notes */
	person_t *first_person;
		
	char presentity_data[1];
} presentity_info_t;

typedef struct {
	str_t list_uri; /* do not modify this !*/
	
	/* vector of pointers to structures presentity_info_t */
	/* ptr_vector_t presentity_infos; */
	
	/* FIXME: the presence tree may include many lists
	 * and may be deep - isn't better to work with
	 * whole documents than with some structures? */
	
	str_t pres_doc;
	str_t content_type;
	char uri_data[1];
} list_presence_info_t;

presentity_info_t *create_presentity_info(const str_t *presentity);
presence_tuple_info_t *create_tuple_info(const str_t *contact, const str_t *id, presence_tuple_status_t status);
void add_tuple_info(presentity_info_t *p, presence_tuple_info_t *t);
void free_presentity_info(presentity_info_t *p);

list_presence_info_t *create_list_presence_info(const str_t *uri);
void free_list_presence_info(list_presence_info_t *p);

presence_note_t *create_presence_note(const str_t *note, const str_t *lang);
presence_note_t *create_presence_note_zt(const char *note, const char *lang);
void free_presence_note(presence_note_t *n);

person_t *create_person(const str_t *element, const str_t *id);

#endif
