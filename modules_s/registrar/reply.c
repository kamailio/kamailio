/*
 * $Id$
 *
 * Send a reply
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
 * --------
 * 2003-01-18: buffer overflow patch committed (Jan on behalf of Maxim)
 * 2003-01-21: Errors reported via Error-Info header field - janakj
 */

#include <stdio.h>
#include "../../parser/msg_parser.h"
#include "../../data_lump_rpl.h"
#include "rerrno.h"
#include "reg_mod.h"
#include "regtime.h"
#include "reply.h"

#define MAX_CONTACT_BUFFER 1024

static char b[MAX_CONTACT_BUFFER];
static int l;


/*
 * Build Contact HF for reply
 */
void build_contact(ucontact_t* _c)
{
	char *lastgoodend;
	int nummissed;
	
	l = 0;
	lastgoodend = b;
	while(_c) {
		if (_c->expires > act_time) {
			if (l + 10 >= MAX_CONTACT_BUFFER)
				break;
			memcpy(b + l, "Contact: <", 10);
			l += 10;
			
			if (l + _c->c.len >= MAX_CONTACT_BUFFER)
				break;
			memcpy(b + l, _c->c.s, _c->c.len);
			l += _c->c.len;
			
			if (l + 4 >= MAX_CONTACT_BUFFER)
				break;
			memcpy(b + l, ">;q=", 4);
			l += 4;
			
			l += snprintf(b + l, MAX_CONTACT_BUFFER - l, "%-3.2f", _c->q);
			if (l >= MAX_CONTACT_BUFFER)
				break;
			
			if (l + 9 >= MAX_CONTACT_BUFFER)
				break;
			memcpy(b + l, ";expires=", 9);
			l += 9;
			
			l += snprintf(b + l, MAX_CONTACT_BUFFER - l, "%d", (int)(_c->expires - act_time));
			if (l >= MAX_CONTACT_BUFFER)
				break;

			if (l + 2 >= MAX_CONTACT_BUFFER)
				break;
			*(b + l++) = '\r';
			*(b + l++) = '\n';
			lastgoodend = b + l;
		}

		_c = _c->next;
	}
	if (lastgoodend - b != l) {
		l = lastgoodend - b;
		for(nummissed = 0; _c; _c = _c->next)
			nummissed++;
		LOG(L_ERR, "build_contact(): Contact list buffer exhaused, %d contact(s) ignored\n", nummissed);
	}

	DBG("build_contact(): Created Contact HF: %.*s\n", l, b);
}



#define MSG_200 "OK"
#define MSG_400 "Bad Request"
#define MSG_500 "Internal Server Error"

#define EI_R_FINE       "No problem"                                /* R_FINE */
#define EI_R_UL_DEL_R   "usrloc_record_delete failed"               /* R_UL_DEL_R */
#define	EI_R_UL_GET_R   "usrloc_record_get failed"                  /* R_UL_GET */
#define	EI_R_UL_NEW_R   "usrloc_record_new failed"                  /* R_UL_NEW_R */
#define	EI_R_INV_CSEQ   "Invalid CSeq number"                       /* R_INV_CSEQ */
#define	EI_R_UL_INS_C   "usrloc_contact_insert failed"              /* R_UL_INS_C */
#define	EI_R_UL_INS_R   "usrloc_record_insert failed"               /* R_UL_INS_R */
#define	EI_R_UL_DEL_C   "usrloc_contact_delete failed"              /* R_UL_DEL_C */
#define	EI_R_UL_UPD_C   "usrloc_contact_update failed"              /* R_UL_UPD_C */
#define	EI_R_TO_USER    "No username in To URI"                     /* R_TO_USER */
#define	EI_R_AOR_LEN    "Address Of Record too long"                /* R_AOR_LEN */
#define	EI_R_AOR_PARSE  "Error while parsing AOR"                   /* R_AOR_PARSE */
#define	EI_R_INV_EXP    "Invalid expires param in contact"          /* R_INV_EXP */
#define	EI_R_INV_Q      "Invalid q param in contact"                /* R_INV_Q */
#define	EI_R_PARSE      "Message parse error"                       /* R_PARSE */
#define	EI_R_TO_MISS    "To header not found"                       /* R_TO_MISS */
#define	EI_R_CID_MISS   "Call-ID header not found"                  /* R_CID_MISS */
#define	EI_R_CS_MISS    "CSeq header not found"                     /* R_CS_MISS */ 
#define	EI_R_PARSE_EXP	"Expires parse error"                       /* R_PARSE_EXP */
#define	EI_R_PARSE_CONT	"Contact parse error"                       /* R_PARSE_CONT */
#define	EI_R_STAR_EXP	"* used in contact and expires != 0"        /* R_STAR__EXP */
#define	EI_R_STAR_CONT	"* used in contact and no. of contacts > 1" /* R_STAR_CONT */
#define	EI_R_OOO	"Out of order request"                      /* R_OOO */
#define	EI_R_RETRANS	"Retransmission"                            /* R_RETRANS */
#define EI_R_UNESCAPE   "Error while unescaping username"           /* R_UNESCAPE */


