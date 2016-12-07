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

#ifndef _DB_URI_H
#define _DB_URI_H  1

/** \ingroup DB_API 
 * @{ 
 */

#include "db_gen.h"
#include "../../str.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct db_uri;

typedef unsigned char (db_uri_cmp_t)(struct db_uri* uri1, struct db_uri* uri2);

typedef struct db_uri {
	db_gen_t gen;      /* Generic part of the structure */
	str scheme;        /* URI scheme */
	str body;          /* Entire URI body */
	db_uri_cmp_t* cmp; /* Comparison function */
} db_uri_t;


/*
 * Create a database URI structure
 */
struct db_uri* db_uri(const char* uri);

void db_uri_free(struct db_uri* uri);

unsigned char db_uri_cmp(struct db_uri* uri1, struct db_uri* uri2);

db_uri_cmp_t db_uri_cmp;

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* _DB_URI_H */
