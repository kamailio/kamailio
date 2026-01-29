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
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

#include "../../core/dprint.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include "../../core/ut.h"

#include "sipp_test.h"
#include "sipp_rpc.h"


/**
 * RPC: sipp.start_test
 *
 * Start a new SIPp test
 *
 * Parameters:
 * - scenario (string, required): Name of the SIPp XML scenario
 * - caller (string, optional): Caller user/number/range/list
 * - service (string, optional): Service user/number/range/list
 * - target_domain (string, optional): Target domain (overrides module param)
 * - concurrent_calls (int, optional): Number of concurrent calls
 * - duration (int, optional): Call duration in seconds
 * - total_calls (int, optional): Max total calls (0 = unlimited)
 * - max_test_duration (int, optional): Max test duration in seconds (0 = unlimited)
 *
 * Returns:
 * - test_id (int): Unique test identifier
 * - status (string): "started"
 *
 * Example:
 *   kamcmd sipp.start_test scenario=uac.xml caller=1000 service=2000 \
 *           target_domain=sip.example.com concurrent_calls=50 duration=60
 */
static void sipp_rpc_start_test(rpc_t *rpc, void *ctx)
{
	str scenario = STR_NULL;
	str first_param = STR_NULL;
	str param = STR_NULL;
	str param_list[8];
	str target_domain = STR_NULL;
	str caller = STR_NULL;
	str service = STR_NULL;
	int concurrent_calls = -1;
	int duration = -1;
	int total_calls = -1;
	int max_test_duration = -1;
	sipp_test_params_t params;
	int param_count = 0;
	int positional_idx = 0;
	int i = 0;
	int ival = 0;
	int test_id = 0;
	void *th = NULL;

	LM_DBG("RPC start_test called\n");

	/* Read first parameter (may be scenario or key=value) */
	if(rpc->scan(ctx, "S", &first_param) < 1 || first_param.s == NULL
			|| first_param.len <= 0) {
		LM_ERR("Failed to scan scenario parameter\n");
		rpc->fault(ctx, 400, "Missing required parameter: scenario");
		return;
	}

	/* Collect remaining parameters (positional or key=value), max 8 */
	while(rpc->scan(ctx, "*.S", &param) > 0) {
		if(param_count < 8) {
			param_list[param_count++] = param;
		} else {
			LM_WARN("Ignoring extra parameter beyond expected 7\n");
		}
	}

	/* If first parameter is key=value, push it to the list; otherwise treat as scenario */
	{
		char *eq = memchr(first_param.s, '=', first_param.len);
		if(eq != NULL) {
			if(param_count < 8) {
				param_list[param_count++] = first_param;
			} else {
				LM_WARN("Ignoring extra parameter beyond expected 8\n");
			}
		} else {
			scenario = first_param;
		}
	}

	for(i = 0; i < param_count; i++) {
		char *eq = memchr(param_list[i].s, '=', param_list[i].len);
		if(eq != NULL) {
			str key;
			str val;
			key.s = param_list[i].s;
			key.len = (int)(eq - param_list[i].s);
			val.s = eq + 1;
			val.len = param_list[i].len - key.len - 1;

			if(key.len == 8 && strncmp(key.s, "scenario", 8) == 0) {
				if(val.len > 0)
					scenario = val;
			} else if(key.len == 6 && strncmp(key.s, "caller", 6) == 0) {
				caller = val;
			} else if(key.len == 7 && strncmp(key.s, "service", 7) == 0) {
				service = val;
			} else if(key.len == 13
					  && strncmp(key.s, "target_domain", 13) == 0) {
				target_domain = val;
			} else if(key.len == 16
					  && strncmp(key.s, "concurrent_calls", 16) == 0) {
				if(val.len > 0 && str2sint(&val, &ival) == 0) {
					concurrent_calls = ival;
				} else if(val.len > 0) {
					rpc->fault(ctx, 400, "Invalid concurrent_calls parameter");
					return;
				}
			} else if(key.len == 8 && strncmp(key.s, "duration", 8) == 0) {
				if(val.len > 0 && str2sint(&val, &ival) == 0) {
					duration = ival;
				} else if(val.len > 0) {
					rpc->fault(ctx, 400, "Invalid duration parameter");
					return;
				}
			} else if(key.len == 11 && strncmp(key.s, "total_calls", 11) == 0) {
				if(val.len > 0 && str2sint(&val, &ival) == 0) {
					total_calls = ival;
				} else if(val.len > 0) {
					rpc->fault(ctx, 400, "Invalid total_calls parameter");
					return;
				}
			} else if(key.len == 17
					  && strncmp(key.s, "max_test_duration", 17) == 0) {
				if(val.len > 0 && str2sint(&val, &ival) == 0) {
					max_test_duration = ival;
				} else if(val.len > 0) {
					rpc->fault(ctx, 400, "Invalid max_test_duration parameter");
					return;
				}
			} else {
				LM_WARN("Unknown parameter '%.*s' ignored\n", key.len, key.s);
			}
		} else {
			switch(positional_idx) {
				case 0:
					caller = param_list[i];
					break;
				case 1:
					service = param_list[i];
					break;
				case 2:
					target_domain = param_list[i];
					break;
				case 3:
					if(str2sint(&param_list[i], &ival) == 0) {
						concurrent_calls = ival;
					} else {
						rpc->fault(
								ctx, 400, "Invalid concurrent_calls parameter");
						return;
					}
					break;
				case 4:
					if(str2sint(&param_list[i], &ival) == 0) {
						duration = ival;
					} else {
						rpc->fault(ctx, 400, "Invalid duration parameter");
						return;
					}
					break;
				case 5:
					if(str2sint(&param_list[i], &ival) == 0) {
						total_calls = ival;
					} else {
						rpc->fault(ctx, 400, "Invalid total_calls parameter");
						return;
					}
					break;
				case 6:
					if(str2sint(&param_list[i], &ival) == 0) {
						max_test_duration = ival;
					} else {
						rpc->fault(ctx, 400,
								"Invalid max_test_duration parameter");
						return;
					}
					break;
				default:
					break;
			}
			positional_idx++;
		}
	}

	/* Scenario is required after parsing */
	if(scenario.s == NULL || scenario.len <= 0) {
		LM_ERR("Missing scenario parameter after parsing\n");
		rpc->fault(ctx, 400, "Missing required parameter: scenario");
		return;
	}

	/* Validate scenario */
	if(scenario.s == NULL || scenario.len <= 0) {
		rpc->fault(ctx, 400, "Invalid scenario parameter");
		return;
	}

	/* Initialize params structure */
	memset(&params, 0, sizeof(params));
	params.scenario = scenario;

	/* Set caller (optional, defaults will be used if not provided) */
	if(caller.s != NULL && caller.len > 0) {
		params.from_number = caller;
	}

	/* Set service (optional, defaults will be used if not provided) */
	if(service.s != NULL && service.len > 0) {
		params.to_number = service;
	}

	/* Set target_domain (optional, module default will be used if not provided) */
	if(target_domain.s != NULL && target_domain.len > 0) {
		params.target_domain = target_domain;
	}

	/* Set concurrent_calls (use module default if not provided) */
	if(concurrent_calls > 0) {
		params.concurrent_calls = concurrent_calls;
	} else {
		params.concurrent_calls = sipp_get_default_concurrent_calls();
	}

	/* Set duration (use module default if not provided) */
	if(duration > 0) {
		params.duration = duration;
	} else {
		params.duration = sipp_get_default_duration();
	}

	/* Set total_calls (0 = unlimited, use module default if not provided) */
	if(total_calls >= 0) {
		params.total_calls_limit = total_calls;
	} else {
		params.total_calls_limit = sipp_get_default_total_calls();
	}

	/* Set max_test_duration (0 = unlimited, use module default if not provided) */
	if(max_test_duration >= 0) {
		params.max_test_duration = max_test_duration;
	} else {
		params.max_test_duration = sipp_get_default_max_test_duration();
	}

	LM_DBG("starting test: scenario=%.*s, caller=%.*s, service=%.*s, "
		   "domain=%.*s, "
		   "calls=%d, duration=%d, total_calls=%d, max_test_duration=%d\n",
			params.scenario.len, params.scenario.s, params.from_number.len,
			params.from_number.s, params.to_number.len, params.to_number.s,
			params.target_domain.len, params.target_domain.s,
			params.concurrent_calls, params.duration, params.total_calls_limit,
			params.max_test_duration);


	/* Start the test */
	if(sipp_test_start(&params, &test_id) < 0) {
		rpc->fault(ctx, 500, "Failed to start SIPp test");
		return;
	}

	/* Return test_id and status */
	if(rpc->add(ctx, "{", &th) < 0) {
		rpc->fault(ctx, 500, "Failed to create response");
		return;
	}

	{
		sipp_test_t test_copy;
		int pid = 0;
		if(sipp_test_get_copy(test_id, &test_copy) == 0) {
			pid = test_copy.pid;
		}
		if(rpc->struct_add(th, "dsd", "test_id", test_id, "status", "started",
				   "pid", pid)
				< 0) {
			rpc->fault(ctx, 500, "Failed to add response fields");
			return;
		}
	}

	LM_INFO("test %d started successfully\n", test_id);
}


