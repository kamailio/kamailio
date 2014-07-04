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

#ifndef __RESOURCE_LISTS_PARSER_H
#define __RESOURCE_LISTS_PARSER_H

#include <xcap/xml_utils.h>
#include <cds/memory.h>

typedef struct _display_name_t {
	SEQUENCE_ABLE(struct _display_name_t)
	char *name;
	char *lang;
} display_name_t;

typedef struct {
	char *uri;
	SEQUENCE(display_name_t) display_names;
} entry_t;

typedef struct {
	char *anchor;
	/* SEQUENCE(display_name_t) display_names; */
} external_t;

typedef struct {
	char *ref;
	/* SEQUENCE(display_name_t) display_names; */
} entry_ref_t;

typedef enum { 
	lct_list,
	lct_entry,
	lct_entry_ref,
	lct_external
} list_content_type_t;

struct _list_t;

typedef struct _list_content_t {
	SEQUENCE_ABLE(struct _list_content_t)
		
	list_content_type_t type;
	union {
		struct _list_t *list;
		entry_t *entry;
		entry_ref_t *entry_ref;
		external_t *external;
	} u;
} list_content_t;

typedef struct _list_t {
	SEQUENCE_ABLE(struct _list_t)
		
/*	entry_t *entries;*/
	char *display_name;
	
	SEQUENCE(list_content_t) content;
	
	char *name;
} list_t;

typedef struct {
	/* list_t *lists; */
	SEQUENCE(list_t) lists;
	
} resource_lists_t;

int parse_resource_lists_xml(const char *data, int data_len, resource_lists_t **dst);
int parse_list_xml(const char *data, int data_len, list_t **dst);
int parse_as_list_content_xml(const char *data, int data_len, list_t **dst);
int parse_entry_xml(const char *data, int data_len, entry_t **dst);
void free_resource_lists(resource_lists_t *rl);
void free_list(list_t *l);
void free_entry(entry_t *e);
void free_display_names(display_name_t *sequence_first);

/* if read_content_only set then the root element need not to be a list element
 * it may be whatever, but content is parsed as if it is list (useful for reading
 * resource-list as list */
int read_list(xmlNode *list_node, list_t **dst, int read_content_only);

#endif
