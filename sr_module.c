/* $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 *  2003-03-19  replaced all mallocs/frees w/ pkg_malloc/pkg_free (andrei)
 *  2003-03-19  Support for flags in find_export (janakj)
 *  2003-03-29  cleaning pkg_mallocs introduced (jiri)
 *  2003-04-24  module version checking introduced (jiri)
 *  2004-09-19  compile flags are checked too (andrei)
 *  2005-01-07  removed find_module-overloading problems, added
 *               find_export_record
 *  2006-02-07  added fix_flag (andrei)
 *  2008-02-29  store all the reponse callbacks in their own array (andrei)
 *  2008-11-17  support dual module interface: ser & kamailio (andrei)
 *  2008-11-26  added fparam_free_contents() and fix_param_types (andrei)
 */


#include "sr_module.h"
#include "dprint.h"
#include "error.h"
#include "mem/mem.h"
#include "core_cmd.h"
#include "ut.h"
#include "re.h"
#include "route_struct.h"
#include "flags.h"
#include "trim.h"
#include "globals.h"
#include "rpc_lookup.h"
#include "sr_compat.h"

#include <sys/stat.h>
#include <regex.h>
#include <dlfcn.h>
#include <strings.h>
#include <stdlib.h>
#include <string.h>


struct sr_module* modules=0;

#ifdef STATIC_EXEC
	extern struct module_exports exec_exports;
#endif
#ifdef STATIC_TM
	extern struct module_exports tm_exports;
#endif

#ifdef STATIC_MAXFWD
	extern struct module_exports maxfwd_exports;
#endif

#ifdef STATIC_AUTH
	extern struct module_exports auth_exports;
#endif

#ifdef STATIC_RR
	extern struct module_exports rr_exports;
#endif

#ifdef STATIC_USRLOC
	extern struct module_exports usrloc_exports;
#endif

#ifdef STATIC_SL
	extern struct module_exports sl_exports;
#endif


int mod_response_cbk_no=0;
response_function* mod_response_cbks=0;


/* initializes statically built (compiled in) modules*/
int register_builtin_modules()
{
	int ret;

	ret=0;
#ifdef STATIC_TM
	ret=register_module(MODULE_INTERFACE_VER, &tm_exports,"built-in", 0);
	if (ret<0) return ret;
#endif

#ifdef STATIC_EXEC
	ret=register_module(MODULE_INTERFACE_VER, &exec_exports,"built-in", 0);
	if (ret<0) return ret;
#endif

#ifdef STATIC_MAXFWD
	ret=register_module(MODULE_INTERFACE_VER, &maxfwd_exports, "built-in", 0);
	if (ret<0) return ret;
#endif

#ifdef STATIC_AUTH
	ret=register_module(MODULE_INTERFACE_VER, &auth_exports, "built-in", 0);
	if (ret<0) return ret;
#endif

#ifdef STATIC_RR
	ret=register_module(MODULE_INTERFACE_VER, &rr_exports, "built-in", 0);
	if (ret<0) return ret;
#endif

#ifdef STATIC_USRLOC
	ret=register_module(MODULE_INTERFACE_VER, &usrloc_exports, "built-in", 0);
	if (ret<0) return ret;
#endif

#ifdef STATIC_SL
	ret=register_module(MODULE_INTERFACE_VER, &sl_exports, "built-in", 0);
	if (ret<0) return ret;
#endif

	return ret;
}



/* registers a module,  register_f= module register  functions
 * returns <0 on error, 0 on success */
static int register_module(unsigned ver, union module_exports_u* e,
					char* path, void* handle)
{
	int ret, i;
	struct sr_module* mod;

	ret=-1;

	/* add module to the list */
	if ((mod=pkg_malloc(sizeof(struct sr_module)))==0){
		LOG(L_ERR, "load_module: memory allocation failure\n");
		ret=E_OUT_OF_MEM;
		goto error;
	}
	memset(mod,0, sizeof(struct sr_module));
	mod->path=path;
	mod->handle=handle;
	mod->mod_interface_ver=ver;
	mod->exports=e;
	mod->next=modules;
	modules=mod;

	if (ver==1 && e->v1.items) {
		/* register module pseudo-variables for kamailio modules */
		LM_DBG("register PV from: %s\n", e->c.name);
		if (register_pvars_mod(e->c.name, e->v1.items)!=0) {
			LM_ERR("failed to register pseudo-variables for module %s\n",
				e->c.name);
			pkg_free(mod);
			return -1;
		}
	}else if (ver==0 && e->v0.rpc_methods){
		/* register rpcs for ser modules */
		i=rpc_register_array(e->v0.rpc_methods);
		if (i<0){
			ERR("failed to register RPCs for module %s\n", e->c.name);
			goto error;
		}else if (i>0){
			ERR("%d duplicate RPCs name detected while registering RPCs"
					" declared in modules %s\n", i, e->c.name);
			goto error;
		}
		/* i==0 => success */
	}

	return 0;
error:
	return ret;
}

#ifndef DLSYM_PREFIX
/* define it to null */
#define DLSYM_PREFIX
#endif

