/* 
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2006-2007 iptelorg GmbH
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
 */

#ifndef _MY_CMD_H
#define _MY_CMD_H  1

#include "../../lib/srdb2/db_drv.h"
#include "../../lib/srdb2/db_cmd.h"
#include <mysql/mysql.h>
#include <stdarg.h>

typedef enum my_flags {
	/** Fetch all data from the server to the client at once */
	MY_FETCH_ALL = (1 << 0),
} my_flags_t;

struct my_cmd {
	db_drv_t gen;

	str sql_cmd; /**< Database command represented in SQL language */
	int next_flag;
	MYSQL_STMT* st; /**< MySQL pre-compiled statement handle */

	/** This is the sequential number of the last
	 * connection reset last time the command was
	 * uploaded to the server. If the reset number
	 * in the corresponding my_con structure is higher
	 * than the number in this variable then we need
	 * to upload the command again, because the
	 * the connection was reconnected meanwhile.
	 */
	unsigned int last_reset;
	unsigned int flags; /**< Various flags, mainly used by setopt and getopt */
};

int my_cmd(db_cmd_t* cmd);

int my_cmd_exec(db_res_t* res, db_cmd_t* cmd);

int my_cmd_first(db_res_t* res);

int my_cmd_next(db_res_t* res);

int my_getopt(db_cmd_t* cmd, char* optname, va_list ap);

int my_setopt(db_cmd_t* cmd, char* optname, va_list ap);

#endif /* _MY_CMD_H */
