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

#ifndef __DSTRING_H
#define __DSTRING_H

#include <cds/sstr.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _dstr_buff_t {
	int len;
	int used;
	struct _dstr_buff_t *next; /* pointer to next buffer */
	char data[1];
} dstr_buff_t;

/** Dynamic string structure. It is used
 * for muliple appends of any strings. 
 *
 * There was an attempt to add flags for SHM/PKG memory using, ...
 * but it shows that it slows down, thus they were removed and only the
 * "most quick" version is used (rather two functions than one with param) */
typedef struct _dstring_t {
	dstr_buff_t *first, *last;
	/** the length of whole string */
	int len;
	int buff_size;

	/** a operation on this string was unsuccesfull -> all other will produce error */
	int error;
} dstring_t;

int dstr_append_zt(dstring_t *dstr, const char *s);
int dstr_append(dstring_t *dstr, const char *s, int len);
int dstr_append_str(dstring_t *dstr, const str_t *s);
/* int dstr_get_data_length(dstring_t *dstr); */
int dstr_get_data(dstring_t *dstr, char *dst);
int dstr_get_str(dstring_t *dstr, str_t *dst);
int dstr_get_str_pkg(dstring_t *dstr, str_t *dst);
int dstr_init(dstring_t *dstr, int buff_size);
int dstr_destroy(dstring_t *dstr);

/* returns nozero if error !!! */
/* int dstr_error(dstring_t *dstr);
void dstr_clear_error(dstring_t *dstr); */

#define dstr_get_data_length(dstr) (dstr)->len
#define dstr_error(dstr) (dstr)->error
#define dstr_clear_error(dstr) (dstr)->error = 0

#ifdef __cplusplus
}
#endif
	
#endif
