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

#ifndef _DB_H
#define _DB_H  1

/**
 * \defgroup DB_API Database Abstraction Layer
 *
 * @{
 */


#include "db_gen.h"
#include "db_ctx.h"
#include "db_uri.h"
#include "db_cmd.h"
#include "db_res.h"
#include "db_rec.h"
#include "db_fld.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/*
 * Various database flags shared by modules 
 */
#define SRDB_LOAD_SER   (1 << 0)  /* The row should be loaded by SER */
#define SRDB_DISABLED   (1 << 1)  /* The row is disabled */
#define SRDB_CANON      (1 << 2)  /* Canonical entry (domain or uri) */
#define SRDB_IS_TO      (1 << 3)  /* The URI can be used in To */
#define SRDB_IS_FROM    (1 << 4)  /* The URI can be used in From */
#define SRDB_FOR_SERWEB (1 << 5)  /* Credentials instance can be used by serweb */
#define SRDB_PENDING    (1 << 6)
#define SRDB_DELETED    (1 << 7)
#define SRDB_CALLER_DELETED (1 << 8) /* Accounting table */
#define SRDB_CALLEE_DELETED (1 << 9) /* Accounting table */
#define SRDB_MULTIVALUE     (1 << 10) /* Attr_types table */
#define SRDB_FILL_ON_REG    (1 << 11) /* Attr_types table */
#define SRDB_REQUIRED       (1 << 12) /* Attr_types table */
#define SRDB_DIR            (1 << 13) /* Domain_settings table */

#define RESERVED_1      (1 << 28) /* Reserved for private use */
#define RESERVED_2      (1 << 29) /* Reserved for private use */
#define RESERVED_3      (1 << 30) /* Reserved for private use */
#define RESERVED_4      (1 << 31) /* Reserved for private use */

struct db_gen;


DBLIST_HEAD(_db_root);

/** \brief The root of all DB API structures
 *
 *  This is the root linked list of all database
 *  structures allocated in SER
 */
extern struct _db_root db_root;

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* _DB_H */
