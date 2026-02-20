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

#ifndef _SIPP_TEST_H_
#define _SIPP_TEST_H_

#include "../../core/str.h"
#include "../../core/locking.h"

/* Test states */
#define SIPP_TEST_STATE_RUNNING 1
#define SIPP_TEST_STATE_STOPPED 2
#define SIPP_TEST_STATE_COMPLETED 3
#define SIPP_TEST_STATE_FAILED 4

/* Transport types */
#define SIPP_TRANSPORT_UDP 1
#define SIPP_TRANSPORT_TCP 2
#define SIPP_TRANSPORT_TLS 3

/* Maximum string lengths */
#define SIPP_MAX_SCENARIO_LEN 256
#define SIPP_MAX_DOMAIN_LEN 256
#define SIPP_MAX_NUMBER_LEN 64
#define SIPP_MAX_IP_LEN 64
#define SIPP_MAX_LOG_PATH_LEN 512
#define SIPP_MAX_STAT_LINE_LEN 1024


/**
 * Structure representing a single SIPp test instance
 */
typedef struct sipp_test
{
	int test_id;	   /* Unique test identifier */
	int pid;		   /* SIPp process ID */
	int state;		   /* Test state (running, stopped, completed, failed) */
	int transport;	   /* Transport type (UDP, TCP, TLS) */
	time_t start_time; /* Test start timestamp */
	time_t end_time;   /* Test end timestamp */

	/* Test configuration */
	char scenario[SIPP_MAX_SCENARIO_LEN];	 /* Scenario name */
	char from_number[SIPP_MAX_NUMBER_LEN];	 /* Caller user/number */
	char to_number[SIPP_MAX_NUMBER_LEN];	 /* Service user/number */
	char target_domain[SIPP_MAX_DOMAIN_LEN]; /* Target domain */
	char local_ip[SIPP_MAX_IP_LEN];			 /* Local IP to bind */
	int local_port;							 /* Local port (ephemeral) */
	int concurrent_calls;					 /* Number of concurrent calls */
	int call_rate;							 /* Calls per second */
	int duration;							 /* Call duration in seconds */
	int total_calls_limit; /* Max total calls (0 = unlimited) */
	int max_test_duration; /* Max test duration in seconds (0 = unlimited) */

	/* Statistics */
	int calls_started;	 /* Number of calls started */
	int calls_completed; /* Number of calls completed successfully */
	int calls_failed;	 /* Number of failed calls */
	int calls_active;	 /* Number of currently active calls */

	/* Remote control */
	int control_socket; /* Socket FD for remote control, -1 if not enabled */
	int control_port;	/* Port for remote control */

	/* Log file path */
	char log_file[SIPP_MAX_LOG_PATH_LEN];

	/* Linked list */
	struct sipp_test *next;
} sipp_test_t;


/**
 * Parameters for starting a new test
 */
typedef struct sipp_test_params
{
	str scenario;		  /* Scenario name (required) */
	str from_number;	  /* Caller user/number/range/list */
	str to_number;		  /* Service user/number/range/list */
	str target_domain;	  /* Target domain (optional, uses module default) */
	int concurrent_calls; /* Number of concurrent calls */
	int call_rate;		  /* Calls per second (0 = use default) */
	int duration;		  /* Call duration in seconds */
	int total_calls_limit; /* Max total calls (0 = unlimited, -1 = use default) */
	int max_test_duration; /* Max test duration in seconds (0 = unlimited, -1 = use default) */
} sipp_test_params_t;


/**
 * Initialize the test management system
 * @return 0 on success, -1 on error
 */
int sipp_test_init(void);

/**
 * Cleanup and destroy all tests
 */
void sipp_test_destroy(void);

/**
 * Start a new SIPp test
 * @param params Test parameters
 * @param test_id Output parameter for the new test ID
 * @return 0 on success, -1 on error
 */
int sipp_test_start(sipp_test_params_t *params, int *test_id);

/**
 * Stop a running test
 * @param test_id Test ID to stop
 * @return 0 on success, -1 on error (test not found or already stopped)
 */
int sipp_test_stop(int test_id);

/**
 * Get test information and statistics
 * @param test_id Test ID to query
 * @return Test structure pointer on success, NULL if not found
 */
sipp_test_t *sipp_test_get(int test_id);

