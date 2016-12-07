/*
 * pua module
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
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
 *
 */

#ifndef PUA_PIDF_H
#define PUA_PIDF_H

#include "../../str.h"
#include <libxml/parser.h>

typedef xmlNodePtr (*xmlDocGetNodeByName_t)(xmlDocPtr doc, const char *name, const char *ns);
typedef xmlNodePtr (*xmlNodeGetNodeByName_t)(xmlNodePtr node, const char *name, const char *ns);
typedef char* (*xmlNodeGetNodeContentByName_t)(xmlNodePtr root, const char *name,
		const char *ns);
typedef char* (*xmlNodeGetAttrContentByName_t)(xmlNodePtr node, const char *name);
xmlNodePtr xmlNodeGetNodeByName(xmlNodePtr node, const char *name,
															const char *ns);
typedef struct libxml_api {
	xmlDocGetNodeByName_t xmlDocGetNodeByName;
	xmlNodeGetNodeByName_t xmlNodeGetNodeByName;
	xmlNodeGetNodeContentByName_t xmlNodeGetNodeContentByName;
	xmlNodeGetAttrContentByName_t xmlNodeGetAttrContentByName;
} libxml_api_t;

xmlNodePtr xmlDocGetNodeByName(xmlDocPtr doc, const char *name, const char *ns);
xmlNodePtr xmlNodeGetChildByName(xmlNodePtr node, const char *name);

char *xmlNodeGetNodeContentByName(xmlNodePtr root, const char *name,
		const char *ns);
char *xmlNodeGetAttrContentByName(xmlNodePtr node, const char *name);

typedef int (*bind_libxml_t)(libxml_api_t* api);

int bind_libxml_api(libxml_api_t* api);

#endif /* PUA_PIDF_H */
