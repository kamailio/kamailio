/* 
 * $Id$ 
 *
 * Usrloc hash table collision slot
 */

#ifndef HSLOT_H
#define HSLOT_H

#include "udomain.h"
#include "urecord.h"


struct udomain;
struct urecord;


typedef struct hslot {
	int n;                  /* Number of elements in the collision slot */
	struct urecord* first;  /* First element in the list */
	struct urecord* last;   /* Last element in the list */
	struct udomain* d;      /* Domain we belong to */
} hslot_t;


/*
 * Initialize slot structure
 */
int init_slot(struct udomain* _d, hslot_t* _s);


/*
 * Deinitialize given slot structure
 */
void deinit_slot(hslot_t* _s);


/*
 * Add an element to slot linked list
 */
void slot_add(hslot_t* _s, struct urecord* _r);


/*
 * Remove an element from slot linked list
 */
void slot_rem(hslot_t* _s, struct urecord* _r);


#endif /* HSLOT_H */
