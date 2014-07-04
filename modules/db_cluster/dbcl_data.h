/*
 * $Id$
 *
 * DB CLuster core functions
 *
 * Copyright (C) 2012 Daniel-Constantin Mierla (asipto.com)
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
 *  \brief DB_CLUSTER :: Core
 *  \ingroup db_cluster
 *  Module: \ref db_cluster
 */



#ifndef _DBCL_DATA_H_
#define _DBCL_DATA_H_


#include "../../lib/srdb1/db.h"
#include "../../str.h"

#define DBCL_PRIO_SIZE	10
#define DBCL_CLIST_SIZE	5

#define DBCL_CON_INACTIVE	1

typedef struct dbcl_shared
{
	int state;
	unsigned int aticks;
} dbcl_shared_t;

typedef struct dbcl_con
{
	str name;
	unsigned int conid;
	str db_url;
	db1_con_t  *dbh;
	db_func_t dbf;
	int flags;
	dbcl_shared_t *sinfo;
	struct dbcl_con *next;
} dbcl_con_t;

typedef struct dbcl_cdata
{
	dbcl_con_t *clist[DBCL_CLIST_SIZE];
	int clen;
	int prio;
	int mode;
	int crt;
} dbcl_cdata_t;

typedef struct dbcl_cls
{
	str name;
	unsigned int clsid;
	unsigned int ref;
	dbcl_cdata_t rlist[DBCL_PRIO_SIZE];
	dbcl_cdata_t wlist[DBCL_PRIO_SIZE];
	dbcl_con_t *usedcon;
	struct dbcl_cls *next;
} dbcl_cls_t;


int dbcl_init_dbf(dbcl_cls_t *cls);
int dbcl_init_connections(dbcl_cls_t *cls);
int dbcl_close_connections(dbcl_cls_t *cls);
dbcl_cls_t *dbcl_get_cluster(str *name);

int dbcl_valid_con(dbcl_con_t *sc);
int dbcl_inactive_con(dbcl_con_t *sc);

int dbcl_parse_con_param(char *val);
int dbcl_parse_cls_param(char *val);
#endif /* KM_DBASE_H */
