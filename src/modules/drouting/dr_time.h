/*
 * Copyright (C) 2005-2009 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */


#ifndef _DR_TIME_H_
#define _DR_TIME_H_


/************************ imported from "ac_tm.h"  ***************************/

#include <time.h>


/* USE_YWEEK_U	-- Sunday system - see strftime %U
 * USE_YWEEK_V	-- ISO 8601 - see strftime %V
 * USE_YWEEK_W	-- Monday system - see strftime %W
*/

#ifndef USE_YWEEK_U
#ifndef USE_YWEEK_V
#ifndef USE_YWEEK_W
#define USE_YWEEK_W
#endif
#endif
#endif

#define is_leap_year(yyyy) \
	((((yyyy) % 400)) ? (((yyyy) % 100) ? (((yyyy) % 4) ? 0 : 1) : 0) : 1)


typedef struct _ac_maxval
{
	int yweek;
	int yday;
	int ywday;
	int mweek;
	int mday;
	int mwday;
} dr_ac_maxval_t, *dr_ac_maxval_p;

typedef struct _ac_tm
{
	time_t time;
	struct tm t;
	int mweek;
	int yweek;
	int ywday;
	int mwday;
	dr_ac_maxval_p mv;
} dr_ac_tm_t, *dr_ac_tm_p;

dr_ac_tm_p dr_ac_tm_new();

int dr_ac_tm_set_time(dr_ac_tm_p, time_t);

int dr_ac_tm_reset(dr_ac_tm_p);
int dr_ac_tm_free(dr_ac_tm_p);

int dr_ac_get_mweek(struct tm *);
int dr_ac_get_yweek(struct tm *);
dr_ac_maxval_p dr_ac_get_maxval(dr_ac_tm_p, int);
int dr_ac_get_wkst();

int dr_ac_print(dr_ac_tm_p);


/************************ imported from "tmrec.h"  ***************************/


#define FREQ_NOFREQ 0
#define FREQ_YEARLY 1
#define FREQ_MONTHLY 2
#define FREQ_WEEKLY 3
#define FREQ_DAILY 4

#define WDAY_SU 0
#define WDAY_MO 1
#define WDAY_TU 2
#define WDAY_WE 3
#define WDAY_TH 4
#define WDAY_FR 5
#define WDAY_SA 6
#define WDAY_NU 7

#define TSW_TSET 1
#define TSW_RSET 2

typedef struct _dr_tr_byxxx
{
	int nr;
	int *xxx;
	int *req;
} dr_tr_byxxx_t, *dr_tr_byxxx_p;

typedef struct _dr_tmrec
{
	time_t dtstart;
	struct tm ts;
	time_t dtend;
	time_t duration;
	time_t until;
	int freq;
	int interval;
	dr_tr_byxxx_p byday;
	dr_tr_byxxx_p bymday;
	dr_tr_byxxx_p byyday;
	dr_tr_byxxx_p bymonth;
	dr_tr_byxxx_p byweekno;
	int wkst;
} dr_tmrec_t, *dr_tmrec_p;

typedef struct _dr_tr_res
{
	int flag;
	time_t rest;
} dr_tr_res_t, *dr_tr_res_p;

dr_tr_byxxx_p dr_tr_byxxx_new();
int dr_tr_byxxx_init(dr_tr_byxxx_p, int);
int dr_tr_byxxx_free(dr_tr_byxxx_p);

dr_tmrec_p dr_tmrec_new();
int dr_tmrec_free(dr_tmrec_p);

int dr_tr_parse_dtstart(dr_tmrec_p, char *);
int dr_tr_parse_dtend(dr_tmrec_p, char *);
int dr_tr_parse_duration(dr_tmrec_p, char *);
int dr_tr_parse_until(dr_tmrec_p, char *);
int dr_tr_parse_freq(dr_tmrec_p, char *);
int dr_tr_parse_interval(dr_tmrec_p, char *);
int dr_tr_parse_byday(dr_tmrec_p, char *);
int dr_tr_parse_bymday(dr_tmrec_p, char *);
int dr_tr_parse_byyday(dr_tmrec_p, char *);
int dr_tr_parse_bymonth(dr_tmrec_p, char *);
int dr_tr_parse_byweekno(dr_tmrec_p, char *);
int dr_tr_parse_wkst(dr_tmrec_p, char *);

int dr_tr_print(dr_tmrec_p);
time_t dr_ic_parse_datetime(char *, struct tm *);
time_t dr_ic_parse_duration(char *);

dr_tr_byxxx_p dr_ic_parse_byday(char *);
dr_tr_byxxx_p dr_ic_parse_byxxx(char *);
int dr_ic_parse_wkst(char *);

int dr_check_tmrec(dr_tmrec_p, dr_ac_tm_p, dr_tr_res_p);


#endif
