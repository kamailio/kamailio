/**
 * Copyright (C) 2026 Aurora Innovation AB
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
 */

/*!
 * \file
 * \brief SIPp Integration Module
 * \ingroup sipp
 * - Module: \ref sipp
 *
 * This module provides integration with SIPp for automated SIP load testing.
 * It allows starting, stopping, and monitoring SIPp tests via RPC commands.
 *
 * Features:
 * - Start SIPp tests with configurable scenarios
 * - Support for multiple concurrent calls
 * - Dynamic target domain and number configuration
 * - Test statistics and monitoring via RPC
 * - Automatic transport detection (UDP/TCP)
 * - Ephemeral port binding to Kamailio send sockets
 *
 * Example configuration:
 * \code
 * loadmodule "sipp.so"
 * modparam("sipp", "scenario_dir", "/etc/kamailio/sipp")
 * modparam("sipp", "target_domain", "sip.example.com")
 * modparam("sipp", "send_socket_name", "udp:eth0:5060")
 * modparam("sipp", "default_concurrent_calls", 50)
 * modparam("sipp", "max_concurrent_calls", 1000)
 * modparam("sipp", "valid_domains", "sip.example.com,test.example.com")
 * \endcode
 *
 * RPC Commands:
 * - sipp.start_test - Start a new SIPp test
 * - sipp.stop_test - Stop a running SIPp test
 * - sipp.get_stats - Get statistics for a running or completed test
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/mod_fix.h"
#include "../../core/socket_info.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include "../../core/timer.h"

#include "sipp_test.h"
#include "sipp_rpc.h"
#include "sipp_prom.h"

MODULE_VERSION


/* Module parameters */
static str sipp_path = STR_NULL;
static str sipp_send_socket_name = STR_NULL;
static str sipp_send_socket = STR_NULL;
static str sipp_target_domain = STR_NULL;
static str sipp_scenario_dir = STR_NULL;
static str sipp_work_dir = STR_NULL;
static str sipp_media_ip = STR_NULL;
static str sipp_valid_domains = STR_NULL;
static str sipp_default_from_number = STR_NULL;
static str sipp_default_to_number = STR_NULL;

static int sipp_default_concurrent_calls = 50;
static int sipp_max_concurrent_calls = 1000;
static int sipp_max_concurrent_tests = 10; /* SIPp hard limit is 60 */
static int sipp_default_duration = 60;
static int sipp_default_call_rate = 10;
static int sipp_default_total_calls = 0;
static int sipp_default_max_test_duration = 0;
static int sipp_enable_prometheus = 1;

/* Module functions */
static int mod_init(void);
static int child_init(int rank);
static void mod_destroy(void);

/* clang-format off */
static param_export_t params[] = {
	{"sipp_path", PARAM_STR, &sipp_path},
	{"send_socket_name", PARAM_STR, &sipp_send_socket_name},
	{"send_socket", PARAM_STR, &sipp_send_socket},
	{"target_domain", PARAM_STR, &sipp_target_domain},
	{"default_caller", PARAM_STR, &sipp_default_from_number},
	{"default_service", PARAM_STR, &sipp_default_to_number},
	{"scenario_dir", PARAM_STR, &sipp_scenario_dir},
	{"work_dir", PARAM_STR, &sipp_work_dir},
	{"media_ip", PARAM_STR, &sipp_media_ip},
	{"valid_domains", PARAM_STR, &sipp_valid_domains},
	{"default_concurrent_calls", PARAM_INT, &sipp_default_concurrent_calls},
	{"max_concurrent_calls", PARAM_INT, &sipp_max_concurrent_calls},
	{"max_concurrent_tests", PARAM_INT, &sipp_max_concurrent_tests},
	{"default_duration", PARAM_INT, &sipp_default_duration},
	{"default_call_rate", PARAM_INT, &sipp_default_call_rate},
	{"default_total_calls", PARAM_INT, &sipp_default_total_calls},
	{"default_max_test_duration", PARAM_INT, &sipp_default_max_test_duration},
	{"enable_prometheus", PARAM_INT, &sipp_enable_prometheus},
	{0, 0, 0}
};

static cmd_export_t cmds[] = {
	{0, 0, 0, 0, 0, 0}
};

struct module_exports exports = {
	"sipp",			   /* module name */
	DEFAULT_DLFLAGS,   /* dlopen flags */
	cmds,			   /* exported functions */
	params,			   /* exported parameters */
	0,				   /* exported RPC methods */
	0,				   /* exported pseudo-variables */
	0,				   /* response function */
	mod_init,		   /* module initialization function */
	child_init,		   /* per-child init function */
	mod_destroy		   /* destroy function */
};
/* clang-format on */


