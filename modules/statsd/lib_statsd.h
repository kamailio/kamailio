
#define BUFFER_SIZE 8192
typedef int Bool;
#define True 1
#define False 0

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

int statsd_connect(void);
int send_command(char *command);
int statsd_set(char *key, char *value);
int statsd_gauge(char *key, char *value);
int statsd_count(char *key, char *value);
int statsd_timing(char *key, int value);
int statsd_init(char *ip, char *port);
int statsd_destroy(void);
