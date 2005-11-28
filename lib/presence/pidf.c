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

static void doc_add_tuple(dstring_t *buf, presentity_info_t *p, presence_tuple_info_t *t)
{
	DEBUG_LOG("doc_add_tuple()\n");
	
	dstr_append_zt(buf, "\t<tuple id=\"");
	dstr_append_str(buf, &t->contact);
	dstr_append_zt(buf, "\">\r\n");
	
	if (t->status == presence_tuple_open) dstr_append_zt(buf, "\t\t<status><basic>open</basic></status>\r\n");
	else dstr_append_zt(buf, "\t\t<status><basic>closed</basic></status>\r\n");
	
	dstr_append_zt(buf, "\t\t<contact>");
	dstr_append_str(buf, &t->contact);
	dstr_append_zt(buf, "</contact>\r\n");

	dstr_append_zt(buf, "\t</tuple>\r\n");
}

static void doc_add_presentity(dstring_t *buf, presentity_info_t *p)
{
	presence_tuple_info_t *t;

	DEBUG_LOG("doc_add_presentity()\r\n");
	dstr_append_zt(buf, "<presence xmlns=\"urn:ietf:params:xml:ns:pidf\" entity=\"");
	dstr_append_str(buf, &p->presentity);
	dstr_append_zt(buf, "\">\r\n");
	
	DEBUG_LOG("doc_add_presentity(): adding tuples\r\n");
	t = p->first_tuple;
	while (t) {
		doc_add_tuple(buf, p, t);
		t = t->next;
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

