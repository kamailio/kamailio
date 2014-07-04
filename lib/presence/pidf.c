/* 
 * PIDF parser
 *
 * $Id$
 * 
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

#include <presence/pidf.h>
#include <cds/dstring.h>
#include <cds/memory.h>
#include <cds/logger.h>
#include <cds/list.h>
#include <presence/xml_utils.h>
#include <string.h>

/* ------------------------------ PIDF document creation ------------------------------ */

static void doc_add_tuple_note(dstring_t *buf, presence_note_t *n)
{
	DEBUG_LOG("doc_add_tuple_note()\n");
	
	dstr_append_zt(buf, "\t\t<note");
	if (n->lang.len > 0) {
		dstr_append_zt(buf, " lang=\"");
		dstr_append_str(buf, &n->lang);
		dstr_append_zt(buf, "\"");
	}
	dstr_append_zt(buf, ">");
	dstr_append_str(buf, &n->value);	
	dstr_append_zt(buf, "</note>\r\n");
}

static inline void doc_add_extension(dstring_t *buf, extension_element_t *ex)
{
	dstr_append_str(buf, &ex->element);
	dstr_append_zt(buf, "\r\n");
}

static void doc_add_tuple(dstring_t *buf, presentity_info_t *p, presence_tuple_info_t *t)
{
	presence_note_t *n;
	extension_element_t *e;
	char tmp[32];
	
	DEBUG_LOG("doc_add_tuple()\n");
	
	dstr_append_zt(buf, "\t<tuple id=\"");
	dstr_append_str(buf, &t->id);
	dstr_append_zt(buf, "\">\r\n");
	
	dstr_append_zt(buf, "\t\t<status>\r\n");
	if (t->status.basic != presence_tuple_undefined_status) {
		/* do not add unknown status it is not mandatory in PIDF */
		dstr_append_zt(buf, "\t\t\t<basic>");
		dstr_append_str(buf, tuple_status2str(t->status.basic));
		dstr_append_zt(buf, "</basic>\r\n");
	}
	/* add extension status elements */
	e = t->status.first_unknown_element;
	while (e) {
		doc_add_extension(buf, e);
		e = e->next;
	}
	dstr_append_zt(buf, "\t\t</status>\r\n");
	
	/* add extension elements */
	e = t->first_unknown_element;
	while (e) {
		doc_add_extension(buf, e);
		e = e->next;
	}

	if (!is_str_empty(&t->contact)) {
		dstr_append_zt(buf, "\t\t<contact priority=\"");
		sprintf(tmp, "%1.2f", t->priority);
		dstr_append_zt(buf, tmp);
		dstr_append_zt(buf, "\">");
		dstr_append_str(buf, &t->contact);
		dstr_append_zt(buf, "</contact>\r\n");
	}

	n = t->first_note;
	while (n) {
		doc_add_tuple_note(buf, n);
		n = n->next;
	}
	
	dstr_append_zt(buf, "\t</tuple>\r\n");
}

static void doc_add_empty_tuple(dstring_t *buf)
{
	/* "empty" tuple is needed in PIDF by Microsoft Windows Messenger v. 5.1 and linphone 1.2) */
	DEBUG_LOG("doc_add_empty_tuple()\n");
	
	dstr_append_zt(buf, "\t<tuple id=\"none\">\r\n");
	dstr_append_zt(buf, "\t\t<status><basic>closed</basic></status>\r\n");

	dstr_append_zt(buf, "\t</tuple>\r\n");
}

