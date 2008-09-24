/**
 * $Id$
 *
 * Copyright (C) 2008 Elena-Ramona Modroiu (asipto.com)
 *
 * This file is part of kamailio, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
		       
#ifndef _HT_API_H_
#define _HT_API_H_

#include "../../usr_avp.h"
#include "../../locking.h"

typedef struct _ht_cell
{
    unsigned int cellid;
    unsigned int msize;
	int flags;
	str name;
	int_str value;
    struct _ht_cell *prev;
    struct _ht_cell *next;
} ht_cell_t;

typedef struct _ht_entry
{
	unsigned int esize;
	ht_cell_t *first;
	gen_lock_t lock;	
} ht_entry_t;

typedef struct _ht
{
	unsigned int htsize;
	ht_entry_t *entries;
} ht_t;

int ht_init(int size);
int ht_destroy();
int ht_set_cell(str *name, int type, int_str *val);
int ht_del_cell(str *name);

int ht_dbg(void);
ht_cell_t* ht_cell_pkg_copy(str *name, ht_cell_t *old);
int ht_cell_pkg_free(ht_cell_t *cell);

#endif
