/* $Id$
 */

#include "sr_module.h"
#include "dprint.h"
#include "error.h"

#include <dlfcn.h>
#include <strings.h>
#include <stdlib.h>


struct sr_module* modules=0;

#ifdef STATIC_TM
	extern struct module_exports* tm_mod_register();
#endif
#ifdef STATIC_MAXFWD
	extern struct module_exports* maxfwd_mod_register();
#endif
#ifdef STATIC_AUTH
        extern struct module_exports* auth_mod_register();
#endif
#ifdef STATIC_RR
        extern struct module_exports* rr_mod_register();
#endif
#ifdef STATIC_USRLOC
        extern struct module_exports* usrloc_mod_register();
#endif


/* initializes statically built (compiled in) modules*/
int init_builtin_modules()
{
	#ifdef STATIC_TM
		register_module(tm_mod_register,"built-in", 0);
	#endif
	#ifdef STATIC_MAXFWD
		register_module(maxfwd_mod_register, "built-in", 0);
	#endif

#ifdef STATIC_AUTH
		register_module(tm_mod_register, "built-in", 0);
#endif

#ifdef STATIC_RR
		register_module(rr_mod_register, "built-in", 0);
#endif

#ifdef STATIC_USRLOC
		register_module(usrloc_mod_register, "built-in", 0);
#endif
}



/* registers a module,  register_f= module register  functions
 * returns <0 on error, 0 on success */
int register_module(module_register register_f, char* path, void* handle)
{
	int ret;
	struct module_exports* e;
	struct sr_module* t, *mod;
	
	ret=-1;
	e=(*register_f)();
	if (e==0){
		LOG(L_ERR, "ERROR: mod_register returned null\n");
		goto error;
	}
	/* add module to the list */
	if ((mod=malloc(sizeof(struct sr_module)))==0){
		LOG(L_ERR, "load_module: memory allocation failure\n");
		ret=E_OUT_OF_MEM;
		goto error;
	}
	memset(mod,0, sizeof(struct sr_module));
	mod->path=path;
	mod->handle=handle;
	mod->exports=e;
	mod->next=modules;
	modules=mod;
	return 0;
error:
	return ret;
}

/*
 * per-child initialization
 */
int init_child(int rank)
{
	struct sr_module* t;

	for(t = modules; t; t = t->next) {
		if (t->exports->init_child_f) {
			if ((t->exports->init_child_f(rank)) < 0) {
				LOG(L_ERR, "init_child(): Initialization of child with rank %d failed\n");
				return -1;
			}
		}
	}
	return 0;
}



/* returns 0 on success , <0 on error */
int load_module(char* path)
{
	void* handle;
	char* error;
	module_register	mod_register;
	struct sr_module* t;
	
	handle=dlopen(path, RTLD_NOW); /* resolve all symbols now */
	if (handle==0){
		LOG(L_ERR, "ERROR: load_module: could not open module <%s>: %s\n",
					path, dlerror() );
		goto error;
	}
	
	for(t=modules;t; t=t->next){
		if (t->handle==handle){
			LOG(L_WARN, "WARNING: load_module: attempting to load the same"
						" module twice (%s)\n", path);
			goto skip;
		}
	}
	/* launch register */
	mod_register = (module_register)dlsym(handle, "mod_register");
	if ( (error =dlerror())!=0 ){
		LOG(L_ERR, "ERROR: load_module: %s\n", error);
		goto error1;
	}
	if (register_module(mod_register, path, handle)<0) goto error1;
	return 0;

error1:
	dlclose(handle);
error:
skip:
	return -1;
}



/* searches the module list and returns a pointer to the "name" function or
 * 0 if not found */
cmd_function find_export(char* name, int param_no)
{
	struct sr_module* t;
	int r;

	for(t=modules;t;t=t->next){
		for(r=0;r<t->exports->cmd_no;r++){
			if((strcmp(name, t->exports->cmd_names[r])==0)&&
				(t->exports->param_no[r]==param_no) ){
				DBG("find_export: found <%s> in module %s [%s]\n",
						name, t->exports->name, t->path);
				return t->exports->cmd_pointers[r];
			}
		}
	}
	DBG("find_export: <%s> not found \n", name);
	return 0;
}



/* finds a module, given a pointer to a module function *
 * returns pointer to module, & if i i!=0, *i=the function index */
struct sr_module* find_module(void* f, int *i)
{
	struct sr_module* t;
	int r;
	for (t=modules;t;t=t->next){
		for(r=0;r<t->exports->cmd_no;r++) 
			if (f==(void*)t->exports->cmd_pointers[r]) {
				if (i) *i=r;
				return t;
			}
	}
	return 0;
}



void destroy_modules()
{
	struct sr_module* t;

	for(t=modules;t;t=t->next)
		if  ((t->exports)&&(t->exports->destroy_f)) t->exports->destroy_f();
}
