/**
 * Copyright (C) 2017 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
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
 * \brief Kamailio topos ::
 * \ingroup topos
 * Module: \ref topos
 */

#ifndef _TOPOS_API_H_
#define _TOPOS_API_H_

#include "tps_storage.h"

typedef int (*tps_insert_dialog_f)(tps_data_t *td);
typedef int (*tps_clean_dialogs_f)(void);
typedef int (*tps_insert_branch_f)(tps_data_t *td);
typedef int (*tps_clean_branches_f)(void);
typedef int (*tps_load_branch_f)(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd);
typedef int (*tps_load_dialog_f)(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd);
typedef int (*tps_update_dialog_f)(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd);
typedef int (*tps_end_dialog_f)(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd);

typedef struct tps_storage_api {
	tps_insert_dialog_f insert_dialog;
	tps_clean_dialogs_f clean_dialogs;
	tps_insert_branch_f insert_branch;
	tps_clean_branches_f clean_branches;
	tps_load_branch_f load_branch;
	tps_load_dialog_f load_dialog;
	tps_update_dialog_f update_dialog;
	tps_end_dialog_f end_dialog;
} tps_storage_api_t;

#endif
