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
 */


#include "reply.h"
#include <stdio.h>
#include "../../parser/msg_parser.h"
#include "../../data_lump_rpl.h"
#include "rerrno.h"
#include "reg_mod.h"
#include "regtime.h"


#define MAX_CONTACT_BUFFER 1024

static char b[MAX_CONTACT_BUFFER];
static int l;


#define MSG_200 "OK"
#define MSG_400 "Bad Request"
#define MSG_500 "Internal Server Error"


/*
 * Build Contact HF for reply
 */
void build_contact(ucontact_t* _c)
{
	l = 0;
	while(_c) {
		if (_c->expires > act_time) {
			memcpy(b + l, "Contact: <", 10);
			l += 10;
			
			memcpy(b + l, _c->c.s, _c->c.len);
			l += _c->c.len;
			
			memcpy(b + l, ">;q=", 4);
			l += 4;
			
			l += sprintf(b + l, "%-3.2f", _c->q);
			
			memcpy(b + l, ";expires=", 9);
			l += 9;
			
			l += sprintf(b + l, "%d", (int)(_c->expires - act_time));
			
			*(b + l++) = '\r';
			*(b + l++) = '\n';
		}

		_c = _c->next;
	}
	
	DBG("build_contact(): Created Contact HF: %.*s\n", l, b);
}


/*
 * Convert rerrno to code and message
 */
static inline void rerrno2msg(int* _c, char** _m)
{
	switch(rerrno) {
	case R_FINE:         *_c = 200; *_m = MSG_200;                                     break;
	case R_UL_DEL_R:   *_c = 500; *_m = MSG_500 " - Usrloc_record_delete failed";      break;
	case R_UL_GET_R:   *_c = 500; *_m = MSG_500 " - Usrloc_record_get failed";         break;
	case R_UL_NEW_R:   *_c = 500; *_m = MSG_500 " - Usrloc_record_new failed";         break;
	case R_INV_CSEQ:   *_c = 400; *_m = MSG_400 " - Invalid CSeq number";              break;
	case R_UL_INS_C:   *_c = 500; *_m = MSG_500 " - Usrloc_contact_insert failed";     break;
	case R_UL_INS_R:   *_c = 500; *_m = MSG_500 " - Usrloc_record_insert failed ";     break;
	case R_UL_DEL_C:   *_c = 500; *_m = MSG_500 " - Usrloc_contact_delete failed";     break;			
	case R_UL_UPD_C:   *_c = 500; *_m = MSG_500 " - Usrloc_contact_update failed";     break;
	case R_TO_USER:    *_c = 400; *_m = MSG_400 " - No username in To URI";            break;
	case R_AOR_LEN:    *_c = 500; *_m = MSG_500 " - Address Of Record too long";       break;
	case R_AOR_PARSE:  *_c = 400; *_m = MSG_400 " - Error while parsing AOR";          break;
	case R_INV_EXP:    *_c = 400; *_m = MSG_400 " - Invalid expires param in contact"; break;
	case R_INV_Q:      *_c = 400; *_m = MSG_400 " - Invalid q param in contact";       break;
	case R_PARSE:      *_c = 400; *_m = MSG_400 " - Message parse error";              break;
	case R_TO_MISS:    *_c = 400; *_m = MSG_400 " - To header not found";              break;
	case R_CID_MISS:   *_c = 400; *_m = MSG_400 " - Call-ID header not found";         break;
	case R_CS_MISS:    *_c = 400; *_m = MSG_400 " - CSeq header not found";            break;
	case R_PARSE_EXP:  *_c = 400; *_m = MSG_400 " - Expires parse error";              break;
	case R_PARSE_CONT: *_c = 400; *_m = MSG_400 " - Contact parse error";              break;
	case R_STAR_EXP:   *_c = 400; *_m = MSG_400 " - star and expires not zero";        break;
	case R_STAR_CONT:  *_c = 400; *_m = MSG_400 " - star and more contacts";           break;
	case R_OOO:        *_c = 200; *_m = MSG_200 " - Out Of Order";                     break;
	case R_RETRANS:    *_c = 200; *_m = MSG_200 " - Retransmission";                   break;
	}
}


/*
 * Send a reply
 */
int send_reply(struct sip_msg* _m)
{
	int code;
	char* msg;

	struct lump_rpl* p;

	if (l > 0) {
		p = build_lump_rpl(b, l);
		add_lump_rpl(_m, p);
		l = 0;
	}

	rerrno2msg(&code, &msg);
	
	if (sl_reply(_m, (char*)code, msg) == -1) {
		LOG(L_ERR, "send_reply(): Error while sending %d %s\n", code, msg);
		return -1;
	} else return 0;	
}