/**
 * Timer callback for periodic metrics update
 * Called every 5-10 seconds
 */
static void sipp_metrics_timer(unsigned int ticks, void *param)
{
	LM_DBG("updating SIPp metrics\n");
	sipp_metrics_update();
}


/**
 * Timer callback for periodic cleanup
 * Called every hour
 */
static void sipp_cleanup_timer(unsigned int ticks, void *param)
{
	LM_DBG("running SIPp cleanup\n");
	sipp_test_cleanup();
}


static int sipp_validate_media_ip(const str *media_ip)
{
	int i;

	if(media_ip == NULL || media_ip->s == NULL || media_ip->len <= 0) {
		return -1;
	}

	for(i = 0; i < media_ip->len; i++) {
		unsigned char c = (unsigned char)media_ip->s[i];
		if(!isalnum(c) && c != '.' && c != ':' && c != '-' && c != '_') {
			LM_ERR("media_ip contains unsafe character '%c' (0x%02x) at position %d\n",
					c, c, i);
			return -1;
		}
	}

	return 0;
}


/**
 * Module initialization function
 * - Validates module parameters
 * - Initializes test management structures
 * - Registers RPC commands
 */
static int mod_init(void)
{
	socket_info_t *si = NULL;
	str sproto;

	LM_INFO("initializing SIPp integration module\n");

	/* Check for required parameters */
	if(sipp_scenario_dir.s == NULL || sipp_scenario_dir.len <= 0) {
		LM_ERR("scenario_dir parameter is required\n");
		return -1;
	}

	if(sipp_target_domain.s == NULL || sipp_target_domain.len <= 0) {
		LM_INFO("target_domain not set (optional)\n");
	}

	/* Log the scenario directory path for troubleshooting */
	LM_INFO("scenario_dir configured as: %.*s\n", sipp_scenario_dir.len,
			sipp_scenario_dir.s);

	if(sipp_work_dir.s != NULL && sipp_work_dir.len > 0) {
		if(access(sipp_work_dir.s, W_OK | X_OK) < 0) {
			LM_ERR("work_dir not writable: %.*s (%s)\n", sipp_work_dir.len,
					sipp_work_dir.s, strerror(errno));
			return -1;
		}
	}

	if(sipp_media_ip.s != NULL && sipp_media_ip.len > 0) {
		if(sipp_validate_media_ip(&sipp_media_ip) < 0) {
			LM_ERR("media_ip contains unsafe characters\n");
			return -1;
		}
	}

	/* Validate that only one send socket parameter is provided */
	if(sipp_send_socket_name.s != NULL && sipp_send_socket.s != NULL) {
		LM_ERR("send_socket_name and send_socket are mutually exclusive\n");
		return -1;
	}

	/* Validate send_socket_name if provided */
	if(sipp_send_socket_name.s != NULL && sipp_send_socket_name.len > 0) {
		si = lookup_local_socket(&sipp_send_socket_name);
		if(si == NULL) {
			LM_ERR("send_socket_name not found: %.*s\n",
					sipp_send_socket_name.len, sipp_send_socket_name.s);
			return -1;
		}
		LM_INFO("using send socket: %.*s (proto=%d, addr=%s:%d)\n",
				sipp_send_socket_name.len, sipp_send_socket_name.s, si->proto,
				ip_addr2a(&si->address), si->port_no);
	}

	/* Validate send_socket if provided */
	if(sipp_send_socket.s != NULL && sipp_send_socket.len > 0) {
		/* Parse the raw socket format: <proto>:<ip>:<port> */
		char *p = sipp_send_socket.s;
		char *end = p + sipp_send_socket.len;
		char *colon1 = memchr(p, ':', end - p);
		if(colon1 == NULL) {
			LM_ERR("invalid send_socket format (missing protocol): %.*s\n",
					sipp_send_socket.len, sipp_send_socket.s);
			return -1;
		}
		sproto.s = p;
		sproto.len = colon1 - p;

		/* Validate protocol */
		if(sproto.len == 3 && strncasecmp(sproto.s, "udp", 3) == 0) {
			/* UDP is valid */
		} else if(sproto.len == 3 && strncasecmp(sproto.s, "tcp", 3) == 0) {
			/* TCP is valid */
		} else if(sproto.len == 3 && strncasecmp(sproto.s, "tls", 3) == 0) {
			LM_WARN("TLS transport specified but not yet fully supported\n");
		} else {
			LM_ERR("invalid transport protocol in send_socket: %.*s\n",
					sproto.len, sproto.s);
			return -1;
		}

		LM_INFO("using raw send socket: %.*s\n", sipp_send_socket.len,
				sipp_send_socket.s);
	}

	/* Validate concurrent call limits */
	if(sipp_default_concurrent_calls < 1) {
		LM_ERR("default_concurrent_calls must be at least 1\n");
		return -1;
	}

	if(sipp_max_concurrent_calls < sipp_default_concurrent_calls) {
		LM_ERR("max_concurrent_calls must be >= default_concurrent_calls\n");
		return -1;
	}

	if(sipp_default_duration < 1) {
		LM_ERR("default_duration must be at least 1 second\n");
		return -1;
	}

	if(sipp_default_total_calls < 0) {
		LM_ERR("default_total_calls must be >= 0\n");
		return -1;
	}

	if(sipp_default_max_test_duration < 0) {
		LM_ERR("default_max_test_duration must be >= 0\n");
		return -1;
	}


	/* Validate and cap max_concurrent_tests at SIPp's hard limit of 60 */
	if(sipp_max_concurrent_tests < 1) {
		LM_ERR("max_concurrent_tests must be at least 1\n");
		return -1;
	}
	if(sipp_max_concurrent_tests > 60) {
		LM_WARN("max_concurrent_tests capped at 60 (SIPp hard limit)\n");
		sipp_max_concurrent_tests = 60;
	}

	/* Initialize test management */
	if(sipp_test_init() < 0) {
		LM_ERR("failed to initialize test management\n");
		return -1;
	}

	/* Register RPC commands */
	if(sipp_rpc_init() < 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	/* Initialize Prometheus integration (optional) */
	if(sipp_prom_init() < 0) {
		LM_ERR("failed to initialize Prometheus integration\n");
		return -1;
	}

	/* Register periodic timers */
	/* Metrics update: every 10 seconds */
	if(register_timer(sipp_metrics_timer, NULL, 10) < 0) {
		LM_ERR("failed to register metrics timer\n");
		return -1;
	}

	/* Cleanup: every 1 hour (3600 seconds) */
	if(register_timer(sipp_cleanup_timer, NULL, 3600) < 0) {
		LM_ERR("failed to register cleanup timer\n");
		return -1;
	}

	LM_INFO("SIPp module initialized successfully\n");
	return 0;
}


/**
 * Per-child initialization function
 */
static int child_init(int rank)
{
	LM_DBG("child [%d] initialized\n", rank);
	return 0;
}


/**
 * Module cleanup function
 * - Stops all running tests
 * - Frees allocated memory
 */
static void mod_destroy(void)
{
	LM_INFO("cleaning up SIPp module\n");
	sipp_test_destroy();
}


/**
 * Accessor functions for module parameters
 */

str *sipp_get_send_socket_name(void)
{
	return &sipp_send_socket_name;
}

str *sipp_get_send_socket(void)
{
	return &sipp_send_socket;
}

str *sipp_get_target_domain(void)
{
	return &sipp_target_domain;
}

str *sipp_get_scenario_dir(void)
{
	return &sipp_scenario_dir;
}

str *sipp_get_work_dir(void)
{
	return &sipp_work_dir;
}

str *sipp_get_media_ip(void)
{
	return &sipp_media_ip;
}

str *sipp_get_valid_domains(void)
{
	return &sipp_valid_domains;
}

int sipp_get_default_concurrent_calls(void)
{
	return sipp_default_concurrent_calls;
}

int sipp_get_max_concurrent_calls(void)
{
	return sipp_max_concurrent_calls;
}

int sipp_get_max_concurrent_tests(void)
{
	return sipp_max_concurrent_tests;
}

int sipp_get_default_duration(void)
{
	return sipp_default_duration;
}

int sipp_get_default_call_rate(void)
{
	return sipp_default_call_rate;
}

int sipp_get_default_total_calls(void)
{
	return sipp_default_total_calls;
}

int sipp_get_default_max_test_duration(void)
{
	return sipp_default_max_test_duration;
}

str *sipp_get_sipp_path(void)
{
	return &sipp_path;
}

str *sipp_get_default_from_number(void)
{
	return &sipp_default_from_number;
}

str *sipp_get_default_to_number(void)
{
	return &sipp_default_to_number;
}

int sipp_get_enable_prometheus(void)
{
	return sipp_enable_prometheus;
}
