/* 
 * $Id$ 
 *
 * UNIXODBC util functions
 *
 * Copyright (C) 2005-2006 Marco Lorrai
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
 *
 *
 * History:
 * --------
 *  2005-12-01  initial commit (chgen)
 */



#define _XOPEN_SOURCE 4     /* bsd */
#define _XOPEN_SOURCE_EXTENDED 1    /* solaris */

#include <strings.h>
#include <string.h>
#include <time.h>  /*strptime, XOPEN issue must be >=4 */
#include "utils.h"


/*
 * Convert time_t structure to format accepted by UNIXODBC database
 */
int time2odbc(time_t _time, char* _result, int _res_len)
{
	struct tm* t;
	
	t = localtime(&_time);
	return strftime(_result, _res_len, "%Y-%m-%d %H:%M:%S", t);
}


/*
 * Convert UNIXODBC time representation to time_t structure
 */
time_t odbc2time(const char* _str)
{
	struct tm time;
	
	/* It is necessary to zero tm structure first */
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
