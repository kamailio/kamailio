/*
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * @file
 * @brief Kamailio core :: modules loading, structures declarations and utilities
 * @ingroup core
 * Module: \ref core
 */


#include "sr_module.h"
#include "mod_fix.h"
#include "dprint.h"
#include "error.h"
#include "mem/mem.h"
#include "core_cmd.h"
#include "ut.h"
#include "re.h"
#include "route_struct.h"
#include "flags.h"
#include "trim.h"
#include "pvapi.h"
#include "globals.h"
#include "rpc_lookup.h"
#include "sr_compat.h"
#include "ppcfg.h"
#include "async_task.h"

#include <sys/stat.h>
#include <regex.h>
#include <dlfcn.h>
#include <strings.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h> /* for offsetof */


struct sr_module* modules=0;

/*We need to define this symbol on Solaris becuase libcurl relies on libnspr which looks for this symbol.
  If it is not defined, dynamic module loading (dlsym) fails */
#ifdef __OS_solaris
	int nspr_use_zone_allocator = 0;
#endif


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

#ifndef offsetof
#warning "use null pointer dereference for offsetof"
#define offsetof(st, m) \
		((size_t) ( (char *)&((st *)(0))->m - (char *)0 ))
#endif

int mod_response_cbk_no=0;
response_function* mod_response_cbks=0;

/* number of usec to wait before initializing a module */
static unsigned int modinit_delay = 0;

unsigned int set_modinit_delay(unsigned int v)
{
	unsigned int r;
	r =  modinit_delay;
	modinit_delay = v;
	return r;
}

/* keep state if server is in destroy modules phase */
static int _sr_destroy_modules_phase = 0;

/**
 * return destroy modules phase state
 */
int destroy_modules_phase(void)
{
	return _sr_destroy_modules_phase;
}

/**
 * if bit 1 set, SIP worker processes handle RPC commands as well
 * if bit 2 set, RPC worker processes handle SIP commands as well
 */
static int child_sip_rpc_mode = 0;

#define CHILD_SIP_RPC	1<<0
#define CHILD_RPC_SIP	1<<1

void set_child_sip_rpc_mode(void)
{
	child_sip_rpc_mode |= CHILD_SIP_RPC;
}

void set_child_rpc_sip_mode(void)
{
	child_sip_rpc_mode |= CHILD_RPC_SIP;
}

int is_rpc_worker(int rank)
{
	if(rank==PROC_RPC
			|| (rank>PROC_MAIN && (child_sip_rpc_mode&CHILD_SIP_RPC)!=0))
		return 1;
	return 0;
}

int is_sip_worker(int rank)
{
	if(rank>PROC_MAIN
			|| ((rank==PROC_RPC || rank==PROC_NOCHLDINIT)
					&& (child_sip_rpc_mode&CHILD_RPC_SIP)!=0))
		return 1;
	return 0;
}

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



/** convert cmd exports to current format.
 * @param ver - module interface versions (0 == ser, 1 == kam).
 * @param src - null terminated array of cmd exports
 *              (either ser_cmd_export_t or kam_cmd_export_t, depending
 *               on ver).
 * @param mod - pointer to module exports structure.
 * @return - pkg_malloc'ed null terminated sr_cmd_export_v31_t array with
 *           the converted cmd exports  or 0 on error.
 */
static sr31_cmd_export_t* sr_cmd_exports_convert(unsigned ver,
													void* src, void* mod)
{
	int i, n;
	ser_cmd_export_t* ser_cmd;
	kam_cmd_export_t* kam_cmd;
	sr31_cmd_export_t* ret;

	ser_cmd = 0;
	kam_cmd = 0;
	ret = 0;
	n = 0;
	/* count the number of elements */
	if (ver == 0) {
		ser_cmd = src;
		for (; ser_cmd[n].name; n++);
	} else if (ver == 1) {
		kam_cmd = src;
		for (; kam_cmd[n].name; n++);
	} else goto error; /* unknown interface version */
	/* alloc & init new array */
	ret = pkg_malloc(sizeof(*ret)*(n+1));
	memset(ret, 0, sizeof(*ret)*(n+1));
	/* convert/copy */
	for (i=0; i < n; i++) {
		if (ver == 0) {
			ret[i].name = ser_cmd[i].name;
			ret[i].function = ser_cmd[i].function;
			ret[i].param_no = ser_cmd[i].param_no;
			ret[i].fixup = ser_cmd[i].fixup;
			ret[i].free_fixup = 0; /* no present in ser  <= 2.1 */
			ret[i].flags = ser_cmd[i].flags;
		} else {
			ret[i].name = kam_cmd[i].name;
			ret[i].function = kam_cmd[i].function;
			ret[i].param_no = kam_cmd[i].param_no;
			ret[i].fixup = kam_cmd[i].fixup;
			ret[i].free_fixup = kam_cmd[i].free_fixup;
			ret[i].flags = kam_cmd[i].flags;
		}
		/* 3.1+ specific stuff */
		ret[i].fixup_flags = 0;
		ret[i].module_exports = mod;
		/* fill known free fixups */
		if (ret[i].fixup && ret[i].free_fixup == 0)
			ret[i].free_fixup = get_fixup_free(ret[i].fixup);
	}
	return ret;
error:
	return 0;
}



