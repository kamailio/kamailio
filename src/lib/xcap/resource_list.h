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

#ifndef __RESOURCE_LIST_H
#define __RESOURCE_LIST_H

#include <xcap/rls_services_parser.h>
#include <xcap/xcap_client.h>
#include <cds/sstr.h>
/* #include <cds/ptr_vector.h> */

/* Functions for downloading the service documents with analyzis
 * and "flatting" - see draft-ietf-simple-xcap-list-usage */

typedef struct _flat_list_t {
	struct _flat_list_t *next;
	char *uri;
	SEQUENCE(display_name_t) names;
} flat_list_t;

char *xcap_uri_for_rls_resource(const str_t *xcap_root, const str_t *uri);
void canonicalize_uri(const str_t *uri, str_t *dst);
int get_rls(const str_t *uri, xcap_query_params_t *xcap_params, 
		const str_t *package, flat_list_t **dst);
int get_rls_from_full_doc(const str_t *uri, 
		/* const str_t *filename,  */
		xcap_query_params_t *xcap_params, 
		const str_t *package, flat_list_t **dst);
int get_resource_list_from_full_doc(const str_t *xcap_root, const str_t *user, xcap_query_params_t *xcap_params, const char *list_name, flat_list_t **dst);
/* TODO: int get_resource_list(const str_t *xcap_root, const str_t *user, xcap_query_t *xcap_params, const str_t *list_name, flat_list_t **dst); */
void free_flat_list(flat_list_t *list);

#endif
