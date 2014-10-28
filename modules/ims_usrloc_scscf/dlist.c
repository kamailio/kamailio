/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * 
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by 
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an 
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 * 
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Fokus. 
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to 
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
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

#include "dlist.h"
#include <stdlib.h>	       /* abort */
#include <string.h>            /* strlen, memcmp */
#include <stdio.h>             /* printf */
#include "../../ut.h"
#include "../../lib/srdb1/db_ut.h"
#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../ip_addr.h"
#include "../../socket_info.h"
#include "udomain.h"           /* new_udomain, free_udomain */
#include "usrloc.h"
#include "utime.h"
#include "ul_mod.h"


/*! \brief Global list of all registered domains */
dlist_t* root = 0;


/*!
 * \brief Find domain with the given name
 * \param _n domain name
 * \param _d pointer to domain
 * \return 0 if the domain was found and 1 of not
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

/*!
 * \brief Get all contacts from the memory, in partitions if wanted
 * \see get_all_ucontacts
 * \param buf target buffer
 * \param len length of buffer
 * \param flags contact flags
 * \param part_idx part index
 * \param part_max maximal part
 * \return 0 on success, positive if buffer size was not sufficient, negative on failure
 */
static inline int get_all_mem_ucontacts(void *buf, int len, unsigned int flags,
	unsigned int part_idx, unsigned int part_max) {
    dlist_t *p;
    impurecord_t *r;
    ucontact_t *c;
    void *cp;
    int shortage;
    int needed;
    int i,j;
    cp = buf;
    shortage = 0;
    /* Reserve space for terminating 0000 */
    len -= sizeof (c->c.len);

    for (p = root; p != NULL; p = p->next) {

	for (i = 0; i < p->d->size; i++) {

	    if ((i % part_max) != part_idx)
		continue;
	    LM_DBG("LOCKING ULSLOT %d\n", i);
	    lock_ulslot(p->d, i);
	    if (p->d->table[i].n <= 0) {
	    	LM_DBG("UNLOCKING ULSLOT %d\n", i);
		unlock_ulslot(p->d, i);
		continue;
	    }
	    for (r = p->d->table[i].first; r != NULL; r = r->next) {
		while (j<MAX_CONTACTS_PER_IMPU && (c = r->newcontacts[j++])) {
		    if (c->c.len <= 0)
			continue;
		    /*
		     * List only contacts that have all requested
		     * flags set
		     */
		    if ((c->cflags & flags) != flags)
			continue;
		    if (c->received.s) {
			needed = (int) (sizeof (c->received.len)
				+ c->received.len + sizeof (c->sock)
				+ sizeof (c->cflags) + sizeof (c->path.len)
				+ c->path.len);
			if (len >= needed) {
			    memcpy(cp, &c->received.len, sizeof (c->received.len));
			    cp = (char*) cp + sizeof (c->received.len);
			    memcpy(cp, c->received.s, c->received.len);
			    cp = (char*) cp + c->received.len;
			    memcpy(cp, &c->sock, sizeof (c->sock));
			    cp = (char*) cp + sizeof (c->sock);
			    memcpy(cp, &c->cflags, sizeof (c->cflags));
			    cp = (char*) cp + sizeof (c->cflags);
			    memcpy(cp, &c->path.len, sizeof (c->path.len));
			    cp = (char*) cp + sizeof (c->path.len);
			    memcpy(cp, c->path.s, c->path.len);
			    cp = (char*) cp + c->path.len;
			    len -= needed;
			} else {
			    shortage += needed;
			}
		    } else {
			needed = (int) (sizeof (c->c.len) + c->c.len +
				sizeof (c->sock) + sizeof (c->cflags) +
				sizeof (c->path.len) + c->path.len);
			if (len >= needed) {
			    memcpy(cp, &c->c.len, sizeof (c->c.len));
			    cp = (char*) cp + sizeof (c->c.len);
			    memcpy(cp, c->c.s, c->c.len);
			    cp = (char*) cp + c->c.len;
			    memcpy(cp, &c->sock, sizeof (c->sock));
			    cp = (char*) cp + sizeof (c->sock);
			    memcpy(cp, &c->cflags, sizeof (c->cflags));
			    cp = (char*) cp + sizeof (c->cflags);
			    memcpy(cp, &c->path.len, sizeof (c->path.len));
			    cp = (char*) cp + sizeof (c->path.len);
			    memcpy(cp, c->path.s, c->path.len);
			    cp = (char*) cp + c->path.len;
			    len -= needed;
			} else {
			    shortage += needed;
			}
		    }
		}
	    }
#ifdef EXTRA_DEBUG
	    LM_DBG("UN-LOCKING ULSLOT %d\n", i);
#endif
	    unlock_ulslot(p->d, i);
	}
    }
    /* len < 0 is possible, if size of the buffer < sizeof(c->c.len) */
    if (len >= 0)
	memset(cp, 0, sizeof (c->c.len));

    /* Shouldn't happen */
    if (shortage > 0 && len > shortage) {
	abort();
    }

    shortage -= len;

    return shortage > 0 ? shortage : 0;
}



