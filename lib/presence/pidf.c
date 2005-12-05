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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <presence/pidf.h>
#include <cds/dstring.h>
#include <cds/memory.h>
#include <cds/logger.h>
#include <cds/list.h>
#include <presence/xml_utils.h>
#include <string.h>

/* ------------------------------ PIDF document creation ------------------------------ */

static void doc_add_tuple(dstring_t *buf, presentity_info_t *p, presence_tuple_info_t *t)
{
	char tmp[32];
	DEBUG_LOG("doc_add_tuple()\n");
	
	dstr_append_zt(buf, "\t<tuple id=\"");
	dstr_append_str(buf, &t->id);
	dstr_append_zt(buf, "\">\r\n");
	
	if (t->status == presence_tuple_open) dstr_append_zt(buf, "\t\t<status><basic>open</basic></status>\r\n");
	else dstr_append_zt(buf, "\t\t<status><basic>closed</basic></status>\r\n");
	
	dstr_append_zt(buf, "\t\t<contact priority=\"");
	sprintf(tmp, "%1.2f", t->priority);
	dstr_append_zt(buf, tmp);
	dstr_append_zt(buf, "\">");
	dstr_append_str(buf, &t->contact);
	dstr_append_zt(buf, "</contact>\r\n");

	dstr_append_zt(buf, "\t</tuple>\r\n");
}

static void doc_add_note(dstring_t *buf, presentity_info_t *p, presence_note_t *n)
{
	DEBUG_LOG("doc_add_tuple()\n");
	
	dstr_append_zt(buf, "\t<note");
	if (n->lang.len > 0) {
		dstr_append_zt(buf, " lang=\"");
		dstr_append_str(buf, &n->lang);
		dstr_append_zt(buf, "\"");
	}
	dstr_append_zt(buf, ">");
	dstr_append_str(buf, &n->value);	
	dstr_append_zt(buf, "</note>\r\n");
}

