
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../mem/shm_mem.h"
#include "pike_funcs.h"




void free_elem(void *elem)
{
	if (elem)
		shm_free(elem);
}




int cmp_ipv4(void* ip41, void *ip42)
{
	if (!ip41 || !ip42)
		return 0;
	if (((struct ip_v4*)ip41)->ip==((struct ip_v4*)ip42)->ip)
		return 0;
	else if (((struct ip_v4*)ip41)->ip>((struct ip_v4*)ip42)->ip)
		return 1;
	return -1;
}




int cmp_ipv6(void* ip61, void *ip62)
{
	if (!ip61 || !ip62)
		return 0;
	return  (memcmp( ((struct ip_v6*)ip61)->ip, ((struct ip_v6*)ip62)->ip, 4));
}
