/**
 * Copyright (C) 2024 kamailio.org
 * Copyright (C) 2024 net2phone.com
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

#ifndef _TOPOS_HTABLE_STORAGE_H_
#define _TOPOS_HTABLE_STORAGE_H_

#include "../topos/api.h"
#include "../htable/ht_api.h"
#include "../htable/api.h"

#define TPS_HTABLE_SIZE_KEY 1024
#define TPS_HTABLE_SIZE_VAL 8192
#define TPS_HTABLE_SIZE_SPEC 128
#define TPS_BASE64_ROWS 32
#define TPS_BASE64_SIZE 1024

#define TPS_HTABLE_DATA_APPEND(_sd, _k, _v, _r)                        \
	do {                                                               \
		if((_sd)->cp + strlen(_v) >= (_sd)->cbuf + TPS_DATA_SIZE) {    \
			LM_ERR("not enough space for %.*s\n", (_k)->len, (_k)->s); \
			return -1;                                                 \
		}                                                              \
		if((_v)) {                                                     \
			(_r)->s = (_sd)->cp;                                       \
			(_r)->len = strlen(_v);                                    \
			memcpy((_sd)->cp, (_v), strlen(_v));                       \
			(_sd)->cp += strlen(_v);                                   \
			(_sd)->cp[0] = '\0';                                       \
			(_sd)->cp++;                                               \
		}                                                              \
	} while(0)


int tps_htable_insert_dialog(tps_data_t *td);
int tps_htable_clean_dialogs(void);
int tps_htable_insert_branch(tps_data_t *td);
int tps_htable_clean_branches(void);
int tps_htable_load_branch(
		sip_msg_t *msg, tps_data_t *md, tps_data_t *sd, uint32_t mode);
int tps_htable_load_dialog(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd);
int tps_htable_update_branch(
		sip_msg_t *msg, tps_data_t *md, tps_data_t *sd, uint32_t mode);
int tps_htable_update_dialog(
		sip_msg_t *msg, tps_data_t *md, tps_data_t *sd, uint32_t mode);
int tps_htable_end_dialog(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd);

#endif
