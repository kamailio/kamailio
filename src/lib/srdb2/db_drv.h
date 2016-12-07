/* 
 * Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2006-2007 iptelorg GmbH
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

#ifndef _DB_DRV_H
#define _DB_DRV_H  1

/** \ingroup DB_API 
 * @{ 
 */

#include "db_gen.h"

#include "../../str.h"
#include "../../list.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct db_drv;
struct db_gen;

typedef void db_drv_free_t(struct db_gen* db_struct, struct db_drv* payload);


/*
 * Generic function exported by DB driver modules
 */
typedef int (*db_drv_func_t)(void* db_struct, ...);


/* Template for driver specific data that can be attached to various
 * DB API structure and where drivers can store driver-specific data
 */
typedef struct db_drv {
	db_drv_free_t* free;  /* Drivers must provide this function */
} db_drv_t;


int db_drv_init(struct db_drv* ptr, void* free_func);

void db_drv_free(struct db_drv* ptr);

/*
 * Find function with given name in module named <module>
 * RETURNS: -1 on error
 *           0 if a function has been found
 *           1 if no function has been found
 */
int db_drv_func(db_drv_func_t* func, str* module, char* func_name);

/*
 * Call function with name <func_name> in DB driver <module>, give
 * it pointer <db_struct> as the pointer to the corresponding DB structure
 * (type of the structure is function-specific) and <offset> is the offset
 * of the driver/connection within the context (used to make DB_DRV_ATTACH 
 * and DB_DRV_DATA macros work)
 */
int db_drv_call(str* module, char* func_name, void* db_struct, int offset);


#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* _DB_DRV_H */
