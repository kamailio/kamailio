/* 
 * $Id$ 
 */

#ifndef __CONTACT_H__
#define __CONTACT_H__

#include <time.h>
#include "../../str.h"


typedef struct contact {
	str* aor;              /* Pointer to the address of record string */
	str c;                 /* Contact address */
	time_t expire;         /* expire parameter */
	float q;               /* q parameter */
	unsigned char new;     /* Entry is not in database yet */
	unsigned char dirty;   /* Entry has been modified and should be flushed asap */
        unsigned char garbage; /* Should be removed asap if set to TRUE */
	struct contact* next;  /* Next contact in the linked list */
} contact_t;


void       print_contact  (contact_t* _c);   /* Print structure content to stdout */
contact_t* create_contact (str* _aor, const char* _c, time_t _expire, float _q,
			   unsigned char _new, unsigned char _dirty, unsigned char _garbage);
void       free_contact   (contact_t* _c);
int        cmp_contact    (contact_t* _c1, contact_t* _c2);

#endif
