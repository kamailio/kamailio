/*
 * Copyright (C) 2009 Sippy Software, Inc., http://www.sippysoft.com
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

#ifndef _PYTHON_EXEC_H
#define  _PYTHON_EXEC_H

#include "../../core/parser/msg_parser.h"

typedef struct sr_apy_env {
	sip_msg_t *msg;
} sr_apy_env_t;

sr_apy_env_t *sr_apy_env_get();

int apy_exec(sip_msg_t *_msg, char *fname, char *fparam, int emode);
int python_exec1(sip_msg_t *, char *, char *);
int python_exec2(sip_msg_t *, char *, char *);

#endif
