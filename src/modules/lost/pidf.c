/*
 * $Id: pidf.c 1953 2007-04-04 08:50:33Z anca_vamanu $
 *
 * presence module - presence server implementation
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

/*! \file
 * \brief Kamailio lost ::  PIDF handling
 * \ingroup lost
 */


/**
 * make strptime available
 * use 600 for 'Single UNIX Specification, Version 3'
 * _XOPEN_SOURCE creates conflict in header definitions in Solaris
 */
#ifndef __OS_solaris
#define _XOPEN_SOURCE 600 /* glibc2 on linux, bsd */
#define _BSD_SOURCE \
	1					  /* needed on linux to "fix" the effect
										  of the above define on
										  features.h/unistd.h syscall() */
#define _DEFAULT_SOURCE 1 /* _BSD_SOURCE is deprecated */
#define _DARWIN_C_SOURCE 1
#else
#define _XOPEN_SOURCE_EXTENDED 1 /* solaris */
#endif

#include <time.h>

#undef _XOPEN_SOURCE
#undef _XOPEN_SOURCE_EXTENDED

#include <string.h>
#include <stdlib.h>
#include <libxml/parser.h>

#include "../../core/mem/mem.h"
#include "../../core/dprint.h"

#include "pidf.h"

xmlAttrPtr xmlNodeGetAttrByName(xmlNodePtr node, const char *name)
{
	xmlAttrPtr attr = node->properties;
	while(attr) {
		if(xmlStrcasecmp(attr->name, (unsigned char *)name) == 0)
			return attr;
		attr = attr->next;
	}
	return NULL;
}

char *xmlNodeGetAttrContentByName(xmlNodePtr node, const char *name)
{
	xmlAttrPtr attr = xmlNodeGetAttrByName(node, name);
	if(attr)
		return (char *)xmlNodeGetContent(attr->children);
	else
		return NULL;
}

xmlNodePtr xmlNodeGetChildByName(xmlNodePtr node, const char *name)
{
	xmlNodePtr cur = node->children;
	while(cur) {
		if(xmlStrcasecmp(cur->name, (unsigned char *)name) == 0)
			return cur;
		cur = cur->next;
	}
	return NULL;
}

xmlNodePtr xmlNodeGetNodeByName(
		xmlNodePtr node, const char *name, const char *ns)
{
	xmlNodePtr cur = node;
	while(cur) {
		xmlNodePtr match = NULL;
		if(xmlStrcasecmp(cur->name, (unsigned char *)name) == 0) {
			if(!ns || (cur->ns && xmlStrcasecmp(cur->ns->prefix,
									(unsigned char *)ns) == 0))
				return cur;
		}
		match = xmlNodeGetNodeByName(cur->children, name, ns);
		if(match)
			return match;
		cur = cur->next;
	}
	return NULL;
}

char *xmlNodeGetNodeContentByName(
		xmlNodePtr root, const char *name, const char *ns)
{
	xmlNodePtr node = xmlNodeGetNodeByName(root, name, ns);
	if(node)
		return (char *)xmlNodeGetContent(node->children);
	else
		return NULL;
}

xmlNodePtr xmlDocGetNodeByName(xmlDocPtr doc, const char *name, const char *ns)
{
	xmlNodePtr cur = doc->children;
	return xmlNodeGetNodeByName(cur, name, ns);
}

char *xmlDocGetNodeContentByName(
		xmlDocPtr doc, const char *name, const char *ns)
{
	xmlNodePtr node = xmlDocGetNodeByName(doc, name, ns);
	if(node)
		return (char *)xmlNodeGetContent(node->children);
	else
		return NULL;
}

xmlXPathObjectPtr xmlGetNodeSet(xmlDocPtr doc, xmlChar *xpath, xmlChar *ns)
{

	xmlXPathContextPtr context = NULL;
	xmlXPathObjectPtr result = NULL;

	context = xmlXPathNewContext(doc);
	if(context == NULL) {
		return NULL;
	}

	if((ns != NULL) && (xmlRegisterNamespaces(context, ns) < 0)) {
		xmlXPathFreeContext(context);
		return NULL;
	}

	result = xmlXPathEvalExpression(xpath, context);
	xmlXPathFreeContext(context);

	if(result == NULL) {
		LM_ERR("xmlXPathEvalExpression() failed\n");
		return NULL;
	}
	if(xmlXPathNodeSetIsEmpty(result->nodesetval)) {
		xmlXPathFreeObject(result);
		LM_DBG("xmlXPathEvalExpression() returned no result\n");
		return NULL;
	}

	return result;
}

int xmlRegisterNamespaces(xmlXPathContextPtr context, const xmlChar *ns)
{
	xmlChar *nsListDup;
	xmlChar *prefix;
	xmlChar *href;
	xmlChar *next;

	nsListDup = xmlStrdup(ns);
	if(nsListDup == NULL) {
		return -1;
	}

	next = nsListDup;
	while(next != NULL) {
		/* skip spaces */
		while((*next) == ' ')
			next++;
		if((*next) == '\0')
			break;

		/* find prefix */
		prefix = next;
		next = (xmlChar *)xmlStrchr(next, '=');
		if(next == NULL) {
			xmlFree(nsListDup);
			return -1;
		}
		*(next++) = '\0';

		/* find href */
		href = next;
		next = (xmlChar *)xmlStrchr(next, ' ');
		if(next != NULL) {
			*(next++) = '\0';
		}

		/* register namespace */
		if(xmlXPathRegisterNs(context, prefix, href) != 0) {
			xmlFree(nsListDup);
			return -1;
		}
	}

	xmlFree(nsListDup);
	return 0;
}