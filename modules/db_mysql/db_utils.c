/* 
 * $Id$ 
 *
 * MySQL module useful functions
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


#define _GNU_SOURCE /* To avoid strptime warning */

#include <string.h>
#include <time.h>
#include "db_utils.h"
#include "utils.h"
#include "defs.h"



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
	return mktime(&time);
}


/* FIXME */
/*
 * SQL URL parser
 */
int parse_sql_url(char* _url, char** _user, char** _pass, 
		  char** _host, char** _port, char** _db)
{
	char* slash, *dcolon, *at, *db_slash;

	*_user = '\0';
	*_pass = '\0';
	*_host = '\0';
	*_port = '\0';
	*_db   = '\0';

	     /* Remove any leading and trailing spaces and tab */
	_url = trim(_url);

	if (strlen(_url) < 6) return -1;

	if (*_url == '\0') return -2; /* Empty string */

	slash = strchr(_url, '/');
	if (!slash) return -3;   /* Invalid string, slashes not found */

	if ((*(++slash)) != '/') {  /* Invalid URL, 2nd slash not found */
		return -4;
	}

	slash++;

	at = strchr(slash, '@');

	db_slash = strchr(slash, '/');
	if (db_slash) {
		*db_slash++ = '\0';
		*_db = trim(db_slash);
	}

	if (!at) {
		dcolon = strchr(slash, ':');
		if (dcolon) {
			*dcolon++ = '\0';
			*_port = trim(dcolon);
		}
		*_host = trim(slash);
	} else {
		dcolon = strchr(slash, ':');
	        *at++ = '\0';
		if (dcolon) {
			*dcolon++ = '\0';
			if (dcolon < at) {   /* user:passwd */
				*_pass = trim(dcolon);
				dcolon = strchr(at, ':');
				if (dcolon) {  /* host:port */
					*dcolon++ = '\0';
					*_port = trim(dcolon);
				}
			} else {            /* host:port */
				*_port = trim(dcolon);
			}
		}
		*_host = trim(at);
		*_user = trim(slash);
	}

	return 0;
}
