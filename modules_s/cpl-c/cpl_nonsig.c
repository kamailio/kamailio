/*
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 *
 * History:
 * -------
 * 2003-06-27: file created (bogdan)
 */

#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "../../dprint.h"
#include "cpl_nonsig.h"


void cpl_aux_process( int cmd_out, char *log_dir)
{
	struct cpl_cmd cmd;
	int len;

	while(1) {
		/* let's read a command from pipe */
		len = read( cmd_out, &cmd, sizeof(struct cpl_cmd));
		if (len!=sizeof(struct cpl_cmd)) {
			if (len>=0) {
				LOG(L_ERR,"ERROR:cpl_aux_proceress: truncated message"
					" read from pipe! -> discarted\n");
			} else if (errno!=EAGAIN) {
				LOG(L_ERR,"ERROR:cpl_aux_proceress: pipe reading failed: "
					" : %s\n",strerror(errno));
			}
			sleep(1);
			continue;
		}

		/* process the command*/
		switch (cmd.code) {
			case CPL_LOG_CMD:
				break;
			case CPL_MAIL_CMD:
				break;
			default:
				LOG(L_ERR,"ERROR:cpl_aux_proceress: unknown command (%d) "
					"recived! -> ignoring\n",cmd.code);
		} /* ens switch*/

	}
}

