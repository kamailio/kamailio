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

static StatsConnection statsd_connection = {
    "127.0.0.1",
    "8125",
    -1
};

bool statsd_connect(void){

    struct addrinfo *serverAddr = NULL;
    int rc;

    if (statsd_connection.sock > 0){
        return true;
    }

    rc = getaddrinfo(
        statsd_connection.ip, statsd_connection.port,
        NULL, &serverAddr);
    if (rc != 0 || serverAddr == NULL)
    {
        LM_ERR("Statsd: could not initiate server information (%s)\n",
            gai_strerror(rc));
        if(serverAddr) freeaddrinfo(serverAddr);
        return false;
    }

    statsd_connection.sock = socket(serverAddr->ai_family, SOCK_DGRAM, IPPROTO_UDP);
    if (statsd_connection.sock < 0 ){
        LM_ERR("Statsd: could not create a socket for statsd connection\n");
        freeaddrinfo(serverAddr);
        return false;
    }

    rc = connect(statsd_connection.sock, serverAddr->ai_addr,
            serverAddr->ai_addrlen);
    freeaddrinfo(serverAddr);
    if (rc < 0){
        LM_ERR("Statsd: could not initiate a connect to statsd\n");
        return false;
    }
    return true;
}

bool send_command(char *command){
    int send_result;

    if (!statsd_connect()){
        return false;
    }

    send_result = send(statsd_connection.sock, command, strlen(command), 0);
    if ( send_result < 0){
        LM_ERR("could not send the correct info to statsd (%i| %s)\n",
            send_result, strerror(errno));
        return true;
    }
    LM_DBG("Sent to statsd (%s)", command);
    return true;
}

bool statsd_set(char *key, char *value){
   char* end = 0;
   char command[254];
   int val;
   val = strtol(value, &end, 0);
   if (*end){
       LM_ERR("statsd_count could not  use the provide value(%s)\n", value);
       return false;
   }
   snprintf(command, sizeof command, "%s:%i|s\n", key, val);
   return send_command(command);
}


bool statsd_gauge(char *key, char *value){
   char command[254];
   snprintf(command, sizeof command, "%s:%s|g\n", key, value);
   return send_command(command);
}

bool statsd_count(char *key, char *value){
   char* end = 0;
   char command[254];
   int val;

   val = strtol(value, &end, 0);
   if (*end){
       LM_ERR("statsd_count could not  use the provide value(%s)\n", value);
       return false;
   }
   snprintf(command, sizeof command, "%s:%i|c\n", key, val);
   return send_command(command);
}

bool statsd_timing(char *key, int value){
   char command[254];
   snprintf(command, sizeof command, "%s:%i|ms\n", key, value);
   return send_command(command);
}

bool statsd_init(char *ip, char *port){

    if (ip != NULL){
        statsd_connection.ip = ip;
    }
    if (port != NULL ){
       statsd_connection.port = port;
    }
    return statsd_connect();
}

bool statsd_destroy(void){
    statsd_connection.sock = 0;
    return true;
}
