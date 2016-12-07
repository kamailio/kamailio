/*
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *
 * History:
 * -------
 * 2003-06-24: file imported from tmrec (bogdan)
 * 2003-xx-xx: file Created (daniel)
 */

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../../mem/mem.h"
#include "cpl_time.h"


/************************ imported from "utils.h"  ***************************/

static inline int strz2int(char *_bp)
{
	int _v;
	char *_p;
	if(!_bp)
		return 0;
	_v = 0;
	_p = _bp;
	while(*_p && *_p>='0' && *_p<='9')
	{
		_v += *_p - '0';
		_p++;
	}
	return _v;
}


static inline char* trim(char* _s)
{
	int len;
	char* end;

	     /* Null pointer, there is nothing to do */
	if (!_s) return _s;

	     /* Remove spaces and tabs from the beginning of string */
	while ((*_s == ' ') || (*_s == '\t')) _s++;

	len = strlen(_s);

        end = _s + len - 1;

	     /* Remove trailing spaces and tabs */
	while ((*end == ' ') || (*end == '\t')) end--;
	if (end != (_s + len - 1)) {
		*(end+1) = '\0';
	}

	return _s;
}




/************************ imported from "ac_tm.c"  ***************************/

/* #define USE_YWEEK_U		// Sunday system
 * #define USE_YWEEK_V		// ISO 8601
 */
#ifndef USE_YWEEK_U
#ifndef USE_YWEEK_V
#ifndef USE_YWEEK_W
#define USE_YWEEK_W		// Monday system
#endif
#endif
#endif

#ifdef USE_YWEEK_U
#define SUN_WEEK(t)	(int)(((t)->tm_yday + 7 - \
				((t)->tm_wday)) / 7)
#else
#define MON_WEEK(t)	(int)(((t)->tm_yday + 7 - \
				((t)->tm_wday ? (t)->tm_wday - 1 : 6)) / 7)
#endif

#define ac_get_wday_yr(t) (int)((t)->tm_yday/7)
#define ac_get_wday_mr(t) (int)(((t)->tm_mday-1)/7)

ac_tm_p ac_tm_new()
{
	ac_tm_p _atp = NULL;
	_atp = (ac_tm_p)pkg_malloc(sizeof(ac_tm_t));
	if(!_atp)
		return NULL;
	memset(_atp, 0, sizeof(ac_tm_t));
	
	return _atp;
}

int ac_tm_fill(ac_tm_p _atp, struct tm* _tm)
{
	if(!_atp || !_tm)
		return -1;
	_atp->t.tm_sec = _tm->tm_sec;       /* seconds */
	_atp->t.tm_min = _tm->tm_min;       /* minutes */
	_atp->t.tm_hour = _tm->tm_hour;     /* hours */
	_atp->t.tm_mday = _tm->tm_mday;     /* day of the month */
	_atp->t.tm_mon = _tm->tm_mon;       /* month */
	_atp->t.tm_year = _tm->tm_year;     /* year */
	_atp->t.tm_wday = _tm->tm_wday;     /* day of the week */
	_atp->t.tm_yday = _tm->tm_yday;     /* day in the year */
	_atp->t.tm_isdst = _tm->tm_isdst;   /* daylight saving time */
	
	_atp->mweek = ac_get_mweek(_tm);
	_atp->yweek = ac_get_yweek(_tm);
	_atp->ywday = ac_get_wday_yr(_tm);
	_atp->mwday = ac_get_wday_mr(_tm);
	DBG("---> fill = %s\n",asctime(&(_atp->t)) );
	return 0;
}

int ac_tm_set(ac_tm_p _atp, struct tm* _tm)
{
	if(!_atp || !_tm)
		return -1;
	_atp->time = mktime(_tm);
	return ac_tm_fill(_atp, _tm);
}

int ac_tm_set_time(ac_tm_p _atp, time_t _t)
{
	if(!_atp)
		return -1;
	_atp->time = _t;
	return ac_tm_fill(_atp, localtime(&_t));
}

int ac_get_mweek(struct tm* _tm)
{
	if(!_tm)
		return -1;
#ifdef USE_YWEEK_U
	return ((_tm->tm_mday-1)/7 + (7-_tm->tm_wday+(_tm->tm_mday-1)%7)/7);
#else
	return ((_tm->tm_mday-1)/7 + (7-(6+_tm->tm_wday)%7+(_tm->tm_mday-1)%7)/7);
#endif
}

