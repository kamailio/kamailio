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

#ifndef _CPL_RUN_H
#define _CPL_RUN_H

#include "../../str.h"
#include "../../parser/msg_parser.h"

#define SCRIPT_END               1
#define SCRIPT_TO_BE_CONTINUED   2

#define CPL_RUN_OUTGOING        (1<<0)
#define CPL_RUN_INCOMING        (1<<1)
#define CPL_LOC_SET_MODIFIED    (1<<2)
#define CPL_PROXT_DONE          (1<<3)


struct cpl_interpreter {
	unsigned int flags;
	str user;              /* user */
	str script;            /* CPL script */
	unsigned char *ip;     /* instruction pointer */
	int recv_time;         /* receiving time stamp */
	struct sip_msg *msg;
//	unsigned char  type;
	struct location *loc_set;     /* location set */
	str *ruri;
	str *to;
	str *from;
	str *subject;
	str *organization;
	str *user_agent;
	str *accepted_langs;
	str *priority;
};

struct cpl_interpreter* new_cpl_interpreter( struct sip_msg *msg, str *script);

void free_cpl_interpreter(struct cpl_interpreter *intr);

int run_cpl_script( struct cpl_interpreter *cpl_intr );

#endif


