/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
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

#ifndef _CPL_TIME_H_
#define _CPL_TIME_H_


/************************ imported from "ac_tm.h"  ***************************/

#include <time.h>


/* USE_YWEEK_U	-- Sunday system - see strftime %U
 * USE_YWEEK_V	-- ISO 8601 - see strftime %V
 * USE_YWEEK_W	-- Monday system - see strftime %W
*/

#ifndef USE_YWEEK_U
# ifndef USE_YWEEK_V
#  ifndef USE_YWEEK_W
#   define USE_YWEEK_W
#  endif
# endif
#endif

#define is_leap_year(yyyy) ((((yyyy)%400))?(((yyyy)%100)?(((yyyy)%4)?0:1):0):1)


typedef struct _ac_maxval
{
	int yweek;
	int yday;
	int ywday;
	int mweek;
	int mday;
	int mwday;
} ac_maxval_t, *ac_maxval_p;

typedef struct _ac_tm
{
	time_t time;
	struct tm t;
	int mweek;
	int yweek;
	int ywday;
	int mwday;
	ac_maxval_p mv;
} ac_tm_t, *ac_tm_p;

ac_tm_p ac_tm_new(void);

int ac_tm_set_time(ac_tm_p, time_t);

int ac_tm_reset(ac_tm_p);
int ac_tm_free(ac_tm_p);

int ac_get_mweek(struct tm*);
int ac_get_yweek(struct tm*);
ac_maxval_p ac_get_maxval(ac_tm_p);
int ac_get_wkst(void);

int ac_print(ac_tm_p);




/************************ imported from "tmrec.h"  ***************************/


#define FREQ_NOFREQ  0
#define FREQ_YEARLY  1
#define FREQ_MONTHLY 2
#define FREQ_WEEKLY  3
#define FREQ_DAILY   4

#define WDAY_SU 0
#define WDAY_MO 1
#define WDAY_TU 2
#define WDAY_WE 3
#define WDAY_TH 4
#define WDAY_FR 5
#define WDAY_SA 6
#define WDAY_NU 7

#define TSW_TSET	1
#define TSW_RSET	2

typedef struct _tr_byxxx
{
	int nr;
	int *xxx;
	int *req;
} tr_byxxx_t, *tr_byxxx_p;

typedef struct _tmrec
{
	time_t dtstart;
	struct tm ts;
	time_t dtend;
	time_t duration;
	time_t until;
	int freq;
	int interval;
	tr_byxxx_p byday;
	tr_byxxx_p bymday;
	tr_byxxx_p byyday;
	tr_byxxx_p bymonth;
	tr_byxxx_p byweekno;
	int wkst;
} tmrec_t, *tmrec_p;

typedef struct _tr_res
{
	int flag;
	time_t rest;
} tr_res_t, *tr_res_p;

tr_byxxx_p tr_byxxx_new(void);
int tr_byxxx_init(tr_byxxx_p, int);
int tr_byxxx_free(tr_byxxx_p);

tmrec_p tmrec_new(void);
int tmrec_free(tmrec_p);

int tr_parse_dtstart(tmrec_p, char*);
int tr_parse_dtend(tmrec_p, char*);
int tr_parse_duration(tmrec_p, char*);
int tr_parse_until(tmrec_p, char*);
int tr_parse_freq(tmrec_p, char*);
int tr_parse_interval(tmrec_p, char*);
int tr_parse_byday(tmrec_p, char*);
int tr_parse_bymday(tmrec_p, char*);
int tr_parse_byyday(tmrec_p, char*);
int tr_parse_bymonth(tmrec_p, char*);
int tr_parse_byweekno(tmrec_p, char*);
int tr_parse_wkst(tmrec_p, char*);

int tr_print(tmrec_p);
time_t ic_parse_datetime(char*,struct tm*);
time_t ic_parse_duration(char*);

tr_byxxx_p ic_parse_byday(char*);
tr_byxxx_p ic_parse_byxxx(char*);
int ic_parse_wkst(char*);

int check_tmrec(tmrec_p, ac_tm_p, tr_res_p);


#endif

