/*
 * $Id$
 *
 * List of registered domains
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
#include <stdlib.h>	       /* abort */
#include <string.h>            /* strlen, memcmp */
#include <stdio.h>             /* printf */
#include "../../ut.h"
#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../globals.h"
#include "udomain.h"           /* new_udomain, free_udomain */
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
 * Return list of all contacts for all currently registered
 * users in all domains. Caller must provide buffer of
 * sufficient length for fitting all those contacts. In the
 * case when buffer was exhausted, the function returns
 * estimated amount of additional space needed, in this
 * case the caller is expected to repeat the call using
 * this value as the hint.
 *
 * Information is packed into the buffer as follows:
 *
 * +------------+----------+-----+------------+----------+-----+
 * |contact1.len|contact1.s|sock1|contact2.len|contact2.s|sock2|
 * +------------+----------+-----+------------+----------+-----+
 * |.............................|contactN.len|contactN.s|sockN|
 * +------------+----------+-----+------------+----------+-----+
 * |000000000000|
 * +------------+
 */
int get_all_ucontacts(void *buf, int len, unsigned int flags)
{
	dlist_t *p;
	urecord_t *r;
	ucontact_t *c;
	void *cp;
	int shortage;

	cp = buf;
	shortage = 0;
	/* Reserve space for terminating 0000 */
	len -= sizeof(c->c.len);
	for (p = root; p != NULL; p = p->next) {
		lock_udomain(p->d);
		if (p->d->d_ll.n <= 0) {
			unlock_udomain(p->d);
			continue;
		}
		for (r = p->d->d_ll.first; r != NULL; r = r->d_ll.next) {
			for (c = r->contacts; c != NULL; c = c->next) {
				if (c->c.len <= 0)
					continue;
				     /*
				      * List only contacts that have all requested
				      * flags set
				      */
				if ((c->flags & flags) != flags)
					continue;

				/* List only contacts with matching server id */
				if (c->server_id != server_id)
					continue;

				if (c->received.s) {
					if (len >= (int)(sizeof(c->received.len) +
							 c->received.len + sizeof(c->sock))) {
						memcpy(cp, &c->received.len, sizeof(c->received.len));
						cp = (char*)cp + sizeof(c->received.len);
						memcpy(cp, c->received.s, c->received.len);
						cp = (char*)cp + c->received.len;
						memcpy(cp, &c->sock, sizeof(c->sock));
						cp = (char*)cp + sizeof(c->sock);
						len -= sizeof(c->received.len) + c->received.len +
							sizeof(c->sock);
					} else {
						shortage += sizeof(c->received.len) +
							c->received.len + sizeof(c->sock);
					}
				} else {
					if (len >= (int)(sizeof(c->c.len) + c->c.len +
					sizeof(c->sock))) {
						memcpy(cp, &c->c.len, sizeof(c->c.len));
						cp = (char*)cp + sizeof(c->c.len);
						memcpy(cp, c->c.s, c->c.len);
						cp = (char*)cp + c->c.len;
						memcpy(cp, &c->sock, sizeof(c->sock));
						cp = (char*)cp + sizeof(c->sock);
						len -= sizeof(c->c.len) + c->c.len + sizeof(c->sock);
					} else {
						shortage += sizeof(c->c.len) + c->c.len +
							sizeof(c->sock);

					}
				}
			}
		}
		unlock_udomain(p->d);
	}
	/* len < 0 is possible, if size of the buffer < sizeof(c->c.len) */
	if (len >= 0)
		memset(cp, 0, sizeof(c->c.len));

	/* Shouldn't happen */
	if (shortage > 0 && len > shortage) {
		abort();
	}

	shortage -= len;

	return shortage > 0 ? shortage : 0;
}

/*
 * Create a new domain structure
 * Returns 0 if everything went OK, otherwise value < 0
 * is returned
 */
static inline int new_dlist(str* _n, dlist_t** _d)
{
	dlist_t* ptr;

	ptr = (dlist_t*)shm_malloc(sizeof(dlist_t));
	if (ptr == 0) {
		LOG(L_ERR, "new_dlist(): No memory left\n");
		return -1;
	}
	memset(ptr, 0, sizeof(dlist_t));

	ptr->name.s = (char*)shm_malloc(_n->len + 1);
	if (ptr->name.s == 0) {
		LOG(L_ERR, "new_dlist(): No memory left 2\n");
		shm_free(ptr);
		return -2;
	}

	memcpy(ptr->name.s, _n->s, _n->len);
	ptr->name.s[_n->len] = '\0';
	ptr->name.len = _n->len;

	if (new_udomain(&(ptr->name), &(ptr->d)) < 0) {
		LOG(L_ERR, "new_dlist(): Error while creating domain structure\n");
		shm_free(ptr->name.s);
		shm_free(ptr);
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
	
	/* Preload domain with data from database if we are gonna
	 * to use database
	 */
	if (db_mode != NO_DB) {
		db = db_ctx("usrloc");
		if (db == NULL) {
			ERR("Error while initializing database layer\n");
			goto err;
		}
		
		if (db_add_db(db, db_url.s) < 0) goto err;
		if (db_connect(db) < 0) goto err;
		
		if (preload_udomain(d->d) < 0) {
			LOG(L_ERR, "register_udomain(): Error while preloading domain '%.*s'\n",
			    s.len, ZSW(s.s));
			goto err;
		}

		db_disconnect(db);
		db_ctx_free(db);
		db = NULL;
	}

	d->next = root;
	root = d;
	
	*_d = d->d;
	return 0;

 err:
	if (db) {
		db_disconnect(db);
		db_ctx_free(db);
		db = NULL;
	}
	free_udomain(d->d);
	shm_free(d->name.s);
	shm_free(d);
	return -1;
}


/*
 * Free all allocated memory
 */
void free_all_udomains(void)
{
	dlist_t* ptr;

	while(root) {
		ptr = root;
		root = root->next;

		free_udomain(ptr->d);
		shm_free(ptr->name.s);
		shm_free(ptr);
	}
}


/*
 * Just for debugging
 */
void print_all_udomains(FILE* _f)
{
	dlist_t* ptr;
	
	ptr = root;

	fprintf(_f, "===Domain list===\n");
	while(ptr) {
		print_udomain(_f, ptr->d);
		ptr = ptr->next;
	}
	fprintf(_f, "===/Domain list===\n");
}


/*
 * Run timer handler of all domains
 */
int synchronize_all_udomains(void)
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
 * Find a particular domain
 */
int find_domain(str* _d, udomain_t** _p)
{
	dlist_t* d;

	if (find_dlist(_d, &d) == 0) {
	        *_p = d->d;
		return 0;
	}

	return 1;
}
