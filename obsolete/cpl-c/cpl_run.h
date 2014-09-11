/*
 * $Id$
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

#ifndef _CPL_RUN_H
#define _CPL_RUN_H

#include "../../str.h"
#include "../../parser/msg_parser.h"

#define SCRIPT_END               0
#define SCRIPT_DEFAULT           1
#define SCRIPT_TO_BE_CONTINUED   2
#define SCRIPT_RUN_ERROR         -1
#define SCRIPT_FORMAT_ERROR      -2

#define CPL_RUN_OUTGOING               (1<<0)
#define CPL_RUN_INCOMING               (1<<1)
#define CPL_IS_STATEFUL                (1<<2)
#define CPL_FORCE_STATEFUL             (1<<3)
#define CPL_LOC_SET_MODIFIED           (1<<5)
#define CPL_PROXY_DONE                 (1<<6)
#define CPL_RURI_DUPLICATED            (1<<10)
#define CPL_TO_DUPLICATED              (1<<11)
#define CPL_FROM_DUPLICATED            (1<<12)
#define CPL_SUBJECT_DUPLICATED         (1<<13)
#define CPL_ORGANIZATION_DUPLICATED    (1<<14)
#define CPL_USERAGENT_DUPLICATED       (1<<15)
#define CPL_ACCEPTLANG_DUPLICATED      (1<<16)
#define CPL_PRIORITY_DUPLICATED        (1<<17)

#define STR_NOT_FOUND           ((str*)0xffffffff)


struct cpl_interpreter {
	unsigned int flags;
	str user;              /* user */
	str script;            /* CPL script */
	char *ip;              /* instruction pointer */
	int recv_time;         /* receiving time stamp */
	struct sip_msg *msg;
	struct location *loc_set;     /* location set */
	/* pointers to the string-headers needed for switches; can point directly
	 * into the sip_msg structure (if no proxy took places) or to private
	 * buffers into shm_memory (after a proxy happend); if a hdr is copy into a
	 * private buffer, a corresponding flag will be set (xxxx_DUPLICATED) */
	str *ruri;
	str *to;
	str *from;
	str *subject;
	str *organization;
	str *user_agent;
	str *accept_language;
	str *priority;
	/* grouped date the is needed when doing proxy */
	struct proxy_st {
		unsigned short ordering;
		unsigned short recurse;
		/* I have to know which will be the last location that will be proxy */
		struct location *last_to_proxy;
		/* shortcuts to the subnodes */
		char *busy;
		char *noanswer;
		char *redirect;
		char *failure;
		char *default_;
	}proxy;
};

struct cpl_interpreter* new_cpl_interpreter( struct sip_msg *msg, str *script);

void free_cpl_interpreter(struct cpl_interpreter *intr);

int cpl_run_script( struct cpl_interpreter *cpl_intr );

#endif


