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

/* Enable GNU extensions for strsep, memmem */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ctype.h>

#include "../../core/dprint.h"
#include "../../core/mem/mem.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/socket_info.h"
#include "../../core/ut.h"
#include "../../core/locking.h"

#include "sipp_test.h"
#include "sipp_prom.h"

/* Global test list (shared memory) */
static sipp_test_t **_sipp_test_list = NULL;
static gen_lock_t *_sipp_test_lock = NULL;
static int *_sipp_test_counter = NULL;


/**
 * Acquire test list lock for external iteration
 */
void sipp_test_lock(void)
{
	if(_sipp_test_lock != NULL) {
		lock_get(_sipp_test_lock);
	}
}


/**
 * Release test list lock
 */
void sipp_test_unlock(void)
{
	if(_sipp_test_lock != NULL) {
		lock_release(_sipp_test_lock);
	}
}


/**
 * Validate string contains only safe characters for shell command
 * Allows: alphanumeric, underscore, hyphen, dot, @, plus
 * Returns: 0 if safe, -1 if contains unsafe characters
 */
int sipp_validate_safe_string(const char *str, int len)
{
	int i;

	if(str == NULL || len <= 0) {
		return -1;
	}

	for(i = 0; i < len; i++) {
		unsigned char c = (unsigned char)str[i];
		if(!isalnum(c) && c != '_' && c != '-' && c != '.' && c != '@'
				&& c != '+') {
			LM_ERR("unsafe character '%c' (0x%02x) at position %d\n", c, c, i);
			return -1;
		}
	}
	return 0;
}


/**
 * Validate target_domain string
 * Allows: alphanumeric, underscore, hyphen, dot, @, plus, colon
 */
static int sipp_validate_target_domain(const char *str, int len)
{
	int i;

	if(str == NULL || len <= 0) {
		return -1;
	}

	for(i = 0; i < len; i++) {
		unsigned char c = (unsigned char)str[i];
		if(!isalnum(c) && c != '_' && c != '-' && c != '.' && c != '@'
				&& c != '+' && c != ':') {
			LM_ERR("unsafe character '%c' (0x%02x) at position %d\n", c, c, i);
			return -1;
		}
	}
	return 0;
}


/**
 * Validate scenario name (safe chars, no path traversal)
 */
int sipp_validate_scenario_name(const char *scenario, int len)
{
	if(scenario == NULL || len <= 0) {
		LM_ERR("empty scenario name\n");
		return -1;
	}

	/* Check for path traversal */
	if(len >= 2
			&& (strncmp(scenario, "..", 2) == 0
					|| memmem(scenario, len, "/..", 3) != NULL
					|| memmem(scenario, len, "../", 3) != NULL)) {
		LM_ERR("scenario name contains path traversal\n");
		return -1;
	}

	/* Cannot be absolute path */
	if(scenario[0] == '/') {
		LM_ERR("scenario name cannot be absolute path\n");
		return -1;
	}

	/* Check for safe characters - allow / for subdirectories */
	int i;
	for(i = 0; i < len; i++) {
		unsigned char c = (unsigned char)scenario[i];
		if(!isalnum(c) && c != '_' && c != '-' && c != '.' && c != '/') {
			LM_ERR("unsafe character '%c' in scenario name at position %d\n", c,
					i);
			return -1;
		}
	}

	return 0;
}


/**
 * Check if a UDP port is available for binding
 * Used to verify control port is not already in use before starting SIPp
 * @param ip IP address to bind to
 * @param port Port number to check
 * @return 0 if available, -1 if in use or error
 */
static int sipp_check_port_available(const char *ip, int port)
{
	int sock;
	struct sockaddr_in addr;
	int ret;
	int reuse = 1;

	if(ip == NULL || port <= 0 || port > 65535) {
		return -1;
	}

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(sock < 0) {
		LM_WARN("failed to create socket for port check: %s\n",
				strerror(errno));
		return -1;
	}

	/* Allow reuse to avoid TIME_WAIT issues */
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if(inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
		/* If IP parsing fails, try binding to INADDR_ANY */
		addr.sin_addr.s_addr = INADDR_ANY;
	}

	ret = bind(sock, (struct sockaddr *)&addr, sizeof(addr));
	close(sock);

	if(ret < 0) {
		LM_DBG("port %d is not available: %s\n", port, strerror(errno));
		return -1;
	}

	LM_DBG("port %d is available\n", port);
	return 0;
}


static int sipp_buf_has_token(
		const char *buf, size_t len, const char *token, size_t tlen)
{
	size_t i;

	if(buf == NULL || token == NULL || tlen == 0 || len < tlen) {
		return 0;
	}

	for(i = 0; i + tlen <= len; i++) {
		if(memcmp(buf + i, token, tlen) == 0) {
			return 1;
		}
	}

	return 0;
}


static int sipp_scenario_has_token(const char *path, const char *token)
{
	int fd;
	char buf[4096 + 64];
	ssize_t rlen;
	size_t tlen;
	size_t carry = 0;

	if(path == NULL || token == NULL) {
		return 0;
	}

	tlen = strlen(token);
	if(tlen == 0 || tlen >= sizeof(buf)) {
		return 0;
	}

	fd = open(path, O_RDONLY);
	if(fd < 0) {
		LM_WARN("failed to open scenario file for token scan: %s (%s)\n", path,
				strerror(errno));
		return 0;
	}

	while((rlen = read(fd, buf + carry, sizeof(buf) - carry)) > 0) {
		size_t total = carry + (size_t)rlen;
		if(sipp_buf_has_token(buf, total, token, tlen) == 1) {
			close(fd);
			return 1;
		}
		if(total >= tlen - 1) {
			carry = tlen - 1;
			memmove(buf, buf + total - carry, carry);
		} else {
			carry = total;
		}
	}

	close(fd);
	return 0;
}


/**
 * Initialize test management system
 */
int sipp_test_init(void)
{
	_sipp_test_lock = lock_alloc();
	if(_sipp_test_lock == NULL) {
		LM_ERR("failed to allocate lock\n");
		return -1;
	}

	if(lock_init(_sipp_test_lock) == 0) {
		LM_ERR("failed to initialize lock\n");
		lock_dealloc(_sipp_test_lock);
		_sipp_test_lock = NULL;
		return -1;
	}

	_sipp_test_list = (sipp_test_t **)shm_malloc(sizeof(sipp_test_t *));
	if(_sipp_test_list == NULL) {
		LM_ERR("failed to allocate test list head\n");
		lock_dealloc(_sipp_test_lock);
		_sipp_test_lock = NULL;
		return -1;
	}
	*_sipp_test_list = NULL;

	_sipp_test_counter = (int *)shm_malloc(sizeof(int));
	if(_sipp_test_counter == NULL) {
		LM_ERR("failed to allocate test counter\n");
		shm_free(_sipp_test_list);
		_sipp_test_list = NULL;
		lock_dealloc(_sipp_test_lock);
		_sipp_test_lock = NULL;
		return -1;
	}
	*_sipp_test_counter = 0;

	LM_DBG("test management system initialized\n");
	return 0;
}


/**
 * Cleanup and destroy all tests
 */
