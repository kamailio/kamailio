/*
 * $Id$
 *
 * Copyright (C) 2004 Voice Sistem SRL
 *
 * This file is part of SIP Express Router.
 *
 * SPEEDDIAL SER-module is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * SPEEDDIAL SER-module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * For any questions about this software and its license, please contact
 * Voice Sistem at following e-mail address:
 *         office@voice-sistem.ro
 *  
 *
 * History:
 * ---------
 * 
 */


#ifndef _SPEEDDIAL_H_
#define _SPEEDDIAL_H_

#include "../../lib/srdb2/db.h"
#include "../../modules/sl/sl.h"
#include "../../parser/msg_parser.h"


/* Module parameters variables */

extern char* uid_column; 
extern char* dial_username_column;
extern char* dial_did_column;
extern char* new_uri_column;

extern db_ctx_t* db;

struct db_table_name {
	char* table;
	db_cmd_t* lookup_num;
};

extern struct db_table_name* tables;
extern unsigned int tables_no;

extern sl_api_t slb;

#endif /* _SPEEDDIAL_H_ */
