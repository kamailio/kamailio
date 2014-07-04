/* 
 * $Id$
 *
 * allow_trusted related functions
 *
 * Copyright (C) 2003 Juha Heinanen
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * --------
 *   2008-08-01: released
 */

#include "../../dprint.h"
#include "../../lib/srdb2/db.h"
#include "../../mem/shm_mem.h"
#include "permissions.h"
#include "ip_set.h"
#include "ip_set_rpc.h"


static struct ip_set_list_item *ip_set_list = NULL;
static int ip_set_list_count = 0;

int ip_set_list_malloc(int num, str* names) {
	int i, j;
	if (num) {
		ip_set_list = shm_malloc(num*sizeof(*ip_set_list));
		if (!ip_set_list) return -1;
		ip_set_list_count = num;
		for (i=0; i<ip_set_list_count; i++) {
			ip_set_list[i].idx = i;
			ip_set_list[i].name = names[i];
			lock_init(&ip_set_list[i].read_lock);
			lock_init(&ip_set_list[i].write_lock);
			ip_set_list[i].ip_set = NULL;
			ip_set_init(&ip_set_list[i].ip_set_pending, 1);
			for (j=0; j<ip_set_list[i].name.len; j++) {
				if (ip_set_list[i].name.s[j]=='=') {
					str s;
					s.s = ip_set_list[i].name.s + j + 1;
					s.len = ip_set_list[i].name.len - j - 1;
					ip_set_list[i].name.len = j;
					ip_set_list[i].ip_set = shm_malloc(sizeof(*ip_set_list[i].ip_set));
					if (!ip_set_list[i].ip_set) return -1;
					atomic_set(&ip_set_list[i].ip_set->refcnt, 1);
					ip_set_add_list(&ip_set_list[i].ip_set->ip_set, s); /* allow pass even in case of error */
				}
			}
		}
	}
	return 0;
}

void ip_set_list_free() {
	int i;
	if (!ip_set_list) return;
	for (i=0; i<ip_set_list_count; i++) {
		lock_destroy(&ip_set_list[i].read_lock);
		lock_destroy(&ip_set_list[i].write_lock);
		if (ip_set_list[i].ip_set) {
			if (atomic_dec_and_test(&ip_set_list[i].ip_set->refcnt)) { /* do not destroy cloned sets because if they can live only in local copy after commit,
										   they must be deleted separately in local copy before this procedure is called*/
				ip_set_destroy(&ip_set_list[i].ip_set->ip_set);
				shm_free(ip_set_list[i].ip_set);
			}			
		}
		ip_set_destroy(&ip_set_list[i].ip_set_pending);
	}
	shm_free(ip_set_list);
	ip_set_list = NULL;

}

struct ip_set_list_item* ip_set_list_find_by_name(str name) {
	int i;
	for (i=0; i<ip_set_list_count; i++) {
		if (ip_set_list[i].name.len == name.len && memcmp(ip_set_list[i].name.s, name.s, name.len) == 0)
			return &ip_set_list[i];
	}
	return NULL;
}


const char* rpc_ip_set_clean_doc[] = {
	"Clean ip set.",
	0
};


void rpc_ip_set_clean(rpc_t* rpc, void* ctx) {
	str name;
	struct ip_set_list_item *p;
	if (rpc->scan(ctx, "S", &name) < 1) {
		return;
	}
	p = ip_set_list_find_by_name(name);
	if (!p) {
		rpc->fault(ctx, 400, "Ip set not found");
		return;
	}
	lock_get(&p->write_lock);
	ip_set_destroy(&p->ip_set_pending);
	ip_set_init(&p->ip_set_pending, 1);
	lock_release(&p->write_lock);
}

const char* rpc_ip_set_add_doc[] = {
	"Add IP/mask in ip set.",
	0
};


void rpc_ip_set_add(rpc_t* rpc, void* ctx) {
	str name, ip, mask;
	struct ip_set_list_item* p;
	if (rpc->scan(ctx, "SSS", &name, &ip, &mask) < 1) {
		return;
	}
	while (mask.len && mask.s[0]=='/') {  /* rpc cannot read plain number as string, adding '/' helps */
		mask.s++;
		mask.len--;
	}
	p = ip_set_list_find_by_name(name);
	if (!p) {
		rpc->fault(ctx, 400, "Ip set not found");
		return;
	}
	lock_get(&p->write_lock);
	if (ip_set_add_ip_s(&p->ip_set_pending, ip, mask) < 0) {
		lock_release(&p->write_lock);
		rpc->fault(ctx, 400, "Cannot add ip/mask into ip set");
		return;
	}
	lock_release(&p->write_lock);
}

