#ifndef _SMS_FUNCS_H
#define  _SMS_FUNCS_H

#include "../../parser/msg_parser.h"
#include <termios.h>


#define MAX_MODEMS    5       /* max number of modems */
#define MAX_NETWORKS  5       /* max number of networks */

#define MAX_CHAR_BUF 128        /* max length of character buffer */
#define MAX_CONFIG_PARAM 1024 /* max length of a config parameter */
#define MAX_SMS_LENGTH   160

#define SMS_FROM      "\n\rFrom "
#define SMS_FROM_LEN  7


struct network {
	char name[MAX_CHAR_BUF+1];
	char smsc[MAX_CHAR_BUF+1];
	int  max_sms_per_call;
	int  pipe_out;
};

struct modem {
	char name[MAX_CHAR_BUF+1];
	char device[MAX_CHAR_BUF+1];
	char pin[MAX_CHAR_BUF+1];
	int  net_list[MAX_NETWORKS];
	struct termios oldtio;
	int  mode;
	int  retry;
	int  looping_interval;
	int  fd;
	int  baudrate;
};

struct sms_msg {
	char text[MAX_SMS_LENGTH+1];
	int  text_len;
	char to[MAX_CHAR_BUF+1];
	int  to_len;
	int  to_user_len;
	char from[MAX_CHAR_BUF+1];
	int  from_len;
	int  is_binary;
	int  udh;
	int  cs_convert;
};

struct incame_sms {
	char sender[31];
	char name[64];
	char date[9];
	char time[9];
	char ascii[500];
	char smsc[31];
	int  userdatalength;
	int  is_statusreport;
};


extern struct modem modems[MAX_MODEMS];
extern struct network networks[MAX_NETWORKS];
extern int net_pipes_in[MAX_NETWORKS];
extern int nr_of_networks;
extern int nr_of_modems;


void modem_process(struct modem*);
int  push_on_network(struct sip_msg*, int);


#endif

