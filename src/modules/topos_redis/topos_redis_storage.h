/**
 * Copyright (C) 2017 kamailio.org
 * Copyright (C) 2017 flowroute.com
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

#ifndef _TOPOS_REDIS_STORAGE_H_
#define _TOPOS_REDIS_STORAGE_H_

#include "../topos/api.h"

int tps_redis_insert_dialog(tps_data_t *td);
int tps_redis_clean_dialogs(void);
int tps_redis_insert_branch(tps_data_t *td);
int tps_redis_clean_branches(void);
int tps_redis_load_branch(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd,
		uint32_t mode);
int tps_redis_load_dialog(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd);
int tps_redis_update_branch(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd,
		uint32_t mode);
int tps_redis_update_dialog(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd,
		uint32_t mode);
int tps_redis_end_dialog(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd);

#endif
