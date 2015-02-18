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
 *  \brief P_USRLOC - P-Usrloc module interface
 *  \ingroup usrloc
 */

#ifndef UL_MOD_H
#define UL_MOD_H


#include "../../lib/srdb1/db.h"
#include "../../str.h"
#include "../../lib/kmi/mi.h"
#include "../usrloc/usrloc.h"

/*
 * Module parameters
 */

#define UL_TABLE_VERSION 1004

/*
 * Matching algorithms
 */
#define CONTACT_ONLY            (0)
#define CONTACT_CALLID          (1)
#define CONTACT_PATH		(2)

#define REG_TABLE   "locdb"
#define URL_COL        "url"
#define ID_COL         "id"
#define NUM_COL        "no"
#define STATUS_COL      "status"
#define FAILOVER_T_COL "failover"
#define SPARE_COL      "spare"
#define ERROR_COL      "errors"
#define RISK_GROUP_COL "rg"
#define DEFAULT_EXPIRE 3600
#define DEFAULT_ERR_THRESHOLD 50
#define DB_RETRY 10
#define DB_DEFAULT_POLICY 0
#define DEFAULT_FAILOVER_LEVEL 1
#define DB_DEFAULT_TRANSACTION_LEVEL "READ UNCOMMITED"
#define DB_DEFAULT_CONNECTION_EXPIRES 300
#define DEFAULT_DB_TYPE "single"
#define DEFAULT_DOMAIN_DB "location=cluster,cfa=single"

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

extern int db_mode;
extern int use_domain;
extern int desc_time_order;
extern int cseq_delay;
extern int ul_fetch_rows;
extern int ul_hash_size;



extern str default_db_url;
extern str default_db_type;
extern int default_dbt;
extern str domain_db;
extern int expire;

extern int matching_mode;

struct mi_root* mi_ul_db_refresh(struct mi_root* cmd, void* param);
struct mi_root* mi_loc_nr_refresh(struct mi_root* cmd, void* param);

extern str write_db_url;
extern str read_db_url;
extern str reg_table;
extern str id_col;
extern str url_col;
extern str num_col;
extern str status_col;
extern str failover_time_col;
extern str spare_col;
extern str error_col;
extern str risk_group_col;
extern int expire_time;
extern int db_error_threshold;
extern int failover_level;
extern int retry_interval;
extern int policy;
extern int db_write;
extern int db_master_write;
extern int db_use_transactions;
extern str db_transaction_level;
extern char * isolation_level;
extern int connection_expires;
extern int alg_location;

extern int  max_loc_nr;

#endif /* UL_MOD_H */
