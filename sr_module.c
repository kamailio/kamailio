/* $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include "sr_module.h"
#include "dprint.h"
#include "error.h"

#include <dlfcn.h>
#include <strings.h>
#include <stdlib.h>
#include <string.h>


struct sr_module* modules=0;

#ifdef STATIC_EXEC
	extern struct module_exports* exec_exports();
#endif
#ifdef STATIC_TM
	extern struct module_exports* tm_exports();
#endif

#ifdef STATIC_MAXFWD
	extern struct module_exports* maxfwd_exports();
#endif

#ifdef STATIC_AUTH
        extern struct module_exports* auth_exports();
#endif

#ifdef STATIC_RR
        extern struct module_exports* rr_exports();
#endif

#ifdef STATIC_USRLOC
        extern struct module_exports* usrloc_exports();
#endif


/* initializes statically built (compiled in) modules*/
int register_builtin_modules()
{
	int ret;

	ret=0;
#ifdef STATIC_TM
	ret=register_module(tm_exports,"built-in", 0); 
	if (ret<0) return ret;
#endif

#ifdef EXEC_TM
	ret=register_module(exec_exports,"built-in", 0); 
	if (ret<0) return ret;
#endif

#ifdef STATIC_MAXFWD
	ret=register_module(maxfwd_exports, "built-in", 0);
	if (ret<0) return ret;
#endif

#ifdef STATIC_AUTH
	ret=register_module(auth_exports, "built-in", 0); 
	if (ret<0) return ret;
#endif
	
#ifdef STATIC_RR
	ret=register_module(rr_exports, "built-in", 0);
	if (ret<0) return ret;
#endif
	
#ifdef STATIC_USRLOC
	ret=register_module(usrloc_exports, "built-in", 0);
	if (ret<0) return ret;
#endif
	
	return ret;
}



/* registers a module,  register_f= module register  functions
 * returns <0 on error, 0 on success */
int register_module(struct module_exports* e, char* path, void* handle)
{
	int ret;
	struct sr_module* mod;
	
	ret=-1;

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
				LOG(L_ERR, "init_child(): Initialization of child %d failed\n",
						rank);
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
	struct module_exports* exp;
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
	exp = (struct module_exports*)dlsym(handle, "exports");
	if ( (error =(char*)dlerror())!=0 ){
		LOG(L_ERR, "ERROR: load_module: %s\n", error);
		goto error1;
	}
	if (register_module(exp, path, handle)<0) goto error1;
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


void* find_param_export(char* mod, char* name, modparam_t type)
{
	struct sr_module* t;
	int r;

	for(t = modules; t; t = t->next) {
		if (strcmp(mod, t->exports->name) == 0) {
			for(r = 0; r < t->exports->par_no; r++) {
				if ((strcmp(name, t->exports->param_names[r]) == 0) &&
				    (t->exports->param_types[r] == type)) {
					DBG("find_param_export: found <%s> in module %s [%s]\n",
					    name, t->exports->name, t->path);
					return t->exports->param_pointers[r];
				}
			}
		}
	}
	DBG("find_param_export: parameter <%s> or module <%s> not found\n",
			name, mod);
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


/*
 * Initialize all loaded modules, the initialization
 * is done *AFTER* the configuration file is parsed
 */
int init_modules(void)
{
	struct sr_module* t;
	
	for(t = modules; t; t = t->next) {
		if ((t->exports) && (t->exports->init_f))
			if (t->exports->init_f() != 0) {
				LOG(L_ERR, "init_modules(): Error while initializing"
							" module %s\n", t->exports->name);
				return -1;
			}
	}
	return 0;
}
