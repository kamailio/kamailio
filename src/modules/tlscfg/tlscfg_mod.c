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
 * @file tlscfg_mod.c
 * @brief TLS profile management — module entry point
 *
 * Companion to the tls module. Provides dynamic profile management via
 * RPC, config file write-back with debounce, and cert file mtime watching.
 * Does not implement any TLS plumbing — delegates all TLS to the tls module.
 *
 * @ingroup tlscfg
 */

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include "../../core/timer_proc.h"
#include "../../core/mod_fix.h"
#include "../../core/kemi.h"

#include "tlscfg_config.h"
#include "tlscfg_profile.h"
#include "tlscfg_rpc.h"
#include "tlscfg_watch.h"

#include <string.h>

MODULE_VERSION

/* ---- module parameters ------------------------------------------------- */

static int cert_check_interval = 300; /* seconds between cert mtime checks */
static int debounce_interval = 2;	  /* seconds idle before write + reload */
static char *cert_base_path = "/var/lib/certman/certs";
static char *config_backup_dir = ""; /* empty = same dir as tls.cfg */
static int config_max_backups = 10;

/* discovered from tls module at init */
static str tls_cfg_path = STR_NULL;

/* ---- forward declarations ---------------------------------------------- */

static int mod_init(void);
static int child_init(int rank);
static void mod_destroy(void);

/* ---- RPC exports ------------------------------------------------------- */

/* clang-format off */
static const char *tlscfg_rpc_profile_add_doc[] = {
	"Add a new TLS profile", 0
};
static const char *tlscfg_rpc_profile_update_doc[] = {
	"Update a field on a TLS profile", 0
};
static const char *tlscfg_rpc_profile_remove_doc[] = {
	"Remove a TLS profile", 0
};
static const char *tlscfg_rpc_profile_list_doc[] = {
	"List all TLS profiles", 0
};
static const char *tlscfg_rpc_profile_get_doc[] = {
	"Get full details of a TLS profile", 0
};
static const char *tlscfg_rpc_profile_enable_doc[] = {
	"Enable a TLS profile", 0
};
static const char *tlscfg_rpc_profile_disable_doc[] = {
	"Disable a TLS profile", 0
};
static const char *tlscfg_rpc_cert_check_doc[] = {
	"Check all referenced cert files for changes", 0
};
static const char *tlscfg_rpc_cert_notify_doc[] = {
	"Notify that a cert file changed (immediate reload)", 0
};
static const char *tlscfg_rpc_reload_doc[] = {
	"Re-read config file and trigger tls.reload", 0
};

static rpc_export_t tlscfg_rpc_cmds[] = {
	{"tlscfg.profile_add",     tlscfg_rpc_profile_add,
		tlscfg_rpc_profile_add_doc,     0},
	{"tlscfg.profile_update",  tlscfg_rpc_profile_update,
		tlscfg_rpc_profile_update_doc,  0},
	{"tlscfg.profile_remove",  tlscfg_rpc_profile_remove,
		tlscfg_rpc_profile_remove_doc,  0},
	{"tlscfg.profile_list",    tlscfg_rpc_profile_list,
		tlscfg_rpc_profile_list_doc,    RET_ARRAY},
	{"tlscfg.profile_get",     tlscfg_rpc_profile_get,
		tlscfg_rpc_profile_get_doc,     0},
	{"tlscfg.profile_enable",  tlscfg_rpc_profile_enable,
		tlscfg_rpc_profile_enable_doc,  0},
	{"tlscfg.profile_disable", tlscfg_rpc_profile_disable,
		tlscfg_rpc_profile_disable_doc, 0},
	{"tlscfg.cert_check",      tlscfg_rpc_cert_check,
		tlscfg_rpc_cert_check_doc,      0},
	{"tlscfg.cert_notify",     tlscfg_rpc_cert_notify,
		tlscfg_rpc_cert_notify_doc,     0},
	{"tlscfg.reload",          tlscfg_rpc_reload,
		tlscfg_rpc_reload_doc,          0},
	{0, 0, 0, 0}
};
/* clang-format on */

/* ---- module exports ---------------------------------------------------- */

/* clang-format off */
static param_export_t params[] = {
	{"cert_check_interval", PARAM_INT, &cert_check_interval},
	{"debounce_interval",   PARAM_INT, &debounce_interval},
	{"cert_base_path",      PARAM_STRING, &cert_base_path},
	{"config_backup_dir",   PARAM_STRING, &config_backup_dir},
	{"config_max_backups",  PARAM_INT, &config_max_backups},
	{0, 0, 0}
};