static inline int version_control(void *handle, char *path)
{
	char **m_ver;
	char **m_flags;
	char* error;

	m_ver=(char **)dlsym(handle, DLSYM_PREFIX "module_version");
	if ((error=(char *)dlerror())!=0) {
		LOG(L_ERR, "ERROR: no version info in module <%s>: %s\n",
			path, error );
		return 0;
	}
	m_flags=(char **)dlsym(handle, DLSYM_PREFIX "module_flags");
	if ((error=(char *)dlerror())!=0) {
		LOG(L_ERR, "ERROR: no compile flags info in module <%s>: %s\n",
			path, error );
		return 0;
	}
	if (!m_ver || !(*m_ver)) {
		LOG(L_ERR, "ERROR: no version in module <%s>\n", path );
		return 0;
	}
	if (!m_flags || !(*m_flags)) {
		LOG(L_ERR, "ERROR: no compile flags in module <%s>\n", path );
		return 0;
	}

	if (strcmp(SER_FULL_VERSION, *m_ver)==0){
		if (strcmp(SER_COMPILE_FLAGS, *m_flags)==0)
			return 1;
		else {
			LOG(L_ERR, "ERROR: module compile flags mismatch for %s "
						" \ncore: %s \nmodule: %s\n",
						path, SER_COMPILE_FLAGS, *m_flags);
			return 0;
		}
	}
	LOG(L_ERR, "ERROR: module version mismatch for %s; "
		"core: %s; module: %s\n", path, SER_FULL_VERSION, *m_ver );
	return 0;
}

/** load a sr module.
 * tries to load the module specified by mod_path.
 * If mod_path is 'modname' or 'modname.so' then
 *  <MODS_DIR>/<modname>.so will be tried and if this fails
 *  <MODS_DIR>/<modname>/<modname>.so
 * If mod_path contain a '/' it is assumed to be the 
 * path to the module and tried first. If fails and mod_path is not
 * absolute path (not starting with '/') then will try:
 *   <MODS_DIR>/mod_path
 * @param modname - path or module name
 * @return 0 on success , <0 on error
 */
int load_module(char* mod_path)
{
	void* handle;
	char* error;
	mod_register_function mr;
	union module_exports_u* exp;
	unsigned* mod_if_ver;
	struct sr_module* t;
	struct stat stat_buf;
	str modname;
	char* mdir;
	char* nxt_mdir;
	char* path;
	int mdir_len;
	int len;
	int dlflags;
	int new_dlflags;
	int retries;
	int path_type;

#ifndef RTLD_NOW
/* for openbsd */
#define RTLD_NOW DL_LAZY
#endif
	path=mod_path;
	path_type = 0;
	modname.s = path;
	modname.len = strlen(mod_path);
	if(modname.len>3 && strcmp(modname.s+modname.len-3, ".so")==0) {
		path_type = 1;
		modname.len -= 3;
	}
	if (!strchr(path, '/'))
		path_type |= 2;
	if((path_type&2) || path[0] != '/') {
		/* module name was given, we try to construct the path */
		mdir=mods_dir; /* search path */
		do{
			nxt_mdir=strchr(mdir, ':');
			if (nxt_mdir) mdir_len=(int)(nxt_mdir-mdir);
			else mdir_len=strlen(mdir);
			
			if(path_type&2) {
				/* try path <MODS_DIR>/<modname>.so */
				path = (char*)pkg_malloc(mdir_len + 1 /* "/" */ +
									modname.len + 3 /* ".so" */ + 1);
				if (path==0) goto error;
				memcpy(path, mdir, mdir_len);
				len = mdir_len;
				if (len != 0 && path[len - 1] != '/'){
					path[len]='/';
					len++;
				}
				path[len]=0;
				strcat(path, modname.s);
				if(!(path_type&1))
					strcat(path, ".so");

				if (stat(path, &stat_buf) == -1) {
					DBG("load_module: module file not found <%s>\n", path);
					pkg_free(path);

					/* try path <MODS_DIR>/<modname>/<modname>.so */
					path = (char*)pkg_malloc(
						mdir_len + 1 /* "/" */ +
						modname.len + 1 /* "/" */ +
						modname.len + 3 /* ".so" */ + 1);
					if (path==0) goto error;
					memcpy(path, mdir, mdir_len);
					len = mdir_len;
					if (len != 0 && path[len - 1] != '/') {
						path[len]='/';
						len++;
					}
					path[len]=0;
					strncat(path, modname.s, modname.len);
					strcat(path, "/");
					strcat(path, modname.s);
					if(!(path_type&1))
						strcat(path, ".so");

					if (stat(path, &stat_buf) == -1) {
						DBG("load_module: module file not found <%s>\n", path);
						pkg_free(path);
						path=0;
					}
				}
			} else {
				/* try mod_path - S compat */
				if(path==mod_path) {
					if (stat(path, &stat_buf) == -1) {
						DBG("load_module: module file not found <%s>\n", path);
						path=0;
					}
				}
				if(path==0) {
					/* try path <MODS_DIR>/mod_path - K compat */
					path = (char*)pkg_malloc(mdir_len + 1 /* "/" */ +
									strlen(mod_path) + 1);
					if (path==0) goto error;
					memcpy(path, mdir, mdir_len);
					len = mdir_len;
					if (len != 0 && path[len - 1] != '/'){
						path[len]='/';
						len++;
					}
					path[len]=0;
					strcat(path, mod_path);

					if (stat(path, &stat_buf) == -1) {
						DBG("load_module: module file not found <%s>\n", path);
						pkg_free(path);
						path=0;
					}
				}
			}
			mdir=nxt_mdir?nxt_mdir+1:0;
		}while(path==0 && mdir);
		if (path==0){
			LOG(L_ERR, "ERROR: load_module: could not find module <%.*s> in"
						" <%s>\n", modname.len, modname.s, mods_dir);
			goto error;
		}
	}
	DBG("load_module: trying to load <%s>\n", path);

	retries=2;
	dlflags=RTLD_NOW;
reload:
	handle=dlopen(path, dlflags); /* resolve all symbols now */
	if (handle==0){
		LOG(L_ERR, "ERROR: load_module: could not open module <%s>: %s\n",
			path, dlerror());
		goto error;
	}

	for(t=modules;t; t=t->next){
		if (t->handle==handle){
			LOG(L_WARN, "WARNING: load_module: attempting to load the same"
						" module twice (%s)\n", path);
			goto skip;
		}
	}
	/* version control */
	if (!version_control(handle, path)) {
		exit(0);
	}
	mod_if_ver = (unsigned *)dlsym(handle,
									DLSYM_PREFIX "module_interface_ver");
	if ( (error =(char*)dlerror())!=0 ){
		LOG(L_ERR, "ERROR: no module interface version in module <%s>\n",
					path );
		goto error1;
	}
	/* launch register */
	mr = (mod_register_function)dlsym(handle, DLSYM_PREFIX "mod_register");
	if (((error =(char*)dlerror())==0) && mr) {
		/* no error call it */
		new_dlflags=dlflags;
		if (mr(path, &new_dlflags, 0, 0)!=0) {
			LOG(L_ERR, "ERROR: load_module: %s: mod_register failed\n", path);
			goto error1;
		}
		if (new_dlflags!=dlflags && new_dlflags!=0) {
			/* we have to reload the module */
			dlclose(handle);
			dlflags=new_dlflags;
			retries--;
			if (retries>0) goto reload;
			LOG(L_ERR, "ERROR: load_module: %s: cannot agree"
					" on the dlflags\n", path);
			goto error;
		}
	}
	exp = (union module_exports_u*)dlsym(handle, DLSYM_PREFIX "exports");
	if ( (error =(char*)dlerror())!=0 ){
		LOG(L_ERR, "ERROR: load_module: %s\n", error);
		goto error1;
	}
	/* hack to allow for kamailio style dlflags inside exports */
	if (*mod_if_ver == 1) {
		new_dlflags = exp->v1.dlflags;
		if (new_dlflags!=dlflags && new_dlflags!=DEFAULT_DLFLAGS) {
			/* we have to reload the module */
			dlclose(handle);
			WARN("%s: exports dlflags interface is deprecated and it will not"
					"be supported in newer versions; consider using"
					" mod_register() instead", path);
			dlflags=new_dlflags;
			retries--;
			if (retries>0) goto reload;
			LOG(L_ERR, "ERROR: load_module: %s: cannot agree"
					" on the dlflags\n", path);
			goto error;
		}
	}
	if (register_module(*mod_if_ver, exp, path, handle)<0) goto error1;
	return 0;

error1:
	dlclose(handle);
error:
skip:
	if (path && path!=mod_path)
		pkg_free(path);
	return -1;
}



