/* 
 * $Id$ 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "timer_dirty.h"
#include "c_elem.h"
#include "utils.h"
#include "dbase.h"
#include "../../dprint.h"

#define DIRTY_BUF_MAX 512


static contact_t** dirty_contacts;
static char* table;


static int scan_table(cache_t* _c)
{
	int offset = 0;
	c_elem_t* el;
	contact_t* t;

#ifdef PARANOID
	if (!_c) return -1;
	if (!dirty_contacts) return -1;
#endif
	el = CACHE_FIRST_ELEM(_c);

	while (el) {
		if ((!el->state.invisible) && (!el->state.garbage)) {
			t = el->loc->contacts;
			while (t) {
				if (t->new) {
					t->dirty = FALSE;
				} else {
					if (t->dirty) {
						dirty_contacts[offset++] = t;
						if (offset == DIRTY_BUF_MAX) return offset;
					}
				}
				t = t->next;
			}
		}
		el = CACHE_NEXT_ELEM(el);
	}

	return offset;
}


void timer_dirty(cache_t* _c)
{
	int cnt, i;

#ifdef PARANOID
	if (!_c) return;
#endif
	mutex_down(_c->mutex);
	cnt = scan_table(_c);
	mutex_up(_c->mutex);

	for(i = 0; i < cnt; i++) {
		if (update_contact(table, dirty_contacts[i]) == FALSE) {
			LOG(L_ERR, "timer_dirty(): Error while inserting contact in database\n");
		} else {
			dirty_contacts[i]->dirty = FALSE;
		}
	}
}




int init_timer_dirty(const char* _table)
{
#ifdef PARANOID
	if (!_table) return FALSE;
#endif

        dirty_contacts = (contact_t**)malloc(sizeof(contact_t*) * DIRTY_BUF_MAX);
	if (!dirty_contacts) {
		return FALSE;
	}

	table = strdup(_table);
	if (!table) {
		return FALSE;
	}

	return TRUE;
}


void close_timer_dirty(void)
{
	if (dirty_contacts) {
		free(dirty_contacts);
	}

	if (table) {
		free(table);
	}
}			   


