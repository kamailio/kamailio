/*
 * Copyright (C) 2005 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */


#ifndef _TM_T_REDIRECT_H
#define _TM_T_REDIRECT_H

#include "../../parser/msg_parser.h"
#include "../../sr_module.h"
#include "../../str.h"
#include "../../modules/tm/tm_load.h"
#include "../acc/acc_logic.h"

typedef int (*tm_get_trans_f)( struct sip_msg*, struct cell**);

extern struct tm_binds rd_tmb;
extern cmd_function   rd_acc_fct;

extern char *acc_db_table;

int get_redirect( struct sip_msg *msg , int maxt, int maxb,
		struct acc_param *reason, unsigned int bflags);

#endif