/* searches the module list for function name in module mod and returns 
 *  a pointer to the "name" function record union or 0 if not found
 * sets also *mod_if_ver to the module interface version (needed to know
 * which member of the union should be accessed v0 or v1)
 * mod==0 is a wildcard matching all modules
 * flags parameter is OR value of all flags that must match
 */
union cmd_export_u* find_mod_export_record(char* mod, char* name,
											int param_no, int flags,
											unsigned* mod_if_ver)
{
	struct sr_module* t;
	union cmd_export_u* cmd;
	int i;
	unsigned mver;

#define FIND_EXPORT_IN_MOD(VER) \
		if (t->exports->VER.cmds) \
			for(i=0, cmd=(void*)&t->exports->VER.cmds[0]; cmd->VER.name; \
					i++, cmd=(void*)&t->exports->VER.cmds[i]){\
				if((strcmp(name, cmd->VER.name)==0)&& \
					((cmd->VER.param_no==param_no) || \
					 (cmd->VER.param_no==VAR_PARAM_NO)) && \
					((cmd->VER.flags & flags) == flags) \
				){ \
					DBG("find_export_record: found <%s> in module %s [%s]\n", \
						name, t->exports->VER.name, t->path); \
					*mod_if_ver=mver; \
					return cmd; \
				} \
			}

	for(t=modules;t;t=t->next){
		if (mod!=0 && (strcmp(t->exports->c.name, mod) !=0))
			continue;
		mver=t->mod_interface_ver;
		switch (mver){
			case 0:
				FIND_EXPORT_IN_MOD(v0);
				break;
			case 1:
				FIND_EXPORT_IN_MOD(v1);
				break;
			default:
				BUG("invalid module interface version %d for modules %s\n",
						t->mod_interface_ver, t->path);
		}
	}
	DBG("find_export_record: <%s> not found \n", name);
	return 0;
}



/* searches the module list for function name and returns 
 *  a pointer to the "name" function record union or 0 if not found
 * sets also *mod_if_ver to the module interface version (needed to know
 * which member of the union should be accessed v0 or v1)
 * mod==0 is a wildcard matching all modules
 * flags parameter is OR value of all flags that must match
 */
union cmd_export_u* find_export_record(char* name,
											int param_no, int flags,
											unsigned* mod_if_ver)
{
	return find_mod_export_record(0, name, param_no, flags, mod_if_ver);
}



cmd_function find_export(char* name, int param_no, int flags)
{
	union cmd_export_u* cmd;
	unsigned mver;
	
	cmd = find_export_record(name, param_no, flags, &mver);
	return cmd?cmd->c.function:0;
}


