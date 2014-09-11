/**
 * $Id$
 *
 * Copyright (C) 2009 Daniel-Constantin Mierla (asipto.com)
 * Copyright (C) 2011 Carsten Bock, carsten@ng-voice.com
 *
 * This file is part of kamailio, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
	int to_route;
	char to_route_name[DLG_TOROUTE_SIZE];
	int to_bye;
	int timeout;
	struct dlg_cell *dlg;
	int set;
	unsigned int dir;
} dlg_ctx_t;

/* A dialog-variable */
struct dlg_var {
	str key;
	str value;
	unsigned int vflags;		/*!< internal variable flags */
	struct dlg_var *next;
};

str * api_get_dlg_variable(str *callid, str *ftag, str *ttag, str *key);
str * get_dlg_variable(struct dlg_cell *dlg, str *key);

int api_set_dlg_variable(str *callid, str *ftag, str *ttag, str *key, str *val);
int set_dlg_variable(struct dlg_cell *dlg, str *key, str *val);

int pv_parse_dialog_var_name(pv_spec_p sp, str *in);

int pv_get_dlg_variable(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);

int pv_set_dlg_variable(struct sip_msg* msg, pv_param_t *param, int op, pv_value_t *val);

/*! Retrieve the current var-list */
struct dlg_var * get_local_varlist_pointer(struct sip_msg *msg, int clear_pointer);

/* Adds, updates and deletes dialog variables */
int set_dlg_variable_unsafe(struct dlg_cell *dlg, str *key, str *val, int new);

extern dlg_ctx_t _dlg_ctx;

int pv_get_dlg_ctx(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res);
int pv_set_dlg_ctx(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val);
int pv_parse_dlg_ctx_name(pv_spec_p sp, str *in);

int pv_get_dlg(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res);
int pv_parse_dlg_name(pv_spec_p sp, str *in);

int dlg_cfg_cb(struct sip_msg *foo, unsigned int flags, void *bar);

void dlg_set_ctx_dialog(struct dlg_cell *dlg);
struct dlg_cell* dlg_get_ctx_dialog(void);

dlg_ctx_t* dlg_get_dlg_ctx(void);

int spiral_detect_reset(struct sip_msg *foo, unsigned int flags, void *bar);

#endif