int ac_get_yweek(struct tm* _tm)
{
	int week = -1;
#ifdef USE_YWEEK_V
	int days;
#endif
	
	if(!_tm)
		return -1;
	
#ifdef USE_YWEEK_U
	week = SUN_WEEK(_tm);
#else
	week = MON_WEEK(_tm);
#endif

#ifdef USE_YWEEK_V
	days = ((_tm->tm_yday + 7 - (_tm->tm_wday ? _tm->tm_wday-1 : 6)) % 7);

	if(days >= 4) 
		week++;
	else 
		if(week == 0) 
			week = 53;
#endif
	return week;
}

int ac_get_wkst()
{
#ifdef USE_YWEEK_U
	return 0;
#else
	return 1;
#endif
}

int ac_tm_reset(ac_tm_p _atp)
{
	if(!_atp)
		return -1;
	memset(_atp, 0, sizeof(ac_tm_t));
	return 0;
}

int ac_tm_free(ac_tm_p _atp)
{
	if(!_atp)
		return -1;
	if(_atp->mv)
		pkg_free(_atp->mv);
	/*pkg_free(_atp);*/
	return 0;
}

ac_maxval_p ac_get_maxval(ac_tm_p _atp)
{
	struct tm _tm;
	int _v;
	ac_maxval_p _amp = NULL;

	if(!_atp)
		return NULL;
	_amp = (ac_maxval_p)pkg_malloc(sizeof(ac_maxval_t));
	if(!_amp)
		return NULL;
	
	// the number of the days in the year
	_amp->yday = 365 + is_leap_year(_atp->t.tm_year+1900);

	// the number of the days in the month
	switch(_atp->t.tm_mon)
	{
		case 1:
			if(_amp->yday == 366)
				_amp->mday = 29;
			else
				_amp->mday = 28;
		break;
		case 3: case 5: case 8: case 10:
			_amp->mday = 30;
		break;
		default:
			_amp->mday = 31;
	}
	
	// maximum occurrences of a week day in the year
	memset(&_tm, 0, sizeof(struct tm));
	_tm.tm_year = _atp->t.tm_year;
	_tm.tm_mon = 11;
	_tm.tm_mday = 31;
	mktime(&_tm);
	_v = 0;
	if(_atp->t.tm_wday > _tm.tm_wday)
		_v = _atp->t.tm_wday - _tm.tm_wday + 1;
	else
		_v = _tm.tm_wday - _atp->t.tm_wday;
	_amp->ywday = (int)((_tm.tm_yday-_v)/7) + 1;
	
	// maximum number of weeks in the year
	_amp->yweek = ac_get_yweek(&_tm) + 1;
	
	// maximum number of the week day in the month
	_amp->mwday=(int)((_amp->mday-1-(_amp->mday-_atp->t.tm_mday)%7)/7)+1;
	
	// maximum number of weeks in the month
	_v = (_atp->t.tm_wday + (_amp->mday - _atp->t.tm_mday)%7)%7;
#ifdef USE_YWEEK_U
	_amp->mweek = (int)((_amp->mday-1)/7+(7-_v+(_amp->mday-1)%7)/7)+1;
#else
	_amp->mweek = (int)((_amp->mday-1)/7+(7-(6+_v)%7+(_amp->mday-1)%7)/7)+1;
#endif

	_atp->mv = _amp;
	return _amp;
}

int ac_print(ac_tm_p _atp)
{
	static char *_wdays[] = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"}; 
	if(!_atp)
	{
		printf("\n(null)\n");
		return -1;
	}
	
	printf("\nSys time: %d\nTime: %02d:%02d:%02d\n", (int)_atp->time,
				_atp->t.tm_hour, _atp->t.tm_min, _atp->t.tm_sec);
	printf("Date: %s, %04d-%02d-%02d\n", _wdays[_atp->t.tm_wday],
				_atp->t.tm_year+1900, _atp->t.tm_mon+1, _atp->t.tm_mday);
	printf("Year day: %d\nYear week-day: %d\nYear week: %d\n", _atp->t.tm_yday,
			_atp->ywday, _atp->yweek);
	printf("Month week: %d\nMonth week-day: %d\n", _atp->mweek, _atp->mwday);
	if(_atp->mv)
	{
		printf("Max ydays: %d\nMax yweeks: %d\nMax yweekday: %d\n",
				_atp->mv->yday, _atp->mv->yweek, _atp->mv->ywday);;
		printf("Max mdays: %d\nMax mweeks: %d\nMax mweekday: %d\n",
				_atp->mv->mday, _atp->mv->mweek, _atp->mv->mwday);;
	}
	return 0;
}





