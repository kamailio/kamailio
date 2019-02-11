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

#include <presence/xpidf.h>
#include <cds/dstring.h>
#include <cds/memory.h>
#include <cds/logger.h>
#include <cds/list.h>
#include <presence/xml_utils.h>
#include <string.h>

/* ------------------------------ XPIDF document creation ------------------------------ */

static void doc_add_tuple_note(dstring_t *buf, presence_note_t *n)
{
	dstr_append_zt(buf, "\t\t\t<note>");
	dstr_append_str(buf, &n->value);	
	dstr_append_zt(buf, "</note>\r\n");
}

/*static void doc_add_note(dstring_t *buf, presentity_info_t *p, presence_note_t *n)
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
}*/

static void doc_add_tuple(dstring_t *buf, presentity_info_t *p, presence_tuple_info_t *t)
{
	presence_note_t *n;
	char tmp[32];
	
	dstr_append_zt(buf, "\t<atom id=\"");
	dstr_append_str(buf, &t->id);
	dstr_append_zt(buf, "\">\r\n");
	
	dstr_append_zt(buf, "\t\t<address uri=\"");
	dstr_append_str(buf, &t->contact);
	dstr_append_zt(buf, "\" priority=\"");
/*	dstr_append_zt(buf, ";user=ip\" priority=\"");*/
	sprintf(tmp, "%1.2f", t->priority);
	dstr_append_zt(buf, tmp);
	dstr_append_zt(buf, "\">\r\n");
	if (t->status.basic == presence_tuple_open) dstr_append_zt(buf, "\t\t\t<status status=\"open\"/>\r\n");
	else dstr_append_zt(buf, "\t\t\t<status status=\"closed\"/>\r\n");
	dstr_append_zt(buf, "\t\t</address>\r\n");

	/* add tuple notes */
	n = t->first_note;
	while (n) {
		doc_add_tuple_note(buf, n);
		n = n->next;
	}
	/* --- */
	
	dstr_append_zt(buf, "\t</atom>\r\n");
}

static void doc_add_empty_tuple(dstring_t *buf, presentity_info_t *p)
{
	dstr_append_zt(buf, "\t<atom id=\"none\">\r\n");
	
	dstr_append_zt(buf, "\t\t<address uri=\"");
	dstr_append_str(buf, &p->uri);
	dstr_append_zt(buf, "\" priority=\"1\">\r\n");
/*	dstr_append_zt(buf, ";user=ip\" priority=\"1\">\r\n");*/
	dstr_append_zt(buf, "\t\t\t<status status=\"closed\"/>\r\n");
	dstr_append_zt(buf, "\t\t</address>\r\n");

	dstr_append_zt(buf, "\t</atom>\r\n");
}

static void doc_add_presentity(dstring_t *buf, presentity_info_t *p)
{
	presence_tuple_info_t *t;
	/* presence_note_t *n; */

	dstr_append_zt(buf, "<presence>\r\n");
	/* !!! there SHOULD be pres URI of presentity !!! */
	dstr_append_zt(buf, "<presentity uri=\"");
	/* dstr_put_pres_uri(buf, &p->presentity); */
	dstr_append_str(buf, &p->uri);
	dstr_append_zt(buf, ";method=SUBSCRIBE\"/>\r\n");
	
	t = p->first_tuple;
	if (!t) doc_add_empty_tuple(buf, p);
	while (t) {
		doc_add_tuple(buf, p, t);
		t = t->next;
	}
	
/*	
	n = p->first_note;
	while (n) {
		doc_add_note(buf, p, n);
		n = n->next;
	}
*/
	dstr_append_zt(buf, "</presence>\r\n");
}

int create_xpidf_document(presentity_info_t *p, str_t *dst, str_t *dst_content_type)
{
	dstring_t buf;
	int err = 0;
	
	if (!dst) return -1;
	
	str_clear(dst);
	if (dst_content_type) str_clear(dst_content_type);

	if (!p) return -1;
	
	if (dst_content_type) 
		if (str_dup_zt(dst_content_type, "application/xpidf+xml;charset=\"UTF-8\"") < 0) {
			return -1;
		}

/*	if (!p->first_tuple) return 0;*/	/* no tuples => nothing to say */ 
	
	dstr_init(&buf, 2048);
	
	dstr_append_zt(&buf, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n");
	dstr_append_zt(&buf, "<!DOCTYPE presence PUBLIC \"-//IETF//DTD RFCxxxx XPIDF 1.0//EN\" \"xpidf.dtd\">\r\n");
	doc_add_presentity(&buf, p);
	
	err = dstr_get_str(&buf, dst);
	dstr_destroy(&buf);

	if (err != 0) {
		str_free_content(dst);
		if (dst_content_type) str_free_content(dst_content_type);
	}
	
	return err;
}

