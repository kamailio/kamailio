/* 
 * $Id$ 
 */

#include "sipdate.h"


/*
 * Declarations
 */
static inline char*  eat_wsp      (char* _b);
static inline char*  parse_month  (char* _date, int* _month);
static inline char*  parse_year   (char* _date, int* _year);
static inline char*  parse_time   (char* _date, int* _hour, int* _minute, int* _second);
static inline char*  parse_day    (char* _date, int* _day);
static inline time_t convert_time (int _second, int _minute, int _hour,
				   int _day, int _month, int _year);


/*
 * Eats white spaces
 *
 * PARAMS : char* _b : buffer to be processed
 * RETURNS: char*    : points after white spaces
 */
static inline char* eat_wsp(char* _b)
{
	while ((*_b == ' ') || (*_b == '\t')) {
		_b++;
	}

	return _b;
}


/*
 * Optimized for speed, supposes that identifier is correct
 * otherwise, unpredictable things may happen (it might even kill your cat) ;-)
 *
 * PARAMS : char* _date : string containing date
 *          int* _month : pointer to integer where month will be stored
 * RETURNS: char*       : pointer after parsed month part
 */
static inline char* parse_month(char* _date, int* _month)
{
	switch(*_date) {
	case 'J': if (*(_date + 1) == 'a')      *_month = 0;
		  else if (*(_date + 2) == 'n') *_month = 5;
		  else                          *_month = 6;
		  break;

	case 'F': *_month = 1;
		  break;

	case 'M': if (*(_date + 2) == 'r') *_month = 2;
		  else *_month = 4;
		  break;

	case 'A': if (*(_date + 1) == 'p') *_month = 3;
	          else *_month = 7;
		  break;

	case 'S': *_month = 8; break;
	case 'O': *_month = 9; break;
	case 'N': *_month = 10; break;
	case 'D': *_month = 11; break;
	}

	return _date + 3;
}


/*
 * Parse year part in SIP-date
 *
 * PARAMS : char* _date : date string pointing to year part
 *          int* _year  : parsed year will be stored in this variable
 * RETURNS: char*       : points immediately after parsed part
 */
static inline char* parse_year(char* _date, int* _year)
{
	*_year = *_date++ - '0';
	*_year *= 10;
	*_year += *_date++ - '0';

        if ((*_date >= '0') && (*_date <= '9')) {
		*_year *= 10;
		*_year += *_date++ - '0';
	}

        if ((*_date >= '0') && (*_date <= '9')) {
		*_year *= 10;
		*_year += *_date++ - '0';
	}

	return _date;
}


/*
 * Parse time in a SIP-date string
 *
 * PARAMS : char* _date  : SIP-date string pointing to time
 *        : int* _hour   : parsed hour will be stored here
 *        : int* _minute : parsed minute value will be stored here
 *        : int* _second : parsed second value will be stored here
 * RETURNS: char*        : immediately after parsed part
 */
static inline char* parse_time(char* _date, int* _hour, int* _minute, int* _second)
{
	*_hour = *_date++ - '0';
	*_hour *= 10;
	*_hour += *_date++ - '0';

	_date = eat_wsp(_date) + 1;
	_date = eat_wsp(_date);

	*_minute = *_date++ - '0';
	*_minute *= 10;
	*_minute += *_date++ - '0';

	_date = eat_wsp(_date);

	if (*_date++ == ':') {
		_date = eat_wsp(_date);
		*_second = *_date++ - '0';
		*_second *= 10;
		*_second += *_date++ - '0';
	} else *_second = 0;

	return _date;
}


/*
 * Parse day in SIP-date string
 *
 * PARAMS : char* _date : SIP-date string pointing to date part
 *        : int* _day   : parsed day value will be stored here
 * RETURNS: char*       : points immediately after parsed part
 */
static inline char* parse_day(char* _date, int* _day)
{
	*_day = *_date++ - '0';

	if ((*_date >= '0') && (*_date <= '9')) {
		*_day *= 10;
		*_day += *_date++ - '0';
	}

	return _date;
}


/*
 * Converts time into time_t structure
 *
 * PARAMS : int _second : seconds (0 - 61)
 *        : int _minute : minutes (0 - 59)
 *        : int _hour   : hours   (0 - 23)
 *        : int _day    : day     (1 - 31)
 *        : int _month  : month   (0 - 11)
 *        : int _year   : year
 * RETURNS: time_t      : converted time
 */ 
static inline time_t convert_time(int _second, int _minute, int _hour,
		    int _day, int _month, int _year)
{
	struct tm time;

	time.tm_sec = _second;
	time.tm_min = _minute;
	time.tm_hour = _hour;
	time.tm_mday = _day;
	time.tm_mon = _month;
	time.tm_year = _year - 1900;
	time.tm_wday = 0;
	time.tm_yday = 0;
	time.tm_isdst = -1;

	return mktime(&time);
}


/*
 * This function parses SIP-date
 *
 * PARAMS : char* _date   : string containing SIP-date
 *        : time_t* _time : pointer to a variable where parsed date will be stored
 * RETURNS: int           : 1 if everything went ok, 0 otherwise
 */ 
int parse_SIP_date(char* _date, time_t* _time)
{
	int day, month, year, hour, minute, second;

	if (!_date) return 0;

	_date = eat_wsp(_date);

	if ((*_date <= '0') || (*_date >= '9')) {
		while(*_date++ != ',');  /* skip day of week */
	}

	_date = eat_wsp(_date);
	_date = parse_day(_date, &day);
	_date = eat_wsp(_date);
	_date = parse_month(_date, &month);
	_date = eat_wsp(_date);
	_date = parse_year(_date, &year);
	_date = eat_wsp(_date);
	_date = parse_time(_date, &hour, &minute, &second);

	*_time = convert_time(second, minute, hour, day, month, year);
	return 1;
}
