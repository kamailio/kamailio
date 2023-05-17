/**
 * Copyright (C) 2016 Daniel-Constantin Mierla (asipto.com)
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

#ifndef _TOPOS_STORAGE_H_
#define _TOPOS_STORAGE_H_

#include "../../core/parser/msg_parser.h"

#define TPS_DIR_DOWNSTREAM 0
#define TPS_DIR_UPSTREAM 1

#define TPS_IFLAG_INIT 1
#define TPS_IFLAG_DLGON 2

#define TPS_DBU_CONTACT (1 << 0)
#define TPS_DBU_RPLATTRS (1 << 1)
#define TPS_DBU_ARR (1 << 2)
#define TPS_DBU_BRR (1 << 3)
#define TPS_DBU_TIME (1 << 4)
#define TPS_DBU_ALL (0xffffffff)

#define TPS_DATA_SIZE 8192
typedef struct tps_data
{
	char cbuf[TPS_DATA_SIZE];
	char *cp;
	str a_uuid;
	str b_uuid;
	str a_callid;
	str a_rr;
	str b_rr;
	str s_rr;
	str a_contact;
	str b_contact;
	str as_contact;
	str bs_contact;
	str a_tag;
	str b_tag;
	str a_uri;
	str b_uri;
	str r_uri;
	str a_srcaddr;
	str b_srcaddr;
	str a_socket;
	str b_socket;
	str x_via1;
	str x_via2;
	str x_vbranch1;
	str x_via;
	str x_tag;
	str x_rr;
	str y_rr;
	str x_uri;
	str s_method;
	str s_cseq;
	str x_context;
	int32_t iflags;
	int32_t direction;
	uint32_t s_method_id;
	int32_t expires;
} tps_data_t;

int tps_storage_dialog_find(sip_msg_t *msg, tps_data_t *td);
int tps_storage_dialog_save(sip_msg_t *msg, tps_data_t *td);
int tps_storage_dialog_rm(sip_msg_t *msg, tps_data_t *td);

int tps_storage_branch_find(sip_msg_t *msg, tps_data_t *td);
int tps_storage_branch_save(sip_msg_t *msg, tps_data_t *td);
int tps_storage_branch_rm(sip_msg_t *msg, tps_data_t *td);

int tps_storage_record(sip_msg_t *msg, tps_data_t *td, int dialog, int dir);
int tps_storage_load_branch(
		sip_msg_t *msg, tps_data_t *md, tps_data_t *sd, uint32_t mode);
int tps_storage_update_branch(
		sip_msg_t *msg, tps_data_t *md, tps_data_t *sd, uint32_t mode);
int tps_storage_load_dialog(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd);
int tps_storage_update_dialog(
		sip_msg_t *msg, tps_data_t *md, tps_data_t *sd, uint32_t mode);
int tps_storage_end_dialog(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd);

int tps_storage_lock_set_init(void);
int tps_storage_lock_get(str *lkey);
int tps_storage_lock_release(str *lkey);
int tps_storage_lock_set_destroy(void);

int tps_storage_link_msg(sip_msg_t *msg, tps_data_t *td, int dir);

void tps_storage_clean(unsigned int ticks, void *param);

#endif
