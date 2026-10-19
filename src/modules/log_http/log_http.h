/**
 * Copyright (C) 2019-2026 Alexey Volokitin (didww.com)
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

#ifndef _LOG_HTTP_H_
#define _LOG_HTTP_H_

#include "../../core/list.h"
#include "../../core/str.h"
#include "../../core/sr_module.h"
#include "../../core/counters.h"

/* one log server endpoint (modparam "target") */
struct lh_target
{
	SLIST_ENTRY(lh_target) next;
	str url;
};

typedef SLIST_HEAD(, lh_target) lh_target_list_t;

#define LH_DEFAULT_START_TYPE 0x1000
#define LH_STANDARD_START_TYPE 0x1

typedef enum
{
	LPT_NONE,

	/* types allowed in "map" - values come from http_log() arguments */
	LPT_STRING = LH_STANDARD_START_TYPE,
	LPT_NUMBER,
	LPT_IPADDR,
	LPT_BOOL,
	LPT_DATE,
	LPT_FLOAT,

	/* types allowed in "default_map" - values are taken automatically */
	LPDT_ROUTENAME = LH_DEFAULT_START_TYPE,
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

extern group_key_t lh_log_grp;
extern lh_target_list_t lh_targets;
extern int lh_timeout;
extern int lh_chunked;
extern unsigned int lh_batch_size;
extern unsigned int lh_batch_delay;
extern unsigned int lh_queue_size;
extern str lh_auth_usrpwd;

extern stat_var *lh_stat_queue_size;
extern stat_var *lh_stat_drop_total;
extern stat_var *lh_stat_err_total;

struct lh_target *lh_get_next_target(void);
int lh_get_param_type(str *type);

#endif
