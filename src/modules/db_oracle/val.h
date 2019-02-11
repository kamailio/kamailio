/*
 * $Id$
 *
 * Copyright (C) 2007,2008 TRUNK MOBILE
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
 */

#ifndef VAL_H
#define VAL_H

#include <oci.h>
#include "../../lib/srdb1/db_val.h"
#include "../../lib/srdb1/db.h"

struct bmap_t {
    dvoid *addr;
    ub4	size;
    ub2	type;
};
typedef struct bmap_t bmap_t;

/*
 * Convert value to sql-string as db bind index
 */
int db_oracle_val2str(const db1_con_t* _con, const db_val_t* _v, char* _s, int* _len);

/*
 * Called after val2str to realy binding
 */
int db_oracle_val2bind(bmap_t* _m, const db_val_t* _v, OCIDate* _o);

#endif /* VAL_H */
