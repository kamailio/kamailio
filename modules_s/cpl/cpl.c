#include <stdio.h>
#include <string.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "jcpli.h"

#define   CPL_SERVER  "gorn.fokus.gmd.de"
#define   CPL_PORT       18011


char               *resp_buf;
unsigned int   resp_len;
unsigned int   resp_code;



static int w_run_script(struct sip_msg* msg, char* str, char* str2);
static int w_is_response_accept(struct sip_msg* msg, char* str, char* str2);
static int w_is_response_reject(struct sip_msg* msg, char* str, char* str2);
static int w_is_response_redirect(struct sip_msg* msg, char* str, char* str2);


static struct module_exports cpl_exports= {
	"cpl_module",
	(char*[]){		"cpl_run_script",
				"cpl_is_response_accept",
				"cpl_is_response_reject",
				"cpl_is_response_redirect"
			},
	(cmd_function[]){
				w_run_script,
				w_is_response_accept,
				w_is_response_reject,
				w_is_response_redirect
					},
	(int[]){
				0,
				0,
				0,
				0
			},
	(fixup_function[]){
				0,
				0,
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
	fprintf(stderr, "cpl - registering\n");
	return &cpl_exports;
}


static int w_run_script(struct sip_msg* msg, char* str, char* str2)
{

	if (resp_buf)
	{
		pkg_free(resp_buf);
		resp_buf = 0;
	}

	resp_code =executeCPLForSIPMessage( msg->orig, msg->len, CPL_SERVER,
	  CPL_PORT, &resp_buf, &resp_len);
	if (!resp_code)
	{
		LOG( L_ERR ,  "ERROR : cpl_run_script : cpl running failed!\n");
		goto error;
	}
	DBG("DEBUG : cpl_run_script : response received -> %d\n",resp_code);

	return 1;

error:
	return -1;
}



static int w_is_response_accept(struct sip_msg* msg, char* str, char* str2)
{
	return (resp_code==ACCEPT_CALL?1:-1);
}


static int w_is_response_reject(struct sip_msg* msg, char* str, char* str2)
{
	TRejectMessage  *reject;

	if (resp_code==REJECT_CALL && resp_buf && resp_len)
	{
		reject = parseRejectResponse( resp_buf , resp_len );
		printRejectMessage( reject );
		freeRejectMessage( reject );
		return 1;
	}
	return -1;
}


static int w_is_response_redirect(struct sip_msg* msg, char* str, char* str2)
{
	TRedirectMessage  *redirect;

	if (resp_code==REDIRECT_CALL && resp_buf && resp_len)
	{
		redirect = parseRedirectResponse( resp_buf , resp_len );
		printRedirectMessage( redirect );
		freeRedirectMessage( redirect );
		return 1;
	}
	return -1;
}


