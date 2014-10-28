/*
 * $Id$
 *
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
 * History:
 * --------
 * 2003-01-18: buffer overflow patch committed (Jan on behalf of Maxim)
 * 2003-01-21: Errors reported via Error-Info header field - janakj
 * 2003-09-11: updated to new build_lump_rpl() interface (bogdan)
 * 2003-11-11: build_lump_rpl() removed, add_lump_rpl() has flags (bogdan)
 */

/*!
 * \file
 * \brief SIP registrar module - Send a reply
 * \ingroup registrar   
 */

#include "../../ut.h"
#include "../../parser/msg_parser.h"
#include "../../parser/contact/contact.h"
#include "../../parser/parse_supported.h"
#include "../../data_lump_rpl.h"
#include "../ims_usrloc_scscf/usrloc.h"
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

extern str scscf_serviceroute_uri_str;

extern struct tm_binds tmb;

static struct {
    char* buf;
    int buf_len;
    int data_len;
} p_associated_uri = {0, 0, 0};


/*! \brief
 * Buffer for Contact header field
 */
//static struct {
//	char* buf;
//	int buf_len;
//	int data_len;
//} contact = {0, 0, 0};

/*! \brief
 * Calculate the length of buffer needed to
 * print contacts
 */
static inline unsigned int calc_buf_len(impurecord_t* impurec) {
    unsigned int len;
    int qlen;
    int i=0;
    ucontact_t* c;

    len = 0;
    while (i<MAX_CONTACTS_PER_IMPU && (c=impurec->newcontacts[i])) {
        if (VALID_CONTACT(c, act_time)) {
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
	i++;
    }

    if (len) len += CONTACT_BEGIN_LEN + CRLF_LEN;
    return len;
}


#define MSG_200 "OK"
#define MSG_400 "Bad Request"
#define MSG_420 "Bad Extension"
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
#define EI_R_SAR_FAILED  "SAR failed"      							/* R_SAR_FAILED */

str error_info[] = {
    {EI_R_FINE, sizeof (EI_R_FINE) - 1},
    {EI_R_UL_DEL_R, sizeof (EI_R_UL_DEL_R) - 1},
    {EI_R_UL_GET_R, sizeof (EI_R_UL_GET_R) - 1},
    {EI_R_UL_NEW_R, sizeof (EI_R_UL_NEW_R) - 1},
    {EI_R_INV_CSEQ, sizeof (EI_R_INV_CSEQ) - 1},
    {EI_R_UL_INS_C, sizeof (EI_R_UL_INS_C) - 1},
    {EI_R_UL_INS_R, sizeof (EI_R_UL_INS_R) - 1},
    {EI_R_UL_DEL_C, sizeof (EI_R_UL_DEL_C) - 1},
    {EI_R_UL_UPD_C, sizeof (EI_R_UL_UPD_C) - 1},
    {EI_R_TO_USER, sizeof (EI_R_TO_USER) - 1},
    {EI_R_AOR_LEN, sizeof (EI_R_AOR_LEN) - 1},
    {EI_R_AOR_PARSE, sizeof (EI_R_AOR_PARSE) - 1},
    {EI_R_INV_EXP, sizeof (EI_R_INV_EXP) - 1},
    {EI_R_INV_Q, sizeof (EI_R_INV_Q) - 1},
    {EI_R_PARSE, sizeof (EI_R_PARSE) - 1},
    {EI_R_TO_MISS, sizeof (EI_R_TO_MISS) - 1},
    {EI_R_CID_MISS, sizeof (EI_R_CID_MISS) - 1},
    {EI_R_CS_MISS, sizeof (EI_R_CS_MISS) - 1},
    {EI_R_PARSE_EXP, sizeof (EI_R_PARSE_EXP) - 1},
    {EI_R_PARSE_CONT, sizeof (EI_R_PARSE_CONT) - 1},
    {EI_R_STAR_EXP, sizeof (EI_R_STAR_EXP) - 1},
    {EI_R_STAR_CONT, sizeof (EI_R_STAR_CONT) - 1},
    {EI_R_OOO, sizeof (EI_R_OOO) - 1},
    {EI_R_RETRANS, sizeof (EI_R_RETRANS) - 1},
    {EI_R_UNESCAPE, sizeof (EI_R_UNESCAPE) - 1},
    {EI_R_TOO_MANY, sizeof (EI_R_TOO_MANY) - 1},
    {EI_R_CONTACT_LEN, sizeof (EI_R_CONTACT_LEN) - 1},
    {EI_R_CALLID_LEN, sizeof (EI_R_CALLID_LEN) - 1},
    {EI_R_PARSE_PATH, sizeof (EI_R_PARSE_PATH) - 1},
    {EI_R_PATH_UNSUP, sizeof (EI_R_PATH_UNSUP) - 1},
    {EI_R_SAR_FAILED, sizeof (EI_R_SAR_FAILED) - 1}

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
    500 /* R_SAR_FAILED */
};


#define RETRY_AFTER "Retry-After: "
#define RETRY_AFTER_LEN (sizeof(RETRY_AFTER) - 1)

static int add_retry_after(struct sip_msg* _m) {
    char* buf, *ra_s;
    int ra_len;

    ra_s = int2str(cfg_get(registrar, registrar_cfg, retry_after), &ra_len);
    buf = (char*) pkg_malloc(RETRY_AFTER_LEN + ra_len + CRLF_LEN);
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

static int add_path(struct sip_msg* _m, str* _p) {
    char* buf;

    buf = (char*) pkg_malloc(PATH_LEN + _p->len + CRLF_LEN);
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


#define SERVICEROUTE_START "Service-Route: <"
#define SERVICEROUTE_START_LEN (sizeof(SERVICEROUTE_START) -1)

#define SERVICEROUTE_END ";lr>\r\n"
#define SERVICEROUTE_END_LEN (sizeof(SERVICEROUTE_END) -1)

static int add_service_route(struct sip_msg* _m, str* _uri) {
    char* buf;

    buf = (char*) pkg_malloc(SERVICEROUTE_START_LEN + _uri->len + SERVICEROUTE_END_LEN);
    if (!buf) {
        LM_ERR("no pkg memory left\n");
        return -1;
    }

    memcpy(buf, SERVICEROUTE_START, SERVICEROUTE_START_LEN);
    memcpy(buf + SERVICEROUTE_START_LEN, _uri->s, _uri->len);
    memcpy(buf + SERVICEROUTE_START_LEN + _uri->len, SERVICEROUTE_END, SERVICEROUTE_END_LEN);
    add_lump_rpl(_m, buf, SERVICEROUTE_START_LEN + _uri->len + SERVICEROUTE_END_LEN, LUMP_RPL_HDR | LUMP_RPL_NODUP);
    return 0;
}

#define UNSUPPORTED "Unsupported: "
#define UNSUPPORTED_LEN (sizeof(UNSUPPORTED) - 1)

static int add_unsupported(struct sip_msg* _m, str* _p) {
    char* buf;

    buf = (char*) pkg_malloc(UNSUPPORTED_LEN + _p->len + CRLF_LEN);
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

int build_expired_contact(contact_t* chi, contact_for_header_t** contact_header) {

    char *p, *cp;
    int len;
    char *tmp;
    int old_data_len = 0;

    contact_for_header_t* c_header = 0;

    len = chi->uri.len
                + 2 /*<>*/
                + chi->uri.len
                + EXPIRES_PARAM_LEN + INT2STR_MAX_LEN;

    if (c_header && c_header->data_len > 0) {
    	//contact header has already been started
    	old_data_len = c_header->data_len - CRLF_LEN;
    	c_header->data_len = c_header->data_len + len + CONTACT_SEP_LEN;
    } else {
    	if (!c_header) {//not yet built one
    		c_header = shm_malloc(sizeof(contact_for_header_t));
    		if (!c_header) {
    			LM_ERR("no more shm mem\n");
    			return 0;
    		}
    		memset(c_header, 0, sizeof(contact_for_header_t));
    	}
    	c_header->data_len = CONTACT_BEGIN_LEN + len + CRLF_LEN;
    }

    if (!c_header->data_len)
    	return 0;

    if (!c_header->buf || (c_header->buf_len < c_header->data_len)) {
        tmp = (char*) shm_malloc(c_header->data_len);
        if (!tmp) {
        	c_header->data_len = 0;
        	c_header->buf_len = 0;
            LM_ERR("no pkg memory left\n");
            return -1;
        }
        if (c_header->buf) {
            //copy and free
            memcpy(tmp, c_header->buf, old_data_len);
            shm_free(c_header->buf);
            c_header->buf = tmp;
        } else {
        	c_header->buf = tmp;
        }
    }
    p = c_header->buf + old_data_len; //just in case we append

    if (old_data_len) {
        //copy new contact
        memcpy(p, CONTACT_SEP, CONTACT_SEP_LEN);
        p += CONTACT_SEP_LEN;

    } else {
        //copy full new structure plus new contact
        memcpy(p, CONTACT_BEGIN, CONTACT_BEGIN_LEN);
        p += CONTACT_BEGIN_LEN;
    }
    memcpy(p++, "<", 1);
    memcpy(p, chi->uri.s, chi->uri.len);
    p += chi->uri.len;
    memcpy(p++, ">", 1);
    memcpy(p, EXPIRES_PARAM, EXPIRES_PARAM_LEN);
    p += EXPIRES_PARAM_LEN;
    cp = int2str((int) (0), &len);
    memcpy(p, cp, len);
    p += len;
    memcpy(p, CRLF, CRLF_LEN);
    p += CRLF_LEN;
    c_header->data_len = p - c_header->buf;

    LM_DBG("de-reg contact is [%.*s]\n", c_header->data_len, c_header->buf);
    *contact_header = c_header;
    return 0;
}

/*! \brief
 * Allocate a memory buffer and print Contact
 * header fields into it
 */

//We use shared memory for this so we can use it when we use async diameter

int build_contact(impurecord_t* impurec, contact_for_header_t** contact_header) {
    char *p, *cp;
    int fl, len;
    ucontact_t* c;
    *contact_header = 0;
    int i=0;

    contact_for_header_t* tmp_contact_header = shm_malloc(sizeof (contact_for_header_t));
    if (!tmp_contact_header) {
    	LM_ERR("no more memory\n");
    	return -1;
    }
    memset(tmp_contact_header, 0, sizeof (contact_for_header_t));

    tmp_contact_header->data_len = calc_buf_len(impurec);
    tmp_contact_header->buf = (char*)shm_malloc(tmp_contact_header->data_len);

    if (tmp_contact_header->data_len) {
        p = tmp_contact_header->buf;

        memcpy(p, CONTACT_BEGIN, CONTACT_BEGIN_LEN);
        p += CONTACT_BEGIN_LEN;

        fl = 0;
	    
        while (i<MAX_CONTACTS_PER_IMPU && (c=impurec->newcontacts[i])) {
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
                cp = int2str((int) (c->expires - act_time), &len);
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
	    i++;
        }

        memcpy(p, CRLF, CRLF_LEN);
        p += CRLF_LEN;

        tmp_contact_header->data_len = p - tmp_contact_header->buf;

        LM_DBG("created Contact HF: %.*s\n", tmp_contact_header->data_len, tmp_contact_header->buf);
    } else 
        LM_DBG("No Contact HF created, no contacts.\n");


    *contact_header = tmp_contact_header;
    return 0;
}

#define PASSOCIATEDURI "P-Associated-URI: "
#define PASSOCIATEDURI_LEN (sizeof(PASSOCIATEDURI)-1)

/*! \brief
 * Calculate the length of buffer needed to
 * print p-associated-uri
 */
static inline unsigned int calc_associateduri_buf_len(ims_subscription* s) {
    unsigned int len;
    int i, j;
    ims_public_identity* id;

    len = 0;
    for (i = 0; i < s->service_profiles_cnt; i++)
        for (j = 0; j < s->service_profiles[i].public_identities_cnt; j++) {
            id = &(s->service_profiles[i].public_identities[j]);
            if (!id->barring)
                len += 4 + id->public_identity.len; /*4 is for ">, <""*/
        }

    if (len) len += PASSOCIATEDURI_LEN + 2 + CRLF_LEN; // <> and \r\n;

    return len;
}

/*! \brief
 * Allocate a memory buffer and print P-Associated_URI
 * header fields into it
 */
int build_p_associated_uri(ims_subscription* s) {
    char *p;
    int i, j, cnt = 0;
    ims_public_identity* id;

    LM_DBG("Building P-Associated-URI\n");

    if (!s) {
        LM_ERR("Strange, no ims subscription data - how did we get here\n");
        return -1;
    }
    p_associated_uri.data_len = calc_associateduri_buf_len(s);
    if (!p_associated_uri.data_len)
        return -1;

    if (!p_associated_uri.buf || (p_associated_uri.buf_len < p_associated_uri.data_len)) {
        if (p_associated_uri.buf)
            pkg_free(p_associated_uri.buf);
        p_associated_uri.buf = (char*) pkg_malloc(p_associated_uri.data_len);
        if (!p_associated_uri.buf) {
            p_associated_uri.data_len = 0;
            p_associated_uri.buf_len = 0;
            LM_ERR("no pkg memory left\n");
            return -1;
        } else {
            p_associated_uri.buf_len = p_associated_uri.data_len;
        }
    }

    p = p_associated_uri.buf;
    memcpy(p, PASSOCIATEDURI, PASSOCIATEDURI_LEN);
    p += PASSOCIATEDURI_LEN;

    for (i = 0; i < s->service_profiles_cnt; i++)
        for (j = 0; j < s->service_profiles[i].public_identities_cnt; j++) {
            id = &(s->service_profiles[i].public_identities[j]);
            if (!id->barring) {
                if (cnt == 0)
                    *p++ = '<';
                else {
                    memcpy(p, ">, <", 4);
                    p += 4;
                }
                memcpy(p, id->public_identity.s, id->public_identity.len);
                p += id->public_identity.len;
                cnt++;
            }
        }
    if (cnt)
        *p++ = '>';

    memcpy(p, "\r\n", CRLF_LEN);
    p += CRLF_LEN;
    p_associated_uri.data_len = p - p_associated_uri.buf;
    LM_DBG("Created P-Associated-URI HF %.*s\n", p_associated_uri.data_len, p_associated_uri.buf);

    return 0;
}

/*! \brief
 * Send a reply
 */
int reg_send_reply_transactional(struct sip_msg* _m, contact_for_header_t* contact_header, struct cell* t_cell) {
    str unsup = str_init(OPTION_TAG_PATH_STR);
    long code;
    str msg = str_init(MSG_200); /* makes gcc shut up */
    char* buf;

    if (contact_header && contact_header->buf && contact_header->data_len > 0) {
    	LM_DBG("Contacts: %.*s\n", contact_header->data_len, contact_header->buf);
        add_lump_rpl(_m, contact_header->buf, contact_header->data_len, LUMP_RPL_HDR | LUMP_RPL_NODUP | LUMP_RPL_NOFREE);
        contact_header->data_len = 0;
    }

    if (rerrno == R_FINE && path_enabled && _m->path_vec.s) {
        if (path_mode != PATH_MODE_OFF) {
            if (parse_supported(_m) < 0 && path_mode == PATH_MODE_STRICT) {
                rerrno = R_PATH_UNSUP;
                if (add_unsupported(_m, &unsup) < 0)
                    return -1;
                if (add_path(_m, &_m->path_vec) < 0)
                    return -1;
            } else if (get_supported(_m) & F_OPTION_TAG_PATH) {
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

    code = codes[rerrno];
    switch (code) {
        case 200: msg.s = MSG_200;
            msg.len = sizeof (MSG_200) - 1;
            break;
        case 400: msg.s = MSG_400;
            msg.len = sizeof (MSG_400) - 1;
            break;
        case 420: msg.s = MSG_420;
            msg.len = sizeof (MSG_420) - 1;
            break;
        case 500: msg.s = MSG_500;
            msg.len = sizeof (MSG_500) - 1;
            break;
        case 503: msg.s = MSG_503;
            msg.len = sizeof (MSG_503) - 1;
            break;
    }

    if (code != 200) {
        buf = (char*) pkg_malloc(E_INFO_LEN + error_info[rerrno].len + CRLF_LEN + 1);
        if (!buf) {
            LM_ERR("no pkg memory left\n");
            return -1;
        }
        memcpy(buf, E_INFO, E_INFO_LEN);
        memcpy(buf + E_INFO_LEN, error_info[rerrno].s, error_info[rerrno].len);
        memcpy(buf + E_INFO_LEN + error_info[rerrno].len, CRLF, CRLF_LEN);
        add_lump_rpl(_m, buf, E_INFO_LEN + error_info[rerrno].len + CRLF_LEN,
                LUMP_RPL_HDR | LUMP_RPL_NODUP);

        if (code >= 500 && code < 600 && cfg_get(registrar, registrar_cfg, retry_after)) {
            if (add_retry_after(_m) < 0) {
                return -1;
            }
        }
    }

    if ((code > 199) && (code < 299)) {
        if (p_associated_uri.data_len > 0) {
            add_lump_rpl(_m, p_associated_uri.buf, p_associated_uri.data_len,
                    LUMP_RPL_HDR | LUMP_RPL_NODUP | LUMP_RPL_NOFREE);
            p_associated_uri.data_len = 0;
        }
        if (add_service_route(_m, &scscf_serviceroute_uri_str) < 0) { //TODO - need to insert orig into this scscf_name
            return -1;
        }
    }

    //if (slb.freply(_m, code, &msg) < 0) {
    if (tmb.t_reply_trans(t_cell, _m, code, msg.s) < 0){
        LM_ERR("failed to send %ld %.*s\n", code, msg.len, msg.s);
        return -1;
    } else return 0;
}


/*! \brief
 * Send a reply
 */
int reg_send_reply(struct sip_msg* _m, contact_for_header_t* contact_header) {
    str unsup = str_init(OPTION_TAG_PATH_STR);
    long code;
    str msg = str_init(MSG_200); /* makes gcc shut up */
    char* buf;

    if (contact_header && contact_header->buf && (contact_header->buf_len > 0) && (contact_header->data_len > 0)) {
    	LM_DBG("Contacts: %.*s\n", contact_header->data_len, contact_header->buf);
        add_lump_rpl(_m, contact_header->buf, contact_header->data_len, LUMP_RPL_HDR | LUMP_RPL_NODUP | LUMP_RPL_NOFREE);
        contact_header->data_len = 0;
    }

    if (rerrno == R_FINE && path_enabled && _m->path_vec.s) {
        if (path_mode != PATH_MODE_OFF) {
            if (parse_supported(_m) < 0 && path_mode == PATH_MODE_STRICT) {
                rerrno = R_PATH_UNSUP;
                if (add_unsupported(_m, &unsup) < 0)
                    return -1;
                if (add_path(_m, &_m->path_vec) < 0)
                    return -1;
            } else if (get_supported(_m) & F_OPTION_TAG_PATH) {
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

    code = codes[rerrno];
    switch (code) {
        case 200: msg.s = MSG_200;
            msg.len = sizeof (MSG_200) - 1;
            break;
        case 400: msg.s = MSG_400;
            msg.len = sizeof (MSG_400) - 1;
            break;
        case 420: msg.s = MSG_420;
            msg.len = sizeof (MSG_420) - 1;
            break;
        case 500: msg.s = MSG_500;
            msg.len = sizeof (MSG_500) - 1;
            break;
        case 503: msg.s = MSG_503;
            msg.len = sizeof (MSG_503) - 1;
            break;
    }

    if (code != 200) {
        buf = (char*) pkg_malloc(E_INFO_LEN + error_info[rerrno].len + CRLF_LEN + 1);
        if (!buf) {
            LM_ERR("no pkg memory left\n");
            return -1;
        }
        memcpy(buf, E_INFO, E_INFO_LEN);
        memcpy(buf + E_INFO_LEN, error_info[rerrno].s, error_info[rerrno].len);
        memcpy(buf + E_INFO_LEN + error_info[rerrno].len, CRLF, CRLF_LEN);
        add_lump_rpl(_m, buf, E_INFO_LEN + error_info[rerrno].len + CRLF_LEN,
                LUMP_RPL_HDR | LUMP_RPL_NODUP);

        if (code >= 500 && code < 600 && cfg_get(registrar, registrar_cfg, retry_after)) {
            if (add_retry_after(_m) < 0) {
                return -1;
            }
        }
    }

    if ((code > 199) && (code < 299)) {
        if (p_associated_uri.data_len > 0) {
            add_lump_rpl(_m, p_associated_uri.buf, p_associated_uri.data_len,
                    LUMP_RPL_HDR | LUMP_RPL_NODUP | LUMP_RPL_NOFREE);
            p_associated_uri.data_len = 0;
        }
        if (add_service_route(_m, &scscf_serviceroute_uri_str) < 0) { //TODO - need to insert orig into this scscf_name
            return -1;
        }
    }

    if (slb.freply(_m, code, &msg) < 0) {
        LM_ERR("failed to send %ld %.*s\n", code, msg.len, msg.s);
        return -1;
    } else return 0;
}

/*! \brief
 * Release contact buffer if any
 */
void free_contact_buf(contact_for_header_t* contact_header) {
    if (contact_header && contact_header->buf) {
        shm_free(contact_header->buf);
        contact_header->buf = 0;
        contact_header->buf_len = 0;
        contact_header->data_len = 0;
    }
    if (contact_header)shm_free(contact_header);
}

void free_p_associated_uri_buf(void) {
    if (p_associated_uri.buf) {
        pkg_free(p_associated_uri.buf);
        p_associated_uri.buf = 0;
        p_associated_uri.buf_len = 0;
        p_associated_uri.data_len = 0;
    }
}

void free_expired_contact_buf(void) {

}
