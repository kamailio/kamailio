/* 
 * Copyright (C) 2001-2005 FhG FOKUS
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

#ifndef _DB_CMD_H
#define _DB_CMD_H  1

/** \ingroup DB_API 
 * @{ 
 */

/** \file
 * Representation of database commands
 */

#include "db_fld.h"
#include "db_drv.h"
#include "db_gen.h"
#include "db_res.h"
#include "db_rec.h"
#include "db_ctx.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct db_cmd;
struct db_res;
struct db_rec;

typedef int (*db_exec_func_t)(struct db_res* res, struct db_cmd* cmd);
typedef int (*db_first_func_t)(struct db_res* res);
typedef int (*db_next_func_t)(struct db_res* res);


enum db_cmd_type {
	DB_PUT,  /* Insert or update new record in database */
	DB_DEL,  /* Delete all matching records from database */
	DB_GET,  /* Get matching records from database */
	DB_UPD,  /* Update matching records in database */
	DB_SQL,  /* Raw SQL query */
};

/**
 * Structure db_cmd describes command in DB-API
 */
typedef struct db_cmd {
	db_gen_t gen;          /**< Generic part of the structure, must be the 1st attribute */
	enum db_cmd_type type; /**< Type of the command to be executed */
	struct db_ctx* ctx;    /**< Context containing database connections to be used */
	str table;             /**< Name of the table to perform the command on */
	db_exec_func_t exec[DB_PAYLOAD_MAX]; /**< Array of exec functions provided by modules */
	db_first_func_t first[DB_PAYLOAD_MAX];
	db_next_func_t next[DB_PAYLOAD_MAX];
	db_fld_t* result;       /**< Fields to to be returned in result */
	db_fld_t* match;        /**< Fields describing WHERE clause */
	db_fld_t* vals;         /**< Fields with values for UPDATE and INSERT statements */
	unsigned int result_count; /* Number of fields in the result set */
	unsigned int match_count;  /* Number of fields in the result set */
	unsigned int vals_count;   /* Number of fields in the result set */
} db_cmd_t;


#define DB_SET_EXEC(db_cmd, func) do { \
		(db_cmd)->exec[db_payload_idx] = (func); \
} while(0)


struct db_cmd* db_cmd(enum db_cmd_type type, struct db_ctx* ctx, char* table, 
					  db_fld_t* result, db_fld_t* match, db_fld_t* value);
void db_cmd_free(struct db_cmd* cmd);

int db_exec(struct db_res** res, struct db_cmd* cmd);

int db_getopt(db_cmd_t* cmd, char* optname, ...);

int db_setopt(db_cmd_t* cmd, char* optname, ...);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* _DB_CMD_H */
