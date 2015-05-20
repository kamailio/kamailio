/*
 * DBText library
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 */


#ifndef _DBT_RAW_QUERY_H_
#define _DBT_RAW_QUERY_H_

#include "../../str.h"



int dbt_raw_query_select(db1_con_t* _h, str* _s, db1_res_t** _r);
int dbt_raw_query_update(db1_con_t* _h, str* _s, db1_res_t** _r);
int dbt_raw_query_delete(db1_con_t* _h, str* _s, db1_res_t** _r);

#endif