static const char *sipp_rpc_start_test_doc[] = {"Start a new SIPp test",
		"Parameters:",
		"  scenario (string, required) - Name of the SIPp XML scenario",
		"  caller (string, optional) - Caller user/number/range/list",
		"  service (string, optional) - Service user/number/range/list",
		"  target_domain (string, optional) - Target domain",
		"  concurrent_calls (int, optional) - Number of concurrent calls",
		"  duration (int, optional) - Call duration in seconds",
		"  total_calls (int, optional) - Max total calls (0 = unlimited)",
		"  max_test_duration (int, optional) - Max test duration in seconds (0 "
		"= unlimited)",
		"Example:", "  kamcmd sipp.start_test uac.xml",
		"  kamcmd sipp.start_test uac.xml 1000 2000 sip.example.com 50 60", 0};


/**
 * RPC: sipp.stop_test
 *
 * Stop a running SIPp test
 *
 * Parameters:
 * - test_id (int, required): Test ID to stop
 *
 * Returns:
 * - test_id (int): Test identifier
 * - status (string): "stopped"
 *
 * Example:
 *   kamcmd sipp.stop_test 1
 */
static void sipp_rpc_stop_test(rpc_t *rpc, void *ctx)
{
	int test_id = 0;
	void *th = NULL;

	/* Parse test_id parameter */
	if(rpc->scan(ctx, "d", &test_id) < 1) {
		rpc->fault(ctx, 400, "Missing required parameter: test_id");
		return;
	}

	if(test_id <= 0) {
		rpc->fault(ctx, 400, "Invalid test_id");
		return;
	}

	LM_DBG("stopping test %d\n", test_id);

	/* Stop the test */
	if(sipp_test_stop(test_id) < 0) {
		rpc->fault(ctx, 404, "Test not found or not running");
		return;
	}

	/* Return test_id and status */
	if(rpc->add(ctx, "{", &th) < 0) {
		rpc->fault(ctx, 500, "Failed to create response");
		return;
	}

	if(rpc->struct_add(th, "ds", "test_id", test_id, "status", "stopped") < 0) {
		rpc->fault(ctx, 500, "Failed to add response fields");
		return;
	}

	LM_DBG("test %d stopped successfully\n", test_id);
}


