/*
 * $Id$
 *
 * Copyright (C)  2007-2008 Voice Sistem SRL
 *
 * This file is part of SIP-router, a free SIP server.
 *
 * SIP-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2007-08-01 initial version (ancuta onofrei)
 */

/*!
 * \file
 * \brief SIP-router dialplan :: Database interface
 * \ingroup dialplan
 * Module: \ref dialplan
 */

#ifndef _DP_DB_H_
#define _DP_DB_H

#include "../../str.h"
#include "../../lib/srdb1/db.h"

#define DP_TABLE_NAME			"dialplan"
#define DPID_COL				"dpid"
#define PR_COL					"pr"
#define MATCH_OP_COL			"match_op"
#define MATCH_EXP_COL			"match_exp"
#define MATCH_LEN_COL			"match_len"
#define SUBST_EXP_COL			"subst_exp"
#define REPL_EXP_COL			"repl_exp"
#define ATTRS_COL				"attrs"


#define DP_TABLE_VERSION		2
#define DP_TABLE_COL_NO 		8

extern str dp_db_url;
extern str dp_table_name;
extern str dpid_column; 
extern str pr_column; 
extern str match_op_column; 
extern str match_exp_column; 
extern str match_len_column; 
extern str subst_exp_column; 
extern str repl_exp_column; 
extern str attrs_column; 

int init_db_data();
int dp_connect_db();
void dp_disconnect_db();

#endif
