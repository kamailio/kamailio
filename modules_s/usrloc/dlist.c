/*
 * $Id$
 *
 * List of registered domains
 */

#include "dlist.h"
#include <string.h>        /* strlen, memcmp */
#include <stdio.h>         /* printf */
#include "../../mem/mem.h" /* pkg_malloc, pkg_free */
#include "../../dprint.h"
#include "udomain.h"       /* new_udomain, free_udomain */
#include "utime.h"
#include "ul_mod.h"


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
	dlist_t* ptr;

	     /* Domains are created before ser forks,
	      * so we can create them using pkg_malloc
	      */
	ptr = (dlist_t*)pkg_malloc(sizeof(dlist_t));
	if (ptr == 0) {
		LOG(L_ERR, "new_dlist(): No memory left\n");
		return -1;
	}
	memset(ptr, 0, sizeof(dlist_t));

	ptr->name.s = (char*)pkg_malloc(_n->len);
	if (ptr->name.s == 0) {
		LOG(L_ERR, "new_dlist(): No memory left 2\n");
		pkg_free(ptr);
		return -2;
	}

	memcpy(ptr->name.s, _n->s, _n->len);
	ptr->name.len = _n->len;

	if (new_udomain(&(ptr->name), 512, &(ptr->d)) < 0) {
		LOG(L_ERR, "new_dlist(): Error while creating domain structure\n");
		pkg_free(ptr->name.s);
		pkg_free(ptr);
		return -3;
	}

	*_d = ptr;
	return 0;
}


/*
 * Function registers a new domain with usrloc
 * if the domain exists, pointer to existing structure
 * will be returned, otherwise a new domain will be
 * created
 */
int register_udomain(const char* _n, udomain_t** _d)
{
	dlist_t* d;
	str s;

	s.s = (char*)_n;
	s.len = strlen(_n);

	if (find_dlist(&s, &d) == 0) {
	        *_d = d->d;
		return 0;
	}
	
	if (new_dlist(&s, &d) < 0) {
		LOG(L_ERR, "register_udomain(): Error while creating new domain\n");
		return -1;
	}

	d->next = root;
	root = d;
	
	*_d = d->d;
	return 0;
}


/*
 * Free all registered domains
 */
void free_all_udomains(void)
{
	dlist_t* ptr;

	while(root) {
		ptr = root;
		root = root->next;
	
		free_udomain(ptr->d);
		pkg_free(ptr->name.s);
		pkg_free(ptr);
	}

	if (db) db_close(db);
}


/*
 * Just for debugging
 */
void print_all_udomains(void)
{
	dlist_t* ptr;
	
	ptr = root;

	printf("===Domain list===\n");
	while(ptr) {
		print_udomain(ptr->d);
		ptr = ptr->next;
	}
	printf("===/Domain list===\n");
}


/*
 * Run timer handler of all domains
 */
int timer_handler(void)
{
	int res = 0;
	dlist_t* ptr;

	get_act_time(); /* Get and save actual time */

	ptr = root;
	while(ptr) {
		res |= timer_udomain(ptr->d);
		ptr = ptr->next;
	}
	
	return res;
}


/*
 * Preload content of all domains from database
 */
int preload_all_udomains(void)
{
	dlist_t* ptr;
	int res = 0;
	
	ptr = root;
	while(ptr) {
		res |= preload_udomain(ptr->d);
		ptr = ptr->next;
	}

	return res;
}