static const char *sipp_rpc_stop_test_doc[] = {"Stop a running SIPp test",
		"Parameters:", "  test_id (int, required) - Test ID to stop",
		"Example:", "  kamcmd sipp.stop_test 1", 0};


/**
 * RPC: sipp.get_stats
 *
 * Get statistics for a SIPp test
 *
 * Parameters:
 * - test_id (int, required): Test ID to query
 *
 * Returns:
 * - test_id (int): Test identifier
 * - state (string): Test state (running, stopped, completed, failed)
 * - scenario (string): Scenario name
 * - caller (string): Caller
 * - service (string): Service
 * - target_domain (string): Target domain
 * - concurrent_calls (int): Number of concurrent calls
 * - duration (int): Test duration
 * - start_time (int): Start timestamp
 * - end_time (int): End timestamp (0 if running)
 * - calls_started (int): Calls started
 * - calls_completed (int): Calls completed
 * - calls_failed (int): Calls failed
 *
 * Example:
 *   kamcmd sipp.get_stats 1
 */
static void sipp_rpc_get_stats(rpc_t *rpc, void *ctx)
{
	int test_id = 0;
	sipp_test_t test_copy;
	void *th = NULL;
	const char *state_str = "unknown";

	/* Parse test_id parameter */
	if(rpc->scan(ctx, "d", &test_id) < 1) {
		rpc->fault(ctx, 400, "Missing required parameter: test_id");
		return;
	}

	if(test_id <= 0) {
		rpc->fault(ctx, 400, "Invalid test_id");
		return;
	}

	LM_DBG("getting stats for test %d\n", test_id);

	/* First update live stats from stat file */
	sipp_test_get_live_stats(test_id);

	/* Get thread-safe copy of test data */
	if(sipp_test_get_copy(test_id, &test_copy) < 0) {
		rpc->fault(ctx, 404, "Test not found");
		return;
	}

	/* Convert state to string */
	switch(test_copy.state) {
		case SIPP_TEST_STATE_RUNNING:
			state_str = "running";
			break;
		case SIPP_TEST_STATE_STOPPED:
			state_str = "stopped";
			break;
		case SIPP_TEST_STATE_COMPLETED:
			state_str = "completed";
			break;
		case SIPP_TEST_STATE_FAILED:
			state_str = "failed";
			break;
		default:
			state_str = "unknown";
			break;
	}

	/* Build response */
	if(rpc->add(ctx, "{", &th) < 0) {
		rpc->fault(ctx, 500, "Failed to create response");
		return;
	}

	if(rpc->struct_add(th, "dsssssddddddddd", "test_id", test_copy.test_id,
			   "state", state_str, "scenario", test_copy.scenario, "caller",
			   test_copy.from_number, "service", test_copy.to_number,
			   "target_domain", test_copy.target_domain, "concurrent_calls",
			   // Removed logging of resolved RPC parameters

			   "total_calls", test_copy.total_calls_limit, "max_test_duration",
			   test_copy.max_test_duration, "start_time",
			   (int)test_copy.start_time, "end_time", (int)test_copy.end_time,
			   "calls_started", test_copy.calls_started, "calls_completed",
			   test_copy.calls_completed, "calls_failed",
			   test_copy.calls_failed)
			< 0) {
		rpc->fault(ctx, 500, "Failed to add response fields");
		return;
	}

	LM_DBG("returned stats for test %d\n", test_id);
}


