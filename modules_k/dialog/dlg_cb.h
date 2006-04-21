/*
 * $Id$
 *
 * Copyright (C) 2006 Voice Sistem SRLs
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
 *  2006-04-11  initial version (bogdan)
 */

#ifndef _DIALOG_DLG_CB_H_
#define _DIALOG_DLG_CB_H_

#include "../../parser/msg_parser.h"

struct dlg_cell* dlg;

/* callback function prototype */
typedef void (dialog_cb) (struct dlg_cell* dlg, int type, struct sip_msg* msg,
		void** param);
/* register callback function prototype */
typedef int (*register_dlgcb_f)(struct dlg_cell* dlg, int cb_types,
		dialog_cb f, void *param);


#define DLGCB_CREATED      (1<<0)
#define DLGCB_FAILED       (1<<1)
#define DLGCB_CONFIRMED    (1<<2)
#define DLGCB_REQ_WITHIN   (1<<3)
#define DLGCB_TERMINATED   (1<<4)
#define DLGCB_EXPIRED      (1<<5)
#define DLGCB_EARLY        (1<<6)
#define DLGCB_RESPONSE_FWDED (1<<7)

struct dlg_callback {
	int types;
	dialog_cb* callback;
	void *param;
	struct dlg_callback* next;
};


struct dlg_head_cbl {
	struct dlg_callback *first;
	int types;
};

int init_dlg_callbacks();

void destroy_dlg_callbacks();

int register_dlgcb( struct dlg_cell* dlg, int types, dialog_cb f, void *param);

void run_create_callbacks(struct dlg_cell *dlg, struct sip_msg *msg);

void run_dlg_callbacks( int type , struct dlg_cell *dlg, struct sip_msg *msg);


#endif
