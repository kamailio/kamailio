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

#ifndef __SERIALIZE_H
#define __SERIALIZE_H

/* serialization/deserialization data structures and functions */

#include <cds/dstring.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	dstring_t out; /* output string */
	str_t in; /* input string */
	int in_pos; /* position in input */
	enum { sstream_in, sstream_out } type;
} sstream_t;

#define is_input_sstream(ss)	(ss->type == sstream_in)

int init_input_sstream(sstream_t *ss, char *data_in, int data_len);
int init_output_sstream(sstream_t *ss, int out_buff_resize);

/** returns serialized data as newly allocated string in shared memory */
int get_serialized_sstream(sstream_t *ss, str_t *dst);

/** returns the length of serialized data */
int get_serialized_sstream_len(sstream_t *ss);

/** copies serialized data into given buffer */
int get_serialized_sstream_data(sstream_t *ss, char *dst);

void destroy_sstream(sstream_t *ss);

int sstream_put(sstream_t *ss, const char *s, int len);
int sstream_put_str(sstream_t *ss, str_t *s);
int sstream_put_zt(sstream_t *ss, const char *s);

int sstream_get(sstream_t *ss, char *c);

/* returns a part of string of given length - it is NOT a copy !!! */
int sstream_get_str_ex(sstream_t *ss, int len, str_t *dst);

/* returns a copy of string from input buffer */
int sstream_get_str(sstream_t *ss, int len, str_t *dst);


int serialize_int(sstream_t *ss, int *num);
int serialize_uint(sstream_t *ss, unsigned int *num);
int serialize_char(sstream_t *ss, char *c);
int serialize_uchar(sstream_t *ss, unsigned char *c);
int serialize_str(sstream_t *ss, str_t *s);
int serialize_str_ex(sstream_t *ss, str_t *s); /* doesn't duplicate read strings */

#ifdef __cplusplus
}
#endif

#endif
