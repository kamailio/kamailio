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
#include "../../sr_module.h"

/* terminate_dlg function prototype */
typedef int (*terminate_dlg_f)(str *callid, str *ftag, str *ttag, str *hdrs, str *reason);

typedef int (*lookup_terminate_dlg_f)(unsigned int h_entry, unsigned int h_id, str *hdrs);

/* get the current dialog based on message function prototype */
typedef struct dlg_cell *(*get_dlg_f)(struct sip_msg *msg);

/* get_dlg_lifetime function prototype */
typedef time_t (*get_dlg_expires_f)(str *callid, str *ftag, str *ttag);

typedef void (*release_dlg_f)(struct dlg_cell *dlg);

struct dlg_binds {
	register_dlgcb_f  		register_dlgcb;
	register_dlgcb_nodlg_f          register_dlgcb_nodlg;
	terminate_dlg_f 		terminate_dlg;
	lookup_terminate_dlg_f          lookup_terminate_dlg;
	set_dlg_variable_f 		set_dlg_var;
	get_dlg_variable_f 		get_dlg_var;
	get_dlg_expires_f 		get_dlg_expires;
	get_dlg_f			get_dlg;
        release_dlg_f                   release_dlg;
};


typedef int(*load_dlg_f)( struct dlg_binds *dlgb );
int load_dlg( struct dlg_binds *dlgb);

static inline int load_dlg_api( struct dlg_binds *dlgb )
{
	load_dlg_f load_dlg;

	/* import the DLG auto-loading function */
	if ( !(load_dlg=(load_dlg_f)find_export("load_dlg", 0, 0)))
		return -1;

	/* let the auto-loading function load all DLG stuff */
	if (load_dlg( dlgb )==-1)
		return -1;

	return 0;
}


#endif
