/*
 *
 * $Id$
 *
 */

#ifndef _KILL_H
#define _KILL_H

struct timer_link {
	struct timer_link *next_tl;
	struct timer_link *prev_tl;
	volatile unsigned int time_out;
	int pid;
};

struct timer_list
{
	struct timer_link  first_tl;
	struct timer_link  last_tl;
};

extern unsigned int time_to_kill;

void destroy_kill();
int initialize_kill();
int schedule_to_kill( int pid );


#endif

