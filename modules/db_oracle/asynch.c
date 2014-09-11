/*
 * $Id$
 *
 * Oracle module interface
 *
 * Copyright (C) 2007,2008 TRUNK MOBILE
 *
 * This file is part of Kamailio, a free SIP server.
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
 */
/*
 * History:
 * --------
 */

#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <oci.h>
#include "../../dprint.h"
#include "../../sr_module.h"
#include "ora_con.h"
#include "asynch.h"

#define MAX_TIMEOUT_S  10
#define MIN_TIMEOUT_MS 100


/* Default is 3.0 second */
static struct timeval request_tm = { .tv_sec = 3, .tv_usec = 0 };

/* Default is 0.2 second */
static struct timeval restore_tm       = { .tv_sec = 0, .tv_usec = 200*1000 };
static const struct timeval defrest_tm = { .tv_sec = 0, .tv_usec = 200*1000 };

static int synch_mode;
static int cur_asynch_mode;
static struct timeval wtm;


static __inline__ int is_zero_tm(const struct timeval* tv)
{
	return !tv->tv_usec && !tv->tv_sec;
}


/*
 * parse timeout value in syntax: nnn.mmm (sec/ms)
 */
static int set_tv(unsigned type, const char* val, struct timeval* tv)
{
	char *eptr;
	unsigned long s, ms;
	double dv;

	if (PARAM_TYPE_MASK(type) != PARAM_STRING) {
		LM_ERR("type of parameter is not PARAM_STRING\n");
		return -1;
	}

	if (!val || !*val) {
		LM_ERR("empty parameter\n");
		return -1;
	}

	errno = 0;
	dv = strtod(val, &eptr);

	if (*eptr) {
		LM_ERR("invalid parameter string\n");
		return -2;
	}

	if (   errno
	    || dv > (double)MAX_TIMEOUT_S
	    || (dv && dv < ((double)MIN_TIMEOUT_MS)/1000))
	{
		LM_ERR("value must be between 0.%u and %u.0\n",
			MIN_TIMEOUT_MS, MAX_TIMEOUT_S);
		return -3;
	}

	s = (unsigned)dv;
	dv -= (double)s;
	ms = (unsigned)(dv * 1000);
	tv->tv_sec = (time_t)s;
	tv->tv_usec = (suseconds_t)ms;
	return 0;
}


/*
 * set operation timeout
 */
int set_timeout(unsigned type, const char* val)
{
	int rc = set_tv(type, val, &request_tm);

	if (!rc) {
		synch_mode = is_zero_tm(&request_tm);
		if (!synch_mode && is_zero_tm(&restore_tm))
			restore_tm = defrest_tm;
	}

	return rc;
}


/*
 * set (re)connect timeout
 */
int set_reconnect(unsigned type, const char* val)
{
	int rc = set_tv(type, val, &restore_tm);

	if (!synch_mode && is_zero_tm(&restore_tm)) {
		LM_WARN("in asyncronus mode reconnect time can't be zero. "
			"Set default value\n");
		restore_tm = defrest_tm;
	}

	return rc;
}


static sword change_mode(ora_con_t* con)
{
	return OCIAttrSet(con->svchp, OCI_HTYPE_SVCCTX, NULL, 0,
		OCI_ATTR_NONBLOCKING_MODE, con->errhp);
}


/*
 * start timelimited operation (if work in synch mode return SUCCESS)
 */
sword begin_timelimit(ora_con_t* con, int connect)
{
	struct timeval* tv;
	sword status;

	if (synch_mode)
		return OCI_SUCCESS;

	if (connect || cur_asynch_mode) {
		ub1 mode;

		status = OCIAttrGet(con->svchp, OCI_HTYPE_SVCCTX, &mode, NULL,
			OCI_ATTR_NONBLOCKING_MODE, con->errhp);
		if (status != OCI_SUCCESS)
			return status;

		if (mode) {
			status = change_mode(con);
			if (status != OCI_SUCCESS)
				return status;
		}
		cur_asynch_mode = 0;
	}

	status = change_mode(con);
	if (status != OCI_SUCCESS && connect >= 0)
		return status;

	cur_asynch_mode = 1;

	gettimeofday(&wtm, NULL);
	tv = &request_tm;
	if (connect)
		tv = &restore_tm;
	wtm.tv_sec += tv->tv_sec;
	wtm.tv_usec += tv->tv_usec;
	if (wtm.tv_usec >= 1000000) {
		wtm.tv_usec -= 1000000;
		++wtm.tv_sec;
	}

	return OCI_SUCCESS;
}


static sword remap_status(ora_con_t* con, sword status)
{
	sword code;

	if (   status == OCI_ERROR
	    && OCIErrorGet(con->errhp, 1, NULL, &code,
			NULL, 0, OCI_HTYPE_ERROR) == OCI_SUCCESS
	    && (code == 3123 /*|| code == 3127*/))
	{
		status = OCI_STILL_EXECUTING;
	}
	return status;
}


/*
 * check completion of timelimited operation (if work in synch mode return 0)
 */
int wait_timelimit(ora_con_t* con, sword status)
{
	struct timeval cur;

	if (!cur_asynch_mode)
		return 0;

	if (remap_status(con, status) != OCI_STILL_EXECUTING)
		return 0;

	gettimeofday(&cur, NULL);
	return (   cur.tv_sec < wtm.tv_sec
		|| (cur.tv_sec == wtm.tv_sec && cur.tv_usec < wtm.tv_usec));
}


/*
 * close current timelimited operation and disconnect if timeout occured
 * return true only if work in asynch mode and timeout detect
 */
int done_timelimit(ora_con_t* con, sword status)
{
	int ret = 0;

	if (!cur_asynch_mode)
		return 0;

	if (remap_status(con, status) == OCI_STILL_EXECUTING) {
		sword code;

		status = OCIBreak(con->svchp, con->errhp);
		if (status != OCI_SUCCESS)
			LM_ERR("driver: %s\n",
				db_oracle_error(con, status));

		status = OCIReset(con->svchp, con->errhp);
		if (   status == OCI_ERROR
		    && OCIErrorGet(con->errhp, 1, NULL, &code,
			    NULL, 0, OCI_HTYPE_ERROR) == OCI_SUCCESS
		    && code == 1013)
		{
			status = OCI_SUCCESS;
		}
		if (status != OCI_SUCCESS)
			LM_ERR("driver: %s\n",
				db_oracle_error(con, status));
		db_oracle_disconnect(con);
		++ret;
	} else {
		status = change_mode(con);
		if (status != OCI_SUCCESS) {
			LM_ERR("driver: %s\n", db_oracle_error(con, status));
			++ret;
		} else {
			cur_asynch_mode = 0;
		}
	}
	return ret;
}
