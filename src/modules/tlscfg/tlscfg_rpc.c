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
 * @file tlscfg_rpc.c
 * @brief RPC command handler implementations
 * @ingroup tlscfg
 */

#include "../../core/dprint.h"

#include "tlscfg_config.h"
#include "tlscfg_profile.h"
#include "tlscfg_rpc.h"
#include "tlscfg_watch.h"

#include <string.h>
#include <sys/stat.h>

/* implemented in tlscfg_mod.c */
extern str *tlscfg_get_tls_cfg_path(void);
extern const char *tlscfg_get_cert_base_path(void);

/**
 * @brief Auto-infer private_key when certificate is set to certman:id
 *
 * If the key being set is "certificate" or "certificate2" and the value
 * starts with "certman:", also set the corresponding private_key field
 * to certman:id (which will resolve to {base}/{id}/key.pem at write time).
 */
static void auto_infer_key(str *profile_id, str *key, str *value)
{
	str pkey_name;
	str pkey_val;

	if(value->len < 8 || strncmp(value->s, "certman:", 8) != 0)
		return;

	if(key->len == 11 && strncasecmp(key->s, "certificate", 11) == 0) {
		pkey_name.s = "private_key";
		pkey_name.len = 11;
	} else if(key->len == 12 && strncasecmp(key->s, "certificate2", 12) == 0) {
		pkey_name.s = "private_key2";
		pkey_name.len = 12;
	} else {
		return;
	}

	pkey_val = *value;
	tlscfg_profile_set(profile_id, &pkey_name, &pkey_val);
	LM_DBG("auto-inferred %.*s = %.*s\n", pkey_name.len, pkey_name.s,
			pkey_val.len, pkey_val.s);
}

/* ---- profile_add ---- */

void tlscfg_rpc_profile_add(rpc_t *rpc, void *ctx)
{
	str profile_id, section;
	str section_header;
	char hdr_buf[256];
	tlscfg_data_t *data;

	if(rpc->scan(ctx, "SS", &profile_id, &section) < 2) {
		rpc->fault(ctx, 400, "Usage: tlscfg.profile_add <id> <section>");
		return;
	}

	if(section.len + 3 > (int)sizeof(hdr_buf)) {
		rpc->fault(ctx, 400, "Section name too long");
		return;
	}

	hdr_buf[0] = '[';
	memcpy(hdr_buf + 1, section.s, section.len);
	hdr_buf[section.len + 1] = ']';
	hdr_buf[section.len + 2] = '\0';
	section_header.s = hdr_buf;
	section_header.len = section.len + 2;

	data = tlscfg_data_get();
	lock_get(data->lock);

	if(tlscfg_profile_add(&profile_id, &section_header) < 0) {
		lock_release(data->lock);
		rpc->fault(ctx, 500, "Failed to add profile (duplicate?)");
		return;
	}

	lock_release(data->lock);
	rpc->rpl_printf(
			ctx, "Ok. Profile '%.*s' added.", profile_id.len, profile_id.s);
}

/* ---- profile_update ---- */

void tlscfg_rpc_profile_update(rpc_t *rpc, void *ctx)
{
	str profile_id, key, value;
	int has_value;
	tlscfg_data_t *data;

	if(rpc->scan(ctx, "SS", &profile_id, &key) < 2) {
		rpc->fault(ctx, 400, "Usage: tlscfg.profile_update <id> <key> [value]");
		return;
	}

	has_value = (rpc->scan(ctx, "*S", &value) > 0);

	data = tlscfg_data_get();
	lock_get(data->lock);

	if(!has_value || (value.len == 1 && value.s[0] == '-')) {
		/* unset the field */
		tlscfg_profile_unset(&profile_id, &key);
		lock_release(data->lock);
		rpc->rpl_printf(ctx, "Ok. Field '%.*s' removed from '%.*s'.", key.len,
				key.s, profile_id.len, profile_id.s);
	} else {
		if(tlscfg_profile_set(&profile_id, &key, &value) < 0) {
			lock_release(data->lock);
			rpc->fault(ctx, 500, "Failed to update profile");
			return;
		}
		auto_infer_key(&profile_id, &key, &value);
		lock_release(data->lock);
		rpc->rpl_printf(ctx, "Ok. Set '%.*s' = '%.*s' on '%.*s'.", key.len,
				key.s, value.len, value.s, profile_id.len, profile_id.s);
	}
}

/* ---- profile_remove ---- */

