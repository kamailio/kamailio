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
 *\file  sst/sst_mi.c
 *\brief Manager functions for the SST module
 * \ingroup sst
 * Module: \ref sst
 */


#include "../../ut.h"
#include "../../lib/kmi/mi.h"
#include "../dialog/dlg_load.h"
#include "sst_handlers.h"

/*! \brief
 * The dialog mi helper function.
 */
void sst_dialog_mi_context_CB(struct dlg_cell* did, int type, struct dlg_cb_params * params)
{
	struct mi_node* parent_node = (struct mi_node*)(params->dlg_data);
	struct mi_node* node;
	struct mi_attr* attr;
	sst_info_t* sst_info = (sst_info_t*)*(params->param);
	char* p;
	int len;

	node = add_mi_node_child(parent_node, 0, "sst", 3, NULL, 0);
	if (node==NULL) {
		LM_ERR("oom\n");
		return;
	}

	p = int2str((unsigned long)(sst_info->requester), &len);
	attr = add_mi_attr(node, MI_DUP_VALUE, "requester_flags", 15, p, len);
	if(attr == NULL) {
		LM_ERR("oom requester_flags\n");
		return;
	}

	p = int2str((unsigned long)(sst_info->supported), &len);
	attr = add_mi_attr(node, MI_DUP_VALUE, "supported_flags", 15, p, len);
	if(attr == NULL) {
		LM_ERR("oom supported_flags\n");
		return;
	}

	p = int2str((unsigned long)(sst_info->interval), &len);
	attr = add_mi_attr(node, MI_DUP_VALUE, "interval", 8, p, len);
	if(attr == NULL) {
		LM_ERR("oom interval\n");
		return;
	}

	return;
}
