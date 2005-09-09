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

static int extract_contact_rpl(struct sip_msg *m, str *dst)
{
	char *tmp = "";
	if (!dst) return -1;
	dst->s = NULL;
	dst->len = 0;
	
	switch(m->rcv.bind_address->proto){ 
		case PROTO_NONE: break;
		case PROTO_UDP: break;
		case PROTO_TCP: tmp = ";transport=tcp";	break;
		case PROTO_TLS: tmp = ";transport=tls"; break;
		case PROTO_SCTP: tmp = ";transport=sctp"; break;
		default: LOG(L_CRIT, "BUG: extract_server_contact: unknown proto %d\n", m->rcv.bind_address->proto); 
	}

	dst->len = 18 + m->rcv.bind_address->name.len + m->rcv.bind_address->port_no_str.len + strlen(tmp);
	dst->s = (char *)shm_malloc(dst->len + 1);
	if (!dst->s) {
		dst->len = 0;
		return -1;
	}
	snprintf(dst->s, dst->len + 1, "Contact: <sip:%.*s:%.*s%s>\r\n",
			m->rcv.bind_address->name.len, m->rcv.bind_address->name.s,
			m->rcv.bind_address->port_no_str.len, m->rcv.bind_address->port_no_str.s,
			tmp);

	return 0;
}

/*
 * Send a reply
 */
int send_reply(struct sip_msg* _m)
{
	int code;
	char* msg = MSG_200; /* makes gcc shut up */

	code = codes[paerrno];
	switch(code) {
	case 200: msg = MSG_200; break;
	case 400: msg = MSG_400; break;
	case 500: msg = MSG_500; break;
	}
	
	if (code != 200) {
		if (add_lump_rpl( _m, error_info[paerrno].s, error_info[paerrno].len,
		LUMP_RPL_HDR|LUMP_RPL_NODUP|LUMP_RPL_NOFREE)==0) {
			LOG(L_ERR, "ERROR:pa:send_reply: cannot add rpl_lump hdr\n");
			return -1;
		}
	}
	else {
		/* add Contact header field */
		str s;
		extract_contact_rpl(_m, &s);
		if (s.len > 0) {
			if (!add_lump_rpl(_m, s.s, s.len, LUMP_RPL_HDR)) {
				LOG(L_ERR, "pa:send_reply: Can't add Contact header to the response\n");
				return -1;
			}
			if (s.s) shm_free(s.s);
		}
	}

	if (tmb.t_reply(_m, code, msg) == -1) {
		LOG(L_ERR, "send_reply(): Error while sending %d %s\n", code, msg);
		return -1;
	} else return 0;	
}
