#ifndef _PIKE_FUNCS_H
#define PIKE_FUNCS_H

#include "../../parser/msg_parser.h"
#include "tree234.h"
#include "lock.h"
#include "timer.h"


enum pike_locks {
	BT4_INDEX_LOCK,
	BT6_INDEX_LOCK,
	TL4_INDEX_LOCK,
	TL6_INDEX_LOCK,
	PIKE_NR_LOCKS
};

enum pike_ip_types {
	IPv4,
	IPv6,
	IP_TYPES
};


extern int                     time_unit;
extern int                     max_value;
extern int                     timeout;
extern tree234                 *btrees[IP_TYPES];
extern pike_lock               *locks;
extern struct pike_timer_head  timers[IP_TYPES];



struct ip_v4 {
	int ip;
	unsigned short counter[2];
	struct pike_timer timer;
};

struct ip_v6 {
	int ip[4];
	unsigned short counter[2];
	struct pike_timer timer;
};



int cmp_ipv4(void*,void*);
int cmp_ipv6(void*,void*);
void free_elem(void *elem);

int pike_check_req(struct sip_msg *msg, char *foo, char *bar);
void clean_routine(void*);
void swap_routine(void*);


#endif
