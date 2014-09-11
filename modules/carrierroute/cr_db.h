/*
 * $Id$
 *
 * Copyright (C) 2007 1&1 Internet AG
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*!
 * \file cr_db.h 
 * \brief Functions for loading routing data from a database.
 * \ingroup carrierroute
 * - Module; \ref carrierroute
 */

#ifndef CR_DB_H
#define CR_DB_H

#include "../../lib/srdb1/db.h"
#include "db_carrierroute.h"
#include "cr_data.h"


#define COLUMN_NUM 12
#define COL_ID             0
#define COL_CARRIER        1
#define COL_DOMAIN         2
#define COL_SCAN_PREFIX    3
#define COL_FLAGS          4
#define COL_MASK           5
#define COL_PROB           6
#define COL_REWRITE_HOST   7
#define COL_STRIP          8
#define COL_REWRITE_PREFIX 9
#define COL_REWRITE_SUFFIX 10
#define COL_COMMENT        11

#define FAILURE_COLUMN_NUM 10
#define FCOL_ID             0
#define FCOL_CARRIER        1
#define FCOL_DOMAIN         2
#define FCOL_SCAN_PREFIX    3
#define FCOL_HOST_NAME      4
#define FCOL_REPLY_CODE     5
#define FCOL_FLAGS          6
#define FCOL_MASK           7
#define FCOL_NEXT_DOMAIN    8
#define FCOL_COMMENT        9

#define CARRIER_NAME_COLUMN_NUM 2
#define CARRIER_NAME_ID_COL 0
#define CARRIER_NAME_NAME_COL 1

#define DOMAIN_NAME_COLUMN_NUM 2
#define DOMAIN_NAME_ID_COL 0
#define DOMAIN_NAME_NAME_COL 1

extern str * columns[];
extern str * carrier_columns[];
extern str * failure_columns[];


/**
 * Loads the routing data from the database given in global
 * variable db_url and stores it in routing tree rd.
 *
 * @param rd Pointer to the route data tree where the routing data
 * shall be loaded into
 *
 * @return 0 means ok, -1 means an error occured
 *
 */
int load_route_data_db (struct route_data_t * rd);

int load_user_carrier(str * user, str * domain);

#endif
