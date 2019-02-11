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

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <cds/sstr.h>

int xmlstrcmp(const xmlChar *xmls, const char *name);
xmlAttr *find_attr(xmlAttr *first, const char *name);
const char *find_value(xmlNode *first_child);
const char *get_node_value(xmlNode *n);
xmlNode *find_node(xmlNode *parent, const char *name, const char *nspace);
const char *get_attr_value(xmlAttr *a);
int cmp_node(xmlNode *node, const char *name, const char *nspace);
int get_int_attr(xmlNode *n, const char *attr_name, int *dst);
int get_str_attr(xmlNode *n, const char *attr_name, str_t *dst);

time_t xmltime2time(const char *xt);

#define SEQUENCE(type)	type*
#define SEQUENCE_ABLE(type)	type *__next;
#define SEQUENCE_ADD(first,last,e) do { \
	if (last) last->__next = e; \
	else first = e; \
	last = e; } while(0);
#define SEQUENCE_FIRST(first) first
#define SEQUENCE_NEXT(e) (e)->__next

extern int xml_parser_flags;
