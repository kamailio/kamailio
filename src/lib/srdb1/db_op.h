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

/**
 * \file lib/srdb1/db_op.h
 * \brief Type that represents a expression operator.
 * \ingroup db1
 */

#ifndef DB1_OP_H
#define DB1_OP_H

/** operator less than */
#define OP_LT  "<"
/** operator greater than */
#define OP_GT  ">"
/** operator equal */
#define OP_EQ  "="
/** operator less than equal */
#define OP_LEQ "<="
/** operator greater than equal */
#define OP_GEQ ">="
/** operator negation */
#define OP_NEQ "<>"
/** bitwise AND */
#define OP_BITWISE_AND "&"


/**
 * This type represents an expression operator uses for SQL queries.
 */
typedef const char* db_op_t;


#endif /* DB1_OP_H */
