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
#include <cds/dstring.h>
#include <cds/memory.h>
#include <cds/list.h>
#include <cds/logger.h>

#define get_current_buffer(dstr) (dstr)->last

static dstr_buff_t *add_new_buffer(dstring_t *dstr) 
{
	dstr_buff_t *buff = NULL;
	
	/* e = dlink_element_alloc_pkg(sizeof(dstr_buff_t) + dstr->buff_size); */
/*	if (dstr->flags & DSTR_PKG_MEM)
		buff = cds_malloc_pkg(sizeof(dstr_buff_t) + dstr->buff_size);
	else
		buff = cds_malloc(sizeof(dstr_buff_t) + dstr->buff_size);
		*/
/*	buff = cds_malloc(sizeof(dstr_buff_t) + dstr->buff_size);*/
	buff = cds_malloc_pkg(sizeof(dstr_buff_t) + dstr->buff_size);
	if (buff) {
		buff->len = dstr->buff_size;
		buff->used = 0;
		buff->next = NULL;
		LINKED_LIST_ADD(dstr->first, dstr->last, buff);
	}
	else dstr->error = 1;
	return buff;
}

int dstr_append(dstring_t *dstr, const char *s, int len)
{
	int size;
	dstr_buff_t *buff;

/*	if (!dstr) return -1; */
	if (dstr->error) return -2;

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
	if (!buff) {
		dstr->error = 1;
		return -1;
	}
	return 0;
}

int dstr_append_zt(dstring_t *dstr, const char *s)
{
/*	if (!dstr) return -1; */
	if (!s) return 0; /*append empty string*/
	return dstr_append(dstr, s, strlen(s));
}

int dstr_append_str(dstring_t *dstr, const str_t *s)
{
/*	if (!dstr) return -1; */
	if (!s) return 0; /*append empty string*/
	return dstr_append(dstr, s->s, s->len);
}

/* int dstr_get_data_length(dstring_t *dstr)
{
	if (!dstr) return 0;
	else return dstr->len;
} */

int dstr_get_data(dstring_t *dstr, char *dst)
{
	dstr_buff_t* buff;
	
	/* if (!dstr) return -1; */
	if (dstr->error) return -2; /* a previous operation returned error */
	
	buff = dstr->first;
	while (buff) {
		memcpy(dst, buff->data, buff->used);
		dst += buff->used;
		buff = buff->next;
	}
	return 0;
}

int dstr_get_str(dstring_t *dstr, str_t *dst)
{
	int res = 0;
	
	if (!dst) return -1;
	if (dstr->error) {
		dst->s = NULL;
		dst->len = 0;
		return -2; /* a previous operation returned error */
	}

	dst->len = dstr_get_data_length(dstr);
	if (dst->len > 0) {
		dst->s = (char*)cds_malloc(dst->len);
		if (!dst->s) {
			res = -1;
			dst->len = 0;
		}
		else res = dstr_get_data(dstr, dst->s);
	} 
	else {
		dst->s = NULL;
		dst->len = 0;
	}

	return res;
}

int dstr_get_str_pkg(dstring_t *dstr, str_t *dst)
{
	int res = 0;
	
	if (!dst) return -1;
	if (dstr->error) {
		dst->s = NULL;
		dst->len = 0;
		return -2; /* a previous operation returned error */
	}

	dst->len = dstr_get_data_length(dstr);
	if (dst->len > 0) {
		dst->s = (char*)cds_malloc_pkg(dst->len);
		if (!dst->s) {
			res = -1;
			dst->len = 0;
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
	/* if (!dstr) return -1; */
	dstr->buff_size = buff_size;
	dstr->len = 0;
	dstr->error = 0;
	dstr->first = 0;
	dstr->last = 0;
	return 0;
}

int dstr_destroy(dstring_t *dstr)
{
	dstr_buff_t *e,*n;
/*	if (!dstr) return -1; */
	/* dlink_destroy(&dstr->buffers); */
	e = dstr->first;
	while (e) {
		n = e->next;
/*		if (dstr->flags & DSTR_PKG_MEM) cds_free_pkg(e);
		else cds_free(e);*/
/*		cds_free(e);*/
		cds_free_pkg(e);
		e = n;
	}
	dstr->first = 0;
	dstr->last = 0;
	return 0;
}

/* int dstr_error(dstring_t *dstr)
{
	if (dstr) return dstr->error;
	else return -1;
}

void dstr_clear_error(dstring_t *dstr)
{
	if (dstr) dstr->error = 0;
}
*/
