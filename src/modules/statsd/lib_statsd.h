#include <stdbool.h>

#define BUFFER_SIZE 8192

typedef struct StatsConnection
{
	char *ip;
	char *port;
	int sock;
} StatsConnection;

bool statsd_connect(void);
bool send_command(char *command);
bool statsd_set(char *key, char *value);
bool statsd_gauge(char *key, char *value);
bool statsd_histogram(char *key, char *value);
bool statsd_count(char *key, char *value);
bool statsd_timing(char *key, int value);
bool statsd_init(char *ip, char *port);
bool statsd_destroy(void);
