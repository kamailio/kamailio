/*
 * Copyright (C) 2008 SOMA Networks, Inc.
 * Written By Ovidiu Sas (osas)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 *
 */

/*!
 *\file  sst/sst_rpc.c
 *\brief Manager functions for the SST module
 * \ingroup sst
 * Module: \ref sst
 */


#include "../../core/ut.h"
#include "../../core/rpc.h"
#include "../dialog/dlg_load.h"
#include "sst_handlers.h"
#include "sst_rpc.h"

/*! \brief
 * The dialog rpc helper function.
 */
void sst_dialog_rpc_context_CB(struct dlg_cell* did, int type,
		struct dlg_cb_params * params)
{
	rpc_cb_ctx_t *rpc_cb = (rpc_cb_ctx_t*)(params->dlg_data);
	rpc_t *rpc = rpc_cb->rpc;
	void *c = rpc_cb->c;
	sst_info_t* sst_info = (sst_info_t*)*(params->param);

	rpc->rpl_printf(c, "sst_requester_flags: %d", sst_info->requester);
	rpc->rpl_printf(c, "sst_supported_flags: %d", sst_info->supported);
	rpc->rpl_printf(c, "sst_interval: %d", sst_info->interval);
}
