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

#ifndef __CDS_MEMORY_H
#define __CDS_MEMORY_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void*(*cds_malloc_func)(unsigned int size);
typedef void(*cds_free_func)(void *ptr);

extern cds_malloc_func cds_malloc;
extern cds_free_func cds_free;

void cds_set_memory_functions(cds_malloc_func _malloc, cds_free_func _free);

#ifdef __cplusplus
}
#endif


#endif

