
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




int pike_check_req(struct sip_msg *msg, char *foo, char *bar)
{
	struct ip_v4 *ip4, *old_ip4;
	//struct ip_v6 *ip6, *old_ip6;

	if (msg->src_ip.af==AF_INET) {
		/* we have an IPV4 address */
		ip4 = (struct ip_v4*)shm_malloc(sizeof(struct ip_v4));
		if (!ip4) {
			LOG(L_ERR,"ERROR:pike_check_req: cannot allocated sh mem!\n");
			goto error;
		}
		ip4->ip = msg->src_ip.u.addr32[0];
		lock(&bt_locks[IPv4]);
		if ((old_ip4=add234(btrees[IPv4],ip4))!=ip4) {
			/* the src ip already in tree */
			DBG("DEBUG:pike_check_req: IPv4 src found [%X] with [%d][%d]\n",
				old_ip4->ip,old_ip4->counter[1],old_ip4->counter[0]);
			old_ip4->counter[0]++;
			if (old_ip4->counter[0]>(unsigned short)max_value) {
				LOG(L_INFO,"INFO: src IP v4 [%x]exceeded!!\n",old_ip4->ip);
				goto exceed;
			}
			unlock(&bt_locks[IPv4]);
			shm_free(ip4);
		} else {
			/* new record */
			unlock(&bt_locks[IPv4]);
			DBG("DEBUG:pike_check_req: new IPv4 src [%X]\n",ip4->ip);
		}
	} else {
	}
error:
	return 1;
exceed:
	return -1;
}




void clean_routine(void *param)
{
}




void swap_routine(void *param)
{
}