static const char *sipp_rpc_get_stats_doc[] = {"Get statistics for a SIPp test",
		"Parameters:", "  test_id (int, required) - Test ID to query",
		"Returns:",
		"  test_id, state, scenario, caller, service, target_domain,",
		"  concurrent_calls, duration, total_calls, max_test_duration,",
		"  start_time, end_time,",
		"  calls_started, calls_completed, calls_failed",
		"Example:", "  kamcmd sipp.get_stats 1", 0};


/**
 * RPC: sipp.running
 *
 * List all running and recent SIPp tests
 *
 * Returns:
 * - Array of test information structures
 *
 * Example:
 *   kamcmd sipp.running
 */
static void sipp_rpc_running(rpc_t *rpc, void *ctx)
{
	sipp_test_t *test = NULL;
	void *th = NULL;
	const char *state_str = "unknown";
	int count = 0;

	LM_DBG("listing all tests\n");

	/* Lock for entire traversal to prevent race conditions */
	sipp_test_lock();

	/* Get all tests */
	test = sipp_test_get_all();

	while(test != NULL) {
		int cur_id = test->test_id;
		sipp_test_t *found = NULL;
		sipp_test_t *next = test->next;

		// Removed logging of effective test parameters
		sipp_test_update_stats(test);

		/* Refresh live stats from CSV without holding the lock */
		sipp_test_unlock();
		sipp_test_get_live_stats(cur_id);
		sipp_test_lock();
		/* Find test again (may have moved/been removed) */
		found = sipp_test_get_all();
		while(found != NULL && found->test_id != cur_id) {
			found = found->next;
		}
		if(found == NULL) {
			/* Continue from next if possible */
			test = next;
			continue;
		}
		test = found;

		/* Convert state to string */
		switch(test->state) {
			case SIPP_TEST_STATE_RUNNING:
				state_str = "running";
				break;
			case SIPP_TEST_STATE_STOPPED:
				state_str = "stopped";
				break;
			case SIPP_TEST_STATE_COMPLETED:
				state_str = "completed";
				break;
			case SIPP_TEST_STATE_FAILED:
				state_str = "failed";
				break;
			default:
				state_str = "unknown";
				break;
		}

		/* Add test info */
		if(rpc->add(ctx, "{", &th) < 0) {
			sipp_test_unlock();
			rpc->fault(ctx, 500, "Failed to create response");
			return;
		}

		if(rpc->struct_add(th, "dsssssddddddddd", "test_id", test->test_id,
				   "state", state_str, "scenario", test->scenario, "caller",
				   test->from_number, "service", test->to_number,
				   "target_domain", test->target_domain, "concurrent_calls",
				   test->concurrent_calls, "duration", test->duration,
				   "total_calls", test->total_calls_limit, "max_test_duration",
				   test->max_test_duration, "start_time", (int)test->start_time,
				   "end_time", (int)test->end_time, "calls_started",
				   test->calls_started, "calls_completed",
				   test->calls_completed, "calls_failed", test->calls_failed)
				< 0) {
			sipp_test_unlock();
			rpc->fault(ctx, 500, "Failed to add response fields");
			return;
		}

		count++;
		test = test->next;
	}

	sipp_test_unlock();
	LM_DBG("listed %d tests\n", count);
}