/************************ imported from "tmrec.c"  ***************************/

#define _D(c) ((c) -'0')

tr_byxxx_p tr_byxxx_new()
{
	tr_byxxx_p _bxp = NULL;
	_bxp = (tr_byxxx_p)pkg_malloc(sizeof(tr_byxxx_t));
	if(!_bxp)
		return NULL;
	memset(_bxp, 0, sizeof(tr_byxxx_t));
	return _bxp;
}

int tr_byxxx_init(tr_byxxx_p _bxp, int _nr)
{
	if(!_bxp)
		return -1;
	_bxp->nr = _nr;
	_bxp->xxx = (int*)pkg_malloc(_nr*sizeof(int));
	if(!_bxp->xxx)
		return -1;
	_bxp->req = (int*)pkg_malloc(_nr*sizeof(int));
	if(!_bxp->req)
	{
		pkg_free(_bxp->xxx);
		return -1;
	}
	
	memset(_bxp->xxx, 0, _nr*sizeof(int));
	memset(_bxp->req, 0, _nr*sizeof(int));
	
	return 0;
}


int tr_byxxx_free(tr_byxxx_p _bxp)
{
	if(!_bxp)
		return -1;
	if(_bxp->xxx)
		pkg_free(_bxp->xxx);
	if(_bxp->req)
		pkg_free(_bxp->req);
	pkg_free(_bxp);
	return 0;
}

tmrec_p tmrec_new()
{
	tmrec_p _trp = NULL;
	_trp = (tmrec_p)pkg_malloc(sizeof(tmrec_t));
	if(!_trp)
		return NULL;
	memset(_trp, 0, sizeof(tmrec_t));
	localtime_r(&_trp->dtstart,&(_trp->ts));
	return _trp;
}

int tmrec_free(tmrec_p _trp)
{
	if(!_trp)
		return -1;
	
	tr_byxxx_free(_trp->byday);
	tr_byxxx_free(_trp->bymday);
	tr_byxxx_free(_trp->byyday);
	tr_byxxx_free(_trp->bymonth);
	tr_byxxx_free(_trp->byweekno);

	/*pkg_free(_trp);*/
	return 0;
}

int tr_parse_dtstart(tmrec_p _trp, char *_in)
{
	if(!_trp || !_in)
		return -1;
	_trp->dtstart = ic_parse_datetime(_in, &(_trp->ts));
	DBG("----->dtstart = %ld | %s\n", (long)_trp->dtstart,
									ctime(&(_trp->dtstart)));
	return (_trp->dtstart==0)?-1:0;
}

int tr_parse_dtend(tmrec_p _trp, char *_in)
{
	struct tm _tm;
	if(!_trp || !_in)
		return -1;
	_trp->dtend = ic_parse_datetime(_in,&_tm);
	DBG("----->dtend = %ld | %s\n",
								(long)_trp->dtend,
								ctime(&(_trp->dtend)));
	return (_trp->dtend==0)?-1:0;
}

int tr_parse_duration(tmrec_p _trp, char *_in)
{
	if(!_trp || !_in)
		return -1;
	_trp->duration = ic_parse_duration(_in);
	return (_trp->duration==0)?-1:0;
}

int tr_parse_until(tmrec_p _trp, char *_in)
{
	struct tm _tm;
	if(!_trp || !_in)
		return -1;
	_trp->until = ic_parse_datetime(_in, &_tm);
	return (_trp->until==0)?-1:0;
}

int tr_parse_freq(tmrec_p _trp, char *_in)
{
	if(!_trp || !_in)
		return -1;
	if(!strcasecmp(_in, "daily"))
	{
		_trp->freq = FREQ_DAILY;
		return 0;
	}
	if(!strcasecmp(_in, "weekly"))
	{
		_trp->freq = FREQ_WEEKLY;
		return 0;
	}
	if(!strcasecmp(_in, "monthly"))
	{
		_trp->freq = FREQ_MONTHLY;
		return 0;
	}
	if(!strcasecmp(_in, "yearly"))
	{
		_trp->freq = FREQ_YEARLY;
		return 0;
	}

	_trp->freq = FREQ_NOFREQ;
	return 0;
}

int tr_parse_interval(tmrec_p _trp, char *_in)
{
	if(!_trp || !_in)
		return -1;
	_trp->interval = strz2int(_in);
	return 0;
}

