/*
 * Accounting module
 *
 * Copyright (C) 2011 - Sven Knoblich 1&1 Internet AG
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
 *
 */

/*! \file
 * \ingroup acc
 * \brief Acc:: File to handle CDR generation with the help of the dialog module
 *
 * - Module: \ref acc
 */

/*! \defgroup acc ACC :: The Kamailio accounting Module
 *
 * The ACC module is used to account transactions information to
 *  different backends like syslog, SQL, RADIUS and DIAMETER (beta
 *  version).
 *
 */
#include "../../modules/tm/tm_load.h"
#include "../../core/str.h"
#include "../dialog/dlg_load.h"

#include "acc_cdr.h"
#include "acc_mod.h"
#include "acc_extra.h"
#include "acc.h"

#include "../../lib/srdb1/db.h"

#include <sys/time.h>

/* Solaris does not provide timersub macro in <sys/time.h> */
#ifdef __OS_solaris
#define timersub(tvp, uvp, vvp)                           \
	do {                                                  \
		(vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;    \
		(vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec; \
		if((vvp)->tv_usec < 0) {                          \
			(vvp)->tv_sec--;                              \
			(vvp)->tv_usec += 1000000;                    \
		}                                                 \
	} while(0)
#endif // __OS_solaris

#define TIME_STR_BUFFER_SIZE 20
#define TIME_BUFFER_LENGTH 256
#define TIME_STRING_FORMAT "%Y-%m-%d %H:%M:%S"

struct dlg_binds dlgb;
struct acc_extra *cdr_extra = NULL;
int cdr_facility = LOG_DAEMON;

static const str zero_duration = {"0", 1};
static const char time_separator = {'.'};
static char time_buffer[TIME_BUFFER_LENGTH];
static const str empty_string = {"", 0};

// buffers which are used to collect the crd data for writing
static str *cdr_attrs = NULL;
static str *cdr_value_array = NULL;
static int *cdr_int_array = NULL;
static char *cdr_type_array = NULL;

extern struct tm_binds tmb;
extern str cdr_start_str;
extern str cdr_end_str;
extern str cdr_duration_str;
extern str acc_cdrs_table;
extern int cdr_log_enable;
extern int _acc_cdr_on_failed;

static int string2time(str *time_str, struct timeval *time_value);

/* write all basic information to buffers(e.g. start-time ...) */
int cdr_core2strar(struct dlg_cell *dlg, str *values, int *unused, char *types)
{
	str dlgvals[MAX_CDR_CORE]; /* start, end, duration */
	int i;

	if(!dlg || !values || !types) {
		LM_ERR("invalid input parameter!\n");
		return 0;
	}

	dlgb.get_dlg_varval(dlg, &cdr_start_str, &dlgvals[0]);	  /* start */
	dlgb.get_dlg_varval(dlg, &cdr_end_str, &dlgvals[1]);	  /* end */
	dlgb.get_dlg_varval(dlg, &cdr_duration_str, &dlgvals[2]); /* duration */

	for(i = 0; i < MAX_CDR_CORE; i++) {
		if(dlgvals[i].s != NULL) {
			values[i].s = (char *)pkg_malloc(dlgvals[i].len + 1);
			if(values[i].s == NULL) {
				PKG_MEM_ERROR;
				/* cleanup already allocated memory and
				 * return that we didn't do anything */
				for(i = i - 1; i >= 0; i--) {
					if(NULL != values[i].s) {
						pkg_free(values[i].s);
						values[i].s = NULL;
					}
				}
				return 0;
			}
			memcpy(values[i].s, dlgvals[i].s, dlgvals[i].len);
			values[i].s[dlgvals[i].len] = '\0';
			values[i].len = dlgvals[i].len;
			if(i != 2) {
				/* [0] - start; [1] - end */
				types[i] = TYPE_DATE;
			} else {
				/* [2] - duration */
				types[i] = TYPE_DOUBLE;
			}
		} else {
			values[i] = empty_string;
			types[i] = TYPE_NULL;
		}
	}

	return MAX_CDR_CORE;
}

/* caution: keys need to be aligned to core format */
static db_key_t *db_cdr_keys = NULL;
static db_val_t *db_cdr_vals = NULL;

/* collect all crd data and write it to a syslog */
static int db_write_cdr(struct dlg_cell *dialog, struct sip_msg *message)
{
	int attr_cnt = 0;
	int core_cnt = 0;
	int extra_cnt = 0;
	int i;
	db_func_t *df = NULL;
	db1_con_t *dh = NULL;
	void *vf = NULL;
	void *vh = NULL;
	struct timeval timeval_val;
	long long_val;
	double double_val;
	char *end;
	struct tm t;
	char cdr_time_format_buf[MAX_CDR_CORE][TIME_STR_BUFFER_SIZE];

	if(acc_cdrs_table.len <= 0)
		return 0;

	if(acc_get_db_handlers(&vf, &vh) < 0) {
		LM_ERR("cannot get db handlers\n");
		return -1;
	}
	df = (db_func_t *)vf;
	dh = (db1_con_t *)vh;

	/* get default values */
	core_cnt = cdr_core2strar(
			dialog, cdr_value_array, cdr_int_array, cdr_type_array);
	attr_cnt += core_cnt;

	for(i = 0; i < core_cnt; i++) {
		db_cdr_keys[i] = &cdr_attrs[i];
		/* reset errno, some strtoX don't reset it */
		errno = 0;
		switch(cdr_type_array[i]) {
			case TYPE_NULL:
				VAL_NULL(db_cdr_vals + i) = 1;
				break;
			case TYPE_INT:
				VAL_TYPE(db_cdr_vals + i) = DB1_INT;
				VAL_NULL(db_cdr_vals + i) = 0;
				long_val = strtol(cdr_value_array[i].s, &end, 10);
				if(errno && (errno != EAGAIN)) {
					LM_ERR("failed to convert string to integer - %d.\n",
							errno);
					goto error;
				}
				VAL_INT(db_cdr_vals + i) = long_val;
				break;
			case TYPE_STR:
				VAL_TYPE(db_cdr_vals + i) = DB1_STR;
				VAL_NULL(db_cdr_vals + i) = 0;
				VAL_STR(db_cdr_vals + i) = cdr_value_array[i];
				break;
			case TYPE_DATE:
				VAL_NULL(db_cdr_vals + i) = 0;
				if(string2time(&cdr_value_array[i], &timeval_val) < 0) {
					LM_ERR("failed to convert string to timeval.\n");
					goto error;
				}
				if(acc_time_mode == 4) {
					VAL_TYPE(db_cdr_vals + i) = DB1_STRING;
					gmtime_r(&timeval_val.tv_sec, &t);
					/* Convert time_t structure to format accepted by the database */
					if(strftime(cdr_time_format_buf[i], TIME_STR_BUFFER_SIZE,
							   TIME_STRING_FORMAT, &t)
							<= 0) {
						cdr_time_format_buf[i][0] = '\0';
					}

					VAL_STRING(db_cdr_vals + i) = cdr_time_format_buf[i];
				} else {
					VAL_TYPE(db_cdr_vals + i) = DB1_DATETIME;
					VAL_TIME(db_cdr_vals + i) = timeval_val.tv_sec;
				}
				break;
			case TYPE_DOUBLE:
				VAL_TYPE(db_cdr_vals + i) = DB1_DOUBLE;
				VAL_NULL(db_cdr_vals + i) = 0;
				double_val = strtod(cdr_value_array[i].s, &end);
				if(errno && (errno != EAGAIN)) {
					LM_ERR("failed to convert string to double - %d.\n", errno);
					goto error;
				}
				VAL_DOUBLE(db_cdr_vals + i) = double_val;
				break;
		}
	}

	/* get extra values */
	if(message) {
		extra_cnt = extra2strar(cdr_extra, message, cdr_value_array + attr_cnt,
				cdr_int_array + attr_cnt, cdr_type_array + attr_cnt);
		attr_cnt += extra_cnt;
		;
	} else if(cdr_expired_dlg_enable) {
		LM_WARN("fallback to dlg_only search because of message doesn't "
				"exist.\n");
		extra_cnt = extra2strar_dlg_only(cdr_extra, dialog,
				cdr_value_array + attr_cnt, cdr_int_array + attr_cnt,
				cdr_type_array + attr_cnt, &dlgb);
		attr_cnt += extra_cnt;
	}

	for(; i < attr_cnt; i++) {
		db_cdr_keys[i] = &cdr_attrs[i];

		if(cdr_extra_nullable == 1 && cdr_type_array[i] == TYPE_NULL) {
			VAL_NULL(db_cdr_vals + i) = 1;
		} else {
			VAL_TYPE(db_cdr_vals + i) = DB1_STR;
			VAL_NULL(db_cdr_vals + i) = 0;
			VAL_STR(db_cdr_vals + i) = cdr_value_array[i];
		}
	}

	if(df->use_table(dh, &acc_cdrs_table /*table*/) < 0) {
		LM_ERR("error in use_table\n");
		goto error;
	}

	if(acc_db_insert_mode == 1 && df->insert_delayed != NULL) {
		if(df->insert_delayed(dh, db_cdr_keys, db_cdr_vals, attr_cnt) < 0) {
			LM_ERR("failed to insert delayed into database\n");
			goto error;
		}
	} else if(acc_db_insert_mode == 2 && df->insert_async != NULL) {
		if(df->insert_async(dh, db_cdr_keys, db_cdr_vals, attr_cnt) < 0) {
			LM_ERR("failed to insert async into database\n");
			goto error;
		}
	} else {
		if(df->insert(dh, db_cdr_keys, db_cdr_vals, attr_cnt) < 0) {
			LM_ERR("failed to insert into database\n");
			goto error;
		}
	}

	/* Free memory allocated by core+extra attrs */
	free_strar_mem(
			&(cdr_type_array[0]), &(cdr_value_array[0]), attr_cnt, attr_cnt);
	return 0;

error:
	/* Free memory allocated by core+extra attrs */
	free_strar_mem(
			&(cdr_type_array[0]), &(cdr_value_array[0]), attr_cnt, attr_cnt);
	return -1;
}

/* collect all crd data and write it to a syslog */
static int log_write_cdr(struct dlg_cell *dialog, struct sip_msg *message)
{
	static char cdr_message[MAX_SYSLOG_SIZE];
	static char *const cdr_message_end =
			cdr_message + MAX_SYSLOG_SIZE
			- 2; // -2 because of the string ending '\n\0'
	char *message_position = NULL;
	int counter = 0;
	int attr_cnt = 0;
	int core_cnt = 0;
	int extra_cnt = 0;

	if(cdr_log_enable == 0)
		return 0;

	/* get default values */
	core_cnt = cdr_core2strar(
			dialog, cdr_value_array, cdr_int_array, cdr_type_array);
	attr_cnt += core_cnt;

	/* get extra values */
	if(message) {
		extra_cnt += extra2strar(cdr_extra, message, cdr_value_array + attr_cnt,
				cdr_int_array + attr_cnt, cdr_type_array + attr_cnt);
		attr_cnt += extra_cnt;
		;
	} else if(cdr_expired_dlg_enable) {
		LM_DBG("fallback to dlg_only search because of message does not "
			   "exist.\n");
		extra_cnt += extra2strar_dlg_only(cdr_extra, dialog,
				cdr_value_array + attr_cnt, cdr_int_array + attr_cnt,
				cdr_type_array + attr_cnt, &dlgb);
		attr_cnt += extra_cnt;
		;
	}

	for(counter = 0, message_position = cdr_message; counter < attr_cnt;
			counter++) {
		const char *const next_message_end =
				message_position + 2 +		 // ', ' -> two letters
				cdr_attrs[counter].len + 1 + // '=' -> one letter
				cdr_value_array[counter].len;

		if(next_message_end >= cdr_message_end
				|| next_message_end < message_position) {
			LM_WARN("cdr message too long, truncating..\n");
			message_position = cdr_message_end;
			break;
		}

		if(counter > 0) {
			*(message_position++) = A_SEPARATOR_CHR;
			*(message_position++) = A_SEPARATOR_CHR_2;
		}

		memcpy(message_position, cdr_attrs[counter].s, cdr_attrs[counter].len);

		message_position += cdr_attrs[counter].len;

		*(message_position++) = A_EQ_CHR;

		memcpy(message_position, cdr_value_array[counter].s,
				cdr_value_array[counter].len);

		message_position += cdr_value_array[counter].len;
	}

	/* terminating line */
	*(message_position++) = '\n';
	*(message_position++) = '\0';

	LM_GEN2(cdr_facility, log_level, "%s", cdr_message);

	/* Free memory allocated by core+extra attrs */
	free_strar_mem(
			&(cdr_type_array[0]), &(cdr_value_array[0]), attr_cnt, attr_cnt);
	return 0;
}

/* collect all crd data and write it to a syslog */
static int write_cdr(struct dlg_cell *dialog, struct sip_msg *message)
{
	int ret = 0;

	if(!dialog) {
		LM_ERR("dialog is empty!");
		return -1;
	}

	/* engines decide if they have cdr_expired_dlg_enable set or not */
	cdr_run_engines(dialog, message);

	/* message can be null when logging expired dialogs  */
	if(!cdr_expired_dlg_enable && !message) {
		LM_ERR("message is empty!");
		return -1;
	}

	/* Skip cdr if cdr_skip dlg_var exists */
	if(cdr_skip.len > 0) {
		str nocdr_val = {0};
		dlgb.get_dlg_varval(dialog, &cdr_skip, &nocdr_val);
		if(nocdr_val.s) {
			LM_DBG("cdr_skip dlg_var set, skip cdr!");
			return 0;
		}
	}

	ret = log_write_cdr(dialog, message);
	ret |= db_write_cdr(dialog, message);
	return ret;
}

/* convert a string into a timeval struct */
static int string2time(str *time_str, struct timeval *time_value)
{
	char *dot_address = NULL;
	int dot_position = -1;
	char zero_terminated_value[TIME_STR_BUFFER_SIZE];

	if(!time_str || !time_str->s) {
		LM_ERR("time_str is empty!");
		return -1;
	}

	if(time_str->len >= TIME_STR_BUFFER_SIZE) {
		LM_ERR("time_str is too long %d >= %d!", time_str->len,
				TIME_STR_BUFFER_SIZE);
		return -1;
	}

	memcpy(zero_terminated_value, time_str->s, time_str->len);
	zero_terminated_value[time_str->len] = '\0';

	dot_address = strchr(zero_terminated_value, time_separator);

	if(!dot_address) {
		LM_ERR("failed to find separator('%c') in '%s'!\n", time_separator,
				zero_terminated_value);
		return -1;
	}

	dot_position = dot_address - zero_terminated_value + 1;

	if(dot_position >= strlen(zero_terminated_value)
			|| strchr(dot_address + 1, time_separator)) {
		LM_ERR("invalid time-string '%s'\n", zero_terminated_value);
		return -1;
	}

	time_value->tv_sec = strtol(zero_terminated_value, (char **)NULL, 10);
	time_value->tv_usec = strtol(dot_address + 1, (char **)NULL, 10)
						  * 1000; // restore usec precision
	return 0;
}

/* convert a timeval struct into a string */
static int time2string(struct timeval *time_value, str *time_str)
{
	int buffer_length;

	if(!time_value) {
		LM_ERR("time_value or any of its fields is empty!\n");
		return -1;
	}

	buffer_length = snprintf(time_buffer, TIME_BUFFER_LENGTH, "%ld%c%03d",
			(long int)time_value->tv_sec, time_separator,
			(int)(time_value->tv_usec / 1000));

	if(buffer_length < 0) {
		LM_ERR("failed to write to buffer.\n");
		return -1;
	}

	time_str->s = time_buffer;
	time_str->len = buffer_length;
	return 0;
}

/* set the duration in the dialog struct */
static int set_duration(struct dlg_cell *dialog)
{
	struct timeval start_time;
	struct timeval end_time;
	struct timeval duration_time;
	str duration_str;
	str dval = {0};

	if(!dialog) {
		LM_ERR("dialog is empty!\n");
		return -1;
	}

	dlgb.get_dlg_varval(dialog, &cdr_start_str, &dval);
	if(string2time(&dval, &start_time) < 0) {
		LM_ERR("failed to extract start time\n");
		return -1;
	}
	dlgb.get_dlg_varval(dialog, &cdr_end_str, &dval);
	if(string2time(&dval, &end_time) < 0) {
		LM_ERR("failed to extract end time\n");
		return -1;
	}

	timersub(&end_time, &start_time, &duration_time);

	if(time2string(&duration_time, &duration_str) < 0) {
		LM_ERR("failed to convert current time to string\n");
		return -1;
	}

	if(dlgb.set_dlg_var(dialog, &cdr_duration_str, &duration_str) != 0) {
		LM_ERR("failed to set duration time");
		return -1;
	}

	return 0;
}

/* set the current time as start-time in the dialog struct */
static int set_start_time(struct dlg_cell *dialog)
{
	struct timeval current_time;
	str start_time;

	if(!dialog) {
		LM_ERR("dialog is empty!\n");
		return -1;
	}

	if(gettimeofday(&current_time, NULL) < 0) {
		LM_ERR("failed to get current time!\n");
		return -1;
	}

	if(time2string(&current_time, &start_time) < 0) {
		LM_ERR("failed to convert current time to string\n");
		return -1;
	}

	if(dlgb.set_dlg_var(dialog, (str *)&cdr_start_str, (str *)&start_time)
			!= 0) {
		LM_ERR("failed to set start time\n");
		return -1;
	}

	if(dlgb.set_dlg_var(dialog, (str *)&cdr_end_str, (str *)&start_time) != 0) {
		LM_ERR("failed to set initiation end time\n");
		return -1;
	}

	if(dlgb.set_dlg_var(dialog, (str *)&cdr_duration_str, (str *)&zero_duration)
			!= 0) {
		LM_ERR("failed to set initiation duration time\n");
		return -1;
	}

	return 0;
}

/* set the current time as end-time in the dialog struct */
static int set_end_time(struct dlg_cell *dialog)
{
	struct timeval current_time;
	str end_time;

	if(!dialog) {
		LM_ERR("dialog is empty!\n");
		return -1;
	}

	if(gettimeofday(&current_time, NULL) < 0) {
		LM_ERR("failed to set time!\n");
		return -1;
	}

	if(time2string(&current_time, &end_time) < 0) {
		LM_ERR("failed to convert current time to string\n");
		return -1;
	}

	if(dlgb.set_dlg_var(dialog, (str *)&cdr_end_str, (str *)&end_time) != 0) {
		LM_ERR("failed to set start time");
		return -1;
	}

	return 0;
}

/* callback for a confirmed (INVITE) dialog. */
static void cdr_on_start(
		struct dlg_cell *dialog, int type, struct dlg_cb_params *params)
{
	if(!dialog) {
		LM_ERR("invalid values\n!");
		return;
	}

	if(cdr_start_on_confirmed == 0) {
		return;
	}

	if(set_start_time(dialog) != 0) {
		LM_ERR("failed to set start time!\n");
		return;
	}
}

/* callback for a failure during a dialog. */
static void cdr_on_failed(
		struct dlg_cell *dialog, int type, struct dlg_cb_params *params)
{
	struct sip_msg *msg = 0;

	if(!dialog || !params) {
		LM_ERR("invalid values\n!");
		return;
	}

	if(params->rpl && params->rpl != FAKED_REPLY) {
		msg = params->rpl;
	} else if(params->req) {
		msg = params->req;
	} else {
		LM_ERR("request and response are invalid!");
		return;
	}

	if(write_cdr(dialog, msg) != 0) {
		LM_ERR("failed to write cdr!\n");
		return;
	}
}

/* callback for the finish of a dialog (reply to BYE). */
void cdr_on_end_confirmed(
		struct dlg_cell *dialog, int type, struct dlg_cb_params *params)
{
	if(!dialog || !params) {
		LM_ERR("invalid values\n!");
		return;
	}

	if(write_cdr(dialog, params->req) != 0) {
		LM_ERR("failed to write cdr!\n");
		return;
	}
}

/* callback for the end of a dialog (BYE). */
static void cdr_on_end(
		struct dlg_cell *dialog, int type, struct dlg_cb_params *params)
{
	if(!dialog) {
		LM_ERR("invalid values\n!");
		return;
	}

	if(set_end_time(dialog) != 0) {
		LM_ERR("failed to set end time!\n");
		return;
	}

	if(set_duration(dialog) != 0) {
		LM_ERR("failed to set duration!\n");
		return;
	}
}

/* callback for an expired dialog. */
static void cdr_on_expired(
		struct dlg_cell *dialog, int type, struct dlg_cb_params *params)
{
	if(!dialog || !params) {
		LM_ERR("invalid values\n!");
		return;
	}

	LM_DBG("dialog '%p' expired!\n", dialog);
	/* compute duration for timed out acknowledged dialog */
	if(params && params->dlg_data) {
		if((void *)CONFIRMED_DIALOG_STATE == params->dlg_data) {
			if(set_end_time(dialog) != 0) {
				LM_ERR("failed to set end time!\n");
				return;
			}

			if(set_duration(dialog) != 0) {
				LM_ERR("failed to set duration!\n");
				return;
			}
		}
	}

	if(cdr_expired_dlg_enable && (write_cdr(dialog, 0) != 0)) {
		LM_ERR("failed to write cdr!\n");
		return;
	}
}

/* callback for the cleanup of a dialog. */
static void cdr_on_destroy(
		struct dlg_cell *dialog, int type, struct dlg_cb_params *params)
{
	if(!dialog) {
		LM_ERR("invalid values\n!");
		return;
	}

	LM_DBG("dialog '%p' destroyed!\n", dialog);
}

/* callback for the creation of a dialog. */
static void cdr_on_create(
		struct dlg_cell *dialog, int type, struct dlg_cb_params *params)
{
	if(!dialog) {
		LM_ERR("invalid values\n!");
		return;
	}

	if(cdr_enable == 0) {
		return;
	}

	if(dlgb.register_dlgcb(dialog, DLGCB_CONFIRMED, cdr_on_start, 0, 0) != 0) {
		LM_ERR("can't register create dialog CONFIRM callback\n");
		return;
	}

	if(_acc_cdr_on_failed == 1) {
		if(dlgb.register_dlgcb(dialog, DLGCB_FAILED, cdr_on_failed, 0, 0)
				!= 0) {
			LM_ERR("can't register create dialog FAILED callback\n");
			return;
		}
	}

	if(dlgb.register_dlgcb(dialog, DLGCB_TERMINATED, cdr_on_end, 0, 0) != 0) {
		LM_ERR("can't register create dialog TERMINATED callback\n");
		return;
	}

	if(dlgb.register_dlgcb(
			   dialog, DLGCB_TERMINATED_CONFIRMED, cdr_on_end_confirmed, 0, 0)
			!= 0) {
		LM_ERR("can't register create dialog TERMINATED CONFIRMED callback\n");
		return;
	}

	if(dlgb.register_dlgcb(dialog, DLGCB_EXPIRED, cdr_on_expired, 0, 0) != 0) {
		LM_ERR("can't register create dialog EXPIRED callback\n");
		return;
	}

	if(dlgb.register_dlgcb(dialog, DLGCB_DESTROY, cdr_on_destroy, 0, 0) != 0) {
		LM_ERR("can't register create dialog DESTROY callback\n");
		return;
	}

	LM_DBG("dialog '%p' created!", dialog);

	if(set_start_time(dialog) != 0) {
		LM_ERR("failed to set start time");
		return;
	}
}

/* callback for loading a dialog from database */
static void cdr_on_load(
		struct dlg_cell *dialog, int type, struct dlg_cb_params *params)
{
	if(!dialog) {
		LM_ERR("invalid values\n!");
		return;
	}

	if(cdr_enable == 0) {
		return;
	}

	if(dlgb.register_dlgcb(dialog, DLGCB_CONFIRMED, cdr_on_start, 0, 0) != 0) {
		LM_ERR("can't register create dialog CONFIRM callback\n");
		return;
	}

	if(_acc_cdr_on_failed == 1) {
		if(dlgb.register_dlgcb(dialog, DLGCB_FAILED, cdr_on_failed, 0, 0)
				!= 0) {
			LM_ERR("can't register create dialog FAILED callback\n");
			return;
		}
	}

	if(dlgb.register_dlgcb(dialog, DLGCB_TERMINATED, cdr_on_end, 0, 0) != 0) {
		LM_ERR("can't register create dialog TERMINATED callback\n");
		return;
	}

	if(dlgb.register_dlgcb(
			   dialog, DLGCB_TERMINATED_CONFIRMED, cdr_on_end_confirmed, 0, 0)
			!= 0) {
		LM_ERR("can't register create dialog TERMINATED CONFIRMED callback\n");
		return;
	}

	if(dlgb.register_dlgcb(dialog, DLGCB_EXPIRED, cdr_on_expired, 0, 0) != 0) {
		LM_ERR("can't register create dialog EXPIRED callback\n");
		return;
	}

	if(dlgb.register_dlgcb(dialog, DLGCB_DESTROY, cdr_on_destroy, 0, 0) != 0) {
		LM_ERR("can't register create dialog DESTROY callback\n");
		return;
	}

	LM_DBG("dialog '%p' loaded and callbacks registered\n", dialog);
}

/* convert the extra-data string into a list and store it */
int set_cdr_extra(char *cdr_extra_value)
{
	struct acc_extra *extra = 0;
	int counter = 0;

	if(cdr_extra_value && (cdr_extra = parse_acc_extra(cdr_extra_value)) == 0) {
		LM_ERR("failed to parse crd_extra param\n");
		return -1;
	}

	/* fixed core attributes */
	cdr_attrs[counter++] = cdr_start_str;
	cdr_attrs[counter++] = cdr_end_str;
	cdr_attrs[counter++] = cdr_duration_str;

	for(extra = cdr_extra; extra; extra = extra->next) {
		cdr_attrs[counter++] = extra->name;
	}

	return 0;
}

/* convert the facility-name string into an id and store it */
int set_cdr_facility(char *cdr_facility_str)
{
	int facility_id = -1;

	if(!cdr_facility_str) {
		LM_ERR("facility is empty\n");
		return -1;
	}

	facility_id = str2facility(cdr_facility_str);

	if(facility_id == -1) {
		LM_ERR("invalid cdr facility configured\n");
		return -1;
	}

	cdr_facility = facility_id;

	return 0;
}

/* initialization of all necessary callbacks to track a dialog */
int init_cdr_generation(void)
{
	if(load_dlg_api(&dlgb) != 0) {
		LM_ERR("can't load dialog API\n");
		return -1;
	}

	if(dlgb.register_dlgcb(0, DLGCB_CREATED, cdr_on_create, 0, 0) != 0) {
		LM_ERR("can't register create callback\n");
		return -1;
	}

	if(dlgb.register_dlgcb(0, DLGCB_LOADED, cdr_on_load, 0, 0) != 0) {
		LM_ERR("can't register create callback\n");
		return -1;
	}

	return 0;
}

/* convert the facility-name string into an id and store it */
void destroy_cdr_generation(void)
{
	if(!cdr_extra) {
		return;
	}

	destroy_extras(cdr_extra);
}

/**
 * @brief execute all acc engines for a SIP request event
 */
int cdr_run_engines(struct dlg_cell *dlg, struct sip_msg *msg)
{
	cdr_info_t inf;
	cdr_engine_t *e;

	e = cdr_api_get_engines();

	if(e == NULL)
		return 0;

	memset(&inf, 0, sizeof(cdr_info_t));
	inf.varr = cdr_value_array;
	inf.iarr = cdr_int_array;
	inf.tarr = cdr_type_array;
	while(e) {
		e->cdr_write(dlg, msg, &inf);
		e = e->next;
	}
	return 0;
}

/**
 * @brief set hooks to acc_info_t attributes
 */
void cdr_api_set_arrays(cdr_info_t *inf)
{
	inf->varr = cdr_value_array;
	inf->iarr = cdr_int_array;
	inf->tarr = cdr_type_array;
}

int cdr_arrays_alloc(void)
{
	if((cdr_attrs = pkg_malloc((MAX_CDR_CORE + cdr_extra_size) * sizeof(str)))
			== NULL) {
		PKG_MEM_ERROR_FMT("failed to alloc cdr_attrs\n");
		return -1;
	}

	if((cdr_value_array = pkg_malloc(
				(MAX_CDR_CORE + cdr_extra_size) * sizeof(str)))
			== NULL) {
		PKG_MEM_ERROR_FMT("failed to alloc cdr_value_array\n");
		cdr_arrays_free();
		return -1;
	}

	if((cdr_int_array = pkg_malloc(
				(MAX_CDR_CORE + cdr_extra_size) * sizeof(int)))
			== NULL) {
		PKG_MEM_ERROR_FMT("failed to alloc cdr_int_array\n");
		cdr_arrays_free();
		return -1;
	}

	if((cdr_type_array = pkg_malloc(
				(MAX_CDR_CORE + cdr_extra_size) * sizeof(char)))
			== NULL) {
		PKG_MEM_ERROR_FMT("failed to alloc cdr_type_array\n");
		cdr_arrays_free();
		return -1;
	}

	if((db_cdr_keys = pkg_malloc(
				(MAX_CDR_CORE + cdr_extra_size) * sizeof(db_key_t)))
			== NULL) {
		PKG_MEM_ERROR_FMT("failed to alloc db_cdr_keys\n");
		cdr_arrays_free();
		return -1;
	}

	if((db_cdr_vals = pkg_malloc(
				(MAX_CDR_CORE + cdr_extra_size) * sizeof(db_val_t)))
			== NULL) {
		PKG_MEM_ERROR_FMT("failed to alloc db_cdr_vals\n");
		cdr_arrays_free();
		return -1;
	}

	return 1;
}

void cdr_arrays_free(void)
{
	if(cdr_attrs) {
		pkg_free(cdr_attrs);
	}

	if(cdr_value_array) {
		pkg_free(cdr_value_array);
	}

	if(cdr_int_array) {
		pkg_free(cdr_int_array);
	}

	if(cdr_type_array) {
		pkg_free(cdr_type_array);
	}

	if(db_cdr_keys) {
		pkg_free(db_cdr_keys);
	}

	if(db_cdr_vals) {
		pkg_free(db_cdr_vals);
	}

	return;
}
