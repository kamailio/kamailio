/*
 * $Id: dlist.c 5160 2008-11-03 17:51:22Z henningw $
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * ========
 * 2006-11-28 added get_number_of_users() (Jeffrey Magder - SOMA Networks)
 * 2007-09-12 added partitioning support for fetching all ul contacts
 *            (bogdan)
 */

/*! \file
 *  \brief USRLOC - List of registered domains
 *  \ingroup usrloc
 *
 * - Module: \ref usrloc
 */


#include "dlist.h"
#include <stdlib.h>	       /* abort */
#include <string.h>            /* strlen, memcmp */
#include <stdio.h>             /* printf */
#include "../../ut.h"
#include "../../lib/srdb1/db.h"
#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "../../ip_addr.h"
#include "../../socket_info.h"
#include "udomain.h"           /* new_udomain, free_udomain */
#include "utime.h"
#include "ul_mod.h"

#include "ul_db_layer.h"

static struct domain_list_item *domain_list;



static inline struct domain_list_item * find_dlist (str *name) {
	struct domain_list_item *item;

	for (item = domain_list; item != NULL; item = item->next) {
		if (item->name.len == name->len
		        && memcmp (item->name.s, name->s, name->len) == 0) {
			return item;
		}
	}
	return NULL;
}




static inline struct domain_list_item * add_to_dlist (str *name, int type) {
	struct domain_list_item *item;

	item = (struct domain_list_item *)
	       pkg_malloc (sizeof (struct domain_list_item));
	if (item == NULL) {
		LM_ERR("Out of shared memory.\n");
		return NULL;
	}
	item->name.s = (char *) pkg_malloc (name->len + 1);
	if (item->name.s == NULL) {
		LM_ERR("Out of shared memory.\n");
		return NULL;
	}
	memcpy (item->name.s, name->s, name->len);
	item->name.s[name->len] = '\0';
	item->name.len = name->len;

	memset (&item->domain, 0, sizeof (struct udomain));
	item->domain.name = &item->name;
	item->domain.dbt = type;
	/* Everything else is not useful for now.  */

	item->next = domain_list;
	domain_list = item;

	return item;
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
int register_udomain(const char *name, udomain_t **domain) {
	struct domain_list_item *item;
	str name_str;
	ul_domain_db_t * d;

	name_str.s = (char *) name;
	name_str.len = strlen (name);
	item = find_dlist (&name_str);
	if (item == NULL) {
		if((d = ul_find_domain(name)) == NULL){
			LM_ERR("domain %s not found.\n", name);
			return -1;
		}
		item = add_to_dlist (&name_str, d->dbt);
	}
	if (item == NULL) {
		return -1;
	}
	*domain = &item->domain;
	LM_DBG("found domain %.*s, type: %s\n", (*domain)->name->len, (*domain)->name->s, (((*domain)->dbt) == DB_TYPE_CLUSTER ? "cluster" : "single"));
	return 0;
}




/*!
 * \brief Loops through all domains summing up the number of users
 * \return the number of users, could be zero
 */
unsigned long get_number_of_users(void)
{
	int numberOfUsers = 0;
	LM_INFO("not available with sp-ul_db interface");
	return numberOfUsers;
}


/*!
 * \brief Run timer handler of all domains
 * \return 0 if all timer return 0, != 0 otherwise
 */
int synchronize_all_udomains(void)
{
	int res = 0;
	LM_INFO("not available with sp-ul_db interface");
	return res;
}


