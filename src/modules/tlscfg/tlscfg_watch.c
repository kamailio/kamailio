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
 * @file tlscfg_watch.c
 * @brief Cert file watcher, debounce flusher, and tls.reload trigger
 * @ingroup tlscfg
 */

#include "../../core/dprint.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"

#include "tlscfg_config.h"
#include "tlscfg_profile.h"
#include "tlscfg_watch.h"

#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

/* implemented in tlscfg_mod.c */
extern str *tlscfg_get_tls_cfg_path(void);
extern int tlscfg_get_cert_check_interval(void);
extern int tlscfg_get_debounce_interval(void);

/* cached tls.reload rpc handler */
static rpc_export_t *_tls_reload_rpc = NULL;

/* ---- internal RPC context for calling tls.reload programmatically ---- */

static void internal_rpc_fault(void *ctx, int code, char *fmt, ...)
{
	va_list ap;
	char buf[256];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	LM_ERR("tls.reload error [%d]: %s\n", code, buf);
	*(int *)ctx = -1;
}

static int internal_rpc_noop_v(void *ctx, char *fmt, ...)
{
	return 0;
}

static int internal_rpc_send(void *ctx)
{
	return 0;
}

static int internal_rpc_rpl_printf(void *ctx, char *fmt, ...)
{
	va_list ap;
	char buf[256];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	LM_INFO("tls.reload: %s\n", buf);
	return 0;
}

/* clang-format off */
static rpc_t _internal_rpc = {
	internal_rpc_fault,
	internal_rpc_send,
	(rpc_add_f)internal_rpc_noop_v,
	(rpc_scan_f)internal_rpc_noop_v,
	internal_rpc_rpl_printf,
	(rpc_struct_add_f)internal_rpc_noop_v,
	(rpc_array_add_f)internal_rpc_noop_v,
	(rpc_struct_scan_f)internal_rpc_noop_v,
	(rpc_struct_printf_f)internal_rpc_noop_v,
	NULL,
	NULL,
	NULL
};
/* clang-format on */

int tlscfg_tls_reload(void)
{
	int result = 0;

	if(!_tls_reload_rpc) {
		_tls_reload_rpc = rpc_lookup("tls.reload", 10);
		if(!_tls_reload_rpc) {
			LM_ERR("tls.reload RPC not found — is the tls module loaded?\n");
			return -1;
		}
	}

	_tls_reload_rpc->function(&_internal_rpc, &result);
	return result;
}

/* ---- debounce flush ---- */

static void debounce_flush(void)
{
	tlscfg_data_t *data;
	str *cfg_path;
	int do_flush = 0;
	time_t now;

	data = tlscfg_data_get();
	if(!data)
		return;

	cfg_path = tlscfg_get_tls_cfg_path();
	if(!cfg_path || !cfg_path->s)
		return;

	now = time(NULL);

	lock_get(data->lock);
	if(data->dirty
			&& (now - data->last_mutation) >= tlscfg_get_debounce_interval()) {
		do_flush = 1;
	}
	lock_release(data->lock);

	if(do_flush) {
		LM_INFO("debounce: flushing config and triggering tls.reload\n");
		lock_get(data->lock);
		tlscfg_config_write(cfg_path);
		lock_release(data->lock);
		tlscfg_tls_reload();
	}
}

/* ---- cert mtime check ---- */

/* snapshot entry for lock-free stat */
#define CERT_SNAP_MAX 128

typedef struct cert_snap
{
	char path[512];
	char prof_id[256];
	time_t old_mtime;
	time_t new_mtime;
	int changed;
	int is_cert2; /* 0 = certificate, 1 = certificate2 */
} cert_snap_t;

/**
 * @brief Helper: add a snap entry for a cert key if it exists on the profile
 * @return 1 if entry added, 0 otherwise
 */
static int snap_add_cert(cert_snap_t *snaps, int *n, tlscfg_profile_t *p,
		str *key, time_t old_mtime, int is_cert2)
{
	tlscfg_kv_t *kv;

	kv = tlscfg_kv_find(p, key);
	if(!kv || !kv->value.s || kv->value.len == 0)
		return 0;
	if(kv->value.len >= (int)sizeof(snaps[0].path))
		return 0;

	memcpy(snaps[*n].path, kv->value.s, kv->value.len);
	snaps[*n].path[kv->value.len] = '\0';

	if(p->profile_id.len < (int)sizeof(snaps[*n].prof_id)) {
		memcpy(snaps[*n].prof_id, p->profile_id.s, p->profile_id.len);
		snaps[*n].prof_id[p->profile_id.len] = '\0';
	} else {
		snaps[*n].prof_id[0] = '\0';
	}

	snaps[*n].old_mtime = old_mtime;
	snaps[*n].new_mtime = 0;
	snaps[*n].changed = 0;
	snaps[*n].is_cert2 = is_cert2;
	(*n)++;
	return 1;
}

