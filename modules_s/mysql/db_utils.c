/* 
 * $Id$ 
 *
 * MySQL module useful functions
 */

#include "db_utils.h"
#include <string.h>
#include "utils.h"
#include "defs.h"

#define _XOPEN_SOURCE
#include <time.h>


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
