/* 
 * $Id$ 
 *
 * MySQL module utilities
 *
 * Copyright (C) 2001-2003 FhG Fokus
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


#ifndef UTILS_H
#define UTILS_H

#include <time.h>


/*
 * Convert time_t structure to format accepted by MySQL database
 */
int time2mysql(time_t _time, char* _result, int _res_len);


/*
 * Convert MySQL time representation to time_t structure
 */
time_t mysql2time(const char* _str);


/*
 * Parse a mysql database URL of form 
 * mysql://[username[:password]@]hostname[:port]/database
 *
 * Returns 0 if parsing was successful and -1 otherwise
 */
int parse_mysql_url(char* _url, char** _user, char** _pass,
		    char** _host, char** _port, char** _db);


#endif /* UTILS_H */
