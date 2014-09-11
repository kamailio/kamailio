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

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <cds/logger.h>
#include <xcap/xml_utils.h>

int xml_parser_flags = XML_PARSE_NOERROR | XML_PARSE_NOWARNING;

static int str2int(const char *s, int *dst)
{
	/* if not null sets the given integer to value */
	if (!s) {
		*dst = 0;
		return -1;
	}
	else *dst = atoi(s);
	return 0;
}

int get_int_attr(xmlNode *n, const char *attr_name, int *dst)
{
	const char *s = get_attr_value(find_attr(n->properties, attr_name));
	if (!s) return 1;
	return str2int(s, dst);
}

int get_str_attr(xmlNode *n, const char *attr_name, str_t *dst)
{
	const char *s = get_attr_value(find_attr(n->properties, attr_name));
	if (!s) {
		str_clear(dst);
		return 1;
	}
	else return str_dup_zt(dst, s);
}

int xmlstrcmp(const xmlChar *xmls, const char *name)
{
	if (!xmls) return -1;
	if (!name) return 1;
	return strcmp((const char*)xmls, name);
}

/* xmlNode *find_node(xmlNode *parent, const char *name) */
xmlNode *find_node(xmlNode *parent, const char *name, const char *nspace)
{
	if (!parent) return NULL;
	xmlNode *n = parent->children;
	while (n) {
		if (cmp_node(n, name, nspace) >= 0) break;
		n = n->next;
	}
	return n;
}

const char *find_value(xmlNode *first_child)
{
	const char *s = NULL;
	
	xmlNode *c = first_child;
	while (c) {
		if (c->type == XML_TEXT_NODE) {
			if (c->content) s = (const char *)c->content;
			break;
		}
		c = c->next;
	}
	
	return s;
}

const char *get_node_value(xmlNode *n)
{
	if (!n) return NULL;
	return find_value(n->children);
}

xmlAttr *find_attr(xmlAttr *first, const char *name)
{
	xmlAttr *a = first;
	while (a) {
		if (xmlstrcmp(a->name, name) == 0) break;
		a = a->next;
	}
	return a;
}

const char *get_attr_value(xmlAttr *a) 
{
	if (!a) return NULL;
	return find_value(a->children);
}

int cmp_node(xmlNode *node, const char *name, const char *nspace)
{
	if (!node) return -1;
	if (node->type != XML_ELEMENT_NODE) return -1;
	
	if (xmlstrcmp(node->name, name) != 0) return -1;
	if (!nspace) return 0;
	if (!node->ns) {
		/* DEBUG_LOG("nemam NS!!!!!!!\n"); */
		return 1;
	}
	if (xmlstrcmp(node->ns->href, nspace) == 0) return 0;
	return -1;
}

time_t xmltime2time(const char *xt)
{
	/* TODO: translate XML time in input parametr to time_t structure */
	ERROR_LOG("can't translate xmltime to time_t: not finished yet!\n");
	return 0;
}