/* registers a module,  register_f= module register  functions
 * returns <0 on error, 0 on success */
static int register_module(unsigned ver, union module_exports_u* e,
					char* path, void* handle)
{
	int ret, i;
	struct sr_module* mod;
	char defmod[64];

	ret=-1;

	/* add module to the list */
	if ((mod=pkg_malloc(sizeof(struct sr_module)))==0){
		LM_ERR("memory allocation failure\n");
		ret=E_OUT_OF_MEM;
		goto error;
	}
	memset(mod,0, sizeof(struct sr_module));
	mod->path=path;
	mod->handle=handle;
	mod->orig_mod_interface_ver=ver;
	/* convert exports to sr31 format */
	if (ver == 0) {
		/* ser <= 3.0 */
		mod->exports.name = e->v0.name;
		if (e->v0.cmds) {
			mod->exports.cmds = sr_cmd_exports_convert(ver, e->v0.cmds, mod);
			if (mod->exports.cmds == 0) {
				LM_ERR("failed to convert module command exports to 3.1 format"
						" for module \"%s\" (%s), interface version %d\n",
						mod->exports.name, mod->path, ver);
				ret = E_UNSPEC;
				goto error;
			}
		}
		mod->exports.params = e->v0.params;
		mod->exports.init_f = e->v0.init_f;
		mod->exports.response_f = e->v0.response_f;
		mod->exports.destroy_f = e->v0.destroy_f;
		mod->exports.onbreak_f = e->v0.onbreak_f;
		mod->exports.init_child_f = e->v0.init_child_f;
		mod->exports.dlflags = 0; /* not used in ser <= 3.0 */
		mod->exports.rpc_methods = e->v0.rpc_methods;
		/* the rest are 0, not used in ser */
	} else if (ver == 1) {
		/* kamailio <= 3.0 */
		mod->exports.name = e->v1.name;
		if (e->v1.cmds) {
			mod->exports.cmds = sr_cmd_exports_convert(ver, e->v1.cmds, mod);
			if (mod->exports.cmds == 0) {
				LM_ERR("failed to convert module command exports to 3.1 format"
						" for module \"%s\" (%s), interface version %d\n",
						mod->exports.name, mod->path, ver);
				ret = E_UNSPEC;
				goto error;
			}
		}
		mod->exports.params = e->v1.params;
		mod->exports.init_f = e->v1.init_f;
		mod->exports.response_f = e->v1.response_f;
		mod->exports.destroy_f = e->v1.destroy_f;
		mod->exports.onbreak_f = 0; /* not used in k <= 3.0 */
		mod->exports.init_child_f = e->v1.init_child_f;
		mod->exports.dlflags = e->v1.dlflags;
		mod->exports.rpc_methods = 0; /* not used in k <= 3.0 */
		mod->exports.stats = e->v1.stats;
		mod->exports.nn_cmds = e->v1.nn_cmds;
		mod->exports.items = e->v1.items;
		mod->exports.procs = e->v1.procs;
	} else {
		LM_ERR("unsupported module interface version %d\n", ver);
		ret = E_UNSPEC;
		goto error;
	}

	if (mod->exports.items) {
		/* register module pseudo-variables for kamailio modules */
		LM_DBG("register PV from: %s\n", mod->exports.name);
		if (register_pvars_mod(mod->exports.name, mod->exports.items)!=0) {
			LM_ERR("failed to register pseudo-variables for module %s (%s)\n",
				mod->exports.name, path);
			ret = E_UNSPEC;
			goto error;
		}
	}
	if (mod->exports.rpc_methods){
		/* register rpcs for ser modules */
		i=rpc_register_array(mod->exports.rpc_methods);
		if (i<0){
			LM_ERR("failed to register RPCs for module %s (%s)\n",
					mod->exports.name, path);
			ret = E_UNSPEC;
			goto error;
		}else if (i>0){
			LM_ERR("%d duplicate RPCs name detected while registering RPCs"
					" declared in module %s (%s)\n",
					i, mod->exports.name, path);
			ret = E_UNSPEC;
			goto error;
		}
		/* i==0 => success */
	}

	/* add cfg define for each module: MOD_modulename */
	if(strlen(mod->exports.name)>=60) {
		LM_ERR("too long module name: %s\n", mod->exports.name);
		goto error;
	}
	strcpy(defmod, "MOD_");
	strcat(defmod, mod->exports.name);
	pp_define_set_type(0);
	if(pp_define(strlen(defmod), defmod)<0) {
		LM_ERR("unable to set cfg define for module: %s\n",
				mod->exports.name);
		goto error;
	}

	/* link module in the list */
	mod->next=modules;
	modules=mod;
	return 0;
error:
	if (mod)
		pkg_free(mod);
	return ret;
}