int tr_parse_byday(tmrec_p _trp, char *_in)
{
	if(!_trp || !_in)
		return -1;
	_trp->byday = ic_parse_byday(_in); 
	return 0;
}

int tr_parse_bymday(tmrec_p _trp, char *_in)
{
	if(!_trp || !_in)
		return -1;
	_trp->bymday = ic_parse_byxxx(_in); 
	return 0;
}

int tr_parse_byyday(tmrec_p _trp, char *_in)
{
	if(!_trp || !_in)
		return -1;
	_trp->byyday = ic_parse_byxxx(_in); 
	return 0;
}

int tr_parse_bymonth(tmrec_p _trp, char *_in)
{
	if(!_trp || !_in)
		return -1;
	_trp->bymonth = ic_parse_byxxx(_in); 
	return 0;
}

int tr_parse_byweekno(tmrec_p _trp, char *_in)
{
	if(!_trp || !_in)
		return -1;
	_trp->byweekno = ic_parse_byxxx(_in); 
	return 0;
}

int tr_parse_wkst(tmrec_p _trp, char *_in)
{
	if(!_trp || !_in)
		return -1;
	_trp->wkst = ic_parse_wkst(_in);
	return 0;
}

int tr_print(tmrec_p _trp)
{
	static char *_wdays[] = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"}; 
	int i;
	
	if(!_trp)
	{
		printf("\n(null)\n");
		return -1;
	}
	printf("Recurrence definition\n-- start time ---\n");
	printf("Sys time: %d\n", (int)_trp->dtstart);
	printf("Time: %02d:%02d:%02d\n", _trp->ts.tm_hour, 
				_trp->ts.tm_min, _trp->ts.tm_sec);
	printf("Date: %s, %04d-%02d-%02d\n", _wdays[_trp->ts.tm_wday],
				_trp->ts.tm_year+1900, _trp->ts.tm_mon+1, _trp->ts.tm_mday);
	printf("---\n");
	printf("End time: %d\n", (int)_trp->dtend);
	printf("Duration: %d\n", (int)_trp->duration);
	printf("Until: %d\n", (int)_trp->until);
	printf("Freq: %d\n", (int)_trp->freq);
	printf("Interval: %d\n", (int)_trp->interval);
	if(_trp->byday)
	{
		printf("Byday: ");
		for(i=0; i<_trp->byday->nr; i++)
			printf(" %d%s", _trp->byday->req[i], _wdays[_trp->byday->xxx[i]]);
		printf("\n");
	}
	if(_trp->bymday)
	{
		printf("Bymday: %d:", _trp->bymday->nr);
		for(i=0; i<_trp->bymday->nr; i++)
			printf(" %d", _trp->bymday->xxx[i]*_trp->bymday->req[i]);
		printf("\n");
	}
	if(_trp->byyday)
	{
		printf("Byyday:");
		for(i=0; i<_trp->byyday->nr; i++)
			printf(" %d", _trp->byyday->xxx[i]*_trp->byyday->req[i]);
		printf("\n");
	}
	if(_trp->bymonth)
	{
		printf("Bymonth: %d:", _trp->bymonth->nr);
		for(i=0; i< _trp->bymonth->nr; i++)
			printf(" %d", _trp->bymonth->xxx[i]*_trp->bymonth->req[i]);
		printf("\n");
	}
	if(_trp->byweekno)
	{
		printf("Byweekno: ");
		for(i=0; i<_trp->byweekno->nr; i++)
			printf(" %d", _trp->byweekno->xxx[i]*_trp->byweekno->req[i]);
		printf("\n");
	}
	printf("Weekstart: %d\n", _trp->wkst);
	return 0;
}

time_t ic_parse_datetime(char *_in, struct tm *_tm)
{
	if(!_in || !_tm)
		return 0;
	
	memset(_tm, 0, sizeof(struct tm));
	_tm->tm_year = _D(_in[0])*1000 + _D(_in[1])*100 
			+ _D(_in[2])*10 + _D(_in[3]) - 1900;
	_tm->tm_mon = _D(_in[4])*10 + _D(_in[5]) - 1;
	_tm->tm_mday = _D(_in[6])*10 + _D(_in[7]);
	_tm->tm_hour = _D(_in[9])*10 + _D(_in[10]);
	_tm->tm_min = _D(_in[11])*10 + _D(_in[12]);
	_tm->tm_sec = _D(_in[13])*10 + _D(_in[14]);
	_tm->tm_isdst = -1 /*daylight*/;
	return mktime(_tm);
}

