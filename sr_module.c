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
/*
 * History:
 * --------
 *  2003-03-10  switched to new module_exports format: updated find_export,
 *               find_export_param, find_module (andrei)
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

#ifdef STATIC_SL
        extern struct module_exports* sl_exports();
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

#ifdef STATIC_EXEC
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

#ifdef STATIC_SL
	ret=register_module(sl_exports, "built-in", 0);
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


/* returns 0 on success , <0 on error */
int load_module(char* path)
{
	void* handle;
	char* error;
	struct module_exports* exp;
	struct sr_module* t;
	
#ifndef RTLD_NOW
/* for openbsd */
#define RTLD_NOW DL_LAZY
#endif
#ifndef DLSYM_PREFIX
/* define it to null */
#define DLSYM_PREFIX
#endif
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
	exp = (struct module_exports*)dlsym(handle, DLSYM_PREFIX "exports");
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
	cmd_export_t* cmd;

	for(t=modules;t;t=t->next){
		for(cmd=t->exports->cmds; cmd && cmd->name; cmd++){
			if((strcmp(name, cmd->name)==0)&&
				(cmd->param_no==param_no) ){
				DBG("find_export: found <%s> in module %s [%s]\n",
						name, t->exports->name, t->path);
				return cmd->function;
			}
		}
	}
	DBG("find_export: <%s> not found \n", name);
	return 0;
}


void* find_param_export(char* mod, char* name, modparam_t type)
{
	struct sr_module* t;
	param_export_t* param;

	for(t = modules; t; t = t->next) {
		if (strcmp(mod, t->exports->name) == 0) {
			for(param=t->exports->params;param && param->name ; param++) {
				if ((strcmp(name, param->name) == 0) &&
				    (param->type == type)) {
					DBG("find_param_export: found <%s> in module %s [%s]\n",
					    name, t->exports->name, t->path);
					return param->param_pointer;
				}
			}
		}
	}
	DBG("find_param_export: parameter <%s> or module <%s> not found\n",
			name, mod);
	return 0;
}



/* finds a module, given a pointer to a module function *
 * returns pointer to module, & if  c!=0, *c=pointer to the
 * function cmd_export structure*/
struct sr_module* find_module(void* f, cmd_export_t  **c)
{
	struct sr_module* t;
	cmd_export_t* cmd;
	
	for (t=modules;t;t=t->next){
		for(cmd=t->exports->cmds; cmd && cmd->name; cmd++) 
			if (f==(void*)cmd->function) {
				if (c) *c=cmd;
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
	modules=0;
}

#ifdef NO_REVERSE_INIT

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

#else


/* recursive module child initialization; (recursion is used to
   process the module linear list in the same order in
   which modules are loaded in config file
*/

static int init_mod_child( struct sr_module* m, int rank )
{
	if (m) {
		/* iterate through the list; if error occurs,
		   propagate it up the stack
		 */
		if (init_mod_child(m->next, rank)!=0) return -1;
		if (m->exports && m->exports->init_child_f) {
			DBG("DEBUG: init_mod_child (%d): %s\n", 
					rank, m->exports->name);
			if (m->exports->init_child_f(rank)<0) {
				LOG(L_ERR, "init_mod_child(): Error while initializing"
							" module %s\n", m->exports->name);
				return -1;
			} else {
				/* module correctly initialized */
				return 0;
			}
		}
		/* no init function -- proceed with success */
		return 0;
	} else {
		/* end of list */
		return 0;
	}
}


/*
 * per-child initialization
 */
int init_child(int rank)
{
	return init_mod_child(modules, rank);
}



/* recursive module initialization; (recursion is used to
   process the module linear list in the same order in
   which modules are loaded in config file
*/

static int init_mod( struct sr_module* m )
{
	if (m) {
		/* iterate through the list; if error occurs,
		   propagate it up the stack
		 */
		if (init_mod(m->next)!=0) return -1;
		if (m->exports && m->exports->init_f) {
			DBG("DEBUG: init_mod: %s\n", m->exports->name);
			if (m->exports->init_f()!=0) {
				LOG(L_ERR, "init_mod(): Error while initializing"
							" module %s\n", m->exports->name);
				return -1;
			} else {
				/* module correctly initialized */
				return 0;
			}
		}
		/* no init function -- proceed with success */
		return 0;
	} else {
		/* end of list */
		return 0;
	}
}

/*
 * Initialize all loaded modules, the initialization
 * is done *AFTER* the configuration file is parsed
 */
int init_modules(void)
{
	return init_mod(modules);
}

#endif
