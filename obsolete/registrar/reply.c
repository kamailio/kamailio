/*
 * $Id$
 *
 * Send a reply
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
 *
 * History:
 * --------
 * 2003-01-18: buffer overflow patch committed (Jan on behalf of Maxim)
 * 2003-01-21: Errors reported via Error-Info header field - janakj
 * 2003-09-11: updated to new build_lump_rpl() interface (bogdan)
 * 2003-11-11: build_lump_rpl() removed, add_lump_rpl() has flags (bogdan)
 */

#include <stdio.h>
#include "../../ut.h"
#include "../../parser/msg_parser.h"
#include "../../data_lump_rpl.h"
#include "../usrloc/usrloc.h"
#include "rerrno.h"
#include "reg_mod.h"
#include "regtime.h"
#include "common.h"
#include "reply.h"


#define MAX_CONTACT_BUFFER 1024

#define E_INFO "P-Registrar-Error: "
#define E_INFO_LEN (sizeof(E_INFO) - 1)

#define CONTACT_BEGIN "Contact: "
#define CONTACT_BEGIN_LEN (sizeof(CONTACT_BEGIN) - 1)

#define Q_PARAM ";q="
#define Q_PARAM_LEN (sizeof(Q_PARAM) - 1)

#define EXPIRES_PARAM ";expires="
#define EXPIRES_PARAM_LEN (sizeof(EXPIRES_PARAM) - 1)

#define CONTACT_SEP ", "
#define CONTACT_SEP_LEN (sizeof(CONTACT_SEP) - 1)


/*
 * Buffer for Contact header field
 */
static struct {
	char* buf;
	int buf_len;
	int data_len;
} contact = {0, 0, 0};


/*
 * Calculate the length of buffer needed to
 * print contacts
 */
static inline unsigned int calc_buf_len(ucontact_t* c, str* aor_filter)
{
	unsigned int len;
	int qlen;

	len = 0;
	while(c) {
		if (VALID_CONTACT(c, act_time) && (!aor_filter->s || VALID_AOR(c, (*aor_filter)))) {
			if (len) len += CONTACT_SEP_LEN;
			len += 2 /* < > */ + c->c.len;
			qlen = len_q(c->q);
			if (qlen) len += Q_PARAM_LEN + qlen;
			len += EXPIRES_PARAM_LEN + INT2STR_MAX_LEN;
			if (c->received.s) {
				len += 1 /* ; */ 
					+ rcv_param.len 
					+ 1 /* = */ 
					+ 1 /* dquote */ 
					+ c->received.len
					+ 1 /* dquote */
					;
			}
		}
		c = c->next;
	}

	if (len) len += CONTACT_BEGIN_LEN + CRLF_LEN;
	return len;
}


/*
 * Allocate a memory buffer and print Contact
 * header fields into it
 */
int build_contact(ucontact_t* c, str* aor_filter)
{
	char *p, *cp;
	int fl, len;

	contact.data_len = calc_buf_len(c, aor_filter);
	if (!contact.data_len) return 0;

	if (!contact.buf || (contact.buf_len < contact.data_len)) {
		if (contact.buf) pkg_free(contact.buf);
		contact.buf = (char*)pkg_malloc(contact.data_len);
		if (!contact.buf) {
			contact.data_len = 0;
			contact.buf_len = 0;
			LOG(L_ERR, "build_contact(): No memory left\n");
			return -1;
		} else {
			contact.buf_len = contact.data_len;
		}
	}

	p = contact.buf;
	
	memcpy(p, CONTACT_BEGIN, CONTACT_BEGIN_LEN);
	p += CONTACT_BEGIN_LEN;

	fl = 0;
	while(c) {
		if (VALID_CONTACT(c, act_time) && (!aor_filter->s || VALID_AOR(c, (*aor_filter)))) {
			if (fl) {
				memcpy(p, CONTACT_SEP, CONTACT_SEP_LEN);
				p += CONTACT_SEP_LEN;
			} else {
				fl = 1;
			}

			*p++ = '<';
			memcpy(p, c->c.s, c->c.len);
			p += c->c.len;
			*p++ = '>';

			len = len_q(c->q);
			if (len) {
				memcpy(p, Q_PARAM, Q_PARAM_LEN);
				p += Q_PARAM_LEN;
				memcpy(p, q2str(c->q, 0), len);
				p += len;
			}

			memcpy(p, EXPIRES_PARAM, EXPIRES_PARAM_LEN);
			p += EXPIRES_PARAM_LEN;
			cp = int2str((int)(c->expires - act_time), &len);
			memcpy(p, cp, len);
			p += len;

			if (c->received.s) {
				*p++ = ';';
				memcpy(p, rcv_param.s, rcv_param.len);
				p += rcv_param.len;
				*p++ = '=';
				*p++ = '\"';
				memcpy(p, c->received.s, c->received.len);
				p += c->received.len;
				*p++ = '\"';
			}
		}

		c = c->next;
	}

	memcpy(p, CRLF, CRLF_LEN);
	p += CRLF_LEN;

	contact.data_len = p - contact.buf;

	DBG("build_contact(): Created Contact HF: %.*s\n", contact.data_len, contact.buf);
	return 0;
}


