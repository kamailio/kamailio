/*
 * Presence Agent, reply building
 *
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
 */

#include "reply.h"
#include "paerrno.h"
#include "../../dprint.h"
#include "pa_mod.h"


#define MSG_200 "OK"
#define MSG_400 "Bad Request"
#define MSG_500 "Internal Server Error"


/*
 * Convert paerrno to code and message
 */
static inline void paerrno2msg(int* _c, char** _m)
{
	switch(paerrno) {
	case PA_OK:        *_c = 200; *_m = MSG_200; break;
	case PA_PARSE_ERR: *_c = 400; *_m = MSG_400 " - Error while parsing headers"; break;
	case PA_CONTACT_MISS:   *_c = 400; *_m = MSG_400 " - Contact header field missing"; break;
	case PA_FROM_MISS: *_c = 400; *_m = MSG_400 " - From header field missing"; break;
	case PA_EVENT_MISS: *_c = 400; *_m = MSG_400 " - Event header field missing"; break;
	case PA_EVENT_PARSE: *_c = 400; *_m = MSG_400 " - Error while parsing Event header field"; break;
	case PA_EXPIRES_PARSE: *_c = 400; *_m = MSG_400 " - Error while parsing Expires header field"; break;
	case PA_EVENT_UNSUPP: *_c = 500; *_m = MSG_500 " - Unsupported event package"; break;
	case PA_NO_MEMORY: *_c = 500; *_m = MSG_500 " - No memory left on the server"; break;
	case PA_TIMER_ERROR: *_c = 500; *_m = MSG_500 " - Error while running timer"; break;
	case PA_EXTRACT_USER: *_c = 400; *_m = MSG_400 " - Cannot extract username from URI"; break;
	case PA_CONT_PARSE: *_c = 400; *_m = MSG_400 " - Error while parsing Contact"; break;
	case PA_CONT_STAR: *_c = 400; *_m = MSG_400 " - Star not allowed in Contact"; break;
	case PA_FROM_ERROR: *_c = 400; *_m = MSG_400 " - Error while parsing From"; break;
	case PA_SMALL_BUFFER: *_c = 500; *_m = MSG_500 " - Buffer too small on server"; break;
	case PA_UNSUPP_DOC: *_c = 500; *_m = MSG_500 " - Unsupported presence document format"; break;
	}
}


/*
 * Send a reply
 */
int send_reply(struct sip_msg* _m)
{
	int code;
	char* msg;

	paerrno2msg(&code, &msg);
	
	if (tmb.t_reply(_m, code, msg) == -1) {
		LOG(L_ERR, "send_reply(): Error while sending %d %s\n", code, msg);
		return -1;
	} else return 0;	
}