rpc_export_t* find_rpc_export(char* name, int flags)
{
	return rpc_lookup((char*)name, strlen(name));
}


/*
 * searches the module list and returns pointer to "name" function in module
 * "mod"
 * 0 if not found
 * flags parameter is OR value of all flags that must match
 */
cmd_function find_mod_export(char* mod, char* name, int param_no, int flags)
{
	union cmd_export_u* cmd;
	unsigned mver;

	cmd=find_mod_export_record(mod, name, param_no, flags, &mver);
	if (cmd)
		return cmd->c.function;
	
	DBG("find_mod_export: <%s> in module <%s> not found\n", name, mod);
	return 0;
}


struct sr_module* find_module_by_name(char* mod) {
	struct sr_module* t;

	for(t = modules; t; t = t->next) {
		if (strcmp(mod, t->exports->c.name) == 0) {
			return t;
		}
	}
	DBG("find_module_by_name: module <%s> not found\n", mod);
	return 0;
}


void* find_param_export(struct sr_module* mod, char* name,
						modparam_t type_mask, modparam_t *param_type)
{
	param_export_t* param;

	if (!mod)
		return 0;
	param=0;
	switch(mod->mod_interface_ver){
		case 0:
			param=mod->exports->v0.params;
			break;
		case 1:
			param=mod->exports->v1.params;
			break;
		default:
			BUG("bad module interface version %d in module %s [%s]\n",
					mod->mod_interface_ver, mod->exports->c.name, mod->path);
			return 0;
	}
	for(;param && param->name ; param++) {
		if ((strcmp(name, param->name) == 0) &&
			((param->type & PARAM_TYPE_MASK(type_mask)) != 0)) {
			DBG("find_param_export: found <%s> in module %s [%s]\n",
				name, mod->exports->c.name, mod->path);
			*param_type = param->type;
			return param->param_pointer;
		}
	}
	DBG("find_param_export: parameter <%s> not found in module <%s>\n",
			name, mod->exports->c.name);
	return 0;
}


