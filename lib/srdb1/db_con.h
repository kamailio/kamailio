/* 
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2007-2008 1&1 Internet AG
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

/*!
 * \file lib/srdb1/db_con.h
 * \ingroup db1
 * \brief Type that represents a database connection
 */

#ifndef DB1_CON_H
#define DB1_CON_H

#include "../../str.h"


/*! \brief
 * This structure represents a database connection, pointer to this structure
 * are used as a connection handle from modules uses the db API.
 */
typedef struct {
	const str* table;      /*!< Default table that should be used              */
	const char *tquote;    /*!< Char to quote special tokens (table/column names) */
	unsigned long tail;    /*!< Variable length tail, database module specific */
} db1_con_t;


/** Return the table of the connection handle */
#define CON_TABLE(cn)      ((cn)->table)
/** Return the tquote of the connection handle */
#define CON_TQUOTE(cn)     ((cn)->tquote)
/** Return the tquote of the connection handle or empty str if null */
#define CON_TQUOTESZ(cn)   (((cn)->tquote)?((cn)->tquote):"")
/** Return the tail of the connection handle */
#define CON_TAIL(cn)       ((cn)->tail)


#endif /* DB1_CON_H */
