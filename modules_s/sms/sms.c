/*
 * $Id$
 *
 * MAXFWD module
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../sr_module.h"
#include "../../error.h"
#include "../../dprint.h"
#include "../im/im_funcs.h"
#include "sms_funcs.h"



static int mod_init(void);
static int sms_send_msg(struct sip_msg*, char*, char* );
static int sms_send_msg_to_net(struct sip_msg*, char*, char*);
static int sms_send_msg_to_center(struct sip_msg* , char*, char*);


/* parameters */
char *networks_config;
char *modems_config;
int  looping_interval;
int  max_sms_per_call;



struct module_exports exports= {
	"sms_module",
	(char*[]){
				"sms_send_msg_to_net",
				"sms_send_msg_to_center",
				"sms_send_msg"
			},
	(cmd_function[]){
					sms_send_msg_to_net,
					sms_send_msg_to_center,
					sms_send_msg
					},
	(int[]){
				1,
				1,
				0
			},
	(fixup_function[]){
				0,
				0,
				0
		},
	3,

	(char*[]) {   /* Module parameter names */
		"networks",
		"modems",
		"looping_interval",
		"max_sms_per_call"
	},
	(modparam_t[]) {   /* Module parameter types */
		STR_PARAM,
		STR_PARAM,
		INT_PARAM,
		INT_PARAM
	},
	(void*[]) {   /* Module parameter variable pointers */
		&networks_config,
		&modems_config,
		&looping_interval,
		&max_sms_per_call
	},
	4,      /* Number of module paramers */

	mod_init,   /* module initialization function */
	(response_function) 0,
	(destroy_function) 0,
	0,
	0  /* per-child init function */
};


static int mod_init(void)
{
	printf("sms - initializing\n");
	//sms_startup();
	return 0;
}




static int sms_send_msg(struct sip_msg *msg, char *foo, char *bar)
{
	str body;

	if ( im_extract_body(msg,&body)==-1 )
	{
		LOG(L_ERR,"ERROR: sms_send_message:cannot extract body from msg!\n");
		goto error;
	}

	return 1;
error:
	return -1;
}




static int sms_send_msg_to_net(struct sip_msg *msg, char *net_name, char *foo)
{
	return 1;
}




static int sms_send_msg_to_center(struct sip_msg *msg, char *smsc, char *foo)
{
	return 1;
}



