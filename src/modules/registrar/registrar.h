/*
 * registrar module interface
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * \brief SIP registrar module - interface
 * \ingroup registrar
 */


#ifndef REG_MOD_H
#define REG_MOD_H

#include "../../core/parser/msg_parser.h"
#include "../../core/qvalue.h"
#include "../../core/usr_avp.h"
#include "../usrloc/usrloc.h"
#include "../../modules/sl/sl.h"

/* if DB support is used, this values must not exceed the
 * storage capacity of the DB columns! See db/schema/entities.xml */
extern int contact_max_size; /* configurable using module parameter "contact_max_size" instead of compile time constant */
#define RECEIVED_MAX_SIZE      255
#define USERNAME_MAX_SIZE      64
#define DOMAIN_MAX_SIZE        128
#define CALLID_MAX_SIZE        255

#define PATH_MODE_STRICT	2
#define PATH_MODE_LAZY		1
#define PATH_MODE_OFF		0

#define REG_SAVE_MEM_FL     	(1<<0) /* save in memory only */
#define REG_SAVE_NORPL_FL   	(1<<1) /* do not send reply internally */
#define REG_SAVE_REPL_FL    	(1<<2) /* replace contacts - one contact per user */
#define REG_SAVE_NOVARIATION_FL (1<<3) /* do not apply expire range */
#define REG_SAVE_PREPRPL_FL   	(1<<4) /* prepare headers for reply */
#define REG_SAVE_ALL_FL     ((1<<5)-1)

#define REG_OUTBOUND_NONE	0
#define REG_OUTBOUND_SUPPORTED	1
#define REG_OUTBOUND_REQUIRE	2

#define REG_REGID_OUTBOUND      0
#define REG_REGID_ALWAYS        1

/* Maximum of 999 to keep flow-timer to 3 digits
 * - make sure to update reply.c:add_flow_timer() if the number of digits
 *   increases! */
#define REG_FLOW_TIMER_MAX	999

#define AVP_RCD_RUID	1
#define AVP_RCD_CNT		2
#define AVP_RCD_EXP		4
#define AVP_RCD_RCV		8
#define AVP_RCD_PATH	16

extern int nat_flag;
extern int tcp_persistent_flag;
extern int received_avp;
extern int reg_use_domain;
extern float def_q;

extern unsigned short rcv_avp_type;
extern int_str rcv_avp_name;

extern str match_callid_name;
extern str match_received_name;
extern str match_contact_name;

extern str rcv_param;
extern int method_filtering;
extern int path_enabled;
extern int path_mode;
extern int path_use_params;
extern int path_check_local;
extern int reg_gruu_enabled;
extern int reg_outbound_mode;
extern int reg_regid_mode;
extern int reg_flow_timer;

extern str sock_hdr_name;
extern int sock_flag;
extern int reg_sock_mode;

extern str reg_xavp_cfg;
extern str reg_xavp_rcd;
extern int reg_xavp_rcd_mask;
extern usrloc_api_t ul;/*!< Structure containing pointers to usrloc functions*/

extern sl_api_t slb;

extern int reg_expire_event_rt;
extern int reg_min_expires_mode;

extern stat_var *accepted_registrations;
extern stat_var *rejected_registrations;
extern stat_var *default_expire_stat;
extern stat_var *max_expires_stat;

#endif /* REG_MOD_H */
