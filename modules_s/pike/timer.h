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
	pike_lock         sem;
};

//void add()

#endif