static void cert_mtime_check(void)
{
	tlscfg_data_t *data;
	tlscfg_profile_t *p;
	struct stat st;
	int changed = 0;
	int i, n = 0;
	cert_snap_t snaps[CERT_SNAP_MAX];
	str cert_key = STR_STATIC_INIT("certificate");
	str cert2_key = STR_STATIC_INIT("certificate2");

	data = tlscfg_data_get();
	if(!data)
		return;

	/* phase 1: snapshot cert paths under lock */
	lock_get(data->lock);

	for(p = data->profiles; p && n < CERT_SNAP_MAX - 1; p = p->next) {
		if(!p->enabled)
			continue;

		snap_add_cert(snaps, &n, p, &cert_key, p->cert_mtime, 0);
		if(n < CERT_SNAP_MAX)
			snap_add_cert(snaps, &n, p, &cert2_key, p->cert2_mtime, 1);
	}

	lock_release(data->lock);

	/* phase 2: stat outside the lock (safe on network storage) */
	for(i = 0; i < n; i++) {
		if(stat(snaps[i].path, &st) == 0) {
			snaps[i].new_mtime = st.st_mtime;
			if(snaps[i].old_mtime != 0 && st.st_mtime != snaps[i].old_mtime) {
				LM_INFO("cert file changed for profile '%s': %s\n",
						snaps[i].prof_id, snaps[i].path);
				snaps[i].changed = 1;
				changed = 1;
			}
		}
	}

	/* phase 3: write back updated mtimes under lock */
	if(n > 0) {
		lock_get(data->lock);

		for(i = 0; i < n; i++) {
			if(snaps[i].new_mtime == 0)
				continue;
			for(p = data->profiles; p; p = p->next) {
				if(p->profile_id.len == (int)strlen(snaps[i].prof_id)
						&& strncmp(p->profile_id.s, snaps[i].prof_id,
								   p->profile_id.len)
								   == 0) {
					if(snaps[i].is_cert2)
						p->cert2_mtime = snaps[i].new_mtime;
					else
						p->cert_mtime = snaps[i].new_mtime;
					break;
				}
			}
		}

		lock_release(data->lock);
	}

	if(changed) {
		LM_INFO("cert file changes detected, triggering tls.reload\n");
		tlscfg_tls_reload();
	}
}

/* ---- refresh cert mtimes ---- */

void tlscfg_refresh_cert_mtimes(void)
{
	tlscfg_data_t *data;
	tlscfg_profile_t *p;
	struct stat st;
	int i, n = 0;
	cert_snap_t snaps[CERT_SNAP_MAX];
	str cert_key = STR_STATIC_INIT("certificate");
	str cert2_key = STR_STATIC_INIT("certificate2");

	data = tlscfg_data_get();
	if(!data)
		return;

	/* phase 1: snapshot paths under lock */
	lock_get(data->lock);

	for(p = data->profiles; p && n < CERT_SNAP_MAX - 1; p = p->next) {
		if(!p->enabled)
			continue;

		snap_add_cert(snaps, &n, p, &cert_key, 0, 0);
		if(n < CERT_SNAP_MAX)
			snap_add_cert(snaps, &n, p, &cert2_key, 0, 1);
	}

	lock_release(data->lock);

	/* phase 2: stat outside the lock */
	for(i = 0; i < n; i++) {
		if(stat(snaps[i].path, &st) == 0) {
			snaps[i].new_mtime = st.st_mtime;
		}
	}

	/* phase 3: write back under lock */
	if(n > 0) {
		lock_get(data->lock);

		for(i = 0; i < n; i++) {
			if(snaps[i].new_mtime == 0)
				continue;
			for(p = data->profiles; p; p = p->next) {
				if(p->profile_id.len == (int)strlen(snaps[i].prof_id)
						&& strncmp(p->profile_id.s, snaps[i].prof_id,
								   p->profile_id.len)
								   == 0) {
					if(snaps[i].is_cert2)
						p->cert2_mtime = snaps[i].new_mtime;
					else
						p->cert_mtime = snaps[i].new_mtime;
					break;
				}
			}
		}

		lock_release(data->lock);
	}
}

/* ---- timer callback ---- */

void tlscfg_watch_timer(unsigned int ticks, void *param)
{
	int check_interval;

	/* debounce flush every tick */
	debounce_flush();

	/* cert mtime check at configured interval */
	check_interval = tlscfg_get_cert_check_interval();
	if(check_interval > 0 && (ticks % (unsigned int)check_interval) == 0) {
		cert_mtime_check();
	}
}