static inline int version_control(void *handle, char *path)
{
	char **m_ver;
	char **m_flags;
	char* error;

	m_ver=(char **)dlsym(handle, "module_version");
	if ((error=(char *)dlerror())!=0) {
		LM_ERR("no version info in module <%s>: %s\n", path, error);
		return 0;
	}
	m_flags=(char **)dlsym(handle, "module_flags");
	if ((error=(char *)dlerror())!=0) {
		LM_ERR("no compile flags info in module <%s>: %s\n", path, error);
		return 0;
	}
	if (!m_ver || !(*m_ver)) {
		LM_ERR("no version in module <%s>\n", path );
		return 0;
	}
	if (!m_flags || !(*m_flags)) {
		LM_ERR("no compile flags in module <%s>\n", path );
		return 0;
	}

	if (strcmp(SER_FULL_VERSION, *m_ver)==0){
		if (strcmp(SER_COMPILE_FLAGS, *m_flags)==0)
			return 1;
		else {
			LM_ERR("module compile flags mismatch for %s "
						" \ncore: %s \nmodule: %s\n",
						path, SER_COMPILE_FLAGS, *m_flags);
			return 0;
		}
	}
	LM_ERR("module version mismatch for %s; "
		"core: %s; module: %s\n", path, SER_FULL_VERSION, *m_ver );
	return 0;
}

/**
 * \brief load a sr module
 *
 * tries to load the module specified by mod_path.
 * If mod_path is 'modname' or 'modname.so' then
 *  \<MODS_DIR\>/\<modname\>.so will be tried and if this fails
 *  \<MODS_DIR\>/\<modname\>/\<modname\>.so
 * If mod_path contain a '/' it is assumed to be the
 * path to the module and tried first. If fails and mod_path is not
 * absolute path (not starting with '/') then will try:
 * \<MODS_DIR\>/mod_path
 * @param mod_path path or module name
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
	str expref;
	char exbuf[64];

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
					LM_DBG("module file not found <%s>\n", path);
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
						LM_DBG("module file not found <%s>\n", path);
						pkg_free(path);
						path=0;
					}
				}
			} else {
				/* try mod_path - S compat */
				if(path==mod_path) {
					if (stat(path, &stat_buf) == -1) {
						LM_DBG("module file not found <%s>\n", path);
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
						LM_DBG("module file not found <%s>\n", path);
						pkg_free(path);
						path=0;
					}
				}
			}
			mdir=nxt_mdir?nxt_mdir+1:0;
		}while(path==0 && mdir);
		if (path==0){
			LM_ERR("could not find module <%.*s> in <%s>\n",
						modname.len, modname.s, mods_dir);
			goto error;
		}
	}
	LM_DBG("trying to load <%s>\n", path);

	retries=2;
	dlflags=RTLD_NOW;
