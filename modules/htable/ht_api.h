/**
 *
 * Copyright (C) 2008 Elena-Ramona Modroiu (asipto.com)
 *
 * This file is part of kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
		       
#ifndef _HT_API_H_
#define _HT_API_H_

#include <time.h>

#include "../../usr_avp.h"
#include "../../locking.h"
#include "../../pvar.h"
#include "../../atomic_ops.h"

#define ht_compute_hash(_s)        core_case_hash(_s,0,0)
#define ht_get_entry(_h,_size)    (_h)&((_size)-1)

typedef struct _ht_cell
{
    unsigned int cellid;
    unsigned int msize;
	int flags;
	str name;
	int_str value;
	time_t  expire;
    struct _ht_cell *prev;
    struct _ht_cell *next;
} ht_cell_t;

typedef struct _ht_entry
{
	unsigned int esize;  /* number of items in the slot */
	ht_cell_t *first;    /* first item in the slot */
	gen_lock_t lock;     /* mutex to access items in the slot */
	atomic_t locker_pid; /* pid of the process that holds the lock */
	int rec_lock_level;  /* recursive lock count */
} ht_entry_t;

typedef struct _ht
{
	str name;
	unsigned int htid;
	unsigned int htexpire;
	str dbtable;
	int dbmode;
	int flags;
	int_str initval;
	int updateexpire;
	unsigned int htsize;
	int dmqreplicate;
	int evrt_expired;
	ht_entry_t *entries;
	struct _ht *next;
} ht_t;

typedef struct _ht_pv {
	str htname;
	ht_t *ht;
	pv_elem_t *pve;
} ht_pv_t, *ht_pv_p;

int ht_add_table(str *name, int autoexp, str *dbtable, int size, int dbmode,
		int itype, int_str *ival, int updateexpire, int dmqreplicate);
int ht_init_tables(void);
int ht_destroy(void);
int ht_set_cell(ht_t *ht, str *name, int type, int_str *val, int mode);
int ht_del_cell(ht_t *ht, str *name);
ht_cell_t* ht_cell_value_add(ht_t *ht, str *name, int val, int mode,
		ht_cell_t *old);

int ht_dbg(void);
ht_cell_t* ht_cell_pkg_copy(ht_t *ht, str *name, ht_cell_t *old);
int ht_cell_pkg_free(ht_cell_t *cell);
int ht_cell_free(ht_cell_t *cell);

int ht_table_spec(char *spec);
ht_t* ht_get_table(str *name);
int ht_db_load_tables(void);
int ht_db_sync_tables(void);

int ht_has_autoexpire(void);
void ht_timer(unsigned int ticks, void *param);
void ht_handle_expired_record(ht_t *ht, ht_cell_t *cell);
void ht_expired_run_event_route(int routeid);
int ht_set_cell_expire(ht_t *ht, str *name, int type, int_str *val);
int ht_get_cell_expire(ht_t *ht, str *name, unsigned int *val);

int ht_rm_cell_re(str *sre, ht_t *ht, int mode);
int ht_count_cells_re(str *sre, ht_t *ht, int mode);
ht_t *ht_get_root(void);
int ht_reset_content(ht_t *ht);

void ht_iterator_init(void);
int ht_iterator_start(str *iname, str *hname);
int ht_iterator_next(str *iname);
int ht_iterator_end(str *iname);
ht_cell_t* ht_iterator_get_current(str *iname);

void ht_slot_lock(ht_t *ht, int idx);
void ht_slot_unlock(ht_t *ht, int idx);
#endif
