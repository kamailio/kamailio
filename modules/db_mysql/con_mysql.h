/* 
 * $Id$
 *
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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


#ifndef CON_MYSQL_H
#define CON_MYSQL_H

#include <mysql/mysql.h>

/*
 * MySQL specific connection data
 */
struct con_mysql {
	MYSQL_RES* res; /* Actual result */
	MYSQL* con;     /* Connection representation */
	MYSQL_ROW row;  /* Actual row in the result */
};


#define CON_RESULT(db_con)     (((struct con_mysql*)((db_con)->tail))->res)
#define CON_CONNECTION(db_con) (((struct con_mysql*)((db_con)->tail))->con)
#define CON_ROW(db_con)        (((struct con_mysql*)((db_con)->tail))->row)


#endif /* CON_MYSQL_H */
