/*
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

#ifndef _MSFUNCS_H_
#define _MSFUNCS_H_

#include "../../str.h"

#define CT_TYPE		1
#define CT_CHARSET	2
#define CT_MSGR		4

#ifdef MSILO_TAG
#undef MSILO_TAG
#endif
#define MSILO_TAG	"msilo-HI4U-Ah0X-bZ98-"

typedef struct _content_type
{
	str type;
	str charset;
	str msgr;
} t_content_type;

/** apostrophes escape - useful for MySQL strings */
int apo_escape(char*, int, char*, int);

/** extract content-type value */
int extract_content_type(char*, int, t_content_type*, int);

/** sends IM using IM library */
int m_send_message(int mid, str *uri, str *to, str *from, str *contact,
				str *ctype, str *msg);

/** build MESSAGE headers */
int m_build_headers(str *buf, str ctype, str contact);

/** build MESSAGE body */
int m_build_body(str *body, int date, str msg);

#endif

