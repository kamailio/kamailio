#ifndef __DB_UTILS_H__
#define __DB_UTILS_H__

#define __USE_XOPEN /* Because of strptime */
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
 * SQL URL parser
 */
int parse_sql_url(char* _url, char** _user, char** _pass, 
		  char** _host, char** _port, char** _db);

#endif
