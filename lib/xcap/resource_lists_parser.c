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

#include <xcap/resource_lists_parser.h>
#include <xcap/xml_utils.h>
#include <cds/logger.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <cds/sstr.h>

static char rl_namespace[] = "urn:ietf:params:xml:ns:resource-lists";

static int read_entry_ref(xmlNode *entry_node, entry_ref_t **dst)
{
	xmlAttr *a;
	const char *a_val;
	
	/* allocate memory and prepare empty node */
	if (!dst) return -1;
	*dst = (entry_ref_t*)cds_malloc(sizeof(entry_ref_t));
	if (!(*dst)) return -2;
	memset(*dst, 0, sizeof(entry_ref_t));

	/* get attributes */
	a = find_attr(entry_node->properties, "ref");
	if (a) {
		a_val = get_attr_value(a);
		if (a_val) (*dst)->ref = zt_strdup(a_val);
	}
	return 0;
}

static int read_name(xmlNode *name_node, display_name_t **dst)
{
	xmlAttr *a;
	const char *a_val;
	
	/* allocate memory and prepare empty node */
	if (!dst) return -1;
	*dst = (display_name_t*)cds_malloc(sizeof(display_name_t));
	if (!(*dst)) return -2;
	memset(*dst, 0, sizeof(display_name_t));

	/* get attributes */
	a = find_attr(name_node->properties, "lang");
	if (a) {
		a_val = get_attr_value(a);
		if (a_val) (*dst)->lang = zt_strdup(a_val);
	}

	a_val = get_node_value(name_node);
	if (a_val) (*dst)->name = zt_strdup(a_val);

	return 0;
}

static int read_names(xmlNode *entry_node, display_name_t **dst)
{
	xmlNode *n;
	display_name_t *name, *last;
	int res = 0;
	
	last = NULL;
	*dst = NULL;
	n = entry_node->children;
	while (n) {
		if (n->type == XML_ELEMENT_NODE) {
			if (cmp_node(n, "display-name", rl_namespace) >= 0) {
				res = read_name(n, &name);
				if (res == 0) {
					if (name) {
						SEQUENCE_ADD((*dst), last, name);
						name = NULL;
					}
				}
				else break;
			}
		}
		n = n->next;
	}
	return res;
}

static int read_entry(xmlNode *entry_node, entry_t **dst)
{
	xmlAttr *a;
	const char *a_val;
	
	/* allocate memory and prepare empty node */
	if (!dst) return -1;
	*dst = (entry_t*)cds_malloc(sizeof(entry_t));
	if (!(*dst)) return -2;
	memset(*dst, 0, sizeof(entry_t));

	/* get attributes */
	a = find_attr(entry_node->properties, "uri");
	if (a) {
		a_val = get_attr_value(a);
		if (a_val) (*dst)->uri = zt_strdup(a_val);
	}

	return read_names(entry_node, &((*dst)->display_names));
}

static int read_external(xmlNode *entry_node, external_t **dst)
{
	xmlAttr *a;
	const char *a_val;
	
	/* allocate memory and prepare empty node */
	if (!dst) return -1;
	*dst = (external_t*)cds_malloc(sizeof(external_t));
	if (!(*dst)) return -2;
	memset(*dst, 0, sizeof(external_t));

	/* get attributes */
	a = find_attr(entry_node->properties, "anchor");
	if (a) {
		a_val = get_attr_value(a);
		if (a_val) (*dst)->anchor = zt_strdup(a_val);
	}
	return 0;
}

int read_list(xmlNode *list_node, list_t **dst, int read_content_only)
{
	int res = 0;
	xmlAttr *a;
	const char *a_val;
	xmlNode *n;
	list_content_t *l, *last_l;
	
	/* allocate memory and prepare empty node */
	if (!dst) return -1;
	*dst = (list_t*)cds_malloc(sizeof(list_t));
	if (!(*dst)) return -2;
	memset(*dst, 0, sizeof(list_t));

	/* get attributes */
	if (!read_content_only) {
		a = find_attr(list_node->properties, "name");
		if (a) {
			a_val = get_attr_value(a);
			if (a_val) (*dst)->name = zt_strdup(a_val);
		}
	}

	/* read entries */
	last_l = NULL;
	n = list_node->children;
	while (n) {
		if (n->type == XML_ELEMENT_NODE) {
			l = (list_content_t*) cds_malloc(sizeof(list_content_t));
			if (!l) return -1;
			memset(l, 0, sizeof(*l));
			
			if (cmp_node(n, "list", rl_namespace) >= 0) {
				res = read_list(n, &l->u.list, 0);
				if (res == 0) {
					if (l->u.list) {
						l->type = lct_list;
						SEQUENCE_ADD((*dst)->content, last_l, l);
						l = NULL;
					}
				}
				else break;
			}
			
			if (cmp_node(n, "entry", rl_namespace) >= 0) {
				res = read_entry(n, &l->u.entry);
				if (res == 0) {
					if (l->u.entry) {
						l->type = lct_entry;
						SEQUENCE_ADD((*dst)->content, last_l, l);
						l = NULL;
					}
				}
				else break;
			}
			
			if (cmp_node(n, "entry-ref", rl_namespace) >= 0) {
				res = read_entry_ref(n, &l->u.entry_ref);
				if (res == 0) {
					if (l->u.entry_ref) {
						l->type = lct_entry_ref;
						SEQUENCE_ADD((*dst)->content, last_l, l);
						l = NULL;
					}
				}
				else break;
			}
			
			if (cmp_node(n, "external", rl_namespace) >= 0) {
				res = read_external(n, &l->u.external);
				if (res == 0) {
					if (l->u.external) {
						l->type = lct_external;
						SEQUENCE_ADD((*dst)->content, last_l, l);
						l = NULL;
					}
				}
				else break;
			}
			
			if (l) {
				cds_free(l);
				l = NULL;
			}
			
		}
		n = n->next;
	}
	
	return 0;
}

