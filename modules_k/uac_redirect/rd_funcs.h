/*
 * $Id$
 *
 * Copyright (C) 2005 Voice Sistem SRL
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *
 * History:
 * ---------
 *  2005-06-22  first version (bogdan)
 */


#ifndef _TM_T_REDIRECT_H
#define _TM_T_REDIRECT_H

#include "../../parser/msg_parser.h"
#include "../../sr_module.h"
#include "../../str.h"
#include "../tm/tm_load.h"

typedef int (*tm_get_trans_f)( struct sip_msg*, struct cell**);

extern struct tm_binds rd_tmb;
extern cmd_function   rd_acc_fct;

extern char *acc_db_table;

int get_redirect( struct sip_msg *msg , int maxt, int maxb, str *reason);

#endif

