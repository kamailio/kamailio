/* $Id$
 *
 * modules/plugin strtuctures declarations
 *
 */

#ifndef sr_module_h
#define sr_module_h

#include "msg_parser.h" /* for sip_msg */

typedef  struct module_exports* (*module_register)();
typedef  int (*cmd_function)(struct sip_msg*, char*, char*);
typedef  int (*fixup_function)(void** param, int param_no);
typedef  int (*response_function)(struct sip_msg*);
typedef  void (*onbreak_function)(struct sip_msg*);
typedef void (*destroy_function)();
typedef int (*child_init_function)(int rank);

struct module_exports{
	char* name; /* null terminated module name */
	char** cmd_names; /* cmd names registered by this modules */
	cmd_function* cmd_pointers; /* pointers to the corresponding functions */
	int* param_no; /* number of parameters used by the function */
	fixup_function* fixup_pointers; /* pointers to functions called to "fix"
										the params, e.g: precompile a re */
	int cmd_no; /* number of registered commands 
				   (size of cmd_{names,pointers}*/
	response_function response_f; /* function used for responses,
											   returns yes or no;
									can be null */
	destroy_function destroy_f; /*function called when the module should
								  be "destroyed", e.g: on ser exit;
								  can be null */
	onbreak_function onbreak_f;
	child_init_function init_child_f;  /* Function will be called by all processes after the fork */
};

struct sr_module{
	char* path;
	void* handle;
	struct module_exports* exports;
	struct sr_module* next;
};
 
struct sr_module* modules; /* global module list*/

int init_builtin_modules();
int register_module(module_register, char*,  void*);
int load_module(char* path);
cmd_function find_export(char* name, int param_no);
struct sr_module* find_module(void *f, int* r);
void destroy_modules();
int init_child(int rank);


/* modules function prototypes:
 * struct module_exports* mod_register(); (type module_register)
 * int   foo_cmd(struct sip_msg* msg, char* param);
 *  - returns >0 if ok , <0 on error, 0 to stop processing (==DROP)
 * int   response_f(struct sip_msg* msg)
 *  - returns >0 if ok, 0 to drop message
 */


#endif
