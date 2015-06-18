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
#include "../../str.h"
#include "../dialog/dlg_load.h"

#include "acc_api.h"
#include "acc_cdr.h"
#include "acc_mod.h"
#include "acc_extra.h"
#include "acc.h"

#ifdef SQL_ACC
#include "../../lib/srdb1/db.h"
#endif

#include <sys/time.h>

/* Solaris does not provide timersub macro in <sys/time.h> */
#ifdef __OS_solaris
#define timersub(tvp, uvp, vvp)                     \
    do {                                \
        (vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;      \
        (vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;   \
        if ((vvp)->tv_usec < 0) {               \
            (vvp)->tv_sec--;                \
            (vvp)->tv_usec += 1000000;          \
        }                           \
    } while (0)
#endif // __OS_solaris

#define TIME_STR_BUFFER_SIZE 20
#define TIME_BUFFER_LENGTH 256

struct dlg_binds dlgb;
struct acc_extra* cdr_extra = NULL;
int cdr_facility = LOG_DAEMON;

static const str zero_duration = { "0", 1};
static const char time_separator = {'.'};
static char time_buffer[ TIME_BUFFER_LENGTH];
static const str empty_string = { "", 0};

// buffers which are used to collect the crd data for writing
static str cdr_attrs[ MAX_CDR_CORE + MAX_CDR_EXTRA];
static str cdr_value_array[ MAX_CDR_CORE + MAX_CDR_EXTRA];
static int cdr_int_array[ MAX_CDR_CORE + MAX_CDR_EXTRA];
static char cdr_type_array[ MAX_CDR_CORE + MAX_CDR_EXTRA];

extern struct tm_binds tmb;
extern str cdr_start_str;
extern str cdr_end_str;
extern str cdr_duration_str;
extern str acc_cdrs_table;
extern int cdr_log_enable;
extern int _acc_cdr_on_failed;

static int string2time( str* time_str, struct timeval* time_value);

/* write all basic information to buffers(e.g. start-time ...) */
static int cdr_core2strar( struct dlg_cell* dlg,
                           str* values,
                           int* unused,
                           char* types)
{
    str* start = NULL;
    str* end = NULL;
    str* duration = NULL;

    if( !dlg || !values || !types)
    {
        LM_ERR( "invalid input parameter!\n");
        return 0;
    }

    start = dlgb.get_dlg_var( dlg, (str*)&cdr_start_str);
    end = dlgb.get_dlg_var( dlg, (str*)&cdr_end_str);
    duration = dlgb.get_dlg_var( dlg, (str*)&cdr_duration_str);

    values[0] = ( start != NULL ? *start : empty_string);
    types[0] = ( start != NULL ? TYPE_DATE : TYPE_NULL);

    values[1] = ( end != NULL ? *end : empty_string);
    types[1] = ( end != NULL ? TYPE_DATE : TYPE_NULL);

    values[2] = ( duration != NULL ? *duration : empty_string);
    types[2] = ( duration != NULL ? TYPE_DOUBLE : TYPE_NULL);

    return MAX_CDR_CORE;
}

#ifdef SQL_ACC
/* caution: keys need to be aligned to core format */
static db_key_t db_cdr_keys[ MAX_CDR_CORE + MAX_CDR_EXTRA];
static db_val_t db_cdr_vals[ MAX_CDR_CORE + MAX_CDR_EXTRA];

/* collect all crd data and write it to a syslog */
static int db_write_cdr( struct dlg_cell* dialog,
                      struct sip_msg* message)
{
	int m = 0;
	int n = 0;
	int i;
	db_func_t *df=NULL;
	db1_con_t *dh=NULL;
	void *vf=NULL;
	void *vh=NULL;
	struct timeval timeval_val;
	long long_val;
	double double_val;
	char * end;

	if(acc_cdrs_table.len<=0)
		return 0;

	if(acc_get_db_handlers(&vf, &vh)<0) {
		LM_ERR("cannot get db handlers\n");
		return -1;
	}
	df = (db_func_t*)vf;
	dh = (db1_con_t*)vh;

	/* get default values */
	m = cdr_core2strar( dialog,
						cdr_value_array,
						cdr_int_array,
						cdr_type_array);

	for(i=0; i<m; i++) {
		db_cdr_keys[i] = &cdr_attrs[i];
		switch(cdr_type_array[i]) {
			case TYPE_NULL:
				VAL_NULL(db_cdr_vals+i)=1;
				break;
			case TYPE_INT:
				VAL_TYPE(db_cdr_vals+i)=DB1_INT;
				VAL_NULL(db_cdr_vals+i)=0;
				long_val = strtol(cdr_value_array[i].s, &end, 10);
				if(errno && (errno != EAGAIN)) {
					LM_ERR("failed to convert string to integer - %d.\n", errno);
					goto error;
				}
				VAL_INT(db_cdr_vals+i) = long_val;
				break;
			case TYPE_STR:
				VAL_TYPE(db_cdr_vals+i)=DB1_STR;
				VAL_NULL(db_cdr_vals+i)=0;
				VAL_STR(db_cdr_vals+i) = cdr_value_array[i];
				break;
			case TYPE_DATE:
				VAL_TYPE(db_cdr_vals+i)=DB1_DATETIME;
				VAL_NULL(db_cdr_vals+i)=0;
				if(string2time(&cdr_value_array[i], &timeval_val) < 0) {
					LM_ERR("failed to convert string to timeval.\n");
					goto error;
				}
				VAL_TIME(db_cdr_vals+i) = timeval_val.tv_sec;
				break;
			case TYPE_DOUBLE:
				VAL_TYPE(db_cdr_vals+i)=DB1_DOUBLE;
				VAL_NULL(db_cdr_vals+i)=0;
				double_val = strtod(cdr_value_array[i].s, &end);
				if(errno && (errno != EAGAIN)) {
					LM_ERR("failed to convert string to double - %d.\n", errno);
					goto error;
				}
				VAL_DOUBLE(db_cdr_vals+i) = double_val;
				break;
		}
	}

    /* get extra values */
    if (message)
    {
		n += extra2strar( cdr_extra,
							message,
							cdr_value_array + m,
							cdr_int_array + m,
							cdr_type_array + m);
		m += n;
    } else if (cdr_expired_dlg_enable){
        LM_WARN( "fallback to dlg_only search because of message doesn't exist.\n");
        m += extra2strar_dlg_only( cdr_extra,
                dialog,
                cdr_value_array + m,
                cdr_int_array + m,
                cdr_type_array +m,
                &dlgb);
    }

	for( ; i<m; i++) {
		db_cdr_keys[i] = &cdr_attrs[i];
		VAL_TYPE(db_cdr_vals+i)=DB1_STR;
		VAL_NULL(db_cdr_vals+i)=0;
		VAL_STR(db_cdr_vals+i) = cdr_value_array[i];
	}

	if (df->use_table(dh, &acc_cdrs_table /*table*/) < 0) {
		LM_ERR("error in use_table\n");
		goto error;
	}

	if(acc_db_insert_mode==1 && df->insert_delayed!=NULL) {
		if (df->insert_delayed(dh, db_cdr_keys, db_cdr_vals, m) < 0) {
			LM_ERR("failed to insert delayed into database\n");
			goto error;
		}
	} else if(acc_db_insert_mode==2 && df->insert_async!=NULL) {
		if (df->insert_async(dh, db_cdr_keys, db_cdr_vals, m) < 0) {
			LM_ERR("failed to insert async into database\n");
			goto error;
		}
	} else {
		if (df->insert(dh, db_cdr_keys, db_cdr_vals, m) < 0) {
			LM_ERR("failed to insert into database\n");
			goto error;
		}
	}

	/* Free memory allocated by acc_extra.c/extra2strar */
	free_strar_mem( &(cdr_type_array[m-n]), &(cdr_value_array[m-n]), n, m);
	return 0;

error:
    /* Free memory allocated by acc_extra.c/extra2strar */
	free_strar_mem( &(cdr_type_array[m-n]), &(cdr_value_array[m-n]), n, m);
    return -1;
}
#endif

/* collect all crd data and write it to a syslog */
static int log_write_cdr( struct dlg_cell* dialog,
                      struct sip_msg* message)
{
    static char cdr_message[ MAX_SYSLOG_SIZE];
    static char* const cdr_message_end = cdr_message +
                                         MAX_SYSLOG_SIZE -
                                         2;// -2 because of the string ending '\n\0'
    char* message_position = NULL;
    int message_index = 0;
	int extra_index = 0;
    int counter = 0;

	if(cdr_log_enable==0)
		return 0;

    /* get default values */
    message_index = cdr_core2strar( dialog,
                                    cdr_value_array,
                                    cdr_int_array,
                                    cdr_type_array);

    /* get extra values */
    if (message)
    {
        extra_index += extra2strar( cdr_extra,
                                      message,
                                      cdr_value_array + message_index,
                                      cdr_int_array + message_index,
                                      cdr_type_array + message_index);
    } else if (cdr_expired_dlg_enable){
        LM_DBG("fallback to dlg_only search because of message does not exist.\n");
        message_index += extra2strar_dlg_only( cdr_extra,
                                               dialog,
                                               cdr_value_array + message_index,
                                               cdr_int_array + message_index,
                                               cdr_type_array + message_index,
                                               &dlgb);
    }
	message_index += extra_index;

    for( counter = 0, message_position = cdr_message;
         counter < message_index ;
         counter++ )
    {
        const char* const next_message_end = message_position +
                                             2 + // ', ' -> two letters
                                             cdr_attrs[ counter].len +
                                             1 + // '=' -> one letter
                                             cdr_value_array[ counter].len;

        if( next_message_end >= cdr_message_end ||
            next_message_end < message_position)
        {
            LM_WARN("cdr message too long, truncating..\n");
            message_position = cdr_message_end;
            break;
        }

        if( counter > 0)
        {
            *(message_position++) = A_SEPARATOR_CHR;
            *(message_position++) = A_SEPARATOR_CHR_2;
        }

        memcpy( message_position,
                cdr_attrs[ counter].s,
                cdr_attrs[ counter].len);

        message_position += cdr_attrs[ counter].len;

        *( message_position++) = A_EQ_CHR;

        memcpy( message_position,
                cdr_value_array[ counter].s,
                cdr_value_array[ counter].len);

        message_position += cdr_value_array[ counter].len;
    }

    /* terminating line */
    *(message_position++) = '\n';
    *(message_position++) = '\0';

    LM_GEN2( cdr_facility, log_level, "%s", cdr_message);

	/* free memory allocated by extra2strar, nothing is done in case no extra strings were found by extra2strar */
    free_strar_mem( &(cdr_type_array[message_index-extra_index]), &(cdr_value_array[message_index-extra_index]),
				   extra_index, message_index);
    return 0;
}

/* collect all crd data and write it to a syslog */
static int write_cdr( struct dlg_cell* dialog,
                      struct sip_msg* message)
{
	int ret = 0;

	if( !dialog)
	{
		LM_ERR( "dialog is empty!");
		return -1;
	}
	/* message can be null when logging expired dialogs  */
	if ( !cdr_expired_dlg_enable && !message ){
		LM_ERR( "message is empty!");
		return -1;
	}

	ret = log_write_cdr(dialog, message);
#ifdef SQL_ACC
	ret |= db_write_cdr(dialog, message);
#endif
	return ret;
}

/* convert a string into a timeval struct */
static int string2time( str* time_str, struct timeval* time_value)
{    
    char* dot_address = NULL;
    int dot_position = -1;
    char zero_terminated_value[TIME_STR_BUFFER_SIZE];

    if( !time_str)
    {
        LM_ERR( "time_str is empty!");
        return -1;
    }
    
    if( time_str->len >= TIME_STR_BUFFER_SIZE)
    {
        LM_ERR( "time_str is too long %d >= %d!",
		time_str->len,
		TIME_STR_BUFFER_SIZE);
        return -1;
    }
    
    memcpy( zero_terminated_value, time_str->s, time_str->len);
    zero_terminated_value[time_str->len] = '\0';
    
    dot_address = strchr( zero_terminated_value, time_separator);
    
    if( !dot_address)
    {
        LM_ERR( "failed to find separator('%c') in '%s'!\n",
                time_separator,
                zero_terminated_value);
        return -1;
    }
    
    dot_position = dot_address-zero_terminated_value + 1;
    
    if( dot_position >= strlen(zero_terminated_value) ||
        strchr(dot_address + 1, time_separator))
    {
        LM_ERR( "invalid time-string '%s'\n", zero_terminated_value);
        return -1;
    }
    
    time_value->tv_sec = strtol( zero_terminated_value, (char **)NULL, 10);
    time_value->tv_usec = strtol( dot_address + 1, (char **)NULL, 10) * 1000; // restore usec precision
    return 0;
}

/* convert a timeval struct into a string */
static int time2string( struct timeval* time_value, str* time_str)
{
    int buffer_length;

    if( !time_value)
    {
        LM_ERR( "time_value or any of its fields is empty!\n");
        return -1;
    }

    buffer_length = snprintf( time_buffer,
                              TIME_BUFFER_LENGTH,
                              "%ld%c%03d",
                              (long int)time_value->tv_sec,
                              time_separator,
                              (int)(time_value->tv_usec/1000));

    if( buffer_length < 0)
    {
        LM_ERR( "failed to write to buffer.\n");
        return -1;
    }

    time_str->s = time_buffer;
    time_str->len = buffer_length;
    return 0;
}

/* set the duration in the dialog struct */
static int set_duration( struct dlg_cell* dialog)
{
    struct timeval start_time;
    struct timeval end_time;
    struct timeval duration_time;
    str duration_str;

    if( !dialog)
    {
        LM_ERR("dialog is empty!\n");
        return -1;
    }

    if ( string2time( dlgb.get_dlg_var( dialog, (str*)&cdr_start_str), &start_time) < 0) {
        LM_ERR( "failed to extract start time\n");
        return -1;
    }
    if ( string2time( dlgb.get_dlg_var( dialog, (str*)&cdr_end_str), &end_time) < 0) {
        LM_ERR( "failed to extract end time\n");
        return -1;
    }

    timersub(&end_time, &start_time, &duration_time);

    if( time2string(&duration_time, &duration_str) < 0) {
        LM_ERR( "failed to convert current time to string\n");
        return -1;
    }

    if( dlgb.set_dlg_var( dialog,
                          (str*)&cdr_duration_str,
                          (str*)&duration_str) != 0)
    {
        LM_ERR( "failed to set duration time");
        return -1;
    }

    return 0;
}

/* set the current time as start-time in the dialog struct */
static int set_start_time( struct dlg_cell* dialog)
{
    struct timeval current_time;
    str start_time;

    if( !dialog)
    {
        LM_ERR("dialog is empty!\n");
        return -1;
    }

    if( gettimeofday( &current_time, NULL) < 0)
    {
        LM_ERR( "failed to get current time!\n");
        return -1;
    }

    if( time2string(&current_time, &start_time) < 0) {
        LM_ERR( "failed to convert current time to string\n");
        return -1;
    }

    if( dlgb.set_dlg_var( dialog,
                          (str*)&cdr_start_str,
                          (str*)&start_time) != 0)
    {
        LM_ERR( "failed to set start time\n");
        return -1;
    }

    if( dlgb.set_dlg_var( dialog,
                          (str*)&cdr_end_str,
                          (str*)&start_time) != 0)
    {
        LM_ERR( "failed to set initiation end time\n");
        return -1;
    }

    if( dlgb.set_dlg_var( dialog,
                          (str*)&cdr_duration_str,
                          (str*)&zero_duration) != 0)
    {
        LM_ERR( "failed to set initiation duration time\n");
        return -1;
    }

    return 0;
}

/* set the current time as end-time in the dialog struct */
static int set_end_time( struct dlg_cell* dialog)
{
    struct timeval current_time;
    str end_time;

    if( !dialog)
    {
        LM_ERR("dialog is empty!\n");
        return -1;
    }

    if( gettimeofday( &current_time, NULL) < 0)
    {
        LM_ERR( "failed to set time!\n");
        return -1;
    }

    if( time2string(&current_time, &end_time) < 0) {
        LM_ERR( "failed to convert current time to string\n");
        return -1;
    }

    if( dlgb.set_dlg_var( dialog,
                          (str*)&cdr_end_str,
                          (str*)&end_time) != 0)
    {
        LM_ERR( "failed to set start time");
        return -1;
    }

    return 0;
}

/* callback for a confirmed (INVITE) dialog. */
static void cdr_on_start( struct dlg_cell* dialog,
                          int type,
                          struct dlg_cb_params* params)
{
    if( !dialog || !params)
    {
        LM_ERR("invalid values\n!");
        return;
    }

    if( cdr_start_on_confirmed == 0)
    {
        return;
    }

    if( set_start_time( dialog) != 0)
    {
        LM_ERR( "failed to set start time!\n");
        return;
    }
}

/* callback for a failure during a dialog. */
static void cdr_on_failed( struct dlg_cell* dialog,
                           int type,
                           struct dlg_cb_params* params)
{
    struct sip_msg* msg = 0;

    if( !dialog || !params)
    {
        LM_ERR("invalid values\n!");
        return;
    }

    if( params->rpl && params->rpl != FAKED_REPLY)
    {
        msg = params->rpl;
    }
    else if( params->req)
    {
        msg = params->req;
    }
    else
    {
        LM_ERR( "request and response are invalid!");
        return;
    }

    if( write_cdr( dialog, msg) != 0)
    {
        LM_ERR( "failed to write cdr!\n");
        return;
    }
}

/* callback for the finish of a dialog (reply to BYE). */
void cdr_on_end_confirmed( struct dlg_cell* dialog,
                        int type,
                        struct dlg_cb_params* params)
{
    if( !dialog || !params )
    {
        LM_ERR("invalid values\n!");
        return;
    }

    if( write_cdr( dialog, params->req) != 0)
    {
        LM_ERR( "failed to write cdr!\n");
        return;
    }
}

/* callback for the end of a dialog (BYE). */
static void cdr_on_end( struct dlg_cell* dialog,
                        int type,
                        struct dlg_cb_params* params)
{
    if( !dialog || !params)
    {
        LM_ERR("invalid values\n!");
        return;
    }

    if( set_end_time( dialog) != 0)
    {
        LM_ERR( "failed to set end time!\n");
        return;
    }

    if( set_duration( dialog) != 0)
    {
        LM_ERR( "failed to set duration!\n");
        return;
    }
}

/* callback for an expired dialog. */
static void cdr_on_expired( struct dlg_cell* dialog,
                            int type,
                            struct dlg_cb_params* params)
{
    if( !dialog || !params)
    {
        LM_ERR("invalid values\n!");
        return;
    }

    LM_DBG("dialog '%p' expired!\n", dialog);
    /* compute duration for timed out acknowledged dialog */
	if ( params && params->dlg_data ) {
		if ( (void*)CONFIRMED_DIALOG_STATE == params->dlg_data) {
			if( set_end_time( dialog) != 0)
			{
				LM_ERR( "failed to set end time!\n");
				return;
			}	
			
			if( set_duration( dialog) != 0)
			{
				LM_ERR( "failed to set duration!\n");
				return;
			}
			
		}
	}

    if( cdr_expired_dlg_enable  && (write_cdr( dialog, 0) != 0))
    {
        LM_ERR( "failed to write cdr!\n");
        return;
    }
}

/* callback for the cleanup of a dialog. */
static void cdr_on_destroy( struct dlg_cell* dialog,
                            int type,
                            struct dlg_cb_params* params)
{
    if( !dialog || !params)
    {
        LM_ERR("invalid values\n!");
        return;
    }

    LM_DBG("dialog '%p' destroyed!\n", dialog);
}

/* callback for the creation of a dialog. */
static void cdr_on_create( struct dlg_cell* dialog,
                           int type,
                           struct dlg_cb_params* params)
{
    if( !dialog || !params || !params->req)
    {
        LM_ERR( "invalid values\n!");
        return;
    }

    if( cdr_enable == 0)
    {
        return;
    }

    if( dlgb.register_dlgcb( dialog, DLGCB_CONFIRMED, cdr_on_start, 0, 0) != 0)
    {
        LM_ERR("can't register create dialog CONFIRM callback\n");
        return;
    }

	if(_acc_cdr_on_failed==1) {
		if( dlgb.register_dlgcb( dialog, DLGCB_FAILED, cdr_on_failed, 0, 0) != 0)
		{
			LM_ERR("can't register create dialog FAILED callback\n");
			return;
		}
	}

    if( dlgb.register_dlgcb( dialog, DLGCB_TERMINATED, cdr_on_end, 0, 0) != 0)
    {
        LM_ERR("can't register create dialog TERMINATED callback\n");
        return;
    }

    if( dlgb.register_dlgcb( dialog, DLGCB_TERMINATED_CONFIRMED, cdr_on_end_confirmed, 0, 0) != 0)
    {
        LM_ERR("can't register create dialog TERMINATED CONFIRMED callback\n");
        return;
    }

    if( dlgb.register_dlgcb( dialog, DLGCB_EXPIRED, cdr_on_expired, 0, 0) != 0)
    {
        LM_ERR("can't register create dialog EXPIRED callback\n");
        return;
    }

    if( dlgb.register_dlgcb( dialog, DLGCB_DESTROY, cdr_on_destroy, 0, 0) != 0)
    {
        LM_ERR("can't register create dialog DESTROY callback\n");
        return;
    }

    LM_DBG("dialog '%p' created!", dialog);

    if( set_start_time( dialog) != 0)
    {
        LM_ERR( "failed to set start time");
        return;
    }
}
/* convert the extra-data string into a list and store it */
int set_cdr_extra( char* cdr_extra_value)
{
    struct acc_extra* extra = 0;
    int counter = 0;

    if( cdr_extra_value && ( cdr_extra = parse_acc_extra( cdr_extra_value))==0)
    {
        LM_ERR("failed to parse crd_extra param\n");
        return -1;
    }

    /* fixed core attributes */
    cdr_attrs[ counter++] = cdr_start_str;
    cdr_attrs[ counter++] = cdr_end_str;
    cdr_attrs[ counter++] = cdr_duration_str;

    for(extra=cdr_extra; extra ; extra=extra->next)
    {
        cdr_attrs[ counter++] = extra->name;
    }

    return 0;
}

/* convert the facility-name string into a id and store it */
int set_cdr_facility( char* cdr_facility_str)
{
    int facility_id = -1;

    if( !cdr_facility_str)
    {
        LM_ERR( "facility is empty\n");
        return -1;
    }

    facility_id = str2facility( cdr_facility_str);

    if( facility_id == -1)
    {
        LM_ERR("invalid cdr facility configured\n");
        return -1;
    }

    cdr_facility = facility_id;

    return 0;
}

/* initialization of all necessary callbacks to track a dialog */
int init_cdr_generation( void)
{
    if( load_dlg_api( &dlgb) != 0)
    {
        LM_ERR("can't load dialog API\n");
        return -1;
    }

    if( dlgb.register_dlgcb( 0, DLGCB_CREATED, cdr_on_create, 0, 0) != 0)
    {
        LM_ERR("can't register create callback\n");
        return -1;
    }

    return 0;
}

/* convert the facility-name string into a id and store it */
void destroy_cdr_generation( void)
{
    if( !cdr_extra)
    {
        return;
    }

    destroy_extras( cdr_extra);
}