void sipp_test_destroy(void)
{
	sipp_test_t *test, *next;

	if(_sipp_test_lock == NULL) {
		return;
	}

	lock_get(_sipp_test_lock);

	test = (_sipp_test_list != NULL) ? *_sipp_test_list : NULL;
	while(test != NULL) {
		next = test->next;

		/* Stop running tests */
		if(test->state == SIPP_TEST_STATE_RUNNING && test->pid > 0) {
			int status;
			LM_DBG("stopping test %d (pid %d)\n", test->test_id, test->pid);
			kill(test->pid, SIGTERM);
			/* Give it 500ms to terminate gracefully */
			usleep(500000);
			/* Check if process exited */
			if(waitpid(test->pid, &status, WNOHANG) == 0) {
				/* Still running, force kill */
				LM_WARN("test %d did not respond to SIGTERM, sending SIGKILL\n",
						test->test_id);
				kill(test->pid, SIGKILL);
				waitpid(test->pid, NULL, 0);
			}
		}

		/* Close control socket if open */
		if(test->control_socket > 0) {
			close(test->control_socket);
			test->control_socket = -1;
		}

		/* Remove log file */
		if(test->log_file[0] != '\0') {
			char extra_file[SIPP_MAX_LOG_PATH_LEN + 16];

			if(unlink(test->log_file) < 0) {
				LM_DBG("failed to remove log file %s: %s\n", test->log_file,
						strerror(errno));
			} else {
				LM_DBG("removed log file %s\n", test->log_file);
			}

			snprintf(extra_file, sizeof(extra_file), "%s.exec.log",
					test->log_file);
			if(unlink(extra_file) < 0 && errno != ENOENT) {
				LM_DBG("failed to remove exec log %s: %s\n", extra_file,
						strerror(errno));
			}

			snprintf(extra_file, sizeof(extra_file), "%s.stat.csv",
					test->log_file);
			if(unlink(extra_file) < 0 && errno != ENOENT) {
				LM_DBG("failed to remove stats file %s: %s\n", extra_file,
						strerror(errno));
			}

			snprintf(extra_file, sizeof(extra_file), "%s.errors.log",
					test->log_file);
			if(unlink(extra_file) < 0 && errno != ENOENT) {
				LM_DBG("failed to remove errors log %s: %s\n", extra_file,
						strerror(errno));
			}
		}

		shm_free(test);
		test = next;
	}

	if(_sipp_test_list != NULL) {
		*_sipp_test_list = NULL;
	}
	lock_release(_sipp_test_lock);

	lock_destroy(_sipp_test_lock);
	lock_dealloc(_sipp_test_lock);
	_sipp_test_lock = NULL;

	if(_sipp_test_list != NULL) {
		shm_free(_sipp_test_list);
		_sipp_test_list = NULL;
	}
	if(_sipp_test_counter != NULL) {
		shm_free(_sipp_test_counter);
		_sipp_test_counter = NULL;
	}

	LM_DBG("test management system destroyed\n");
}


/**
 * Get transport type from module configuration
 */
int sipp_get_transport(void)
{
	str *send_socket_name = sipp_get_send_socket_name();
	str *send_socket = sipp_get_send_socket();
	socket_info_t *si = NULL;

	/* Try send_socket_name first */
	if(send_socket_name != NULL && send_socket_name->s != NULL
			&& send_socket_name->len > 0) {
		si = lookup_local_socket(send_socket_name);
		if(si != NULL) {
			switch(si->proto) {
				case PROTO_UDP:
					return SIPP_TRANSPORT_UDP;
				case PROTO_TCP:
					return SIPP_TRANSPORT_TCP;
				case PROTO_TLS:
					return SIPP_TRANSPORT_TLS;
				default:
					LM_WARN("unknown protocol %d, defaulting to UDP\n",
							si->proto);
					return SIPP_TRANSPORT_UDP;
			}
		}
	}

	/* Try send_socket (raw format) */
	if(send_socket != NULL && send_socket->s != NULL && send_socket->len > 0) {
		/* Parse protocol from <proto>:<ip>:<port> */
		if(send_socket->len >= 3) {
			if(strncasecmp(send_socket->s, "udp", 3) == 0) {
				return SIPP_TRANSPORT_UDP;
			} else if(strncasecmp(send_socket->s, "tcp", 3) == 0) {
				return SIPP_TRANSPORT_TCP;
			} else if(strncasecmp(send_socket->s, "tls", 3) == 0) {
				return SIPP_TRANSPORT_TLS;
			}
		}
	}

	/* Default to UDP */
	LM_DBG("no transport specified, defaulting to UDP\n");
	return SIPP_TRANSPORT_UDP;
}


/**
 * Get local IP address to bind SIPp
 */
int sipp_get_local_ip(char *ip_buf, int buf_len)
{
	str *send_socket_name = sipp_get_send_socket_name();
	str *send_socket = sipp_get_send_socket();
	socket_info_t *si = NULL;

	if(ip_buf == NULL || buf_len <= 0) {
		LM_ERR("invalid buffer\n");
		return -1;
	}

	/* Try send_socket_name first */
	if(send_socket_name != NULL && send_socket_name->s != NULL
			&& send_socket_name->len > 0) {
		si = lookup_local_socket(send_socket_name);
		if(si != NULL) {
			char *ip_str = ip_addr2a(&si->address);
			if(ip_str != NULL) {
				snprintf(ip_buf, buf_len, "%s", ip_str);
				LM_DBG("resolved local IP from send_socket_name: %s\n", ip_buf);
				return 0;
			}
		}
	}

	/* Try send_socket (raw format: <proto>:<ip>:<port>) */
	if(send_socket != NULL && send_socket->s != NULL && send_socket->len > 0) {
		char *p = send_socket->s;
		char *end = p + send_socket->len;
		char *colon1 = memchr(p, ':', end - p);

		if(colon1 != NULL) {
			char *colon2 = memchr(colon1 + 1, ':', end - (colon1 + 1));
			if(colon2 != NULL) {
				int ip_len = colon2 - (colon1 + 1);
				if(ip_len > 0 && ip_len < buf_len) {
					memcpy(ip_buf, colon1 + 1, ip_len);
					ip_buf[ip_len] = '\0';
					LM_DBG("parsed local IP from send_socket: %s\n", ip_buf);
					return 0;
				}
			}
		}
	}

	LM_ERR("failed to determine local IP address\n");
	return -1;
}


/**
 * Get local port from send_socket configuration
 * Returns the port number, or 5060 as default
 */
int sipp_get_local_port(void)
{
	str *send_socket_name = sipp_get_send_socket_name();
	str *send_socket = sipp_get_send_socket();
	socket_info_t *si = NULL;

	/* Try send_socket_name first */
	if(send_socket_name != NULL && send_socket_name->s != NULL
			&& send_socket_name->len > 0) {
		si = lookup_local_socket(send_socket_name);
		if(si != NULL && si->port_no > 0) {
			LM_DBG("resolved local port from send_socket_name: %d\n",
					si->port_no);
			return si->port_no;
		}
	}

	/* Try send_socket (raw format: <proto>:<ip>:<port>) */
	if(send_socket != NULL && send_socket->s != NULL && send_socket->len > 0) {
		char *p = send_socket->s;
		char *end = p + send_socket->len;
		char *colon1 = memchr(p, ':', end - p);

		if(colon1 != NULL) {
			char *colon2 = memchr(colon1 + 1, ':', end - (colon1 + 1));
			if(colon2 != NULL && colon2 + 1 < end) {
				int port = atoi(colon2 + 1);
				if(port > 0 && port <= 65535) {
					LM_DBG("parsed local port from send_socket: %d\n", port);
					return port;
				}
			}
		}
	}

	/* Default to 5060 */
	LM_DBG("no port specified, defaulting to 5060\n");
	return 5060;
}


/**
 * Parse number specification
 * Supports: single number (1000), range (1000-1020), comma-separated (1000,1001,1002)
 */
int sipp_parse_number_spec(str *spec, char *output, int output_len)
{
	if(spec == NULL || spec->s == NULL || spec->len <= 0) {
		LM_ERR("invalid number specification\n");
		return -1;
	}

	if(output == NULL || output_len <= 0) {
		LM_ERR("invalid output buffer\n");
		return -1;
	}

	/* For simplicity, we'll use the first number/username from the spec */
	/* In a production implementation, this would need to handle ranges and lists */

	/* Check for range (e.g., 1000-1020) */
	char *dash = memchr(spec->s, '-', spec->len);
	if(dash != NULL) {
		/* Extract first number */
		int len = dash - spec->s;
		if(len > 0 && len < output_len) {
			memcpy(output, spec->s, len);
			output[len] = '\0';
			LM_DBG("parsed range start: %s\n", output);
			return 0;
		}
	}

	/* Check for comma-separated list (e.g., 1000,1001,1002) */
	char *comma = memchr(spec->s, ',', spec->len);
	if(comma != NULL) {
		/* Extract first number */
		int len = comma - spec->s;
		if(len > 0 && len < output_len) {
			memcpy(output, spec->s, len);
			output[len] = '\0';
			LM_DBG("parsed list first: %s\n", output);
			return 0;
		}
	}

	/* Single number or username */
	if(spec->len < output_len) {
		memcpy(output, spec->s, spec->len);
		output[spec->len] = '\0';
		LM_DBG("parsed single value: %s\n", output);
		return 0;
	}

	LM_ERR("number specification too long\n");
	return -1;
}


