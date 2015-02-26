/*
 * Sanity Checks Module
 *
 * Copyright (C) 2006 iptelorg GbmH
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

#ifndef MOD_SANITY_CHK_H
#define MOD_SANITY_CHK_H

#include "../../str.h"
#include "../../modules/sl/sl.h"
#include "../../parser/msg_parser.h"

#define SANITY_RURI_SIP_VERSION        (1<<0)
#define SANITY_RURI_SCHEME             (1<<1)
#define SANITY_REQUIRED_HEADERS        (1<<2)
#define SANITY_VIA_SIP_VERSION         (1<<3)
#define SANITY_VIA_PROTOCOL            (1<<4)
#define SANITY_CSEQ_METHOD             (1<<5)
#define SANITY_CSEQ_VALUE              (1<<6)
#define SANITY_CL                      (1<<7)
#define SANITY_EXPIRES_VALUE           (1<<8)
#define SANITY_PROXY_REQUIRE           (1<<9)
#define SANITY_PARSE_URIS              (1<<10)
#define SANITY_CHECK_DIGEST            (1<<11)
#define SANITY_CHECK_DUPTAGS           (1<<12)
#define SANITY_MAX_CHECKS              (1<<13)  /* Make sure this is the highest value */

/* VIA_SIP_VERSION and VIA_PROTOCOL do not work yet
 * and PARSE_URIS is very expensive */
#define SANITY_DEFAULT_CHECKS 	SANITY_RURI_SIP_VERSION | \
								SANITY_RURI_SCHEME | \
								SANITY_REQUIRED_HEADERS | \
								SANITY_CSEQ_METHOD | \
								SANITY_CSEQ_VALUE | \
								SANITY_CL | \
								SANITY_EXPIRES_VALUE | \
								SANITY_PROXY_REQUIRE | \
                                SANITY_CHECK_DIGEST


#define SANITY_URI_CHECK_RURI    (1<<0)
#define SANITY_URI_CHECK_FROM    (1<<1)
#define SANITY_URI_CHECK_TO      (1<<2)
#define SANITY_URI_CHECK_CONTACT (1<<3)
#define SANITY_URI_MAX_CHECKS    (1<<4)  /* Make sure this is the highest value */

#define SANITY_DEFAULT_URI_CHECKS	SANITY_URI_CHECK_RURI | \
									SANITY_URI_CHECK_FROM | \
									SANITY_URI_CHECK_TO

#define SANITY_CHECK_PASSED 1
#define SANITY_CHECK_FAILED 0
#define SANITY_CHECK_ERROR -1

struct _strlist {
	str string;            /* the string */
	struct _strlist* next; /* the next strlist element */
};

typedef struct _strlist strl;

extern int default_checks;
extern strl* proxyrequire_list;

extern sl_api_t slb;

#endif /* MOD_SANITY_CHK_H */