time_t ic_parse_duration(char *_in)
{
	time_t _t, _ft;
	char *_p;
	int _fl;
	
	if(!_in || (*_in!='+' && *_in!='-' && *_in!='P' && *_in!='p'))
		return 0;
	
	if(*_in == 'P' || *_in=='p')
		_p = _in+1;
	else
	{
		if(strlen(_in)<2 || (_in[1]!='P' && _in[1]!='p'))
			return 0;
		_p = _in+2;
	}
	
	_t = _ft = 0;
	_fl = 1;
	
	while(*_p)
	{
		switch(*_p)
		{
			case '0': case '1': case '2':
			case '3': case '4': case '5':
			case '6': case '7': case '8':
			case '9':
				_t = _t*10 + *_p - '0';
			break;
			
			case 'w':
			case 'W':
				if(!_fl)
					return 0;
				_ft += _t*7*24*3600;
				_t = 0;
			break;
			case 'd':
			case 'D':
				if(!_fl)
					return 0;
				_ft += _t*24*3600;
				_t = 0;
			break;
			case 'h':
			case 'H':
				if(_fl)
					return 0;
				_ft += _t*3600;
				_t = 0;
			break;
			case 'm':
			case 'M':
				if(_fl)
					return 0;
				_ft += _t*60;
				_t = 0;
			break;
			case 's':
			case 'S':
				if(_fl)
					return 0;
				_ft += _t;
				_t = 0;
			break;
			case 't':
			case 'T':
				if(!_fl)
					return 0;
				_fl = 0;
			break;
			default:
				return 0;
		}
		_p++;
	}

	return _ft;
}

tr_byxxx_p ic_parse_byday(char *_in)
{
	tr_byxxx_p _bxp = NULL;
	int _nr, _s, _v;
	char *_p;

	if(!_in)
		return NULL;
	_bxp = tr_byxxx_new();
	if(!_bxp)
		return NULL;
	_p = _in;
	_nr = 1;
	while(*_p)
	{
		if(*_p == ',')
			_nr++;
		_p++;
	}
	if(tr_byxxx_init(_bxp, _nr) < 0)
	{
		tr_byxxx_free(_bxp);
		return NULL;
	}
	_p = _in;
	_nr = _v = 0;
	_s = 1;
	while(*_p && _nr < _bxp->nr)
	{
		switch(*_p)
		{
			case '0': case '1': case '2':
			case '3': case '4': case '5':
			case '6': case '7': case '8':
			case '9':
				_v = _v*10 + *_p - '0';
			break;
			
			case 's':
			case 'S':
				_p++;
				switch(*_p)
				{
					case 'a':
					case 'A':
						_bxp->xxx[_nr] = WDAY_SA;
						_bxp->req[_nr] = _s*_v;
					break;
					case 'u':
					case 'U':
						_bxp->xxx[_nr] = WDAY_SU;
						_bxp->req[_nr] = _s*_v;
					break;
					default:
						goto error;
				}
				_s = 1;
				_v = 0;
			break;
			case 'm':
			case 'M':
				_p++;
				if(*_p!='o' && *_p!='O')
					goto error;
				_bxp->xxx[_nr] = WDAY_MO;
				_bxp->req[_nr] = _s*_v;
				_s = 1;
				_v = 0;
			break;
			case 't':
			case 'T':
				_p++;
				switch(*_p)
				{
					case 'h':
					case 'H':
						_bxp->xxx[_nr] = WDAY_TH;
						_bxp->req[_nr] = _s*_v;
					break;
					case 'u':
					case 'U':
						_bxp->xxx[_nr] = WDAY_TU;
						_bxp->req[_nr] = _s*_v;
					break;
					default:
						goto error;
				}
				_s = 1;
				_v = 0;
			break;
			case 'w':
			case 'W':
				_p++;
				if(*_p!='e' && *_p!='E')
					goto error;
				_bxp->xxx[_nr] = WDAY_WE;
				_bxp->req[_nr] = _s*_v;
				_s = 1;
				_v = 0;
			break;
			case 'f':
			case 'F':
				_p++;
				if(*_p!='r' && *_p!='R')
					goto error;
				_bxp->xxx[_nr] = WDAY_FR;
				_bxp->req[_nr] = _s*_v;
				_s = 1;
				_v = 0;
			break;
			case '-':
				_s = -1;
			break;
			case '+':
			case ' ':
			case '\t':
			break;
			case ',':
				_nr++;
			break;
			default:
				goto error;
		}
		_p++;
	}

	return _bxp;

error:
	tr_byxxx_free(_bxp);
	return NULL;
}