static int read_resource_lists(xmlNode *root, resource_lists_t **dst)
{
	resource_lists_t *rl;
	/* xmlAttr *a; */
	xmlNode *n;
	list_t *l, *last_l;
	int res = 0;
	
	if (!dst) return -1;
	else *dst = NULL;
	if (!root) return -1;
	
	if (cmp_node(root, "resource-lists", rl_namespace) < 0) {
		ERROR_LOG("document is not a resource-lists\n");
		return -1;
	}

	rl = (resource_lists_t*)cds_malloc(sizeof(resource_lists_t));
	if (!rl) return -2;
	*dst = rl;
	rl->lists = NULL;
	
	last_l = NULL;
	n = root->children;
	while (n) {
		if (n->type == XML_ELEMENT_NODE) {
			if (cmp_node(n, "list", rl_namespace) >= 0) {
				res = read_list(n, &l, 0);
				if (res == 0) {
					if (l) SEQUENCE_ADD(rl->lists, last_l, l);
				}
				else break;
			}
		}
		n = n->next;
	}

	return res;
}

int parse_resource_lists_xml(const char *data, int data_len, resource_lists_t **dst)
{
	int res = 0;
	xmlDocPtr doc; /* the resulting document tree */

	if (dst) *dst = NULL;
	doc = xmlReadMemory(data, data_len, NULL, NULL, xml_parser_flags);
	if (doc == NULL) {
		ERROR_LOG("can't parse document\n");
		return -1;
	}
	
	res = read_resource_lists(xmlDocGetRootElement(doc), dst);

	xmlFreeDoc(doc);
	return res;
}

int parse_list_xml(const char *data, int data_len, list_t **dst)
{
	int res = 0;
	xmlDocPtr doc; /* the resulting document tree */

	if (dst) *dst = NULL;
	doc = xmlReadMemory(data, data_len, NULL, NULL, xml_parser_flags);
	if (doc == NULL) {
		ERROR_LOG("can't parse document\n");
		return -1;
	}
	
	res = read_list(xmlDocGetRootElement(doc), dst, 0);

	xmlFreeDoc(doc);
	return res;
}

int parse_as_list_content_xml(const char *data, int data_len, list_t **dst)
{
	int res = 0;
	xmlDocPtr doc; /* the resulting document tree */

	if (dst) *dst = NULL;
	doc = xmlReadMemory(data, data_len, NULL, NULL, xml_parser_flags);
	if (doc == NULL) {
		ERROR_LOG("can't parse document\n");
		return -1;
	}
	
	res = read_list(xmlDocGetRootElement(doc), dst, 1);

	xmlFreeDoc(doc);
	return res;
}

int parse_entry_xml(const char *data, int data_len, entry_t **dst)
{
	int res = 0;
	xmlDocPtr doc; /* the resulting document tree */

	if (dst) *dst = NULL;
	doc = xmlReadMemory(data, data_len, NULL, NULL, xml_parser_flags);
	if (doc == NULL) {
		ERROR_LOG("can't parse document\n");
		return -1;
	}
	
	res = read_entry(xmlDocGetRootElement(doc), dst);

	xmlFreeDoc(doc);
	return res;
}

void free_display_name(display_name_t *n)
{
	if (!n) return;
	if (n->name) cds_free(n->name);
	if (n->lang) cds_free(n->lang);
	cds_free(n);
}

void free_display_names(display_name_t *sequence_first)
{
	display_name_t *d, *n;
	
	if (!sequence_first) return;
	
	d = SEQUENCE_FIRST(sequence_first);
	while (d) {
		n = SEQUENCE_NEXT(d);
		free_display_name(d);
		d = n;
	}
	
}

void free_entry(entry_t *e)
{
	if (!e) return;
	
	if (e->uri) cds_free(e->uri);
	free_display_names(e->display_names);
	
	cds_free(e);
}

void free_entry_ref(entry_ref_t *e)
{
	if (!e) return;
	if (e->ref) cds_free(e->ref);
	cds_free(e);
}

void free_external(external_t *e)
{
	if (!e) return;
	if (e->anchor) cds_free(e->anchor);
	cds_free(e);
}

void free_list(list_t *l)
{
	list_content_t *e, *f;

	if (!l) return;
	
	if (l->name) cds_free(l->name);

	e = SEQUENCE_FIRST(l->content);
	while (e) {
		switch (e->type) {
			case lct_list: free_list(e->u.list); break;
			case lct_entry: free_entry(e->u.entry); break;
			case lct_entry_ref: free_entry_ref(e->u.entry_ref); break;
			case lct_external: free_external(e->u.external); break;
		}
		f = e;
		e = SEQUENCE_NEXT(e);
		/* TRACE_LOG("freeing %p\n", f); */
		cds_free(f);
	}
	cds_free(l);
}


void free_resource_lists(resource_lists_t *rl)
{
	list_t *e, *f;
	if (!rl) return;
	
	e = SEQUENCE_FIRST(rl->lists);
	while (e) {
		f = SEQUENCE_NEXT(e);
		free_list(e);
		e = f;
	}
	cds_free(rl);
}

