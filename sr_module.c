/* $Id$
 */

#include "sr_module.h"
#include "dprint.h"

#include <dlfcn.h>
#include <strings.h>
#include <stdlib.h>


static struct sr_module* modules=0;



/* returns 0 on success , <0 on error */
int load_module(char* path)
{
	void* handle;
	char* error;
	struct sr_module* t, *mod;
	struct module_exports* e;
	struct module_exports* (*mod_register)();
	
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
	mod_register = dlsym(handle, "mod_register");
	if ( (error =dlerror())!=0 ){
		LOG(L_ERR, "ERROR: load_module: %s\n", error);
		goto error1;
	}
	
	e=(*mod_register)();
	if (e==0){
		LOG(L_ERR, "ERROR: mod_register returned null\n");
		goto error1;
	}
	/* add module to the list */
	if ((mod=malloc(sizeof(struct sr_module)))==0){
		LOG(L_ERR, "load_module: memory allocation failure\n");
		goto error1;
	}
	memset(mod,0, sizeof(struct sr_module));
	mod->path=path;
	mod->handle=handle;
	mod->exports=e;
	mod->next=modules;
	modules=mod;
	return 0;

error1:
	dlclose(handle);
error:
skip:
	return -1;
}



/* searches the module list and returns a pointer to the "name" function or
 * 0 if not found */
cmd_function find_export(char* name)
{
	struct sr_module* t;
	int r;

	for(t=modules;t;t=t->next){
		for(r=0;r<t->exports->cmd_no;r++){
			if(strcmp(name, t->exports->cmd_names[r])==0){
				DBG("find_export: found <%s> in module %s [%s]\n",
						name, t->exports->name, t->path);
				return t->exports->cmd_pointers[r];
			}
		}
	}
	DBG("find_export: <%s> not found \n", name);
	return 0;
}

	
