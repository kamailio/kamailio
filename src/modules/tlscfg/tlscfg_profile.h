/*
 * tlscfg module - TLS profile management companion for the tls module
 *
 * Copyright (C) 2026 Aurora Innovation
 *
 * Author: Daniel Donoghue
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
 */

/**
 * @file tlscfg_profile.h
 * @brief In-memory TLS profile index declarations
 * @ingroup tlscfg
 */

#ifndef _TLSCFG_PROFILE_H_
#define _TLSCFG_PROFILE_H_

#include "../../core/str.h"
#include "../../core/locking.h"

#include <time.h>

#define TLSCFG_TYPE_SERVER 0
#define TLSCFG_TYPE_CLIENT 1

/**
 * @brief Key-value pair for arbitrary tls.cfg directives (shm allocated)
 */
typedef struct tlscfg_kv
{
	str key;
	str value;
	struct tlscfg_kv *next;
} tlscfg_kv_t;

/**
 * @brief In-memory representation of a single TLS profile (shm allocated)
 */
typedef struct tlscfg_profile
{
	str profile_id;
	str section_header;
	int profile_type;
	int enabled;
	time_t cert_mtime;
	time_t cert2_mtime;
	time_t cert_expiry;
	tlscfg_kv_t *kvs;
	struct tlscfg_profile *next;
} tlscfg_profile_t;

/**
 * @brief Shared data — lives in shared memory, accessed from all processes
 */
typedef struct tlscfg_data
{
	gen_lock_t *lock;
	tlscfg_profile_t *profiles;
	int dirty;
	time_t last_mutation;
	time_t config_mtime;
} tlscfg_data_t;

int tlscfg_data_init(void);
void tlscfg_data_destroy(void);
tlscfg_data_t *tlscfg_data_get(void);

/* All profile functions below require the caller to hold data->lock */
tlscfg_profile_t *tlscfg_profile_find(str *profile_id);
int tlscfg_profile_add(str *profile_id, str *section_header);
int tlscfg_profile_remove(str *profile_id);
int tlscfg_profile_set(str *profile_id, str *key, str *value);
int tlscfg_profile_unset(str *profile_id, str *key);
int tlscfg_profile_set_enabled(str *profile_id, int enabled);
int tlscfg_profile_set_id(str *old_id, str *new_id);
void tlscfg_profile_clear(void);

tlscfg_kv_t *tlscfg_kv_find(tlscfg_profile_t *p, str *key);

#endif /* _TLSCFG_PROFILE_H_ */
