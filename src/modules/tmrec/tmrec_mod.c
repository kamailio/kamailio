/*
 * Copyright (C) 2012 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
/*! \file
 * \brief The TMREC module
 * \ingroup tmrec
 */

/*! \defgroup tmrec TMREC
 * This module provides time recurrence matching functions.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../pvar.h"
#include "../../mod_fix.h"
#include "../../lib/srutils/tmrec.h"
#include "period.h"


MODULE_VERSION

static int  mod_init(void);
static int  child_init(int);
static void mod_destroy(void);

static int w_tmrec_match(struct sip_msg* msg, char* rec, char* t);
static int fixup_tmrec_match(void** param, int param_no);
static int w_is_leap_year(struct sip_msg* msg, char* t, char* p2);
static int fixup_is_leap_year(void** param, int param_no);
static int fixup_time_period_match(void** param, int param_no);
static int w_time_period_match(struct sip_msg* msg, char* period, char* t);

int tmrec_wday = 0;
char tmrec_separator = '|';
char *tmrec_separator_param = NULL;

static cmd_export_t cmds[]={
	{"tmrec_match", (cmd_function)w_tmrec_match, 1, fixup_tmrec_match,
		0, ANY_ROUTE},
	{"tmrec_match", (cmd_function)w_tmrec_match, 2, fixup_tmrec_match,
		0, ANY_ROUTE},
	{"is_leap_year", (cmd_function)w_is_leap_year, 0, fixup_is_leap_year,
		0, ANY_ROUTE},
	{"is_leap_year", (cmd_function)w_is_leap_year, 1, fixup_is_leap_year,
		0, ANY_ROUTE},
	{"time_period_match", (cmd_function)w_time_period_match, 1, fixup_time_period_match,
		0, ANY_ROUTE},
	{"time_period_match", (cmd_function)w_time_period_match, 2, fixup_time_period_match,
		0, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"wday",		INT_PARAM,   &tmrec_wday},
	{"separator",   PARAM_STRING,   &tmrec_separator_param},
	{0, 0, 0}
};

struct module_exports exports = {
	"tmrec",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,
	0,              /* exported MI functions */
	0,              /* exported pseudo-variables */
	0,              /* extra processes */
	mod_init,       /* module initialization function */
	0,              /* response function */
	mod_destroy,    /* destroy function */
	child_init      /* per child init function */
};



/**
 * init module function
 */
static int mod_init(void)
{
	if(tmrec_separator_param!=NULL)
		tmrec_separator = tmrec_separator_param[0];
	return 0;
}

/**
 * @brief Initialize async module children
 */
static int child_init(int rank)
{
	if (rank!=PROC_MAIN)
		return 0;

	return 0;
}
/**
 * destroy module function
 */
static void mod_destroy(void)
{
	return;
}

static int w_is_leap_year(struct sip_msg* msg, char* t, char* str2)
{
	time_t tv;
	struct tm *tb;
	int y;
	
	if(msg==NULL)
		return -1;

	if(t!=NULL)
	{
		if(fixup_get_ivalue(msg, (gparam_t*)t, &y)!=0)
		{
			LM_ERR("invalid time parameter value\n");
			return -1;
		}
	} else {
		tv = time(NULL);
		tb = localtime(&tv);
		y = 1900 + tb->tm_year;
	}

	if(tr_is_leap_year(y))
		return 1;
	return -1;
}

static int fixup_is_leap_year(void** param, int param_no)
{
	if(param_no==1)
		return fixup_igp_null(param, param_no);

	return 0;
}

static int w_tmrec_match(struct sip_msg* msg, char* rec, char* t)
{
	str rv;
	time_t tv;
	int ti;
	ac_tm_t act;
	tmrec_t tmr;

	if(msg==NULL)
		return -1;

	if(fixup_get_svalue(msg, (gparam_t*)rec, &rv)!=0)
	{
		LM_ERR("invalid time recurrence parameter value\n");
		return -1;
	}

	if(t!=NULL)
	{
		if(fixup_get_ivalue(msg, (gparam_t*)t, &ti)!=0)
		{
			LM_ERR("invalid time stamp parameter value\n");
			return -1;
		}
		tv = (time_t)ti;
	} else {
		tv = time(NULL);
	}

	memset(&act, 0, sizeof(act));
	memset(&tmr, 0, sizeof(tmr));

	/* parse time recurrence definition */
	if(tr_parse_recurrence_string(&tmr, rv.s, tmrec_separator)<0)
		return -1;

	/* if there is no dstart, timerec is valid */
	if (tmr.dtstart==0)
		goto done;

	/* set current time */
	if (ac_tm_set_time(&act, tv)<0)
		goto error;

	/* match the specified recurence */
	if (tr_check_recurrence(&tmr, &act, 0)!=0)
		goto error;

done:
	tmrec_destroy(&tmr);
	ac_tm_destroy(&act);
	return 1;

error:
	tmrec_destroy(&tmr);
	ac_tm_destroy(&act);
	return -1;
}

static int fixup_tmrec_match(void** param, int param_no)
{
	if(param_no==1)
	{
		if(fixup_spve_null(param, 1)<0)
			return -1;
		return 0;
	} else if(param_no==2) {
		if(fixup_igp_null(param, 1)<0)
			return -1;
	}
	return 0;
}

static int fixup_time_period_match(void** param, int param_no)
{
	if(param_no==1)
	{
		if(fixup_spve_null(param, 1)<0)
			return -1;
		return 0;
	} else if(param_no==2) {
		if(fixup_igp_null(param, 1)<0)
			return -1;
	}
	return 0;
}

static int w_time_period_match(struct sip_msg* msg, char* period, char* t)
{
	str rv;
	time_t tv;
	int ti;

	if(msg==NULL)
		return -2;

	if(fixup_get_svalue(msg, (gparam_t*)period, &rv)!=0)
	{
		LM_ERR("invalid period parameter value\n");
		return -3;
	}

	if(t!=NULL)
	{
		if(fixup_get_ivalue(msg, (gparam_t*)t, &ti)!=0)
		{
			LM_ERR("invalid time stamp parameter value\n");
			return -4;
		}
		tv = (time_t)ti;
	} else {
		tv = time(NULL);
	}

	if (in_period(tv, rv.s))
		return 1;
	return -1;
}
