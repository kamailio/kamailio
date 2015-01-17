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

#ifndef _DB_RES_H
#define _DB_RES_H  1

/** \ingroup DB_API 
 * @{ 
 */

#include "db_gen.h"
#include "db_rec.h"
#include "db_cmd.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct db_res {
	db_gen_t gen;           /* Generic part of the structure */
	unsigned int field_count;    /* Number of fields in the result */
	struct db_rec* cur_rec; /* Currently active record in the result */
	struct db_cmd* cmd;     /* Command that produced the result */
} db_res_t;

struct db_res* db_res(struct db_cmd* cmd);
void db_res_free(struct db_res* res);

struct db_rec* db_first(struct db_res* res);

struct db_rec* db_next(struct db_res* res);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* _DB_RES_H */
