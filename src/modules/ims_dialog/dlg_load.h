/*
 * $Id$
 *
 * dialog module - basic support for dialog tracking
 *
 * Copyright (C) 2006 Voice Sistem SRL
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
 * History:
 * --------
 *  2006-04-14  initial version (bogdan)
 */

#ifndef _DIALOG_DLG_LOAD_H_
#define _DIALOG_DLG_LOAD_H_

#include "dlg_cb.h"
#include "../../core/sr_module.h"

/* terminate_dlg function prototype */
typedef int (*ims_terminate_dlg_f)(
		str *callid, str *ftag, str *ttag, str *hdrs, str *reason);

typedef int (*ims_lookup_terminate_dlg_f)(
		unsigned int h_entry, unsigned int h_id, str *hdrs);

/* get the current dialog based on message function prototype */
typedef struct dlg_cell *(*ims_get_dlg_f)(struct sip_msg *msg);

/* get_dlg_lifetime function prototype */
typedef time_t (*ims_get_dlg_expires_f)(str *callid, str *ftag, str *ttag);

typedef void (*ims_release_dlg_f)(struct dlg_cell *dlg);

typedef struct ims_dlg_binds
{
	ims_register_dlgcb_f register_dlgcb;
	ims_register_dlgcb_nodlg_f register_dlgcb_nodlg;
	ims_terminate_dlg_f terminate_dlg;
	ims_lookup_terminate_dlg_f lookup_terminate_dlg;
	ims_set_dlg_variable_f set_dlg_var;
	ims_get_dlg_variable_f get_dlg_var;
	ims_get_dlg_expires_f get_dlg_expires;
	ims_get_dlg_f get_dlg;
	ims_release_dlg_f release_dlg;
} ims_dlg_api_t;


typedef int (*load_ims_dlg_f)(ims_dlg_api_t *dlgb);
int load_ims_dlg(ims_dlg_api_t *dlgb);

static inline int load_ims_dlg_api(ims_dlg_api_t *dlgb)
{
	load_ims_dlg_f load_ims_dlg_p;

	/* import the DLG auto-loading function */
	if(!(load_ims_dlg_p = (load_ims_dlg_f)find_export("load_ims_dlg", 0, 0)))
		return -1;

	/* let the auto-loading function load all DLG stuff */
	if(load_ims_dlg_p(dlgb) == -1)
		return -1;

	return 0;
}


#endif
