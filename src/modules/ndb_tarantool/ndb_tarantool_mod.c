/*
 * Copyright (C) 2026 Andrei Lashchinskii <koorwork+kamailio@gmail.com>
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/kemi.h"

#include "tarantool_client.h"
#include "tarantool_kemi.h"

MODULE_VERSION

/* Global connection pool */
tnt_pool_t tnt_global_pool;

/* Module parameters */
static char *tnt_server = NULL;
static char *tnt_host = TNT_DEFAULT_HOST;
static int tnt_port = TNT_DEFAULT_PORT;
static char *tnt_user = NULL;
static char *tnt_pass = NULL;
static int tnt_pool_size = TNT_DEFAULT_POOL_SIZE;
static int tnt_connect_timeout = 1000;
static int tnt_cmd_timeout = 500;
static int tnt_disable_time = 10;
static int tnt_allowed_timeouts = 3;
static int init_without_tarantool = 0;

static int mod_init(void);
static void mod_destroy(void);

static cmd_export_t cmds[] = {{0, 0, 0, 0, 0, 0}};

static param_export_t params[] = {{"server", PARAM_STRING, &tnt_server},
		{"host", PARAM_STRING, &tnt_host}, {"port", PARAM_INT, &tnt_port},
		{"user", PARAM_STRING, &tnt_user}, {"pass", PARAM_STRING, &tnt_pass},
		{"pool_size", PARAM_INT, &tnt_pool_size},
		{"connect_timeout", PARAM_INT, &tnt_connect_timeout},
		{"cmd_timeout", PARAM_INT, &tnt_cmd_timeout},
		{"disable_time", PARAM_INT, &tnt_disable_time},
		{"allowed_timeouts", PARAM_INT, &tnt_allowed_timeouts},
		{"init_without_tarantool", PARAM_INT, &init_without_tarantool},
		{0, 0, 0}};

struct module_exports exports = {
		"ndb_tarantool", /* module name */
		DEFAULT_DLFLAGS, /* dlopen flags */
		cmds,			 /* exported functions */
		params,			 /* exported parameters */
		0,				 /* exported RPC methods */
		0,				 /* exported pseudo-variables */
		0,				 /* response function */
		mod_init,		 /* module initialization function */
		0,				 /* per child init function */
		mod_destroy		 /* destroy function */
};

/* Helper to parse server URL string (e.g. "addr=127.0.0.1;port=3301;user=rtpe;pass=sec;pool=8") */
static void parse_server_url(const char *url)
{
	if(!url)
		return;
	char *copy = strdup(url);
	if(!copy)
		return;

	char *token = strtok(copy, ";");
	while(token) {
		char *eq = strchr(token, '=');
		if(eq) {
			*eq = '\0';
			char *key = token;
			char *val = eq + 1;
			if(strcmp(key, "addr") == 0 || strcmp(key, "host") == 0) {
				tnt_host = strdup(val);
			} else if(strcmp(key, "port") == 0) {
				tnt_port = atoi(val);
			} else if(strcmp(key, "user") == 0) {
				tnt_user = strdup(val);
			} else if(strcmp(key, "pass") == 0) {
				tnt_pass = strdup(val);
			} else if(strcmp(key, "pool") == 0) {
				tnt_pool_size = atoi(val);
			}
		}
		token = strtok(NULL, ";");
	}
	free(copy);
}

static int mod_init(void)
{
	if(tnt_server) {
		parse_server_url(tnt_server);
	}

	LM_INFO("Initializing ndb_tarantool connecting to %s:%d (pool=%d, "
			"timeout=%dms)...\n",
			tnt_host, tnt_port, tnt_pool_size, tnt_connect_timeout);

	int rc = tnt_pool_init(&tnt_global_pool, tnt_host, tnt_port, tnt_user,
			tnt_pass, tnt_pool_size, tnt_connect_timeout);
	if(rc != 0) {
		if(init_without_tarantool) {
			LM_WARN("Failed to connect to Tarantool at bootstrap, but "
					"init_without_tarantool is enabled. Continuing...\n");
			return 0;
		}
		LM_ERR("Failed to initialize Tarantool connection pool\n");
		return -1;
	}

	LM_INFO("ndb_tarantool module initialized successfully.\n");
	return 0;
}

static void mod_destroy(void)
{
	LM_INFO("Destroying ndb_tarantool module...\n");
	tnt_pool_destroy(&tnt_global_pool);
}

/* KEMI Bindings */
static int ki_tarantool_call(sip_msg_t *msg, str *proc, str *params, str *res)
{
	return sr_kemi_tarantool_call(
			msg, (str_t *)proc, (str_t *)params, (str_t *)res);
}

static int ki_tarantool_eval(sip_msg_t *msg, str *code, str *params, str *res)
{
	return sr_kemi_tarantool_eval(
			msg, (str_t *)code, (str_t *)params, (str_t *)res);
}

static sr_kemi_t sr_kemi_ndb_tarantool_exports[] = {
		{str_init("tarantool"), str_init("call"), SR_KEMIP_INT,
				(void *)ki_tarantool_call,
				{SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
						SR_KEMIP_NONE, SR_KEMIP_NONE}},
		{str_init("tarantool"), str_init("eval"), SR_KEMIP_INT,
				(void *)ki_tarantool_eval,
				{SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
						SR_KEMIP_NONE, SR_KEMIP_NONE}},
		{{0, 0}, {0, 0}, 0, NULL, {0, 0, 0, 0, 0, 0}}};

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	(void)path;
	(void)dlflags;
	(void)p1;
	(void)p2;
	sr_kemi_modules_add(sr_kemi_ndb_tarantool_exports);
	return 0;
}
