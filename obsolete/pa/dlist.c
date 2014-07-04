/*
 * Presence Agent, domain list
 *
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 */


#include "dlist.h"
#include "../../dprint.h"
#include "../../mem/shm_mem.h"
#include "../../sr_module.h"
#include "../../ut.h"
#include "paerrno.h"
#include <string.h>
#include "ptime.h"
#include "presentity.h"

/*
 * List of all registered domains
 */
dlist_t* root = 0;


/*
 * Find domain with the given name
 * Returns 0 if the domain was found
 * and 1 of not
 */
static inline int find_dlist(str* _n, dlist_t** _d)
{
	dlist_t* ptr;

	ptr = root;
	while(ptr) {
		if ((_n->len == ptr->name.len) &&
		    !memcmp(_n->s, ptr->name.s, _n->len)) {
			*_d = ptr;
			return 0;
		}
		
		ptr = ptr->next;
	}
	
	return 1;
}


/*
 * Create a new domain structure
 * Returns 0 if everything went OK, otherwise value < 0
 * is returned
 *
 * The structure is NOT created in shared memory so the
 * function must be called before ser forks if it should
 * be available to all processes
 */
static inline int new_dlist(str* _n, dlist_t** _d)
{
	cmd_function reg;
	cmd_function unreg;

	dlist_t* ptr;

	     /* Domains are created before ser forks,
	      * so we can create them using pkg_malloc
	      */
	ptr = (dlist_t*)mem_alloc(sizeof(dlist_t));
	if (ptr == 0) {
		paerrno = PA_NO_MEMORY;
		LOG(L_ERR, "new_dlist(): No memory left\n");
		return -1;
	}
	memset(ptr, 0, sizeof(dlist_t));

	ptr->name.s = (char*)mem_alloc(_n->len);
	if (ptr->name.s == 0) {
		paerrno = PA_NO_MEMORY;
		LOG(L_ERR, "new_dlist(): No memory left 2\n");
		mem_free(ptr);
		return -2;
	}

	memcpy(ptr->name.s, _n->s, _n->len);
	ptr->name.len = _n->len;

	if ((_n->len == 9) && (!strncasecmp(_n->s, "registrar", 9))) {
		reg = find_export("ul_register_watcher", 1, 0);
		if (reg == 0) {
			LOG(L_ERR, "new_dlist(): ~ul_register_watcher not found\n");
			return -3;
		}
		unreg = find_export("ul_unregister_watcher", 1, 0);
		if (unreg == 0) {
			LOG(L_ERR, "new_dlist(): ~ul_unregister_watcher not found\n");
			return -4;
		}
	} else if ((_n->len == 6) && (!strncasecmp(_n->s, "jabber", 6))) {
		reg = find_export("jab_register_watcher", 1, 0);
		if (reg == 0) {
			LOG(L_ERR, "new_dlist(): jab_register_watcher not found\n");
			return -5;
		}
		unreg = find_export("jab_unregister_watcher", 1, 0);
		if (unreg == 0) {
			LOG(L_ERR, "new_dlist(): jab_unregister_watcher not found\n");
			return -6;
		}
	} else {
		LOG(L_ERR, "new_dlist(): Unknown module to bind: %.*s\n", _n->len, ZSW(_n->s));
			return -7;
	}

	if (new_pdomain(&(ptr->name), 512, &(ptr->d), (register_watcher_t)reg, (unregister_watcher_t)unreg) < 0) {
		LOG(L_ERR, "new_dlist(): Error while creating domain structure\n");
		mem_free(ptr->name.s);
		mem_free(ptr);
		return -8;
	}

	*_d = ptr;
	return 0;
}

int find_pdomain(const char* _n, pdomain_t** _d)
{
	dlist_t* d;
	str s;

	s.s = (char*)_n;
	s.len = strlen(_n);

	if (find_dlist(&s, &d) == 0) {
	        *_d = d->d;
		return 0;
	}
	
	return 1;
}

/*
 * Function registers a new domain with presence agent
 * if the domain exists, pointer to existing structure
 * will be returned, otherwise a new domain will be
 * created
 */
int register_pdomain(const char* _n, pdomain_t** _d)
{
	pdomain_t *pdomain;
	dlist_t* d;
	str s;

	s.s = (char*)_n;
	s.len = strlen(_n);

	if (find_dlist(&s, &d) == 0) {
	        *_d = d->d;
		return 0;
	}
	
	if (new_dlist(&s, &d) < 0) {
		LOG(L_ERR, "register_pdomain(): Error while creating new domain\n");
		return -1;
	} 

	pdomain = d->d;
	lock_pdomain(pdomain);	/* do not enable timer to delete presentities in it */
	d->next = root;
	root = d;
	
	*_d = pdomain;

	/* Preload domain with data from database if we are gonna
	 * to use database
	 */
	pdomain_load_presentities(pdomain);
	unlock_pdomain(pdomain);

	return 0;
}


/*
 * Free all allocated memory
 */
void free_all_pdomains(void)
{
	dlist_t* ptr;

	while(root) {
		ptr = root;
		root = root->next;

		free_pdomain(ptr->d);
		mem_free(ptr->name.s);
		mem_free(ptr);
	}
}

/*
 * Run timer handler of all domains
 */
int timer_all_pdomains(void)
{
	int res = 0;
	dlist_t* ptr;

	get_act_time(); /* Get and save actual time */

	ptr = root;
	while(ptr) {
		res |= timer_pdomain(ptr->d);
		ptr = ptr->next;
	}
	
	return res;
}

