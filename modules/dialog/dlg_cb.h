/*
 * Copyright (C) 2006 Voice Sistem SRLs
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
 * \brief Callbacks
 * \ingroup dialog
 * Module: \ref dialog
 */

#ifndef _DIALOG_DLG_CB_H_
#define _DIALOG_DLG_CB_H_

#include "../../parser/msg_parser.h"

struct dlg_cell;

struct dlg_cb_params {
	struct sip_msg* req;       /* sip request msg related to the callback event */
    struct sip_msg* rpl;       /* sip reply msg related to the callback event */
	unsigned int direction;    /* direction of the sip msg */
	void *dlg_data;            /* generic paramter, specific to callback */
	void **param;              /* parameter passed at callback registration*/
};

/* callback function prototype */
typedef void (dialog_cb) (struct dlg_cell* dlg, int type, 
		struct dlg_cb_params * params);
/* function to free the callback param */
typedef void (param_free_cb) (void *param);
/* register callback function prototype */
typedef int (*register_dlgcb_f)(struct dlg_cell* dlg, int cb_types,
		dialog_cb f, void *param, param_free_cb ff);

/* method to set a variable within a dialog */
typedef int (*set_dlg_variable_f)( struct dlg_cell* dlg,
                                   str* key,
                                   str* val);
/* method to get a variable from a dialog */
typedef str* (*get_dlg_variable_f)( struct dlg_cell* dlg,
                                    str* key);

#define CONFIRMED_DIALOG_STATE 1

#define DLGCB_LOADED          (1<<0)
#define DLGCB_CREATED         (1<<1)
#define DLGCB_FAILED          (1<<2)
#define DLGCB_CONFIRMED_NA    (1<<3)
#define DLGCB_CONFIRMED       (1<<4)
#define DLGCB_REQ_WITHIN      (1<<5)
#define DLGCB_TERMINATED      (1<<6)
#define DLGCB_EXPIRED         (1<<7)
#define DLGCB_EARLY           (1<<8)
#define DLGCB_RESPONSE_FWDED  (1<<9)
#define DLGCB_RESPONSE_WITHIN (1<<10)
#define DLGCB_MI_CONTEXT      (1<<11)
#define DLGCB_RPC_CONTEXT     (1<<12)
#define DLGCB_DESTROY         (1<<13)
#define DLGCB_SPIRALED        (1<<14)
#define DLGCB_TERMINATED_CONFIRMED (1<<15)

struct dlg_callback {
	int types;
	dialog_cb* callback;
	void *param;
	param_free_cb* callback_param_free;
	struct dlg_callback* next;
};


struct dlg_head_cbl {
	struct dlg_callback *first;
	int types;
};


void destroy_dlg_callbacks(unsigned int type);

void destroy_dlg_callbacks_list(struct dlg_callback *cb);

int register_dlgcb( struct dlg_cell* dlg, int types, dialog_cb f, void *param, param_free_cb ff);

void run_create_callbacks(struct dlg_cell *dlg, struct sip_msg *msg);

void run_dlg_callbacks( int type ,
                        struct dlg_cell *dlg,
                        struct sip_msg *req,
                        struct sip_msg *rpl,
                        unsigned int dir,
                        void *dlg_data);

void run_load_callbacks( void );


/*!
 * \brief Function that returns valid SIP message from given dialog callback parameter struct
 * \param cb_params dialog callback parameter struct
 * \return pointer to valid SIP message if existent, NULL otherwise
 */
static inline struct sip_msg *dlg_get_valid_msg(struct dlg_cb_params *cb_params)
{
	struct sip_msg *msg;

	if (cb_params == NULL) {
		LM_ERR("no dialog parameters given\n");
		return NULL;
	}

	msg = cb_params->req;
	if (msg == NULL) {
		msg = cb_params->rpl;
		if (msg == NULL || msg == FAKED_REPLY) {
			return NULL;
		}
	}

	return msg;
};

#endif
