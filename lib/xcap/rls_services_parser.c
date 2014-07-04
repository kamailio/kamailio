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

#include <xcap/rls_services_parser.h>
#include <xcap/xml_utils.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <cds/sstr.h>
#include <cds/logger.h>

static char rls_namespace[] = "urn:ietf:params:xml:ns:rls-services";
/*
static int read_entry(xmlNode *entry_node, entry_t **dst)
{
	xmlAttr *a;
	const char *a_val;
	
	/ * allocate memory and prepare empty node * /
	if (!dst) return -1;
	*dst = (entry_t*)cds_malloc(sizeof(entry_t));
	if (!(*dst)) return -2;
	memset(*dst, 0, sizeof(entry_t));

	/ * get attributes * /
	a = find_attr(entry_node->properties, "uri");
	if (a) {
		a_val = get_attr_value(a);
		if (a_val) (*dst)->uri = zt_strdup(a_val);
	}
	return 0;
}*/

static int read_package(xmlNode *n, package_t **dst)
{
	const char *name;
	if (!dst) return -1;
	
	*dst = (package_t*)cds_malloc(sizeof(package_t));
	if (!(*dst)) return -2;
	memset(*dst, 0, sizeof(package_t));

	name = get_node_value(n);
	if (name) (*dst)->name = zt_strdup(name);
	return 0;
}

static int read_packages(xmlNode *list_node, packages_t **dst)
{
	int res = 0;
	xmlNode *n;
	package_t *p, *last;
	
	/* allocate memory and prepare empty node */
	if (!dst) return -1;
	*dst = (packages_t*)cds_malloc(sizeof(packages_t));
	if (!(*dst)) return -2;
	memset(*dst, 0, sizeof(packages_t));

	/* read packages */
	n = list_node->children;
	last = NULL;
	while (n) {
		if (n->type == XML_ELEMENT_NODE) {
			if (cmp_node(n, "package", rls_namespace) >= 0) {
				res = read_package(n, &p);
				if ((res == 0) && p) {
					SEQUENCE_ADD((*dst)->package, last, p);
				}
			}
		}
		n = n->next;
	}
	
	return 0;
}

int read_service(xmlNode *list_node, service_t **dst)
{
	int res = 0;
	xmlAttr *a;
	const char *a_val;
	xmlNode *n;
	int first_node;

	DEBUG_LOG("read_service(): called\n");
	/* allocate memory and prepare empty node */
	if (!dst) return -1;
	*dst = (service_t*)cds_malloc(sizeof(service_t));
	if (!(*dst)) return -2;
	memset(*dst, 0, sizeof(service_t));

	/* get attributes */
	a = find_attr(list_node->properties, "uri");
	if (a) {
		a_val = get_attr_value(a);
		if (a_val) (*dst)->uri = zt_strdup(a_val);
	}

	/* read child nodes */
	n = list_node->children;
	first_node = 1;
	while (n) {
		if (n->type == XML_ELEMENT_NODE) {
			if (first_node) {
				/* element must be list or resource-list */
				if (cmp_node(n, "list", rls_namespace) >= 0) {
					res = read_list(n, &(*dst)->content.list, 0);
					if ( (res == 0) && ((*dst)->content.list) ) {
						(*dst)->content_type = stc_list;
					}
					else return -1;
				}
				else if (cmp_node(n, "resource-list", rls_namespace) >= 0) {
					a_val = get_node_value(n);
					if (a_val)
						(*dst)->content.resource_list = zt_strdup(a_val);
					else
						(*dst)->content.resource_list = NULL;
					(*dst)->content_type = stc_resource_list;
				}
				else return -1;

				first_node = 0;
			}
			else { /* packages node */
				if (cmp_node(n, "packages", rls_namespace) >= 0) {
					res = read_packages(n, &(*dst)->packages);
				}
				break;
			}
		}
		n = n->next;
	}
	
	return 0;
}

static int read_rls_services(xmlNode *root, rls_services_t **dst)
{
	/* xmlAttr *a; */
	xmlNode *n;
	service_t *l, *last_l;
	int res = 0;
	
	if (!root) return -1;
	if (!dst) return -1;
	
	if (cmp_node(root, "rls-services", rls_namespace) < 0) {
		ERROR_LOG("document is not a rls-services\n");
		return -1;
	}

	*dst = (rls_services_t*)cds_malloc(sizeof(rls_services_t));
	if (!(*dst)) return -2;
	(*dst)->rls_services = NULL;
	
	last_l = NULL;
	n = root->children;
	while (n) {
		if (n->type == XML_ELEMENT_NODE) {
			if (cmp_node(n, "service", rls_namespace) >= 0) {
				res = read_service(n, &l);
				if (res == 0) {
					if (l) SEQUENCE_ADD((*dst)->rls_services, last_l, l);
				}
				else break;
			}
		}
		n = n->next;
	}

	return res;
}

int parse_rls_services_xml(const char *data, int data_len, rls_services_t **dst)
{
	int res = 0;
	xmlDocPtr doc; /* the resulting document tree */

	doc = xmlReadMemory(data, data_len, NULL, NULL, xml_parser_flags);
	if (doc == NULL) {
		ERROR_LOG("can't parse document\n");
		return -1;
	}
	
	res = read_rls_services(xmlDocGetRootElement(doc), dst);

	xmlFreeDoc(doc);
	return res;
}

int parse_service(const char *data, int data_len, service_t **dst)
{
	int res = 0;
	xmlDocPtr doc; /* the resulting document tree */

	doc = xmlReadMemory(data, data_len, NULL, NULL, xml_parser_flags);
	if (doc == NULL) {
		ERROR_LOG("can't parse document\n");
		return -1;
	}
	
	res = read_service(xmlDocGetRootElement(doc), dst);

	xmlFreeDoc(doc);
	return res;
}

static void free_package(package_t *p)
{
	if (!p) return;
	if (p->name) cds_free(p->name);
	cds_free(p);
}

static void free_packages(packages_t *p)
{
	package_t *e, *f;
	if (!p) return;
	
	e = SEQUENCE_FIRST(p->package);
	while (e) {
		f = SEQUENCE_NEXT(e);
		free_package(e);
		e = f;
	}
	cds_free(p);
}

void free_service(service_t *s)
{
	if (!s) return;
	
	if (s->uri) cds_free(s->uri);

	switch (s->content_type) {
		case stc_list: free_list(s->content.list); break;
		case stc_resource_list: cds_free(s->content.resource_list); break;
	}
	
	free_packages(s->packages);

	cds_free(s);
}

void free_rls_services(rls_services_t *rls)
{
	service_t *e, *f;
	if (!rls) return;
	
	e = SEQUENCE_FIRST(rls->rls_services);
	while (e) {
		f = SEQUENCE_NEXT(e);
		free_service(e);
		e = f;
	}
	cds_free(rls);
}

