/*
 * Copyright (C) 2009 Daniel-Constantin Mierla (asipto.com)
 * Copyright (C) 2011 Carsten Bock, carsten@ng-voice.com
 *
 * This file is part of kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*!
 * \file
 * \brief Dialog variables
 * \ingroup dialog
 * Module: \ref dialog
 */
		       
#ifndef _DLG_VAR_H_
#define _DLG_VAR_H_

#include "../../pvar.h"
#include "dlg_hash.h"

#define DLG_TOROUTE_SIZE	32
/*! dialog context */
typedef struct _dlg_ctx {
	int on;
	unsigned int flags;
	unsigned int iflags;
	int to_route;
	char to_route_name[DLG_TOROUTE_SIZE];
	int to_bye;
	int timeout;
	dlg_cell_t *dlg1;
	dlg_iuid_t iuid;
	int cpid;
	int set;
	unsigned int dir;
	int t;				/* set to 1 if tm req in callback executed */
	int expect_t;		/* set to 1 if expects that t is set after config */
} dlg_ctx_t;

/* A dialog-variable */
typedef struct dlg_var {
	str key;
	str value;
	unsigned int vflags;		/*!< internal variable flags */
	struct dlg_var *next;
} dlg_var_t;

str* get_dlg_variable(dlg_cell_t *dlg, str *key);
int set_dlg_variable(dlg_cell_t *dlg, str *key, str *val);

int pv_parse_dialog_var_name(pv_spec_p sp, str *in);

int pv_get_dlg_variable(sip_msg_t *msg, pv_param_t *param, pv_value_t *res);

int pv_set_dlg_variable(sip_msg_t *msg, pv_param_t *param, int op, pv_value_t *val);

/*! Retrieve the current var-list */
dlg_var_t *get_local_varlist_pointer(sip_msg_t *msg, int clear_pointer);

/* Adds, updates and deletes dialog variables */
int set_dlg_variable_unsafe(dlg_cell_t *dlg, str *key, str *val);

extern dlg_ctx_t _dlg_ctx;

int pv_get_dlg_ctx(sip_msg_t *msg,  pv_param_t *param,
		pv_value_t *res);
int pv_set_dlg_ctx(sip_msg_t *msg, pv_param_t *param,
		int op, pv_value_t *val);
int pv_parse_dlg_ctx_name(pv_spec_p sp, str *in);

int pv_get_dlg(sip_msg_t *msg,  pv_param_t *param,
		pv_value_t *res);
int pv_parse_dlg_name(pv_spec_p sp, str *in);

int dlg_cfg_cb(sip_msg_t *foo, unsigned int flags, void *bar);
int cb_dlg_cfg_reset(sip_msg_t *msg, unsigned int flags, void *cbp);
int cb_dlg_locals_reset(sip_msg_t *msg, unsigned int flags, void *cbp);

void dlg_set_ctx_iuid(dlg_cell_t *dlg);
void dlg_reset_ctx_iuid(void);
dlg_cell_t* dlg_get_ctx_dialog(void);

dlg_ctx_t* dlg_get_dlg_ctx(void);

int spiral_detect_reset(sip_msg_t *foo, unsigned int flags, void *bar);

#endif
