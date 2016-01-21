#include <stdbool.h>

#define BUFFER_SIZE 8192

typedef struct StatsConnection{
    char *ip;
    char *port;
} StatsConnection;

typedef struct StatsdSocket {
    char *name; // name
    int sock; // socket
    int timeout; // how many miliseconds to wait for an answer
    time_t last_failure; // time of the last failure
    char data[BUFFER_SIZE]; // buffer for the answer data
} StatsdSocket;

bool statsd_connect(void);
bool send_command(char *command);
bool statsd_set(char *key, char *value);
bool statsd_gauge(char *key, char *value);
bool statsd_count(char *key, char *value);
bool statsd_timing(char *key, int value);
bool statsd_init(char *ip, char *port);
bool statsd_destroy(void);