struct module_exports exports = {
	"tlscfg",        /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	0,               /* cmd (cfg function) exports */
	params,          /* param exports */
	0,               /* RPC method exports */
	0,               /* pseudo-variables exports */
	0,               /* response handling function */
	mod_init,        /* module init function */
	child_init,      /* per-child init function */
	mod_destroy      /* module destroy function */
};
/* clang-format on */

/* ---- accessors for other translation units ----------------------------- */

str *tlscfg_get_tls_cfg_path(void)
{
	return &tls_cfg_path;
}

int tlscfg_get_cert_check_interval(void)
{
	return cert_check_interval;
}

int tlscfg_get_debounce_interval(void)
{
	return debounce_interval;
}

const char *tlscfg_get_cert_base_path(void)
{
	return cert_base_path;
}

const char *tlscfg_get_config_backup_dir(void)
{
	return config_backup_dir;
}

int tlscfg_get_config_max_backups(void)
{
	return config_max_backups;
}

/* ---- module lifecycle -------------------------------------------------- */

/**
 * @brief Discover the tls module's config file path
 *
 * Uses find_param_export() to locate the "config" modparam on the tls
 * module. The value is copied so we are independent of the tls module's
 * internal storage after init.
 *
 * @return 0 on success, -1 on error
 */
static int tlscfg_discover_tls_config(void)
{
	modparam_t param_type;
	str *cfg_file;

	if(!module_loaded("tls")) {
		LM_ERR("tls module is not loaded — tlscfg requires it\n");
		return -1;
	}

	cfg_file = (str *)find_param_export(
			find_module_by_name("tls"), "config", PARAM_STR, &param_type);
	if(cfg_file == NULL || cfg_file->s == NULL || cfg_file->len == 0) {
		LM_ERR("tls module has no 'config' parameter set — tlscfg needs "
			   "a tls.cfg file to manage\n");
		return -1;
	}

	/* take our own copy */
	tls_cfg_path.len = cfg_file->len;
	tls_cfg_path.s = pkg_malloc(cfg_file->len + 1);
	if(tls_cfg_path.s == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	memcpy(tls_cfg_path.s, cfg_file->s, cfg_file->len);
	tls_cfg_path.s[cfg_file->len] = '\0';

	LM_INFO("discovered tls config file: %.*s\n", tls_cfg_path.len,
			tls_cfg_path.s);
	return 0;
}

static int mod_init(void)
{
	LM_INFO("tlscfg module initializing\n");

	/* init shared memory data structures */
	if(tlscfg_data_init() < 0) {
		LM_ERR("failed to init shared data\n");
		return -1;
	}

	/* discover tls module config path */
	if(tlscfg_discover_tls_config() < 0) {
		return -1;
	}

	/* register RPC commands */
	if(rpc_register_array(tlscfg_rpc_cmds) != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	/* load existing tls.cfg into profile index */
	{
		tlscfg_data_t *data = tlscfg_data_get();
		lock_get(data->lock);
		if(tlscfg_config_load(&tls_cfg_path) < 0) {
			LM_WARN("failed to load tls config — starting with "
					"empty profile index\n");
		}
		lock_release(data->lock);
	}

	/* reserve timer slot: 1-second tick for debounce + cert watch */
	if(register_basic_timers(1) != 0) {
		LM_ERR("failed to register timer slot\n");
		return -1;
	}

	LM_INFO("tlscfg module initialized (cert_check_interval=%d, "
			"debounce_interval=%d, cert_base_path=%s)\n",
			cert_check_interval, debounce_interval, cert_base_path);
	return 0;
}

static int child_init(int rank)
{
	if(rank != PROC_MAIN) {
		return 0;
	}

	/* fork 1-second timer for debounce flush + cert mtime watch */
	if(fork_basic_timer(PROC_TIMER, "TLSCFG WATCHER", 1, tlscfg_watch_timer,
			   NULL, 1 /*sec*/)
			< 0) {
		LM_ERR("failed to fork watcher timer process\n");
		return -1;
	}

	return 0;
}

static void mod_destroy(void)
{
	tlscfg_data_destroy();
	if(tls_cfg_path.s != NULL) {
		pkg_free(tls_cfg_path.s);
		tls_cfg_path.s = NULL;
		tls_cfg_path.len = 0;
	}
	LM_INFO("tlscfg module destroyed\n");
}
