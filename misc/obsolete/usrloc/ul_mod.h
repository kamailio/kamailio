/*
 * $Id$
 *
 * User location module interface
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * ---------
 * 2003-03-12 added replication and state column (nils)
 */


#ifndef UL_MOD_H
#define UL_MOD_H


#include "../../lib/srdb2/db.h"
#include "../../str.h"


/*
 * Module parameters
 */

enum ul_db_type {
    NO_DB = 0,     /* No DB access */
    WRITE_THROUGH, /* Propagate changes to DB immediately */
    WRITE_BACK,    /* Propagate changes with delay */
    READONLY,      /* Perform initial raad and don't update */
    UL_DB_MAX 
};

#define NO_DB         0
#define WRITE_THROUGH 1
#define WRITE_BACK    2
#define READONLY      3

extern str uid_col;
extern str contact_col;
extern str expires_col;
extern str q_col;
extern str callid_col;
extern str cseq_col;
extern str method_col;
extern str flags_col;
extern str user_agent_col;
extern str received_col;
extern str instance_col;
extern str aor_col;
extern str server_id_col;
extern str db_url;
extern int timer_interval;
extern int db_mode;
extern int desc_time_order;
extern int db_skip_delete;

extern db_ctx_t* db;
extern db_cmd_t** del_rec;
extern db_cmd_t** del_contact;
extern db_cmd_t** ins_contact;
extern int cmd_n, cur_cmd;

#endif /* UL_MOD_H */
