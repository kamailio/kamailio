#ifndef _PIKE_TIMER_H
#define _PIKE_TIMER_H

#include "lock.h"


struct pike_timer_link {
	int timeout;
	struct pike_timer_link *next;
	struct pike_timer_link *prev;
};

struct pike_timer_head {
	struct pike_timer_link *first;
	struct pike_timer_link *last;
	pike_lock              *sem;
};

void append_to_timer(struct pike_timer_head*, struct pike_timer_link* );
void remove_from_timer(struct pike_timer_head*, struct pike_timer_link* );
int  is_empty(struct pike_timer_head*);
struct pike_timer_link *check_and_split_timer(struct pike_timer_head*,int);

#endif