tr_byxxx_p ic_parse_byxxx(char *_in)
{
	tr_byxxx_p _bxp = NULL;
	int _nr, _s, _v;
	char *_p;

	if(!_in)
		return NULL;
	_bxp = tr_byxxx_new();
	if(!_bxp)
		return NULL;
	_p = _in;
	_nr = 1;
	while(*_p)
	{
		if(*_p == ',')
			_nr++;
		_p++;
	}
	if(tr_byxxx_init(_bxp, _nr) < 0)
	{
		tr_byxxx_free(_bxp);
		return NULL;
	}
	_p = _in;
	_nr = _v = 0;
	_s = 1;
	while(*_p && _nr < _bxp->nr)
	{
		switch(*_p)
		{
			case '0': case '1': case '2':
			case '3': case '4': case '5':
			case '6': case '7': case '8':
			case '9':
				_v = _v*10 + *_p - '0';
			break;
			
			case '-':
				_s = -1;
			break;
			case '+':
			case ' ':
			case '\t':
			break;
			case ',':
				_bxp->xxx[_nr] = _v;
				_bxp->req[_nr] = _s;
				_s = 1;
				_v = 0;
				_nr++;
			break;
			default:
				goto error;
		}
		_p++;
	}
	if(_nr < _bxp->nr)
	{
		_bxp->xxx[_nr] = _v;
		_bxp->req[_nr] = _s;
	}
	return _bxp;

error:
	tr_byxxx_free(_bxp);
	return NULL;
}

int ic_parse_wkst(char *_in)
{
	if(!_in || strlen(_in)!=2)
		goto error;
	
	switch(_in[0])
	{
		case 's':
		case 'S':
			switch(_in[1])
			{
				case 'a':
				case 'A':
					return WDAY_SA;
				case 'u':
				case 'U':
					return WDAY_SU;
				default:
					goto error;
			}
		case 'm':
		case 'M':
			if(_in[1]!='o' && _in[1]!='O')
				goto error;
			return WDAY_MO;
		case 't':
		case 'T':
			switch(_in[1])
			{
				case 'h':
				case 'H':
					return WDAY_TH;
				case 'u':
				case 'U':
					return WDAY_TU;
				default:
					goto error;
			}
		case 'w':
		case 'W':
			if(_in[1]!='e' && _in[1]!='E')
				goto error;
			return WDAY_WE;
		case 'f':
		case 'F':
			if(_in[1]!='r' && _in[1]!='R')
				goto error;
			return WDAY_FR;
		break;
		default:
			goto error;
	}
	
error:
#ifdef USE_YWEEK_U
	return WDAY_SU;
#else
	return WDAY_MO;
#endif
}






/*********************** imported from "checktr.c"  **************************/

#define REC_ERR    -1
#define REC_MATCH   0
#define REC_NOMATCH 1

#define _IS_SET(x) (((x)>0)?1:0)

/*** local headers ***/
int get_min_interval(tmrec_p);
int check_min_unit(tmrec_p, ac_tm_p, tr_res_p);
int check_freq_interval(tmrec_p _trp, ac_tm_p _atp);
int check_byxxx(tmrec_p, ac_tm_p);

/**
 *
 * return 0/REC_MATCH - the time falls in
 *       -1/REC_ERR - error
 *        1/REC_NOMATCH - the time falls out
 */
int check_tmrec(tmrec_p _trp, ac_tm_p _atp, tr_res_p _tsw)
{
	if(!_trp || !_atp || (!_IS_SET(_trp->duration) && !_IS_SET(_trp->dtend)))
		return REC_ERR;

	// it is before start date
	if(_atp->time < _trp->dtstart)
		return REC_NOMATCH;
	
	// compute the duration of the recurrence interval
	if(!_IS_SET(_trp->duration))
		_trp->duration = _trp->dtend - _trp->dtstart;
	
	if(_atp->time <= _trp->dtstart+_trp->duration)
	{
		if(_tsw)
		{
			if(_tsw->flag & TSW_RSET)
			{
				if(_tsw->rest>_trp->dtstart+_trp->duration-_atp->time)
					_tsw->rest = _trp->dtstart+_trp->duration - _atp->time;
			}
			else
			{
				_tsw->flag |= TSW_RSET;
				_tsw->rest = _trp->dtstart+_trp->duration - _atp->time;
			}
		}
		return REC_MATCH;
	}
	
	// after the bound of recurrence
	if(_IS_SET(_trp->until) && _atp->time >= _trp->until + _trp->duration)
		return REC_NOMATCH;
	
	// check if the instance of recurrence matches the 'interval'
	if(check_freq_interval(_trp, _atp)!=REC_MATCH)
		return REC_NOMATCH;

	if(check_min_unit(_trp, _atp, _tsw)!=REC_MATCH)
		return REC_NOMATCH;

	if(check_byxxx(_trp, _atp)!=REC_MATCH)
		return REC_NOMATCH;

	return REC_MATCH;
}


