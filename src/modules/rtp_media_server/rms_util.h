/*
 * Copyright (C) 2017-2018 Julien Chavanton jchavanton@gmail.com
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef rms_util_h
#define rms_util_h

/**
 * \brief Make a copy of a str structure using shm_malloc/pkg_malloc
 * garanty to return a null terminated string, even if the source is not
 * \param dst destination
 * \param src source
 * \param shared use shared memory
 * \return 0 on success, -1 on failure
 */
static inline int rms_str_dup(str *dst, str *src, int shared)
{
	if(!dst) {
		LM_ERR("dst null\n");
		return 0;
	}
	dst->len = 0;
	dst->s = NULL;
	if(!src || !src->s || src->len < 0) {
		LM_ERR("src null or invalid\n");
		return 0;
	}
	if(src->len == 0)
		return 1;
	dst->len = src->len;
	if(shared) {
		dst->s = shm_malloc(dst->len + 1);
	} else {
		dst->s = pkg_malloc(dst->len + 1);
	}
	if(!dst->s) {
		LM_ERR("%s_malloc: can't allocate memory (%d bytes)\n",
				shared ? "shm" : "pkg", src->len);
		return 0;
	}
	memcpy(dst->s, src->s, src->len);
	dst->s[dst->len] = '\0';
	return 1;
}

static inline char *rms_char_dup(char *s, int shared)
{
	str src;
	str dst;
	src.s = s;
	src.len = strlen(s);
	if(!rms_str_dup(&dst, &src, shared))
		return NULL;
	return dst.s;
}

#endif