/**
 * Validate domain against valid_domains list
 */
int sipp_validate_domain(str *domain)
{
	str *valid_domains = sipp_get_valid_domains();

	if(domain == NULL || domain->s == NULL || domain->len <= 0) {
		LM_DBG("empty domain, skipping validation\n");
		return 0;
	}

	/* If no valid_domains list, allow any domain */
	if(valid_domains == NULL || valid_domains->s == NULL
			|| valid_domains->len <= 0) {
		LM_DBG("no valid_domains restriction, allowing domain: %.*s\n",
				domain->len, domain->s);
		return 0;
	}

	/* Check if domain is in the comma-separated list */
	char *p = valid_domains->s;
	char *end = p + valid_domains->len;

	while(p < end) {
		/* Skip whitespace */
		while(p < end && (*p == ' ' || *p == '\t')) {
			p++;
		}

		/* Find next comma or end */
		char *comma = memchr(p, ',', end - p);
		char *domain_end = (comma != NULL) ? comma : end;

		/* Trim trailing whitespace */
		while(domain_end > p
				&& (*(domain_end - 1) == ' ' || *(domain_end - 1) == '\t')) {
			domain_end--;
		}

		/* Compare domain */
		int len = domain_end - p;
		if(len == domain->len && strncasecmp(p, domain->s, len) == 0) {
			LM_DBG("domain validated: %.*s\n", domain->len, domain->s);
			return 0;
		}

		/* Move to next domain */
		p = (comma != NULL) ? comma + 1 : end;
	}

	LM_ERR("domain not in valid_domains list: %.*s\n", domain->len, domain->s);
	return -1;
}


/**
 * Build SIPp command line
 */
static int sipp_build_command(sipp_test_t *test, char *cmd_buf, int buf_len)
{
	str *scenario_dir = sipp_get_scenario_dir();
	str *sipp_path = sipp_get_sipp_path();
	str *media_ip = sipp_get_media_ip();
	const char *sipp_cmd = "sipp"; /* Default if not configured */
	int transport = test->transport;
	const char *transport_arg = "";
	char scenario_path[1024];
	char set_from[256];
	char set_domain[256];
	char set_media_ip[256];
	char max_calls_arg[64];
	int has_from = 0;
	int has_domain = 0;
	size_t domain_len = 0;

	/* Use configured sipp_path if available */
	if(sipp_path != NULL && sipp_path->s != NULL && sipp_path->len > 0) {
		sipp_cmd = sipp_path->s;
	}

	/* Determine transport argument for SIPp */
	switch(transport) {
		case SIPP_TRANSPORT_TCP:
			transport_arg = "-t tn";
			break;
		case SIPP_TRANSPORT_TLS:
			transport_arg = "-t ln";
			break;
		case SIPP_TRANSPORT_UDP:
		default:
			transport_arg = "-t un";
			break;
	}

	/* Build SIPp command:
	 * sipp -sf <scenario_file> -s <service>
	 *      -i <local_ip> -p <local_port>
	 *      -r <call_rate> -l <concurrent_calls>
	 *      -d <duration> [-m <max_calls>]
	 *      -key caller <caller>
	 *      -key target_domain <target_domain>
	 *      -trace_err
	 *      -bg -log_file <log_file>
	 *      [-ci <local_ip> -cp <control_port>]  (if remote control enabled)
	 *      <local_ip>:5060
	 *
	 * Note: The positional target (<local_ip>:5060) tells SIPp where to send traffic.
	 * This should be Kamailio's listening address (from send_socket config).
	 * The target_domain variable is used within scenarios for the domain part of SIP URIs.
	 * The control socket (-ci/-cp) enables real-time control and monitoring.
	 */

	snprintf(scenario_path, sizeof(scenario_path), "%.*s/%s", scenario_dir->len,
			scenario_dir->s, test->scenario);

	has_from = sipp_scenario_has_token(scenario_path, "[caller]")
			   || sipp_scenario_has_token(scenario_path, "[$caller]");
	has_domain = sipp_scenario_has_token(scenario_path, "[target_domain]")
				 || sipp_scenario_has_token(scenario_path, "[$target_domain]");

	if(has_from) {
		snprintf(set_from, sizeof(set_from), "-key caller %s ",
				test->from_number);
	} else {
		set_from[0] = '\0';
	}

	if(has_domain) {
		domain_len = strnlen(test->target_domain, sizeof(test->target_domain));
		snprintf(set_domain, sizeof(set_domain), "-key target_domain %.*s ",
				(int)domain_len, test->target_domain);
	} else {
		set_domain[0] = '\0';
	}

	if(media_ip != NULL && media_ip->s != NULL && media_ip->len > 0) {
		snprintf(set_media_ip, sizeof(set_media_ip), "-mi %.*s ", media_ip->len,
				media_ip->s);
	} else {
		set_media_ip[0] = '\0';
	}

	if(test->total_calls_limit > 0) {
		snprintf(max_calls_arg, sizeof(max_calls_arg), "-m %d ",
				test->total_calls_limit);
	} else {
		max_calls_arg[0] = '\0';
	}

	char set_service[256];
	snprintf(set_service, sizeof(set_service), "-s %s ", test->to_number);

	char control_args[256] = "";
	if(test->control_port > 0) {
		snprintf(control_args, sizeof(control_args), "-ci %s -cp %d ",
				test->local_ip, test->control_port);
	}

	/* Get the target port from send_socket config (defaults to 5060) */
	int target_port = sipp_get_local_port();
	char error_file[SIPP_MAX_LOG_PATH_LEN + 16];
	int err_ret = snprintf(
			error_file, sizeof(error_file), "%s.errors.log", test->log_file);
	if(err_ret < 0 || err_ret >= (int)sizeof(error_file)) {
		LM_ERR("error log path too long\n");
		return -1;
	}

	int n = snprintf(cmd_buf, buf_len,
			"%s -sf %.*s/%s "
			"%s"
			"-i %s -p %d "
			"%s"
			"%s "
			"-r %d -l %d "
			"-d %d "
			"%s"
			"%s"
			"%s"
			"%s"
			"-stf %s.stat.csv -fd 1 "
			"-error_file %s "
			"-trace_err -trace_stat "
			"-bg "
			"-log_file %s "
			"%s:%d",
			sipp_cmd, scenario_dir->len, scenario_dir->s, test->scenario,
			set_service, test->local_ip, test->local_port, set_media_ip,
			transport_arg,
			test->call_rate, test->concurrent_calls,
			test->duration * 1000, /* Convert to ms */
			max_calls_arg, set_from, set_domain, control_args, test->log_file,
			error_file, test->log_file, test->local_ip,
			target_port); /* Send to Kamailio's listening address */

	if(n < 0 || n >= buf_len) {
		LM_ERR("command buffer too small\n");
		return -1;
	}

	LM_DBG("SIPp command: %s\n", cmd_buf);
	return 0;
}


/**
 * Start a new SIPp test
 */
