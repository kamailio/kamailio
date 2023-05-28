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

#ifndef _USRLOC_MOD_H_
#define _USRLOC_MOD_H_


#include "../../lib/srdb1/db.h"
#include "../../core/str.h"


/*
 * Module parameters
 */


#define UL_TABLE_VERSION 9

extern str ul_ruid_col;
extern str ul_user_col;
extern str ul_domain_col;
extern str ul_contact_col;
extern str ul_expires_col;
extern str ul_q_col;
extern str ul_callid_col;
extern str ul_cseq_col;
extern str ul_flags_col;
extern str ul_cflags_col;
extern str ul_user_agent_col;
extern str ul_received_col;
extern str ul_path_col;
extern str ul_sock_col;
extern str ul_methods_col;
extern str ul_instance_col;
extern str ul_reg_id_col;
extern str ul_srv_id_col;
extern str ul_con_id_col;
extern str ul_keepalive_col;
extern str ul_partition_col;
extern str ul_last_mod_col;

extern str ulattrs_user_col;
extern str ulattrs_domain_col;
extern str ulattrs_ruid_col;
extern str ulattrs_aname_col;
extern str ulattrs_atype_col;
extern str ulattrs_avalue_col;
extern str ulattrs_last_mod_col;


extern str ul_db_url;
extern int ul_timer_interval;
extern int ul_db_mode;
extern int ul_db_insert_update;
extern int ul_use_domain;
extern int ul_desc_time_order;
extern int ul_cseq_delay;
extern int ul_fetch_rows;
extern int ul_hash_size;
extern int ul_db_update_as_insert;
extern int ul_db_check_update;
extern int ul_keepalive_timeout;
extern int ul_handle_lost_tcp;
extern int ul_close_expired_tcp;
extern int ul_skip_remote_socket;


/*! nat branch flag */
extern unsigned int ul_nat_bflag;
/*! flag to protect against wrong initialization */
extern unsigned int ul_init_flag;

extern str ul_xavp_contact_name;

extern db1_con_t *ul_dbh; /* Database connection handle */
extern db_func_t ul_dbf;

/* filter on load and during cleanup by server id */
extern unsigned int ul_db_srvid;

/*
 * Matching algorithms
 */
#define CONTACT_ONLY (0)
#define CONTACT_CALLID (1)
#define CONTACT_PATH (2)
#define CONTACT_CALLID_ONLY (3)

extern int ul_matching_mode;

extern int ul_db_ops_ruid;

extern int ul_expires_type;

#define UL_DB_EXPIRES_SET(r, v)               \
	do {                                      \
		if(ul_expires_type == 1) {            \
			(r)->type = DB1_BIGINT;           \
			(r)->val.ll_val = (long long)(v); \
		} else {                              \
			(r)->type = DB1_DATETIME;         \
			(r)->val.time_val = (time_t)(v);  \
		}                                     \
	} while(0)

#define UL_DB_EXPIRES_GET(r) \
	((ul_expires_type == 1) ? (time_t)VAL_BIGINT(r) : VAL_TIME(r))

#endif /* _USRLOC_MOD_H_ */
