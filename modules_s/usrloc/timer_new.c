/* 
 * $Id$ 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "timer_new.h"
#include "c_elem.h"
#include "utils.h"
#include "dbase.h"
#include "../../dprint.h"

#define NEW_BUF_MAX 512


static contact_t** new_contacts;
static char* table;


static int scan_table(cache_t* _c)
{
	int offset = 0;
	c_elem_t* el;
	contact_t* t;

#ifdef PARANOID
	if (!_c) return -1;
	if (!new_contacts) return -1;
#endif
	el = CACHE_FIRST_ELEM(_c);

	while (el) {
		if ((!el->state.invisible) && (!el->state.garbage)) {
			t = el->loc->contacts;
			while (t) {
				if (t->new) {
					if (t->dirty) t->dirty = FALSE;
					new_contacts[offset++] = t;
					if (offset == NEW_BUF_MAX) return offset;
				}
				t = t->next;
			}
		}
		el = CACHE_NEXT_ELEM(el);
	}

	return offset;
}


void timer_new(cache_t* _c)
{
	int cnt, i;

#ifdef PARANOID
	if (!_c) return;
#endif
	mutex_down(_c->mutex);
	cnt = scan_table(_c);
	mutex_up(_c->mutex);

	for(i = 0; i < cnt; i++) {
		if (insert_contact(table, new_contacts[i]) == FALSE) {
			LOG(L_ERR, "timer_new(): Error while inserting contact in database\n");
		} else {
			new_contacts[i]->new = FALSE;
		}
	}
}




int init_timer_new(const char* _table)
{
#ifdef PARANOID
	if (!_table) return FALSE;
#endif

        new_contacts = (contact_t**)malloc(sizeof(contact_t*) * NEW_BUF_MAX);
	if (!new_contacts) {
		return FALSE;
	}

	table = strdup(_table);
	if (!table) {
		return FALSE;
	}

	return TRUE;
}


void close_timer_new(void)
{
	if (new_contacts) {
		free(new_contacts);
	}

	if (table) {
		free(table);
	}
}			   


