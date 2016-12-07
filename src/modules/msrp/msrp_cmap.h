/**
 * Copyright (C) 2013 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _MSRP_CMAP_H_
#define _MSRP_CMAP_H_

#include <time.h>

#include "../../str.h"
#include "../../locking.h"

#include "msrp_parser.h"

typedef struct _msrp_citem
{
    unsigned int citemid;
	str sessionid;
	str peer;
	str addr;
	str sock;
	int conid;
	int cflags;
	time_t  expires;
    struct _msrp_citem *prev;
    struct _msrp_citem *next;
} msrp_citem_t;

typedef struct _msrp_centry
{
	unsigned int lsize;
	msrp_citem_t *first;
	gen_lock_t lock;	
} msrp_centry_t;

typedef struct _msrp_cmap
{
	unsigned int mapexpire;
	unsigned int mapsize;
	msrp_centry_t *cslots;
	struct _msrp_cmap *next;
} msrp_cmap_t;

int msrp_cmap_init(int msize);
int msrp_cmap_destroy(void);
int msrp_cmap_clean(void);

int msrp_cmap_save(msrp_frame_t *mf);
int msrp_cmap_lookup(msrp_frame_t *mf);

int msrp_sruid_init(void);

int msrp_cmap_init_rpc(void);
#endif
