/*
 * pua_dialoginfo module - publish dialog-info from dialog module
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
 * Copyright (C) 2008 Klaus Darilion IPCom
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
 */

#ifndef _PUA_DLGINFO_H
#define _PUA_DLGINFO_H
#include "../../core/locking.h"
#include "../pua/pua_bind.h"

extern send_publish_t pua_send_publish;

void dialog_publish_multi(char *state, struct str_list* ruris, str *entity, str *peer, str *callid,
	unsigned int initiator, unsigned int lifetime, str *localtag, str *remotetag,
	str *localtarget, str *remotetarget, unsigned short do_pubruri_localcheck, str *uuid);

/* store the important data locally to avoid reading the data from the
 * dlg_cell during the callback (as this could create a race condition
 * if the dlg_cell gets meanwhile deleted) */
struct dlginfo_cell {
	gen_lock_t lock;
	str from_uri;
	str to_uri;
	str callid;
	str from_tag;
	/* str *to_tag; */
	str req_uri;
	str from_contact;
	struct str_list* pubruris_caller;
	struct str_list* pubruris_callee;
	unsigned int lifetime;
	/*dialog module does not always resend all flags, so we use flags set on first request*/
	int disable_caller_publish;
	int disable_callee_publish;
	str uuid;
};


void free_dlginfo_cell(void *param);
void free_str_list_all(struct str_list * del_current);

#endif
