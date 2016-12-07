/*
 * Copyright (C) 2013 Crocodile RCS Ltd
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
/*!
 * \file
 * \brief Kamailio core :: STUN support
 * STUN support callback, used by the STUN module
 * \ingroup core
 * Module: \ref core
 */

#include "events.h"
#include "ip_addr.h"
#include "stun.h"

int stun_process_msg(char* buf, unsigned int len, struct receive_info* ri)
{
        int ret;
	stun_event_info_t sev;

        ret = 0;
        LM_DBG("STUN Message: [[>>>\n%.*s<<<]]\n", len, buf);
        if(likely(sr_event_enabled(SREV_STUN_IN))) {
		memset(&sev, 0, sizeof(stun_event_info_t));
		sev.buf = buf;
		sev.len = len;
		sev.rcv = ri;
                ret = sr_event_exec(SREV_STUN_IN, (void *) &sev);
        } else {
                LM_DBG("no callback registering for handling STUN -"
			" dropping!\n");
        }
        return ret;
}
