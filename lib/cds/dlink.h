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

#ifndef __DLINK_H
#define __DLINK_H

#include <stdio.h>

/** 
 * \defgroup dlink Universal double linked lists
 *
 */

/**
 * One element of dynamic linked list. Each element 
 * can carry an arbitrary data element, which is allocated 
 * together with extra data needed for linking 
 * (pointers to next and previous elements).
 */
typedef struct dlink_element {
	struct dlink_element *next;
	struct dlink_element *prev;
	
	int data_length;
	/** data array of unspecified size */
	char data[1];
} dlink_element_t;

dlink_element_t *dlink_element_alloc(int _data_length);
void dlink_element_free(dlink_element_t *e);
/* dlink_element_t *dlink_element_alloc_pkg(int _data_length);
void dlink_element_free_pkg(dlink_element_t *e); */
char* dlink_element_data(dlink_element_t *e);

/**
 * Structure carying information about linked list.
 */
typedef struct dlink {
	/** Pointer to the first element of this list. */
	dlink_element_t *first;
	
	/** Pointer to the last element of this list due 
	 * to more effective adding to the end of the list */	
	dlink_element_t *last;

	/* TODO: add some members for monitoring, ...
	 * add hash map?
	 */
} dlink_t;

/**
 * Initializes dlink structure - clears pointers to the
 * start and end of the link and so on.
 */
void dlink_init(dlink_t *l);

/** destroys all elements in shared memory */
void dlink_destroy(dlink_t *l);

/* destroys all elements in pkg memory */
/* void dlink_destroy_pkg(dlink_t *l); */

/** Adds one (only one!) element e to the end of the link l. */
void dlink_add(dlink_t *l, dlink_element_t *e);

/** Removes an element e from the link. */
void dlink_remove(dlink_t *l, dlink_element_t *e);

/** Thiss method initiates walking through the list.
 * It returns a pointer to the first element of the link. */
dlink_element_t *dlink_start_walk(dlink_t *l);

dlink_element_t *dlink_last_element(dlink_t *l);

dlink_element_t *dlink_next_element(dlink_element_t *e);
dlink_element_t *dlink_prev_element(dlink_element_t *e);

#endif
