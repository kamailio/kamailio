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
#include "mf_funcs.h"


static int w_decrement_maxfwd(struct sip_msg* msg, char* str, char* str2);
static int w_add_maxfwd_header(struct sip_msg* msg, char* str, char* str2);
static int w_is_maxfwd_zero(struct sip_msg* msg, char* str, char* str2);
static int w_reply_to_maxfwd_zero(struct sip_msg* msg, char* str, char* str2);
static int fixup_add_maxfwd_header(void** param, int param_no);


static struct module_exports mf_exports= {
	"maxfwd_module",
	(char*[]){			"mf_decrement_maxfwd",
				"mf_add_maxfwd_header",
				"mf_is_maxfwd_zero",
				"mf_reply_to_maxfwd_zero"
			},
	(cmd_function[]){
					w_decrement_maxfwd,
					w_add_maxfwd_header,
					w_is_maxfwd_zero,
					w_reply_to_maxfwd_zero
					},
	(int[]){
				0,
				1,
				0,
				0
			},
	(fixup_function[]){
				0,
				fixup_add_maxfwd_header,
				0,
				0
		},
	4,
	(response_function) 0,
	(destroy_function) 0,
	0
};


struct module_exports* mod_register()
{
	fprintf(stderr, "maxfwd - registering\n");
	return &mf_exports;
}



static int fixup_add_maxfwd_header(void** param, int param_no)
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
			LOG(L_ERR, "MAXFWD module:fixup_add_maxfwd_header: bad  number <%s>\n",
					*param);
			return E_UNSPEC;
		}
	}
	return 0;
}


static int w_decrement_maxfwd(struct sip_msg* msg, char* str, char* str2)
{
	return decrement_maxfed( msg );
}

static int w_add_maxfwd_header(struct sip_msg* msg, char* str, char* str2)
{
	return add_maxfwd_header( msg , (unsigned int)str );
}

static int w_is_maxfwd_zero(struct sip_msg* msg, char* str, char* str2)
{
	return is_maxfwd_zero( msg );
}

static int w_reply_to_maxfwd_zero(struct sip_msg* msg, char* str, char* str2)
{
	return reply_to_maxfwd_zero( msg );
}


