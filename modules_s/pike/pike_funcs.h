/* 
 * $Id$
 *
 */

#ifndef _PIKE_FUNCS_H
#define PIKE_FUNCS_H

#include "../../parser/msg_parser.h"
#include "ip_tree.h"
#include "lock.h"
#include "timer.h"
#include "ip_tree.h"


enum pike_locks {
	TREE_LOCK,
	TIMER_LOCK,
	PIKE_NR_LOCKS
};


extern int                     time_unit;
extern int                     max_value;
extern int                     timeout;
extern struct ip_node          *tree;
extern pike_lock               *locks;
extern struct pike_timer_head  *timer;


int  pike_check_req(struct sip_msg *msg, char *foo, char *bar);
void clean_routine(unsigned int, void*);
void swap_routine(unsigned int, void*);


#endif