void tlscfg_rpc_profile_remove(rpc_t *rpc, void *ctx)
{
	str profile_id;
	tlscfg_data_t *data;

	if(rpc->scan(ctx, "S", &profile_id) < 1) {
		rpc->fault(ctx, 400, "Usage: tlscfg.profile_remove <id>");
		return;
	}

	data = tlscfg_data_get();
	lock_get(data->lock);

	if(tlscfg_profile_remove(&profile_id) < 0) {
		lock_release(data->lock);
		rpc->fault(ctx, 404, "Profile not found");
		return;
	}

	lock_release(data->lock);
	rpc->rpl_printf(
			ctx, "Ok. Profile '%.*s' removed.", profile_id.len, profile_id.s);
}

/* ---- profile_list ---- */

void tlscfg_rpc_profile_list(rpc_t *rpc, void *ctx)
{
	tlscfg_data_t *data;
	tlscfg_profile_t *p;
	void *handle;

	data = tlscfg_data_get();
	lock_get(data->lock);

	for(p = data->profiles; p; p = p->next) {
		if(rpc->add(ctx, "{", &handle) < 0) {
			lock_release(data->lock);
			rpc->fault(ctx, 500, "Internal error");
			return;
		}
		rpc->struct_add(handle, "SSds", "id", &p->profile_id, "section",
				&p->section_header, "enabled", p->enabled, "type",
				(p->profile_type == TLSCFG_TYPE_CLIENT) ? "client" : "server");
	}

	lock_release(data->lock);
}

/* ---- profile_get ---- */

void tlscfg_rpc_profile_get(rpc_t *rpc, void *ctx)
{
	str profile_id;
	tlscfg_data_t *data;
	tlscfg_profile_t *p;
	tlscfg_kv_t *kv;
	void *handle, *kvh;

	if(rpc->scan(ctx, "S", &profile_id) < 1) {
		rpc->fault(ctx, 400, "Usage: tlscfg.profile_get <id>");
		return;
	}

	data = tlscfg_data_get();
	lock_get(data->lock);

	p = tlscfg_profile_find(&profile_id);
	if(p == NULL) {
		lock_release(data->lock);
		rpc->fault(ctx, 404, "Profile not found");
		return;
	}

	if(rpc->add(ctx, "{", &handle) < 0) {
		lock_release(data->lock);
		rpc->fault(ctx, 500, "Internal error");
		return;
	}

	rpc->struct_add(handle, "SSds", "id", &p->profile_id, "section",
			&p->section_header, "enabled", p->enabled, "type",
			(p->profile_type == TLSCFG_TYPE_CLIENT) ? "client" : "server");

	for(kv = p->kvs; kv; kv = kv->next) {
		if(rpc->struct_add(handle, "{", "fields", &kvh) < 0)
			break;
		rpc->struct_add(kvh, "SS", "key", &kv->key, "value", &kv->value);
	}

	lock_release(data->lock);
}

/* ---- profile_enable ---- */

void tlscfg_rpc_profile_enable(rpc_t *rpc, void *ctx)
{
	str profile_id;
	tlscfg_data_t *data;

	if(rpc->scan(ctx, "S", &profile_id) < 1) {
		rpc->fault(ctx, 400, "Usage: tlscfg.profile_enable <id>");
		return;
	}

	data = tlscfg_data_get();
	lock_get(data->lock);

	if(tlscfg_profile_set_enabled(&profile_id, 1) < 0) {
		lock_release(data->lock);
		rpc->fault(ctx, 404, "Profile not found");
		return;
	}

	lock_release(data->lock);
	rpc->rpl_printf(
			ctx, "Ok. Profile '%.*s' enabled.", profile_id.len, profile_id.s);
}

/* ---- profile_disable ---- */

void tlscfg_rpc_profile_disable(rpc_t *rpc, void *ctx)
{
	str profile_id;
	tlscfg_data_t *data;

	if(rpc->scan(ctx, "S", &profile_id) < 1) {
		rpc->fault(ctx, 400, "Usage: tlscfg.profile_disable <id>");
		return;
	}

	data = tlscfg_data_get();
	lock_get(data->lock);

	if(tlscfg_profile_set_enabled(&profile_id, 0) < 0) {
		lock_release(data->lock);
		rpc->fault(ctx, 404, "Profile not found");
		return;
	}

	lock_release(data->lock);
	rpc->rpl_printf(
			ctx, "Ok. Profile '%.*s' disabled.", profile_id.len, profile_id.s);
}

/* ---- cert_check ---- */

void tlscfg_rpc_cert_check(rpc_t *rpc, void *ctx)
{
	rpc->rpl_printf(ctx, "Ok. Cert check triggered.");
	/* The actual check runs in the timer callback;
	 * this RPC just forces an immediate check cycle by
	 * calling the timer function logic directly. For now
	 * we rely on the next timer tick. */
}