int sipp_test_start(sipp_test_params_t *params, int *test_id)
{
	sipp_test_t *test = NULL;
	sipp_test_t *t = NULL;
	char cmd_buf[2048];
	str *sipp_path = NULL;
	int transport;
	char local_ip[SIPP_MAX_IP_LEN];
	str *target_domain = NULL;
	char fallback_domain[SIPP_MAX_DOMAIN_LEN];
	str fallback_domain_str = STR_NULL;
	str *default_from = NULL;
	str *default_to = NULL;
	str *from_to_use = NULL;
	str *to_to_use = NULL;
	int max_calls;
	int running_tests = 0;
	int max_tests;
	int target_port = 0;
	str *scenario_dir = NULL;
	char scenario_path[1024];

	if(params == NULL || test_id == NULL) {
		LM_ERR("invalid parameters\n");
		return -1;
	}

	/* ============================================================
	 * PHASE 1: Validation BEFORE acquiring lock or allocating memory
	 * This ensures we don't leak resources on validation failures
	 * ============================================================ */

	/* Validate scenario name length */
	if(params->scenario.s == NULL || params->scenario.len <= 0
			|| params->scenario.len >= SIPP_MAX_SCENARIO_LEN) {
		LM_ERR("invalid scenario name\n");
		return -1;
	}

	/* Validate scenario name for command injection and path traversal */
	if(sipp_validate_scenario_name(params->scenario.s, params->scenario.len)
			< 0) {
		LM_ERR("scenario name contains unsafe characters or path traversal\n");
		return -1;
	}

	/* Validate caller availability and safety */
	if(params->from_number.s != NULL && params->from_number.len > 0) {
		if(sipp_validate_safe_string(
				   params->from_number.s, params->from_number.len)
				< 0) {
			LM_ERR("caller contains unsafe characters\n");
			return -1;
		}
		from_to_use = &params->from_number;
	} else {
		default_from = sipp_get_default_from_number();
		if(default_from == NULL || default_from->s == NULL
				|| default_from->len <= 0) {
			LM_ERR("caller not provided and default_caller not set\n");
			return -1;
		}
		from_to_use = default_from;
	}

	/* Validate service availability and safety */
	if(params->to_number.s != NULL && params->to_number.len > 0) {
		if(sipp_validate_safe_string(params->to_number.s, params->to_number.len)
				< 0) {
			LM_ERR("service contains unsafe characters\n");
			return -1;
		}
		to_to_use = &params->to_number;
	} else {
		default_to = sipp_get_default_to_number();
		if(default_to == NULL || default_to->s == NULL
				|| default_to->len <= 0) {
			LM_ERR("service not provided and default_service not set\n");
			return -1;
		}
		to_to_use = default_to;
	}

	/* Validate target_domain if provided */
	if(params->target_domain.s != NULL && params->target_domain.len > 0) {
		if(sipp_validate_target_domain(
				   params->target_domain.s, params->target_domain.len)
				< 0) {
			LM_ERR("target_domain contains unsafe characters\n");
			return -1;
		}
	}

	/* Check max concurrent tests limit */
	max_tests = sipp_get_max_concurrent_tests();
	lock_get(_sipp_test_lock);
	for(t = (_sipp_test_list != NULL) ? *_sipp_test_list : NULL; t != NULL;
			t = t->next) {
		if(t->state == SIPP_TEST_STATE_RUNNING) {
			running_tests++;
		}
	}
	lock_release(_sipp_test_lock);

	if(running_tests >= max_tests) {
		LM_ERR("max concurrent tests limit reached (%d/%d)\n", running_tests,
				max_tests);
		return -1;
	}

	/* Validate concurrent calls */
	max_calls = sipp_get_max_concurrent_calls();
	if(params->concurrent_calls > max_calls) {
		LM_ERR("concurrent_calls (%d) exceeds max_concurrent_calls (%d)\n",
				params->concurrent_calls, max_calls);
		return -1;
	}

	/* Validate total_calls_limit and max_test_duration (allow -1 = use default) */
	if(params->total_calls_limit < -1) {
		LM_ERR("total_calls_limit must be >= 0 (or -1 for default)\n");
		return -1;
	}
	if(params->max_test_duration < -1) {
		LM_ERR("max_test_duration must be >= 0 (or -1 for default)\n");
		return -1;
	}

	/* Determine target domain */
	if(params->target_domain.s != NULL && params->target_domain.len > 0) {
		/* Validate domain */
		if(sipp_validate_domain(&params->target_domain) < 0) {
			LM_ERR("invalid target domain\n");
			return -1;
		}
		target_domain = &params->target_domain;
	} else {
		target_domain = sipp_get_target_domain();
	}

	/* If scenario uses [target_domain], ensure it is set */

	scenario_dir = sipp_get_scenario_dir();
	if(scenario_dir != NULL && scenario_dir->s != NULL
			&& scenario_dir->len > 0) {
		int ret = snprintf(scenario_path, sizeof(scenario_path), "%.*s/%.*s",
				scenario_dir->len, scenario_dir->s, params->scenario.len,
				params->scenario.s);
		if(ret < 0 || ret >= (int)sizeof(scenario_path)) {
			LM_ERR("scenario path too long (max %zu bytes)\n",
					sizeof(scenario_path) - 1);
			return -1;
		}

		/* Get local IP and target port for target_domain fallback */
		if(sipp_get_local_ip(local_ip, sizeof(local_ip)) < 0) {
			LM_ERR("failed to get local IP\n");
			return -1;
		}
		target_port = sipp_get_local_port();

		if(sipp_scenario_has_token(scenario_path, "[target_domain]")
				|| sipp_scenario_has_token(scenario_path, "[$target_domain]")) {
			if(target_domain == NULL || target_domain->s == NULL
					|| target_domain->len <= 0) {
				int fd_ret = snprintf(fallback_domain, sizeof(fallback_domain),
						"%s:%d", local_ip, target_port);
				if(fd_ret < 0 || fd_ret >= (int)sizeof(fallback_domain)) {
					LM_ERR("fallback target_domain too long\n");
					return -1;
				}
				fallback_domain_str.s = fallback_domain;
				fallback_domain_str.len = fd_ret;
				target_domain = &fallback_domain_str;
				LM_DBG("target_domain not set; using fallback %s\n",
						fallback_domain);
			}
		}
	}

	/* Get transport type */
	transport = sipp_get_transport();

	/* Allocate test structure */
	test = (sipp_test_t *)shm_malloc(sizeof(sipp_test_t));
	if(test == NULL) {
		LM_ERR("failed to allocate test structure\n");
		return -1;
	}
	memset(test, 0, sizeof(sipp_test_t));

	/* Acquire lock and assign test ID */
	lock_get(_sipp_test_lock);
	if(_sipp_test_counter == NULL) {
		LM_ERR("test counter not initialized\n");
		lock_release(_sipp_test_lock);
		shm_free(test);
		return -1;
	}
	(*_sipp_test_counter)++;

	/* Check for test_id overflow */
	if(*_sipp_test_counter <= 0) {
		LM_ERR("test_id counter overflow\n");
		lock_release(_sipp_test_lock);
		shm_free(test);
		return -1;
	}

	test->test_id = *_sipp_test_counter;
	*test_id = test->test_id;

	/* Initialize test structure */
	test->state = SIPP_TEST_STATE_RUNNING;
	test->transport = transport;
	test->start_time = time(NULL);
	test->end_time = 0;
	test->control_socket = -1; /* Explicitly initialize to invalid */

	/* Copy scenario name */
	snprintf(test->scenario, sizeof(test->scenario), "%.*s",
			params->scenario.len, params->scenario.s);

	/* Parse and copy caller (already validated in Phase 1) */
	if(sipp_parse_number_spec(
			   from_to_use, test->from_number, sizeof(test->from_number))
			< 0) {
		LM_ERR("failed to parse caller\n");
		lock_release(_sipp_test_lock);
		shm_free(test);
		return -1;
	}

	/* Parse and copy service (already validated in Phase 1) */
	if(sipp_parse_number_spec(
			   to_to_use, test->to_number, sizeof(test->to_number))
			< 0) {
		LM_ERR("failed to parse service\n");
		lock_release(_sipp_test_lock);
		shm_free(test);
		return -1;
	}

	/* Copy target domain (may be empty) */
	if(target_domain != NULL && target_domain->s != NULL
			&& target_domain->len > 0) {
		snprintf(test->target_domain, sizeof(test->target_domain), "%.*s",
				target_domain->len, target_domain->s);
	} else {
		test->target_domain[0] = '\0';
	}

	/* Copy local IP */
	snprintf(test->local_ip, sizeof(test->local_ip), "%s", local_ip);

	/* Use ephemeral port (0 = let OS assign) */
	test->local_port = 0;

	/* Setup remote control socket
	 * SIPp's default control port base is 8888, and it supports up to 60 instances.
	 * We use -cp to explicitly set the port based on test_id.
	 * Port range: 8888 + test_id (so 8889, 8890, ..., up to 8888+test_id)
	 */
#define SIPP_CONTROL_PORT_BASE 8888
	test->control_port = SIPP_CONTROL_PORT_BASE + test->test_id;

	/* Validate port range - shouldn't happen with max 60 tests, but check anyway */
	if(test->control_port > 65535) {
		LM_ERR("control port %d exceeds max port 65535 (test_id=%d too "
			   "large)\n",
				test->control_port, test->test_id);
		lock_release(_sipp_test_lock);
		shm_free(test);
		return -1;
	}

	/* Check if control port is available before starting SIPp
	 * This prevents confusing failures when another process is using the port
	 */
	if(sipp_check_port_available(local_ip, test->control_port) < 0) {
		LM_ERR("control port %d is already in use - cannot start test\n",
				test->control_port);
		LM_ERR("another SIPp instance or process may be using this port\n");
		LM_DBG("hint: stop conflicting process or wait for test_id to "
			   "increment "
			   "past the conflict\n");
		lock_release(_sipp_test_lock);
		shm_free(test);
		return -1;
	}

	/* Validate scenario file exists */
	if(scenario_dir != NULL && scenario_dir->s != NULL
			&& scenario_dir->len > 0) {
		if(access(scenario_path, F_OK) < 0) {
			LM_ERR("scenario file not found: %s\n", scenario_path);
			lock_release(_sipp_test_lock);
			shm_free(test);
			return -1;
		}
	}


	/* Copy test parameters */
	test->concurrent_calls = params->concurrent_calls;
	test->call_rate = params->call_rate > 0 ? params->call_rate
											: sipp_get_default_call_rate();
	test->duration = params->duration;
	if(params->total_calls_limit >= 0) {
		test->total_calls_limit = params->total_calls_limit;
	} else {
		test->total_calls_limit = sipp_get_default_total_calls();
	}
	if(params->max_test_duration >= 0) {
		test->max_test_duration = params->max_test_duration;
	} else {
		test->max_test_duration = sipp_get_default_max_test_duration();
	}

	/* Generate log file path with bounds checking */
	{
		str *work_dir = sipp_get_work_dir();
		if(work_dir != NULL && work_dir->s != NULL && work_dir->len > 0) {
			int log_ret = snprintf(test->log_file, sizeof(test->log_file),
					"%.*s/sipp_test_%d_%ld.log", work_dir->len, work_dir->s,
					test->test_id, (long)test->start_time);
			if(log_ret < 0 || log_ret >= (int)sizeof(test->log_file)) {
				LM_ERR("log file path too long\n");
				lock_release(_sipp_test_lock);
				shm_free(test);
				return -1;
			}
		} else {
			int log_ret = snprintf(test->log_file, sizeof(test->log_file),
					"/tmp/sipp_test_%d_%ld.log", test->test_id,
					(long)test->start_time);
			if(log_ret < 0 || log_ret >= (int)sizeof(test->log_file)) {
				LM_ERR("log file path too long\n");
				lock_release(_sipp_test_lock);
				shm_free(test);
				return -1;
			}
		}
	}

	/* Add to test list */
	if(_sipp_test_list != NULL) {
		test->next = *_sipp_test_list;
		*_sipp_test_list = test;
	} else {
		test->next = NULL;
	}

	lock_release(_sipp_test_lock);

	/* Build SIPp command */
	if(sipp_build_command(test, cmd_buf, sizeof(cmd_buf)) < 0) {
		LM_ERR("failed to build SIPp command\n");
		test->state = SIPP_TEST_STATE_FAILED;
		test->end_time = time(NULL); /* Mark end time for cleanup */
		return -1;
	}

	/* Validate configured sipp_path (if set) */
	sipp_path = sipp_get_sipp_path();
	if(sipp_path != NULL && sipp_path->s != NULL && sipp_path->len > 0) {
		if(access(sipp_path->s, X_OK) < 0) {
			LM_ERR("sipp_path not executable: %.*s (%s)\n", sipp_path->len,
					sipp_path->s, strerror(errno));
			test->state = SIPP_TEST_STATE_FAILED;
			test->end_time = time(NULL); /* Mark end time for cleanup */
			return -1;
		}
	}

	/* Fork and execute SIPp */
	pid_t pid = fork();
	if(pid < 0) {
		LM_ERR("fork failed: %s\n", strerror(errno));
		test->state = SIPP_TEST_STATE_FAILED;
		test->end_time = time(NULL); /* Mark end time for cleanup */
		return -1;
	}

	if(pid == 0) {
		/* Child process */
		int null_fd = -1;
		int log_fd = -1;
		char exec_log[512];
		str *work_dir = sipp_get_work_dir();

		/* Reset signal handlers to default - don't inherit Kamailio's handlers */
		signal(SIGTERM, SIG_DFL);
		signal(SIGINT, SIG_DFL);
		signal(SIGHUP, SIG_DFL);
		signal(SIGCHLD, SIG_DFL);
		signal(SIGPIPE, SIG_DFL);

		if(work_dir != NULL && work_dir->s != NULL && work_dir->len > 0) {
			if(chdir(work_dir->s) < 0) {
				LM_WARN("failed to chdir to work_dir %.*s: %s\n", work_dir->len,
						work_dir->s, strerror(errno));
			}
		}

		null_fd = open("/dev/null", O_RDONLY);
		if(null_fd >= 0) {
			dup2(null_fd, 0);
			if(null_fd > 2)
				close(null_fd);
		}

		{
			int max_len = (int)sizeof(exec_log) - 1 - (int)strlen(".exec.log");
			if(max_len < 0) {
				max_len = 0;
			}
			snprintf(exec_log, sizeof(exec_log), "%.*s.exec.log", max_len,
					test->log_file);
		}
		log_fd = open(exec_log, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if(log_fd >= 0) {
			dup2(log_fd, 1);
			dup2(log_fd, 2);
			if(log_fd > 2)
				close(log_fd);
		} else {
			LM_WARN("failed to open exec log %s: %s\n", exec_log,
					strerror(errno));
		}

		/* Execute SIPp */
		execl("/bin/sh", "sh", "-c", cmd_buf, (char *)NULL);

		/* If we get here, exec failed */
		dprintf(2, "execl failed: %s\n", strerror(errno));
		exit(1);
	}

	/* Parent process */
	test->pid = pid;

	/* Check if SIPp exited immediately (capture exec log in Kamailio logs) */
	{
		int status = 0;
		pid_t wpid;
		char exec_log[512];
		char buf[2048];
		int fd;
		ssize_t rlen;

		usleep(100000); /* 100ms */
		wpid = waitpid(test->pid, &status, WNOHANG);
		if(wpid == test->pid) {
			int bg_pid = -1;
			{
				int max_len =
						(int)sizeof(exec_log) - 1 - (int)strlen(".exec.log");
				if(max_len < 0) {
					max_len = 0;
				}
				snprintf(exec_log, sizeof(exec_log), "%.*s.exec.log", max_len,
						test->log_file);
			}
			fd = open(exec_log, O_RDONLY);
			if(fd >= 0) {
				rlen = read(fd, buf, sizeof(buf) - 1);
				if(rlen > 0) {
					buf[rlen] = '\0';
					{
						char *pid_start = strstr(buf, "PID=[");
						if(pid_start != NULL) {
							pid_start += 5;
							bg_pid = atoi(pid_start);
						}
					}
					if(bg_pid > 0) {
						LM_DBG("SIPp background mode detected for test %d: "
							   "parent pid=%d, bg pid=%d\n",
								test->test_id, (int)test->pid, bg_pid);
					} else {
						LM_WARN("SIPp exited immediately (pid %d). "
								"Output:\n%s\n",
								(int)test->pid, buf);
					}
				} else {
					LM_ERR("SIPp exited immediately (pid %d). No output "
						   "captured.\n",
							(int)test->pid);
				}
				close(fd);
				if(unlink(exec_log) < 0 && errno != ENOENT) {
					LM_DBG("failed to remove exec log %s: %s\n", exec_log,
							strerror(errno));
				}
			} else {
				LM_ERR("SIPp exited immediately (pid %d). Exec log missing "
					   "(%s).\n",
						(int)test->pid, strerror(errno));
			}

			if(bg_pid > 0) {
				test->pid = bg_pid;
				return 0;
			}
			if(test->log_file[0] != '\0') {
				char extra_file[SIPP_MAX_LOG_PATH_LEN + 16];

				if(unlink(test->log_file) < 0 && errno != ENOENT) {
					LM_DBG("failed to remove log file %s: %s\n", test->log_file,
							strerror(errno));
				}

				snprintf(extra_file, sizeof(extra_file), "%s.exec.log",
						test->log_file);
				if(unlink(extra_file) < 0 && errno != ENOENT) {
					LM_DBG("failed to remove exec log %s: %s\n", extra_file,
							strerror(errno));
				}

				snprintf(extra_file, sizeof(extra_file), "%s.stat.csv",
						test->log_file);
				if(unlink(extra_file) < 0 && errno != ENOENT) {
					LM_DBG("failed to remove stats file %s: %s\n", extra_file,
							strerror(errno));
				}
			}
			test->state = SIPP_TEST_STATE_FAILED;
			test->end_time = time(NULL); /* Mark end time for cleanup */
			return -1;
		}
	}

	LM_INFO("started SIPp test %d (pid %d): scenario=%s, caller=%s, "
			"service=%s, "
			"domain=%s, calls=%d, duration=%d, total_calls_limit=%d, "
			"max_test_duration=%d\n",
			test->test_id, test->pid, test->scenario, test->from_number,
			test->to_number, test->target_domain, test->concurrent_calls,
			test->duration, test->total_calls_limit, test->max_test_duration);

	return 0;
}


/**
 * Stop a running test
 */
int sipp_test_stop(int test_id)
{
	sipp_test_t *test = NULL;
	int found = 0;
	int pid = 0;
	int status = 0;
	int sent_quit = 0;
	int i = 0;

	lock_get(_sipp_test_lock);

	/* Find test */
	test = (_sipp_test_list != NULL) ? *_sipp_test_list : NULL;
	while(test != NULL) {
		if(test->test_id == test_id) {
			found = 1;
			break;
		}
		test = test->next;
	}

	if(!found) {
		lock_release(_sipp_test_lock);
		LM_ERR("test %d not found\n", test_id);
		return -1;
	}

	/* Check if test is running */
	if(test->state != SIPP_TEST_STATE_RUNNING) {
		lock_release(_sipp_test_lock);
		LM_WARN("test %d is not running (state=%d)\n", test_id, test->state);
		return -1;
	}

	pid = test->pid;
	lock_release(_sipp_test_lock);

	if(pid > 0) {
		if(sipp_test_control_command(test_id, 'Q') == 0) {
			sent_quit = 1;
		}
	}

	if(sent_quit && pid > 0) {
		for(i = 0; i < 10; i++) {
			if(waitpid(pid, &status, WNOHANG) > 0) {
				lock_get(_sipp_test_lock);
				test = (_sipp_test_list != NULL) ? *_sipp_test_list : NULL;
				while(test != NULL) {
					if(test->test_id == test_id) {
						test->state = SIPP_TEST_STATE_STOPPED;
						test->end_time = time(NULL);
						break;
					}
					test = test->next;
				}
				lock_release(_sipp_test_lock);
				return 0;
			}
			usleep(200000);
		}
	}

	lock_get(_sipp_test_lock);
	found = 0;
	test = (_sipp_test_list != NULL) ? *_sipp_test_list : NULL;
	while(test != NULL) {
		if(test->test_id == test_id) {
			found = 1;
			break;
		}
		test = test->next;
	}

	if(!found) {
		lock_release(_sipp_test_lock);
		LM_ERR("test %d not found\n", test_id);
		return -1;
	}

	if(test->state != SIPP_TEST_STATE_RUNNING) {
		lock_release(_sipp_test_lock);
		return 0;
	}

	/* Send SIGTERM to SIPp process and wait briefly */
	if(test->pid > 0) {
		LM_INFO("stopping test %d (pid %d)\n", test_id, test->pid);
		if(kill(test->pid, SIGTERM) < 0) {
			if(errno == ESRCH) {
				LM_DBG("SIGTERM skipped; pid %d no longer exists\n", test->pid);
			} else {
				LM_WARN("failed to send SIGTERM to pid %d: %s\n", test->pid,
						strerror(errno));
			}
		}
		/* Give process 500ms to terminate gracefully */
		usleep(500000);
		/* Send SIGKILL if still running */
		if(waitpid(test->pid, &status, WNOHANG) == 0) {
			LM_WARN("test %d did not terminate, sending SIGKILL\n", test_id);
			kill(test->pid, SIGKILL);
			waitpid(test->pid, NULL, 0);
		}
	}

	test->state = SIPP_TEST_STATE_STOPPED;
	test->end_time = time(NULL);

	lock_release(_sipp_test_lock);

	return 0;
}


/**
 * Get test information
 * WARNING: Returns pointer without lock - use sipp_test_get_copy() for thread-safety
 */
sipp_test_t *sipp_test_get(int test_id)
{
	sipp_test_t *test = NULL;

	lock_get(_sipp_test_lock);

	test = (_sipp_test_list != NULL) ? *_sipp_test_list : NULL;
	while(test != NULL) {
		if(test->test_id == test_id) {
			lock_release(_sipp_test_lock);
			return test;
		}
		test = test->next;
	}

	lock_release(_sipp_test_lock);
	return NULL;
}


/**
 * Thread-safe copy of test data
 * @param test_id Test ID to query
 * @param out Output buffer for test copy (caller-allocated)
 * @return 0 on success, -1 if not found
 */
int sipp_test_get_copy(int test_id, sipp_test_t *out)
{
	sipp_test_t *test = NULL;
	int found = 0;

	if(out == NULL) {
		return -1;
	}

	lock_get(_sipp_test_lock);

	test = (_sipp_test_list != NULL) ? *_sipp_test_list : NULL;
	while(test != NULL) {
		if(test->test_id == test_id) {
			memcpy(out, test, sizeof(sipp_test_t));
			out->next = NULL; /* Don't expose internal pointer */
			found = 1;
			break;
		}
		test = test->next;
	}

	lock_release(_sipp_test_lock);
	return found ? 0 : -1;
}


/**
 * Get all tests
 * NOTE: Caller MUST hold _sipp_test_lock for entire traversal
 */
sipp_test_t *sipp_test_get_all(void)
{
	return (_sipp_test_list != NULL) ? *_sipp_test_list : NULL;
}


/**
 * Update test statistics from SIPp log file
 */
int sipp_test_update_stats(sipp_test_t *test)
{
	/* This is a placeholder for parsing SIPp CSV output */
	/* In a production implementation, this would read the log file */
	/* and extract statistics like calls_started, calls_completed, calls_failed */

	if(test == NULL) {
		return -1;
	}

	/* For now, we'll just check if the process is still running */
	if(test->pid > 0 && test->state == SIPP_TEST_STATE_RUNNING) {
		int status;
		pid_t result = waitpid(test->pid, &status, WNOHANG);

		if(result > 0) {
			/* Process has exited */
			if(WIFEXITED(status)) {
				int exit_code = WEXITSTATUS(status);
				if(exit_code == 0) {
					test->state = SIPP_TEST_STATE_COMPLETED;
					LM_INFO("test %d completed successfully\n", test->test_id);
				} else {
					test->state = SIPP_TEST_STATE_FAILED;
					LM_WARN("test %d failed with exit code %d\n", test->test_id,
							exit_code);
				}
			} else if(WIFSIGNALED(status)) {
				test->state = SIPP_TEST_STATE_FAILED;
				LM_WARN("test %d terminated by signal %d\n", test->test_id,
						WTERMSIG(status));
			}
			test->end_time = time(NULL);
		}
	}

	return 0;
}


/**
 * Update and push metrics for all running tests to Prometheus
 * Called frequently (every 5-10 seconds) by timer
 */
void sipp_metrics_update(void)
{
	sipp_test_t *test, *next;
	int cur_id = 0;
	int next_id = 0;

	lock_get(_sipp_test_lock);

	test = (_sipp_test_list != NULL) ? *_sipp_test_list : NULL;
	while(test != NULL) {
		/* Save next pointer and ids before potentially releasing lock */
		next = test->next;
		cur_id = test->test_id;
		next_id = (next != NULL) ? next->test_id : 0;

		/* Enforce optional max test duration */
		if(test->state == SIPP_TEST_STATE_RUNNING
				&& test->max_test_duration > 0) {
			time_t now = time(NULL);
			if(now - test->start_time >= test->max_test_duration) {
				LM_INFO("test %d reached max_test_duration=%d (elapsed=%ld) - "
						"stopping\n",
						cur_id, test->max_test_duration,
						(long)(now - test->start_time));
				lock_release(_sipp_test_lock);
				sipp_test_stop(cur_id);
				lock_get(_sipp_test_lock);
				/* Continue from next_id if possible */
				if(next_id > 0) {
					test = (_sipp_test_list != NULL) ? *_sipp_test_list : NULL;
					while(test != NULL && test->test_id != next_id) {
						test = test->next;
					}
					continue;
				}
				test = NULL;
				continue;
			}
		}

		/* Update stats for running tests */
		if(test->state == SIPP_TEST_STATE_RUNNING) {
			sipp_test_update_stats(test);

			/* Get live stats via control socket if enabled */
			if(test->control_port > 0) {
				lock_release(_sipp_test_lock);
				sipp_test_get_live_stats(cur_id);
				lock_get(_sipp_test_lock);
				/* Find test again as it may have been modified/removed */
				sipp_test_t *found =
						(_sipp_test_list != NULL) ? *_sipp_test_list : NULL;
				while(found != NULL && found->test_id != cur_id) {
					found = found->next;
				}
				if(found == NULL) {
					/* Test was removed during lock release, continue from next_id */
					if(next_id > 0) {
						test = (_sipp_test_list != NULL) ? *_sipp_test_list
														 : NULL;
						while(test != NULL && test->test_id != next_id) {
							test = test->next;
						}
						continue;
					}
					test = NULL;
					continue;
				}
				test = found;
				next = test->next;
			}

			/* Push metrics to Prometheus */
			sipp_push_prometheus_metrics(test);
		}

		test = next;
	}

	lock_release(_sipp_test_lock);
}


/**
 * Clean up old completed tests and remove log files
 * Called infrequently (every 1 hour) by timer
 */
void sipp_test_cleanup(void)
{
	sipp_test_t *test, *prev;
	time_t now = time(NULL);
	int cleanup_age = 1800; /* 30 minutes */

	lock_get(_sipp_test_lock);

	prev = NULL;
	test = (_sipp_test_list != NULL) ? *_sipp_test_list : NULL;

	while(test != NULL) {
		/* Remove old completed/stopped/failed tests */
		if((test->state == SIPP_TEST_STATE_COMPLETED
				   || test->state == SIPP_TEST_STATE_STOPPED
				   || test->state == SIPP_TEST_STATE_FAILED)
				&& (now - test->end_time > cleanup_age)) {

			LM_DBG("cleaning up old test %d\n", test->test_id);

			/* Close control socket if open */
			if(test->control_socket >= 0) {
				close(test->control_socket);
				test->control_socket = -1;
			}

			/* Remove log files */
			if(test->log_file[0] != '\0') {
				char extra_file[SIPP_MAX_LOG_PATH_LEN + 16];

				/* Remove main log file */
				if(unlink(test->log_file) < 0 && errno != ENOENT) {
					LM_DBG("failed to remove log file %s: %s\n", test->log_file,
							strerror(errno));
				}

				/* Remove exec log file */
				snprintf(extra_file, sizeof(extra_file), "%s.exec.log",
						test->log_file);
				if(unlink(extra_file) < 0 && errno != ENOENT) {
					LM_DBG("failed to remove exec log %s: %s\n", extra_file,
							strerror(errno));
				}

				/* Remove stats CSV file */
				snprintf(extra_file, sizeof(extra_file), "%s.stat.csv",
						test->log_file);
				if(unlink(extra_file) < 0 && errno != ENOENT) {
					LM_DBG("failed to remove stats file %s: %s\n", extra_file,
							strerror(errno));
				}

				/* Remove errors log file */
				snprintf(extra_file, sizeof(extra_file), "%s.errors.log",
						test->log_file);
				if(unlink(extra_file) < 0 && errno != ENOENT) {
					LM_DBG("failed to remove errors log %s: %s\n", extra_file,
							strerror(errno));
				}
			}

			/* Remove from list */
			if(prev == NULL) {
				if(_sipp_test_list != NULL) {
					*_sipp_test_list = test->next;
				}
			} else {
				prev->next = test->next;
			}

			sipp_test_t *to_free = test;
			test = test->next;
			shm_free(to_free);
			continue;
		}

		prev = test;
		test = test->next;
	}

	lock_release(_sipp_test_lock);
}


/**
 * Create UDP socket for SIPp remote control
 * NOTE: SIPp uses UDP for remote control, not TCP!
 * NOTE: Caller must already hold _sipp_test_lock
 * Returns: socket fd on success, -1 on failure
 * On success, caches socket in test->control_socket
 */
static int sipp_control_connect(sipp_test_t *test)
{
	struct sockaddr_in addr;
	int sock;
	struct timeval tv;

	if(test->control_port <= 0) {
		return -1; /* Remote control not enabled */
	}

	if(test->control_socket >= 0) {
		return test->control_socket; /* Already have a socket */
	}

	/* Create UDP socket - SIPp uses UDP for remote control */
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(sock < 0) {
		LM_ERR("failed to create UDP control socket: %s\n", strerror(errno));
		return -1;
	}

	/* Set timeout to avoid blocking (shorter for UDP) */
	tv.tv_sec = 0;
	tv.tv_usec = 500000; /* 500ms */
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);
	setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof tv);

	/* Use connect() on UDP to set default destination */
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(test->control_port);
	if(inet_pton(AF_INET, test->local_ip, &addr.sin_addr) <= 0) {
		LM_ERR("invalid local IP address: %s\n", test->local_ip);
		close(sock);
		return -1;
	}

	if(connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		LM_WARN("failed to connect UDP socket to control port %d: %s\n",
				test->control_port, strerror(errno));
		close(sock);
		test->control_socket = -1;
		return -1;
	}

	test->control_socket = sock;
	LM_DBG("created UDP control socket for test %d (port %d)\n", test->test_id,
			test->control_port);

	return sock;
}


