/*
 * Send a reply
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * \brief SIP registrar module - Send a reply
 * \ingroup registrar   
 */  

#include <stdio.h>
#include "../../ut.h"
#include "../../xavp.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_require.h"
#include "../../parser/parse_supported.h"
#include "../../data_lump_rpl.h"
#include "../usrloc/usrloc.h"
#include "rerrno.h"
#include "reg_mod.h"
#include "regtime.h"
#include "reply.h"
#include "config.h"

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

#define GR_PARAM ";gr="
#define GR_PARAM_LEN (sizeof(GR_PARAM) - 1)

#define SIP_INSTANCE_PARAM	";+sip.instance="
#define SIP_INSTANCE_PARAM_LEN	(sizeof(SIP_INSTANCE_PARAM) - 1)

#define PUB_GRUU_PARAM ";pub-gruu="
#define PUB_GRUU_PARAM_LEN (sizeof(PUB_GRUU_PARAM) - 1)

#define TMP_GRUU_PARAM ";temp-gruu="
#define TMP_GRUU_PARAM_LEN (sizeof(TMP_GRUU_PARAM) - 1)

#define REG_ID_PARAM ";reg-id="
#define REG_ID_PARAM_LEN (sizeof(REG_ID_PARAM) - 1)

extern int reg_gruu_enabled;

/*! \brief
 * Buffer for Contact header field
 */
static struct {
	char* buf;
	int buf_len;
	int data_len;
} contact = {0, 0, 0};


/*! \brief
 * Calculate the length of buffer needed to
 * print contacts
 * - mode specifies if GRUU header params are added
 */
static inline unsigned int calc_buf_len(ucontact_t* c, str *host, int mode)
{
	unsigned int len;
	int qlen;

	len = 0;
	while(c) {
		if (VALID_CONTACT(c, act_time)) {
			if (len) len += CONTACT_SEP_LEN;
			len += 2 /* < > */ + c->c.len;
			qlen = len_q(c->q);
			if (qlen) len += Q_PARAM_LEN + qlen;
			len += EXPIRES_PARAM_LEN + INT2STR_MAX_LEN;
			if (rcv_param.len>0 && c->received.s) {
				len += 1 /* ; */ 
					+ rcv_param.len 
					+ 1 /* = */ 
					+ 1 /* dquote */ 
					+ c->received.len
					+ 1 /* dquote */
					;
			}
			if (reg_gruu_enabled==1 && c->instance.len>0 && mode==1) {
				/* pub-gruu */
				len += PUB_GRUU_PARAM_LEN
					+ 1 /* " */
					+ 4 /* sip: */
					+ c->aor->len
					+ 1 /* @ */
					+ host->len
					+ GR_PARAM_LEN
					+ c->instance.len
					+ 1 /* " */
					;
				/* temp-gruu */
				len += TMP_GRUU_PARAM_LEN
					+ 1 /* " */
					+ 4 /* sip: */
					+ c->ruid.len
					+ 1 /* 'sep' */
					+ 8 /* max hex int */
					+ 1 /* @ */
					+ host->len
					+ GR_PARAM_LEN
					- 1 /* = */
					+ 1 /* " */
					;
			}
			if (c->instance.len>0) {
				/* +sip-instance */
				len += SIP_INSTANCE_PARAM_LEN
					+ 1 /* " */
					+ c->instance.len
					+ 1 /* " */
					;
			}
			if (c->reg_id>0) {
				/* reg-id */
				len += REG_ID_PARAM_LEN + INT2STR_MAX_LEN;
			}
		}
		c = c->next;
	}

	if (len) len += CONTACT_BEGIN_LEN + CRLF_LEN;
	return len;
}


/*! \brief
 * Allocate a memory buffer and print Contact
 * header fields into it
 */
