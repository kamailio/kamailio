/*
 * Accounting module
 *
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * ---------
 * 2003-04-04  grand acc cleanup (jiri)
 */


#ifndef _ACC_MOD_H
#define _ACC_MOD_H

#include "../../db/db.h"
#include "defs.h"

/* module parameter declaration */
extern int log_level;
extern int early_media;
extern int failed_transactions;

extern char *log_fmt;
extern int report_cancels;
extern int log_flag;
extern int log_missed_flag;


#ifdef RAD_ACC
extern int radius_flag;
extern int radius_missed_flag;
#endif

#ifdef SQL_ACC
extern int db_flag;
extern int db_missed_flag;

extern db_con_t* db_handle; /* Database connection handle */

extern char *db_url;
extern char *db_table_acc;
extern char *db_table_mc;

extern char* acc_sip_from_col;
extern char* acc_sip_to_col;
extern char* acc_sip_status_col;
extern char* acc_sip_method_col;
extern char* acc_i_uri_col;
extern char* acc_o_uri_col;
extern char* acc_sip_callid_col;
extern char* acc_user_col;
extern char* acc_time_col;
extern char* acc_from_uri;
extern char* acc_to_uri;
extern char* acc_totag_col;
extern char* acc_fromtag_col;


#endif /* SQL_ACC */

static inline int is_log_acc_on(struct sip_msg *rq)
{   
	return log_flag && isflagset(rq, log_flag)==1;
}   
#ifdef SQL_ACC
static inline int is_db_acc_on(struct sip_msg *rq)
{   
	return db_flag && isflagset(rq, db_flag)==1;
}   
#endif
#ifdef RAD_ACC
static inline int is_rad_acc_on(struct sip_msg *rq)
{   
	return radius_flag && isflagset(rq, radius_flag)==1;
}   
#endif
    
static inline int is_acc_on(struct sip_msg *rq)
{   
	if (is_log_acc_on(rq)) return 1;
#ifdef SQL_ACC
	if (is_db_acc_on(rq)) return 1;
#endif
#ifdef RAD_ACC
	if (is_rad_acc_on(rq)) return 1;
#endif
	return 0;
}

static inline int is_log_mc_on(struct sip_msg *rq)
{
	return log_missed_flag && isflagset(rq, log_missed_flag)==1;
}

#ifdef SQL_ACC
static inline int is_db_mc_on(struct sip_msg *rq)
{
	return db_missed_flag && isflagset(rq, db_missed_flag)==1;
}
#endif
#ifdef RAD_ACC
static inline int is_rad_mc_on(struct sip_msg *rq)
{
	return radius_missed_flag && isflagset(rq, radius_missed_flag)==1;
}
#endif


static inline int is_mc_on(struct sip_msg *rq)
{
	if (is_log_mc_on(rq)) return 1;
#ifdef SQL_ACC
	if (is_db_mc_on(rq)) return 1;
#endif
#ifdef RAD_ACC
	if (is_rad_mc_on(rq)) return 1;
#endif
	return 0;
}


#endif