static const char *sipp_rpc_running_doc[] = {
		"List all running and recent SIPp tests",
		"Returns:", "  Array of test information structures",
		"Example:", "  kamcmd sipp.running", 0};

/**
 * RPC: sipp.list
 *
 * List available SIPp scenario files from the configured scenario_dir
 *
 * Returns:
 * - Array of scenario information structures
 *
 * Example:
 *   kamcmd sipp.list
 */
static void sipp_rpc_list(rpc_t *rpc, void *ctx)
{
	void *th = NULL;
	str *scenario_dir_str = NULL;
	char *scenario_dir = NULL;
	DIR *dir = NULL;
	struct dirent *entry = NULL;
	int count = 0;

	/* Get scenario directory */
	scenario_dir_str = sipp_get_scenario_dir();
	if(scenario_dir_str == NULL || scenario_dir_str->s == NULL
			|| scenario_dir_str->len == 0) {
		rpc->fault(ctx, 500, "Scenario directory not configured");
		return;
	}

	scenario_dir = scenario_dir_str->s;
	LM_DBG("listing scenarios from directory: %s\n", scenario_dir);

	/* Open directory */
	dir = opendir(scenario_dir);
	if(dir == NULL) {
		LM_ERR("failed to open scenario directory '%s': %s\n", scenario_dir,
				strerror(errno));
		rpc->fault(ctx, 500, "Failed to open scenario directory");
		return;
	}

	/* Read directory entries */
	while((entry = readdir(dir)) != NULL) {
		char filepath[512];
		struct stat st;
		int len = strlen(entry->d_name);
		FILE *f = NULL;
		char line[512];
		char scenario_name[256] = "";

		/* Skip . and .. */
		if(strcmp(entry->d_name, ".") == 0
				|| strcmp(entry->d_name, "..") == 0) {
			continue;
		}

		/* Only show .xml files */
		if(len < 4 || strcmp(entry->d_name + len - 4, ".xml") != 0) {
			continue;
		}

		/* Build full path */
		snprintf(filepath, sizeof(filepath), "%s/%s", scenario_dir,
				entry->d_name);

		/* Check if it's a regular file */
		if(stat(filepath, &st) != 0 || !S_ISREG(st.st_mode)) {
			continue;
		}

		/* Try to extract scenario name from XML */
		f = fopen(filepath, "r");
		if(f != NULL) {
			while(fgets(line, sizeof(line), f) != NULL) {
				char *p = strstr(line, "<scenario");
				if(p != NULL) {
					char *name_start = strstr(p, "name=\"");
					if(name_start != NULL) {
						name_start += 6; /* Skip 'name="' */
						char *name_end = strchr(name_start, '"');
						if(name_end != NULL) {
							int name_len = name_end - name_start;
							if(name_len > 0
									&& name_len < sizeof(scenario_name)) {
								strncpy(scenario_name, name_start, name_len);
								scenario_name[name_len] = '\0';
							}
						}
					}
					break;
				}
			}
			fclose(f);
		}

		/* Add scenario info */
		if(rpc->add(ctx, "{", &th) < 0) {
			rpc->fault(ctx, 500, "Failed to create response");
			closedir(dir);
			return;
		}

		if(scenario_name[0] != '\0') {
			if(rpc->struct_add(th, "ssd", "scenario", entry->d_name, "name",
					   scenario_name, "size", (int)st.st_size)
					< 0) {
				rpc->fault(ctx, 500, "Failed to add response fields");
				closedir(dir);
				return;
			}
		} else {
			if(rpc->struct_add(th, "sd", "scenario", entry->d_name, "size",
					   (int)st.st_size)
					< 0) {
				rpc->fault(ctx, 500, "Failed to add response fields");
				closedir(dir);
				return;
			}
		}

		count++;
	}

	closedir(dir);
	LM_DBG("listed %d scenarios\n", count);
}


