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

#include <cds/serialize.h>
#include <cds/logger.h>
#include <stdio.h>

int init_input_sstream(sstream_t *ss, char *data_in, int data_len)
{
	if (!ss) return -1;
	
	ss->type = sstream_in;
	ss->in.len = data_len;
	ss->in.s = data_in;
	ss->in_pos = 0;
	return 0;
}

int init_output_sstream(sstream_t *ss, int out_buff_resize)
{
	if (!ss) return -1;
	
	ss->type = sstream_out;
	str_clear(&ss->in);
	ss->in_pos = 0;
	dstr_init(&ss->out, out_buff_resize);
	return 0;
}

int get_serialized_sstream(sstream_t *ss, str_t *dst)
{
	if (ss->type == sstream_out) return dstr_get_str(&ss->out, dst);
	else return -1; /* no output for input stream */
}

int get_serialized_sstream_data(sstream_t *ss, char *dst)
{
	if (ss->type == sstream_out) return dstr_get_data(&ss->out, dst);
	else return -1; /* no output for input stream */
}

int get_serialized_sstream_len(sstream_t *ss)
{
	if (ss->type == sstream_out) return dstr_get_data_length(&ss->out);
	else return 0; /* no output for input stream */
}

int sstream_get(sstream_t *ss, char *c)
{
	/* if (!is_input_sstream(ss)) return -1;  */ /* optimalization */
	/* if (!c) return -1; */ /* dangerous optimalization */
	if (ss->in_pos < ss->in.len) {
		*c = ss->in.s[ss->in_pos++];
		return 0;
	}
	else return 1;
}

int sstream_put_str(sstream_t *ss, str_t *s)
{
	/* if (is_input_sstream(ss)) return -1;  */ /* dangerous optimalization */
	return dstr_append_str(&ss->out, s);
}

/* returns a part of string of given length - it is NOT a copy !!! */
int sstream_get_str_ex(sstream_t *ss, int len, str_t *dst)
{
	int l;
	int res = 0;
	
	if (!is_input_sstream(ss)) return -1;
	if (!dst) return -1;

	if (len == 0) {
		str_clear(dst);
		return 0;
	}
	
	l = ss->in.len - ss->in_pos;
	dst->s = ss->in.s + ss->in_pos;

	if (len > l) {
		dst->len = l;
		res = 1; /* not whole requested string is returned ! */
	}
	else dst->len = len;
	ss->in_pos += dst->len;
	
	return 0;
}

/* returns a copy of string from input buffer */
int sstream_get_str(sstream_t *ss, int len, str_t *dst)
{
	str_t tmp;
	int res = sstream_get_str_ex(ss, len, &tmp);
	if (res >= 0) {
		res = str_dup(dst, &tmp);
		if (res != 0) str_clear(dst);
	}
	return res;
}

int sstream_put_zt(sstream_t *ss, const char *s)
{
	/* if (!is_input_sstream(ss)) return -1;  */ /* dangerous optimalization */
	return dstr_append_zt(&ss->out, s);
}

int sstream_put(sstream_t *ss, const char *s, int len)
{
	/* if (!is_input_sstream(ss)) return -1;  */ /* dangerous optimalization */
	return dstr_append(&ss->out, s, len);
}

void destroy_sstream(sstream_t *ss)
{
	if (ss->type == sstream_out) dstr_destroy(&ss->out);
}

/*****************************************************************/

int serialize_int(sstream_t *ss, int *num)
{
	char sep = ':';
	
	if (!num) return -1;
	
	if (is_input_sstream(ss)) {
		char c;
		int first = 1;
		int sign = 1; /* positive */
		
		*num = 0;
		while (sstream_get(ss, &c) == 0) {
			if (c == sep) break;
			if ((c >= '0') && (c <= '9')) *num = 10 * (*num) + (c - '0');
			else {
				switch (c) {
					case '-': 
						if (first) sign = -1;
						else return -1;
					case '+': 
						if (!first) return -1;
					default: 
						return -1; /* unknown character */
				}
			}
			first = 0;
		}
		*num = sign * (*num);
	}
	else {
		char tmp[32];

		sprintf(tmp, "%d%c", *num, sep);
		sstream_put_zt(ss, tmp);
	}
	return 0;
}

int serialize_uint(sstream_t *ss, unsigned int *num)
{
	char sep = ':';
	
	if (!num) return -1;
	
	if (is_input_sstream(ss)) {
		char c;
		
		*num = 0;
		while (sstream_get(ss, &c) == 0) {
			if (c == sep) break;
			if ((c >= '0') && (c <= '9')) *num = 10 * (*num) + (c - '0');
			else return -1; /* unknown character */
		}
	}
	else {
		char tmp[32];

		sprintf(tmp, "%u%c", *num, sep);
		sstream_put_zt(ss, tmp);
	}
	return 0;
}

int serialize_str(sstream_t *ss, str_t *s)
{
	int res = 0;
	
	if (!s) return -1; 
	
	if (serialize_int(ss, &s->len) != 0) return -1;
	if (is_input_sstream(ss)) {
		if (s->len == 0) s->s = NULL;
		else res = sstream_get_str(ss, s->len, s); /* duplicates read string */
	}
	else res = sstream_put(ss, s->s, s->len);

	return res;
}

int serialize_str_ex(sstream_t *ss, str_t *s)
{
	int res = 0;
	
	if (!s) return -1; 
	
	if (serialize_int(ss, &s->len) != 0) return -1;
	if (is_input_sstream(ss)) {
		if (s->len == 0) s->s = NULL;
		else res = sstream_get_str_ex(ss, s->len, s); /* doesn't duplicate read string */
	}
	else res = sstream_put(ss, s->s, s->len);

	return res;
}

int serialize_char(sstream_t *ss, char *c)
{
	if (!c) return -1;
	if (is_input_sstream(ss)) return sstream_get(ss, c);
	else return sstream_put(ss, c, 1);
}

int serialize_uchar(sstream_t *ss, unsigned char *c)
{
	if (!c) return -1;
	if (is_input_sstream(ss)) return sstream_get(ss, (char *)c);
	else return sstream_put(ss, (char *)c, 1);
}