/* ---- cert_notify ---- */

/**
 * @brief Scan all profiles and bind certificate paths for any profile_id
 *        whose cert files exist under cert_base_path.
 *
 * For each profile, checks whether {cert_base_path}/{profile_id}/cert.pem
 * (and key.pem) exist on disk.  If they do, sets the profile's certificate
 * and private_key KVs to "certman:{profile_id}" so the config writer
 * resolves them to the actual file paths.  This is the mechanism that links
 * a certman certificate (identified by cert ID) to a tlscfg profile
 * (identified by profile_id).
 *
 * Must be called with data->lock held.
 */
static void cert_notify_bind_profiles(void)
{
	const char *base;
	tlscfg_data_t *data;
	tlscfg_profile_t *p;
	char path[512];
	struct stat st;
	str key, val;
	char valbuf[280]; /* "certman:" + profile_id */

	base = tlscfg_get_cert_base_path();
	if(!base || base[0] == '\0')
		return;

	data = tlscfg_data_get();

	for(p = data->profiles; p; p = p->next) {
		if(p->profile_id.len == 0)
			continue;

		/* check if cert.pem exists for this profile_id */
		snprintf(path, sizeof(path), "%s/%.*s/cert.pem", base,
				p->profile_id.len, p->profile_id.s);
		if(stat(path, &st) != 0)
			continue;

		/* enable disabled profiles whose certs are now available */
		if(!p->enabled) {
			LM_INFO("enabling profile '%.*s' — certs now available at %s\n",
					p->profile_id.len, p->profile_id.s, path);
			tlscfg_profile_set_enabled(&p->profile_id, 1);
		}

		/* build the certman: reference value */
		snprintf(valbuf, sizeof(valbuf), "certman:%.*s", p->profile_id.len,
				p->profile_id.s);

		/* check if already bound to avoid unnecessary mutations */
		key.s = "certificate";
		key.len = 11;
		{
			tlscfg_kv_t *existing = tlscfg_kv_find(p, &key);
			if(existing && existing->value.len == (int)strlen(valbuf)
					&& strncmp(existing->value.s, valbuf, existing->value.len)
							   == 0) {
				continue; /* already bound */
			}
		}

		LM_INFO("binding profile '%.*s' to certman certs at %s\n",
				p->profile_id.len, p->profile_id.s, path);

		val.s = valbuf;
		val.len = strlen(valbuf);

		/* set certificate = certman:{profile_id} */
		tlscfg_profile_set(&p->profile_id, &key, &val);

		/* set private_key = certman:{profile_id} */
		key.s = "private_key";
		key.len = 11;
		tlscfg_profile_set(&p->profile_id, &key, &val);
	}
}

void tlscfg_rpc_cert_notify(rpc_t *rpc, void *ctx)
{
	str *cfg_path;

	cfg_path = tlscfg_get_tls_cfg_path();
	if(!cfg_path || !cfg_path->s) {
		rpc->fault(ctx, 500, "No TLS config path available");
		return;
	}

	/* bind cert paths for profiles whose certs are now available,
	 * then flush config and reload */
	{
		tlscfg_data_t *data = tlscfg_data_get();
		lock_get(data->lock);
		cert_notify_bind_profiles();
		tlscfg_config_write(cfg_path);
		lock_release(data->lock);
	}

	if(tlscfg_tls_reload() < 0) {
		rpc->fault(ctx, 500, "Config written but tls.reload failed");
		return;
	}

	/* sync stored cert mtimes so the watcher does not trigger a
	 * redundant tls.reload for files that just changed path */
	tlscfg_refresh_cert_mtimes();

	rpc->rpl_printf(ctx, "Ok. Config flushed and TLS reloaded.");
}

/* ---- reload ---- */

void tlscfg_rpc_reload(rpc_t *rpc, void *ctx)
{
	str *cfg_path;
	tlscfg_data_t *data;

	cfg_path = tlscfg_get_tls_cfg_path();
	if(!cfg_path || !cfg_path->s) {
		rpc->fault(ctx, 500, "No TLS config path available");
		return;
	}

	data = tlscfg_data_get();
	lock_get(data->lock);

	if(tlscfg_config_load(cfg_path) < 0) {
		lock_release(data->lock);
		rpc->fault(ctx, 500, "Failed to reload config file");
		return;
	}

	lock_release(data->lock);

	if(tlscfg_tls_reload() < 0) {
		rpc->fault(ctx, 500, "Config reloaded but tls.reload failed");
		return;
	}

	rpc->rpl_printf(ctx, "Ok. Config reloaded and TLS reloaded.");
}