/**
 * Get all tests (for listing)
 * NOTE: Caller must hold test lock via sipp_test_lock() for entire traversal
 * @return Pointer to first test in linked list, NULL if no tests
 */
sipp_test_t *sipp_test_get_all(void);

/**
 * Thread-safe copy of test data
 * @param test_id Test ID to query
 * @param out Output buffer for test copy (caller-allocated)
 * @return 0 on success, -1 if not found
 */
int sipp_test_get_copy(int test_id, sipp_test_t *out);

/**
 * Acquire test list lock for external iteration
 * Must be paired with sipp_test_unlock()
 */
void sipp_test_lock(void);

/**
 * Release test list lock
 */
void sipp_test_unlock(void);

/**
 * Validate string contains only safe characters (alphanumeric, _, -, ., @)
 * @param str String to validate
 * @param len Length of string
 * @return 0 if safe, -1 if contains unsafe characters
 */
int sipp_validate_safe_string(const char *str, int len);

/**
 * Validate scenario name (safe chars, no path traversal)
 * @param scenario Scenario name
 * @param len Length of scenario name
 * @return 0 if valid, -1 if invalid
 */
int sipp_validate_scenario_name(const char *scenario, int len);

/**
 * Parse number specification (single, range, or comma-separated list)
 * @param spec Number specification string
 * @param output Output buffer for parsed first number
 * @param output_len Maximum length of output buffer
 * @return 0 on success, -1 on error
 */
int sipp_parse_number_spec(str *spec, char *output, int output_len);

/**
 * Validate domain against valid_domains list
 * @param domain Domain to validate
 * @return 0 if valid, -1 if invalid
 */
int sipp_validate_domain(str *domain);

/**
 * Determine transport from socket info
 * @return Transport type (SIPP_TRANSPORT_UDP, SIPP_TRANSPORT_TCP, etc.)
 */
int sipp_get_transport(void);

/**
 * Get local IP address to bind SIPp
 * @param ip_buf Buffer to store IP address
 * @param buf_len Maximum length of buffer
 * @return 0 on success, -1 on error
 */
int sipp_get_local_ip(char *ip_buf, int buf_len);

/**
 * Update test statistics from SIPp output
 * @param test Test structure to update
 * @return 0 on success, -1 on error
 */
int sipp_test_update_stats(sipp_test_t *test);

/**
 * Update and push metrics for all running tests to Prometheus
 * Called frequently (every 5-10 seconds) by timer
 */
void sipp_metrics_update(void);

/**
 * Clean up old completed tests and remove log files
 * Called infrequently (every 1 hour) by timer
 */
void sipp_test_cleanup(void);

/**
 * Send remote control command to SIPp
 * @param test_id Test ID
 * @param cmd Command character (q=quit, p=pause, r=resume, etc.)
 * @return 0 on success, -1 on error
 */
int sipp_test_control_command(int test_id, char cmd);

/**
 * Get real-time statistics from SIPp via control socket
 * @param test_id Test ID
 * @return 0 on success, -1 on error
 */
int sipp_test_get_live_stats(int test_id);

/**
 * Adjust call rate dynamically
 * @param test_id Test ID
 * @param rate_change '+' to increase, '-' to decrease
 * @return 0 on success, -1 on error
 */
int sipp_test_adjust_rate(int test_id, char rate_change);

/**
 * Push metrics to Prometheus (if xhttp_prom is loaded)
 * @param test Test structure
 */
void sipp_push_prometheus_metrics(sipp_test_t *test);

/* Accessor functions for module parameters */
str *sipp_get_sipp_path(void);
str *sipp_get_send_socket_name(void);
str *sipp_get_send_socket(void);
str *sipp_get_target_domain(void);
str *sipp_get_default_from_number(void);
str *sipp_get_default_to_number(void);
str *sipp_get_scenario_dir(void);
str *sipp_get_work_dir(void);
str *sipp_get_media_ip(void);
str *sipp_get_valid_domains(void);
int sipp_get_default_concurrent_calls(void);
int sipp_get_max_concurrent_calls(void);
int sipp_get_max_concurrent_tests(void);
int sipp_get_default_duration(void);
int sipp_get_default_call_rate(void);
int sipp_get_default_total_calls(void);
int sipp_get_default_max_test_duration(void);
int sipp_get_enable_prometheus(void);

#endif /* _SIPP_TEST_H_ */
