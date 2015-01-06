/*
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
 */


/*!
 * \file
 * \brief Functions related to dialog handling
 * \ingroup dialog
 * Module: \ref dialog
 */

#ifndef _DIALOG_DLG_LOAD_H_
#define _DIALOG_DLG_LOAD_H_

#include "dlg_cb.h"
#include "../../sr_module.h"

/* terminate_dlg function prototype */
typedef int (*terminate_dlg_f)(struct dlg_cell* dlg, str *hdrs);

typedef struct dlg_cell *(*get_dlg_f)(struct sip_msg *msg);

typedef void (*release_dlg_f)(struct dlg_cell *dlg);

struct dlg_binds {
	register_dlgcb_f  register_dlgcb;
	terminate_dlg_f terminate_dlg;
    set_dlg_variable_f set_dlg_var;
	get_dlg_variable_f get_dlg_var;
	get_dlg_f          get_dlg;
	release_dlg_f      release_dlg;
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
