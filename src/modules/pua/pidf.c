/*
 * pua module
 *
 * Copyright (C) 2007 Voice Sistem S.R.L.
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

#include <string.h>
#include <stdlib.h>
#include <libxml/parser.h>
#include "../../dprint.h"
#include "../../sr_module.h"
#include "pidf.h"


xmlAttrPtr xmlNodeGetAttrByName(xmlNodePtr node, const char *name)
{
	xmlAttrPtr attr = node->properties;
	while (attr) {
		if (xmlStrcasecmp(attr->name, (unsigned char*)name) == 0)
			return attr;
		attr = attr->next;
	}
	return NULL;
}

char *xmlNodeGetAttrContentByName(xmlNodePtr node, const char *name)
{
	xmlAttrPtr attr = xmlNodeGetAttrByName(node, name);
	if (attr)
		return (char*)xmlNodeGetContent(attr->children);
	else
		return NULL;
}

xmlNodePtr xmlNodeGetChildByName(xmlNodePtr node, const char *name)
{
	xmlNodePtr cur = node->children;
	while (cur) {
		if (xmlStrcasecmp(cur->name, (unsigned char*)name) == 0)
			return cur;
		cur = cur->next;
	}
	return NULL;
}

xmlNodePtr xmlNodeGetNodeByName(xmlNodePtr node, const char *name,
															const char *ns)
{
	xmlNodePtr cur = node;
	while (cur) {
		xmlNodePtr match = NULL;
		if (xmlStrcasecmp(cur->name, (unsigned char*)name) == 0) {
			if (!ns || (cur->ns && xmlStrcasecmp(cur->ns->prefix,
							(unsigned char*)ns) == 0))
				return cur;
		}
		match = xmlNodeGetNodeByName(cur->children, name, ns);
		if (match)
			return match;
		cur = cur->next;
	}
	return NULL;
}

char *xmlNodeGetNodeContentByName(xmlNodePtr root, const char *name,
		const char *ns)
{
	xmlNodePtr node = xmlNodeGetNodeByName(root, name, ns);
	if (node)
		return (char*)xmlNodeGetContent(node->children);
	else
		return NULL;
}

xmlNodePtr xmlDocGetNodeByName(xmlDocPtr doc, const char *name, const char *ns)
{
	xmlNodePtr cur = doc->children;
	return xmlNodeGetNodeByName(cur, name, ns);
}

char *xmlDocGetNodeContentByName(xmlDocPtr doc, const char *name, 
		const char *ns)
{
	xmlNodePtr node = xmlDocGetNodeByName(doc, name, ns);
	if (node)
		return (char*)xmlNodeGetContent(node->children);
	else
		return NULL;
}

int bind_libxml_api(libxml_api_t* api)
{
	if (!api)
	{
		LM_ERR("Invalid parameter value\n");
		return -1;
	}
	api->xmlDocGetNodeByName         =  xmlDocGetNodeByName;
	api->xmlNodeGetNodeByName        =  xmlNodeGetNodeByName;
	api->xmlNodeGetNodeContentByName =  xmlNodeGetNodeContentByName;
	api->xmlNodeGetAttrContentByName =  xmlNodeGetAttrContentByName;
	
	return 0;
}	

