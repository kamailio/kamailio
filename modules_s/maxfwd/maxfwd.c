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
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "mf_funcs.h"


static int w_decrement_maxfwd(struct sip_msg* msg, char* str, char* str2);
static int w_add_maxfwd_header(struct sip_msg* msg, char* str, char* str2);
static int w_is_maxfwd_zero(struct sip_msg* msg, char* str, char* str2);
static int w_reply_to_maxfwd_zero(struct sip_msg* msg, char* str, char* str2);
static int fixup_maxfwd_header(void** param, int param_no);
static int w_is_maxfwd_present(struct sip_msg* msg, char* str, char* str2);
static int w_process_maxfwd_header(struct sip_msg* msg,char* str,char* str2);
static int mod_init(void);


struct module_exports exports= {
	"maxfwd_module",
	(char*[]){	"mf_decrement_maxfwd",
				"mf_add_maxfwd_header",
				"mf_is_maxfwd_zero",
				"mf_reply_to_maxfwd_zero",
				"mf_is_maxfwd_present",
				"mf_process_maxfwd_header"
			},
	(cmd_function[]){
					w_decrement_maxfwd,
					w_add_maxfwd_header,
					w_is_maxfwd_zero,
					w_reply_to_maxfwd_zero,
					w_is_maxfwd_present,
					w_process_maxfwd_header
					},
	(int[]){
				0,
				1,
				0,
				0,
				0,
				1
			},

	(fixup_function[]){
				0,
				fixup_maxfwd_header,
				0,
				0,
				0,
				fixup_maxfwd_header
		},
	6,

	NULL,   /* Module parameter names */
	NULL,   /* Module parameter types */
	NULL,   /* Module parameter variable pointers */
	0,      /* Number of module paramers */

	mod_init,
	(response_function) 0,
	(destroy_function) 0,
	0,
	0  /* per-child init function */
};


static int mod_init(void)
{
	fprintf(stderr, "maxfwd - initializing\n");
	mf_startup();
	return 0;
}



static int fixup_maxfwd_header(void** param, int param_no)
{
	unsigned int code;
	int err;

	if (param_no==1){
		code=str2s(*param, strlen(*param), &err);
		if (err==0){
			if (code>255){
				LOG(L_ERR, "MAXFWD module:fixup_maxfwd_header: "
					"number to big <%d> (max=255)\n",code);
				return E_UNSPEC;
			}
			free(*param);
			*param=(void*)code;
			return 0;
		}else{
			LOG(L_ERR, "MAXFWD module:fixup_maxfwd_header: bad  number <%s>\n",
					(char*)(*param));
			return E_UNSPEC;
		}
	}
	return 0;
}


static int w_decrement_maxfwd(struct sip_msg* msg, char* str, char* str2)
{
	return decrement_maxfwd( msg );
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

static int w_is_maxfwd_present(struct sip_msg* msg, char* str, char* str2)
{
	return is_maxfwd_present( msg );
}

static int w_process_maxfwd_header(struct sip_msg* msg, char* str,char* str2)
{
	if (is_maxfwd_present(msg)==1)
	{
		if ( decrement_maxfwd(msg)!=1 )
		{
			LOG( L_ERR,"ERROR: MAX_FWD module : unable to decrement header\n");
			goto OK;
		}
		if (is_maxfwd_zero(msg)==1 )
		{
			LOG( L_INFO,"INFO: MAX_FWD module : zero value found\n!");
			goto error;
		}
	}
	else
		add_maxfwd_header( msg , (unsigned int)str );

OK:
	return 1;
error:
	return -1;
}




