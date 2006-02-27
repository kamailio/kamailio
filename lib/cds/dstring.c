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

#include <stdio.h>
#include <string.h>
#include <cds/dstring.h>
#include <cds/memory.h>

static dstr_buff_t *get_current_buffer(dstring_t *dstr) 
{
	dstr_buff_t *buff;
	buff = (dstr_buff_t*)dlink_element_data(dlink_last_element(&dstr->buffers));
	return buff;
}

static dstr_buff_t *add_new_buffer(dstring_t *dstr) 
{
	dstr_buff_t *buff = NULL;
	dlink_element_t *e;
	
	/* e = dlink_element_alloc_pkg(sizeof(dstr_buff_t) + dstr->buff_size); */
	e = dlink_element_alloc(sizeof(dstr_buff_t) + dstr->buff_size);
	if (e) {
		buff = (dstr_buff_t*)dlink_element_data(e);
		buff->len = dstr->buff_size;
		buff->used = 0;
		dlink_add(&dstr->buffers, e);
	}
	return buff;
}

int dstr_append(dstring_t *dstr, const char *s, int len)
{
	int size;
	dstr_buff_t *buff;
	
	if (len == 0) return 0; /*append empty string*/
	
	buff = get_current_buffer(dstr);
	if (!buff) buff = add_new_buffer(dstr);
	while ((len > 0) && (buff)) {
		size = buff->len - buff->used;
		if (size > len) size = len;
		memcpy(buff->data + buff->used, s, size);
		buff->used += size;
		len -= size;
		s += size;
		dstr->len += size;
		if (len > 0) buff = add_new_buffer(dstr);
	}
	if (!buff) return -1;
	return 0;
}

int dstr_append_zt(dstring_t *dstr, const char *s)
{
	if (!dstr) return -1;
	if (!s) return 0; /*append empty string*/
	return dstr_append(dstr, s, strlen(s));
}

int dstr_append_str(dstring_t *dstr, const str_t *s)
{
	if (!dstr) return -1;
	if (!s) return 0; /*append empty string*/
	return dstr_append(dstr, s->s, s->len);
}

int dstr_get_data_length(dstring_t *dstr)
{
	if (!dstr) return 0;
	else return dstr->len;
}

int dstr_get_data(dstring_t *dstr, char *dst)
{
	dlink_element_t *e;
	dstr_buff_t* buff;
	
	if (!dstr) return -1;
	e = dlink_start_walk(&dstr->buffers);
	while (e) {
		buff = (dstr_buff_t*)dlink_element_data(e);
		memcpy(dst, buff->data, buff->used);
		dst += buff->used;
		e = dlink_next_element(e);
	}
	return 0;
}

int dstr_get_str(dstring_t *dstr, str_t *dst)
{
	int res = 0;
	
	if (!dst) return -1;
	dst->len = dstr_get_data_length(dstr);
	if (dst->len > 0) {
		dst->s = (char*)cds_malloc(dst->len);
		if (!dst->s) {
			res = -1;
			dst->len = 0;
			dst->s = NULL;
		}
		else res = dstr_get_data(dstr, dst->s);
	} 
	else {
		dst->s = NULL;
		dst->len = 0;
	}

	return res;
}


int dstr_init(dstring_t *dstr, int buff_size)
{
	if (!dstr) return -1;
	dstr->buff_size = buff_size;
	dstr->len = 0;
	dlink_init(&dstr->buffers);
	return 0;
}

int dstr_destroy(dstring_t *dstr)
{
	dlink_element_t *e,*n;
	if (!dstr) return -1;
	/* dlink_destroy(&dstr->buffers); */
	e = dlink_start_walk(&dstr->buffers);
	while (e) {
		n = dlink_next_element(e);
		dlink_remove(&dstr->buffers, e);
		dlink_element_free(e);
		/* dlink_element_free_pkg(e); */
		e = n;
	}
	return 0;
}

