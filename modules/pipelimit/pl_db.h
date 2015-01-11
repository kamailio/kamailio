/*
 * pipelimit module
 *
 * Copyright (C) 2009 Daniel-Constantin Mierla (asipto.com)
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
 
/*! \file
 * \ingroup pipelimit
 * \brief pipelimit :: pl_db
 */

#ifndef _RL_DB_H_
#define _RL_DB_H_

#include "../../str.h"

extern str pl_db_url;
extern str rlp_pipeid_col;
extern str rlp_limit_col;
extern str rlp_algorithm_col;
extern str rlp_table_name;

int pl_init_db(void);

#endif
