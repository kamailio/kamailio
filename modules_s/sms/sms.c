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



static int mod_init(void);
static int sms_send_message(struct sip_msg*, char*, char* );


struct module_exports exports= {
	"sms_module",
	(char*[]){
				"sms_send_message"
			},
	(cmd_function[]){
					sms_send_message
					},
	(int[]){
				0
			},
	(fixup_function[]){
				0
		},
	1,

	NULL,   /* Module parameter names */
	NULL,   /* Module parameter types */
	NULL,   /* Module parameter variable pointers */
	0,      /* Number of module paramers */

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




static int sms_send_message(struct sip_msg *msg, char* foo1, char * foo2)
{
	str body;

	if ( !im_extract_body(msg,&body) )
	{
		LOG(L_ERR,"ERROR: sms_send_message:cannot extract body from msg!\n");
		goto error;
	}

	return 1;
error:
	return -1;
}


