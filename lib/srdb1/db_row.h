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
 * \file db/db_row.h
 * \brief Type that represents a row in a database.
 *
 * This file holds a type that represents a row in a database, some convenience
 * macros and a function for memory managements.
 */


#ifndef DB_ROW_H
#define DB_ROW_H

#include "db_val.h"


/**
 * Structure holding the result of a query table function.
 * It represents one row in a database table. In other words, the row is an
 * array of db_val_t variables, where each db_val_t variable represents exactly
 * one cell in the table.
 */
typedef struct db_row {
	db_val_t* values;  /**< Columns in the row */
	int n;             /**< Number of columns in the row */
} db_row_t;

/** Return the columns in the row */
#define ROW_VALUES(rw) ((rw)->values)
/** Return the number of colums */
#define ROW_N(rw)      ((rw)->n)

/**
 * Release memory used by a row. This method only frees values that are inside
 * the row if the free flag of the specific value is set. Otherwise this
 * storage must be released when the database specific result free function is
 * called. Only string based values are freed if wanted, null values are skipped.
 * \param _r row that should be released
 * \return zero on success, negative on error
 */
inline int db_free_row(db_row_t* _r);

#endif /* DB_ROW_H */
