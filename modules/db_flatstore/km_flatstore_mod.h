/* 
 * $Id$ 
 *
 * Flatstore module interface
 *
 * Copyright (C) 2004 FhG Fokus
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
 * History:
 * --------
 *  2003-03-11  updated to the new module exports interface (andrei)
 *  2003-03-16  flags export parameter added (janakj)
 */

#ifndef KM_FLATSTORE_MOD_H
#define KM_FLATSTORE_MOD_H

#include <time.h>

#include "../../lib/srdb1/db.h"

/*
 * Process number used in filenames
 */
extern int km_flat_pid;


/*
 * Delmiter delimiting columns
 */
extern char* km_flat_delimiter;


/*
 * The timestamp of log rotation request from
 * the FIFO interface
 */
extern time_t* km_flat_rotate;


/*
 * Local timestamp marking the time of the
 * last log rotation in the process
 */
extern time_t km_local_timestamp;

int km_mod_init(void);

void km_mod_destroy(void);

int km_child_init(int rank);

int db_flat_bind_api(db_func_t *dbb);


#endif /* KM_FLATSTORE_MOD_H */