int build_contact(sip_msg_t *msg, ucontact_t* c, str *host)
{
	char *p, *cp;
	char *a;
	int fl, len;
	str user;
	str inst;
	unsigned int ahash;
	unsigned short digit;
	int mode;
	sr_xavp_t *xavp=NULL;
	sr_xavp_t *list=NULL;
	str xname = {"ruid", 4};
	sr_xval_t xval;



	if(msg!=NULL && parse_supported(msg)==0
			&& (get_supported(msg) & F_OPTION_TAG_GRUU))
		mode = 1;
	else
		mode = 0;

	contact.data_len = calc_buf_len(c, host, mode);

	if (!contact.data_len) return 0;

	if (!contact.buf || (contact.buf_len < contact.data_len)) {
		if (contact.buf) pkg_free(contact.buf);
		contact.buf = (char*)pkg_malloc(contact.data_len);
		if (!contact.buf) {
			contact.data_len = 0;
			contact.buf_len = 0;
			LM_ERR("no pkg memory left\n");
			return -1;
		} else {
			contact.buf_len = contact.data_len;
		}
	}

	p = contact.buf;
	
	memcpy(p, CONTACT_BEGIN, CONTACT_BEGIN_LEN);
	p += CONTACT_BEGIN_LEN;

	/* add xavp with details of the record (ruid, ...) */
	if(reg_xavp_rcd.s!=NULL)
	{
		list = xavp_get(&reg_xavp_rcd, NULL);
		xavp = list;
	}

	fl = 0;
	while(c) {
		if (VALID_CONTACT(c, act_time)) {
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

			if (rcv_param.len>0 && c->received.s) {
				*p++ = ';';
				memcpy(p, rcv_param.s, rcv_param.len);
				p += rcv_param.len;
				*p++ = '=';
				*p++ = '\"';
				memcpy(p, c->received.s, c->received.len);
				p += c->received.len;
				*p++ = '\"';
			}
			if (reg_gruu_enabled==1 && c->instance.len>0 && mode==1) {
				user.s = c->aor->s;
				a = memchr(c->aor->s, '@', c->aor->len);
				if(a!=NULL) {
					user.len = a - user.s;
				} else {
					user.len = c->aor->len;
				}
				/* pub-gruu */
				memcpy(p, PUB_GRUU_PARAM, PUB_GRUU_PARAM_LEN);
				p += PUB_GRUU_PARAM_LEN;
				*p++ = '\"';
				memcpy(p, "sip:", 4);
				p += 4;
				if(a!=NULL) {
					memcpy(p, c->aor->s, c->aor->len);
					p += c->aor->len;
				} else {
					memcpy(p, user.s, user.len);
					p += user.len;
					*p++ = '@';
					memcpy(p, host->s, host->len);
					p += host->len;
				}
				memcpy(p, GR_PARAM, GR_PARAM_LEN);
				p += GR_PARAM_LEN;
				inst = c->instance;
				if(inst.s[0]=='<' && inst.s[inst.len-1]=='>') {
					inst.s++;
					inst.len -= 2;
				}
				memcpy(p, inst.s, inst.len);
				p += inst.len;
				*p++ = '\"';
				/* temp-gruu */
				memcpy(p, TMP_GRUU_PARAM, TMP_GRUU_PARAM_LEN);
				p += TMP_GRUU_PARAM_LEN;
				*p++ = '\"';
				memcpy(p, "sip:", 4);
				p += 4;
				memcpy(p, c->ruid.s, c->ruid.len);
				p += c->ruid.len;
				*p++ = '-';
				ahash = ul.get_aorhash(c->aor);
				while(ahash!=0)
				{
					digit =  ahash & 0x0f;
					*p++ = (digit >= 10) ? digit + 'a' - 10 : digit + '0';
					ahash >>= 4;
				}
				*p++ = '@';
				memcpy(p, host->s, host->len);
				p += host->len;
				memcpy(p, GR_PARAM, GR_PARAM_LEN);
				p += GR_PARAM_LEN - 1;
				*p++ = '\"';
			}

			if (c->instance.len>0) {
				/* +sip-instance */
				memcpy(p, SIP_INSTANCE_PARAM, SIP_INSTANCE_PARAM_LEN);
				p += SIP_INSTANCE_PARAM_LEN;
				*p++ = '\"';
				memcpy(p, c->instance.s, c->instance.len);
				p += c->instance.len;
				*p++ = '\"';
			}
			if (c->reg_id>0) {
				/* reg-id */
				memcpy(p, REG_ID_PARAM, REG_ID_PARAM_LEN);
				p += REG_ID_PARAM_LEN;
				cp = int2str(c->reg_id, &len);
				memcpy(p, cp, len);
				p += len;
			}
			if(reg_xavp_rcd.s!=NULL)
			{
				memset(&xval, 0, sizeof(sr_xval_t));
				xval.type = SR_XTYPE_STR;
				xval.v.s = c->ruid;
				if(xavp_add_value(&xname, &xval, &xavp)==NULL) {
					LM_ERR("cannot add ruid value to xavp\n");
				}
			}
		}

		c = c->next;
	}

	/* add xavp with details of the record (ruid, ...) */
	if(reg_xavp_rcd.s!=NULL)
	{
		if(list==NULL && xavp!=NULL)
		{
			/* no reg_xavp_rcd xavp in root list - add it */
			xval.type = SR_XTYPE_XAVP;
			xval.v.xavp = xavp;
			if(xavp_add_value(&reg_xavp_rcd, &xval, NULL)==NULL) {
				LM_ERR("cannot add ruid xavp to root list\n");
				xavp_destroy_list(&xavp);
			}
		}
	}

	memcpy(p, CRLF, CRLF_LEN);
	p += CRLF_LEN;

	contact.data_len = p - contact.buf;

	LM_DBG("created Contact HF: %.*s\n", contact.data_len, contact.buf);
	return 0;
}


