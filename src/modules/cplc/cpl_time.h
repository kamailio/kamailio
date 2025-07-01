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
#ifndef USE_YWEEK_V
#ifndef USE_YWEEK_W
#define USE_YWEEK_W
#endif
#endif
#endif

#define is_leap_year(yyyy) \
	((((yyyy) % 400)) ? (((yyyy) % 100) ? (((yyyy) % 4) ? 0 : 1) : 0) : 1)


typedef struct _cpl_ac_maxval
{
	int yweek;
	int yday;
	int ywday;
	int mweek;
	int mday;
	int mwday;
} cpl_ac_maxval_t, *cpl_ac_maxval_p;

typedef struct _cpl_ac_tm
{
	time_t time;
	struct tm t;
	int mweek;
	int yweek;
	int ywday;
	int mwday;
	cpl_ac_maxval_p mv;
} cpl_ac_tm_t, *cpl_ac_tm_p;

cpl_ac_tm_p cpl_ac_tm_new(void);

int cpl_ac_tm_set_time(cpl_ac_tm_p, time_t);

int cpl_ac_tm_reset(cpl_ac_tm_p);
int cpl_ac_tm_free(cpl_ac_tm_p);

int cpl_ac_get_mweek(struct tm *);
int cpl_ac_get_yweek(struct tm *);
cpl_ac_maxval_p cpl_ac_get_maxval(cpl_ac_tm_p);
int cpl_ac_get_wkst(void);

int cpl_ac_print(cpl_ac_tm_p);


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

typedef struct _cpl_tr_byxxx
{
	int nr;
	int *xxx;
	int *req;
} cpl_tr_byxxx_t, *cpl_tr_byxxx_p;

typedef struct _cpl_tmrec
{
	time_t dtstart;
	struct tm ts;
	time_t dtend;
	time_t duration;
	time_t until;
	int freq;
	int interval;
	cpl_tr_byxxx_p byday;
	cpl_tr_byxxx_p bymday;
	cpl_tr_byxxx_p byyday;
	cpl_tr_byxxx_p bymonth;
	cpl_tr_byxxx_p byweekno;
	int wkst;
} cpl_tmrec_t, *cpl_tmrec_p;

typedef struct _cpl_tr_res
{
	int flag;
	time_t rest;
} cpl_tr_res_t, *cpl_tr_res_p;

cpl_tr_byxxx_p cpl_tr_byxxx_new(void);
int cpl_tr_byxxx_init(cpl_tr_byxxx_p, int);
int cpl_tr_byxxx_free(cpl_tr_byxxx_p);

cpl_tmrec_p cpl_tmrec_new(void);
int cpl_tmrec_free(cpl_tmrec_p);

int cpl_tr_parse_dtstart(cpl_tmrec_p, char *);
int cpl_tr_parse_dtend(cpl_tmrec_p, char *);
int cpl_tr_parse_duration(cpl_tmrec_p, char *);
int cpl_tr_parse_until(cpl_tmrec_p, char *);
int cpl_tr_parse_freq(cpl_tmrec_p, char *);
int cpl_tr_parse_interval(cpl_tmrec_p, char *);
int cpl_tr_parse_byday(cpl_tmrec_p, char *);
int cpl_tr_parse_bymday(cpl_tmrec_p, char *);
int cpl_tr_parse_byyday(cpl_tmrec_p, char *);
int cpl_tr_parse_bymonth(cpl_tmrec_p, char *);
int cpl_tr_parse_byweekno(cpl_tmrec_p, char *);
int cpl_tr_parse_wkst(cpl_tmrec_p, char *);

int cpl_tr_print(cpl_tmrec_p);
time_t cpl_ic_parse_datetime(char *, struct tm *);
time_t cpl_ic_parse_duration(char *);

cpl_tr_byxxx_p cpl_ic_parse_byday(char *);
cpl_tr_byxxx_p cpl_ic_parse_byxxx(char *);
int cpl_ic_parse_wkst(char *);

int cpl_check_tmrec(cpl_tmrec_p, cpl_ac_tm_p, cpl_tr_res_p);

#endif
