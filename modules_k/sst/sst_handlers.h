/*
 * $Id$
 *
 * Copyright (C) 2006 SOMA Networks, Inc.
 * Written By Ron Winacott
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
 * 2006-05-11  initial version (ronw)
 */


#ifndef _SST_HANDLERS_H_
#define _SST_HANDLERS_H_

#include "../../sr_module.h" /* Needed for find_export() */
#include "../../items.h"
#include "../../parser/msg_parser.h"
#include "../../str.h"
#include "../dialog/dlg_load.h"

/**
 * The sst_cb is the prototype for any callback functions you want to
 * register with the sst module via the register_sstcb() function
 * call. (see below.)
 *
 * @param did - The Dialog ID
 * @param type - The event that triggered the callback.
 * @param msg - The SIP message (request/response)
 * @param param - The pointer to the optional parameter you passed to
 *                the register_sstcb() function.
 */
typedef void (sst_cb)(struct dlg_cell *did, int type, 
		struct sip_msg *msg, void **param);

typedef int (*register_sst_f)(struct dlg_cell *did, int cb_types,
		sst_cb func, void *param);

/**
 * Called from other modules that want to register a callback with us.
 *
 * @param sst - The sst ID 
 * @param type - The call back type. See cbtype_t
 * @param func - The sst callback function that should be called on
 *               the type of event.
 * @param param - Any parameters you would like passed into the
 *                callback when the event triggers. it.
 *
 * @return 0 on success, non-zero on an error.
 */
int register_sstcb(struct dlg_cell *did, int type, sst_cb func, void *param);

/**
 * The static (opening) callback function for all dialog creations
 */
void sstDialogCreatedCB(struct dlg_cell *did, int type, 
		struct sip_msg* msg, void** param);

int sstCheckMinHandler(struct sip_msg *msg, char *str1, char *str2);

void sstHandlerInit(xl_spec_t *timeout_avp, unsigned int minSE);

#endif /* _SST_HANDLERSZ_H_ */
