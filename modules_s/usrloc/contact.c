/* 
 * $Id$ 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contact.h"
#include "../../str.h"
#include "utils.h"
#include "../../dprint.h"


void print_contact(contact_t* _c)
{
	char* t;
#ifdef PARANOID
	if (!_c) return;
#endif
	t = ctime(&(_c->expire));
	t[strlen(t) - 1] = '\0';
	printf("    Contact=\"%s\" Expire=\"%s\" q=%10.2f new=%d dirty=%d\n",
	       _c->c.s, t, _c->q, _c->new, _c->dirty);
}


contact_t* create_contact(str* _aor, const char* _c, time_t _expire, float _q,
			  unsigned char _new, unsigned char _dirty, unsigned char _garbage)
{
	contact_t* ptr;
	int len;
#ifdef PARANOID
	if (!_c) return NULL;
	if (!_aor) return NULL;
#endif

	ptr = (contact_t*)malloc(sizeof(contact_t));
	if (!ptr) {
	        LOG(L_ERR, "create_contact(): No memory left\n");
		return NULL;
	}

	len = strlen(_c);
	ptr->c.s = (char *)malloc(len + 1);
	memcpy(ptr->c.s, _c, len + 1);
	strlower(ptr->c.s, len);
        ptr->c.len = len;

	ptr->aor = _aor;
	ptr->expire = _expire;
	ptr->q = _q;
	ptr->next = NULL;

	ptr->new = _new;
	ptr->dirty = _dirty;
	ptr->garbage = _garbage;

	return ptr;
}	


void free_contact(contact_t* _c)
{
	if (_c) {
		free(_c->c.s);
		free(_c);
	}
}



int cmp_contact(contact_t* _c1, contact_t* _c2)
{
	return (strcmp(_c1->c.s, _c2->c.s));
}