/**
 * Send remote control command to SIPp
 */
int sipp_test_control_command(int test_id, char cmd)
{
	sipp_test_t *test;
	int sock;
	ssize_t sent;

	lock_get(_sipp_test_lock);
	test = (_sipp_test_list != NULL) ? *_sipp_test_list : NULL;
	while(test != NULL) {
		if(test->test_id == test_id) {
			break;
		}
		test = test->next;
	}

	if(test == NULL) {
		lock_release(_sipp_test_lock);
		LM_ERR("test %d not found\n", test_id);
		return -1;
	}

	if(test->state != SIPP_TEST_STATE_RUNNING) {
		lock_release(_sipp_test_lock);
		LM_ERR("test %d is not running\n", test_id);
		return -1;
	}

	/* Connect to control socket if needed */
	sock = sipp_control_connect(test);
	if(sock < 0) {
		lock_release(_sipp_test_lock);
		LM_ERR("failed to connect to control socket for test %d\n", test_id);
		return -1;
	}

	/* Send command */
	sent = send(sock, &cmd, 1, 0);
	if(sent != 1) {
		LM_ERR("failed to send control command to test %d: %s\n", test_id,
				strerror(errno));
		/* Close and invalidate control socket only (sock is the cached socket) */
		if(test->control_socket >= 0) {
			close(test->control_socket);
			test->control_socket = -1;
		}
		lock_release(_sipp_test_lock);
		return -1;
	}

	/* Socket is cached in test->control_socket, don't close it here */
	LM_DBG("sent control command '%c' to test %d\n", cmd, test_id);
	lock_release(_sipp_test_lock);
	return 0;
}