#define MSG_200 "OK"
#define MSG_400 "Bad Request"
#define MSG_420 "Bad Extension"
#define MSG_421 "Extension Required"
#define MSG_439 "First Hop Lacks Outbound Support"
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
#define EI_R_CONTACT_LEN  "Contact/received too long"               /* R_CONTACT_LEN */
#define EI_R_CALLID_LEN  "Callid too long"                          /* R_CALLID_LEN */
#define EI_R_PARSE_PATH  "Path parse error"                         /* R_PARSE_PATH */
#define EI_R_PATH_UNSUP  "No support for found Path indicated"      /* R_PATH_UNSUP */
#define EI_R_OB_UNSUP    "No support for Outbound indicated"        /* R_OB_UNSUP */
#define EI_R_OB_REQD     "No support for Outbound on server"        /* R_OB_REQD */
#define EI_R_OB_UNSUP_EDGE "No support for Outbound on edge proxy"  /* R_OB_UNSUP_EDGE */


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
	{EI_R_TOO_MANY,   sizeof(EI_R_TOO_MANY) - 1},
	{EI_R_CONTACT_LEN,sizeof(EI_R_CONTACT_LEN) - 1},
	{EI_R_CALLID_LEN, sizeof(EI_R_CALLID_LEN) - 1},
	{EI_R_PARSE_PATH, sizeof(EI_R_PARSE_PATH) - 1},
	{EI_R_PATH_UNSUP, sizeof(EI_R_PATH_UNSUP) - 1},
	{EI_R_OB_UNSUP,   sizeof(EI_R_OB_UNSUP) - 1},
	{EI_R_OB_REQD,    sizeof(EI_R_OB_REQD) - 1},
	{EI_R_OB_UNSUP_EDGE, sizeof(EI_R_OB_UNSUP_EDGE) - 1},
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
	503, /* R_TOO_MANY */
	400, /* R_CONTACT_LEN */
	400, /* R_CALLID_LEN */
	400, /* R_PARSE_PATH */
	420, /* R_PATH_UNSUP */
	421, /* R_OB_UNSUP */
	420, /* R_OB_REQD */
	439, /* R_OB_UNSUP_EDGE */
};


#define RETRY_AFTER "Retry-After: "
#define RETRY_AFTER_LEN (sizeof(RETRY_AFTER) - 1)

static int add_retry_after(struct sip_msg* _m)
{
	char* buf, *ra_s;
 	int ra_len;
 	
 	ra_s = int2str(cfg_get(registrar, registrar_cfg, retry_after), &ra_len);
 	buf = (char*)pkg_malloc(RETRY_AFTER_LEN + ra_len + CRLF_LEN);
 	if (!buf) {
 		LM_ERR("no pkg memory left\n");
 		return -1;
 	}
 	memcpy(buf, RETRY_AFTER, RETRY_AFTER_LEN);
 	memcpy(buf + RETRY_AFTER_LEN, ra_s, ra_len);
 	memcpy(buf + RETRY_AFTER_LEN + ra_len, CRLF, CRLF_LEN);
 	add_lump_rpl(_m, buf, RETRY_AFTER_LEN + ra_len + CRLF_LEN,
 		     LUMP_RPL_HDR | LUMP_RPL_NODUP);
 	return 0;
}

#define PATH "Path: "
#define PATH_LEN (sizeof(PATH) - 1)