str error_info[] = {
	{EI_R_FINE,       sizeof(EI_R_FINE) - 1},
	{EI_R_UL_DEL_R,   sizeof(EI_R_UL_DEL_R) - 1},
	{EI_R_UL_GET_R,   sizeof(EI_R_UL_GET_R) - 1},
	{EI_R_UL_NEW_R,   sizeof(EI_R_UL_NEW_R) - 1},
	{EI_R_INV_CSEQ,   sizeof(EI_R_INV_CSEQ) - 1},
	{EI_R_UL_INS_C,   sizeof(EI_R_UL_INS_C) - 1},
	{EI_R_UL_INS_R,   sizeof(EI_R_UL_INS_R) - 1},
	{EI_R_UL_DEL_C,   sizeof(EI_R_UL_DEL_C) - 1},
	{EI_R_UL_UPD_C,   sizeof(EI_R_UL_UPD_C) - 1},
	{EI_R_TO_USER,    sizeof(EI_R_TO_USER) - 1},
	{EI_R_AOR_LEN,    sizeof(EI_R_AOR_LEN) - 1},
	{EI_R_AOR_PARSE,  sizeof(EI_R_AOR_PARSE) - 1},
	{EI_R_INV_EXP,    sizeof(EI_R_INV_EXP) - 1},
	{EI_R_INV_Q,      sizeof(EI_R_INV_Q) - 1},
	{EI_R_PARSE,      sizeof(EI_R_PARSE) - 1},
	{EI_R_TO_MISS,    sizeof(EI_R_TO_MISS) - 1},
	{EI_R_CID_MISS,   sizeof(EI_R_CID_MISS) - 1},
	{EI_R_CS_MISS,    sizeof(EI_R_CS_MISS) - 1},
	{EI_R_PARSE_EXP,  sizeof(EI_R_PARSE_EXP) - 1},
	{EI_R_PARSE_CONT, sizeof(EI_R_PARSE_CONT) - 1},
	{EI_R_STAR_EXP,   sizeof(EI_R_STAR_EXP) - 1},
	{EI_R_STAR_CONT,  sizeof(EI_R_STAR_CONT) - 1},
	{EI_R_OOO,        sizeof(EI_R_OOO) - 1},
	{EI_R_RETRANS,    sizeof(EI_R_RETRANS) - 1},
	{EI_R_UNESCAPE,   sizeof(EI_R_UNESCAPE) - 1}
};

int codes[] = {
	200, /* R_FINE */
     	500, /* R_UL_DEL_R */
	500, /* R_UL_GET */
	500, /* R_UL_NEW_R */
	400, /* R_INV_CSEQ */
	500, /* R_UL_INS_C */
	500, /* R_UL_INS_R */
	500, /* R_UL_DEL_C */
	500, /* R_UL_UPD_C */
	400, /* R_TO_USER */
	500, /* R_AOR_LEN */
	400, /* R_AOR_PARSE */
	400, /* R_INV_EXP */
	400, /* R_INV_Q */
	400, /* R_PARSE */
	400, /* R_TO_MISS */
	400, /* R_CID_MISS */
	400, /* R_CS_MISS */
	400, /* R_PARSE_EXP */
	400, /* R_PARSE_CONT */
	400, /* R_STAR_EXP */
	400, /* R_STAR_CONT */
	200, /* R_OOO */
	200, /* R_RETRANS */
	400  /* R_UNESCAPE */
};



/*
 * Send a reply
 */
int send_reply(struct sip_msg* _m)
{
	int code;
	char* msg = MSG_200; /* makes gcc shut up */

	struct lump_rpl* p, *ei;

	if (l > 0) {
		p = build_lump_rpl(b, l);
		add_lump_rpl(_m, p);
		l = 0;
	}

	code = codes[rerrno];
	switch(code) {
	case 200: msg = MSG_200; break;
	case 400: msg = MSG_400; break;
	case 500: msg = MSG_500; break;
	}
	
	if (code != 200) {
		ei = build_lump_rpl(error_info[rerrno].s, error_info[rerrno].len);
		add_lump_rpl(_m, ei);
	}

	if (sl_reply(_m, (char*)code, msg) == -1) {
		LOG(L_ERR, "send_reply(): Error while sending %d %s\n", code, msg);
		return -1;
	} else return 0;	
}