static const char *sipp_rpc_list_doc[] = {"List available SIPp scenario files",
		"Returns:", "  Array of scenario files with size",
		"Example:", "  kamcmd sipp.list", 0};


/**
 * RPC: sipp.pause_test
 *
 * Pause a running SIPp test (requires remote control enabled)
 *
 * Parameters:
 * - test_id (int, required): Test ID to pause
 *
 * Returns:
 * - test_id (int): Test identifier
 * - status (string): "paused"
 *
 * Example:
 *   kamcmd sipp.pause_test 1
 */
static void sipp_rpc_pause_test(rpc_t *rpc, void *ctx)
{
	int test_id = 0;
	void *th = NULL;

	/* Parse test_id parameter */
	if(rpc->scan(ctx, "d", &test_id) < 1) {
		rpc->fault(ctx, 400, "Missing required parameter: test_id");
		return;
	}

	if(test_id <= 0) {
		rpc->fault(ctx, 400, "Invalid test_id");
		return;
	}

	LM_DBG("pausing test %d\n", test_id);

	/* Send pause command */
	if(sipp_test_control_command(test_id, 'p') < 0) {
		rpc->fault(ctx, 500,
				"Failed to pause test (remote control may not be enabled)");
		return;
	}

	/* Return test_id and status */
	if(rpc->add(ctx, "{", &th) < 0) {
		rpc->fault(ctx, 500, "Failed to create response");
		return;
	}

	if(rpc->struct_add(th, "ds", "test_id", test_id, "status", "paused") < 0) {
		rpc->fault(ctx, 500, "Failed to add response fields");
		return;
	}

	LM_DBG("test %d paused successfully\n", test_id);
}


static const char *sipp_rpc_pause_test_doc[] = {
		"Pause a running SIPp test (requires remote control)",
		"Parameters:", "  test_id (int, required) - Test ID to pause",
		"Example:", "  kamcmd sipp.pause_test 1", 0};


