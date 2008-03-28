/*
 * $Id$
 *
 * Copyright (C) 2006 SOMA Networks, Inc.
 * Written By Ron Winacott (karwin)
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 * History:
 * --------
 * 2006-05-11 initial version (karwin)
 * 2006-10-10 Code cleanup of this header file. (karwin)
 */

#ifndef _SST_HANDLERS_H_
#define _SST_HANDLERS_H_

#include "../../sr_module.h" /* Needed for find_export() */
#include "../../pvar.h"
#include "../../parser/msg_parser.h"
#include "../dialog/dlg_load.h"

/**
 * The static (opening) callback function for all dialog creations
 */
void sst_dialog_created_CB(struct dlg_cell *did, int type, 
		struct dlg_cb_params * params);

/**
 * The script function
 */
int sst_check_min(struct sip_msg *msg, char *str1, char *str2);

/**
 * The handlers initializer function
 */
void sst_handler_init(pv_spec_t *timeout_avp, unsigned int minSE, 
		int flag, unsigned int reject);

#endif /* _SST_HANDLERS_H_ */
