/*
 * PIKE module
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "ip_tree.h"
#include "../../core/rpc_lookup.h"
/*??? #include "rpc.h" */
/*??? #include "top.h" */
#include "../../core/timer.h" /* ticks_t */

#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "pike_top.h"

#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
// TODO FIXME LCH remove arpa/inet.h after testing
#include <arpa/inet.h>

// IPv6 address is a 16 bytes long
#define MAX_DEPTH 16

static unsigned int g_max_hits = 0;

static void traverse_subtree(pike_ip_node_t *node, int depth, int options)
{
	static unsigned char ip_addr[MAX_DEPTH];

	pike_ip_node_t *foo;

	DBG("pike:rpc traverse_subtree, depth: %d, byte: %d", depth, node->byte);

	assert(depth < MAX_DEPTH);

	ip_addr[depth] = node->byte;

	if(node->flags & NODE_IPLEAF_FLAG) {
		int ns = node_status(node);
		DBG("pike:traverse_subtree: options: 0x%02x, node status: 0x%02x",
				options, ns);
		/* add to the result list if it has requested status */
		switch(options) {
			case NODE_STATUS_HOT:
				if(ns & NODE_STATUS_HOT)
					pike_top_add_entry(ip_addr, depth + 1, node->leaf_hits,
							node->hits, node->expires - get_ticks(), ns);
				break;
			case NODE_STATUS_ALL:
				pike_top_add_entry(ip_addr, depth + 1, node->leaf_hits,
						node->hits, node->expires - get_ticks(), ns);
				break;
		}
	} else if(!node->kids) {
		/* TODO non IP leaf of ip_tree - it is possible to report WARM nodes here */
/*		if ( options == ns )
			pike_top_add_entry(ip_addr, depth+1, node->leaf_hits, node->hits,
					node->expires - get_ticks(), ns);
*/	}
	else {	/* not a any kind of leaf - inner node */
	DBG("pike:rpc traverse_subtree, not IP leaf, depth: %d, ip: %d.%d.%d.%d"
		"   hits[%d,%d], expires: %d",
			depth, ip_addr[0], ip_addr[1], ip_addr[2], ip_addr[3],
			node->hits[0], node->hits[1], node->expires - get_ticks());
}

foo = node->kids;
while(foo) {
	traverse_subtree(foo, depth + 1, options);
	foo = foo->next;
}
}

static void collect_data(int options)
{
	int i;

	g_max_hits = get_max_hits();

	DBG("pike: collect_data");

	// maybe try_lock first and than do the rest?
	for(i = 0; i < MAX_IP_BRANCHES; i++) {
		if(get_tree_branch(i) == 0)
			continue;
		DBG("pike: collect_data: branch %d", i);
		lock_tree_branch(i);
		if(get_tree_branch(i))
			traverse_subtree(get_tree_branch(i), 0, options);
		unlock_tree_branch(i);
	}
}

static void pike_top(rpc_t *rpc, void *c)
{
	int i;
	void *handle;
	void *list;
	void *item;
	struct TopListItem_t *top_list_root;
	struct TopListItem_t *ti = 0;
	char addr_buff[PIKE_BUFF_SIZE * sizeof(char)];
	char *stropts;
	int options = 0;

	DBG("pike: top");

	/* obtain params */
	if(rpc->scan(c, "s", &stropts) <= 0)
		stropts = "HOT";

	DBG("pike:top: string options: '%s'", stropts);
	if(strz_casesearch_strz(stropts, "ALL")) {
		options = NODE_STATUS_ALL;
	} else if(strz_casesearch_strz(stropts, "HOT")) {
		options |= NODE_STATUS_HOT;
	} else if(strz_casesearch_strz(stropts, "WARM")) {
		options |= NODE_STATUS_WARM;
	}
	DBG("pike:top: options: 0x%02x\n", options);


	if(options == 0) {
		rpc->fault(c, 500, "Bad argument. Select: ALL, HOT or WARM");
		return;
	}


	print_tree(0);

	collect_data(options);
	top_list_root = pike_top_get_root();
	DBG("pike_top: top_list_root = %p", top_list_root);

	rpc->add(c, "{", &handle);
	rpc->struct_add(handle, "d[", "max_hits", get_max_hits(), "list", &list);
	i = 0; // it is passed as number of rows
	if(top_list_root == 0) {
		DBG("pike_top: no data");
	} else {
		for(ti = top_list_root, i = 0; ti != 0; ti = ti->next, ++i) {
			pike_top_print_addr(
					ti->ip_addr, ti->addr_len, addr_buff, sizeof(addr_buff));
			DBG("pike:top: result[%d]: %s leaf_hits[%d,%d] hits[%d,%d]"
				" expires: %d status: 0x%02x",
					i, addr_buff, ti->leaf_hits[0], ti->leaf_hits[1],
					ti->hits[0], ti->hits[1], ti->expires, ti->status);
			rpc->array_add(list, "{", &item);
			rpc->struct_add(item, "sddds", "ip_addr", addr_buff,
					"leaf_hits_prev", ti->leaf_hits[0], "leaf_hits_curr",
					ti->leaf_hits[1], "expires", ti->expires, "status",
					node_status_array[ti->status]);
		}
	}
	rpc->struct_add(handle, "d", "number_of_rows", i);
	pike_top_list_clear();
}

/* ----- exported data structure with methods ----- */

// TODO check documentation
static const char *pike_top_doc[] = {
		"pike.top Dump parts of the pike table", /* Documentation string */
		0										 /* Method signature(s) */
};

/*
 * RPC Methods exported by this module
 */

rpc_export_t pike_rpc_methods[] = {{"pike.top", pike_top, pike_top_doc, 0},
		{"pike.list", pike_top, pike_top_doc, 0}, {0, 0, 0, 0}};
