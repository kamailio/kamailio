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

#ifndef __RLS_SERVICES_PARSER_PARSER_H
#define __RLS_SERVICES_PARSER_PARSER_H

#include <xcap/xml_utils.h>
#include <xcap/resource_lists_parser.h>

typedef struct _package_t {
	SEQUENCE_ABLE(struct _package_t)
	char *name;
} package_t;

typedef struct _packages_t {
	SEQUENCE(package_t) package;
} packages_t;

typedef enum { 
	stc_list,
	stc_resource_list,
} service_content_type_t;

typedef struct _service_t {
	SEQUENCE_ABLE(struct _service_t)
		
	packages_t *packages;
	
	service_content_type_t content_type;
	union {
		list_t *list;
		char *resource_list; /* uri */
	} content;
	
	char *uri;
} service_t;

typedef struct {
	SEQUENCE(service_t) rls_services;
} rls_services_t;

int parse_rls_services_xml(const char *data, int data_len, rls_services_t **dst);
int parse_service(const char *data, int data_len, service_t **dst);
void free_rls_services(rls_services_t *rl);
void free_service(service_t *s);
int read_service(xmlNode *list_node, service_t **dst);

#endif