static void doc_add_note(dstring_t *buf, presentity_info_t *p, presence_note_t *n)
{
	DEBUG_LOG("doc_add_note()\n");
	
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

static void dstr_put_pres_uri(dstring_t *buf, str_t *uri)
{
	char *c;
	int len = 0;
	
	if (!uri) return;
	
	c = str_strchr(uri, ':');
	if (c) {
		len = uri->len - (c - uri->s) - 1;
		if (len > 0) c++;
	}
	else {
		c = uri->s;
		len = uri->len;
	}
	if (len > 0) {
		dstr_append_zt(buf, "pres:");
		dstr_append(buf, c, len);
	}
}

static void doc_add_presentity(dstring_t *buf, presentity_info_t *p, int use_cpim_pidf_ns)
{
	presence_tuple_info_t *t;
	presence_note_t *n;
	extension_element_t *e;

	DEBUG_LOG("doc_add_presentity()\n");
	if (use_cpim_pidf_ns)
		dstr_append_zt(buf, "<presence xmlns=\"urn:ietf:params:xml:ns:cpim-pidf\" entity=\"");
	else 
		dstr_append_zt(buf, "<presence xmlns=\"urn:ietf:params:xml:ns:pidf\" entity=\"");
	/* !!! there SHOULD be pres URI of presentity !!! */
	dstr_put_pres_uri(buf, &p->uri);
	/* dstr_append_str(buf, &p->presentity); */ /* only for test !!! */
	dstr_append_zt(buf, "\">\r\n");
	
	DEBUG_LOG("adding tuples\n");
	t = p->first_tuple;
	if (!t) doc_add_empty_tuple(buf); /* correction for some strange clients :-) */
	while (t) {
		doc_add_tuple(buf, p, t);
		t = t->next;
	}
	
	DEBUG_LOG("adding notes\n");
	n = p->first_note;
	while (n) {
		doc_add_note(buf, p, n);
		n = n->next;
	}
	
	/* add extension elements */
	DEBUG_LOG("adding extension elements\n");
	e = p->first_unknown_element;
	while (e) {
		doc_add_extension(buf, e);
		e = e->next;
	}

	dstr_append_zt(buf, "</presence>\r\n");
}

int create_pidf_document_ex(presentity_info_t *p, str_t *dst, str_t *dst_content_type, int use_cpim_pidf_ns)
{
	dstring_t buf;
	int err;
	
	if (!dst) return -1;
	
	str_clear(dst);
	if (dst_content_type) str_clear(dst_content_type);

	if (!p) return -1;
	
	if (dst_content_type) {
		if (use_cpim_pidf_ns)
			err = str_dup_zt(dst_content_type, "application/cpim-pidf+xml");
		else
			err = str_dup_zt(dst_content_type, "application/pidf+xml;charset=\"UTF-8\"");
		if (err < 0) return -1;
	}
	
/*	if (!p->first_tuple) return 0;*/	/* no tuples => nothing to say */ 
	
	dstr_init(&buf, 2048);
	
	dstr_append_zt(&buf, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n");
	doc_add_presentity(&buf, p, use_cpim_pidf_ns);
	
	err = dstr_get_str(&buf, dst);
	dstr_destroy(&buf);
	
	if (err != 0) {
		str_free_content(dst);
		if (dst_content_type) str_free_content(dst_content_type);
	}
	
	return err;
}

int create_pidf_document(presentity_info_t *p, str_t *dst, str_t *dst_content_type)
{
	return create_pidf_document_ex(p, dst, dst_content_type, 0);
}

/* ------------------------------ PIDF document parsing ------------------------------ */

static char *pidf_ns = "urn:ietf:params:xml:ns:pidf";
/* static char *rpid_ns = "urn:ietf:params:xml:ns:pidf:rpid"; */
/* static char *data_model_ns = "urn:ietf:params:xml:ns:pidf:data-model"; */

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

static int get_whole_node_content(xmlNode *n, str_t *dst, xmlDocPtr doc)
{
	int res = 0;

	str_clear(dst);
	if (n) {
		n = xmlCopyNode(n, 1); /* this inserts namespaces into element correctly */
		if (!n) {
			ERROR_LOG("can't duplicate XML node\n");
			return -1;
		}
	}
	if (n) {
		xmlBufferPtr buf;
		buf = xmlBufferCreate();
		if (buf == NULL) {
			ERROR_LOG("Error creating the xml buffer\n");
			return -1;
		}
		if (xmlNodeDump(buf, doc, n, 0, 0) < 0) res = -1;
		if ((res == 0) && (buf->use > 0)) {
			str_t s;
			s.s = (char *)buf->content;
			s.len = buf->use;
			res = str_dup(dst, &s);
		}
		xmlBufferFree(buf);
		xmlFreeNode(n); /* was duplicated due to namespaces! */
	}
	return res;
}

static int read_extension(xmlNode *ex, extension_element_t **dst, xmlDocPtr doc)
{
	extension_element_t *e;
	/* xmlNode *n; */

	if (!dst) return -1;
	*dst = NULL;
	
	e = (extension_element_t*)cds_malloc(sizeof(extension_element_t));
	if (!e) return -1;
	
	memset(e, 0, sizeof(*e));
	*dst = e;

	/* do not care about internals - take whole element ! */
	if (get_whole_node_content(ex, &e->element, doc) != 0) {
		cds_free(e);
		*dst = NULL;
		return -1;
	}
	
	return 0;
}

static int read_tuple(xmlNode *tuple, presence_tuple_info_t **dst, int ignore_ns, xmlDocPtr doc)
{
	str_t contact, id;
	basic_tuple_status_t status;
	xmlNode *n, *status_node;
	double priority = 0;
	const char *s;
	int res = 0;
	presence_note_t *note;
	char *ns = ignore_ns ? NULL: pidf_ns;
	extension_element_t *ex;

	*dst = NULL;

	DEBUG_LOG("read_tuple()\n");
	/* process contact (only one node) */
	n = find_node(tuple, "contact", ns);
	if (!n) {
		/* ERROR_LOG("contact not found\n"); */
		str_clear(&contact);
		/* return -1; */
	}
	else {
		s = get_attr_value(find_attr(n->properties, "priority"));
		if (s) priority = atof(s);
		s = get_node_value(n);
		contact.s = (char *)s;
		if (s) contact.len = strlen(s);
		else contact.len = 0;
		if (contact.len < 1) {
			ERROR_LOG("empty contact using default\n");
			/* return -1; */
		}	
	}
	
	/* process status (only one node) */
	status_node = find_node(tuple, "status", ns);
	if (!status_node) {
		ERROR_LOG("status not found\n");
		return -1;
	}
	n = find_node(status_node, "basic", ns);
	if (!n) {
		ERROR_LOG("basic status not found - using \'closed\'\n");
		/* return -1; */
		s = "closed";
	}
	else s = get_node_value(n);
	if (!s) {
		ERROR_LOG("basic status without value\n");
		return -1;
	}

	/* translate status */
	status = presence_tuple_closed; /* default value */
	if (strcasecmp(s, "open") == 0) status = presence_tuple_open;
	if (strcasecmp(s, "closed") == 0) status = presence_tuple_closed;
	/* FIXME: handle not standardized variants too (add note to basic status) */
	
	/* get ID from tuple node attribute? */
	id.s = (char *)get_attr_value(find_attr(tuple->properties, "id"));
	if (id.s) id.len = strlen(id.s);
	else id.len = 0;
	
	*dst = create_tuple_info(&contact, &id, status);
	if (!(*dst)) return -1;

	(*dst)->priority = priority;

	/* handle nested elements */
	n = tuple->children;
	while (n) {
		if (n->type == XML_ELEMENT_NODE) {
			if (cmp_node(n, "note", ns) >= 0) {
				res = read_note(n, &note);
				if ((res == 0) && note) {
					DOUBLE_LINKED_LIST_ADD((*dst)->first_note, 
							(*dst)->last_note, note);
				}
				else break;
			}
			else if (cmp_node(n, "contact", ns) >= 0) {
				/* skip, already processed */
			}
			else if (cmp_node(n, "status", ns) >= 0) {
				/* skip, already processed */
			}	
			else if (cmp_node(n, "timestamp", ns) >= 0) {
				/* FIXME: process */
			}
			else { /* PIDF extensions - only from non-PIDF namespace? */
				res = read_extension(n, &ex, doc);
				if ((res == 0) && ex) 
					DOUBLE_LINKED_LIST_ADD((*dst)->first_unknown_element, 
							(*dst)->last_unknown_element, ex);
			}
		}
		n = n->next;
	}
	
	/* handle nested elements in status */
	if (status_node) n = status_node->children;
	else n = NULL;
	while (n) {
		if (n->type == XML_ELEMENT_NODE) {
			if (cmp_node(n, "basic", ns) >= 0) {
				/* skip, already processed */
			}
			else { /* PIDF extensions - only from non-PIDF namespace? */
				res = read_extension(n, &ex, doc);
				if ((res == 0) && ex) 
					DOUBLE_LINKED_LIST_ADD((*dst)->status.first_unknown_element, 
							(*dst)->status.last_unknown_element, ex);
			}
		}
		n = n->next;
	}

	return res;
}

static int read_presentity(xmlNode *root, presentity_info_t **dst, int ignore_ns, xmlDocPtr doc)
{
	xmlNode *n;
	str_t entity;
	presence_tuple_info_t *t;
	presence_note_t *note;
	int res = 0;
	char *ns = ignore_ns ? NULL: pidf_ns;
	extension_element_t *ex;
	
	/* TRACE_LOG("read_presentity(ns=%s)\n", ns ? ns : ""); */
	if (cmp_node(root, "presence", ns) < 0) {
		ERROR_LOG("document is not presence \n");
		return -1;
	}

	entity = zt2str((char*)get_attr_value(find_attr(root->properties, "entity")));
	*dst = create_presentity_info(&entity);
	if (!(*dst)) return -1; /* memory */

	n = root->children;
	while (n) {
		if (n->type == XML_ELEMENT_NODE) {
			if (cmp_node(n, "tuple", ns) >= 0) {
				res = read_tuple(n, &t, ignore_ns, doc);
				if ((res == 0) && t) add_tuple_info(*dst, t);
				else break;
			}
			else if (cmp_node(n, "note", ns) >= 0) {
					res = read_note(n, &note);
					if ((res == 0) && note) {
						DOUBLE_LINKED_LIST_ADD((*dst)->first_note, 
								(*dst)->last_note, note);
					}
					else break;
				}
			else { /* PIDF extensions - only from non-PIDF namespace? */
				res = read_extension(n, &ex, doc);
				if ((res == 0) && ex) 
					DOUBLE_LINKED_LIST_ADD((*dst)->first_unknown_element, 
							(*dst)->last_unknown_element, ex);
				/*if (res != 0) break; ignore errors there */
			}
			
		}
		n = n->next;
	}

	return res;
}

/* ignore ns added for cpim-pidf+xml, draft version 07 (differs only in ns) */
int parse_pidf_document_ex(presentity_info_t **dst, const char *data, int data_len, int ignore_ns)
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

	res = read_presentity(xmlDocGetRootElement(doc), dst, ignore_ns, doc);
	if (res != 0) {
		/* may be set => must be freed */
		if (*dst) free_presentity_info(*dst);
		*dst = NULL;
	}

	xmlFreeDoc(doc);
	return res;
}

/* libxml2 must be initialized before calling this function ! */
int parse_pidf_document(presentity_info_t **dst, const char *data, int data_len)
{
	return parse_pidf_document_ex(dst, data, data_len, 0);
}

/* --------------- CPIM_PIDF document creation/parsing ---------------- */

int parse_cpim_pidf_document(presentity_info_t **dst, const char *data, int data_len)
{
	return parse_pidf_document_ex(dst, data, data_len, 1);
}

int create_cpim_pidf_document(presentity_info_t *p, str_t *dst, str_t *dst_content_type)
{
	return create_pidf_document_ex(p, dst, dst_content_type, 1);
}

