#ifndef _SMS_FUNCS_H
#define  _SMS_FUNCS_H

#include "../../parser/msg_parser.h"
#include <termios.h>


#define MAX_MODEMS    5       /* max number of modems */
#define MAX_NETWORKS  5       /* max number of networks */

#define MAX_CHAR_BUF 64        /* max length of character buffer */
#define MAX_CONFIG_PARAM 1024 /* max length of a config parameter */
#define MAX_SMS_LENGTH   160


#define MODE_OLD   1



struct network {
	char name[MAX_CHAR_BUF+1];
	char smsc[MAX_CHAR_BUF+1];
	int  pipe_out;
};

struct modem {
	char name[MAX_CHAR_BUF+1];
	char device[MAX_CHAR_BUF+1];
	char pin[MAX_CHAR_BUF+1];
	int  net_list[MAX_NETWORKS];
	struct termios oldtio;
	int  mode;
	int  report;
	int  fd;
	int  boundrate;
};

struct sms_msg {
	char text[MAX_SMS_LENGTH+1];
	char to[MAX_CHAR_BUF+1];
	int is_binary;
	int udh;
	int cs_convert;
};



extern struct modem modems[MAX_MODEMS];
extern struct network networks[MAX_NETWORKS];
extern int net_pipes_in[MAX_NETWORKS];
extern int nr_of_networks;
extern int nr_of_modems;
extern int looping_interval;
extern int max_sms_per_call;
extern int default_net;


void modem_process(struct modem*);
int  push_on_network(struct sip_msg*, int);


#endif