void destroy_modules()
{
	struct sr_module* t, *foo;

	t=modules;
	while(t) {
		foo=t->next;
		if (t->exports){
			switch(t->mod_interface_ver){
				case 0:
					if ((t->exports->v0.destroy_f)) t->exports->v0.destroy_f();
					break;
				case 1:
					if ((t->exports->v1.destroy_f)) t->exports->v1.destroy_f();
					break;
				default:
					BUG("bad module interface version %d in module %s [%s]\n",
						t->mod_interface_ver, t->exports->c.name,
						t->path);
			}
		}
		pkg_free(t);
		t=foo;
	}
	modules=0;
	if (mod_response_cbks){
		pkg_free(mod_response_cbks);
		mod_response_cbks=0;
	}
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
		if (t->exports){
			switch(t->mod_interface_ver){
				case 0:
					if (t->exports->v0.init_f)
						if (t->exports->v0.init_f() != 0) {
							LOG(L_ERR, "init_modules(): Error while"
										" initializing module %s\n",
										t->exports->v0.name);
							return -1;
						}
					if (t->exports->v0.response_f)
						mod_response_cbk_no++;
					break;
				case 1:
					if (t->exports->v1.init_f)
						if (t->exports->v1.init_f() != 0) {
							LOG(L_ERR, "init_modules(): Error while"
										" initializing module %s\n",
										t->exports->v1.name);
							return -1;
						}
					if (t->exports->v1.response_f)
						mod_response_cbk_no++;
					break;
				default:
					BUG("bad module interface version %d in module %s [%s]\n",
						t->exports->c.name, t->path);
					return -1;
			}
		}
	}
	mod_response_cbks=pkg_malloc(mod_response_cbk_no * 
									sizeof(response_function));
	if (mod_response_cbks==0){
		LOG(L_ERR, "init_modules(): memory allocation failure"
					" for %d response_f callbacks\n", mod_response_cbk_no);
		return -1;
	}
	for (t=modules, i=0; t && (i<mod_response_cbk_no); t=t->next){
		if (t->exports){
			switch(t->mod_interface_ver){
				case 0:
					if (t->exports->v0.response_f){
						mod_response_cbks[i]=t->exports->v0.response_f;
						i++;
					}
					break;
				case 1:
					if (t->exports->v1.response_f){
						mod_response_cbks[i]=t->exports->v1.response_f;
						i++;
					}
					break;
			}
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
	char* type;

	switch(rank) {
	case PROC_MAIN:     type = "PROC_MAIN";     break;
	case PROC_TIMER:    type = "PROC_TIMER";    break;
	case PROC_FIFO:     type = "PROC_FIFO";     break;
	case PROC_TCP_MAIN: type = "PROC_TCP_MAIN"; break;
	default:            type = "CHILD";         break;
	}
	DBG("init_child: initializing %s with rank %d\n", type, rank);


	for(t = modules; t; t = t->next) {
		switch(t->mod_interface_ver){
			case 0:
				if (t->exports->v0.init_child_f) {
					if ((t->exports->v0.init_child_f(rank)) < 0) {
						LOG(L_ERR, "init_child(): Initialization of child"
									" %d failed\n", rank);
						return -1;
					}
				}
				break;
			case 1:
				if (t->exports->v1.init_child_f) {
					if ((t->exports->v1.init_child_f(rank)) < 0) {
						LOG(L_ERR, "init_child(): Initialization of child"
									" %d failed\n", rank);
						return -1;
					}
				}
				break;
			default:
				BUG("bad module interface version %d in module %s [%s]\n",
						t->mod_interface_ver, t->exports->c.name,
						t->path);
				return -1;
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
		if (m->exports){
			switch(m->mod_interface_ver){
				case 0:
					if (m->exports->v0.init_child_f) {
						DBG("DEBUG: init_mod_child (%d): %s\n",
								rank, m->exports->v0.name);
						if (m->exports->v0.init_child_f(rank)<0) {
							LOG(L_ERR, "init_mod_child(): Error while"
										" initializing module %s\n",
										m->exports->v0.name);
							return -1;
						} else {
							/* module correctly initialized */
							return 0;
						}
					}
					/* no init function -- proceed with success */
					return 0;
				case 1:
					if (m->exports->v1.init_child_f) {
						DBG("DEBUG: init_mod_child (%d): %s\n",
								rank, m->exports->v1.name);
						if (m->exports->v1.init_child_f(rank)<0) {
							LOG(L_ERR, "init_mod_child(): Error while"
										" initializing module %s\n",
										m->exports->v1.name);
							return -1;
						} else {
							/* module correctly initialized */
							return 0;
						}
					}
					/* no init function -- proceed with success */
					return 0;
			}
		}
		/* no exports -- proceed with success */
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
		if (m->exports){
			switch(m->mod_interface_ver){
				case 0:
					if ( m->exports->v0.init_f) {
						DBG("DEBUG: init_mod: %s\n", m->exports->v0.name);
						if (m->exports->v0.init_f()!=0) {
							LOG(L_ERR, "init_mod(): Error while initializing"
										" module %s\n", m->exports->v0.name);
							return -1;
						} else {
							/* module correctly initialized */
							return 0;
						}
					}
					/* no init function -- proceed with success */
					return 0;
				case 1:
					if ( m->exports->v1.init_f) {
						DBG("DEBUG: init_mod: %s\n", m->exports->v1.name);
						if (m->exports->v1.init_f()!=0) {
							LOG(L_ERR, "init_mod(): Error while initializing"
										" module %s\n", m->exports->v1.name);
							return -1;
						} else {
							/* module correctly initialized */
							return 0;
						}
					}
					/* no init function -- proceed with success */
					return 0;
			}
		}
		/* no exports -- proceed with success */
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
	struct sr_module* t;
	int i;
	
	i = init_mod(modules);
	if(i!=0)
		return i;

	for(t = modules; t; t = t->next)
		if (t->exports){
			switch(t->mod_interface_ver){
				case 0:
					if (t->exports->v0.response_f)
						mod_response_cbk_no++;
					break;
				case 1:
					if (t->exports->v1.response_f)
						mod_response_cbk_no++;
					break;
			}
		}
	mod_response_cbks=pkg_malloc(mod_response_cbk_no * 
									sizeof(response_function));
	if (mod_response_cbks==0){
		LOG(L_ERR, "init_modules(): memory allocation failure"
					" for %d response_f callbacks\n", mod_response_cbk_no);
		return -1;
	}
	for (t=modules, i=0; t && (i<mod_response_cbk_no); t=t->next){
		if (t->exports){
			switch(t->mod_interface_ver){
				case 0:
					if (t->exports->v0.response_f){
						mod_response_cbks[i]=t->exports->v0.response_f;
						i++;
					}
					break;
				case 1:
					if (t->exports->v1.response_f){
						mod_response_cbks[i]=t->exports->v1.response_f;
						i++;
					}
					break;
			}
		}
	}
	
	return 0;
}

#endif


action_u_t *fixup_get_param(void **cur_param, int cur_param_no,
							int required_param_no)
{
	action_u_t *a, a2;
	/* cur_param points to a->u.string, get pointer to a */
	a = (void*) ((char *)cur_param - ((char *)&a2.u.string-(char *)&a2));
	return a + required_param_no - cur_param_no;
}

int fixup_get_param_count(void **cur_param, int cur_param_no)
{
	action_u_t *a;
	a = fixup_get_param(cur_param, cur_param_no, 0);
	if (a)
		return a->u.number;
	else
		return -1;
}


/* fixes flag params (resolves possible named flags)
 * use PARAM_USE_FUNC|PARAM_STRING as a param. type and create
 * a wrapper function that does just:
 * return fix_flag(type, val, "my_module", "my_param", &flag_var)
 * see also param_func_t.
 */
int fix_flag( modparam_t type, void* val,
					char* mod_name, char* param_name, int* flag)
{
	int num;
	int err;
	int f, len;
	char* s;
	char *p;

	if ((type & PARAM_STRING)==0){
		LOG(L_CRIT, "BUG: %s: fix_flag(%s): bad parameter type\n",
					mod_name, param_name);
		return -1;
	}
	s=(char*)val;
	len=strlen(s);
	f=-1;
	/* try to see if it's a number */
	num = str2s(s, len, &err);
	if (err != 0) {
		/* see if it's in the name:<no> format */
		p=strchr(s, ':');
		if (p){
			f= str2s(p+1, strlen(p+1), &err);
			if (err!=0){
				LOG(L_ERR, "ERROR: %s: invalid %s format:"
						" \"%s\"", mod_name, param_name, s);
				return -1;
			}
			*p=0;
		}
		if ((num=get_flag_no(s, len))<0){
			/* not declared yet, declare it */
			num=register_flag(s, f);
		}
		if (num<0){
			LOG(L_ERR, "ERROR: %s: bad %s %s\n", mod_name, param_name, s);
			return -1;
		} else if ((f>0) && (num!=f)){
			LOG(L_ERR, "WARNING: %s: flag %s already defined"
					" as %d (and not %d), using %s:%d\n",
					mod_name, s, num, f, s, num);
		}
	}
	*flag=num;
	return 0;
}

/*
 * Common function parameter fixups
 */

/** Generic parameter fixup function.
 *  Creates a fparam_t structure.
 *  @param type  contains allowed parameter types
 *  @param param is the parameter that will be fixed-up
 *
 * @return
 *    0 on success, 
 *    1 if the param doesn't match the specified type
 *    <0 on failure
 */
int fix_param(int type, void** param)
{
	fparam_t* p;
	str name, s;
	unsigned int num;
	int err;

	p = (fparam_t*)pkg_malloc(sizeof(fparam_t));
	if (!p) {
		ERR("No memory left\n");
		return E_OUT_OF_MEM;
	}
	memset(p, 0, sizeof(fparam_t));
	p->orig = *param;
	
	switch(type) {
		case FPARAM_UNSPEC:
			ERR("Invalid type value\n");
			goto error;
		case FPARAM_STRING:
			p->v.asciiz = *param;
			/* no break */
		case FPARAM_STR:
			p->v.str.s = (char*)*param;
			p->v.str.len = strlen(p->v.str.s);
			p->fixed = &p->v;
			break;
		case FPARAM_INT:
			s.s = (char*)*param;
			s.len = strlen(s.s);
			err = str2int(&s, &num);
			if (err == 0) {
				p->v.i = (int)num;
			} else {
				/* Not a number */
				pkg_free(p);
				return 1;
			}
			p->fixed = (void*)(long)num;
			break;
		case FPARAM_REGEX:
			if ((p->v.regex = pkg_malloc(sizeof(regex_t))) == 0) {
				ERR("No memory left\n");
				goto error;
			}
			if (regcomp(p->v.regex, *param,
						REG_EXTENDED|REG_ICASE|REG_NEWLINE)) {
				pkg_free(p->v.regex);
				p->v.regex=0;
				/* not a valid regex */
				goto no_match;
			}
			p->fixed = p->v.regex;
			break;
		case FPARAM_AVP:
			name.s = (char*)*param;
			name.len = strlen(name.s);
			trim(&name);
			if (!name.len || name.s[0] != '$') {
				/* Not an AVP identifier */
				goto no_match;
			}
			name.s++;
			name.len--;
			if (parse_avp_ident(&name, &p->v.avp) < 0) {
				/* invalid avp identifier (=> no match) */
				goto no_match;
			}
			p->fixed = &p->v;
			break;
		case FPARAM_SELECT:
			name.s = (char*)*param;
			name.len = strlen(name.s);
			trim(&name);
			if (!name.len || name.s[0] != '@') {
				/* Not a select identifier */
				goto no_match;
			}
			if (parse_select(&name.s, &p->v.select) < 0) {
				ERR("Error while parsing select identifier\n");
				goto error;
			}
			p->fixed = &p->v;
			break;
		case FPARAM_SUBST:
			s.s = *param;
			s.len = strlen(s.s);
			p->v.subst = subst_parser(&s);
			if (!p->v.subst) {
				ERR("Error while parsing regex substitution\n");
				goto error;
			}
			p->fixed = &p->v;
			break;
		case FPARAM_PVS:
			name.s = (char*)*param;
			name.len = strlen(name.s);
			trim(&name);
			if (!name.len || name.s[0] != '$'){
				/* not a pvs identifier */
				goto no_match;
			}
			p->v.pvs=pkg_malloc(sizeof(pv_spec_t));
			if (p->v.pvs==0){
				ERR("out of memory while parsing pv_spec_t\n");
				goto error;
			}
			if (pv_parse_spec2(&name, p->v.pvs, 1)==0){
				/* not a valid pvs identifier (but it might be an avp) */
				pkg_free(p->v.pvs);
				p->v.pvs=0;
				goto no_match;
			}
			p->fixed = p->v.pvs;
			break;
		case FPARAM_PVE:
			name.s = (char*)*param;
			name.len = strlen(name.s);
			if (pv_parse_format(&name, &p->v.pve)<0){
				ERR("bad PVE format: \"%.*s\"\n", name.len, name.s);
				goto error;
			}
			p->fixed = &p->v;
			break;
	}
	
	p->type = type;
	*param = (void*)p;
	return 0;
	
no_match:
	pkg_free(p);
	return 1;
error:
	pkg_free(p);
	return E_UNSPEC;
}



/** fparam_t free function.
 *  Frees the "content" of a fparam, but not the fparam itself.
 *  Assumes pkg_malloc'ed content.
 *  @param fp -  fparam to be freed
 *
 */
void fparam_free_contents(fparam_t* fp)
{

	if (fp==0)
		return;
	switch(fp->type) {
		case FPARAM_UNSPEC:
		case FPARAM_STRING: /* asciiz string, not str */
		case FPARAM_INT:
		case FPARAM_STR:
			/* nothing to do */
			break;
		case FPARAM_REGEX:
			if (fp->v.regex){
				regfree(fp->v.regex);
				pkg_free(fp->v.regex);
				fp->v.regex=0;
			}
			break;
		case FPARAM_AVP:
			free_avp_name(&fp->v.avp.flags, &fp->v.avp.name);
			break;
		case FPARAM_SELECT:
			if (fp->v.select){
				free_select(fp->v.select);
				fp->v.select=0;
			}
			break;
		case FPARAM_SUBST:
			if (fp->v.subst){
				subst_expr_free(fp->v.subst);
				fp->v.subst=0;
			}
			break;
		case FPARAM_PVS:
			if (fp->v.pvs){
				pv_spec_free(fp->v.pvs);
				fp->v.pvs=0;
			}
			break;
		case FPARAM_PVE:
			if (fp->v.pve){
				pv_elem_free_all(fp->v.pve);
				fp->v.pve=0;
			}
			break;
	}
	if (fp->orig){
		pkg_free(fp->orig);
		fp->orig=0;
	}
}



/** fix a param to one of the given types (mask).
  *
  * @param types - bitmap of the allowed types (e.g. FPARAM_INT|FPARAM_STR)
  * @param param - value/result
  * @return - 0 on success, -1 on error, 1 if param doesn't
  *           match any of the types
  */
int fix_param_types(int types, void** param)
{
	int ret;
	int t;
	
	for (t=types & ~(types-1); types; types&=(types-1), t=types & ~(types-1)){
		if ((ret=fix_param(t, param))<=0) return ret;
	}
	return E_UNSPEC;
}



/*
 * Fixup variable string, the parameter can be
 * AVP, SELECT, or ordinary string. AVP and select
 * identifiers will be resolved to their values during
 * runtime
 *
 * The parameter value will be converted to fparam structure
 * This function returns -1 on an error
 */
int fixup_var_str_12(void** param, int param_no)
{
	int ret;
	if ((sr_cfg_compat!=SR_COMPAT_SER) &&
		((ret = fix_param(FPARAM_PVS, param)) <= 0)) return ret;
	if ((ret = fix_param(FPARAM_AVP, param)) <= 0) return ret;
	if ((ret = fix_param(FPARAM_SELECT, param)) <= 0) return ret;
	if ((ret = fix_param(FPARAM_STR, param)) <= 0) return ret;
	ERR("Error while fixing parameter, AVP, SELECT, and str conversions"
			" failed\n");
	return -1;
}

/* Same as fixup_var_str_12 but applies to the 1st parameter only */
int fixup_var_str_1(void** param, int param_no)
{
	if (param_no == 1) return fixup_var_str_12(param, param_no);
	else return 0;
}

/* Same as fixup_var_str_12 but applies to the 2nd parameter only */
int fixup_var_str_2(void** param, int param_no)
{
	if (param_no == 2) return fixup_var_str_12(param, param_no);
	else return 0;
}


/*
 * Fixup variable integer, the parameter can be
 * AVP, SELECT, or ordinary integer. AVP and select
 * identifiers will be resolved to their values and
 * converted to int if necessary during runtime
 *
 * The parameter value will be converted to fparam structure
 * This function returns -1 on an error
 */
int fixup_var_int_12(void** param, int param_no)
{
	int ret;
	if ((sr_cfg_compat!=SR_COMPAT_SER) &&
		((ret = fix_param(FPARAM_PVS, param)) <= 0)) return ret;
	if ((ret = fix_param(FPARAM_AVP, param)) <= 0) return ret;
	if ((ret = fix_param(FPARAM_SELECT, param)) <= 0) return ret;
	if ((ret = fix_param(FPARAM_INT, param)) <= 0) return ret;
	ERR("Error while fixing parameter, AVP, SELECT, and int conversions"
			" failed\n");
	return -1;
}

/* Same as fixup_var_int_12 but applies to the 1st parameter only */
int fixup_var_int_1(void** param, int param_no)
{
	if (param_no == 1) return fixup_var_int_12(param, param_no);
	else return 0;
}

/* Same as fixup_var_int_12 but applies to the 2nd parameter only */
int fixup_var_int_2(void** param, int param_no)
{
	if (param_no == 2) return fixup_var_int_12(param, param_no);
	else return 0;
}


/*
 * The parameter must be a regular expression which must compile, the
 * parameter will be converted to compiled regex
 */
int fixup_regex_12(void** param, int param_no)
{
	int ret;

	if ((ret = fix_param(FPARAM_REGEX, param)) <= 0) return ret;
	ERR("Error while compiling regex in function parameter\n");
	return -1;
}

/* Same as fixup_regex_12 but applies to the 1st parameter only */
int fixup_regex_1(void** param, int param_no)
{
	if (param_no == 1) return fixup_regex_12(param, param_no);
	else return 0;
}

/* Same as fixup_regex_12 but applies to the 2nd parameter only */
int fixup_regex_2(void** param, int param_no)
{
	if (param_no == 2) return fixup_regex_12(param, param_no);
	else return 0;
}

/*
 * The string parameter will be converted to integer
 */
int fixup_int_12(void** param, int param_no)
{
	int ret;

	if ((ret = fix_param(FPARAM_INT, param)) <= 0) return ret;
	ERR("Cannot function parameter to integer\n");
	return -1;

}

/* Same as fixup_int_12 but applies to the 1st parameter only */
int fixup_int_1(void** param, int param_no)
{
	if (param_no == 1) return fixup_int_12(param, param_no);
	else return 0;
}

/* Same as fixup_int_12 but applies to the 2nd parameter only */
int fixup_int_2(void** param, int param_no)
{
	if (param_no == 2) return fixup_int_12(param, param_no);
	else return 0;
}

/*
 * Parse the parameter as static string, do not resolve
 * AVPs or selects, convert the parameter to str structure
 */
int fixup_str_12(void** param, int param_no)
{
	int ret;

	if ((ret = fix_param(FPARAM_STR, param)) <= 0) return ret;
	ERR("Cannot function parameter to string\n");
	return -1;
}

/* Same as fixup_str_12 but applies to the 1st parameter only */
int fixup_str_1(void** param, int param_no)
{
	if (param_no == 1) return fixup_str_12(param, param_no);
	else return 0;
}

/* Same as fixup_str_12 but applies to the 2nd parameter only */
int fixup_str_2(void** param, int param_no)
{
	if (param_no == 2) return fixup_str_12(param, param_no);
	else return 0;
}



#define PV_PRINT_BUF_SIZE  1024
#define PV_PRINT_BUF_NO    6
/** Get the function parameter value as string.
 *  @return  0 - Success
 *          -1 - Cannot get value
 */
int get_str_fparam(str* dst, struct sip_msg* msg, fparam_t* param)
{
	int_str val;
	int ret;
	avp_t* avp;
	pv_value_t pv_val;
	static int buf_itr = 0; /* ugly hack needed for PVE */
	static char pve_buf[PV_PRINT_BUF_NO][PV_PRINT_BUF_SIZE];
	
	switch(param->type) {
		case FPARAM_REGEX:
		case FPARAM_UNSPEC:
		case FPARAM_INT:
			return -1;
		case FPARAM_STRING:
			dst->s = param->v.asciiz;
			dst->len = strlen(param->v.asciiz);
			break;
		case FPARAM_STR:
			*dst = param->v.str;
			break;
		case FPARAM_AVP:
			avp = search_first_avp(param->v.avp.flags, param->v.avp.name,
									&val, 0);
			if (unlikely(!avp)) {
				DBG("Could not find AVP from function parameter '%s'\n",
						param->orig);
				return -1;
			}
			if (likely(avp->flags & AVP_VAL_STR)) {
				*dst = val.s;
			} else {
		 		/* The caller does not know of what type the AVP will be so
				 * convert int AVPs into string here
				 */
				dst->s = int2str(val.n, &dst->len);
			}
			break;
		case FPARAM_SELECT:
			ret = run_select(dst, param->v.select, msg);
			if (unlikely(ret < 0 || ret > 0)) return -1;
			break;
		case FPARAM_PVS:
			if (likely((pv_get_spec_value(msg, param->v.pvs, &pv_val)==0) &&
					   ((pv_val.flags&(PV_VAL_NULL|PV_VAL_STR))==PV_VAL_STR))){
					*dst=pv_val.rs;
			}else{
				ERR("Could not convert PV to str\n");
				return -1;
			}
			break;
		case FPARAM_PVE:
			dst->s=pve_buf[buf_itr];
			dst->len=PV_PRINT_BUF_SIZE;
			buf_itr = (buf_itr+1)%PV_PRINT_BUF_NO;
			if (unlikely(pv_printf(msg, param->v.pve, dst->s, &dst->len)!=0)){
				ERR("Could not convert the PV-formated string to str\n");
				dst->len=0;
				return -1;
			};
			break;
	}
	return 0;
}


/** Get the function parameter value as integer.
 *  @return  0 - Success
 *          -1 - Cannot get value
 */
int get_int_fparam(int* dst, struct sip_msg* msg, fparam_t* param)
{
	int_str val;
	int ret;
	avp_t* avp;
	str tmp;
	pv_value_t pv_val;

	switch(param->type) {
		case FPARAM_INT:
			*dst = param->v.i;
			return 0;
		case FPARAM_REGEX:
		case FPARAM_UNSPEC:
		case FPARAM_STRING:
		case FPARAM_STR:
			return -1;
		case FPARAM_AVP:
			avp = search_first_avp(param->v.avp.flags, param->v.avp.name,
									&val, 0);
			if (unlikely(!avp)) {
				DBG("Could not find AVP from function parameter '%s'\n",
						param->orig);
				return -1;
			}
			if (avp->flags & AVP_VAL_STR) {
				if (str2int(&val.s, (unsigned int*)dst) < 0) {
					ERR("Could not convert AVP string value to int\n");
					return -1;
				}
			} else {
				*dst = val.n;
			}
			break;
		case FPARAM_SELECT:
			ret = run_select(&tmp, param->v.select, msg);
			if (unlikely(ret < 0 || ret > 0)) return -1;
			if (unlikely(str2int(&tmp, (unsigned int*)dst) < 0)) {
				ERR("Could not convert select result to int\n");
				return -1;
			}
			break;
		case FPARAM_PVS:
			if (likely((pv_get_spec_value(msg, param->v.pvs, &pv_val)==0) &&
					   ((pv_val.flags&(PV_VAL_NULL|PV_VAL_INT))==PV_VAL_INT))){
					*dst=pv_val.ri;
			}else{
				ERR("Could not convert PV to int\n");
				return -1;
			}
			break;
		case FPARAM_PVE:
			return -1;
	}
	return 0;
}

/**
 * Retrieve the compiled RegExp.
 * @return: 0 for success, negative on error.
 */
int get_regex_fparam(regex_t *dst, struct sip_msg* msg, fparam_t* param)
{
	switch (param->type) {
		case FPARAM_REGEX:
			*dst = *param->v.regex;
			return 0;
		default:
			ERR("unexpected parameter type (%d), instead of regexp.\n", 
					param->type);
	}
	return -1;
}
