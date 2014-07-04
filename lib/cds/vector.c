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

#include <cds/memory.h>
#include <cds/vector.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int vector_add(vector_t *vector, void *element)
{
	if (vector->element_count >= vector->allocated_count) {
		void *new_data;
		int new_size = vector->allocated_count + vector->allocation_count;
		if (new_size <= vector->allocated_count) return -1;
	
		new_data = (void *)cds_malloc(vector->element_size * new_size);
		if (!new_data) return -1;
		if (vector->data) {
			memcpy(new_data, vector->data, 
					vector->element_size * vector->allocated_count);
			cds_free(vector->data);
		}
		vector->data = new_data;
		vector->allocated_count = new_size;
	}
	memcpy(vector->data + (vector->element_count * vector->element_size), 
			element, vector->element_size);
	vector->element_count++;
	
	return 0;
}

int vector_remove(vector_t *vector, int index)
{
	int cnt;
	if (index >= vector->element_count) return -1;
	
	cnt = vector->element_count - index - 1;
	if (cnt > 0) {
		memmove(vector->data + (index * vector->element_size), 
			vector->data + ((index + 1) * vector->element_size),
			cnt *  vector->element_size);
	}
	vector->element_count--;
	
	return 0;
}

int vector_get(vector_t *vector, int index, void *element_dst)
{
	if (index >= vector->element_count) return -1;
	
	memcpy(element_dst, vector->data + (index * vector->element_size), vector->element_size);

	return 0;
}

void* vector_get_ptr(vector_t *vector, int index)
{
	if (index >= vector->element_count) return NULL;
	else return vector->data + (index * vector->element_size);
}

void vector_destroy(vector_t *vector)
{
	if (vector) {
		if (vector->data) cds_free(vector->data);
		vector->data = NULL;
		vector->allocation_count = 0;
		vector->element_count = 0;
	}
}

int vector_init(vector_t *vector, int element_size, int allocation_count)
{
	if (!vector) return -1;
	vector->element_size = element_size;
	vector->element_count = 0;
	vector->allocation_count = allocation_count;
	vector->data = (void *)cds_malloc(element_size * allocation_count);
	if (!vector->data) {
		vector->allocated_count = 0;
		return -1;
	}
	else {
		vector->allocated_count = allocation_count;
		return 0;
	}
}

int vector_test(void)
{
	/*TODO: do tests for vector_t - all "methods" must be called */
	return 0;
}
