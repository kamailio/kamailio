/*
 * $Id$
 *
 * MAXFWD module
 *
 */

#include <stdio.h>
#include <string.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "sl_funcs.h"


static int w_sl_filter_ACK(struct sip_msg* msg, char* str, char* str2);
static int w_sl_send_reply(struct sip_msg* msg, char* str, char* str2);
static int fixup_sl_send_reply(void** param, int param_no);


static struct module_exports sl_exports= {
	"sl_module",
	(char*[]){
				"sl_send_reply",
				"sl_filter_ACK"
			},
	(cmd_function[]){
					w_sl_send_reply,
					w_sl_filter_ACK
					},
	(int[]){
				2,
				0
			},
	(fixup_function[]){
				fixup_sl_send_reply,
				0
		},
	2,
	(response_function) 0,
	(destroy_function) 0,
	0,
	0  /* per-child init function */
};


#ifdef STATIC_STATELESS
struct module_exports* sl_mod_register()
#else
struct module_exports* mod_register()
#endif
{
	fprintf(stderr, "stateless - registering\n");
	sl_startup();
	return &sl_exports;
}


static int fixup_sl_send_reply(void** param, int param_no)
{
	unsigned int code;
	int err;

	if (param_no==1){
		code=str2s(*param, strlen(*param), &err);
		if (err==0){
			free(*param);
			*param=(void*)code;
			return 0;
		}else{
			LOG(L_ERR, "SL module:fixup_sl_send_reply: bad  number <%s>\n",
					*param);
			return E_UNSPEC;
		}
	}
	return 0;
}




static int w_sl_filter_ACK(struct sip_msg* msg, char* str, char* str2)
{
	return sl_filter_ACK(msg);
}



static int w_sl_send_reply(struct sip_msg* msg, char* str, char* str2)
{
	return sl_send_reply(msg,(unsigned int)str,str2);
}





