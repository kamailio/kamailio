#ifndef _SMS_FUNCS_H
#define  _SMS_FUNCS_H

#define MAX_MODEMS    5       /* max number of modems */
#define MAX_NETWORKS  5       /* max number of networks */

#define MAX_CHAR_BUF 64        /* max length of character buffer */
#define MAX_CONFIG_PARAM 1024 /* max length of a config parameter */

struct network {
	char name[MAX_CHAR_BUF];
	char smsc[MAX_CHAR_BUF];
	int pipe_in;
};

struct modem {
	char device[MAX_CHAR_BUF];
	int net_mask;
};

extern struct modem modems[MAX_MODEMS];
extern struct network networks[MAX_NETWORKS];
extern int nr_of_networks;
extern int nr_of_modems;
extern int looping_interval;
extern int max_sms_per_call;

#endif

