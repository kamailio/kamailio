/*$Id$
 *
 * Example ser module, it will just print its string parameter to stdout
 *
 */



#include "../../sr_module.h"
#include <stdio.h>

static int print_f(struct sip_msg*, char*,char*);
static int mod_init(void);

char* str_param;
int int_param;

struct module_exports exports = {
	"print_stdout", 
	(char*[]){"print"},
	(cmd_function[]){print_f},
	(int[]){1},
	(fixup_function[]){0},
	1, /* number of functions*/

	(char*[]){"str_param", "int_param"},
	(modparam_t[]){STR_PARAM, INT_PARAM},
	(void*[]){&str_param, &int_param},
	2,
	
	mod_init, /* module initialization function */
	0,        /* response function*/
	0,        /* destroy function */
	0,        /* oncancel function */
	0         /* per-child init function */
};


static int mod_init(void)
{
	fprintf(stderr, "print - initializing\n");
	return 0;
}


static int print_f(struct sip_msg* msg, char* str, char* str2)
{
	/*we registered only 1 param, so we ignore str2*/
	printf("%s\n",str);
	return 1;
}


