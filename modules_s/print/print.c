/*$Id$
 *
 * Example ser module, it will just print its string parameter to stdout
 *
 */



#include "../../sr_module.h"
#include <stdio.h>

static int print_f(struct sip_msg*, char*,char*);

static struct module_exports print_exports= {	"print_stdout", 
												(char*[]){"print"},
												(cmd_function[]){print_f},
												(int[]){1},
												(fixup_function[]){0},
												1, /* number of fucntions*/
												0, /* response function*/
												0,  /* destroy function */
												0   /* oncancel function */
											};


struct module_exports* mod_register()
{
	fprintf(stderr, "print - registering...\n");
	return &print_exports;
}


static int print_f(struct sip_msg* msg, char* str, char* str2)
{
	/*we registered only 1 param, so we ignore str2*/
	printf("%s\n",str);
	return 1;
}