/**
 * Get real-time statistics from SIPp stat file
 * SIPp writes periodic CSV stats to <log_file>.stat.csv when -trace_stat is used.
 * NOTE: Control socket is for commands only, not for querying stats.
 */
int sipp_test_get_live_stats(int test_id)
{
	sipp_test_t *test;
	char stat_file[SIPP_MAX_LOG_PATH_LEN + 16];
	FILE *f;
	char line[SIPP_MAX_STAT_LINE_LEN];
	char last_line[SIPP_MAX_STAT_LINE_LEN] = "";
	char header_line[SIPP_MAX_STAT_LINE_LEN] = "";
	int found_data = 0;

	lock_get(_sipp_test_lock);
	test = (_sipp_test_list != NULL) ? *_sipp_test_list : NULL;
	while(test != NULL) {
		if(test->test_id == test_id) {
			break;
		}
		test = test->next;
	}

	if(test == NULL) {
		lock_release(_sipp_test_lock);
		LM_ERR("test %d not found\n", test_id);
		return -1;
	}

	/* Allow stats refresh even after test completion */

	/* Build stat file path */
	snprintf(stat_file, sizeof(stat_file), "%s.stat.csv", test->log_file);

	/* Read stat file - we need to release lock during file I/O */
	int cur_test_id = test->test_id;
	lock_release(_sipp_test_lock);

	f = fopen(stat_file, "r");
	if(f == NULL) {
		/* File may not exist yet if test just started */
		LM_DBG("stat file not found for test %d: %s\n", test_id, stat_file);
		return 0;
	}

	/* Read header and last data line (most recent stats) */
	while(fgets(line, sizeof(line), f) != NULL) {
		/* Capture header line */
		if(strstr(line, "ElapsedTime") != NULL
				|| strstr(line, "CurrentTime") != NULL) {
			strncpy(header_line, line, sizeof(header_line) - 1);
			header_line[sizeof(header_line) - 1] = '\0';
			continue;
		}
		/* Skip empty lines */
		if(line[0] == '\n' || line[0] == '\r') {
			continue;
		}
		/* Save this line as potential last data line */
		strncpy(last_line, line, sizeof(last_line) - 1);
		last_line[sizeof(last_line) - 1] = '\0';
		found_data = 1;
	}
	fclose(f);

	if(!found_data) {
		LM_DBG("no data in stat file for test %d\n", test_id);
		return 0;
	}

	/* Parse the last line
	 * SIPp CSV format (semicolon separated), common headers:
	 * StartTime;LastResetTime;CurrentTime;ElapsedTime(P);ElapsedTime(C);...
	 * TotalCallCreated;CurrentCall;SuccessfulCall(P);SuccessfulCall(C);
	 * FailedCall(P);FailedCall(C);FailedCallRejected(P);FailedCallRejected(C);...
	 */
	int total_created = 0, current_calls = 0, successful = 0, failed = 0;
	int idx_total = 6, idx_current = 7, idx_success = 8, idx_failed = 9;
	int failed_comp_idx[64];
	int failed_comp_count = 0;
	int found_success_c = 0;
	int found_failed_c = 0;

	/* If header is available, map indices by name */
	if(header_line[0] != '\0') {
		char *hp = header_line;
		char *hfield;
		int hidx = 0;
		while((hfield = strsep(&hp, ";")) != NULL) {
			size_t hlen;
			/* Trim leading/trailing whitespace and newline */
			while(*hfield == ' ' || *hfield == '\t')
				hfield++;
			hlen = strlen(hfield);
			if(hlen > 0) {
				for(char *e = hfield + hlen - 1; e >= hfield; e--) {
					if(*e == '\n' || *e == '\r' || *e == ' ' || *e == '\t') {
						*e = '\0';
					} else {
						break;
					}
				}
			}
			if(strcmp(hfield, "TotalCallCreated") == 0) {
				idx_total = hidx;
			} else if(strcmp(hfield, "CurrentCall") == 0) {
				idx_current = hidx;
			} else if(strcmp(hfield, "SuccessfulCall(C)") == 0) {
				idx_success = hidx;
				found_success_c = 1;
			} else if(strcmp(hfield, "SuccessfulCall") == 0
					  || (!found_success_c
							  && strcmp(hfield, "SuccessfulCall(P)") == 0)) {
				idx_success = hidx;
			} else if(strcmp(hfield, "FailedCall(C)") == 0) {
				idx_failed = hidx;
				found_failed_c = 1;
			} else if(strcmp(hfield, "FailedCall") == 0
					  || (!found_failed_c
							  && strcmp(hfield, "FailedCall(P)") == 0)) {
				idx_failed = hidx;
			}

			if(strncmp(hfield, "Failed", 6) == 0 && hlen >= 3
					&& strcmp(hfield, "FailedCall(C)") != 0) {
				const char *suffix = hfield + hlen - 3;
				if(strcmp(suffix, "(C)") == 0
						&& failed_comp_count
								   < (int)(sizeof(failed_comp_idx)
										   / sizeof(failed_comp_idx[0]))) {
					failed_comp_idx[failed_comp_count++] = hidx;
				}
			}
			hidx++;
		}
	}

	/* Parse last data line using mapped indices */
	{
		char *p = last_line;
		char *field;
		int field_idx = 0;
		int failed_components_sum = 0;
		while((field = strsep(&p, ";")) != NULL) {
			if(field_idx == idx_total) {
				total_created = atoi(field);
			} else if(field_idx == idx_current) {
				current_calls = atoi(field);
			} else if(field_idx == idx_success) {
				successful = atoi(field);
			} else if(field_idx == idx_failed) {
				failed = atoi(field);
			}
			if(failed_comp_count > 0) {
				for(int i = 0; i < failed_comp_count; i++) {
					if(field_idx == failed_comp_idx[i]) {
						failed_components_sum += atoi(field);
						break;
					}
				}
			}
			field_idx++;
		}

		if(failed == 0 && failed_components_sum > 0) {
			failed = failed_components_sum;
		}
	}

	/* Re-acquire lock and update test */
	lock_get(_sipp_test_lock);

	/* Find test again (may have been removed) */
	test = (_sipp_test_list != NULL) ? *_sipp_test_list : NULL;
	while(test != NULL) {
		if(test->test_id == cur_test_id) {
			break;
		}
		test = test->next;
	}

	if(test != NULL) {
		test->calls_started = total_created;
		test->calls_active = current_calls;
		test->calls_completed = successful;
		test->calls_failed = failed;

		LM_DBG("test %d stats from file: started=%d active=%d completed=%d "
			   "failed=%d\n",
				test_id, test->calls_started, test->calls_active,
				test->calls_completed, test->calls_failed);
	}

	lock_release(_sipp_test_lock);
	return 0;
}


/**
 * Adjust call rate dynamically
 */
int sipp_test_adjust_rate(int test_id, char rate_change)
{
	if(rate_change != '+' && rate_change != '-') {
		LM_ERR("invalid rate change command: %c (use + or -)\n", rate_change);
		return -1;
	}

	return sipp_test_control_command(test_id, rate_change);
}


/**
 * Push metrics to Prometheus (if xhttp_prom is loaded)
 */
void sipp_push_prometheus_metrics(sipp_test_t *test)
{
	/* Delegate to Prometheus integration module */
	sipp_prom_push_metrics(test);
}
