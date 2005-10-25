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
#include <cds/dlink.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dstr_buff {
	int len;
	int used;
	char data[1];
} dstr_buff_t;

/** Dynamic string structure. It is used
 * for muliple appends of any strings. */
typedef struct dstring {
	dlink_t buffers;
	/** the length of whole string */
	int len;
	int buff_size;
} dstring_t;

int dstr_append_zt(dstring_t *dstr, const char *s);
int dstr_append(dstring_t *dstr, const char *s, int len);
int dstr_append_str(dstring_t *dstr, const str_t *s);
int dstr_get_data_length(dstring_t *dstr);
int dstr_get_data(dstring_t *dstr, char *dst);
int dstr_get_str(dstring_t *dstr, str_t *dst);
int dstr_init(dstring_t *dstr, int buff_size);
int dstr_destroy(dstring_t *dstr);

#ifdef __cplusplus
}
#endif
	
#endif
