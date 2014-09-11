/*
 * Presence Agent, reply building
 *
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "../../dprint.h"
#include "../../data_lump_rpl.h"
#include "reply.h"
#include "paerrno.h"
#include "pa_mod.h"
#include <presence/utils.h>

#define MSG_200 "OK"
#define MSG_400 "Bad Request"
#define MSG_500 "Server Internal Error"
#define MSG_403 "Forbidden"

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
#define EI_PA_SUBSCRIPTION_REJECTED  "Subscription rejected"


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
	{EI_PA_INTERNAL_ERROR, sizeof(EI_PA_INTERNAL_ERROR) - 1},
	{EI_PA_SUBSCRIPTION_REJECTED, sizeof(EI_PA_SUBSCRIPTION_REJECTED) - 1}
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
	500,  /* EI_PA_INTERNAL_ERROR */
	403 /* EI_PA_SUBSCRIPTION_REJECTED */
};

/*
 * Send a reply
 */
int send_reply(struct sip_msg* _m)
{
	int code = 200;
	char* msg = MSG_200; /* makes gcc shut up */

	/* code = codes[paerrno]; */
	switch (paerrno) {
		case PA_OK: msg = MSG_200; code = 200; break;
		case PA_PARSE_ERR: msg = MSG_400; code = 400; break;
		case PA_FROM_MISS: msg = MSG_400; code = 400; break;
		case PA_EVENT_MISS: 
					msg = "Unsupported event package"; 
					code = 489;
					break;
		case PA_EVENT_PARSE: msg = MSG_400; code = 400; break;
		case PA_EXPIRES_PARSE: msg = MSG_400; code = 400; break;
		case PA_EVENT_UNSUPP: 
					msg = "Unsupported event package"; 
					code = 489;
					break;
		case PA_WRONG_ACCEPTS: 
					msg = "Unsupported document format for given package"; 
					code = 415; 
					break;
		case PA_NO_MEMORY: msg = MSG_500; code = 500; break;
		case PA_TIMER_ERROR: msg = MSG_500; code = 500; break;
		case PA_EXTRACT_USER: msg = MSG_400; code = 400; break;
		case PA_FROM_ERR: msg = MSG_400; code = 400; break;
		case PA_TO_ERR: msg = MSG_400; code = 400; break;
		case PA_SMALL_BUFFER: msg = MSG_500; code = 500; break;
		case PA_UNSUPP_DOC: 
					msg = "Unsupported document format"; 
					code = 415; 
					break;
		case PA_ACCEPT_PARSE: msg = MSG_400; code = 400; break;
		case PA_URI_PARSE: msg = MSG_400; code = 400; break;
		case PA_DIALOG_ERR: msg = MSG_500; code = 500; break;
		case PA_INTERNAL_ERROR: msg = MSG_500; code = 500; break;
		case PA_SUBSCRIPTION_REJECTED: msg = MSG_403; code = 403; break;
		case PA_NO_MATCHING_TUPLE: msg = "Conditional Request Failed"; 
								   code = 412; 
								   break;
		case PA_OK_WAITING_FOR_AUTH:
						msg = "Accepted"; 
						code = 202;
						break;
						/* OK but waiting for auth -> should return 202 */
		case PA_SUBSCRIPTION_NOT_EXISTS:
						msg = "Subscription does not exist"; 
						code = 481;
						break;
						/* OK but waiting for auth -> should return 202 */
	}	
	
	if ((code >= 200) && (code < 300)) {
		/* add Contact header field into response */
		str s;
		if (extract_server_contact(_m, &s, 0) == 0) {
			if (s.len > 0) {
				if (!add_lump_rpl(_m, s.s, s.len, LUMP_RPL_HDR)) {
					ERR("Can't add Contact header into the response\n");
					if (s.s) mem_free(s.s);
					return -1;
				}
			}
			if (s.s) mem_free(s.s);
		}
	}
	
	if (tmb.t_reply(_m, code, msg) < 0) {
		ERR("Error while sending %d %s\n", code, msg);
		return -1;
	} else return 0;
}
