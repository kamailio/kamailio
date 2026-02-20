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

#include <stdio.h>
#include <string.h>

#include "../../core/dprint.h"
#include "../../core/sr_module.h"

#include "../xhttp_prom/bind_prom.h"
#include "sipp_prom.h"
#include "sipp_test.h"

/* Prometheus module API */
static prom_api_t _sipp_prom_api;
static int _sipp_prom_metrics_ready = 0;

static int _sipp_prom_enabled = 0;

static int sipp_prom_create_metrics(void)
{
	if(_sipp_prom_metrics_ready) {
		return 0;
	}

	if(_sipp_prom_api.counter_create == NULL
			|| _sipp_prom_api.gauge_create == NULL) {
		LM_ERR("Prometheus API missing create functions\n");
		return -1;
	}

	if(_sipp_prom_api.counter_create(
			   "name=sipp_calls_started_total;help=Total calls initiated;"
			   "label=test_id:scenario:from_to")
			|| _sipp_prom_api.counter_create(
					"name=sipp_calls_completed_total;help=Successfully "
					"completed "
					"calls;label=test_id:scenario:from_to")
			|| _sipp_prom_api.counter_create(
					"name=sipp_calls_failed_total;help=Failed calls;"
					"label=test_id:scenario:from_to")
			|| _sipp_prom_api.gauge_create(
					"name=sipp_calls_active;help=Currently active calls;"
					"label=test_id:scenario:from_to")
			|| _sipp_prom_api.gauge_create(
					"name=sipp_concurrent_calls_configured;help=Configured "
					"concurrent call limit;label=test_id:scenario:from_to")) {
		LM_ERR("Failed to create Prometheus metrics\n");
		return -1;
	}

	_sipp_prom_metrics_ready = 1;
	return 0;
}


/**
 * Initialize Prometheus integration
 * Checks if xhttp_prom module is loaded and binds to its API
 */
int sipp_prom_init(void)
{
	if(!sipp_get_enable_prometheus()) {
		LM_INFO("Prometheus integration disabled by config\n");
		return 0;
	}
	if(prom_load_api(&_sipp_prom_api) != 0) {
		LM_INFO("xhttp_prom module not loaded - Prometheus metrics disabled\n");
		LM_INFO("To enable metrics, load xhttp_prom module before sipp "
				"module\n");
		_sipp_prom_enabled = 0;
		return 0;
	}

	if(sipp_prom_create_metrics() != 0) {
		_sipp_prom_enabled = 0;
		return -1;
	}

	_sipp_prom_enabled = 1;
	LM_INFO("Prometheus integration enabled - xhttp_prom API bound\n");
	return 0;
}


/**
 * Push SIPp test metrics to Prometheus
 */
void sipp_prom_push_metrics(sipp_test_t *test)
{
	char test_id_str[32];
	char scenario_name[256];
	char from_number[SIPP_MAX_NUMBER_LEN];
	char to_number[SIPP_MAX_NUMBER_LEN];
	char from_to_number[(SIPP_MAX_NUMBER_LEN * 2) + 8];
	str s_name;
	str l1;
	str l2;
	str l3;

	if(!_sipp_prom_enabled || test == NULL) {
		return;
	}

	/* Prepare labels */
	snprintf(test_id_str, sizeof(test_id_str), "%d", test->test_id);
	snprintf(scenario_name, sizeof(scenario_name), "%s", test->scenario);
	snprintf(from_number, sizeof(from_number), "%s", test->from_number);
	snprintf(to_number, sizeof(to_number), "%s", test->to_number);
	snprintf(from_to_number, sizeof(from_to_number), "%s->%s", from_number,
			to_number);

	l1.s = test_id_str;
	l1.len = (int)strlen(test_id_str);
	l2.s = scenario_name;
	l2.len = (int)strlen(scenario_name);
	l3.s = from_to_number;
	l3.len = (int)strlen(from_to_number);

	/* Counter: Total calls started */
	s_name.s = "sipp_calls_started_total";
	s_name.len = (int)strlen(s_name.s);
	_sipp_prom_api.counter_inc(&s_name, test->calls_started, &l1, &l2, &l3);

	/* Counter: Total calls completed */
	s_name.s = "sipp_calls_completed_total";
	s_name.len = (int)strlen(s_name.s);
	_sipp_prom_api.counter_inc(&s_name, test->calls_completed, &l1, &l2, &l3);

	/* Counter: Total calls failed */
	s_name.s = "sipp_calls_failed_total";
	s_name.len = (int)strlen(s_name.s);
	_sipp_prom_api.counter_inc(&s_name, test->calls_failed, &l1, &l2, &l3);

	/* Gauge: Active calls */
	s_name.s = "sipp_calls_active";
	s_name.len = (int)strlen(s_name.s);
	_sipp_prom_api.gauge_set(
			&s_name, (double)test->calls_active, &l1, &l2, &l3);

	/* Gauge: Concurrent calls configured */
	s_name.s = "sipp_concurrent_calls_configured";
	s_name.len = (int)strlen(s_name.s);
	_sipp_prom_api.gauge_set(
			&s_name, (double)test->concurrent_calls, &l1, &l2, &l3);

	LM_DBG("pushed metrics for test %d to Prometheus (from=%s, to=%s)\n",
			test->test_id, test->from_number, test->to_number);
}


/**
 * Check if Prometheus integration is enabled
 */
int sipp_prom_is_enabled(void)
{
	return _sipp_prom_enabled;
}