static int add_path(struct sip_msg* _m, str* _p)
{
	char* buf;

 	buf = (char*)pkg_malloc(PATH_LEN + _p->len + CRLF_LEN);
 	if (!buf) {
 		LM_ERR("no pkg memory left\n");
 		return -1;
 	}
 	memcpy(buf, PATH, PATH_LEN);
 	memcpy(buf + PATH_LEN, _p->s, _p->len);
 	memcpy(buf + PATH_LEN + _p->len, CRLF, CRLF_LEN);
 	add_lump_rpl(_m, buf, PATH_LEN + _p->len + CRLF_LEN,
 		     LUMP_RPL_HDR | LUMP_RPL_NODUP);
 	return 0;
}

#define UNSUPPORTED "Unsupported: "
#define UNSUPPORTED_LEN (sizeof(UNSUPPORTED) - 1)

static int add_unsupported(struct sip_msg* _m, str* _p)
{
	char* buf;

 	buf = (char*)pkg_malloc(UNSUPPORTED_LEN + _p->len + CRLF_LEN);
 	if (!buf) {
 		LM_ERR("no pkg memory left\n");
 		return -1;
 	}
 	memcpy(buf, UNSUPPORTED, UNSUPPORTED_LEN);
 	memcpy(buf + UNSUPPORTED_LEN, _p->s, _p->len);
 	memcpy(buf + UNSUPPORTED_LEN + _p->len, CRLF, CRLF_LEN);
 	add_lump_rpl(_m, buf, UNSUPPORTED_LEN + _p->len + CRLF_LEN,
 		     LUMP_RPL_HDR | LUMP_RPL_NODUP);
 	return 0;
}

#define REQUIRE "Require: "
#define REQUIRE_LEN (sizeof(REQUIRE) - 1)

static int add_require(struct sip_msg* _m, str* _p)
{
	char* buf;

 	buf = (char*)pkg_malloc(REQUIRE_LEN + _p->len + CRLF_LEN);
 	if (!buf) {
 		LM_ERR("no pkg memory left\n");
 		return -1;
 	}
 	memcpy(buf, REQUIRE, REQUIRE_LEN);
 	memcpy(buf + REQUIRE_LEN, _p->s, _p->len);
 	memcpy(buf + REQUIRE_LEN + _p->len, CRLF, CRLF_LEN);
 	add_lump_rpl(_m, buf, REQUIRE_LEN + _p->len + CRLF_LEN,
 		     LUMP_RPL_HDR | LUMP_RPL_NODUP);
 	return 0;
}

#define SUPPORTED "Supported: "
#define SUPPORTED_LEN (sizeof(SUPPORTED) - 1)

static int add_supported(struct sip_msg* _m, str* _p)
{
	char* buf;

 	buf = (char*)pkg_malloc(SUPPORTED_LEN + _p->len + CRLF_LEN);
 	if (!buf) {
 		LM_ERR("no pkg memory left\n");
 		return -1;
 	}
 	memcpy(buf, SUPPORTED, SUPPORTED_LEN);
 	memcpy(buf + SUPPORTED_LEN, _p->s, _p->len);
 	memcpy(buf + SUPPORTED_LEN + _p->len, CRLF, CRLF_LEN);
 	add_lump_rpl(_m, buf, SUPPORTED_LEN + _p->len + CRLF_LEN,
 		     LUMP_RPL_HDR | LUMP_RPL_NODUP);
 	return 0;
}

#define FLOW_TIMER "Flow-Timer: "
#define FLOW_TIMER_LEN (sizeof(FLOW_TIMER) - 1)

static int add_flow_timer(struct sip_msg* _m)
{
	char* buf;
	int lump_len;

	/* Add three as REG_FLOW_TIMER_MAX is 999 - three digits */
 	buf = (char*)pkg_malloc(FLOW_TIMER_LEN + 3 + CRLF_LEN);
 	if (!buf) {
 		LM_ERR("no pkg memory left\n");
 		return -1;
 	}
	lump_len = snprintf(buf, FLOW_TIMER_LEN + 3 + CRLF_LEN,
				"%.*s%d%.*s",
				(int)FLOW_TIMER_LEN, FLOW_TIMER,
				reg_flow_timer,
				(int)CRLF_LEN, CRLF);
 	add_lump_rpl(_m, buf, lump_len, LUMP_RPL_HDR | LUMP_RPL_NODUP);
 	return 0;
}
 
/*! \brief
 * Send a reply
 */
