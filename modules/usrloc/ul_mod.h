/*
 * User location module interface
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 *  \brief USRLOC - Usrloc module interface
 *  \ingroup usrloc
 */

#ifndef UL_MOD_H
#define UL_MOD_H


#include "../../lib/srdb1/db.h"
#include "../../str.h"


/*
 * Module parameters
 */


#define UL_TABLE_VERSION 6

extern str ruid_col;
extern str user_col;
extern str domain_col;
extern str contact_col;
extern str expires_col;
extern str q_col;
extern str callid_col;
extern str cseq_col;
extern str flags_col;
extern str cflags_col;
extern str user_agent_col;
extern str received_col;
extern str path_col;
extern str sock_col;
extern str methods_col;
extern str instance_col;
extern str reg_id_col;
extern str last_mod_col;

extern str ulattrs_user_col;
extern str ulattrs_domain_col;
extern str ulattrs_ruid_col;
extern str ulattrs_aname_col;
extern str ulattrs_atype_col;
extern str ulattrs_avalue_col;
extern str ulattrs_last_mod_col;


extern str db_url;
extern int timer_interval;
extern int db_mode;
extern int use_domain;
extern int desc_time_order;
extern int cseq_delay;
extern int ul_fetch_rows;
extern int ul_hash_size;
extern int ul_db_update_as_insert;
extern int ul_db_check_update;
extern int ul_keepalive_timeout;
extern int handle_lost_tcp;
extern int close_expired_tcp;


/*! nat branch flag */
extern unsigned int nat_bflag;
/*! flag to protect against wrong initialization */
extern unsigned int init_flag;

extern str ul_xavp_contact_name;

extern db1_con_t* ul_dbh;   /* Database connection handle */
extern db_func_t ul_dbf;


/*
 * Matching algorithms
 */
#define CONTACT_ONLY        (0)
#define CONTACT_CALLID      (1)
#define CONTACT_PATH        (2)

extern int matching_mode;

extern int ul_db_ops_ruid;

extern int ul_expires_type;

#define UL_DB_EXPIRES_SET(r, v)   do { \
			if(ul_expires_type==1) { \
				(r)->type = DB1_BIGINT; \
				(r)->val.ll_val = (long long)(v); \
			} else { \
				(r)->type = DB1_DATETIME; \
				(r)->val.time_val = (time_t)(v); \
			} \
		} while(0)

#define UL_DB_EXPIRES_GET(r)  ((ul_expires_type==1)?(time_t)VAL_BIGINT(r):VAL_TIME(r))

#endif /* UL_MOD_H */
