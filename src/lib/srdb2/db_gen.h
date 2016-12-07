/* 
 * Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2006-2007 iptelorg GmbH
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

#ifndef _DB_GEN_H
#define _DB_GEN_H  1

/** \ingroup DB_API 
 * @{ 
 */

#include "db_drv.h"
#include "../../str.h"
#include "../../list.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * Declare a list of DB API structures with given structure
 * name
 */
#define DBLIST_HEAD(name) \
	STAILQ_HEAD(name, db_gen)

/*
 * All structures that ought to be members of the DB API
 * linked lists must have this element as the _first_
 * element in the structure
 */
#define DBLIST_ENTRY \
	STAILQ_ENTRY(db_gen) next


/*
 * Initialize the static head of linked lists of DB API
 * structures
 */
#define DBLIST_INITIALIZER(head) \
	STAILQ_HEAD_INITIALIZER(head)

/*
 * Initialize the head of the list
 */
#define DBLIST_INIT(head) \
	STAILQ_INIT(head)

#define	DBLIST_FIRST(head) SLIST_FIRST(head)

/*
 * Insert a new DB API structure at the beginning of the
 * linked list
 */
#define DBLIST_INSERT_HEAD(head, elem) \
	STAILQ_INSERT_HEAD((head), (struct db_gen*)(elem), next)

/*
 * Add an element at the tail of the list
 */
#define DBLIST_INSERT_TAIL(head, elem) \
	STAILQ_INSERT_TAIL((head), ((struct db_gen*)(elem)), next)

/*
 * Remove a given structure from a linked list of DB API
 * structures
 */
#define DBLIST_REMOVE(head, elem) \
	STAILQ_REMOVE(head, (struct db_gen*)(elem), db_gen, next)

/*
 * Remove a given structure from a linked list of DB API
 * structures
 */
#define DBLIST_REMOVE_HEAD(head) \
	STAILQ_REMOVE_HEAD(head, next)

/*
 * Iterate through the elements of the list, store
 * the pointer to the current element in var variable
 */

/*
 * FIXME: We should find some other way of doing this than just copying
 * and pasting the code from STAILQ_FOREACH
 */
#define DBLIST_FOREACH(var, head)				 \
	for((var) = (void*)STAILQ_FIRST((head));	 \
		(var);								     \
		(var) = (void*)STAILQ_NEXT(((struct db_gen*)(var)), next))

/*
 * Iterate through the elements of the list, the pointer
 * to the current element is stored in var variable, this
 * is the safe version of the macro which allows you to
 * remove the element from the list. tvar is a temporary
 * variable for internal use by the macro
 */

/*
 * FIXME: We should find some other way of doing this than just copying
 * and pasting the code from STAILQ_FOREACH_SAFE
 */
#define DBLIST_FOREACH_SAFE(var, head, tvar)					 \
	for ((var) = (void*)STAILQ_FIRST((head));					 \
		 (var) && ((tvar) = (void*)STAILQ_NEXT(((struct db_gen*)(var)), next), 1); \
		 (var) = (tvar))

/*
 * Maximum number of payload structures that can be attached to
 * any DB API structure at a time.
 */
#define DB_PAYLOAD_MAX 16

struct db_drv;

/*
 * Template for generic data structures defined in the
 * DB API. Drivers can cast structure pointers to this to
 * obtain the pointer to driver specific data
 *
 * All variables and attributes to be shared across all DB API
 * structures should be put into this structure. This structure
 * is at the beginnning of each DB API structure to ensure that
 * all DB API structures share some common variables.
 */
typedef struct db_gen {
        DBLIST_ENTRY;

	/* Array of pointers to driver-specific data. The database API
	 * supports access to multiple databases at the same time and each
	 * database driver may want to append some data to generic DB structures,
	 * hence an array. The current position in the array is stored 
	 * in db_data_idx
	 */
	struct db_drv* data[DB_PAYLOAD_MAX];
} db_gen_t;

/*
 * Global variable holding the current index of the payload of the driver that
 * is being executed. DB API is responsible for setting this vaiable before 
 * calling functions of DB drivers.
 */
extern int db_payload_idx;


/*
 * Macros to set/get variable (DB driver specific)
 * payload to/from generic DB API structures
 */


/*
 * Attach a driver specific data structure to a generic 
 * DB API structure
 */
#define DB_SET_PAYLOAD(db_struct, drv_data) do { \
    ((struct db_gen*)(db_struct))->data[db_payload_idx] = (struct db_drv*)(drv_data); \
} while(0)


/*
 * Return a driver specific data structure
 */
#define DB_GET_PAYLOAD(db_struct) \
    ((void*)(((struct db_gen*)(db_struct))->data[db_payload_idx]))


/*
 * Initialize a db_gen structure and make space for the data
 * from n database drivers
 */
int db_gen_init(struct db_gen* gen);


/*
 * Free all memory allocated by a db_gen structure
 */
void db_gen_free(struct db_gen* gen);


#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* _DB_GEN_H */
