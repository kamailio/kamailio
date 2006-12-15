/*
 * $Id$
 *
 * Copyright (C) 2006 Voice System SRL
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 * 2006-04-14  initial version (bogdan)
 */


#ifndef _DIALOG_DLG_HANDLERS_H_
#define _DIALOG_DLG_HANDLERS_H_

#include "../../parser/msg_parser.h"
#include "../../str.h"
#include "../../items.h"
#include "../tm/t_hooks.h"
#include "dlg_timer.h"

#define MAX_DLG_RR_PARAM_NAME 32

void init_dlg_handlers(char *rr_param, int dlg_flag,
		xl_spec_t *timeout_avp, int default_timeout, int use_tight_match);

void destroy_dlg_handlers();

void dlg_onreq(struct cell* t, int type, struct tmcb_params *param);

void dlg_onroute(struct sip_msg* req, str *rr_param, void *param);

void dlg_ontimeout( struct dlg_tl *tl);

/* item/pseudo-variables functions */
int it_get_dlg_lifetime(struct sip_msg *msg, xl_value_t *res,
		xl_param_t *param, int flags);

int it_get_dlg_status(struct sip_msg *msg, xl_value_t *res,
		xl_param_t *param, int flags);
#endif
