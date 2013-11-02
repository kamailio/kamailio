/*
 * $Id$
 *
 * TLS module - common functions
 *
 * Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2004,2005 Free Software Foundation, Inc.
 * Copyright (C) 2005 iptelorg GmbH
 *
 * This file is part of SIP-router, a free SIP server.
 *
 * SIP-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define _GNU_SOURCE 1 /* Needed for strndup */

#include <string.h>
#include <libgen.h>
#include "../../mem/shm_mem.h"
#include "../../globals.h"
#include "../../dprint.h"
#include "tls_mod.h"
#include "tls_util.h"
/*!
 * \file
 * \brief SIP-router TLS support :: Common functions
 * \ingroup tls
 * Module: \ref tls
 */



/*
 * Make a shared memory copy of ASCII zero terminated string
 * Return value: -1 on error
 *                0 on success
 */
int shm_asciiz_dup(char** dest, char* val)
{
	char* ret;
	int len;

	if (!val) {
		*dest = NULL;
		return 0;
	}

	len = strlen(val);
	ret = shm_malloc(len + 1);
	if (!ret) {
		ERR("No memory left\n");
		return -1;
	}
	memcpy(ret, val, len + 1);
	*dest = ret;
        return 0;
}


/*
 * Delete old TLS configuration that is not needed anymore
 */
void collect_garbage(void)
{
	tls_domains_cfg_t* prev, *cur;

	     /* Make sure we do not run two garbage collectors
	      * at the same time
	      */
	lock_get(tls_domains_cfg_lock);

	     /* Skip the current configuration, garbage starts
	      * with the 2nd element on the list
	      */
	prev = *tls_domains_cfg;
	cur = (*tls_domains_cfg)->next;

	while(cur) {
		if (cur->ref_count == 0) {
			     /* Not referenced by any existing connection */
			prev->next = cur->next;
			tls_free_cfg(cur);
		}

		prev = cur;
		cur = cur->next;
	}

	lock_release(tls_domains_cfg_lock);
}

