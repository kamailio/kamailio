/* 
 * $Id$ 
 *
 * MySQL module utilities
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
 *
 * History:
 * -------
 * 2003-04-14 tm_isdst in struct tm set to -1 to let mktime 
 *            guess daylight saving (janakj)
 */

#define _XOPEN_SOURCE

#include <strings.h>
#include <string.h>
#include "utils.h"


/*
 * Convert time_t structure to format accepted by MySQL database
 */
int time2mysql(time_t _time, char* _result, int _res_len)
{
	struct tm* t;
	
	     /*
	       if (_time == MAX_TIME_T) {
	       snprintf(_result, _res_len, "0000-00-00 00:00:00");
	       }
	     */

	t = localtime(&_time);
	return strftime(_result, _res_len, "%Y-%m-%d %H:%M:%S", t);
}


/*
 * Convert MySQL time representation to time_t structure
 */
time_t mysql2time(const char* _str)
{
	struct tm time;
	
	     /* It is neccessary to zero tm structure first */
	memset(&time, '\0', sizeof(struct tm));
	strptime(_str, "%Y-%m-%d %H:%M:%S", &time);

	     /* Daylight saving information got lost in the database
	      * so let mktime to guess it. This eliminates the bug when
	      * contacts reloaded from the database have different time
	      * of expiration by one hour when daylight saving is used
	      */ 
	time.tm_isdst = -1;   
	return mktime(&time);
}


/*
 * Parse a mysql database URL of form 
 * mysql://[username[:password]@]hostname[:port]/database
 *
 * Returns 0 if parsing was sucessful and -1 otherwise
 */
int parse_mysql_url(char* _url, char** _user, char** _pass,
		    char** _host, char** _port, char** _db)
{
#define SHORTEST_MYSQL_URL "mysql://a/b"
#define SHORTEST_MYSQL_URL_LEN (sizeof(SHORTEST_MYSQL_URL) - 1)

#define MYSQL_URL_PREFIX "mysql://"
#define MYSQL_URL_PREFIX_LEN (sizeof(MYSQL_URL_PREFIX) - 1)

	enum state {
		ST_USER_HOST,  /* Username or hostname */
		ST_PASS_PORT,  /* Password or port part */
		ST_HOST,       /* Hostname part */
		ST_PORT,       /* Port part */
		ST_DB          /* Database part */
	};

	enum state st;
	int len, i;
	char* begin, *prev_begin;

	if (!_url || !_user || !_pass || !_host || !_port || !_db) {
		return -1;
	}
	
	len = strlen(_url);
	if (len < SHORTEST_MYSQL_URL_LEN) {
		return -1;
	}
	
	if (strncasecmp(_url, MYSQL_URL_PREFIX, MYSQL_URL_PREFIX_LEN)) {
		return -1;
	}

	     /* Skip the prefix part */
	_url += MYSQL_URL_PREFIX_LEN;
	len -= MYSQL_URL_PREFIX_LEN;
	
	     /* Initialize all variables */
	*_user = '\0';
	*_pass = '\0';
	*_host = '\0';
	*_port = '\0';
	*_db   = '\0';

	st = ST_USER_HOST;
	begin = _url;
	prev_begin = 0;

	for(i = 0; i < len; i++) {
		switch(st) {
		case ST_USER_HOST:
			switch(_url[i]) {
			case '@':
				st = ST_HOST;
				*_user = begin;
				begin = _url + i + 1;
				_url[i] = '\0';
				break;

			case ':':
				st = ST_PASS_PORT;
				prev_begin = begin;
				begin = _url + i + 1;
				_url[i] = '\0';
				break;

			case '/':
				*_host = begin;
				_url[i] = '\0';

				*_db = _url + i + 1;
				return 0;
			}

		case ST_PASS_PORT:
			switch(_url[i]) {
			case '@':
				st = ST_HOST;
				*_user = prev_begin;
				*_pass = begin;
				begin = _url + i + 1;
				_url[i] = '\0';
				break;

			case '/':
				*_host = prev_begin;
				*_port = begin;
				_url[i] = '\0';

				*_db = _url + i + 1;
				return 0;
			}

		case ST_HOST:
			switch(_url[i]) {
			case ':':
				st = ST_PORT;
				*_host = begin;
				begin = _url + i + 1;
				_url[i] = '\0';
				break;

			case '/':
				*_host = begin;
				_url[i] = '\0';

				*_db = _url + i + 1;
				return 0;
			}

		case ST_PORT:
			switch(_url[i]) {
			case '/':
				*_port = begin;
				_url[i] = '\0';

				*_db = _url + i + 1;
				return 0;
			}
			break;
			
		case ST_DB:
			break;
		}
	}

	if (st != ST_DB) {
		return -1;
	}

	return 0;
}
