/*
 * $Id$
 *
 * AVP_DB fifo functions
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


#include "avp_db.h"
#include "avp_list.h"
#include "fifo.h"
#include "../../fifo_server.h"
#include "../../dprint.h"


/*
 * Fifo function to reload avp list
 */
static int avp_list_reload ( FILE* pipe, char* response_file )
{
	if (reload_avp_list() == 1) {
		fifo_reply (response_file, "200 OK\n");
		return 1;
	} else {
		fifo_reply (response_file, "400 AVP list reload failed\n");
		return -1;
	}
}


/*
 * Register avp list fifo functions
 */
int init_avp_fifo( void ) 
{
	if (register_fifo_cmd(avp_list_reload, AVP_LIST_RELOAD, 0) < 0) {
		LOG(L_CRIT, "Cannot register avp_reload\n");
		return -1;
	}

	return 1;
}
