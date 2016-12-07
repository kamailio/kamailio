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

#ifndef __DSTRING_H
#define __DSTRING_H

#include <cds/sstr.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup cds
 * @{ 
 *
 * \defgroup cds_dstring Dynamic strings
 *
 * Dynamic strings were introduced to satisfy needs of presence
 * modules when building presence documents.
 *
 * Dynamic string uses a list of buffers holding data. 
 * Buffers are allocated when needed - when there is not enough
 * space in the last buffer. The whole result can be copied into one
 * destination buffer with \ref dstr_get_data, \ref dstr_get_str
 * or \ref dstr_get_str_pkg function.
 *
 * \todo Function with sprintf syntax which will help with 
 * readibility of code using dynamic strings.
 * @{
 * */

/** Buffer used by dynamic string. 
 *
 * \todo 'len' and 'used' can be replaced by 'unused' member 
 * but it doesn't save too much */
typedef struct _dstr_buff_t {
	int len; /**< the buffer length */
	int used; /**< already used bytes from buffer */
	struct _dstr_buff_t *next; /**< pointer to next buffer in the list*/
	char data[1]; /** buffer data */
} dstr_buff_t;

/** Dynamic string structure. It is used
 * for muliple appends of any strings. 
 *
 * \note There was an attempt to add flags for SHM/PKG memory using, ...
 * but it shows that it slows down, thus they were removed and only the
 * "most quick" version is used (rather two functions than one with param) */
typedef struct _dstring_t {
	/** pointer to the first buffer in the list */
	dstr_buff_t *first;
	/** pointer to the last buffer in the list */
	dstr_buff_t *last;
	/** the length of whole string */
	int len;
	/** predefined buffer size */
	int buff_size;

	/** a operation on this string was unsuccesfull -> 
	 * all other operations will produce error */
	int error;
} dstring_t;

/** Appends zero terminated string to dynamic string.
 * \retval 0 if successful
 * \retval negative on error */
int dstr_append_zt(dstring_t *dstr, const char *s);

/** Appends string with given length to dynamic string.
 * \retval 0 if successful
 * \retval negative on error */
int dstr_append(dstring_t *dstr, const char *s, int len);

/** Appends string to dynamic string.
 * \retval 0 if successful
 * \retval negative on error */
int dstr_append_str(dstring_t *dstr, const str_t *s);

/* int dstr_get_data_length(dstring_t *dstr); */

/** Returns data stored in dynamic string. It does NOT allocate
 * space for them - it expects that the buffer is already allocated. 
 * \retval 0 if successful
 * \retval negative on error */
int dstr_get_data(dstring_t *dstr, char *dst);

/** Returns data stored in dynamic string. It allocates space for
 * them with cds_malloc (SER's shared memory). 
 * \retval 0 if successful
 * \retval negative on error */
int dstr_get_str(dstring_t *dstr, str_t *dst);

/** Returns data stored in dynamic string. It allocates space for
 * them with cds_malloc_pkg (SER's package memory).
 * \retval 0 if successful
 * \retval negative on error */
int dstr_get_str_pkg(dstring_t *dstr, str_t *dst);
/** Initializes dynamic string.
 * \param dstr dynamic string to be initialized
 * \param buff_size size of buffer used with this dynamic string 
 * \retval 0 if successful
 * \retval negative on error */
int dstr_init(dstring_t *dstr, int buff_size);

/** Destroys dynamic string. It frees all allocated buffers. */
int dstr_destroy(dstring_t *dstr);

/* returns nozero if error !!! */
/* int dstr_error(dstring_t *dstr);
void dstr_clear_error(dstring_t *dstr); */

/** Macro returning length of data stored in dynamic string. */
#define dstr_get_data_length(dstr) (dstr)->len

/** Macro pointing to error in dynamic string. If set
 * there was an error during a previous operation with
 * this dynamic string. */
#define dstr_error(dstr) (dstr)->error

/** Macro for cleaning error flag in dynamic string. */
#define dstr_clear_error(dstr) (dstr)->error = 0

/** @} 
 @} */

#ifdef __cplusplus
}
#endif
	
#endif
