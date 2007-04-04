/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2006-2007 iptelorg GmbH
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _DB_H
#define _DB_H  1

/**
 * \defgroup DB_API Database Abstraction Layer
 * \brief brief description
 *
 * I wonder where this text goes.
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

struct db_gen;


DBLIST_HEAD(db_root);

/** \brief The root of all DB API structures
 *
 *  This is the root linked list of all database
 *  structures allocated in SER
 */
extern struct db_root db;

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* _DB_H */