/*!
 * \brief Get all contacts from the usrloc, in partitions if wanted
 *
 * Return list of all contacts for all currently registered
 * users in all domains. The caller must provide buffer of
 * sufficient length for fitting all those contacts. In the
 * case when buffer was exhausted, the function returns
 * estimated amount of additional space needed, in this
 * case the caller is expected to repeat the call using
 * this value as the hint.
 *
 * Information is packed into the buffer as follows:
 *
 * +------------+----------+-----+------+-----+
 * |contact1.len|contact1.s|sock1|flags1|path1|
 * +------------+----------+-----+------+-----+
 * |contact2.len|contact2.s|sock2|flags2|path1|
 * +------------+----------+-----+------+-----+
 * |..........................................|
 * +------------+----------+-----+------+-----+
 * |contactN.len|contactN.s|sockN|flagsN|pathN|
 * +------------+----------+-----+------+-----+
 * |000000000000|
 * +------------+
 *
 * \param buf target buffer
 * \param len length of buffer
 * \param flags contact flags
 * \param part_idx part index
 * \param part_max maximal part
 * \return 0 on success, positive if buffer size was not sufficient, negative on failure
 */
int get_all_ucontacts(void *buf, int len, unsigned int flags,
								unsigned int part_idx, unsigned int part_max)
{
	return get_all_mem_ucontacts( buf, len, flags, part_idx, part_max);
}



/*!
 * \brief Create a new domain structure
 * \return 0 if everything went OK, otherwise value < 0 is returned
 *
 * \note The structure is NOT created in shared memory so the
 * function must be called before the server forks if it should
 * be available to all processes
 */
static inline int new_dlist(str* _n, dlist_t** _d)
{
	dlist_t* ptr;

	/* Domains are created before ser forks,
	 * so we can create them using pkg_malloc
	 */
	ptr = (dlist_t*)shm_malloc(sizeof(dlist_t));
	if (ptr == 0) {
		LM_ERR("no more share memory\n");
		return -1;
	}
	memset(ptr, 0, sizeof(dlist_t));

	/* copy domain name as null terminated string */
	ptr->name.s = (char*)shm_malloc(_n->len+1);
	if (ptr->name.s == 0) {
		LM_ERR("no more memory left\n");
		shm_free(ptr);
		return -2;
	}

	memcpy(ptr->name.s, _n->s, _n->len);
	ptr->name.len = _n->len;
	ptr->name.s[ptr->name.len] = 0;

	if (new_udomain(&(ptr->name), ul_hash_size, &(ptr->d)) < 0) {
		LM_ERR("creating domain structure failed\n");
		shm_free(ptr->name.s);
		shm_free(ptr);
		return -3;
	}

	*_d = ptr;
	return 0;
}

/*!
 * \brief Registers a new domain with usrloc
 *
 * Find and return a usrloc domain (location table)
 * \param _n domain name
 * \param _d usrloc domain
 * \return 0 on success, -1 on failure
 */
int get_udomain(const char* _n, udomain_t** _d)
{
	dlist_t* d;
	str s;

	s.s = (char*)_n;
	s.len = strlen(_n);

	if (find_dlist(&s, &d) == 0) {
		*_d = d->d;
		return 0;
	}
	*_d = NULL;
	return -1;
}

/*!
 * \brief Registers a new domain with usrloc
 *
 * Registers a new domain with usrloc. If the domain exists,
 * a pointer to existing structure will be returned, otherwise
 * a new domain will be created
 * \param _n domain name
 * \param _d new created domain
 * \return 0 on success, -1 on failure
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
		LM_ERR("failed to create new domain\n");
		return -1;
	}

	d->next = root;
	root = d;
	
	*_d = d->d;
	return 0;
}


/*!
 * \brief Free all allocated memory for domains
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


/*!
 * \brief Print all domains, just for debugging
 * \param _f output file
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


/*!
 * \brief Loops through all domains summing up the number of users
 * \return the number of users, could be zero
 */
unsigned long get_number_of_users(void)
{
	long numberOfUsers = 0;

	dlist_t* current_dlist;
	
	current_dlist = root;

	while (current_dlist)
	{
		numberOfUsers += get_stat_val(current_dlist->d->users); 
		current_dlist  = current_dlist->next;
	}

	return numberOfUsers;
}


/*!
 * \brief Run timer handler of all domains
 * \return 0 if all timer return 0, != 0 otherwise
 */
int synchronize_all_udomains(void)
{
	int res = 0;
	dlist_t* ptr;

	get_act_time(); /* Get and save actual time */

	for( ptr=root ; ptr ; ptr=ptr->next)
		mem_timer_udomain(ptr->d);

	return res;
}


/*!
 * \brief Find a particular domain, small wrapper around find_dlist
 * \param _d domain name
 * \param _p pointer to domain if found
 * \return 1 if domain was found, 0 otherwise
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