static void doc_add_presentity(dstring_t *buf, presentity_info_t *p)
{
	presence_tuple_info_t *t;
	presence_note_t *n;

	DEBUG_LOG("doc_add_presentity()\n");
	dstr_append_zt(buf, "<presence xmlns=\"urn:ietf:params:xml:ns:pidf\" entity=\"");
	dstr_append_str(buf, &p->presentity);
	dstr_append_zt(buf, "\">\r\n");
	
	DEBUG_LOG("doc_add_presentity(): adding tuples\n");
	t = p->first_tuple;
	while (t) {
		doc_add_tuple(buf, p, t);
		t = t->next;
	}
	
	DEBUG_LOG("doc_add_presentity(): adding notes\n");
	n = p->first_note;
	while (n) {
		doc_add_note(buf, p, n);
		n = n->next;
	}

	dstr_append_zt(buf, "</presence>");
}

		
int create_pidf_document(presentity_info_t *p, str_t *dst, str_t *dst_content_type)
{
	dstring_t buf;
	
	if (!dst) return -1;
	
	str_clear(dst);
	if (dst_content_type) str_clear(dst_content_type);

	if (!p) return -1;
	
	if (dst_content_type) 
		str_dup_zt(dst_content_type, "application/pidf+xml;charset=\"UTF-8\"");

	if (!p->first_tuple) return 0;	/* no tuples => nothing to say */ 
	
	dstr_init(&buf, 2048);
	
	dstr_append_zt(&buf, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n");
	doc_add_presentity(&buf, p);
	
	dst->len = dstr_get_data_length(&buf);
	dst->s = cds_malloc(dst->len);
	if (!dst->s) dst->len = 0;
	else dstr_get_data(&buf, dst->s);
	dstr_destroy(&buf);
	
	return 0;
}

/* ------------------------------ PIDF document parsing ------------------------------ */

static char *pidf_ns = "urn:ietf:params:xml:ns:pidf";

static int read_note(xmlNode *node, presence_note_t **dst)
{
	const char *note = NULL;
	const char *lang = NULL;

	note = get_node_value(node);
	lang = get_attr_value(find_attr(node->properties, "lang"));

	*dst = create_presence_note_zt(note, lang);
	if (!dst) return -1;
	
	return 0;
}


static int read_tuple(xmlNode *tuple, presence_tuple_info_t **dst)
{
	str_t contact, id;
	presence_tuple_status_t status;
	xmlNode *n;
	double priority = 0;
	const char *s;
	int res = 0;
	presence_note_t *note;

	*dst = NULL;

	DEBUG_LOG("read_tuple()\n");
	/* process contact (only one node) */
	n = find_node(tuple, "contact", pidf_ns);
	if (!n) {
		ERROR_LOG("contact not found\n");
		return -1;
	}
	s = get_attr_value(find_attr(n->properties, "priority"));
	if (s) priority = atof(s);
	s = get_node_value(n);
	contact.s = (char *)s;
	if (s) contact.len = strlen(s);
	else contact.len = 0;
	if (contact.len < 1) {
		ERROR_LOG("empty contact\n");
		return -1;
	}	
	
	/* process status (only one node) */
	n = find_node(tuple, "status", pidf_ns);
	if (!n) {
		ERROR_LOG("status not found\n");
		return -1;
	}
	n = find_node(n, "basic", pidf_ns);
	if (!n) {
		ERROR_LOG("basic status not found\n");
		return -1;
	}
	s = get_node_value(n);
	if (!s) {
		ERROR_LOG("basic status without value\n");
		return -1;
	}

	/* translate status */
	status = presence_tuple_closed; /* default value */
	if (strcmp(s, "open") == 0) status = presence_tuple_open;
	if (strcmp(s, "closed") == 0) status = presence_tuple_closed;
	/* FIXME: handle not standardized variants too (add note to basic status) */
	
	/* get ID from tuple node attribute? */
	id.s = (char *)get_attr_value(find_attr(tuple->properties, "id"));
	if (id.s) id.len = strlen(id.s);
	else id.len = 0;
	
	*dst = create_tuple_info(&contact, &id, status);
	if (!(*dst)) return -1;

	(*dst)->priority = priority;

	/* handle notes */
	n = tuple->children;
	while (n) {
		if (n->type == XML_ELEMENT_NODE) {
			if (cmp_node(n, "note", pidf_ns) >= 0) {
				res = read_note(n, &note);
				if ((res == 0) && note) {
					DOUBLE_LINKED_LIST_ADD((*dst)->first_note, 
							(*dst)->last_note, note);
				}
				else break;
			}
		}
		n = n->next;
	}

	return res;
}

static int read_presentity(xmlNode *root, presentity_info_t **dst)
{
	xmlNode *n;
	str_t entity;
	presence_tuple_info_t *t;
	presence_note_t *note;
	int res = 0;
	
	DEBUG_LOG("read_presentity()\n");
	if (cmp_node(root, "presence", pidf_ns) < 0) {
		ERROR_LOG("document is not presence \n");
		return -1;
	}

	entity = zt2str((char*)get_attr_value(find_attr(root->properties, "entity")));
	*dst = create_presentity_info(&entity);
	if (!(*dst)) return -1; /* memory */

	n = root->children;
	while (n) {
		if (n->type == XML_ELEMENT_NODE) {
			if (cmp_node(n, "tuple", pidf_ns) >= 0) {
				res = read_tuple(n, &t);
				if ((res == 0) && t) add_tuple_info(*dst, t);
				else break;
			}
			if (cmp_node(n, "note", pidf_ns) >= 0) {
				res = read_note(n, &note);
				if ((res == 0) && note) {
					DOUBLE_LINKED_LIST_ADD((*dst)->first_note, 
							(*dst)->last_note, note);
				}
				else break;
			}
		}
		n = n->next;
	}

	return res;
}

/* libxml2 must be initialized before calling this function ! */
int parse_pidf_document(presentity_info_t **dst, const char *data, int data_len)
{
	int res = 0;
	xmlDocPtr doc;
	
	if (!dst) return -1;
	if ((!data) || (data_len < 1)) return -2;

	*dst = NULL;
	doc = xmlReadMemory(data, data_len, NULL, NULL, xml_parser_flags);
	if (doc == NULL) {
		ERROR_LOG("can't parse document\n");
		return -1;
	}
	
	res = read_presentity(xmlDocGetRootElement(doc), dst);
	if (res != 0) {
		/* may be set => must be freed */
		if (*dst) free_presentity_info(*dst);
		*dst = NULL;
	}

	xmlFreeDoc(doc);
	return res;
}

