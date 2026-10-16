/**
 * Copyright (C) 2019-2026 Alexey Volokitin (didww.com)
 * Copyright (C) 2025-2026 Michael Furmur (didww.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _LOG_VICTORIALOGS_H_
#define _LOG_VICTORIALOGS_H_

#include "../../core/list.h"
#include "../../core/str.h"
#include "../../core/sr_module.h"
#include "../../core/counters.h"

/* one VictoriaLogs ingestion endpoint (modparam "target") */
struct vl_target
{
	SLIST_ENTRY(vl_target) next;
	str url;
};

typedef SLIST_HEAD(, vl_target) vl_target_list_t;

#define VL_DEFAULT_START_TYPE 0x1000
#define VL_STANDARD_START_TYPE 0x1

typedef enum
{
	LPT_NONE,

	/* types allowed in "map" - values come from vl_log() arguments */
	LPT_STRING = VL_STANDARD_START_TYPE,
	LPT_NUMBER,
	LPT_IPADDR,
	LPT_BOOL,
	LPT_DATE,
	LPT_FLOAT,

	/* types allowed in "default_map" - values are taken automatically */
	LPDT_ROUTENAME = VL_DEFAULT_START_TYPE,
	LPDT_TIMESTAMP,
	LPDT_PLACEHOLDER,
	LPDT_IMMEDIATE
} log_param_type_t;

struct log_grp_param
{
	SLIST_ENTRY(log_grp_param) next;
	str name;
	log_param_type_t type;
	str stype;
};

typedef struct
{
	SLIST_HEAD(, log_grp_param) head;
	int size;
} log_grp_param_t;

typedef struct
{
	log_grp_param_t default_params;
	log_grp_param_t params;
} log_grp_entry_t;

struct group_key
{
	SLIST_ENTRY(group_key) next;
	str group_key;
	log_grp_entry_t params;
};

typedef SLIST_HEAD(, group_key) group_key_t;

extern group_key_t vl_log_grp;
extern vl_target_list_t vl_targets;
extern str vl_stream_fields;
extern str vl_msg_field;
extern str vl_time_field;
extern str vl_extra_fields;
extern int vl_account_id;
extern int vl_project_id;
extern int vl_timeout;
extern unsigned int vl_batch_size;
extern unsigned int vl_batch_delay;
extern unsigned int vl_queue_size;
extern str vl_auth_usrpwd;

extern stat_var *vl_stat_queue_size;
extern stat_var *vl_stat_drop_total;
extern stat_var *vl_stat_err_total;

struct vl_target *vl_get_next_target(void);
int vl_get_param_type(str *type);

#endif
