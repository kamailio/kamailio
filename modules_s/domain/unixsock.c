/*
 * $Id$
 *
 * UNIX Socket Interface
 *
 * Copyright (C) 2002-2004 FhG FOKUS
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "hash.h"
#include "../../unixsock_server.h"
#include "../../dprint.h"
#include "../../db/db.h"
#include "../../ut.h"
#include "unixsock.h"
#include "fifo.h" /* Because of reload_domain_table */

/* FIXME: Check for value of db_mode and return immediately if 0 */

/*
 * Fifo function to reload domain table
 */
static int domain_reload(str* msg)
{
	if (reload_domain_list() < 0) {
		unixsock_reply_asciiz("400 Domain table reload failed\n");
	} else {
		unixsock_reply_asciiz("200 OK\n");
	}
	unixsock_reply_send();
	return 0;
}


static void dump_domain_unx(domain_t* d)
{
	int i;
	avp_t* a;
	str* name;
	int_str val;

	unixsock_reply_printf("did: %.*s\n", d->did.len, d->did.s);
	unixsock_reply_asciiz("  domains: ");
	for(i = 0; i < d->n; i++) {
		unixsock_reply_printf("%.*s (%u)", d->domain[i].len, d->domain[i].s, d->flags[i]);
		if (i < d->n-1) unixsock_reply_asciiz(", ");
	}
	unixsock_reply_asciiz("\n");
	unixsock_reply_asciiz("  attrs: ");
	a = d->attrs;
	while(a) {
		name = get_avp_name(a);
		get_avp_val(a, &val);
		unixsock_reply_printf("%.*s", name->len, name->s);
		if (a->flags & AVP_VAL_STR) {
			if (val.s.len && val.s.s) {
				unixsock_reply_printf("=\"%.*s\"", val.s.len, val.s.s);
			}
		} else {
			unixsock_reply_printf("=%d", val.n);
		}
		if (a->next) unixsock_reply_asciiz(", ");
		a = a->next;
	}
}


static void dump_domain_list_unx(domain_t* list)
{
	while(list) {
		dump_domain_unx(list);
		unixsock_reply_asciiz("\n");
		list = list->next;
	}
}


/*
 * Fifo function to print domains from current hash table
 */
static int domain_dump(str* msg)
{
	domain_t* list;
	if (*active_hash == hash_1) list = *domains_1;
	else list = *domains_2;

	unixsock_reply_asciiz("200 OK\n");
	dump_domain_list_unx(list);
	unixsock_reply_send();
	return 0;
}


/*
 * Register domain unixsock functions
 */
int init_domain_unixsock(void) 
{
	if (unixsock_register_cmd("domain_reload", domain_reload) < 0) {
		LOG(L_ERR, "domain:init_domain_unixsock: Cannot register domain_reload\n");
		return -1;
	}

	if (unixsock_register_cmd("domain_dump", domain_dump) < 0) {
		LOG(L_ERR, "init_domain_unixsock: Cannot register domain_dump\n");
		return -1;
	}

	return 0;
}
