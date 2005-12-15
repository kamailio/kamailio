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
 *
 * History:
 * -------
 * 2003-04-14 tm_isdst in struct tm set to -1 to let mktime 
 *            guess daylight saving (janakj)
 */


#define _XOPEN_SOURCE 4     /* bsd */
#define _XOPEN_SOURCE_EXTENDED 1    /* solaris */
#define _SVID_SOURCE 1 /* timegm */

#include <strings.h>
#include <string.h>
#include <time.h>  /*strptime, XOPEN issue must be >=4 */
#include "../../ut.h"
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

	t = gmtime(&_time);
	return strftime(_result, _res_len, "%Y-%m-%d %H:%M:%S", t);
}


/*
 * Convert MySQL time representation to time_t structure
 */
time_t mysql2time(const char* _str)
{
	struct tm time;
	
	     /* It is necessary to zero tm structure first */
	memset(&time, '\0', sizeof(struct tm));
	strptime(_str, "%Y-%m-%d %H:%M:%S", &time);

	     /* Daylight saving information got lost in the database
	      * so let timegm to guess it. This eliminates the bug when
	      * contacts reloaded from the database have different time
	      * of expiration by one hour when daylight saving is used
	      */ 
	time.tm_isdst = -1;   
#ifdef HAVE_TIMEGM
	return timegm(&time);
#else
	return _timegm(&time);
#endif /* HAVE_TIMEGM */
}
