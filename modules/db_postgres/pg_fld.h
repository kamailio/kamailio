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

#ifndef _PG_FLD_H
#define _PG_FLD_H

/** \addtogroup postgres
 * @{ 
 */

/** \file 
 * Implementation of pg_fld data structure representing PostgreSQL fields and
 * related functions.
 */

#include "pg_oid.h"
#include "pg_cmd.h"
#include "../../ut.h"
#include "../../lib/srdb2/db_gen.h"
#include "../../lib/srdb2/db_fld.h"
#include <libpq-fe.h>

struct pg_fld {
	db_drv_t gen;

	/**
	 * A union of varius data types from db_fld, postgres expects binary
	 * data in network byte order so we use these variables as temporary
	 * buffer to store values after the conversion.
	 */
	union {
		int          int4[2]; /**< Integer value in network byte order */
		short        int2[4];
		float        flt;     /**< Float value in network byte order */
		double       dbl;     /**< Double value in network byte order */
		time_t       time;    /**< Unix timestamp in network byte order */
		unsigned int bitmap;  /**< Bitmap value in network byte order */ 
		long long    int8;    /**< 8-byte integer value in network byte order */
		char         byte[8];
	} v;
	char buf[INT2STR_MAX_LEN]; /**< Buffer for int2str conversions */
	Oid oid;                   /**< Type of the field on the server */
};


/** Creates a new PostgreSQL specific payload.
 * This function creates a new PostgreSQL specific payload structure and
 * attaches the structure to the generic db_fld structure.
 * @param fld A generic db_fld structure to be exended.
 * @param table Name of the table on the server.
 * @retval 0 on success.
 * @retval A negative number on error.
 */
int pg_fld(db_fld_t* fld, char* table);

int pg_resolve_param_oids(db_fld_t* vals, db_fld_t* match, 
						  int n1, int n2, PGresult* res);

int pg_resolve_result_oids(db_fld_t* fld, int n, PGresult* res);


/** Converts arrays of db_fld fields to PostgreSQL parameters.
 * The function converts fields in SER db_fld format to parameters suitable
 * for PostgreSQL API functions.
 * @param values An array of pointers to values in PostgreSQL format. The
 *               function will store pointers to converted values there.
 * @param lenghts An array of integers that will be filled with lenghts
 *                of values.
 * @param formats An array of value formats, see PostgreSQL API client
 *                library documentation for more detail.
 * @param oids Types of corresponding columns on the server.
 * @param types A type conversion table.
 * @param fld An array of db_fld fields to be converted.
 * @param flags Connection flags controlling how values are converted.
 * @todo Implement support for bit fields with size bigger than 32 
 * @todo Implement support for varbit properly to remove leading zeroes
 * @todo Check if timezones are handled properly
 * @todo Support for DB_NONE in pg_pg2fld and pg_check_pg2fld
 * @todo local->UTC conversion (also check the SQL command in ser-oob)
 */
int pg_fld2pg(struct pg_params* dst, int off, pg_type_t* types, 
			  db_fld_t* src, unsigned int flags);


/** Converts fields from result in PGresult format into SER format.
 * The function converts fields from PostgreSQL result (PGresult structure)
 * into the internal format used in SER. The function converts one row at a
 * time.
 * @param fld The destination array of db_fld fields to be filled with converted
 *            values.
 * @param pres A PostgreSQL result structure to be converted into SER format.
 * @param row Number of the row to be converted.
 * @param flags Connection flags controlling how values are converted.
 * @retval 0 on success
 * @retval A negative number on error.
 * @todo UTC->local conversion
 */
int pg_pg2fld(db_fld_t* dst, PGresult* src, int row, pg_type_t* types, 
			  unsigned int flags);


/** Checks if all db_fld fields have types compatible with corresponding field 
 * types on the server.
 * The functions checks whether all db_fld fields in the last parameter are
 * compatible with column types on the server.
 * @param oids An array of Oids of corresponding columns on the server.
 * @param lenghts An array of sizes of corresponding columns on the server.
 * @param types An array used to map internal field types to Oids.
 * @param fld An array of db_fld fields to be checked.
 * @retval 0 on success
 * @retval A negative number on error.
 */
int pg_check_fld2pg(db_fld_t* fld, pg_type_t* types);


int pg_check_pg2fld(db_fld_t* fld, pg_type_t* types);


/** @} */

#endif /* _PG_FLD_H */
