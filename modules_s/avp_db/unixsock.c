/*
 * $Id$
 *
 * UNIX Socket Interface
 *
 * Copyright (C) 2002-2004 FhG FOKUS
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
#include "unixsock.h"
#include "../../unixsock_server.h"
#include "../../dprint.h"


/*
 * Unixsock function to reload avp list
 */
static int avp_list_reload(str* msg)
{
	if (reload_avp_list() == 1) {
		unixsock_reply_asciiz("200 OK\n");
		unixsock_reply_send();
		return 0;
	} else {
		unixsock_reply_asciiz("400 AVP list reload failed\n");
		unixsock_reply_send();
		return -1;
	}
}


/*
 * Register avp list unixsock functions
 */
int init_avp_unixsock(void) 
{
	if (unixsock_register_cmd(AVP_LIST_RELOAD, avp_list_reload) < 0) {
		LOG(L_ERR, "init_avp_unixsock: Cannot register avp_list_reload\n");
		return -1;
	}

	return 0;
}