int check_freq_interval(tmrec_p _trp, ac_tm_p _atp)
{
	int _t0, _t1;
	struct tm _tm;
	if(!_trp || !_atp)
		return REC_ERR;
	
	if(!_IS_SET(_trp->freq))
		return REC_NOMATCH;
	
	if(!_IS_SET(_trp->interval) || _trp->interval==1)
		return REC_MATCH;
	
	switch(_trp->freq)
	{
		case FREQ_DAILY:
		case FREQ_WEEKLY:
			memset(&_tm, 0, sizeof(struct tm));
			_tm.tm_year = _trp->ts.tm_year;
			_tm.tm_mon = _trp->ts.tm_mon;
			_tm.tm_mday = _trp->ts.tm_mday;
			_t0 = (int)mktime(&_tm);
			memset(&_tm, 0, sizeof(struct tm));
			_tm.tm_year = _atp->t.tm_year;
			_tm.tm_mon = _atp->t.tm_mon;
			_tm.tm_mday = _atp->t.tm_mday;
			_t1 = (int)mktime(&_tm);
			if(_trp->freq == FREQ_DAILY)
				return (((_t1-_t0)/(24*3600))%_trp->interval==0)?
					REC_MATCH:REC_NOMATCH;
#ifdef USE_YWEEK_U
			_t0 -= _trp->ts.tm_wday*24*3600;
			_t1 -= _atp->t.tm_wday*24*3600;
#else
			_t0 -= ((_trp->ts.tm_wday+6)%7)*24*3600;
			_t1 -= ((_atp->t.tm_wday+6)%7)*24*3600;
#endif
			return (((_t1-_t0)/(7*24*3600))%_trp->interval==0)?
					REC_MATCH:REC_NOMATCH;
		case FREQ_MONTHLY:
			_t0 = (_atp->t.tm_year-_trp->ts.tm_year)*12
					+ _atp->t.tm_mon-_trp->ts.tm_mon;
			return (_t0%_trp->interval==0)?REC_MATCH:REC_NOMATCH;
		case FREQ_YEARLY:
			return ((_atp->t.tm_year-_trp->ts.tm_year)%_trp->interval==0)?
					REC_MATCH:REC_NOMATCH;
	}
	
	return REC_NOMATCH;
}

int get_min_interval(tmrec_p _trp)
{
	if(!_trp)
		return FREQ_NOFREQ;
	
	if(_trp->freq == FREQ_DAILY || _trp->byday || _trp->bymday || _trp->byyday)
		return FREQ_DAILY;
	if(_trp->freq == FREQ_WEEKLY || _trp->byweekno) 
		return FREQ_WEEKLY;
	if(_trp->freq == FREQ_MONTHLY || _trp->bymonth)
		return FREQ_MONTHLY;
	if(_trp->freq == FREQ_YEARLY)
		return FREQ_YEARLY;
	
	return FREQ_NOFREQ;
}

int check_min_unit(tmrec_p _trp, ac_tm_p _atp, tr_res_p _tsw)
{
	int _v0, _v1;
	if(!_trp || !_atp)
		return REC_ERR;
	switch(get_min_interval(_trp))
	{
		case FREQ_DAILY:
		break;
		case FREQ_WEEKLY:
			if(_trp->ts.tm_wday != _atp->t.tm_wday)
				return REC_NOMATCH;
		break;
		case FREQ_MONTHLY:
			if(_trp->ts.tm_mday != _atp->t.tm_mday)
				return REC_NOMATCH;
		break;
		case FREQ_YEARLY:
			if(_trp->ts.tm_mon != _atp->t.tm_mon 
					|| _trp->ts.tm_mday != _atp->t.tm_mday)
				return REC_NOMATCH;
		break;
		default:
			return REC_NOMATCH;
	}
	_v0 = _trp->ts.tm_hour*3600 + _trp->ts.tm_min*60 + _trp->ts.tm_sec;
	_v1 = _atp->t.tm_hour*3600 + _atp->t.tm_min*60 + _atp->t.tm_sec;
	if(_v1 >= _v0 && _v1 < _v0 + _trp->duration)
	{
		if(_tsw)
		{
			if(_tsw->flag & TSW_RSET)
			{
				if(_tsw->rest>_v0+_trp->duration-_v1)
					_tsw->rest = _v0 + _trp->duration - _v1;
			}
			else
			{
				_tsw->flag |= TSW_RSET;
				_tsw->rest = _v0 + _trp->duration - _v1;
			}
		}
		return REC_MATCH;
	}
	
	return REC_NOMATCH;
}

