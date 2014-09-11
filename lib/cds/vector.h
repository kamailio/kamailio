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

#ifndef __VECTOR_H
#define __VECTOR_H

#ifdef __cplusplus
extern "C" {
#endif

/* Structure representing dynamic array of elements of
 * equal size. */
typedef struct {
	int element_size;
	/** the number of used elements */
	int element_count;
	/** the number of allocated elements */
	int allocated_count;
	/** number of elements allocated together (better than allocation 
	 * for each element separately) */
	int allocation_count;
	void *data;
} vector_t;

int vector_add(vector_t *vector, void *element);
int vector_get(vector_t *vector, int index, void *element_dst);
void* vector_get_ptr(vector_t *vector, int index);
int vector_remove(vector_t *vector, int index);
void vector_destroy(vector_t *vector);
int vector_init(vector_t *vector, int element_size, int allocation_count);

/** testing function - returns 0 if no errors */
int vector_test(void);

#define vector_size(v) (v)->element_count

#ifdef __cplusplus
}
#endif

#endif
