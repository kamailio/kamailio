/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2007-2008 1&1 Internet AG
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * \file db/db_ut.h
 * \brief Utility functions for database drivers.
 *
 * This utility methods are used from the database SQL driver to convert
 * values and print SQL queries from the internal API representation.
*/

#ifndef DB_UT_H
#define DB_UT_H

/**
 * maximal SQL buffer length for database drivers
 */
#define SQL_BUF_LEN 65536

/**
 * make strptime available
 * use 600 for 'Single UNIX Specification, Version 3'
 */
#define _XOPEN_SOURCE 600          /* glibc2 on linux, bsd */
#define _XOPEN_SOURCE_EXTENDED 1   /* solaris */

#include <time.h>

#undef _XOPEN_SOURCE
#undef _XOPEN_SOURCE_EXTENDED

#include "db_key.h"
#include "db.h"


/**
 * Converts a char into an integer value.
 *
 * \param _s source value
 * \param _v target value
 * \return zero on sucess, negative on conversion errors
 */
int db_str2int(const char* _s, int* _v);


/**
 * Converts a char into a double value.
 *
 * \param _s source value
 * \param _v target value
 * \return zero on sucess, negative on conversion errors
 */
int db_str2double(const char* _s, double* _v);


/**
 * Converts a integer value in a char pointer.
 *
 * \param _v source value
 * \param _s target value
 * \param _l available length and target length
 * \return zero on sucess, negative on conversion errors
 */
int db_int2str(int _v, char* _s, int* _l);


/**
 * Converts a double value into a char pointer.
 *
 * \param _v source value
 * \param _s target value
 * \param _l available length and target length
 * \return zero on sucess, negative on conversion errors
 */
int db_double2str(double _v, char* _s, int* _l);


/**
 * Convert a time_t value to string.
 *
 * \param _v source value
 * \param _s target value
 * \param _l available length and target length
 * \return zero on sucess, negative on conversion errors
 * \todo This functions add quotes to the time value. This
 * should be done in the val2str function, as some databases
 * like db_berkeley don't need or like this at all.
 */
int db_time2str(time_t _v, char* _s, int* _l);


/**
 * Converts a char into a time_t value.
 *
 * \param _s source value
 * \param _v target value
 * \return zero on sucess, negative on conversion errors
 */
int db_str2time(const char* _s, time_t* _v);


/**
 * Print columns for a SQL statement, separated by commas.
 *
 * \param _b target char
 * \param _l length of the target
 * \param _c keys that should be printed
 * \param _n number of keys
 * \return the length of the printed result on success, negative on errors
 */
int db_print_columns(char* _b, const int _l, const db_key_t* _c, const int _n);


/**
 * Print values for a SQL statement.
 *
 * \param _c structure representing database connection
 * \param _b target char
 * \param _l length of the target
 * \param _v values that should be printed
 * \param _n number of values
 * \param  (*val2str) function pointer to a db specific conversion function
 * \return the length of the printed result on success, negative on errors
 */
int db_print_values(const db_con_t* _c, char* _b, const int _l, const db_val_t* _v,
	const int _n, int (*val2str)(const db_con_t*, const db_val_t*, char*, int*));


/**
 * Print where clause for a SQL statement.
 *
 * \param _c structure representing database connection
 * \param _b target char
 * \param _l length of the target
 * \param _k keys that should be printed
 * \param _o optional operators
 * \param _v values that should be printed
 * \param _n number of key/value pairs
 * \param  (*val2str) function pointer to a db specific conversion function
 * \return the length of the printed result on success, negative on errors
 */
int db_print_where(const db_con_t* _c, char* _b, const int _l, const db_key_t* _k,
	const db_op_t* _o, const db_val_t* _v, const int _n, int (*val2str)
	(const 	db_con_t*, const db_val_t*, char*, int*));


/**
 * Print set clause for a SQL statement.
 *
 * \param _c structure representing database connection
 * \param _b target char
 * \param _l length of the target
 * \param _k keys that should be printed
 * \param _v vals that should be printed
 * \param _n number of key/value pairs
 * \param  (*val2str) function pointer to a db specific conversion function
 * \return the length of the printed result on success, negative on errors
 */
int db_print_set(const db_con_t* _c, char* _b, const int _l,
	const db_key_t* _k, const db_val_t* _v, const int _n, int (*val2str)
	(const db_con_t*, const db_val_t*, char*, int*));

#endif
