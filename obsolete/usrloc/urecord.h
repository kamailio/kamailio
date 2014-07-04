/* 
 * $Id$ 
 *
 * Usrloc record structure
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 *
 * History:
 * ---------
 * 2003-03-12 added replication mark support (nils)
 */


#ifndef URECORD_H
#define URECORD_H


#include <stdio.h>
#include <time.h>
#include "hslot.h"
#include "../../str.h"
#include "../../qvalue.h"
#include "ucontact.h"
#include "notify.h"


struct hslot;


/*
 * Basic hash table element
 */
typedef struct urecord {
	str* domain;                   /* Pointer to domain we belong to */
	str uid;                       /* User id */
	ucontact_t* contacts;          /* One or more contact fields */
	
	struct hslot* slot;            /* Collision slot in the hash table array we belong to */
	struct {
		struct urecord* prev;  /* Next item in the linked list */
		struct urecord* next;  /* Previous item in the linked list */
	} d_ll;
	struct {                         /* Linked list of all elements in hash table */
		struct urecord* prev;  /* Previous item in the list */
		struct urecord* next;  /* Next item in the list */
	} s_ll;

	struct notify_cb* watchers;         /* List of watchers */
} urecord_t;


/* Create a new record */
int new_urecord(str* _dom, str* _uid, urecord_t** _r);


/* Free all memory associated with the element */
void free_urecord(urecord_t* _r);


/*
 * Print an element, for debugging purposes only
 */
void print_urecord(FILE* _f, urecord_t* _r);


/*
 * Add a new contact
 */
int mem_insert_ucontact(urecord_t* _r, str* aor, str* _c, time_t _e, qvalue_t _q, str* _cid, int _cs, 
			unsigned int _flags, struct ucontact** _con, str *_ua, str* _recv,
						struct socket_info* sock, str* _inst, int sid);



/*
 * Remove the contact from lists
 */
void mem_remove_ucontact(urecord_t* _r, ucontact_t* _c);


/*
 * Remove contact from the list and delete 
 */
void mem_delete_ucontact(urecord_t* _r, ucontact_t* _c);


/*
 * Timer handler
 */
int timer_urecord(urecord_t* _r);


/* ===== Module interface ======== */


/*
 * Release urecord previously obtained
 * through get_urecord
 */
typedef void (*release_urecord_t)(urecord_t* _r);
void release_urecord(urecord_t* _r);


/*
 * Create and insert new contact
 * into urecord with additional replication argument
 */
typedef int (*insert_ucontact_t)(urecord_t* _r, str* aor, str* _c, time_t _e, qvalue_t _q, str* _cid, int _cs, 
								 unsigned int _flags, struct ucontact** _con, str *_ua, str* _recv,
								 struct socket_info* sock, str* _inst, int sid);
int insert_ucontact(urecord_t* _r, str* aor, str* _c, time_t _e, qvalue_t _q, str* _cid, int _cs, 
			unsigned int _flags, struct ucontact** _con, str *_ua, str* _recv,
					struct socket_info* sock, str* _inst, int sid);

/*
 * Delete ucontact from urecord
 */
typedef int (*delete_ucontact_t)(urecord_t* _r, struct ucontact* _c);
int delete_ucontact(urecord_t* _r, struct ucontact* _c);


/*
 * Get pointer to ucontact with given contact
 */
typedef int (*get_ucontact_t)(urecord_t* _r, str* _c, struct ucontact** _co);
int get_ucontact(urecord_t* _r, str* _c, struct ucontact** _co);

typedef int (*get_ucontact_by_inst_t)(urecord_t* _r, str* _c, str* _i, struct ucontact** _co);
int get_ucontact_by_instance(urecord_t* _r, str* _c, str* _i, struct ucontact** _co);
#endif /* URECORD_H */
