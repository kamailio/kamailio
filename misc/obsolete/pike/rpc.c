#include "ip_tree.h"
#include "rpc.h"
#include "top.h"
#include "../../timer.h"	/* ticks_t */	

#include "../../dprint.h"

#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
// TODO FIXME LCH remove arpa/inet.h after testing
#include <arpa/inet.h>

// IPv6 address is a 16 bytes long
#define MAX_DEPTH 16

static unsigned int g_max_hits = 0;

static void traverse_subtree( struct ip_node *node, int depth, int options )
{
	static unsigned char ip_addr[MAX_DEPTH];

	struct ip_node *foo;
	
	DBG("pike:rpc traverse_subtree, depth: %d, byte: %d", depth, node->byte);

	assert( depth < MAX_DEPTH );
	
	ip_addr[depth] = node->byte;

	if ( node->flags & NODE_IPLEAF_FLAG ) {
		int ns = node_status(node);
		DBG("pike:traverse_subtree: options: 0x%02x, node status: 0x%02x", options, ns);
		/* add to the result list if it has requested status */
		switch (options) {
			case NODE_STATUS_HOT:
						if ( ns & NODE_STATUS_HOT )
							pike_top_add_entry(ip_addr, depth+1, node->leaf_hits, node->hits, node->expires - get_ticks(), ns);
						break;
			case NODE_STATUS_ALL:
						pike_top_add_entry(ip_addr, depth+1, node->leaf_hits, node->hits, node->expires - get_ticks(), ns);
						break;
		}
	}
	else if (! node->kids) {	/* TODO non IP leaf of ip_tree - it is possible to report WARM nodes here */
/*		if ( options == ns )
			pike_top_add_entry(ip_addr, depth+1, node->leaf_hits, node->hits, node->expires - get_ticks(), ns);
*/	}
	else {	/* not a any kind of leaf - inner node */
		DBG("pike:rpc traverse_subtree, not IP leaf, depth: %d, ip: %d.%d.%d.%d   hits[%d,%d], expires: %d",
			depth, ip_addr[0], ip_addr[1], ip_addr[2], ip_addr[3], node->hits[0], node->hits[1], node->expires - get_ticks());
	}
	
	foo = node->kids;
	while (foo) {
		traverse_subtree( foo, depth + 1, options );
		foo = foo->next;
	}
}

static void collect_data(int options)
{
	int i;

	g_max_hits = get_max_hits();

	DBG("pike: collect_data");
	
	// maybe try_lock first and than do the rest?
	for(i=0;i<MAX_IP_BRANCHES;i++) {
		if (get_tree_branch(i)==0)
			continue;
		DBG("pike: collect_data: branch %d", i);
		lock_tree_branch(i);
		if (get_tree_branch(i))
			traverse_subtree( get_tree_branch(i), 0, options );
		unlock_tree_branch(i);
    }
}

/* do not use static buffer with this function */
static const char *concat_err = "ERROR while concatenating string";
static char *concat(char *buff, size_t buffsize, const char *first, int second)
{
	int rv;
	size_t size;
	
	while ( (rv = snprintf(buff, buffsize, "%s%d", first, second)) >= buffsize ) {
		size = rv > 128 ? rv : 128;
		buff = (char *)realloc(buff, size);
		if ( buff == 0 )
			return (char*)concat_err;
		buffsize = size;
		DBG("pike:rpc:concat: new buffer size for %s: %d", first,
				(int)buffsize);
	}
	return buff;
}

static void pike_top(rpc_t *rpc, void *c)
{
	int i;
	void *handle;
	struct TopListItem_t *top_list_root;
	struct TopListItem_t *ti = 0;
	char addr_buff[40];
	char *ip_addr = 0;
	char *leaf_hits_prev = 0;
	char *leaf_hits_curr = 0;
	char *expires = 0;
	char *status = 0;
	size_t ip_addr_size = 0;
	size_t leaf_hits_prev_size = 0;
	size_t leaf_hits_curr_size = 0;
	size_t expires_size = 0;
	size_t status_size = 0;
	char *stropts;
	int   options = 0;

	DBG("pike: top");
	
	/* obtain params */
	if (rpc->scan(c, "s", &stropts) <= 0)
		stropts = "HOT";

	DBG("pike:top: string options: '%s'", stropts);
	if ( strstr(stropts, "ALL") ) { 
		options = NODE_STATUS_ALL;
	} else if ( strstr(stropts, "HOT") ) {
		options |= NODE_STATUS_HOT;
	} else if ( strstr(stropts, "WARM") ) {
		options |= NODE_STATUS_WARM;
	}
	DBG("pike:top: options: 0x%02x\n", options);
	
	
	print_tree( 0 );
	
	collect_data(options);
	top_list_root = pike_top_get_root();
	DBG("pike_top: top_list_root = %p", top_list_root);
	
	rpc->add(c, "{", &handle);
	rpc->struct_add(handle, "d", "max_hits", get_max_hits());
	i = 0; // it is passed as number of rows
	if ( top_list_root == 0 ) {
		DBG("pike_top: no data");
	}
	else {
		for( ti = top_list_root, i = 0; ti != 0; ti = ti->next, ++i ) {
			pike_top_print_addr(ti->ip_addr, ti->addr_len, addr_buff, sizeof(addr_buff));
			DBG("pike:top: result[%d]: %s leaf_hits[%d,%d] hits[%d,%d] expires: %d status: 0x%02x",
					i, addr_buff, ti->leaf_hits[0], ti->leaf_hits[1],
					ti->hits[0], ti->hits[1], ti->expires, ti->status); 
			rpc->struct_add(handle, "sddds",
							concat(ip_addr, ip_addr_size, "ip_addr", i), addr_buff,
							concat(leaf_hits_prev, leaf_hits_prev_size, "leaf_hits_prev", i), ti->leaf_hits[0],
							concat(leaf_hits_curr, leaf_hits_curr_size, "leaf_hits_curr", i), ti->leaf_hits[1],
							concat(expires, expires_size, "expires", i), ti->expires,
							concat(status, status_size, "status", i), node_status_array[ti->status]);
		}
	}
	rpc->struct_add(handle, "d", "number_of_rows", i);
	/* free buffers */
	free(ip_addr);
	free(leaf_hits_prev);
	free(leaf_hits_curr);
	free(expires);
	free(status);
	pike_top_list_clear();
	
	rpc->send(c);
}

/* ----- exported data structure with methods ----- */

// TODO check documentation
static const char* pike_top_doc[] = {
	"pike.top doc.",  /* Documentation string */
	0                 /* Method signature(s) */
};

/* 
 * RPC Methods exported by this module 
 */

rpc_export_t pike_rpc_methods[] = {
	{"pike.top",   pike_top,     pike_top_doc, 0},
	{0, 0, 0, 0}
};