const char* rpc_ip_set_commit_doc[] = {
	"Commit changes in ip set.",
	0
};

void rpc_ip_set_commit(rpc_t* rpc, void* ctx) {
	str name;
	struct ip_set_list_item *p;
	struct ip_set_ref *new_ip_set;
	if (rpc->scan(ctx, "S", &name) < 1) {
		return;
	}
	p = ip_set_list_find_by_name(name);
	if (!p) {
		rpc->fault(ctx, 400, "Ip set not found");
		return;
	}
	lock_get(&p->write_lock);
	lock_get(&p->read_lock);
	new_ip_set = shm_malloc(sizeof(*new_ip_set));
	if (!new_ip_set) {
		rpc->fault(ctx, 500, "Not enough memory");
		goto err;
	}
	
	if (p->ip_set) {
		if (atomic_dec_and_test(&p->ip_set->refcnt)) {
			ip_set_destroy(&p->ip_set->ip_set);  /* not used in local copy */
			shm_free(p->ip_set);
		}
	}
	new_ip_set->ip_set = p->ip_set_pending;
	atomic_set(&new_ip_set->refcnt, 1);
	p->ip_set = new_ip_set;
	
	ip_set_init(&p->ip_set_pending, 1);
err:	
	lock_release(&p->read_lock);
	lock_release(&p->write_lock);
}

const char* rpc_ip_set_list_doc[] = {
	"List ip set names.",
	0
};


void rpc_ip_set_list(rpc_t* rpc, void* ctx) {
	int i;
	void *c;
	rpc->add(ctx, "{", &c);
	for (i=0; i<ip_set_list_count; i++) {
		if (rpc->struct_add(c, "S", "Name", &ip_set_list[i].name) < 0) {
			 rpc->fault(ctx, 500, "Error when listing ip sets");
		}
	}
}

const char* rpc_ip_set_print_doc[] = {
	"Print ip set.",
	0
};


static int rpc_ip_tree_print(rpc_t* rpc, void *ctx, char *prefix, struct ip_tree_leaf *tree, unsigned int indent) {
	int j;
	if (!tree) {
		if (rpc->struct_printf(ctx, "", "%*snil", indent, prefix) < 0) return -1;
	}
	else {
		str s;
		s = ip_tree_mask_to_str(tree->prefix_match, tree->prefix_match_len);
		if (rpc->struct_printf(ctx, "", "%*smatch %d bits {%.*s}", indent, prefix, tree->prefix_match_len, s.len, s.s) < 0) 
			return -1;
		for (j=0; j<=1; j++) {
			if (rpc_ip_tree_print(rpc, ctx, j==0?"0:":"1:", tree->next[j], indent+2) < 0)
				return -1;
		}
	}
	return 0;
}

	
void rpc_ip_set_print(rpc_t* rpc, void* ctx) {
	struct ip_set_list_item *p;
	struct ip_set *ip_set, ip_set2;
	void *c;
	str name;
	int pending;
	if (rpc->scan(ctx, "Sd", &name, &pending) < 1) {
		return;
	}
	p = ip_set_list_find_by_name(name);
	if (!p) {
		rpc->fault(ctx, 400, "Ip set not found");
		return;
	}
	if (pending) {
		lock_get(&p->write_lock);
		ip_set = &p->ip_set_pending;
	} else {
		lock_get(&p->read_lock);
		if (!p->ip_set) {
			ip_set_init(&ip_set2, 1); /* dummy to return empty ip set */
			ip_set = &ip_set2;
		}
		else
			ip_set = &p->ip_set->ip_set;
	}

	/* nested array/struct not supported */
	rpc->add(ctx, "{", &c);
	if (rpc->struct_add(c, "s", "IPv", "4") < 0) 
		goto err;	
	if (rpc_ip_tree_print(rpc, c, "", ip_set->ipv4_tree, 0) < 0) 
		goto err;
	rpc->add(ctx, "{", &c);
	if (rpc->struct_add(c, "s", "IPv", "6") < 0) 
		goto err;	
	if (rpc_ip_tree_print(rpc, c, "", ip_set->ipv6_tree, 0) < 0) 
		goto err;

err:		
	if (pending)
		lock_release(&p->write_lock);
	else
		lock_release(&p->read_lock);

}

