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
 */


#ifndef _ACC_H
#define _ACC_H

#include "../../db/db.h"

/* module parameter declaration */
extern int use_db;
extern char *db_url;
extern char *uid_column;
extern char *db_table_acc;
extern char *db_table_mc;
extern int log_level;
extern int early_media;
extern int failed_transactions;
extern int flagged_only;
extern int usesyslog;

extern db_con_t* db_handle; /* Database connection handle */

extern char* acc_sip_from_col;
extern char* acc_sip_to_col;
extern char* acc_sip_status_col;
extern char* acc_sip_method_col;
extern char* acc_i_uri_col;
extern char* acc_o_uri_col;
extern char* acc_sip_callid_col;
extern char* acc_user_col;
extern char* acc_time_col;

extern char* mc_sip_from_col;
extern char* mc_sip_to_col;
extern char* mc_sip_status_col;
extern char* mc_sip_method_col;
extern char* mc_i_uri_col;
extern char* mc_o_uri_col;
extern char* mc_sip_callid_col;
extern char* mc_user_col;
extern char* mc_time_col;

#endif
