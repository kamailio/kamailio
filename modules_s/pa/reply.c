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

#include "../../dprint.h"
#include "../../data_lump_rpl.h"
#include "reply.h"
#include "paerrno.h"
#include "pa_mod.h"


#define MSG_200 "OK"
#define MSG_400 "Bad Request"
#define MSG_500 "Server Internal Error"

#define	EI_PA_OK             "No problem"
#define	EI_PA_PARSE_ERR      "Error while parsing headers"
#define	EI_PA_CONTACT_MISS   "Contact header field missing"
#define	EI_PA_FROM_MISS      "From header field missing"
#define	EI_PA_EVENT_MISS     "Event header field missing"
#define	EI_PA_EVENT_PARSE    "Error while parsing Event header field"
#define	EI_PA_EXPIRES_PARSE  "Error while parsing Expires header field"
#define	EI_PA_EVENT_UNSUPP   "Unsupported event package"
#define	EI_PA_NO_MEMORY      "No memory left on the server"
#define	EI_PA_TIMER_ERROR    "Error while running timer"
#define	EI_PA_EXTRACT_USER   "Cannot extract username from URI"
#define	EI_PA_CONT_PARSE     "Error while parsing Contact"
#define	EI_PA_CONT_STAR      "Start not allowed in Contact"
#define	EI_PA_FROM_ERROR     "Error while parsing From"
#define	EI_PA_SMALL_BUFFER   "Buffer too small on the server"
#define	EI_PA_UNSUPP_DOC     "Unsupported document format"
#define EI_PA_INTERNAL_ERROR "Internal Server Error"


str error_info[] = {
	{EI_PA_OK,             sizeof(EI_PA_OK) - 1            },
	{EI_PA_PARSE_ERR,      sizeof(EI_PA_PARSE_ERR) - 1     },
	{EI_PA_CONTACT_MISS,   sizeof(EI_PA_CONTACT_MISS) - 1  },
	{EI_PA_FROM_MISS,      sizeof(EI_PA_FROM_MISS) - 1     },
	{EI_PA_EVENT_MISS,     sizeof(EI_PA_EVENT_MISS) - 1    },
	{EI_PA_EVENT_PARSE,    sizeof(EI_PA_EVENT_PARSE) - 1   },
	{EI_PA_EXPIRES_PARSE,  sizeof(EI_PA_EXPIRES_PARSE) - 1 },
	{EI_PA_EVENT_UNSUPP,   sizeof(EI_PA_EVENT_UNSUPP) - 1  },
	{EI_PA_NO_MEMORY,      sizeof(EI_PA_NO_MEMORY) - 1     },
	{EI_PA_TIMER_ERROR,    sizeof(EI_PA_TIMER_ERROR) - 1   },
	{EI_PA_EXTRACT_USER,   sizeof(EI_PA_EXTRACT_USER) - 1  },
	{EI_PA_CONT_PARSE,     sizeof(EI_PA_CONT_PARSE) - 1    },
	{EI_PA_CONT_STAR,      sizeof(EI_PA_CONT_STAR) - 1     },
	{EI_PA_FROM_ERROR,     sizeof(EI_PA_FROM_ERROR) - 1    },
	{EI_PA_SMALL_BUFFER,   sizeof(EI_PA_SMALL_BUFFER) - 1  },
	{EI_PA_UNSUPP_DOC,     sizeof(EI_PA_UNSUPP_DOC) - 1    },
	{EI_PA_INTERNAL_ERROR, sizeof(EI_PA_INTERNAL_ERROR) - 1}
};


int codes[] = {
	200, /* EI_PA_OK */
	400, /* EI_PA_PARSE_ERR */
	400, /* EI_PA_CONTACT_MISS */
	400, /* EI_PA_FROM_MISS */
	400, /* EI_PA_EVENT_MISS */
	400, /* EI_PA_EVENT_PARSE */
	400, /* EI_PA_EXPIRES_PARSE */
	500, /* EI_PA_EVENT_UNSUPP */
	500, /* EI_PA_NO_MEMORY */
	500, /* EI_PA_TIMER_ERROR */
	400, /* EI_PA_EXTRACT_USER */
	400, /* EI_PA_CONT_PARSE */
	400, /* EI_PA_CONT_STAR */
	400, /* EI_PA_FROM_ERROR */
	500, /* EI_PA_SMALL_BUFFER */
	500, /* EI_PA_UNSUPP_DOC */
	500  /* EI_PA_INTERNAL_ERROR */
};


/*
 * Send a reply
 */
int send_reply(struct sip_msg* _m)
{
	int code;
	char* msg = MSG_200; /* makes gcc shut up */

	struct lump_rpl  *ei;

	code = codes[paerrno];
	switch(code) {
	case 200: msg = MSG_200; break;
	case 400: msg = MSG_400; break;
	case 500: msg = MSG_500; break;
	}
	
	if (code != 200) {
		ei = build_lump_rpl(error_info[paerrno].s, error_info[paerrno].len);
		add_lump_rpl(_m, ei);
	}

	if (tmb.t_reply(_m, code, msg) == -1) {
		LOG(L_ERR, "send_reply(): Error while sending %d %s\n", code, msg);
		return -1;
	} else return 0;	
}
