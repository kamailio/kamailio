#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netdb.h>
#include <math.h>
#include <errno.h>

#include "../../core/sr_module.h"
#include "lib_statsd.h"

bool isNumber(char *str);

static StatsConnection statsd_connection = {"127.0.0.1", "8125", -1};

enum actions
{
	GAUGE = 0,
	COUNTER,
	SET,
	HISTOGRAM,
	TIMMING
};

static const char *const actions_val[] = {[GAUGE] = "g",
		[COUNTER] = "c",
		[SET] = "s",
		[HISTOGRAM] = "h",
		[TIMMING] = "ms"};


bool statsd_connect(void)
{

	struct addrinfo *serverAddr = NULL;
	int rc;

	if(statsd_connection.sock > 0) {
		return true;
	}

	rc = getaddrinfo(
			statsd_connection.ip, statsd_connection.port, NULL, &serverAddr);
	if(rc != 0 || serverAddr == NULL) {
		LM_ERR("Statsd: could not initiate server information (%s)\n",
				gai_strerror(rc));
		if(serverAddr)
			freeaddrinfo(serverAddr);
		return false;
	}

	statsd_connection.sock =
			socket(serverAddr->ai_family, SOCK_DGRAM, IPPROTO_UDP);
	if(statsd_connection.sock < 0) {
		LM_ERR("Statsd: could not create a socket for statsd connection\n");
		freeaddrinfo(serverAddr);
		return false;
	}

	rc = connect(statsd_connection.sock, serverAddr->ai_addr,
			serverAddr->ai_addrlen);
	freeaddrinfo(serverAddr);
	if(rc < 0) {
		LM_ERR("Statsd: could not initiate a connect to statsd\n");
		return false;
	}
	return true;
}

bool send_command(char *command)
{
	int send_result;

	if(!statsd_connect()) {
		return false;
	}

	send_result = send(statsd_connection.sock, command, strlen(command), 0);
	if(send_result < 0) {
		LM_ERR("could not send the correct info to statsd (%i| %s)\n",
				send_result, strerror(errno));
		return true;
	}
	LM_DBG("Sent to statsd (%s)", command);
	return true;
}

bool statsd_send_command(
		char *key, char *value, enum actions action, char *labels)
{
	size_t labels_len = 0;
	if(labels != NULL) {
		labels_len = strlen(labels);
	}
	const char *action_str = actions_val[action];
	size_t command_len =
			strlen(key) + strlen(value) + labels_len + strlen(action_str) + 6;
	char command[command_len];

	if(labels_len == 0) {
		snprintf(command, command_len, "%s:%s|%s\n", key, value, action_str);
	} else {
		snprintf(command, sizeof command, "%s:%s|%s|#%s\n", key, value,
				action_str, labels);
	}
	return send_command(command);
}


bool statsd_set(char *key, char *value, char *labels)
{
	if(!isNumber(value)) {
		LM_ERR("statsd_set could not  use the provide value(%s)\n", value);
		return false;
	}
	return statsd_send_command(key, value, SET, labels);
}


bool statsd_gauge(char *key, char *value, char *labels)
{
	return statsd_send_command(key, value, GAUGE, labels);
}

bool statsd_histogram(char *key, char *value, char *labels)
{
	return statsd_send_command(key, value, HISTOGRAM, labels);
}

bool statsd_count(char *key, char *value, char *labels)
{
	if(!isNumber(value)) {
		LM_ERR("statsd_count could not  use the provide value(%s)\n", value);
		return false;
	}
	return statsd_send_command(key, value, COUNTER, labels);
}

bool statsd_timing(char *key, int value, char *labels)
{
	int value_len = 1;
	if(value > 0) {
		value_len = (int)((ceil(log10(value)) + 1) * sizeof(char));
	}
	char val[value_len];
	sprintf(val, "%i", value);
	return statsd_send_command(key, val, TIMMING, labels);
}

bool statsd_init(char *ip, char *port)
{

	if(ip != NULL) {
		statsd_connection.ip = ip;
	}
	if(port != NULL) {
		statsd_connection.port = port;
	}
	return statsd_connect();
}

bool statsd_destroy(void)
{
	statsd_connection.sock = 0;
	return true;
}

bool isNumber(char *s)
{
	char *e = NULL;
	(void)strtol(s, &e, 0);
	return e != NULL && *e == (char)0;
}
