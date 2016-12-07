/**
 * $Id$
 *
 * eXtended JABber module
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

/***
 * ---
 *
 * History
 * -------
 * 2003-06-05  previously added macro replaced with 'xj_extract_aor', (dcm)
 * 2003-05-09  added macro for adjusting a SIP address, (dcm)
 */


#ifndef _XJAB_BASE_H_
#define _XJAB_BASE_H_

#include "../../str.h"

#define XJ_NULL				0
#define XJ_SEND_MESSAGE		1
#define XJ_JOIN_JCONF		2
#define XJ_EXIT_JCONF		4
#define XJ_GO_ONLINE		8
#define XJ_GO_OFFLINE		16
#define XJ_REG_WATCHER		32
#define XJ_DEL_WATCHER		64

#define XJ_FLAG_OPEN		0
#define XJ_FLAG_CLOSE		1

typedef void (*pa_callback_f)(str* _user, str* _contact, int _state, void *p);

/**********             ***/

typedef struct _xj_jkey
{
	int hash;
	int flag;
	str *id;
} t_xj_jkey, *xj_jkey;

/**********             ***/

typedef struct _xj_sipmsg
{
	int type;			// type of message
	xj_jkey jkey;		// pointer to FROM
	str to;				// destination
	str msg;			// message body
	pa_callback_f cbf;	// callback function
	void *p;			// callback parameter
} t_xj_sipmsg, *xj_sipmsg;

/**********   LOOK AT IMPLEMENTATION OF FUNCTIONS FOR DESCRIPTION    ***/

void xj_sipmsg_free(xj_sipmsg);

/**********             ***/

int xj_jkey_cmp(void*, void*);
void xj_jkey_free_p(void*);
void xj_jkey_free(xj_jkey);

/**********             ***/

int xj_get_hash(str*, str*);
char *shahash(const char *);

int xj_extract_aor(str*, int);

#endif

