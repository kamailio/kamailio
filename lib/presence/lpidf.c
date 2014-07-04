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

#include <presence/lpidf.h>
#include <cds/dstring.h>
#include <cds/memory.h>
#include <cds/logger.h>
#include <cds/list.h>
#include <presence/xml_utils.h>
#include <string.h>

/* ------------------------------ LPIDF document creation ------------------------------ */

static void doc_add_tuple(dstring_t *buf, presentity_info_t *p, presence_tuple_info_t *t)
{
	char tmp[64];
	
	if (t->status.basic == presence_tuple_closed) return; /* do not append closed tuples */
	
	dstr_append_zt(buf, "Contact: ");
	dstr_append_str(buf, &t->contact);
	dstr_append_zt(buf, ";q=");
	sprintf(tmp, "%1.2f", t->priority);
	dstr_append_zt(buf, tmp);
	dstr_append_zt(buf, "\r\n");
}

static void doc_add_presentity(dstring_t *buf, presentity_info_t *p)
{
	presence_tuple_info_t *t;
	/* presence_note_t *n; */

	dstr_append_zt(buf, "To: ");
	dstr_append_str(buf, &p->uri);
	dstr_append_zt(buf, "\r\n");
	
	t = p->first_tuple;
	while (t) {
		doc_add_tuple(buf, p, t);
		t = t->next;
	}
}

int create_lpidf_document(presentity_info_t *p, str_t *dst, str_t *dst_content_type)
{
	dstring_t buf;
	int err;
	
	if (!dst) return -1;
	
	str_clear(dst);
	if (dst_content_type) str_clear(dst_content_type);

	if (!p) return -1;
	
	if (dst_content_type) {
		if (str_dup_zt(dst_content_type, "text/lpidf") < 0) {
			return -1;
		}
	}

/*	if (!p->first_tuple) return 0;*/	/* no tuples => nothing to say */ 
	
	dstr_init(&buf, 2048);
	
	doc_add_presentity(&buf, p);
	
	err = dstr_get_str(&buf, dst);
	dstr_destroy(&buf);
	
	if (err != 0) {
		str_free_content(dst);
		if (dst_content_type) str_free_content(dst_content_type);
	}
	
	return err;
}

