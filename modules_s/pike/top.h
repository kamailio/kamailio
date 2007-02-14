#ifndef __PIKE_TOP_H
#define __PIKE_TOP_H

#include "ip_tree.h"
#include "../../ip_addr.h"

struct TopListItem_t {
	int             addr_len;
	unsigned char   ip_addr[16];
	unsigned short  leaf_hits[2];
	unsigned short  hits[2];
	unsigned int    expires;	/* in seconds */
	node_status_t   status;

	struct TopListItem_t *next;
};

// returns 1 when OK and 0 when failed
int pike_top_add_entry( unsigned char *ip_addr, int iplen, unsigned int leaf_hits[2], unsigned int hits[2], unsigned int expires, node_status_t status );

struct TopListItem_t *pike_top_get_root();
void pike_top_list_clear();

/* helpful functions */
char *pike_top_print_addr( unsigned char *ip_addr, int addrlen, char *buff, int buffsize );


#endif // PIKE_TOP_H