int check_byxxx(tmrec_p _trp, ac_tm_p _atp)
{
	int i;
	ac_maxval_p _amp = NULL;
	if(!_trp || !_atp)
		return REC_ERR;
	if(!_trp->byday && !_trp->bymday && !_trp->byyday && !_trp->bymonth 
			&& !_trp->byweekno)
		return REC_MATCH;
	
	_amp = ac_get_maxval(_atp);
	if(!_amp)
		return REC_NOMATCH;
	
	if(_trp->bymonth)
	{
		for(i=0; i<_trp->bymonth->nr; i++)
		{
			if(_atp->t.tm_mon == 
					(_trp->bymonth->xxx[i]*_trp->bymonth->req[i]+12)%12)
				break;
		}
		if(i>=_trp->bymonth->nr)
			return REC_NOMATCH;
	}
	if(_trp->freq==FREQ_YEARLY && _trp->byweekno)
	{
		for(i=0; i<_trp->byweekno->nr; i++)
		{
			if(_atp->yweek == (_trp->byweekno->xxx[i]*_trp->byweekno->req[i]+
							_amp->yweek)%_amp->yweek)
				break;
		}
		if(i>=_trp->byweekno->nr)
			return REC_NOMATCH;
	}
	if(_trp->byyday)
	{
		for(i=0; i<_trp->byyday->nr; i++)
		{
			if(_atp->t.tm_yday == (_trp->byyday->xxx[i]*_trp->byyday->req[i]+
						_amp->yday)%_amp->yday)
				break;
		}
		if(i>=_trp->byyday->nr)
			return REC_NOMATCH;
	}
	if(_trp->bymday)
	{
		for(i=0; i<_trp->bymday->nr; i++)
		{
#ifdef EXTRA_DEBUG
			DBG("Req:bymday: %d == %d\n", _atp->t.tm_mday,
				(_trp->bymday->xxx[i]*_trp->bymday->req[i]+
				_amp->mday)%_amp->mday + ((_trp->bymday->req[i]<0)?1:0));
#endif
			if(_atp->t.tm_mday == (_trp->bymday->xxx[i]*_trp->bymday->req[i]+
						_amp->mday)%_amp->mday + (_trp->bymday->req[i]<0)?1:0)
				break;
		}
		if(i>=_trp->bymday->nr)
			return REC_NOMATCH;
	}
	if(_trp->byday)
	{
		for(i=0; i<_trp->byday->nr; i++)
		{
			if(_trp->freq==FREQ_YEARLY)
			{
#ifdef EXTRA_DEBUG
				DBG("Req:byday:y: %d==%d && %d==%d\n", _atp->t.tm_wday,
					_trp->byday->xxx[i], _atp->ywday+1, 
					(_trp->byday->req[i]+_amp->ywday)%_amp->ywday);
#endif
				if(_atp->t.tm_wday == _trp->byday->xxx[i] &&
						_atp->ywday+1 == (_trp->byday->req[i]+_amp->ywday)%
						_amp->ywday)
					break;
			}
			else
			{
				if(_trp->freq==FREQ_MONTHLY)
				{
#ifdef EXTRA_DEBUG
					DBG("Req:byday:m: %d==%d && %d==%d\n", _atp->t.tm_wday,
						_trp->byday->xxx[i], _atp->mwday+1, 
						(_trp->byday->req[i]+_amp->mwday)%_amp->mwday);
#endif
					if(_atp->t.tm_wday == _trp->byday->xxx[i] &&
							_atp->mwday+1==(_trp->byday->req[i]+
							_amp->mwday)%_amp->mwday)
						break;
				}
				else
				{
					if(_atp->t.tm_wday == _trp->byday->xxx[i])
						break;
				}
			}
		}
		if(i>=_trp->byday->nr)
			return REC_NOMATCH;
	}

	return REC_MATCH;
}