#define MSG_200 "OK"
#define MSG_400 "Bad Request"
#define MSG_500 "Server Internal Error"
#define MSG_503 "Service Unavailable"

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
#define	EI_R_STAR_EXP	"* used in contact and expires is not zero" /* R_STAR__EXP */
#define	EI_R_STAR_CONT	"* used in contact and more than 1 contact" /* R_STAR_CONT */
#define	EI_R_OOO	"Out of order request"                      /* R_OOO */
#define	EI_R_RETRANS	"Retransmission"                            /* R_RETRANS */
#define EI_R_UNESCAPE   "Error while unescaping username"           /* R_UNESCAPE */
#define EI_R_TOO_MANY   "Too many registered contacts"              /* R_TOO_MANY */

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
	{EI_R_UNESCAPE,   sizeof(EI_R_UNESCAPE) - 1},
	{EI_R_TOO_MANY,   sizeof(EI_R_TOO_MANY) - 1}
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
	400, /* R_UNESCAPE */
	503  /* R_TOO_MANY */
};


/*
 * Send a reply
 */
int send_reply(struct sip_msg* _m)
{
	long code;
	char* msg = MSG_200; /* makes gcc shut up */
	char* buf;

	if (contact.data_len > 0) {
		add_lump_rpl( _m, contact.buf, contact.data_len,
				LUMP_RPL_HDR|LUMP_RPL_NODUP|LUMP_RPL_NOFREE);
		contact.data_len = 0;
	}

	code = codes[rerrno];
	switch(code) {
	case 200: msg = MSG_200; break;
	case 400: msg = MSG_400; break;
	case 500: msg = MSG_500; break;
	case 503: msg = MSG_503; break;
	}
	
	if (code != 200) {
		buf = (char*)pkg_malloc(E_INFO_LEN + error_info[rerrno].len
				+ CRLF_LEN + 1);
		if (!buf) {
			LOG(L_ERR, "send_reply(): No memory left\n");
			return -1;
		}
		memcpy(buf, E_INFO, E_INFO_LEN);
		memcpy(buf + E_INFO_LEN, error_info[rerrno].s, error_info[rerrno].len);
		memcpy(buf + E_INFO_LEN + error_info[rerrno].len, CRLF, CRLF_LEN);
		add_lump_rpl( _m, buf, E_INFO_LEN + error_info[rerrno].len + CRLF_LEN,
			LUMP_RPL_HDR|LUMP_RPL_NODUP);

	}

	if (slb.zreply(_m, code, msg) == -1) {
		ERR("Error while sending %ld %s\n", code, msg);
		return -1;
	} else return 0;	
}


/*
 * Release contact buffer if any
 */
void free_contact_buf(void)
{
	if (contact.buf) {
		pkg_free(contact.buf);
		contact.buf = 0;
		contact.buf_len = 0;
		contact.data_len = 0;
	}
}


int setup_attrs(struct sip_msg* msg)
{
	int code;
	str reason, c;
	avp_value_t val;

	code = codes[rerrno];

	if (reply_code_attr.len) {
		val.n = code;
		if (add_avp(avpid_code.flags, avpid_code.name, val) < 0) {
			ERR("Error while creating reply code attribute\n");
			return -1;
		}
	}

	if (reply_reason_attr.len) {
		reason.s = NULL;
		reason.len = 0;
		switch(code) {
		case 200: reason.s = MSG_200; reason.len = sizeof(MSG_200) - 1; break;
		case 400: reason.s = MSG_400; reason.len = sizeof(MSG_400) - 1; break;
		case 500: reason.s = MSG_500; reason.len = sizeof(MSG_500) - 1; break;
		case 503: reason.s = MSG_503; reason.len = sizeof(MSG_503) - 1; break;
		}
		val.s = reason;
		if (add_avp(avpid_reason.flags | AVP_VAL_STR, avpid_reason.name, val)
				< 0) {
			ERR("Error while creating reply reason attribute\n");
			return -1;
		}
	}

	if (contact_attr.len && contact.data_len > 0) {
		c.s = contact.buf;
		c.len = contact.data_len;
		val.s = c;

		if (add_avp(avpid_contact.flags | AVP_VAL_STR, avpid_contact.name, val)
				< 0) {
			ERR("Error while creating contact attribute\n");
			return -1;
		}

		contact.data_len = 0;
	}

	return 0;
}