reload:
	handle=dlopen(path, dlflags); /* resolve all symbols now */
	if (handle==0){
		LM_ERR("could not open module <%s>: %s\n", path, dlerror());
		goto error;
	}

	for(t=modules;t; t=t->next){
		if (t->handle==handle){
			LM_WARN("attempting to load the same module twice (%s)\n", path);
			goto skip;
		}
	}
	/* version control */
	if (!version_control(handle, path)) {
		exit(-1);
	}
	mod_if_ver = (unsigned *)dlsym(handle, "module_interface_ver");
	if (mod_if_ver==NULL || (error =(char*)dlerror())!=0 ){
		LM_ERR("no module interface version in module <%s>\n", path );
		goto error1;
	}
	/* launch register */
	mr = (mod_register_function)dlsym(handle, "mod_register");
	if (((error =(char*)dlerror())==0) && mr) {
		/* no error call it */
		new_dlflags=dlflags;
		if (mr(path, &new_dlflags, 0, 0)!=0) {
			LM_ERR("%s: mod_register failed\n", path);
			goto error1;
		}
		if (new_dlflags!=dlflags && new_dlflags!=0) {
			/* we have to reload the module */
			dlclose(handle);
			dlflags=new_dlflags;
			retries--;
			if (retries>0) goto reload;
			LM_ERR("%s: cannot agree on the dlflags\n", path);
			goto error;
		}
	}
	exp = (union module_exports_u*)dlsym(handle, "exports");
	if(exp==NULL) {
		error =(char*)dlerror();
		LM_DBG("attempt to lookup exports structure failed - dlerror: %s\n",
				(error)?error:"none");
		/* 'exports' structure not found, look up for '_modulename_exports' */
		mdir = strrchr(mod_path, '/');
		if (!mdir) {
			expref.s = mod_path;
		} else {
			expref.s = mdir+1;
		}
		expref.len = strlen(expref.s);
		if(expref.len>3 && strcmp(expref.s+expref.len-3, ".so")==0)
			expref.len -= 3;
		snprintf(exbuf, 62, "_%.*s_exports", expref.len, expref.s);
		LM_DBG("looking up exports with name: %s\n", exbuf);
		exp = (union module_exports_u*)dlsym(handle, exbuf);
		if(exp==NULL || (error =(char*)dlerror())!=0 ){
			LM_ERR("failure for exports symbol: %s - dlerror: %s\n",
					exbuf, (error)?error:"none");
			goto error1;
		}
	}
	/* hack to allow for kamailio style dlflags inside exports */
	if (*mod_if_ver == 1) {
		new_dlflags = exp->v1.dlflags;
		if (new_dlflags!=dlflags && new_dlflags!=DEFAULT_DLFLAGS) {
			/* we have to reload the module */
			dlclose(handle);
			DEBUG("%s: exports dlflags interface is deprecated and it will not"
					" be supported in newer versions; consider using"
					" mod_register() instead\n", path);
			dlflags=new_dlflags;
			retries--;
			if (retries>0) goto reload;
			LM_ERR("%s: cannot agree on the dlflags\n", path);
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

/**
 * test if command flags are compatible with route block flags (type)
 * - decide if the command is allowed to run within a specific route block
 * - return: 1 if allowed; 0 if not allowed
 */
static inline int sr_cmd_flags_match(int cflags, int rflags)
{
	if((cflags & rflags) == rflags) {
		return 1;
	}
	if((rflags==EVENT_ROUTE) && (cflags & EVENT_ROUTE)) {
		return 1;
	}
	return 0;
}

/* searches the module list for function name in module mod and returns
 *  a pointer to the "name" function record union or 0 if not found
 * sets also *mod_if_ver to the original module interface version.
 * mod==0 is a wildcard matching all modules
 * flags parameter is OR value of all flags that must match
 */
sr31_cmd_export_t* find_mod_export_record(char* mod, char* name,
											int param_no, int flags,
											unsigned* mod_if_ver)
{
	struct sr_module* t;
	sr31_cmd_export_t* cmd;

	for(t=modules;t;t=t->next){
		if (mod!=0 && (strcmp(t->exports.name, mod) !=0))
			continue;
		if (t->exports.cmds)
			for(cmd=&t->exports.cmds[0]; cmd->name; cmd++) {
				if((strcmp(name, cmd->name) == 0) &&
					((cmd->param_no == param_no) ||
					(cmd->param_no==VAR_PARAM_NO)) &&
					(sr_cmd_flags_match(cmd->flags, flags)==1)
				){
					LM_DBG("found export of <%s> in module %s [%s]\n",
						name, t->exports.name, t->path);
					*mod_if_ver=t->orig_mod_interface_ver;
					return cmd;
				}
			}
	}
	LM_DBG("export of <%s> not found (flags %d)\n", name, flags);
	return 0;
}



/* searches the module list for function name and returns
 *  a pointer to the "name" function record union or 0 if not found
 * sets also *mod_if_ver to the module interface version (needed to know
 * which member of the union should be accessed v0 or v1)
 * mod==0 is a wildcard matching all modules
 * flags parameter is OR value of all flags that must match
 */
sr31_cmd_export_t* find_export_record(char* name,
											int param_no, int flags,
											unsigned* mod_if_ver)
{
	return find_mod_export_record(0, name, param_no, flags, mod_if_ver);
}



cmd_function find_export(char* name, int param_no, int flags)
{
	sr31_cmd_export_t* cmd;
	unsigned mver;

	cmd = find_export_record(name, param_no, flags, &mver);
	return cmd?cmd->function:0;
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
	sr31_cmd_export_t* cmd;
	unsigned mver;

	cmd=find_mod_export_record(mod, name, param_no, flags, &mver);
	if (cmd)
		return cmd->function;

	LM_DBG("<%s> in module <%s> not found\n", name, mod);
	return 0;
}


struct sr_module* find_module_by_name(char* mod) {
	struct sr_module* t;

	for(t = modules; t; t = t->next) {
		if (strcmp(mod, t->exports.name) == 0) {
			return t;
		}
	}
	LM_DBG("module <%s> not found\n", mod);
	return 0;
}

sr_module_t* get_loaded_modules(void) {
	return modules;
}

/*!
 * \brief Find a parameter with given type
 * \param mod module
 * \param name parameter name
 * \param type_mask parameter mask
 * \param param_type parameter type
 * \return parameter address in memory, if there is no such parameter, NULL is returned
 */
void* find_param_export(struct sr_module* mod, char* name,
						modparam_t type_mask, modparam_t *param_type)
{
	param_export_t* param;

	if (!mod)
		return 0;
	for(param = mod->exports.params ;param && param->name ; param++) {
		if ((strcmp(name, param->name) == 0) &&
			((param->type & PARAM_TYPE_MASK(type_mask)) != 0)) {
			LM_DBG("found <%s> in module %s [%s]\n",
				name, mod->exports.name, mod->path);
			*param_type = param->type;
			return param->param_pointer;
		}
	}
	LM_DBG("parameter <%s> not found in module <%s>\n",
			name, mod->exports.name);
	return 0;
}


void destroy_modules()
{
	struct sr_module* t, *foo;

	_sr_destroy_modules_phase = 1;
	/* call first destroy function from each module */
	t=modules;
	while(t) {
		foo=t->next;
		if (t->exports.destroy_f){
			t->exports.destroy_f();
		}
		t=foo;
	}
	/* free module exports structures */
	t=modules;
	while(t) {
		foo=t->next;
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

	if(async_task_init()<0)
		return -1;

	for(t = modules; t; t = t->next) {
		if (t->exports.init_f) {
			if (t->exports.init_f() != 0) {
				LM_ERR("Error while initializing module %s\n", t->exports.name);
				return -1;
			}
			/* delay next module init, if configured */
			if(unlikely(modinit_delay>0))
				sleep_us(modinit_delay);
		}
		if (t->exports.response_f)
			mod_response_cbk_no++;
	}
	mod_response_cbks=pkg_malloc(mod_response_cbk_no *
									sizeof(response_function));
	if (mod_response_cbks==0){
		LM_ERR("memory allocation failure for %d response_f callbacks\n",
					mod_response_cbk_no);
		return -1;
	}
	for (t=modules, i=0; t && (i<mod_response_cbk_no); t=t->next) {
		if (t->exports.response_f) {
			mod_response_cbks[i]=t->exports.response_f;
			i++;
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
	LM_DBG("initializing %s with rank %d\n", type, rank);

	if(async_task_child_init(rank)<0)
		return -1;

	for(t = modules; t; t = t->next) {
		if (t->exports.init_child_f) {
			if ((t->exports.init_child_f(rank)) < 0) {
				LM_ERR("Initialization of child %d failed\n", rank);
				return -1;
			}
		}
	}
	return 0;
}

#else


/* recursive module child initialization; (recursion is used to
 * process the module linear list in the same order in
 * which modules are loaded in config file
 */

static int init_mod_child( struct sr_module* m, int rank )
{
	if (m) {
		/* iterate through the list; if error occurs,
		 * propagate it up the stack
		 */
		if (init_mod_child(m->next, rank)!=0) return -1;
		if (m->exports.init_child_f) {
			LM_DBG("idx %d rank %d: %s [%s]\n", process_no, rank,
					m->exports.name, my_desc());
			if (m->exports.init_child_f(rank)<0) {
				LM_ERR("error while initializing module %s (%s)"
						" (idx: %d rank: %d desc: [%s])\n",
						m->exports.name, m->path, process_no, rank, my_desc());
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
	if(async_task_child_init(rank)<0)
		return -1;

	return init_mod_child(modules, rank);
}



/* recursive module initialization; (recursion is used to
 * process the module linear list in the same order in
 * which modules are loaded in config file
*/

static int init_mod( struct sr_module* m )
{
	if (m) {
		/* iterate through the list; if error occurs,
		 * propagate it up the stack
		 */
		if (init_mod(m->next)!=0) return -1;
			if (m->exports.init_f) {
				LM_DBG("%s\n", m->exports.name);
				if (m->exports.init_f()!=0) {
					LM_ERR("Error while initializing module %s (%s)\n",
								m->exports.name, m->path);
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
	struct sr_module* t;
	int i;

	if(async_task_init()<0)
		return -1;

	i = init_mod(modules);
	if(i!=0)
		return i;

	for(t = modules; t; t = t->next)
		if (t->exports.response_f)
			mod_response_cbk_no++;
	mod_response_cbks=pkg_malloc(mod_response_cbk_no *
									sizeof(response_function));
	if (mod_response_cbks==0){
		LM_ERR("memory allocation failure for %d response_f callbacks\n", mod_response_cbk_no);
		return -1;
	}
	for (t=modules, i=0; t && (i<mod_response_cbk_no); t=t->next)
		if (t->exports.response_f) {
			mod_response_cbks[i]=t->exports.response_f;
			i++;
		}

	return 0;
}

#endif


action_u_t *fixup_get_param(void **cur_param, int cur_param_no,
							int required_param_no)
{
	action_u_t *a;
	/* cur_param points to a->u.string, get pointer to a */
	a = (void*) ((char *)cur_param - offsetof(action_u_t, u.string));
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



/** get a pointer to a parameter internal type.
 * @param param
 * @return pointer to the parameter internal type.
 */
action_param_type* fixup_get_param_ptype(void** param)
{
	action_u_t* a;
	a = (void*)((char*)param - offsetof(action_u_t, u.string));
	return &a->type;
}


/** get a parameter internal type.
 * @see fixup_get_param_ptype().
 * @return paramter internal type.
 */
action_param_type fixup_get_param_type(void** param)
{
	return *fixup_get_param_ptype(param);
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
		LM_CRIT("%s: fix_flag(%s): bad parameter type\n",
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
				LM_ERR("%s: invalid %s format: \"%s\"",
						mod_name, param_name, s);
				return -1;
			}
			*p=0;
		}
		if ((num=get_flag_no(s, len))<0){
			/* not declared yet, declare it */
			num=register_flag(s, f);
		}
		if (num<0){
			LM_ERR("%s: bad %s %s\n", mod_name, param_name, s);
			return -1;
		} else if ((f>0) && (num!=f)){
			LM_ERR("%s: flag %s already defined"
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
	int num;
	int err;

	p = (fparam_t*)pkg_malloc(sizeof(fparam_t));
	if (!p) {
		LM_ERR("No memory left\n");
		return E_OUT_OF_MEM;
	}
	memset(p, 0, sizeof(fparam_t));
	p->orig = *param;

	switch(type) {
		case FPARAM_UNSPEC:
			LM_ERR("Invalid type value\n");
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
			err = str2sint(&s, &num);
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
				LM_ERR("No memory left\n");
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
				LM_ERR("Error while parsing select identifier\n");
				goto error;
			}
			p->fixed = &p->v;
			break;
		case FPARAM_SUBST:
			s.s = *param;
			s.len = strlen(s.s);
			p->v.subst = subst_parser(&s);
			if (!p->v.subst) {
				LM_ERR("Error while parsing regex substitution\n");
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
				LM_ERR("out of memory while parsing pv_spec_t\n");
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
				LM_ERR("bad PVE format: \"%.*s\"\n", name.len, name.s);
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
 *  Note: it doesn't free fp->orig!
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
}


/**
 * @brief Generic free fixup type function for a fixed fparam
 *
 * Generic free fixup type function for a fixed fparam. It will free whatever
 * was allocated during the initial fparam fixup and restore the original param
 * value.
 * @param param freed parameters
 */
void fparam_free_restore(void** param)
{
	fparam_t *fp;
	void *orig;

	fp = *param;
	orig = fp->orig;
	fp->orig = 0;
	fparam_free_contents(fp);
	pkg_free(fp);
	*param = orig;
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

	if (fixup_get_param_type(param) == STRING_RVE_ST &&
			(types & (FPARAM_INT|FPARAM_STR|FPARAM_STRING))) {
		/* if called with a RVE already converted to string =>
		 * don't try AVP, PVAR or SELECT (to avoid double
		 * deref., e.g.: $foo="$bar"; f($foo) ) */
		types &= ~ (FPARAM_AVP|FPARAM_PVS|FPARAM_SELECT|FPARAM_PVE);
	}
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
	if (fixup_get_param_type(param) != STRING_RVE_ST) {
		/* if called with a RVE already converted to string =>
		 * don't try AVP, PVAR or SELECT (to avoid double
		 * deref., e.g.: $foo="$bar"; f($foo) ) */
		if ((ret = fix_param(FPARAM_PVS, param)) <= 0) return ret;
		if ((ret = fix_param(FPARAM_AVP, param)) <= 0) return ret;
		if ((ret = fix_param(FPARAM_SELECT, param)) <= 0) return ret;
	}
	if ((ret = fix_param(FPARAM_STR, param)) <= 0) return ret;
	LM_ERR("Error while fixing parameter, PV, AVP, SELECT, and str conversions"
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

/** fixup variable-pve-only-string.
 * The parameter can be a PVE (pv based format string)
 * or string.
 * non-static PVEs  identifiers will be resolved to
 * their values during runtime.
 * The parameter value will be converted to fparam structure
 * @param  param - double pointer to param, as for normal fixup functions.
 * @param  param_no - parameter number, ignored.
 * @return -1 on an error, 0 on success.
 */
int fixup_var_pve_12(void** param, int param_no)
{
	int ret;
	fparam_t* fp;
	if (fixup_get_param_type(param) != STRING_RVE_ST) {
		/* if called with a RVE already converted to string =>
		 * don't try PVE again (to avoid double
		 * deref., e.g.: $foo="$bar"; f($foo) ) */
		if ((ret = fix_param(FPARAM_PVE, param)) <= 0) {
			if (ret < 0)
				return ret;
			/* check if it resolved to a dynamic or "static" PVE.
			 * If the resulting PVE is static (normal string), discard
			 * it and use the normal string fixup (faster at runtime) */
			fp = (fparam_t*)*param;
			if (fp->v.pve->spec == 0 || fp->v.pve->spec->getf == 0)
				fparam_free_restore(param); /* fallback to STR below */
			else
				return ret; /* dynamic PVE => return */
		}

	}
	if ((ret = fix_param(FPARAM_STR, param)) <= 0) return ret;
	LM_ERR("Error while fixing parameter - PVE or str conversions failed\n");
	return -1;
}



/** fixup variable-pve-string.
 * The parameter can be a PVAR, AVP, SELECT, PVE (pv based format string)
 * or string.
 * PVAR, AVP and select and non-static PVEs  identifiers will be resolved to
 * their values during runtime.
 * The parameter value will be converted to fparam structure
 * @param  param - double pointer to param, as for normal fixup functions.
 * @param  param_no - parameter number, ignored.
 * @return -1 on an error, 0 on success.
 */
int fixup_var_pve_str_12(void** param, int param_no)
{
	int ret;
	fparam_t* fp;
	if (fixup_get_param_type(param) != STRING_RVE_ST) {
		/* if called with a RVE already converted to string =>
		 * don't try AVP, PVAR, SELECT or PVE again (to avoid double
		 * deref., e.g.: $foo="$bar"; f($foo) ) */
		if ((ret = fix_param(FPARAM_PVS, param)) <= 0) return ret;
		if ((ret = fix_param(FPARAM_AVP, param)) <= 0) return ret;
		if ((ret = fix_param(FPARAM_SELECT, param)) <= 0) return ret;
		if ((ret = fix_param(FPARAM_PVE, param)) <= 0) {
			if (ret < 0)
				return ret;
			/* check if it resolved to a dynamic or "static" PVE.
			 * If the resulting PVE is static (normal string), discard
			 * it and use the normal string fixup (faster at runtime) */
			fp = (fparam_t*)*param;
			if (fp->v.pve->spec == 0 || fp->v.pve->spec->getf == 0)
				fparam_free_restore(param); /* fallback to STR below */
			else
				return ret; /* dynamic PVE => return */
		}

	}
	if ((ret = fix_param(FPARAM_STR, param)) <= 0) return ret;
	LM_ERR("Error while fixing parameter, PV, AVP, SELECT, and str conversions"
			" failed\n");
	return -1;
}

/* Same as fixup_var_pve_str_12 but applies to the 1st parameter only */
int fixup_var_pve_str_1(void** param, int param_no)
{
	if (param_no == 1) return fixup_var_pve_str_12(param, param_no);
	else return 0;
}

/* Same as fixup_var_pve_str_12 but applies to the 2nd parameter only */
int fixup_var_pve_str_2(void** param, int param_no)
{
	if (param_no == 2) return fixup_var_pve_str_12(param, param_no);
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
	if (fixup_get_param_type(param) != STRING_RVE_ST) {
		/* if called with a RVE already converted to string =>
		 * don't try AVP, PVAR or SELECT (to avoid double
		 * deref., e.g.: $foo="$bar"; f($foo) ) */
		if ((ret = fix_param(FPARAM_PVS, param)) <= 0) return ret;
		if ((ret = fix_param(FPARAM_AVP, param)) <= 0) return ret;
		if ((ret = fix_param(FPARAM_SELECT, param)) <= 0) return ret;
	}
	if ((ret = fix_param(FPARAM_INT, param)) <= 0) return ret;
	LM_ERR("Error while fixing parameter, PV, AVP, SELECT, and int conversions"
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
	LM_ERR("Error while compiling regex in function parameter\n");
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
	LM_ERR("Cannot function parameter to integer\n");
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
	LM_ERR("Cannot function parameter to string\n");
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
				LM_DBG("Could not find AVP from function parameter '%s'\n",
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
				LM_ERR("Could not convert PV to str\n");
				return -1;
			}
			break;
		case FPARAM_PVE:
			dst->s=pv_get_buffer();
			dst->len=pv_get_buffer_size();
			if (unlikely(pv_printf(msg, param->v.pve, dst->s, &dst->len)!=0)){
				LM_ERR("Could not convert the PV-formated string to str\n");
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
			LM_ERR("Unsupported param type for int value: %d\n", param->type);
			return -1;
		case FPARAM_AVP:
			avp = search_first_avp(param->v.avp.flags, param->v.avp.name,
									&val, 0);
			if (unlikely(!avp)) {
				LM_DBG("Could not find AVP from function parameter '%s'\n",
						param->orig);
				return -1;
			}
			if (avp->flags & AVP_VAL_STR) {
				if (str2int(&val.s, (unsigned int*)dst) < 0) {
					LM_ERR("Could not convert AVP string value to int\n");
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
				LM_ERR("Could not convert select result to int\n");
				return -1;
			}
			break;
		case FPARAM_PVS:
			if (likely((pv_get_spec_value(msg, param->v.pvs, &pv_val)==0) &&
						((pv_val.flags&(PV_VAL_NULL|PV_VAL_INT))==PV_VAL_INT))){
					*dst=pv_val.ri;
			}else{
				LM_ERR("Could not convert PV to int\n");
				return -1;
			}
			break;
		case FPARAM_PVE:
			LM_ERR("Unsupported param type for int value: %d\n", param->type);
			return -1;
		default:
			LM_ERR("Unexpected param type: %d\n", param->type);
			return -1;
	}
	return 0;
}

/** Get the function parameter value as string or/and integer (if possible).
 *  @return  0 - Success
 *          -1 - Cannot get value
 */
int get_is_fparam(int* i_dst, str* s_dst, struct sip_msg* msg, fparam_t* param, unsigned int *flags)
{
	int_str val;
	int ret;
	avp_t* avp;
	str tmp;
	pv_value_t pv_val;

	*flags = 0;
	switch(param->type) {
		case FPARAM_INT:
			*i_dst = param->v.i;
			*flags |= PARAM_INT;
			return 0;
		case FPARAM_REGEX:
		case FPARAM_UNSPEC:
		case FPARAM_STRING:
			s_dst->s = param->v.asciiz;
			s_dst->len = strlen(param->v.asciiz);
			*flags |= PARAM_STR;
			break;
		case FPARAM_STR:
			*s_dst = param->v.str;
			*flags |= PARAM_STR;
			break;
		case FPARAM_AVP:
			avp = search_first_avp(param->v.avp.flags, param->v.avp.name,
									&val, 0);
			if (unlikely(!avp)) {
				LM_DBG("Could not find AVP from function parameter '%s'\n",
						param->orig);
				return -1;
			}
			if (avp->flags & AVP_VAL_STR) {
				*s_dst = val.s;
				*flags |= PARAM_STR;
				if (str2int(&val.s, (unsigned int*)i_dst) < 0) {
					LM_ERR("Could not convert AVP string value to int\n");
					return -1;
				}
			} else {
				*i_dst = val.n;
				*flags |= PARAM_INT;
			}
			break;
		case FPARAM_SELECT:
			ret = run_select(&tmp, param->v.select, msg);
			if (unlikely(ret < 0 || ret > 0)) return -1;
			if (unlikely(str2int(&tmp, (unsigned int*)i_dst) < 0)) {
				LM_ERR("Could not convert select result to int\n");
				return -1;
			}
			*flags |= PARAM_INT;
			break;
		case FPARAM_PVS:
			if (likely(pv_get_spec_value(msg, param->v.pvs, &pv_val)==0)) {
				if ((pv_val.flags&(PV_VAL_NULL|PV_VAL_INT))==PV_VAL_INT){
					*i_dst=pv_val.ri;
					*flags |= PARAM_INT;
				}
				if ((pv_val.flags&(PV_VAL_NULL|PV_VAL_STR))==PV_VAL_STR){
					*s_dst=pv_val.rs;
					*flags |= PARAM_STR;
				}
			}else{
				LM_ERR("Could not get PV\n");
				return -1;
			}
			break;
		case FPARAM_PVE:
			s_dst->s=pv_get_buffer();
			s_dst->len=pv_get_buffer_size();
			if (unlikely(pv_printf(msg, param->v.pve, s_dst->s, &s_dst->len)!=0)){
				LM_ERR("Could not convert the PV-formated string to str\n");
				s_dst->len=0;
				return -1;
			}
			*flags |= PARAM_STR;
			break;
	}

	/* Let's convert to int, if possible */
	if (!(*flags & PARAM_INT) && (*flags & PARAM_STR) && str2sint(s_dst, i_dst) == 0)
		*flags |= PARAM_INT;

	if (!*flags) return -1;

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
			LM_ERR("unexpected parameter type (%d), instead of regexp.\n",
					param->type);
	}
	return -1;
}



/** generic free fixup function for "pure" fparam type fixups.
 * @param  param - double pointer to param, as for normal fixup functions.
 * @param  param_no - parameter number, ignored.
 * @return 0 on success (always).
 */
int fixup_free_fparam_all(void** param, int param_no)
{
	fparam_free_restore(param);
	return 0;
}



/** generic free fixup function for "pure"  first parameter fparam type fixups.
 * @param  param - double pointer to param, as for normal fixup functions.
 * @param  param_no - parameter number: the function will work only for
 *                     param_no == 1 (first parameter).
 * @return 0 on success (always).
 */
int fixup_free_fparam_1(void** param, int param_no)
{
	if (param_no == 1)
		fparam_free_restore(param);
	return 0;
}



/** generic free fixup function for "pure"  2nd parameter fparam type fixups.
 * @param  param - double pointer to param, as for normal fixup functions.
 * @param  param_no - parameter number: the function will work only for
 *                     param_no == 2 (2nd parameter).
 * @return 0 on success (always).
 */
int fixup_free_fparam_2(void** param, int param_no)
{
	if (param_no == 2)
		fparam_free_restore(param);
	return 0;
}



/** returns true if a fixup is a fparam_t* one.
 * Used to automatically detect "pure" fparam fixups that can be used with non
 * contant RVEs.
 * @param f - function pointer
 * @return 1 for fparam fixups, 0 for others.
 */
int is_fparam_rve_fixup(fixup_function f)
{
	if (f == fixup_var_str_12 ||
		f == fixup_var_str_1 ||
		f == fixup_var_str_2 ||
		f == fixup_var_pve_str_12 ||
		f == fixup_var_pve_str_1 ||
		f == fixup_var_pve_str_2 ||
		f == fixup_var_int_12 ||
		f == fixup_var_int_1 ||
		f == fixup_var_int_2 ||
		f == fixup_int_12 ||
		f == fixup_int_1 ||
		f == fixup_int_2 ||
		f == fixup_str_12 ||
		f == fixup_str_1 ||
		f == fixup_str_2 ||
		f == fixup_regex_12 ||
		f == fixup_regex_1 ||
		f == fixup_regex_2
		)
		return 1;
	return 0;
}


/**
 * @brief returns the corresponding fixup_free* for various known fixup types
 *
 * Returns the corresponding fixup_free* for various known fixup types.
 * Used to automatically fill in free_fixup* functions.
 * @param f fixup function pointer
 * @return free fixup function pointer on success, 0 on failure (unknown
 * fixup or no free fixup function).
 */
free_fixup_function get_fixup_free(fixup_function f)
{
	free_fixup_function ret;
	/* "pure" fparam, all parameters */
	if (f == fixup_var_str_12 ||
		f == fixup_var_pve_str_12 ||
		f == fixup_var_int_12 ||
		f == fixup_int_12 ||
		f == fixup_str_12 ||
		f == fixup_regex_12)
		return fixup_free_fparam_all;

	/* "pure" fparam, 1st parameter */
	if (f == fixup_var_str_1 ||
		f == fixup_var_pve_str_1 ||
		f == fixup_var_int_1 ||
		f == fixup_int_1 ||
		f == fixup_str_1 ||
		f == fixup_regex_1)
		return fixup_free_fparam_1;

	/* "pure" fparam, 2nd parameters */
	if (f == fixup_var_str_2 ||
		f == fixup_var_pve_str_2 ||
		f == fixup_var_int_2 ||
		f == fixup_int_2 ||
		f == fixup_str_2 ||
		f == fixup_regex_2)
		return fixup_free_fparam_2;

	/* mod_fix.h kamailio style fixups */
	if ((ret = mod_fix_get_fixup_free(f)) != 0)
		return ret;

	/* unknown */
	return 0;
}
