/* 
 * $Id$ 
 *
 * PostgreSQL Database Driver for SER
 *
 * Portions Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2003 August.Net Services, LLC
 * Portions Copyright (C) 2005-2008 iptelorg GmbH
 *
 * This file is part of SER, a free SIP server.
 *
 * SER is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version
 *
 * For a license to use the ser software under conditions other than those
 * described here, or to purchase support for this software, please contact
 * iptel.org by e-mail at the following addresses: info@iptel.org
 *
 * SER is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/** \addtogroup postgres
 * @{ 
 */

/** \file
 * Implementation of functions related to PostgreSQL Oid identifiers.
 */


#include "pg_oid.h"
#include "../../dprint.h"
#include "../../ut.h"
#include <strings.h>
#include <stdlib.h>
#include <string.h>

/** An array of supported PostgreSQL field types. */
static char* pg_type_id_name[] = {
	"bool",
	"bytea",
	"char",
	"int8",
	"int2",
	"int4",
	"text",
	"float4",
	"float8",
	"inet",
	"bpchar",
	"varchar",
	"timestamp",
	"timestamptz",
	"bit",
	"varbit",
};


static int get_index(char* name)
{
	int i;
	
	for(i = 0; i < PG_ID_MAX; i++) {
		if (strcasecmp(name, pg_type_id_name[i]) == 0) return i; 
	}
	return -1;
}


pg_type_t* pg_new_oid_table(PGresult* res)
 {
	pg_type_t* table = NULL;
	int row, n = 0, end, idx, fields;
	str s;
	
	if (res == NULL || PQresultStatus(res) != PGRES_TUPLES_OK) goto error;

	n = PQntuples(res);
	if (n <= 0) goto error;
	fields = PQnfields(res);
	if (fields != 2) goto error;

	table = (pg_type_t*)malloc(sizeof(pg_type_t) * (n + 1));
	if (table == NULL) goto error;
	memset(table, '\0', sizeof(pg_type_t) * (n + 1));
	
	end = n - 1;
	for(row = 0; row < n; row++) {

		/* Get name */
		s.s = PQgetvalue(res, row, 0);
		if (s.s == NULL) goto error;

		/* Find index where the record is to be stored */
		idx = get_index(s.s);
		if (idx == -1) idx = end--;

		/* Store the name */
		table[idx].name = strdup(s.s);
		if (table[idx].name == NULL) goto error;

		/* Oid */
		s.s = PQgetvalue(res, row, 1);
		if (s.s == NULL) goto error;
		s.len = strlen(s.s);
		if (str2int(&s, &table[idx].oid) < 0) goto error;

		DBG("postgres: Type %s maps to Oid %d\n", table[idx].name, table[idx].oid);
	}
	return table;
	
 error:
	ERR("postgres: Error while obtaining field/data type description from server\n");
	if (table) {
		for(idx = 0; idx < n; idx++) {
			if (table[idx].name) free(table[idx].name);
		}
		free(table);
	}
	return NULL;
}


void pg_destroy_oid_table(pg_type_t* table)
{
	int i;
	if (table) {
		for(i = 0; table[i].name; i++) {
			free(table[i].name);
		}
		free(table);
	}
}


int pg_name2oid(Oid* oid, pg_type_t* table, const char* name)
{
	int i;

	if (!oid || !table) {
		BUG("postgres: Invalid parameters to pg_name2oid\n");
		return -1;
	}

	if (name == NULL || name[0] == '\0') return 1;

	for(i = 0; table[i].name; i++) {
		if (strcasecmp(table[i].name, name) == 0) {
			*oid = table[i].oid;
			return 0;
		}
	}
	return 1;
}


int pg_oid2name(const char** name, pg_type_t* table, Oid oid)
{
	int i;

	if (!table || !name) {
		BUG("postgres: Invalid parameters to pg_oid2name\n");
		return -1;
	}

	for(i = 0; table[i].name; i++) {
		if (oid == table[i].oid) {
			*name = table[i].name;
			return 0;
		}
	}
	return 1;
}

/** @} */
