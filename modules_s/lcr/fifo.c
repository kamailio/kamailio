/*
 * lcr fifo functions
 *
 * Copyright (C) 2002-2003 Juha Heinanen
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
/*
 * History:
 * --------
 *  2005-02-07 Introduced fifo.c
 */


#include "lcr_mod.h"
#include "fifo.h"
#include "../../fifo_server.h"
#include "../../dprint.h"
#include "../../db/db.h"


/*
 * Fifo function to reload lcr table(s)
 */
static int lcr_reload ( FILE* pipe, char* response_file )
{
	if (reload_gws () == 1) {
		fifo_reply (response_file, "200 OK\n");
		return 1;
	} else {
		fifo_reply (response_file, "400 Reload of gateways failed\n");
		return -1;
	}
}


/*
 * Fifo function to print gws from current gw table
 */
static int lcr_dump ( FILE* pipe, char* response_file )
{
	FILE *reply_file;
	
	reply_file=open_reply_pipe(response_file);
	if (reply_file==0) {
		LOG(L_ERR, "lcr_dump(): Opening of response file failed\n");
		return -1;
	}
	fputs( "200 OK\n", reply_file );
	print_gws(reply_file );
	fclose(reply_file);
	return 1;
}


/*
 * Register domain fifo functions
 */
int init_lcr_fifo( void ) 
{
	if (register_fifo_cmd(lcr_reload, LCR_RELOAD, 0) < 0) {
		LOG(L_CRIT, "Cannot register lcr_reload\n");
		return -1;
	}

	if (register_fifo_cmd(lcr_dump, LCR_DUMP, 0) < 0) {
		LOG(L_CRIT, "Cannot register lcr_dump\n");
		return -1;
	}

	return 1;
}