/**
 * RPC: sipp.resume_test
 *
 * Resume a paused SIPp test (requires remote control enabled)
 *
 * Parameters:
 * - test_id (int, required): Test ID to resume
 *
 * Returns:
 * - test_id (int): Test identifier
 * - status (string): "resumed"
 *
 * Example:
 *   kamcmd sipp.resume_test 1
 */
static void sipp_rpc_resume_test(rpc_t *rpc, void *ctx)
{
	int test_id = 0;
	void *th = NULL;

	/* Parse test_id parameter */
	if(rpc->scan(ctx, "d", &test_id) < 1) {
		rpc->fault(ctx, 400, "Missing required parameter: test_id");
		return;
	}

	if(test_id <= 0) {
		rpc->fault(ctx, 400, "Invalid test_id");
		return;
	}

	LM_DBG("resuming test %d\n", test_id);

	/* Send resume command */
	if(sipp_test_control_command(test_id, 'r') < 0) {
		rpc->fault(ctx, 500,
				"Failed to resume test (remote control may not be enabled)");
		return;
	}

	/* Return test_id and status */
	if(rpc->add(ctx, "{", &th) < 0) {
		rpc->fault(ctx, 500, "Failed to create response");
		return;
	}

	if(rpc->struct_add(th, "ds", "test_id", test_id, "status", "resumed") < 0) {
		rpc->fault(ctx, 500, "Failed to add response fields");
		return;
	}

	LM_DBG("test %d resumed successfully\n", test_id);
}


static const char *sipp_rpc_resume_test_doc[] = {
		"Resume a paused SIPp test (requires remote control)",
		"Parameters:", "  test_id (int, required) - Test ID to resume",
		"Example:", "  kamcmd sipp.resume_test 1", 0};


/**
 * RPC: sipp.adjust_rate
 *
 * Adjust call rate of a running SIPp test (requires remote control enabled)
 *
 * Parameters:
 * - test_id (int, required): Test ID
 * - change (string, required): "increase" or "decrease"
 *
 * Returns:
 * - test_id (int): Test identifier
 * - status (string): "rate_adjusted"
 *
 * Example:
 *   kamcmd sipp.adjust_rate 1 increase
 */
static void sipp_rpc_adjust_rate(rpc_t *rpc, void *ctx)
{
	int test_id = 0;
	char *change_str = NULL;
	char rate_cmd;
	void *th = NULL;

	/* Parse parameters */
	if(rpc->scan(ctx, "ds", &test_id, &change_str) < 2) {
		rpc->fault(ctx, 400, "Missing required parameters: test_id, change");
		return;
	}

	if(test_id <= 0) {
		rpc->fault(ctx, 400, "Invalid test_id");
		return;
	}

	if(strcmp(change_str, "increase") == 0) {
		rate_cmd = '+';
	} else if(strcmp(change_str, "decrease") == 0) {
		rate_cmd = '-';
	} else {
		rpc->fault(ctx, 400,
				"Invalid change value (use 'increase' or 'decrease')");
		return;
	}

	LM_DBG("adjusting rate for test %d (%c)\n", test_id, rate_cmd);

	/* Send rate adjustment command */
	if(sipp_test_adjust_rate(test_id, rate_cmd) < 0) {
		rpc->fault(ctx, 500,
				"Failed to adjust rate (remote control may not be enabled)");
		return;
	}

	/* Return test_id and status */
	if(rpc->add(ctx, "{", &th) < 0) {
		rpc->fault(ctx, 500, "Failed to create response");
		return;
	}

	if(rpc->struct_add(th, "ds", "test_id", test_id, "status", "rate_adjusted")
			< 0) {
		rpc->fault(ctx, 500, "Failed to add response fields");
		return;
	}

	LM_DBG("test %d rate adjusted (%s)\n", test_id, change_str);
}


