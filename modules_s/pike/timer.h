#ifndef _PIKE_TIMER_H
#define _PIKE_TIMER_H

#include "lock.h"


struct pike_timer {
	int timeout;
	struct pike_timer *next;
	struct pike_timer *prev;
};

struct pike_timer_head {
	struct pike_timer *first;
	struct pike_timer *last;
	pike_lock         *sem;
};

void append_to_timer(struct pike_timer_head*, struct pike_timer*);
void remove_from_timer(struct pike_timer_head*, struct pike_timer*);
int  is_empty(struct pike_timer_head*);
struct pike_timer *check_and_split_timer(struct pike_timer_head*,int);

#endif

