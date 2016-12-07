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

#include <cds/ptr_vector.h>
#include <stdio.h>

int ptr_vector_add(ptr_vector_t *vector, void *ptr)
{
	return vector_add(vector, &ptr);
}

void *ptr_vector_get(ptr_vector_t *vector, int index)
{
	void *ptr = NULL;
	if (vector_get(vector, index, &ptr) != 0) return NULL;
	return ptr;
}

int ptr_vector_remove(ptr_vector_t *vector, int index)
{
	return vector_remove(vector, index);
}

void ptr_vector_destroy(ptr_vector_t *vector)
{
	return vector_destroy(vector);
}

int ptr_vector_init(vector_t *vector, int allocation_count)
{
	return vector_init(vector, sizeof(void *), allocation_count);
}