static const char *sipp_rpc_adjust_rate_doc[] = {
		"Adjust call rate of a running SIPp test (requires remote control)",
		"Parameters:", "  test_id (int, required) - Test ID",
		"  change (string, required) - 'increase' or 'decrease'",
		"Example:", "  kamcmd sipp.adjust_rate 1 increase", 0};


/**
 * RPC: sipp.get_live_stats
 *
 * Get real-time statistics from SIPp control socket
 *
 * Parameters:
 * - test_id (int, required): Test ID to query
 *
 * Returns:
 * - test_id (int): Test identifier
 * - calls_active (int): Currently active calls
 * - calls_started (int): Total calls started
 * - calls_completed (int): Calls completed successfully
 * - calls_failed (int): Failed calls
 *
 * Example:
 *   kamcmd sipp.get_live_stats 1
 */
static void sipp_rpc_get_live_stats(rpc_t *rpc, void *ctx)
{
	int test_id = 0;
	sipp_test_t test_copy;
	void *th = NULL;

	/* Parse test_id parameter */
	if(rpc->scan(ctx, "d", &test_id) < 1) {
		rpc->fault(ctx, 400, "Missing required parameter: test_id");
		return;
	}

	if(test_id <= 0) {
		rpc->fault(ctx, 400, "Invalid test_id");
		return;
	}

	LM_DBG("getting live stats for test %d\n", test_id);

	/* Get live stats from stat file */
	if(sipp_test_get_live_stats(test_id) < 0) {
		rpc->fault(ctx, 500, "Failed to get live stats");
		return;
	}

	/* Get thread-safe copy of test data */
	if(sipp_test_get_copy(test_id, &test_copy) < 0) {
		rpc->fault(ctx, 404, "Test not found");
		return;
	}

	/* Return stats */
	if(rpc->add(ctx, "{", &th) < 0) {
		rpc->fault(ctx, 500, "Failed to create response");
		return;
	}

	if(rpc->struct_add(th, "ddddd", "test_id", test_copy.test_id,
			   "calls_active", test_copy.calls_active, "calls_started",
			   test_copy.calls_started, "calls_completed",
			   test_copy.calls_completed, "calls_failed",
			   test_copy.calls_failed)
			< 0) {
		rpc->fault(ctx, 500, "Failed to add response fields");
		return;
	}

	LM_DBG("live stats retrieved for test %d\n", test_id);
}


static const char *sipp_rpc_get_live_stats_doc[] = {
		"Get real-time statistics from SIPp stat file",
		"Parameters:", "  test_id (int, required) - Test ID to query",
		"Example:", "  kamcmd sipp.get_live_stats 1", 0};

/* RPC command exports */
/* clang-format off */
rpc_export_t sipp_rpc_cmds[] = {
	{"sipp.start_test", sipp_rpc_start_test, sipp_rpc_start_test_doc, 0},
	{"sipp.stop_test", sipp_rpc_stop_test, sipp_rpc_stop_test_doc, 0},
	{"sipp.get_stats", sipp_rpc_get_stats, sipp_rpc_get_stats_doc, 0},
	{"sipp.running", sipp_rpc_running, sipp_rpc_running_doc, RET_ARRAY},
	{"sipp.list", sipp_rpc_list, sipp_rpc_list_doc, RET_ARRAY},
	{"sipp.pause_test", sipp_rpc_pause_test, sipp_rpc_pause_test_doc, 0},
	{"sipp.resume_test", sipp_rpc_resume_test, sipp_rpc_resume_test_doc, 0},
	{"sipp.adjust_rate", sipp_rpc_adjust_rate, sipp_rpc_adjust_rate_doc, 0},
	{"sipp.get_live_stats", sipp_rpc_get_live_stats, sipp_rpc_get_live_stats_doc, 0},
	{0, 0, 0, 0}
};
/* clang-format on */


/**
 * Initialize RPC interface
 */
int sipp_rpc_init(void)
{
	if(rpc_register_array(sipp_rpc_cmds) != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	LM_DBG("RPC commands registered successfully\n");
	return 0;
}