int reg_send_reply(struct sip_msg* _m)
{
	str unsup = str_init(OPTION_TAG_PATH_STR);
	str outbound_str = str_init(OPTION_TAG_OUTBOUND_STR);
	long code;
	str msg = str_init(MSG_200); /* makes gcc shut up */
	char* buf;

	if (contact.data_len > 0) {
		add_lump_rpl( _m, contact.buf, contact.data_len, LUMP_RPL_HDR|LUMP_RPL_NODUP|LUMP_RPL_NOFREE);
		contact.data_len = 0;
	}

	switch (rerrno) {
	case R_FINE:
		if (path_enabled && _m->path_vec.s) {
			if (path_mode != PATH_MODE_OFF) {
				if (parse_supported(_m)<0 && path_mode == PATH_MODE_STRICT) {
					rerrno = R_PATH_UNSUP;
					if (add_unsupported(_m, &unsup) < 0)
						return -1;
					if (add_path(_m, &_m->path_vec) < 0)
						return -1;
				}
				else if (get_supported(_m) & F_OPTION_TAG_PATH) {
					if (add_path(_m, &_m->path_vec) < 0)
						return -1;
				} else if (path_mode == PATH_MODE_STRICT) {
					rerrno = R_PATH_UNSUP;
					if (add_unsupported(_m, &unsup) < 0)
						return -1;
					if (add_path(_m, &_m->path_vec) < 0)
						return -1;
				}
			}
		}

		switch(reg_outbound_mode)
		{
		case REG_OUTBOUND_NONE:
		default:
			break;
		case REG_OUTBOUND_REQUIRE:
			if (add_require(_m, &outbound_str) < 0)
				return -1;

			if (add_supported(_m, &outbound_str) < 0)
				return -1;

			if (reg_flow_timer > 0) {
				if (add_flow_timer(_m) < 0)
					return -1;
			}
			break;
		case REG_OUTBOUND_SUPPORTED:
			if (add_supported(_m, &outbound_str) < 0)
				return -1;

			if ((get_require(_m) & F_OPTION_TAG_OUTBOUND)
			    || (get_supported(_m) & F_OPTION_TAG_OUTBOUND)) {
				if (add_require(_m, &outbound_str) < 0)
					return -1;

				if (reg_flow_timer > 0) {
					if (add_flow_timer(_m) < 0)
						return -1;
				}
			}
			break;
		}
		break;
	case R_OB_UNSUP:
		if (add_require(_m, &outbound_str) < 0)
			return -1;
		if (add_supported(_m, &outbound_str) < 0)
			return -1;
		break;
	case R_OB_REQD:
		if (add_unsupported(_m, &outbound_str) < 0)
			return -1;
		break;
	default:
		break;
	}

	code = codes[rerrno];
	switch(code) {
	case 200: msg.s = MSG_200; msg.len = sizeof(MSG_200)-1;break;
	case 400: msg.s = MSG_400; msg.len = sizeof(MSG_400)-1;break;
	case 420: msg.s = MSG_420; msg.len = sizeof(MSG_420)-1;break;
	case 421: msg.s = MSG_421; msg.len = sizeof(MSG_421)-1;break;
	case 439: msg.s = MSG_439; msg.len = sizeof(MSG_439)-1;break;
	case 500: msg.s = MSG_500; msg.len = sizeof(MSG_500)-1;break;
	case 503: msg.s = MSG_503; msg.len = sizeof(MSG_503)-1;break;
	}
	
	if (code != 200) {
		buf = (char*)pkg_malloc(E_INFO_LEN + error_info[rerrno].len + CRLF_LEN + 1);
		if (!buf) {
			LM_ERR("no pkg memory left\n");
			return -1;
		}
		memcpy(buf, E_INFO, E_INFO_LEN);
		memcpy(buf + E_INFO_LEN, error_info[rerrno].s, error_info[rerrno].len);
		memcpy(buf + E_INFO_LEN + error_info[rerrno].len, CRLF, CRLF_LEN);
		add_lump_rpl( _m, buf, E_INFO_LEN + error_info[rerrno].len + CRLF_LEN,
			LUMP_RPL_HDR|LUMP_RPL_NODUP);

		if (code >= 500 && code < 600 && cfg_get(registrar, registrar_cfg, retry_after)) {
			if (add_retry_after(_m) < 0) {
				return -1;
			}
		} 
	}
	
	if (slb.freply(_m, code, &msg) < 0) {
		LM_ERR("failed to send %ld %.*s\n", code, msg.len,msg.s);
		return -1;
	} else return 0;
}


/*! \brief
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
