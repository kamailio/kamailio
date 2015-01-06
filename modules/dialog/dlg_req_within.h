/*
 * Copyright (C) 2007 Voice System SRL
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
 * \brief Request
 * \ingroup dialog
 * Module: \ref dialog
 */

#ifndef DLG_REQUEST_WITHIN_H
#define DLG_REQUEST_WITHIN_H

#include "dlg_hash.h"
#include "../../modules/tm/tm_load.h"

#define MAX_FWD			"70"
#define MAX_SIZE		256
#define RCV_BYE_REPLY	1

#define MI_DIALOG_NOT_FOUND 		"Requested Dialog not found"
#define MI_DIALOG_NOT_FOUND_LEN 	(sizeof(MI_DIALOG_NOT_FOUND)-1)
#define MI_DLG_OPERATION_ERR		"Operation failed"
#define MI_DLG_OPERATION_ERR_LEN	(sizeof(MI_DLG_OPERATION_ERR)-1)

extern struct tm_binds d_tmb;
extern int dlg_enable_stats;
extern stat_var * active_dlgs;

struct mi_root * mi_terminate_dlg(struct mi_root *cmd_tree, void *param );

dlg_t* build_dlg_t(struct dlg_cell * cell, int dir);
int free_tm_dlg(dlg_t *td);
int dlg_bye(struct dlg_cell *dlg, str *hdrs, int side);
int dlg_bye_all(struct dlg_cell *dlg, str *hdrs);
int dlg_send_ka(dlg_cell_t *dlg, int dir);

#endif
