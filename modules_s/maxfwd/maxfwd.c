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


static int fixup_maxfwd_header(void** param, int param_no);
static int w_process_maxfwd_header(struct sip_msg* msg,char* str,char* str2);
static int mod_init(void);


#ifdef STATIC_MAXFWD
struct module_exports maxfwd_exports = {
#else
struct module_exports exports= {
#endif
	"maxfwd_module",
	(char*[]){
				"mf_process_maxfwd_header"
			},
	(cmd_function[]){
				w_process_maxfwd_header
			},
	(int[]){
				1
			},
	(fixup_function[]){
				fixup_maxfwd_header
		},
	1,		/* Number of exported functions*/
	NULL,	/* Module parameter names */
	NULL,	/* Module parameter types */
	NULL,	/* Module parameter variable pointers */
	0,		/* Number of module paramers */
	mod_init,
	(response_function) 0,
	(destroy_function) 0,
	0,
	0  /* per-child init function */
};




static int mod_init(void)
{
	fprintf(stderr, "Maxfwd module- initializing\n");
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




static int w_process_maxfwd_header(struct sip_msg* msg, char* str1,char* str2)
{
	int val;
	str mf_value;

	val=is_maxfwd_present(msg, &mf_value);
	switch (val)
	{
		case -1:
			add_maxfwd_header( msg, (unsigned int)str1 );
			break;
		case -2:
			break;
		case 0:
			return -1;
		default:
			if ( decrement_maxfwd(msg, val, &mf_value)!=1 )
				LOG( L_ERR,"ERROR: MAX_FWD module : error on decrement!\n");
	}
	return 1;
}




