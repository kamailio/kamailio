/*
 * Copyright (C) 2026 lean1ee <https://github.com/lean1ee>
 *
 * Author: lean1ee
 * Module: ndb_tarantool - High performance Tarantool 3.x connector for Kamailio
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__has_include)
#if __has_include("../../core/sr_module.h")
#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/kemi.h"
#define HAS_KAMAILIO_CORE 1
#endif
#endif

#ifndef HAS_KAMAILIO_CORE
#define MODULE_VERSION
#define DEFAULT_DLFLAGS 0
#define LM_INFO(...) printf("[ndb_tarantool] INFO: " __VA_ARGS__)
#define LM_ERR(...) fprintf(stderr, "[ndb_tarantool] ERROR: " __VA_ARGS__)
#define LM_WARN(...) fprintf(stderr, "[ndb_tarantool] WARN: " __VA_ARGS__)
#define LM_DBG(...) printf("[ndb_tarantool] DBG: " __VA_ARGS__)
#define PARAM_STRING 1
#define PARAM_INT 2
#define SR_KEMIP_NONE 0
#define SR_KEMIP_INT 1
#define SR_KEMIP_STR 2

typedef struct str_core
{
	char *s;
	int len;
} str;
#define str_init(v) {(char *)(v), (int)(sizeof(v) - 1)}

typedef struct param_export_s
{
	const char *name;
	int type;
	void *param_pointer;
} param_export_t;
typedef struct cmd_export_s
{
	const char *name;
	void *function;
	int fixup;
	int free_fixup;
	int flags;
	int extra;
} cmd_export_t;

struct module_exports
{
	const char *name;
	int dlflags;
	cmd_export_t *cmds;
	param_export_t *params;
	void *rpc;
	void *pvs;
	void *resp;
	int (*init_f)(void);
	int (*child_init_f)(int);
	void (*destroy_f)(void);
};

typedef struct sip_msg
{
	int id;
} sip_msg_t;
typedef struct sr_kemi_s
{
	str mname;
	str fname;
	int rtype;
	void *func;
	int ptypes[6];
} sr_kemi_t;
static inline int sr_kemi_modules_add(sr_kemi_t *k)
{
	(void)k;
	return 0;
}
#endif

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
