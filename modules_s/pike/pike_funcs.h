#ifndef _PIKE_FUNCS_H
#define PIKE_FUNCS_H

#include "tree234.h"
#include "lock.h"


extern int         time_unit;
extern int         max_value;
extern int         timeout;
extern tree234     *ipv4_bt;
extern tree234     *ipv6_bt;
extern ser_lock_t  *pike_locks;

enum pike_locks{BT4_INDEX_LOCK,BT6_INDEX_LOCK,PTL_INDEX_LOCK,PIKE_NR_LOCKS};

#define BT4_lock &(pike_locks[BT4_INDEX_LOCK])
#define BT6_lock &(pike_locks[BT6_INDEX_LOCK])
#define PTL_lock &(pike_locks[PTL_INDEX_LOCK])

struct ip_v4 {
	int ip;
	// timer list linker
};
struct ip_v6 {
	int ip[4];
	// timer list linker
};

int cmp_ipv4(void*,void*);
int cmp_ipv6(void*,void*);
void free_elem(void *elem);



#endif
