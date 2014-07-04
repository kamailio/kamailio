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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
	presence_tuple_closed,
	presence_tuple_undefined_status
} basic_tuple_status_t;

typedef enum {
	presence_auth_rejected,
	presence_auth_polite_block,
	presence_auth_unresolved,
	presence_auth_granted
} presence_authorization_status_t;

/* additional data which need not to be understood by us */
typedef struct _extension_element_t {
	str_t element;
	/* TODO: add mustUnderstand attribute and its handling */
	struct _extension_element_t *next, *prev; /* there can be more person elements in PIDF */
} extension_element_t;

typedef struct {
	basic_tuple_status_t basic;
	extension_element_t *first_unknown_element, *last_unknown_element;
} presence_tuple_status_t;

typedef struct _presence_tuple_info_t {
	str_t contact;
	str_t id;
	double priority;
	presence_tuple_status_t status;
	extension_element_t *first_unknown_element, *last_unknown_element;
	struct _presence_tuple_info_t *next, *prev;
	presence_note_t *first_note, *last_note;/* published notes */
	/* TODO: add timestamp element */
} presence_tuple_info_t;

typedef struct {
	str_t uri; /* do not modify this !*/
	presence_tuple_info_t *first_tuple, *last_tuple;
	presence_note_t *first_note, *last_note;/* published notes */
	extension_element_t *first_unknown_element, *last_unknown_element;
		
	char presentity_data[1];
} presentity_info_t;

typedef struct {
	str_t uri; /* do not modify this !*/
	
	str_t pres_doc;
	str_t content_type;
	char uri_data[1];
} raw_presence_info_t;

typedef struct {
	str_t list_uri; /* do not modify this !*/

	str_t pres_doc;
	str_t content_type;
	char uri_data[1];
} presence_info_t;

presentity_info_t *create_presentity_info(const str_t *presentity);
presence_tuple_info_t *create_tuple_info(const str_t *contact, const str_t *id, basic_tuple_status_t status);
void add_tuple_info(presentity_info_t *p, presence_tuple_info_t *t);
void free_presentity_info(presentity_info_t *p);

raw_presence_info_t *create_raw_presence_info(const str_t *uri);
void free_raw_presence_info(raw_presence_info_t *p);

presence_note_t *create_presence_note(const str_t *note, const str_t *lang);
presence_note_t *create_presence_note_zt(const char *note, const char *lang);
void free_presence_note(presence_note_t *n);

extension_element_t *create_extension_element(const str_t *element);
void free_extension_element(extension_element_t *p);

/** returns pointer to constant string (do not free it!),
 * the return value is never NULL */
str_t* tuple_status2str(basic_tuple_status_t status);

basic_tuple_status_t str2tuple_status(const str_t *s);

/* duplicates presentity info */
presentity_info_t *dup_presentity_info(presentity_info_t *p);

/* content type names usable with QSA */
#define CT_PRESENCE_INFO    "structured/presence-info" /* uses presence_info_t */
#define CT_PIDF_XML         "application/pidf+xml" /* carries XML */
#define CT_RAW              "raw" /* uses raw_presence_info_t */

#endif
