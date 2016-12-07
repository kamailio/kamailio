/*
 *
 * $Id$
 *
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

#ifndef _EXEC_HF_H
#define _EXEC_HF_H

#include "../../parser/msg_parser.h"

/* prefix prepended to header field name in env var name */
#define SIP "SIP_"
#define HF_PREFIX SIP "HF_"
#define HF_PREFIX_LEN (sizeof(HF_PREFIX)-1)
/* well known variable names */
#define EV_SRCIP SIP "SRCIP"
#define EV_RURI SIP "RURI"
#define EV_ORURI SIP "ORUI"
#define EV_USER SIP "USER"
#define EV_OUSER SIP "OUSER"
#define EV_TID SIP "TID"
#define EV_DID SIP "DID"
/* env var assignment operator */
#define EV_ASSIGN '='
/* header field separator */
#define HF_SEPARATOR ','
/* RFC3261 -- characters legal in header names; a really
 * _bloated_ thing
 */
#define UNRESERVED_MARK	"-_.!~*'()"
#define HNV_UNRESERVED	"[]/?:+$"
#define ESCAPE '%'
/* and this is what all such crazy symbols in header field
 * name will be replaced with in env vars */
#define HFN_SYMBOL '_'

#define VAR_VIA "VIA"
#define VAR_VIA_LEN (sizeof(VAR_VIA)-1)
#define VAR_CTYPE "CONTENT_TYPE"
#define VAR_CTYPE_LEN (sizeof(VAR_CTYPE)-1)
#define VAR_FROM "FROM"
#define VAR_FROM_LEN (sizeof(VAR_FROM)-1)
#define VAR_CALLID "CALLID"
#define VAR_CALLID_LEN (sizeof(VAR_CALLID)-1)
#define VAR_SUPPORTED "SUPPORTED"
#define VAR_SUPPORTED_LEN (sizeof(VAR_SUPPORTED)-1)
#define VAR_CLEN "CONTENT_LENGTH"
#define VAR_CLEN_LEN (sizeof(VAR_CLEN)-1)
#define VAR_CONTACT "CONTACT"
#define VAR_CONTACT_LEN (sizeof(VAR_CONTACT)-1)
#define VAR_TO "TO"
#define VAR_TO_LEN (sizeof(VAR_TO)-1)
#define VAR_EVENT "EVENT"
#define VAR_EVENT_LEN (sizeof(VAR_EVENT)-1)



#if 0
/* _JUST_FOR_INFO_HERE */
struct hdr_field {
        int type;                /* Header field type */
        str name;                /* Header field name */
        str body;                /* Header field body */
        void* parsed;            /* Parsed data structures */
        struct hdr_field* next;  /* Next header field in the list */
};
#endif

typedef struct env {
	char** env;
	int old_cnt;
} environment_t;

struct attrval {
	str attr;
	str val;
};

enum wrapper_type { W_HF=1, W_AV };

struct hf_wrapper {
	enum wrapper_type var_type;
	union {
		struct hdr_field *hf;
		struct attrval av;
	} u;
	/* next header field of the same type */
	struct hf_wrapper *next_same;
	/* next header field of a different type */
	struct hf_wrapper *next_other;
	/* env var value (zero terminated) */
	char *envvar;
	/* variable name prefix (if any) */
	char *prefix;
	int prefix_len;
};

extern unsigned int setvars;
extern char **environ;

environment_t *set_env(struct sip_msg *msg);
void unset_env(environment_t *backup_env);

#endif
